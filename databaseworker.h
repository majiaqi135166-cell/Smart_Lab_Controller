#ifndef DATABASEWORKER_H
#define DATABASEWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QTimer>
#include <QVector>
#include <QVariantList>
#include <QJsonObject>
#include <QDebug>

// 缓冲区中的数据条目
struct SensorRecord {
    int slaveId;      // 从站地址（数据来源）
    int sensorId;     // 寄存器地址
    double value;

    SensorRecord(int sid = 0, int reg = 0, double val = 0.0)
        : slaveId(sid), sensorId(reg), value(val) {}
};

class DatabaseWorker : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseWorker(QObject *parent = nullptr);
    ~DatabaseWorker();

public slots:
    void init();
    void logToDatabase(const QString &level, const QString &target,
                       const QString &action, const QString &details);
    void getHistory();
    bool exportToCSV(const QString &fileName, const QVariantList &latestHistory);

    // 传感器数据入库（入缓冲 → 批量落库）
    void logSensorData(int slaveId,int sensorId, double value);

signals:
    void historyReady(QVariantList history);
    void errorOccurred(const QString &error);
    void entryAdded(QVariantMap entry);

    // 批量落库完成信号（可选，用于日志）
    void bufferFlushed(int recordCount);

private:
    void flushBuffer();

    QSqlDatabase m_db;
    const QString m_connectionName = "worker_connection";

    // ── 批量写入缓冲区 ──
    QVector<SensorRecord> m_buffer;
    QTimer *m_flushTimer = nullptr;

    // 可配置的阈值
    enum : int { BufferMaxSize = 100 };       // 攒满 100 条触发
    enum : int { FlushIntervalMs = 5000 };    // 或 5 秒触发
};

#endif // DATABASEWORKER_H
