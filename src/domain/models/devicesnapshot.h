#ifndef DOMAIN_MODELS_DEVICESNAPSHOT_H
#define DOMAIN_MODELS_DEVICESNAPSHOT_H

#include <QtGlobal>

struct DeviceSnapshot
{
    double laserPower = 0.0;
    int lightBrightness = 0;
    bool fanActive = false;
    bool online = false;

    enum ChangeFlag {
        NoChange        = 0,
        LaserChanged    = 1 << 0,
        BrightnessChanged = 1 << 1,
        FanChanged      = 1 << 2,
        OnlineChanged   = 1 << 3
    };
    Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag)

    /// Compare with another snapshot and return a bitmask of what changed.
    ChangeFlags diff(const DeviceSnapshot &other) const
    {
        ChangeFlags flags = NoChange;
        if (!qFuzzyCompare(laserPower + 1.0, other.laserPower + 1.0)) {
            flags |= LaserChanged;
        }
        if (lightBrightness != other.lightBrightness) {
            flags |= BrightnessChanged;
        }
        if (fanActive != other.fanActive) {
            flags |= FanChanged;
        }
        if (online != other.online) {
            flags |= OnlineChanged;
        }
        return flags;
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DeviceSnapshot::ChangeFlags)

#endif
