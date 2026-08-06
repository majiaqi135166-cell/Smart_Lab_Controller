#ifndef MODBUSMANAGER_H
#define MODBUSMANAGER_H

#include <QObject>
#include <QModbusRtuSerialMaster>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSerialPort>
#include <QTimer>
#include <QDebug>
#include <QVariant>

class ModbusManager : public QObject
{
    Q_OBJECT
public:
    explicit ModbusManager(QObject *parent = nullptr);
    ~ModbusManager() override;

    bool connectToSlave(const QString &portName, int baudRate = 9600);
    void disconnectSlave();

    void readHoldRegisters(int slaveId, int startAddr, int count);
    void writeSingleRegister(int slaveId, int addr, quint16 value);

    bool isConnected() const { return m_isConnected; }

    // 设置轮询参数：从站地址、起始寄存器、寄存器数量
    void setPollConfig(int slaveId, int startAddr, int count);

signals:
    // 核心信号：传感器数据就绪（跨线程传递到 DatabaseWorker）
    // sensorId = 寄存器地址，value = 原始寄存器值
    void dataReceived(int sensorId, double value);

    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private slots:
    void onReadReady();
    void onStateChanged(QModbusDevice::State state);

private:
    QModbusRtuSerialMaster *m_modbusClient = nullptr;
    QTimer *m_autoReadTimer = nullptr;

    bool m_isConnected = false;

    // 轮询配置
    int m_slaveId = 1;
    int m_startAddr = 0;
    int m_registerCount = 2;
};

#endif // MODBUSMANAGER_H
