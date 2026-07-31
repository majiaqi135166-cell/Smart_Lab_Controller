#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QDebug>

class ConfigManager : public QObject
{
    Q_OBJECT
    // 定义属性，方便 QML 绑定
    Q_PROPERTY(QString serverIp READ serverIp WRITE setServerIp NOTIFY serverIpChanged)
    Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverPortChanged)
    Q_PROPERTY(double threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged)
public:
    // 1. 获取单例的唯一静态接口
    static ConfigManager* instance() {
        static ConfigManager _instance; // C++11 保证局部静态变量初始化是线程安全的
        return &_instance;
    }

    // 属性获取函数
    QString serverIp() const { return m_serverIp; }
    int serverPort() const { return m_serverPort; }
    double threshold() const { return m_threshold; }
    
    // 属性设置函数
    void setServerIp(const QString &ip);
    void setServerPort(int port);
    void setThreshold(double threshold);

    // 保存和加载
    void saveConfig();
    void loadConfig();

signals:
    void serverIpChanged();
    void serverPortChanged();
    void thresholdChanged();

private:
    // 2. 私有构造函数，防止外部实例化
    explicit ConfigManager(QObject *parent = nullptr);
    
    // 3. 禁止拷贝构造和赋值操作
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString m_serverIp;
    int m_serverPort;
    QSettings *m_settings;
    double m_threshold;
};

#endif // CONFIGMANAGER_H
