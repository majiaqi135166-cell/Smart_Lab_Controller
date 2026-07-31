#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QThread>

class FileUtil
{
    static QMutex mutex;

public:
    /// Returns the log file path under the application directory.
    static QString logFilePath()
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/logs.txt");
    }

    static void writeLog(const QString &message)
    {
        QMutexLocker locker(&mutex);
        QFile file(logFilePath());
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            const QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            out << QString("[%1] %2\n").arg(time).arg(message);
            file.close();
        } else {
            qWarning() << "FileUtil: Failed to open log file for writing:" << file.errorString();
        }
    }

    static void clearLog()
    {
        QMutexLocker locker(&mutex);
        QFile file(logFilePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.close();
            qDebug() << "FileUtil: Log file cleared.";
        } else {
            qWarning() << "FileUtil: Failed to clear log file:" << file.errorString();
        }
    }
};

#endif // FILEUTIL_H
