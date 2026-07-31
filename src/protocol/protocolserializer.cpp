#include "protocolserializer.h"

#include <QDataStream>
#include <QJsonDocument>

QByteArray ProtocolSerializer::serializeCommand(const ControlCommand &command) const
{
    QJsonObject payload;
    payload.insert("target", command.target);
    payload.insert("action", command.action);
    payload.insert("value", QJsonValue::fromVariant(command.value));
    return buildPacket(ProtocolMessage::Command, payload);
}

QByteArray ProtocolSerializer::serializeSnapshot(const DeviceSnapshot &snapshot) const
{
    QJsonObject payload;
    payload.insert("laser_power", snapshot.laserPower);
    payload.insert("brightness", snapshot.lightBrightness);
    payload.insert("fan_active", snapshot.fanActive);
    payload.insert("online", snapshot.online);
    return buildPacket(ProtocolMessage::Snapshot, payload);
}

QByteArray ProtocolSerializer::serializeHandshake() const
{
    QJsonObject payload;
    payload.insert("role", QStringLiteral("lab-controller"));
    return buildPacket(ProtocolMessage::Handshake, payload);
}

QByteArray ProtocolSerializer::serializeHeartbeat() const
{
    return buildPacket(ProtocolMessage::Heartbeat);
}

QByteArray ProtocolSerializer::buildPacket(ProtocolMessage::Type type, const QJsonObject &payload) const
{
    const QByteArray body = payload.isEmpty() ? QByteArray() : QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // Refuse to serialize oversized payloads
    if (static_cast<quint32>(body.size()) > PacketHeader::MaxPayloadSize) {
        return {};
    }

    // Compute CRC over [type, payloadLength, payload] (everything after magic+version)
    const quint16 crc = PacketHeader::computeCrc16(static_cast<quint8>(type),
                                                   static_cast<quint32>(body.size()), body);

    QByteArray packet;
    packet.reserve(PacketHeader::EncodedSize + body.size());

    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint16>(PacketHeader::Magic)
           << static_cast<quint8>(PacketHeader::Version)
           << static_cast<quint8>(type)
           << static_cast<quint32>(body.size())
           << crc;

    if (!body.isEmpty()) {
        packet.append(body);
    }

    return packet;
}
