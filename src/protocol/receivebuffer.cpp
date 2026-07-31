#include "receivebuffer.h"

ReceiveBuffer::ReceiveBuffer()
{
    m_buffer.reserve(4096);
}

bool ReceiveBuffer::append(const QByteArray &bytes)
{
    if (size() + bytes.size() > MaxBufferSize) {
        // Buffer full — reject new data to prevent memory exhaustion
        return false;
    }
    compactIfNeeded();
    m_buffer.append(bytes);
    return true;
}

bool ReceiveBuffer::canPeek(int size) const
{
    return canPeek(0, size);
}

bool ReceiveBuffer::canPeek(int offset, int size) const
{
    return size >= 0 && offset >= 0 && m_readOffset + offset + size <= m_buffer.size();
}

QByteArray ReceiveBuffer::peek(int size) const
{
    return peek(0, size);
}

QByteArray ReceiveBuffer::peek(int offset, int size) const
{
    if (!canPeek(offset, size)) {
        return {};
    }

    return QByteArray::fromRawData(m_buffer.constData() + m_readOffset + offset, size);
}

void ReceiveBuffer::consume(int size)
{
    if (size <= 0) {
        return;
    }

    m_readOffset += qMin(size, this->size());
    compactIfNeeded();
}

int ReceiveBuffer::size() const
{
    return m_buffer.size() - m_readOffset;
}

void ReceiveBuffer::clear()
{
    m_buffer.clear();
    m_readOffset = 0;
}

bool ReceiveBuffer::isFull() const
{
    return size() >= MaxBufferSize;
}

void ReceiveBuffer::compactIfNeeded()
{
    if (m_readOffset == 0) {
        return;
    }

    if (m_readOffset >= m_buffer.size()) {
        clear();
        return;
    }

    if (m_readOffset < 1024 && m_readOffset * 2 < m_buffer.size()) {
        return;
    }

    m_buffer.remove(0, m_readOffset);
    m_readOffset = 0;
}
