#ifndef DATABASEWORKER_H
#define DATABASEWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>
#include <QJsonObject>
#include <QDebug>

class DatabaseWorker : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseWorker(QObject *parent = nullptr);
    ~DatabaseWorker();

public slots:
    void init(); // 在新线程中初始化数据库连接
    void logToDatabase(const QString &level, const QString &target, const QString &action, const QString &details);
    void getHistory(); // 异步获取历史记录
    bool exportToCSV(const QString &fileName, const QVariantList &latestHistory);

    // ★ 新增：传感器数据入库（由 ModbusManager 跨线程触发）
    void logSensorData(int sensorId, double value);

signals:
    void historyReady(QVariantList history);
    void errorOccurred(const QString &error);
    void entryAdded(QVariantMap entry);

private:
    QSqlDatabase m_db;
    const QString m_connectionName = "worker_connection";
};

#endif // DATABASEWORKER_H
