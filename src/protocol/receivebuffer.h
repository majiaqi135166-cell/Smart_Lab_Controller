#ifndef PROTOCOL_RECEIVEBUFFER_H
#define PROTOCOL_RECEIVEBUFFER_H

#include <QByteArray>

class ReceiveBuffer
{
public:
    static constexpr int MaxBufferSize = 1024 * 1024; // 1 MB — reject larger to prevent memory exhaustion

    ReceiveBuffer();

    /// Append bytes. Returns false and does nothing if the buffer is already full.
    bool append(const QByteArray &bytes);
    bool canPeek(int size) const;
    bool canPeek(int offset, int size) const;
    QByteArray peek(int size) const;
    QByteArray peek(int offset, int size) const;
    void consume(int size);
    int size() const;
    void clear();
    bool isFull() const;

private:
    void compactIfNeeded();

private:
    QByteArray m_buffer;
    int m_readOffset = 0;
};

#endif
