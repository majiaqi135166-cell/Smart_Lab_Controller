#ifndef DOMAIN_MODELS_TELEMETRYFRAME_H
#define DOMAIN_MODELS_TELEMETRYFRAME_H

struct TelemetryFrame
{
    double laserPower = 0.0;
    int lightBrightness = 0;
    bool fanActive = false;
};

#endif
