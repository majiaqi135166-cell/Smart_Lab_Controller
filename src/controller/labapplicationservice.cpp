#include "labapplicationservice.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QtGlobal>

#include "ConfigManager.h"

LabApplicationService::LabApplicationService(QObject *parent)
    : QObject(parent)
    , m_deviceLogTimer(new QTimer(this))
    , m_logThread(new QThread(this))
    , m_worker(new LogWorker())          // 不能传 this：后面 moveToThread 要求无 parent
    , m_dbThread(new QThread(this))
    , m_dbWorker(new DatabaseWorker())  // 同上
    , m_modbusManager(new ModbusManager())  // 主线程：Modbus 是异步的，不阻塞 UI
    , m_transport(new TcpDeviceChannel(this))
    , m_controller(new LabController([this](const QByteArray &payload) {
        m_transport->send(payload);
    }, this))
{
    setupLogThread();
    setupDbThread();
    setupModbus();   // ★ 建立 Modbus → DB 的跨线程数据链路

    // Transport ↔ controller wiring (all main-thread objects → direct connections are fine)
    connect(m_transport, &TcpDeviceChannel::frameReceived,
            this, &LabApplicationService::onTransportFrameReceived);
    connect(m_transport, &TcpDeviceChannel::statusChanged,
            this, &LabApplicationService::onTransportStatusChanged);
    connect(m_controller, &LabController::snapshotChanged,
            this, &LabApplicationService::onSnapshotChanged);
    connect(m_controller, &LabController::remoteMessage,
            this, &LabApplicationService::remoteCommandReceived);
    connect(m_controller, &LabController::thresholdExceeded, this,
            [this](const QString &message) {
                emit overplanpower(message);
                emit securityRisk();
                saveLog(message);
            });
    // Debounce timer for device log writes
    m_deviceLogTimer->setSingleShot(true);
    m_deviceLogTimer->setInterval(800);
    connect(m_deviceLogTimer, &QTimer::timeout,
            this, &LabApplicationService::handleDeviceLog);

    updateSensorData(SensorData(20.0, 20));
    m_transport->start();
}

LabApplicationService::~LabApplicationService()
{
    m_transport->stop();

    // 先断开 Modbus → DB 的跨线程连接，防止 Worker 已销毁后信号仍然投递
    disconnect(m_modbusManager, nullptr, m_dbWorker, nullptr);

    disconnect(m_transport, nullptr, this, nullptr);
    disconnect(m_controller, nullptr, m_worker, nullptr);
    disconnect(m_controller, nullptr, m_dbWorker, nullptr);

    // ModbusManager 无 parent，需手动释放（在 DB Worker 还活着时断开）
    delete m_modbusManager;

    teardownThreads();
}

const DeviceSnapshot &LabApplicationService::snapshot() const
{
    return m_snapshot;
}

QVariantList LabApplicationService::history() const
{
    return m_latestHistory;
}

void LabApplicationService::setLightBrightness(int value)
{
    if (m_snapshot.lightBrightness == value) {
        return;
    }
    m_controller->setLightBrightness(value);
    m_deviceLogTimer->start();
}

void LabApplicationService::setFanActive(bool active)
{
    if (m_snapshot.fanActive == active) {
        return;
    }
    m_controller->setFanActive(active);
    m_deviceLogTimer->start();
}

void LabApplicationService::setLaserPower(double power)
{
    if (qFuzzyCompare(m_snapshot.laserPower + 1.0, power + 1.0)) {
        return;
    }
    m_controller->setLaserPower(power);
    m_deviceLogTimer->start();
}

void LabApplicationService::updateSensorData(const SensorData &data)
{
    m_controller->setLightBrightness(data.brightness);
    m_controller->setLaserPower(data.power);
}

void LabApplicationService::emergencyStop()
{
    m_controller->emergencyStop();
    saveLog(QStringLiteral("Emergency Stop Triggered! All systems reset."));
}

void LabApplicationService::saveLog(const QString &message)
{
    emit saveLogRequested(message);
}

void LabApplicationService::clearLog()
{
    emit clearLogRequested();
}

QByteArray LabApplicationService::currentSnapshotPacket() const
{
    return m_controller->currentSnapshotPacket();
}

bool LabApplicationService::exportToCSV(const QString &fileName)
{
    if (m_latestHistory.isEmpty()) {
        qDebug() << "导出失败：没有历史记录可供导出。";
        return false;
    }

    // 用 BlockingQueuedConnection 让文件 I/O 在 DB 工作线程执行，
    // 主线程等待结果返回。10 行 CSV 的写入时间可忽略不计。
    bool ok = false;
    QMetaObject::invokeMethod(m_dbWorker, "exportToCSV",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(QString, fileName),
                              Q_ARG(QVariantList, m_latestHistory));
    return ok;
}

void LabApplicationService::requestHistory()
{
    emit dbHistoryRequested();
}

double LabApplicationService::deviceValue(const QString &deviceName) const
{
    return m_controller->deviceValue(deviceName);
}

// ─── Private helpers ────────────────────────────────────────────────────────

void LabApplicationService::setupLogThread()
{
    m_worker->moveToThread(m_logThread);
    // Explicit QueuedConnection since signals are emitted from main thread but slot runs in log thread
    connect(this, &LabApplicationService::saveLogRequested,
            m_worker, &LogWorker::doWritelog, Qt::QueuedConnection);
    connect(this, &LabApplicationService::clearLogRequested,
            m_worker, &LogWorker::clearLog, Qt::QueuedConnection);
    connect(m_logThread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    m_logThread->start();
}

void LabApplicationService::setupDbThread()
{
    m_dbWorker->moveToThread(m_dbThread);
    connect(m_dbThread, &QThread::started,
            m_dbWorker, &DatabaseWorker::init);
    // Explicit QueuedConnection across thread boundary
    connect(this, &LabApplicationService::dbLogRequested,
            m_dbWorker, &DatabaseWorker::logToDatabase, Qt::QueuedConnection);
    connect(this, &LabApplicationService::dbHistoryRequested,
            m_dbWorker, &DatabaseWorker::getHistory, Qt::QueuedConnection);
    connect(m_dbWorker, &DatabaseWorker::historyReady,
            this, &LabApplicationService::onHistoryReady, Qt::QueuedConnection);
    connect(m_dbWorker, &DatabaseWorker::entryAdded,
            this, &LabApplicationService::historyUpdated, Qt::QueuedConnection);
    connect(m_dbThread, &QThread::finished,
            m_dbWorker, &QObject::deleteLater);
    m_dbThread->start();
}

void LabApplicationService::setupModbus()
{
    // ── 连接 1：ModbusManager → DatabaseWorker（跨线程入库）──
    connect(m_modbusManager, &ModbusManager::dataReceived,
            m_dbWorker, &DatabaseWorker::logSensorData,
            Qt::QueuedConnection);

    // ── 连接 2：ModbusManager → LabApplicationService（UI 更新，同在主线程）──
    connect(m_modbusManager, &ModbusManager::dataReceived,
            this, &LabApplicationService::onModbusDataReceived);

    // ── 连接 3：ModbusManager 连接/断开状态 → UI ──
    connect(m_modbusManager, &ModbusManager::connected,
            this, &LabApplicationService::onModbusConnected);
    connect(m_modbusManager, &ModbusManager::disconnected,
            this, &LabApplicationService::onModbusDisconnected);

    // 从配置文件读取上次的串口号，自动连接
    const QString port = ConfigManager::instance()->modbusPort();
    if (!port.isEmpty()) {
        qDebug() << "[LabAppService] 🔌 正在连接 Modbus 串口:" << port;
        m_modbusManager->connectToSlave(port);
    }

    qDebug() << "[LabAppService] ✅ Modbus → DatabaseWorker 跨线程链路已建立";
}

// ── Modbus 控制（QML → backend → 这里）──────────────────────────────────────

void LabApplicationService::connectModbus(const QString &portName, int baudRate)
{
    qDebug() << "[LabAppService] 🔌 QML 发起连接:" << portName << baudRate;
    // 持久化配置，下次启动自动连接
    ConfigManager::instance()->setModbusPort(portName);
    m_modbusManager->connectToSlave(portName, baudRate);
}

void LabApplicationService::disconnectModbus()
{
    qDebug() << "[LabAppService] 🔌 QML 发起断开连接";
    m_modbusManager->disconnectSlave();
}

void LabApplicationService::writeModbusRegister(int slaveId, int addr, int value)
{
    qDebug() << "[LabAppService] ✏️ QML 发起写入: 从站" << slaveId
             << "寄存器" << addr << "=" << value;
    m_modbusManager->writeSingleRegister(slaveId, addr,
                                         static_cast<quint16>(value));
    // 不需要等回复——1秒后的轮询会自动读取到新值并更新 UI
}

QVariantMap LabApplicationService::modbusRegisters() const
{
    QVariantMap map;
    for (auto it = m_modbusRegisters.cbegin(); it != m_modbusRegisters.cend(); ++it) {
        map.insert(QString::number(it.key()), it.value());
    }
    return map;
}

// ── Modbus 内部槽 ──────────────────────────────────────────────────────────

void LabApplicationService::onModbusDataReceived(int sensorId, double value)
{
    m_modbusRegisters.insert(sensorId, value);
    emit modbusDataChanged(sensorId, value);
    // 这条信号 → backend → QML property binding → UI 自动刷新
}

void LabApplicationService::onModbusConnected()
{
    m_modbusConnected = true;
    emit modbusConnectedChanged(true);
}

void LabApplicationService::onModbusDisconnected()
{
    m_modbusConnected = false;
    emit modbusConnectedChanged(false);
}

void LabApplicationService::teardownThreads()
{
    if (m_logThread->isRunning()) {
        m_logThread->quit();
        m_logThread->wait(3000);
    }
    if (m_dbThread->isRunning()) {
        m_dbThread->quit();
        m_dbThread->wait(3000);
    }
}

// ─── Slots ──────────────────────────────────────────────────────────────────

void LabApplicationService::handleDeviceLog()
{
    const QString logMsg = QString("Device Status Update -fanstatus:%1 Brightness: %2%, Power: %3W")
                               .arg(m_snapshot.fanActive ? "open" : "close")
                               .arg(m_snapshot.lightBrightness)
                               .arg(m_snapshot.laserPower, 0, 'f', 1);
    saveLog(logMsg);
}

void LabApplicationService::onHistoryReady(QVariantList history)
{
    m_latestHistory = history;
    emit historyFetched(history);
}

void LabApplicationService::onSnapshotChanged(const DeviceSnapshot &snapshot)
{
    const DeviceSnapshot::ChangeFlags changes = m_snapshot.diff(snapshot);
    m_snapshot = snapshot;

    if (changes & DeviceSnapshot::LaserChanged) {
        logToDatabase("laser", "set_power", QString::number(snapshot.laserPower));
    }
    if (changes & DeviceSnapshot::BrightnessChanged) {
        logToDatabase("brightness", "set", QString::number(snapshot.lightBrightness));
    }
    if (changes & DeviceSnapshot::FanChanged) {
        logToDatabase("fan", "switch", snapshot.fanActive ? QStringLiteral("1") : QStringLiteral("0"));
    }
    if (changes & DeviceSnapshot::OnlineChanged) {
        logToDatabase("network", snapshot.online ? QStringLiteral("connected") : QStringLiteral("disconnected"),
                      snapshot.online ? QStringLiteral("1") : QStringLiteral("0"));
    }

    emit snapshotChanged(m_snapshot);
}

void LabApplicationService::onTransportStatusChanged(TransportStatus status)
{
    m_controller->setOnline(status.online);
}

void LabApplicationService::onTransportFrameReceived(const TransportFrame &frame)
{
    m_controller->processMessage(frame.message);
}

void LabApplicationService::logToDatabase(const QString &target, const QString &action, const QString &details)
{
    emit dbLogRequested(QStringLiteral("info"), target, action, details);
}


