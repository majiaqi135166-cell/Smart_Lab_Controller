#include "protocoldeserializer.h"

#include <QDataStream>
#include <QJsonDocument>

void ProtocolDeserializer::append(const QByteArray &bytes)
{
    if (!m_buffer.append(bytes)) {
        emit errorOccurred(QStringLiteral("Receive buffer overflow (%1 bytes) — discarding new data")
                               .arg(m_buffer.size()));
    }
}

QVector<ProtocolMessage> ProtocolDeserializer::takeMessages()
{
    QVector<ProtocolMessage> messages;

    while (true) {
        PacketHeader header;
        if (!tryReadHeader(header)) {
            if (!tryConsumeCorruptedPrefix()) {
                break;
            }
            continue;
        }

        // Reject oversized payloads to prevent memory exhaustion
        if (header.payloadLength > PacketHeader::MaxPayloadSize) {
             // ★ 用 qint64 计算：quint32 转 int 会截断成负数，导致 skipSize ≤ 0 → 循环无法推进
             const qint64 declaredTotal = static_cast<qint64>(PacketHeader::EncodedSize)
                                        + static_cast<qint64>(header.payloadLength);
             if (declaredTotal <= m_buffer.size()) {
                 m_buffer.consume(static_cast<int>(declaredTotal));   // 整包已到齐：精确丢弃
             } else {
                 m_buffer.consume(PacketHeader::EncodedSize);
                 m_buffer.consume(m_buffer.size());   // 声明长度 > 缓冲上限，这帧永远收不全
             }
             emit errorOccurred(QStringLiteral("Oversized payload declared (%1 bytes) — packet discarded")
                                    .arg(header.payloadLength));
             continue;   // 此时缓冲必为空或已对齐，循环必然收敛
         }

        const int packetSize = PacketHeader::EncodedSize + static_cast<int>(header.payloadLength);
        if (!m_buffer.canPeek(packetSize)) {
            break;
        }

        const QByteArray payload = m_buffer.peek(PacketHeader::EncodedSize, static_cast<int>(header.payloadLength));
        m_buffer.consume(packetSize);

        // CRC integrity check
        if (!validateCrc(header, payload)) {
            emit errorOccurred(QStringLiteral("CRC mismatch — packet discarded"));
            continue;
        }

        messages.append(parseMessage(header, payload));
    }

    return messages;
}

bool ProtocolDeserializer::tryReadHeader(PacketHeader &header) const
{
    if (!m_buffer.canPeek(PacketHeader::EncodedSize)) {
        return false;
    }

    QByteArray headerBytes = m_buffer.peek(PacketHeader::EncodedSize);
    QDataStream stream(headerBytes);
    stream.setByteOrder(QDataStream::BigEndian);
    stream >> header.magic >> header.version >> header.type >> header.payloadLength >> header.crc16;

    return header.magic == PacketHeader::Magic && header.version == PacketHeader::Version;
}

bool ProtocolDeserializer::tryConsumeCorruptedPrefix()
{
    if (!m_buffer.canPeek(2)) {
        return false;
    }

    if (!m_buffer.canPeek(PacketHeader::EncodedSize))
             return false;

    const QByteArray available = m_buffer.peek(m_buffer.size());

    // Magic bytes in network byte order (0xAB 0xCD)
    const char magicBytes[2] = {
        static_cast<char>((PacketHeader::Magic >> 8) & 0xFF),
        static_cast<char>(PacketHeader::Magic & 0xFF)
    };

    // Search for the magic marker in the buffer
    for (int i = 2; i < available.size(); ++i) {
        if (static_cast<quint8>(available.at(i - 1)) == static_cast<quint8>(magicBytes[0])
            && static_cast<quint8>(available.at(i)) == static_cast<quint8>(magicBytes[1])) {
            m_buffer.consume(i - 1);
            return true;
        }
    }

    // No magic found — discard all but the last byte (which might be the first half of magic)
    m_buffer.consume(qMax(0, available.size() - 1));
    return false;
}

bool ProtocolDeserializer::validateCrc(const PacketHeader &header, const QByteArray &payload) const
{
    const quint16 expected = PacketHeader::computeCrc16(header.type, header.payloadLength, payload);
    return expected == header.crc16;
}

ProtocolMessage ProtocolDeserializer::parseMessage(const PacketHeader &header, const QByteArray &payload) const
{
    ProtocolMessage message;
    message.type = static_cast<ProtocolMessage::Type>(header.type);

    if (payload.isEmpty()) {
        return message;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (document.isObject()) {
        message.payload = document.object();
    } else if (document.isNull() && !payload.isEmpty()) {
        // Malformed JSON — not an object. Keep the empty payload.
    }
    return message;
}
