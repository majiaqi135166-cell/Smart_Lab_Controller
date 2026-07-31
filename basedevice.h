#ifndef BASEDEVICE_H
#define BASEDEVICE_H

#include <QObject>
#include <QString>
#include <QDebug>
#include<FileUtil.h>

class BaseDevice : public QObject {
    Q_OBJECT
public:
    explicit BaseDevice(const QString &name, QObject *parent = nullptr) 
        : QObject(parent), m_deviceName(name) {}
    
    virtual ~BaseDevice() { 
        qDebug() << m_deviceName << " 销毁了。"; 
    }

    virtual void checkself() = 0;
    
    QString deviceName() const { return m_deviceName; }

signals:
    void deviceError(const QString &msg);
    void statusChanged(const QString &newStatus);


public:
    QString m_deviceName;
};

#endif // BASEDEVICE_H
