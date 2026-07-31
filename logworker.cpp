#include "logworker.h"

LogWorker::LogWorker(QObject *parent) : QObject(parent)
{
}

void LogWorker::doWritelog(const QString &message)
{
    FileUtil::writeLog(message);
    qDebug() << "后台线程：" << message << " 已记入磁盘";
}

void LogWorker::clearLog()
{
    FileUtil::clearLog();
}
