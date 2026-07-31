#ifndef LASERDEVICE_H
#define LASERDEVICE_H

#include "basedevice.h"

class LaserDevice : public BaseDevice {
    Q_OBJECT
public:
    explicit LaserDevice(QObject *parent = nullptr) 
        : BaseDevice("激光/光源设备", parent), m_power(0.0), m_brightness(0) {}
    
    double power() const { return m_power; }
    void setPower(double power) {
        if(power>180.0)
        {
           emit deviceError(QString("警告：激光功率过载！当前：%1").arg(m_power));
           emit statusChanged(QString("当前功率：%1  亮度:%2").arg(m_power).arg(m_brightness));
        }
        m_power = power; }
    
    int brightness() const { return m_brightness; }
    void setBrightness(int b) { m_brightness = b;
                              emit statusChanged(QString("当前功率：%1  亮度:%2").arg(m_power).arg(m_brightness));}
    
    void checkself() override {
        qDebug() <<m_deviceName<<(m_power?"工作中...":"待机中...");
        qDebug() << "当前功率：" << m_power << "，亮度：" << m_brightness;
    }

private:
    double m_power;
    int m_brightness;
};

#endif // LASERDEVICE_H
