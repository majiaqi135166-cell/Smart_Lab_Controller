#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "backend.h"
#include "ConfigManager.h"

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)
    QFile outfile(QCoreApplication::applicationDirPath() + QStringLiteral("/app_runtime.log"));
    if (!outfile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }
    QTextStream ts(&outfile);
    const QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ts << time << "[" << type << "]" << msg << endl;
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);
    QQmlApplicationEngine engine;

    backend backendObject(&app);
    engine.rootContext()->setContextProperty("config", ConfigManager::instance());
    engine.rootContext()->setContextProperty("cppbackend", &backendObject);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
