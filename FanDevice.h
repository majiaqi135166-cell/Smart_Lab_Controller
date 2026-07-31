    #ifndef FANDEVICE_H
#define FANDEVICE_H

#include "basedevice.h"

class FanDevice : public BaseDevice {
    Q_OBJECT

public:
    explicit FanDevice(QObject *parent = nullptr) 
        : BaseDevice("风扇设备", parent), m_active(false) {}
    
    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active;
                                  if(m_active)
                                emit statusChanged("旋转中");
                                  else
                                emit statusChanged("停止中");}
    
    void checkself() override {
        qDebug() <<m_deviceName<< (m_active ?"设备工作中...":"设备关闭中...");
    }

private:
    bool m_active;
};

#endif // FANDEVICE_H
