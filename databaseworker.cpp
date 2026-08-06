#include "databaseworker.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include<QThread>

DatabaseWorker::DatabaseWorker(QObject *parent) : QObject(parent)
{
}

DatabaseWorker::~DatabaseWorker()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void DatabaseWorker::init()
{
    // 在子线程中创建数据库连接，必须指定唯一的连接名
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    const QString dbPath = QCoreApplication::applicationDirPath() + QStringLiteral("/lab_system.db");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        emit errorOccurred("数据库打开失败: " + m_db.lastError().text());
        return;
    }

    QSqlQuery query(m_db);

    // 设备操作日志表（已有）
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

    // ★ 新增：传感器数据表（Modbus 轮询入库）
    sql = "CREATE TABLE IF NOT EXISTS sensor_data("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "sensor_id INTEGER,"
          "value DOUBLE,"
          "timestamp DATETIME DEFAULT (datetime('now','localtime')))";
    if (!query.exec(sql)) {
        emit errorOccurred("建表失败(sensor_data): " + query.lastError().text());
        return;
    }

    qDebug() << "[DB Worker] 子线程数据库初始化成功 (devise_Logs + sensor_data)";
}

void DatabaseWorker::logToDatabase(const QString &level, const QString &target, const QString &action, const QString &details)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO devise_Logs (level, timestamp, device_name, action, val) "
                  "VALUES (:level, datetime('now', 'localtime'), :name, :action, :val)");
    query.bindValue(":level", level);
    query.bindValue(":name", target);
    query.bindValue(":action", action);
    query.bindValue(":val", details.toDouble());

    if (!query.exec()) {
        emit errorOccurred("插入失败: " + query.lastError().text());
    } else {
        qDebug() << "[DB Worker] INSERT OK [" << level << "]:" << target << action;

        // 发送信号告知主线程有新条目（用于 UI 实时更新）
        QVariantMap entry;
        entry.insert("level", level);
        entry.insert("time", "刚刚");
        entry.insert("device", target);
        entry.insert("action", action);
        entry.insert("val", details.toDouble());
        emit entryAdded(entry);
    }
}

void DatabaseWorker::getHistory()
{
    if (!m_db.isOpen()) return;

    QVariantList list;
    QSqlQuery query("SELECT timestamp, device_name, action, val, level FROM devise_Logs "
                    "ORDER BY id DESC LIMIT 10", m_db);
    
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

void DatabaseWorker::logSensorData(int sensorId, double value)
{
    if (!m_db.isOpen()) return;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO sensor_data (sensor_id, value) "
                  "VALUES (:sid, :val)");
    query.bindValue(":sid", sensorId);
    query.bindValue(":val", value);

    if (!query.exec()) {
        emit errorOccurred("传感器数据插入失败: " + query.lastError().text());
    } else {
        qDebug() << "[DB Worker] 🌡️ sensor" << sensorId
                 << "=" << value << "→ 已入库";
    }
}

bool DatabaseWorker::exportToCSV(const QString &fileName, const QVariantList &latestHistory)
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
