#ifndef INFRASTRUCTURE_TRANSPORT_ITRANSPORTCHANNEL_H
#define INFRASTRUCTURE_TRANSPORT_ITRANSPORTCHANNEL_H

#include <QObject>
#include <QByteArray>
#include "transporttypes.h"

class ITransportChannel : public QObject
{
    Q_OBJECT
public:
    explicit ITransportChannel(QObject *parent = nullptr) : QObject(parent) {}
    ~ITransportChannel() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool send(const QByteArray &payload) = 0;
    virtual TransportStatus status() const = 0;

signals:
    void frameReceived(const TransportFrame &frame);
    void errorOccurred(const QString &error);
    void statusChanged(TransportStatus status);
};

#endif
