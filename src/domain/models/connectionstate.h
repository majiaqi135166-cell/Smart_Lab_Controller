#ifndef DOMAIN_MODELS_CONNECTIONSTATE_H
#define DOMAIN_MODELS_CONNECTIONSTATE_H

enum class ConnectionState
{
    Idle,
    LoadingConfig,
    Connecting,
    WaitingHandshake,
    Ready,
    Recovering
};

#endif
