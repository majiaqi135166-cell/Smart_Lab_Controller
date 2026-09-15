#include "databaseworker.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QThread>

// ═══════════════════════════════════════════════════════════════════════════
// 构造 & 析构
// ═══════════════════════════════════════════════════════════════════════════

DatabaseWorker::DatabaseWorker(QObject *parent)
    : QObject(parent)
{
    // 预分配缓冲区，避免频繁扩容
    m_buffer.reserve(BufferMaxSize + 10);
}

DatabaseWorker::~DatabaseWorker()
{
    // ★ 析构前兜底：把缓冲里最后的数据落库
    flushBuffer();

    if (m_db.isOpen()) {
        m_db.close();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 初始化（在 m_dbThread 中执行）
// ═══════════════════════════════════════════════════════════════════════════

void DatabaseWorker::init()
{
    // 在子线程中创建数据库连接，必须指定唯一的连接名
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    const QString dbPath = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/lab_system.db");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        emit errorOccurred("数据库打开失败: " + m_db.lastError().text());
        return;
    }

    QSqlQuery query(m_db);

    // 设备操作日志表
    QString sql = "CREATE TABLE IF NOT EXISTS devise_Logs("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "level TEXT,"
                  "timestamp DATETIME,"
                  "device_name TEXT,"
                  "action TEXT,"
                  "val DOUBLE)";
    if (!query.exec(sql)) {
        emit errorOccurred("建表失败(devise_Logs): " + query.lastError().text());
        return;
    }

    // 传感器数据表
    sql = "CREATE TABLE IF NOT EXISTS sensor_data("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "slave_id INTEGER,"
          "sensor_id INTEGER,"
          "value DOUBLE,"
          "timestamp DATETIME DEFAULT (datetime('now','localtime')))";
    if (!query.exec(sql)) {
        emit errorOccurred("建表失败(sensor_data): " + query.lastError().text());
        return;
    }

    // ── 初始化批量落库定时器 ──
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(FlushIntervalMs);
    connect(m_flushTimer, &QTimer::timeout,
            this, &DatabaseWorker::flushBuffer);
    m_flushTimer->start();

    qDebug() << "[DB Worker] 初始化成功 "
             << "(sensor_data 批量缓冲:" << BufferMaxSize << "条 /"
             << FlushIntervalMs / 1000 << "秒)";
}

// ═══════════════════════════════════════════════════════════════════════════
// 设备操作日志（逐条落库，保持原有逻辑不变）
// ═══════════════════════════════════════════════════════════════════════════

void DatabaseWorker::logToDatabase(const QString &level, const QString &target,
                                    const QString &action, const QString &details)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO devise_Logs (level, timestamp, device_name, "
                  "action, val) "
                  "VALUES (:level, datetime('now', 'localtime'), "
                  ":name, :action, :val)");
    query.bindValue(":level", level);
    query.bindValue(":name", target);
    query.bindValue(":action", action);
    query.bindValue(":val", details.toDouble());

    if (!query.exec()) {
        emit errorOccurred("插入失败(devise_Logs): " + query.lastError().text());
        return;
    }

    // 通知主线程 UI 有新条目
    QVariantMap entry;
    entry.insert("level", level);
    entry.insert("time", "刚刚");
    entry.insert("device", target);
    entry.insert("action", action);
    entry.insert("val", details.toDouble());
    emit entryAdded(entry);
}

// ═══════════════════════════════════════════════════════════════════════════
// 历史查询 & CSV 导出（未改动）
// ═══════════════════════════════════════════════════════════════════════════

void DatabaseWorker::getHistory()
{
    if (!m_db.isOpen()) return;

    QVariantList list;
    QSqlQuery query("SELECT timestamp, device_name, action, val, level "
                    "FROM devise_Logs ORDER BY id DESC LIMIT 10", m_db);

    while (query.next()) {
        QVariantMap map;
        map.insert("time", query.value(0).toString());
        map.insert("device", query.value(1).toString());
        map.insert("action", query.value(2).toString());
        map.insert("val", query.value(3).toString());
        map.insert("level", query.value(4).toString());
        list.append(map);
    }

    emit historyReady(list);
}

bool DatabaseWorker::exportToCSV(const QString &fileName,
                                  const QVariantList &latestHistory)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "导出失败：无法打开文件进行写入:" << fileName;
        return false;
    }

    QTextStream out(&file);
    out.setGenerateByteOrderMark(true);
    out << "时间,设备,操作,数值,级别\n";

    for (const QVariant &var : latestHistory) {
        const QVariantMap entry = var.toMap();
        out << entry.value("time").toString() << ","
            << entry.value("device").toString() << ","
            << entry.value("action").toString() << ","
            << entry.value("val").toString() << ","
            << entry.value("level").toString() << "\n";
    }

    file.close();
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// ★ 传感器数据：缓冲 → 批量落库（核心改动）
// ═══════════════════════════════════════════════════════════════════════════

void DatabaseWorker::logSensorData(int slaveId,int sensorId, double value)
{
    if (!m_db.isOpen()) return;

    // ── 入缓冲（微秒级，不碰磁盘）──
    m_buffer.append(SensorRecord(slaveId, sensorId, value));

    // ── 检查是否攒够了 ──
    if (m_buffer.size() >= BufferMaxSize) {
        flushBuffer();
        // flushBuffer 内部会重置定时器，保证不和定时间隔竞争
    }
}

void DatabaseWorker::flushBuffer()
{
    if (m_buffer.isEmpty()) return;
    if (!m_db.isOpen()) return;

    const int count = m_buffer.size();

    // ── 开启事务 ──
    if (!m_db.transaction()) {
        emit errorOccurred("事务开启失败: " + m_db.lastError().text());
        // 开启失败不清空缓冲，等下次触发再试
        return;
    }

    // ── 预编译 SQL（只解析一次）──
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO sensor_data (slave_id, sensor_id, value) "
                  "VALUES (:slave, :sid, :val)");

    // ── 构建批量参数列表 ──
    QVariantList slaves, ids, vals;
    slaves.reserve(count);
    ids.reserve(count);
    vals.reserve(count);
    for (const SensorRecord &rec : m_buffer) {
        slaves.append(rec.slaveId);
        ids.append(rec.sensorId);
        vals.append(rec.value);
    }

    query.bindValue(":slave", slaves);  // 绑定整列
    query.bindValue(":sid", ids);
    query.bindValue(":val", vals);

    // ── 批量执行 ──
    if (!query.execBatch()) {
        qDebug() << "[DB Worker] ❌ 批量写入失败:"
                 << query.lastError().text()
                 << "— 回滚事务";
        m_db.rollback();
        emit errorOccurred("传感器批量写入失败: " + query.lastError().text());
        // 不清空缓冲！等下次再试
        return;
    }

    // ── 提交事务（一次 fsync）──
    if (!m_db.commit()) {
        qDebug() << "[DB Worker] ❌ 事务提交失败:"
                 << m_db.lastError().text();
        m_db.rollback();
        emit errorOccurred("事务提交失败: " + m_db.lastError().text());
        return;
    }

    qDebug() << "[DB Worker] ✅ 批量写入" << count << "条传感器数据，已提交";

    m_buffer.clear();

    // 重置定时器：避免刚写完又触发定时器空跑一次
    if (m_flushTimer) {
        m_flushTimer->start();
    }

    emit bufferFlushed(count);
}
