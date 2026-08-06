#ifndef LABAPPLICATIONSERVICE_H
#define LABAPPLICATIONSERVICE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>

#include "src/controller/labcontroller.h"
#include "src/domain/models/devicesnapshot.h"
#include "src/infrastructure/transport/tcpdevicechannel.h"
#include "src/Modbus/modbusmanager.h"
#include "logworker.h"
#include "databaseworker.h"

struct SensorData
{
    double power;
    int brightness;
    SensorData(double p = 0.0, int b = 0) : power(p), brightness(b) {}
};

class LabApplicationService : public QObject
{
    Q_OBJECT
public:
    explicit LabApplicationService(QObject *parent = nullptr);
    ~LabApplicationService() override;

    const DeviceSnapshot &snapshot() const;
    QVariantList history() const;

    void setLightBrightness(int value);
      void setFanActive(bool active);
    void setLaserPower(double power);
    void updateSensorData(const SensorData &data);
    void emergencyStop();
    void saveLog(const QString &message);
    void clearLog();
    QByteArray currentSnapshotPacket() const;
    bool exportToCSV(const QString &fileName);
    void requestHistory();
    double deviceValue(const QString &deviceName) const;

    // Modbus 传感器访问接口
    ModbusManager *modbusManager() const { return m_modbusManager; }

    // Modbus 控制接口（供 QML 调用）
    Q_INVOKABLE void connectModbus(const QString &portName, int baudRate = 9600);
    Q_INVOKABLE void disconnectModbus();
    Q_INVOKABLE void writeModbusRegister(int slaveId, int addr, int value);

    // Modbus 实时数据（供 QML 绑定）
    QVariantMap modbusRegisters() const;
    bool modbusConnected() const { return m_modbusConnected; }

signals:
    void snapshotChanged(const DeviceSnapshot &snapshot);
    void securityRisk();
    void remoteCommandReceived(const QString &message);
    void historyUpdated(QVariantMap newEntry);
    void historyFetched(QVariantList history);
    void saveLogRequested(const QString &message);
    void clearLogRequested();
    void dbLogRequested(const QString &level, const QString &target, const QString &action, const QString &details);
    void dbHistoryRequested();
    void overplanpower(const QString &msg);

    // Modbus 实时信号（驱动 QML 界面刷新）
    void modbusDataChanged(int sensorId, double value);
    void modbusConnectedChanged(bool connected);

private slots:
    void handleDeviceLog();
    void onHistoryReady(QVariantList history);
    void onSnapshotChanged(const DeviceSnapshot &snapshot);
    void onTransportStatusChanged(TransportStatus status);
    void onTransportFrameReceived(const TransportFrame &frame);

    // Modbus 内部槽
    void onModbusDataReceived(int sensorId, double value);
    void onModbusConnected();
    void onModbusDisconnected();

private:
    void logToDatabase(const QString &target, const QString &action, const QString &details);
    void setupLogThread();
    void setupDbThread();
    void setupModbus();
    void teardownThreads();

private:
    QTimer *m_deviceLogTimer;
    QThread *m_logThread;
    LogWorker *m_worker;
    QThread *m_dbThread;
    DatabaseWorker *m_dbWorker;
    ModbusManager *m_modbusManager;
    bool m_modbusConnected = false;
    QMap<int, double> m_modbusRegisters;   // 寄存器地址 → 当前值
    QVariantList m_latestHistory;
    DeviceSnapshot m_snapshot;
    TcpDeviceChannel *m_transport;
    LabController *m_controller;
};

#endif
