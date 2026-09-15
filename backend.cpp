#include "backend.h"

#include <QtGlobal>

backend::backend(QObject *parent)
    : QObject(parent)
    , m_service(new LabApplicationService(this))
{
    connect(m_service, &LabApplicationService::snapshotChanged,
            this, &backend::onSnapshotChanged);
    connect(m_service, &LabApplicationService::securityRisk,
            this, &backend::securityRisk);
    connect(m_service, &LabApplicationService::remoteCommandReceived,
            this, &backend::remoteCommandReceived);
    connect(m_service, &LabApplicationService::historyUpdated,
            this, &backend::historyUpdated);
    connect(m_service, &LabApplicationService::historyFetched,
            this, &backend::onHistoryFetched);
    connect(m_service, &LabApplicationService::overplanpower,
            this, &backend::overplanpower);

    // Modbus 信号转发
    connect(m_service, &LabApplicationService::modbusDataChanged,
            this, &backend::onModbusDataChanged);
    connect(m_service, &LabApplicationService::modbusConnectedChanged,
            this, &backend::onModbusConnectedChanged);
    connect(m_service, &LabApplicationService::modbusReconnectingChanged,
            this, &backend::onModbusReconnectingChanged);
    connect(m_service,&LabApplicationService::ModbussensorAlarm,this,&backend::onModbusSensorAlarm);

    onSnapshotChanged(m_service->snapshot());
}

backend::~backend() = default;

double backend::laserPower() const
{
    return m_snapshot.laserPower;
}

int backend::lightBrightness() const
{
    return m_snapshot.lightBrightness;
}

bool backend::isFanActive() const
{
    return m_snapshot.fanActive;
}

bool backend::isOnline() const
{
    return m_snapshot.online;
}

void backend::setLightBrightness(int value)
{
    m_service->setLightBrightness(value);
}

void backend::setIsFanActive(bool active)
{
    m_service->setFanActive(active);
}

void backend::setLaserPower(double power)
{
    m_service->setLaserPower(power);
}

void backend::emergencyStop()
{
    m_service->emergencyStop();
}

void backend::saveLog(const QString &message)
{
    m_service->saveLog(message);
}

void backend::clearLog()
{
    m_service->clearLog();
}

void backend::printCurrentJson()
{
    const QByteArray packet = m_service->currentSnapshotPacket();
    qDebug() << "--- 当前设备状态 Packet 预览 ---";
    qDebug() << packet.toHex(' ');
}

bool backend::exportToCSV(QString fileName)
{
    return m_service->exportToCSV(fileName);
}

QVariantList backend::getHistory()
{
    if (!m_pendingHistoryRequest) {
        // No request in flight — fire a new one
        m_pendingHistoryRequest = true;
        m_service->requestHistory();
    }
    // Return current cache (will be updated asynchronously via onHistoryFetched)
    return m_latestHistory;
}

double backend::getDeviceValue(const QString &deviceName)
{
    return m_service->deviceValue(deviceName);
}

void backend::onSnapshotChanged(const DeviceSnapshot &snapshot)
{
    const DeviceSnapshot::ChangeFlags changes = m_snapshot.diff(snapshot);
    m_snapshot = snapshot;

    if (changes & DeviceSnapshot::LaserChanged) {
        emit laserPowerChanged();
    }
    if (changes & DeviceSnapshot::BrightnessChanged) {
        emit lightBrightnessChanged();
    }
    if (changes & DeviceSnapshot::FanChanged) {
        emit fanActiveChanged();
    }
    if (changes & DeviceSnapshot::OnlineChanged) {
        emit isOnlineChanged();
    }
}

void backend::onHistoryFetched(const QVariantList &history)
{
    m_pendingHistoryRequest = false;
    m_latestHistory = history;
    emit historyFetched(history);
}

// ── Modbus（QML ↔ 底层桥接）─────────────────────────────────────────────────

void backend::connectModbus(const QString &portName, int baudRate)
{
    m_service->connectModbus(portName, baudRate);
}

void backend::disconnectModbus()
{
    m_service->disconnectModbus();
}

void backend::writeModbusRegister(int slaveId, int addr, int value)
{
    m_service->writeModbusRegister(slaveId, addr, value);
}

bool backend::modbusConnected() const
{
    return m_service->modbusConnected();
}

bool backend::modbusReconnecting() const
{
    return m_service->modbusReconnecting();
}

QVariantMap backend::modbusRegisters() const
{
    return m_service->modbusRegisters();
}

void backend::onModbusDataChanged(int sensorId, double value)
{
    Q_UNUSED(sensorId)
    Q_UNUSED(value)
    emit modbusRegistersChanged();
    // QML 绑定 modbusRegisters 的属性会自动触发重新读取
}

void backend::onModbusConnectedChanged(bool connected)
{
    Q_UNUSED(connected)
    emit modbusConnectedChanged();
}

void backend::onModbusReconnectingChanged(bool reconnecting)
{
    Q_UNUSED(reconnecting)
    emit modbusReconnectingChanged();
}

void backend::onModbusSensorAlarm(QString msg)
{
    emit ModbusALarm(msg);
}
