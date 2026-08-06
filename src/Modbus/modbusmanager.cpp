#include "modbusmanager.h"

ModbusManager::ModbusManager(QObject *parent)
    : QObject(parent)
{
    m_modbusClient = new QModbusRtuSerialMaster(this);
    m_autoReadTimer = new QTimer(this);

    // ── 轮询定时器：每 1 秒从传感器读取一次数据 ──
    m_autoReadTimer->setInterval(1000);
    connect(m_autoReadTimer, &QTimer::timeout, this, [this]() {
        if (m_isConnected
            && m_modbusClient
            && m_modbusClient->state() == QModbusDevice::ConnectedState) {
            readHoldRegisters(m_slaveId, m_startAddr, m_registerCount);
        }
    });

    // ── 设备状态变化 → 管理 m_isConnected 和定时器 ──
    connect(m_modbusClient, &QModbusDevice::stateChanged,
            this, &ModbusManager::onStateChanged);

    // 注意：定时器不在构造函数中启动！
    // 启动时机：connectToSlave 成功后，在 onStateChanged 中收到 ConnectedState 时启动。
    // 这样避免了"还没连接就开始轮询"的无效操作。
}

ModbusManager::~ModbusManager()
{
    m_autoReadTimer->stop();
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
    }
}

void ModbusManager::setPollConfig(int slaveId, int startAddr, int count)
{
    m_slaveId = slaveId;
    m_startAddr = startAddr;
    m_registerCount = count;
}

// ── 连接/断开 ───────────────────────────────────────────────────────────────

bool ModbusManager::connectToSlave(const QString &portName, int baudRate)
{
    if (!m_modbusClient) {
        return false;
    }

    // 如果已经连接，先断开
    if (m_modbusClient->state() == QModbusDevice::ConnectedState) {
        m_modbusClient->disconnectDevice();
    }

    // 配置串口参数
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
    // 注意：connectDevice() 返回 true 只表示"连接请求已发起"，不代表物理连接已建立。
    // 真正的连接确认在 onStateChanged 回调中。
    return true;
}

void ModbusManager::disconnectSlave()
{
    m_autoReadTimer->stop();
    m_isConnected = false;
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
    }
    emit disconnected();
}

// ── 读取/写入 ───────────────────────────────────────────────────────────────

void ModbusManager::readHoldRegisters(int slaveId, int startAddr, int count)
{
    if (!m_isConnected || !m_modbusClient
        || m_modbusClient->state() != QModbusDevice::ConnectedState) {
        return; // 静默返回，避免每秒刷屏
    }

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startAddr, count);

    if (auto *reply = m_modbusClient->sendReadRequest(readUnit, slaveId)) {
        if (!reply->isFinished()) {
            // 异步等待：回复到达后由 onReadReady 处理
            connect(reply, &QModbusReply::finished,
                    this, &ModbusManager::onReadReady);
        } else {
            // 同步完成（极少数情况，如缓存命中）
            delete reply;
        }
    } else {
        qDebug() << "❌ 发送 Modbus 读取请求失败:" << m_modbusClient->errorString();
        emit errorOccurred(m_modbusClient->errorString());
    }
}

void ModbusManager::writeSingleRegister(int slaveId, int addr, quint16 value)
{
    if (!m_modbusClient
        || m_modbusClient->state() != QModbusDevice::ConnectedState) {
        qDebug() << "❌ 写入失败：设备未连接";
        return;
    }

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, addr, 1);
    writeUnit.setValue(0, value);

    if (auto *reply = m_modbusClient->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [reply]() {
                if (reply->error() == QModbusDevice::NoError) {
                    qDebug() << "✅ Modbus 写入成功";
                } else {
                    qDebug() << "❌ Modbus 写入失败:" << reply->errorString();
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();
        }
    } else {
        qDebug() << "❌ 发送写入请求失败:" << m_modbusClient->errorString();
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void ModbusManager::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_isConnected = true;
        qDebug() << "🟢 Modbus 设备已连接，启动轮询...";
        m_autoReadTimer->start();
        emit connected();
    } else if (state == QModbusDevice::UnconnectedState) {
        m_isConnected = false;
        qDebug() << "🔴 Modbus 设备已断开，暂停轮询";
        m_autoReadTimer->stop();
        emit disconnected();
    }
}

void ModbusManager::onReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) {
        return;
    }

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        for (uint i = 0; i < unit.valueCount(); ++i) {
            const int sensorId = unit.startAddress() + static_cast<int>(i);
            const double value = static_cast<double>(unit.value(i));

            qDebug() << QString("📡 Modbus 寄存器 [%1] → %2")
                            .arg(sensorId)
                            .arg(value);

            // ★ 核心信号：数据跨线程投递到 DatabaseWorker
            emit dataReceived(sensorId, value);
        }
    } else if (reply->error() != QModbusDevice::NoError) {
        qDebug() << "⚠️ Modbus 读取错误:" << reply->errorString();
    }

    reply->deleteLater();
}
