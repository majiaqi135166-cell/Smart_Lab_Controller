#ifndef DOMAIN_DEVICES_FANSTRATEGY_H
#define DOMAIN_DEVICES_FANSTRATEGY_H

#include "idevicestrategy.h"

class FanStrategy : public IDeviceStrategy
{
public:
    QString target() const override { return "fan"; }
    void apply(DeviceSnapshot &snapshot, const QString &action, const QVariant &value) override
    {
        if (action == "switch") {
            snapshot.fanActive = value.toBool();
        }
    }
};

#endif
