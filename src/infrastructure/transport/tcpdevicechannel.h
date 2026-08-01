#ifndef INFRASTRUCTURE_TRANSPORT_TCPDEVICECHANNEL_H
#define INFRASTRUCTURE_TRANSPORT_TCPDEVICECHANNEL_H

#include <QQueue>
#include <QTimer>
#include <QTcpSocket>
#include "itransportchannel.h"
#include "src/protocol/protocoldeserializer.h"
#include "src/protocol/protocolserializer.h"

class TcpDeviceChannel : public ITransportChannel
{
    Q_OBJECT
public:
    explicit TcpDeviceChannel(QObject *parent = nullptr);

public slots:
    void start() override;
    void stop() override;

public:
    /// Send payload. Returns true if sent immediately, false if queued/offline.
    bool send(const QByteArray &payload) override;

    TransportStatus status() const override;

    /// Maximum number of outbound messages to queue while offline.
    enum : int { MaxSendQueueSize = 256 };

    /// Handshake retry limits.
    enum : int { MaxHandshakeRetries = 3 };
    enum : int { HandshakeTimeoutMs = 5000 };

    /// Reconnect backoff bounds (milliseconds).
    enum : int { MinReconnectDelayMs = 1000 };
    enum : int { MaxReconnectDelayMs = 30000 };

signals:
    void sendQueueDropped(int count);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onHeartbeatTimeout();
    void onHandshakeRetryTimeout();
    void onReconnectTimer();

private:
    void updateState(ConnectionState state, bool online);
    void sendHandshake();
    void handleMessage(const ProtocolMessage &message);
    void drainSendQueue();
    bool doWrite(const QByteArray &data);
    void scheduleReconnect();
    void startHandshakeRetryTimer();

private:
    QTcpSocket m_socket;
    QTimer m_heartbeatTimer;
    QTimer m_handshakeRetryTimer;
    QTimer m_reconnectTimer;
    TransportStatus m_status;
    ProtocolSerializer m_serializer;
    ProtocolDeserializer m_deserializer;
    QQueue<QByteArray> m_sendQueue;
    bool m_handshakeComplete = false;
    int m_handshakeRetries = 0;
    int m_reconnectDelayMs = MinReconnectDelayMs;
};

#endif
