#ifndef PROTOCOL_PROTOCOLMESSAGE_H
#define PROTOCOL_PROTOCOLMESSAGE_H

#include <QJsonObject>

struct ProtocolMessage
{
    enum Type {
        Unknown = 0,
        Heartbeat = 1,
        Command = 2,
        Snapshot = 3,
        Handshake = 4
    };

    Type type = Unknown;
    QJsonObject payload;
};

#endif
