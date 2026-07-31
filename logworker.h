#ifndef LOGWORKER_H
#define LOGWORKER_H

#include <QObject>
#include <QDebug>
#include "FileUtil.h"

class LogWorker : public QObject
{
    Q_OBJECT
public:
    explicit LogWorker(QObject *parent = nullptr);

public slots:
    void doWritelog(const QString &message);
    void clearLog();

signals:

};

#endif // LOGWORKER_H
