#ifndef DOMAIN_DEVICES_LASERSTRATEGY_H
#define DOMAIN_DEVICES_LASERSTRATEGY_H

#include "idevicestrategy.h"

class LaserStrategy : public IDeviceStrategy
{
public:
    QString target() const override { return "laser"; }
    void apply(DeviceSnapshot &snapshot, const QString &action, const QVariant &value) override
    {
        if (action == "set_power") {
            snapshot.laserPower = value.toDouble();
            return;
        }
        if (action == "set_brightness") {
            snapshot.lightBrightness = value.toInt();
        }
    }
};

#endif
