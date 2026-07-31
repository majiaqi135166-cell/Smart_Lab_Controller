#ifndef CONTROLLER_LABCONTROLLER_H
#define CONTROLLER_LABCONTROLLER_H

#include <QObject>
#include <QString>
#include <vector>
#include<memory>
#include <functional>

#include "src/domain/models/controlcommand.h"
#include "src/domain/models/devicesnapshot.h"
#include "src/protocol/protocolmessage.h"
#include "src/protocol/protocolserializer.h"

class IDeviceStrategy;

class LabController : public QObject
{
    Q_OBJECT
public:
    explicit LabController(std::function<void(const QByteArray &)> sendData, QObject *parent = nullptr);
    ~LabController() override;

    const DeviceSnapshot &snapshot() const;
    void setLaserPower(double power);
    void setLightBrightness(int brightness);
    void setFanActive(bool active);
    void emergencyStop();
    void processMessage(const ProtocolMessage &message);
    void setOnline(bool online);
    double deviceValue(const QString &deviceName) const;
    QByteArray currentSnapshotPacket() const;

signals:
    void snapshotChanged(const DeviceSnapshot &snapshot);
    void remoteMessage(const QString &message);
    void thresholdExceeded(const QString &message);

private:
    void applyCommand(const ControlCommand &command, bool remote);
    void emitSnapshot();
    void checkSecurity();

private:
    DeviceSnapshot m_snapshot;
    std::function<void(const QByteArray &)> m_sendData;
    ProtocolSerializer m_serializer;
    std::vector<std::unique_ptr<IDeviceStrategy>> m_strategies;
};

#endif
