#ifndef DOMAIN_MODELS_CONTROLCOMMAND_H
#define DOMAIN_MODELS_CONTROLCOMMAND_H

#include <QString>
#include <QVariant>

struct ControlCommand
{
    QString target;
    QString action;
    QVariant value;
};

#endif
