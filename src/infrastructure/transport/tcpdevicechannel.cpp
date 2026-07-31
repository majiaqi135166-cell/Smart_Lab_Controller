#include "tcpdevicechannel.h"

#include "ConfigManager.h"

TcpDeviceChannel::TcpDeviceChannel(QObject *parent)
    : ITransportChannel(parent)
{
    // Heartbeat watchdog
    m_heartbeatTimer.setSingleShot(true);
    m_heartbeatTimer.setInterval(10000);

    // Handshake retry
    m_handshakeRetryTimer.setSingleShot(true);
    m_handshakeRetryTimer.setInterval(HandshakeTimeoutMs);

    // Reconnect backoff
    m_reconnectTimer.setSingleShot(true);

    connect(&m_socket, &QTcpSocket::connected, this, &TcpDeviceChannel::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &TcpDeviceChannel::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &TcpDeviceChannel::onReadyRead);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &TcpDeviceChannel::onHeartbeatTimeout);
    connect(&m_handshakeRetryTimer, &QTimer::timeout, this, &TcpDeviceChannel::onHandshakeRetryTimeout);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &TcpDeviceChannel::onReconnectTimer);

    // Forward deserializer errors
    connect(&m_deserializer, &ProtocolDeserializer::errorOccurred,
            this, &ITransportChannel::errorOccurred);
}

void TcpDeviceChannel::start()
{
    updateState(ConnectionState::LoadingConfig, false);
    m_handshakeComplete = false;
    m_handshakeRetries = 0;
    m_reconnectDelayMs = MinReconnectDelayMs;
    m_reconnectTimer.stop();
    m_handshakeRetryTimer.stop();
    m_sendQueue.clear();

    updateState(ConnectionState::Connecting, false);
    m_socket.connectToHost(ConfigManager::instance()->serverIp(),
                           ConfigManager::instance()->serverPort());
}

void TcpDeviceChannel::stop()
{
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_handshakeRetryTimer.stop();
    m_handshakeComplete = false;
    m_sendQueue.clear();
    m_socket.abort();  // abort: immediately close, no wait
    updateState(ConnectionState::Idle, false);
}

bool TcpDeviceChannel::send(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return false;
    }

    // If connected and handshaken, try to write immediately
    if (m_socket.state() == QAbstractSocket::ConnectedState && m_handshakeComplete) {
        if (doWrite(payload)) {
            return true;
        }
        // Write failed — fall through to queue
    }

    // Queue the message for later delivery
    if (m_sendQueue.size() >= MaxSendQueueSize) {
        m_sendQueue.dequeue(); // drop oldest
        emit sendQueueDropped(1);
    }
    m_sendQueue.enqueue(payload);
    return false; // queued, not sent immediately
}

TransportStatus TcpDeviceChannel::status() const
{
    return m_status;
}

// ─── Slots ──────────────────────────────────────────────────────────────────

void TcpDeviceChannel::onConnected()
{
    m_reconnectDelayMs = MinReconnectDelayMs; // reset backoff on successful connect
    m_heartbeatTimer.stop();
    m_handshakeRetryTimer.stop();
    m_reconnectTimer.stop();

    updateState(ConnectionState::WaitingHandshake, false);
    sendHandshake();
    startHandshakeRetryTimer();
    m_heartbeatTimer.start();
}

void TcpDeviceChannel::onDisconnected()
{
    m_heartbeatTimer.stop();
    m_handshakeRetryTimer.stop();
    m_handshakeComplete = false;
    updateState(ConnectionState::Recovering, false);
    scheduleReconnect();
}

void TcpDeviceChannel::onReadyRead()
{
    m_deserializer.append(m_socket.readAll());

    const QVector<ProtocolMessage> messages = m_deserializer.takeMessages();
    if (messages.isEmpty()) {
        return;
    }

    // Any valid message resets the heartbeat watchdog
    m_heartbeatTimer.start();

    for (const ProtocolMessage &message : messages) {
        handleMessage(message);
    }
}

void TcpDeviceChannel::onHeartbeatTimeout()
{
    m_handshakeComplete = false;
    m_handshakeRetries = 0;
    m_handshakeRetryTimer.stop();
    updateState(ConnectionState::Recovering, false);
    emit errorOccurred(QStringLiteral("Heartbeat timeout — reconnecting"));
    m_socket.abort();
    scheduleReconnect();
}

void TcpDeviceChannel::onHandshakeRetryTimeout()
{
    if (m_handshakeComplete) {
        return; // already completed
    }

    if (m_handshakeRetries < MaxHandshakeRetries) {
        ++m_handshakeRetries;
        sendHandshake();
        startHandshakeRetryTimer();
    } else {
        // All retries exhausted — abort and reconnect
        emit errorOccurred(QStringLiteral("Handshake failed after %1 retries — reconnecting")
                               .arg(MaxHandshakeRetries));
        m_handshakeRetries = 0;
        m_handshakeRetryTimer.stop();
        m_handshakeComplete = false;
        m_socket.abort();
        scheduleReconnect();
    }
}

void TcpDeviceChannel::onReconnectTimer()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        return;
    }
    m_socket.connectToHost(ConfigManager::instance()->serverIp(),
                           ConfigManager::instance()->serverPort());
}

// ─── Private helpers ────────────────────────────────────────────────────────

void TcpDeviceChannel::updateState(ConnectionState state, bool online)
{
    m_status.state = state;
    m_status.online = online;
    emit statusChanged(m_status);
}

void TcpDeviceChannel::sendHandshake()
{
    doWrite(m_serializer.serializeHandshake());
}

void TcpDeviceChannel::handleMessage(const ProtocolMessage &message)
{
    TransportFrame frame;
    frame.message = message;

    if (message.type == ProtocolMessage::Handshake) {
        m_handshakeComplete = true;
        m_handshakeRetries = 0;
        m_handshakeRetryTimer.stop();
        updateState(ConnectionState::Ready, true);
        frame.handshakeAccepted = true;
        drainSendQueue();
        emit frameReceived(frame);
        return;
    }

    if (message.type == ProtocolMessage::Heartbeat) {
        if (!m_handshakeComplete) {
            m_handshakeComplete = true;
            m_handshakeRetries = 0;
            m_handshakeRetryTimer.stop();
            drainSendQueue();
        }
        updateState(ConnectionState::Ready, true);
        // Heartbeat is transport-layer — online state already propagated via statusChanged.
        // No need to emit frameReceived to the business layer.
        return;
    }

    if (!m_handshakeComplete) {
        updateState(ConnectionState::WaitingHandshake, false);
        return;
    }

    updateState(ConnectionState::Ready, true);
    emit frameReceived(frame);
}

void TcpDeviceChannel::drainSendQueue()
{
    while (!m_sendQueue.isEmpty()) {
        const QByteArray payload = m_sendQueue.dequeue();
        if (!doWrite(payload)) {
            // Put back at front and stop draining — we're likely disconnected
            m_sendQueue.prepend(payload);
            break;
        }
    }
}

bool TcpDeviceChannel::doWrite(const QByteArray &data)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    const qint64 written = m_socket.write(data);
    if (written < 0) {
        emit errorOccurred(QStringLiteral("Socket write error: %1").arg(m_socket.errorString()));
        return false;
    }
    if (written < data.size()) {
        // Partial write — flush to push the remainder out
        m_socket.flush();
        // If after flush there's still data buffered internally, QTcpSocket handles it.
        // We still count this as "sent" since the OS/kernel takes over.
    }
    return true;
}

void TcpDeviceChannel::scheduleReconnect()
{
    m_reconnectTimer.stop();
    m_reconnectTimer.setInterval(m_reconnectDelayMs);
    m_reconnectTimer.start();

    // Exponential backoff with upper bound
    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, static_cast<int>(MaxReconnectDelayMs));
}

void TcpDeviceChannel::startHandshakeRetryTimer()
{
    m_handshakeRetryTimer.start();
}
