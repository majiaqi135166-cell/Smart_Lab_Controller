#ifndef PROTOCOL_PROTOCOLSERIALIZER_H
#define PROTOCOL_PROTOCOLSERIALIZER_H

#include <QByteArray>
#include <QJsonObject>
#include "packetheader.h"
#include "protocolmessage.h"
#include "src/domain/models/controlcommand.h"
#include "src/domain/models/devicesnapshot.h"

class ProtocolSerializer
{
public:
    QByteArray serializeCommand(const ControlCommand &command) const;
    QByteArray serializeSnapshot(const DeviceSnapshot &snapshot) const;
    QByteArray serializeHandshake() const;
    QByteArray serializeHeartbeat() const;

private:
    QByteArray buildPacket(ProtocolMessage::Type type, const QJsonObject &payload = {}) const;
};

#endif
