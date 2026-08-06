#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>
#include <QVariantList>

#include "src/controller/labapplicationservice.h"
#include "src/domain/models/devicesnapshot.h"

class backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double laserPower READ laserPower WRITE setLaserPower NOTIFY laserPowerChanged)
    Q_PROPERTY(int lightBrightness READ lightBrightness WRITE setLightBrightness NOTIFY lightBrightnessChanged)
    Q_PROPERTY(bool isFanActive READ isFanActive WRITE setIsFanActive NOTIFY fanActiveChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)

    // Modbus 属性（供 QML 绑定）
    Q_PROPERTY(bool modbusConnected READ modbusConnected NOTIFY modbusConnectedChanged)
    Q_PROPERTY(QVariantMap modbusRegisters READ modbusRegisters NOTIFY modbusRegistersChanged)

public:
    explicit backend(QObject *parent = nullptr);
    ~backend() override;

    double laserPower() const;
    int lightBrightness() const;
    bool isFanActive() const;
    bool isOnline() const;

    void setLightBrightness(int value);
    void setIsFanActive(bool active);
    void setLaserPower(double power);

    Q_INVOKABLE void emergencyStop();
    Q_INVOKABLE void saveLog(const QString &message);
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void printCurrentJson();
    Q_INVOKABLE bool exportToCSV(QString fileName);
    Q_INVOKABLE QVariantList getHistory();
    Q_INVOKABLE double getDeviceValue(const QString &deviceName);

    // Modbus 操作（供 QML 调用）
    Q_INVOKABLE void connectModbus(const QString &portName, int baudRate);
    Q_INVOKABLE void disconnectModbus();
    Q_INVOKABLE void writeModbusRegister(int slaveId, int addr, int value);

    // Modbus 属性读取
    bool modbusConnected() const;
    QVariantMap modbusRegisters() const;

signals:
    void laserPowerChanged();
    void lightBrightnessChanged();
    void fanActiveChanged();
    void securityRisk();
    void isOnlineChanged();
    void remoteCommandReceived(QString msg);
    void historyUpdated(QVariantMap newEntry);
    void historyFetched(QVariantList history);
    void overplanpower(QString msg);

    // Modbus 信号（通知 QML）
    void modbusConnectedChanged();
    void modbusRegistersChanged();

private slots:
    void onSnapshotChanged(const DeviceSnapshot &snapshot);
    void onHistoryFetched(const QVariantList &history);
    void onModbusDataChanged(int sensorId, double value);
    void onModbusConnectedChanged(bool connected);

private:
    LabApplicationService *m_service;
    DeviceSnapshot m_snapshot;
    QVariantList m_latestHistory;
    bool m_pendingHistoryRequest = false;
};

#endif
