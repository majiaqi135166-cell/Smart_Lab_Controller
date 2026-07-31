#include "labcontroller.h"

#include "ConfigManager.h"
#include "src/domain/devices/fanstrategy.h"
#include "src/domain/devices/idevicestrategy.h"
#include "src/domain/devices/laserstrategy.h"

LabController::LabController(std::function<void(const QByteArray &)> sendData, QObject *parent)
    : QObject(parent)
    , m_sendData(std::move(sendData))
{
    m_strategies.push_back(std::make_unique<LaserStrategy>());
    m_strategies.push_back(std::make_unique<FanStrategy>());
}

LabController::~LabController() = default;

const DeviceSnapshot &LabController::snapshot() const
{
    return m_snapshot;
}

void LabController::setLaserPower(double power)
{
    applyCommand({"laser", "set_power", power}, false);
}

void LabController::setLightBrightness(int brightness)
{
    applyCommand({"laser", "set_brightness", brightness}, false);
}

void LabController::setFanActive(bool active)
{
    applyCommand({"fan", "switch", active}, false);
}

void LabController::emergencyStop()
{
    applyCommand({"laser", "set_brightness", 0}, false);
    applyCommand({"fan", "switch", false}, false);
    applyCommand({"laser", "set_power", 0.0}, false);
}

void LabController::processMessage(const ProtocolMessage &message)
{
    // Heartbeat and Handshake are transport-layer concerns —
    // online state is managed via onTransportStatusChanged → setOnline().
    if (message.type != ProtocolMessage::Command) {
        return;
    }

    const QString target = message.payload.value("target").toString();
    const QString action = message.payload.value("action").toString();
    QVariant value;
    if (message.payload.contains("value")) {
        value = message.payload.value("value").toVariant();
    } else if (message.payload.contains("status")) {
        value = message.payload.value("status").toBool();
    }

    applyCommand({target, action, value}, true);
}

void LabController::setOnline(bool online)
{
    if (m_snapshot.online == online) {
        return;
    }

    m_snapshot.online = online;
    emitSnapshot();
}

double LabController::deviceValue(const QString &deviceName) const
{
    if (deviceName == "laser") {
        return m_snapshot.laserPower;
    }
    if (deviceName == "fan") {
        return m_snapshot.fanActive ? 1.0 : 0.0;
    }
    if (deviceName == "brightness") {
        return static_cast<double>(m_snapshot.lightBrightness);
    }
    return 0.0;
}

QByteArray LabController::currentSnapshotPacket() const
{
    return m_serializer.serializeSnapshot(m_snapshot);
}

void LabController::applyCommand(const ControlCommand &command, bool remote)
{
    bool handled = false;
    for (const auto &ptr : m_strategies) {
        if (ptr->target() != command.target) {
            continue;
        }

        ptr->apply(m_snapshot, command.action, command.value);
        handled = true;

        if (!remote && m_sendData) {
            m_sendData(m_serializer.serializeCommand(command));
        }

        if (remote) {
            const double threshold = ConfigManager::instance()->threshold();

            if (command.target == "laser" && command.action == "set_power"
                && command.value.toDouble() > threshold) {
                emit thresholdExceeded(QStringLiteral("激光功率超过%1W").arg(threshold));
            }

            if (command.target == "laser" && command.action == "set_power") {
                emit remoteMessage(QStringLiteral("远程控制：激光功率已设为 %1W").arg(command.value.toDouble()));
            } else if (command.target == "fan" && command.action == "switch") {
                emit remoteMessage(QStringLiteral("已经远程控制：风扇已 %1")
                                      .arg(command.value.toBool() ? QStringLiteral("开启") : QStringLiteral("关闭")));
            }
        }

        checkSecurity();
        emitSnapshot();
        return;
    }

    if (remote && !handled) {
        emit remoteMessage(QStringLiteral("未知设备指令: target=%1 action=%2").arg(command.target, command.action));
    }
}

void LabController::emitSnapshot()
{
    emit snapshotChanged(m_snapshot);
}

void LabController::checkSecurity()
{
    if (m_snapshot.lightBrightness > 80 && !m_snapshot.fanActive) {
        emit thresholdExceeded(QStringLiteral("高亮度下风扇未开启"));
    }
}
