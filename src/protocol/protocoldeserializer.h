#ifndef PROTOCOL_PROTOCOLDESERIALIZER_H
#define PROTOCOL_PROTOCOLDESERIALIZER_H

#include <QObject>
#include <QVector>
#include "packetheader.h"
#include "protocolmessage.h"
#include "receivebuffer.h"

/// Deserializes a byte stream into ProtocolMessage objects.
/// Emits errorOccurred on CRC mismatch, oversized payload, or buffer overflow.
class ProtocolDeserializer : public QObject
{
    Q_OBJECT
public:
    explicit ProtocolDeserializer(QObject *parent = nullptr) : QObject(parent) {}

    /// Append raw bytes from the socket. Drops data if the buffer is full.
    void append(const QByteArray &bytes);

    /// Extract all complete, valid messages currently in the buffer.
    QVector<ProtocolMessage> takeMessages();

signals:
    void errorOccurred(const QString &error);

private:
    bool tryReadHeader(PacketHeader &header) const;
    bool tryConsumeCorruptedPrefix();
    ProtocolMessage parseMessage(const PacketHeader &header, const QByteArray &payload) const;
    bool validateCrc(const PacketHeader &header, const QByteArray &payload) const;

private:
    ReceiveBuffer m_buffer;
};

#endif
