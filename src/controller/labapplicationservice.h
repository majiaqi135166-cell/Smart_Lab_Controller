#ifndef LABAPPLICATIONSERVICE_H
#define LABAPPLICATIONSERVICE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>

#include "src/controller/labcontroller.h"
#include "src/domain/models/devicesnapshot.h"
#include "src/infrastructure/transport/tcpdevicechannel.h"
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

private slots:
    void handleDeviceLog();
    void onHistoryReady(QVariantList history);
    void onSnapshotChanged(const DeviceSnapshot &snapshot);
    void onTransportStatusChanged(TransportStatus status);
    void onTransportFrameReceived(const TransportFrame &frame);

private:
    void logToDatabase(const QString &target, const QString &action, const QString &details);
    void setupLogThread();
    void setupDbThread();
    void teardownThreads();
    void setupIOThread();

private:
    QTimer *m_deviceLogTimer;
    QThread *m_logThread;
    LogWorker *m_worker;
    QThread *m_dbThread;
    DatabaseWorker *m_dbWorker;
    QVariantList m_latestHistory;
    DeviceSnapshot m_snapshot;
    TcpDeviceChannel *m_transport;
    LabController *m_controller;
    QThread *m_IOThread;
};

#endif
