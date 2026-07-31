#ifndef DOMAIN_DEVICES_IDEVICESTRATEGY_H
#define DOMAIN_DEVICES_IDEVICESTRATEGY_H

#include <QString>
#include <QVariant>
#include "src/domain/models/devicesnapshot.h"

class IDeviceStrategy
{
public:
    virtual ~IDeviceStrategy() = default;
    virtual QString target() const = 0;
    virtual void apply(DeviceSnapshot &snapshot, const QString &action, const QVariant &value) = 0;
};

#endif
