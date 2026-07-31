#ifndef INFRASTRUCTURE_TRANSPORT_TRANSPORTTYPES_H
#define INFRASTRUCTURE_TRANSPORT_TRANSPORTTYPES_H

#include "src/domain/models/connectionstate.h"
#include "src/protocol/protocolmessage.h"

struct TransportStatus
{
    ConnectionState state = ConnectionState::Idle;
    bool online = false;
};

struct TransportFrame
{
    ProtocolMessage message;
    bool handshakeAccepted = false;
};

#endif
