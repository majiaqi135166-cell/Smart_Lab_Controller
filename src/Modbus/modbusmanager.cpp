#include "modbusmanager.h"
#include <QDateTime>

// ═══════════════════════════════════════════════════════════════════════════
// 构造函数 & 析构
// ═══════════════════════════════════════════════════════════════════════════

ModbusManager::ModbusManager(QObject *parent)
    : QObject(parent)
{
    m_modbusClient = new QModbusRtuSerialMaster(this);

    // ── 统一轮询定时器：每 200ms 检查哪些从站该轮询了 ──
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(PollCheckIntervalMs);
    connect(m_pollTimer, &QTimer::timeout,
            this, &ModbusManager::onPollTimer);
    // 注意：m_pollTimer 不在构造函数中 start()
    // 它会在串口连接成功后（onStateChanged → ConnectedState）启动

    // ── 重连定时器（指数退避，单次触发）──
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &ModbusManager::onReconnectTimer);

    // ── 回复超时定时器（防止坏从站卡死队列）──
    m_replyTimeoutTimer = new QTimer(this);
    m_replyTimeoutTimer->setSingleShot(true);
    connect(m_replyTimeoutTimer, &QTimer::timeout,
            this, &ModbusManager::onReplyTimeout);

    // ── 设备状态变化 ──
    connect(m_modbusClient, &QModbusDevice::stateChanged,
            this, &ModbusManager::onStateChanged);
}

ModbusManager::~ModbusManager()
{
    m_pollTimer->stop();
    m_reconnectTimer->stop();
    m_replyTimeoutTimer->stop();

    m_highQueue.clear();
    m_normalQueue.clear();
    m_pendingReply = nullptr;

    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 从站管理
// ═══════════════════════════════════════════════════════════════════════════

void ModbusManager::addSlave(const ModbusSlaveConfig &config)
{
    // 检查是否已存在同一个 slaveId
    for (int i = 0; i < m_slaves.size(); ++i) {
        if (m_slaves[i].slaveId == config.slaveId) {
            // 替换旧配置
            m_slaves[i] = config;
            qDebug() << "[Modbus] 更新从站配置: ID" << config.slaveId << config.name;
            return;
        }
    }

    m_slaves.append(config);
    m_lastPollTime.insert(config.slaveId, 0);
    qDebug() << "[Modbus] 添加从站: ID" << config.slaveId << config.name
             << "轮询间隔:" << config.pollIntervalMs << "ms";
}

void ModbusManager::removeSlave(int slaveId)
{
    for (int i = 0; i < m_slaves.size(); ++i) {
        if (m_slaves[i].slaveId == slaveId) {
            qDebug() << "[Modbus] 移除从站: ID" << slaveId << m_slaves[i].name;
            m_slaves.removeAt(i);
            m_lastPollTime.remove(slaveId);
            return;
        }
    }
}

void ModbusManager::clearSlaves()
{
    m_slaves.clear();
    m_lastPollTime.clear();
    qDebug() << "[Modbus] 已清除所有从站";
}

// ═══════════════════════════════════════════════════════════════════════════
// 串口连接/断开
// ═══════════════════════════════════════════════════════════════════════════

bool ModbusManager::connectToSlave(const QString &portName, int baudRate)
{
    if (!m_modbusClient) {
        return false;
    }

    m_lastPortName = portName;
    m_lastBaudRate = baudRate;
    m_autoReconnect = true;

    if (m_modbusClient->state() == QModbusDevice::ConnectedState) {
        m_modbusClient->disconnectDevice();
    }

    m_modbusClient->setConnectionParameter(
        QModbusDevice::SerialPortNameParameter, QVariant(portName));
    m_modbusClient->setConnectionParameter(
        QModbusDevice::SerialBaudRateParameter, QVariant(baudRate));
    m_modbusClient->setConnectionParameter(
        QModbusDevice::SerialDataBitsParameter,
        QVariant(static_cast<int>(QSerialPort::Data8)));
    m_modbusClient->setConnectionParameter(
        QModbusDevice::SerialParityParameter,
        QVariant(static_cast<int>(QSerialPort::NoParity)));
    m_modbusClient->setConnectionParameter(
        QModbusDevice::SerialStopBitsParameter,
        QVariant(static_cast<int>(QSerialPort::OneStop)));

    m_modbusClient->setTimeout(1000);
    m_modbusClient->setNumberOfRetries(3);

    if (!m_modbusClient->connectDevice()) {
        qDebug() << "❌ 串口连接失败:" << m_modbusClient->errorString();
        emit errorOccurred(m_modbusClient->errorString());
        return false;
    }

    qDebug() << "🔌 成功发起串口连接，目标端口:" << portName
             << "波特率:" << baudRate;
    return true;
}

void ModbusManager::disconnectSlave()
{
    m_autoReconnect = false;
    m_reconnectTimer->stop();
    m_replyTimeoutTimer->stop();

    m_pollTimer->stop();
    m_highQueue.clear();
    m_normalQueue.clear();
    m_queueState = QueueState::Idle;
    m_pendingReply = nullptr;

    m_isConnected = false;
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
    }
    emit disconnected();
}

// ═══════════════════════════════════════════════════════════════════════════
// 对外接口（不再直接发请求，改为入队）
// ═══════════════════════════════════════════════════════════════════════════

void ModbusManager::readHoldRegisters(int slaveId, int startAddr, int count)
{
    if (!m_isConnected) return;
    enqueue(ModbusTask::makeRead(slaveId, startAddr, count));
}

void ModbusManager::writeSingleRegister(int slaveId, int addr, quint16 value)
{
    if (!m_isConnected) return;
    enqueue(ModbusTask::makeWrite(slaveId, addr, value));
}

// ═══════════════════════════════════════════════════════════════════════════
// 任务队列核心
// ═══════════════════════════════════════════════════════════════════════════

void ModbusManager::enqueue(const ModbusTask &task)
{
    // 按优先级分别放入两个队列
    if (task.priority == ModbusTask::High) {
        m_highQueue.enqueue(task);
    } else {
        m_normalQueue.enqueue(task);
    }

    qDebug() << QString("[Modbus 队列] 入队: %1 slave%2 addr=%3 cnt=%4 "
                        "高优:%5 普通:%6")
                    .arg(task.type == ModbusTask::Read ? "读" : "写")
                    .arg(task.slaveId)
                    .arg(task.startAddr)
                    .arg(task.registerCount)
                    .arg(m_highQueue.size())
                    .arg(m_normalQueue.size());

    // 如果当前空闲，立即处理
    if (m_queueState == QueueState::Idle) {
        processQueue();
    }
    // 如果正在等待回复 → 新任务排队等待，当前回复到达后 processQueue() 会处理
}

void ModbusManager::processQueue()
{
    // ── 条件检查 ──
    if (m_queueState != QueueState::Idle) {
        return;  // 还有请求在飞，不能发新的
    }

    if (!m_modbusClient
        || m_modbusClient->state() != QModbusDevice::ConnectedState) {
        return;  // 没连接，不发
    }

    // ── 取任务：高优先级队列优先 ──
    ModbusTask task;
    if (!m_highQueue.isEmpty()) {
        task = m_highQueue.dequeue();     // ★ 写入优先
    } else if (!m_normalQueue.isEmpty()) {
        task = m_normalQueue.dequeue();   // 轮询读取
    } else {
        return;  // 两个队列都空
    }

    // ── 构造 Modbus 数据单元 ──
    QModbusDataUnit unit(
        task.type == ModbusTask::Read
            ? QModbusDataUnit::HoldingRegisters
            : QModbusDataUnit::HoldingRegisters,  // 读写都是 HoldingRegisters
        task.startAddr,
        task.registerCount);

    if (task.type == ModbusTask::Write) {
        unit.setValue(0, task.writeValue);
    }

    // ── 发送请求 ──
    QModbusReply *reply = nullptr;
    if (task.type == ModbusTask::Read) {
        reply = m_modbusClient->sendReadRequest(unit, task.slaveId);
    } else {
        reply = m_modbusClient->sendWriteRequest(unit, task.slaveId);
    }

    if (!reply) {
        qDebug() << "[Modbus 队列] ❌ 发送请求失败:"
                 << m_modbusClient->errorString();
        // 发送失败 → 不阻塞，立即处理下一个
        processQueue();
        return;
    }

    // ── 发送成功 → 进入等待状态 ──
    m_queueState = QueueState::WaitingReply;
    m_pendingReply = reply;
    m_pendingTaskType = task.type;   // ★ 记住：这个回复是读还是写

    // 注册回复回调
    connect(reply, &QModbusReply::finished,
            this, &ModbusManager::onReplyFinished);

    // 启动超时定时器
    m_replyTimeoutTimer->start(ReplyTimeoutMs);

    qDebug() << QString("[Modbus 队列] ➤ 发送: %1 slave%2 addr=%3 cnt=%4")
                    .arg(task.type == ModbusTask::Read ? "读" : "写")
                    .arg(task.slaveId)
                    .arg(task.startAddr)
                    .arg(task.registerCount);
}

// ═══════════════════════════════════════════════════════════════════════════
// Slots
// ═══════════════════════════════════════════════════════════════════════════

void ModbusManager::onReplyFinished()
{
    m_replyTimeoutTimer->stop();  // 回复到了，取消超时

    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) {
        m_queueState = QueueState::Idle;
        m_pendingReply = nullptr;
        processQueue();
        return;
    }

    const int slaveId = reply->serverAddress();

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();

        // ★ 按任务类型分支处理：读写回复完全不同
        if (m_pendingTaskType == ModbusTask::Read) {
            // ── 读回复：解析寄存器值 → 广播数据 ──
            for (uint i = 0; i < unit.valueCount(); ++i) {
                const int sensorId = unit.startAddress() + static_cast<int>(i);
                const double value = static_cast<double>(unit.value(i));

                qDebug() << QString("📡 [从站%1] 寄存器[%2] → %3")
                                .arg(slaveId)
                                .arg(sensorId)
                                .arg(value);

                emit dataReceived(slaveId,sensorId, value);
            }
        } else {
            // ── 写回复：只是请求回显，不是新数据 ──
            // 不 emit dataReceived！下一次轮询读取会自动读到新值并广播
            qDebug() << QString("✅ [从站%1] 写入寄存器[%2]成功")
                            .arg(slaveId)
                            .arg(unit.startAddress());
        }

        emit taskCompleted(slaveId, true, QString());

    } else {
        // ── 失败 ──
        qDebug() << QString("⚠️ [从站%1] 请求失败: %2")
                        .arg(slaveId)
                        .arg(reply->errorString());

        emit taskCompleted(slaveId, false, reply->errorString());
    }

    reply->deleteLater();

    // ★ 解锁 → 立即处理队列中下一个任务
    m_queueState = QueueState::Idle;
    m_pendingReply = nullptr;
    processQueue();
}

void ModbusManager::onReplyTimeout()
{
    qDebug() << "[Modbus 队列] ⏰ 回复超时！强制解锁，跳过当前请求";

    // 强制解锁状态机，让队列继续
    m_queueState = QueueState::Idle;
    m_pendingReply = nullptr;

    processQueue();
}

void ModbusManager::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_isConnected = true;
        m_reconnectDelayMs = MinReconnectDelayMs;
        m_reconnectTimer->stop();

        qDebug() << "🟢 Modbus 设备已连接，启动轮询...";

        // 启动统一轮询定时器
        m_pollTimer->start();

        emit connected();

    } else if (state == QModbusDevice::UnconnectedState) {
        m_isConnected = false;
        m_pollTimer->stop();
        m_replyTimeoutTimer->stop();

        // 清空队列（断线了，积压的任务没意义）
        m_highQueue.clear();
        m_normalQueue.clear();
        m_queueState = QueueState::Idle;
        m_pendingReply = nullptr;

        if (m_autoReconnect) {
            qDebug() << "🔴 Modbus 意外断开，启动指数退避重连...";
            emit reconnecting();
            scheduleReconnect();
        } else {
            qDebug() << "🔴 Modbus 设备已断开（用户主动）";
            emit disconnected();
        }
    }
}

void ModbusManager::onPollTimer()
{
    // 每 200ms 检查一次：哪些从站的轮询间隔到了？
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const ModbusSlaveConfig &slave : m_slaves) {
        const qint64 lastPoll = m_lastPollTime.value(slave.slaveId, 0);
        const qint64 elapsed = now - lastPoll;

        if (elapsed >= slave.pollIntervalMs) {
            // 该从站的轮询间隔到了 → 入队一个 Read 任务
            m_lastPollTime.insert(slave.slaveId, now);
            readHoldRegisters(slave.slaveId, slave.startAddr,
                              slave.registerCount);
            // readHoldRegisters 内部调 enqueue()
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 指数退避重连（未改动，与之前一致）
// ═══════════════════════════════════════════════════════════════════════════

void ModbusManager::scheduleReconnect()
{
    m_reconnectTimer->stop();
    m_reconnectTimer->setInterval(m_reconnectDelayMs);
    m_reconnectTimer->start();

    qDebug() << QString("🟡 重连中... %1ms 后尝试重连")
                    .arg(m_reconnectDelayMs);

    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2,
                              static_cast<int>(MaxReconnectDelayMs));
}

void ModbusManager::onReconnectTimer()
{
    if (m_modbusClient
        && m_modbusClient->state() == QModbusDevice::ConnectedState) {
        return;
    }

    qDebug() << "🟡 指数退避触发：尝试重连" << m_lastPortName << m_lastBaudRate;

    if (!m_modbusClient->connectDevice()) {
        qDebug() << "❌ 重连失败:" << m_modbusClient->errorString();
        scheduleReconnect();
    }
}
