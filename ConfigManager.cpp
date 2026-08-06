#include "ConfigManager.h"

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_serverIp(QStringLiteral("127.0.0.1"))
    , m_serverPort(8080)
    , m_threshold(200.0)
    , m_modbusPort(QStringLiteral("COM1"))
{
    m_settings = new QSettings("config.ini", QSettings::IniFormat, this);
    loadConfig();
}

void ConfigManager::loadConfig()
{
    m_serverIp = m_settings->value("Network/IP", "127.0.0.1").toString();
    m_serverPort = m_settings->value("Network/Port", 8080).toInt();
    m_threshold = m_settings->value("Safety/Threshold", 200.0).toDouble();
    m_modbusPort = m_settings->value("Modbus/Port", "COM1").toString();
    qDebug() << "Config Loaded: " << m_serverIp << ":" << m_serverPort
             << "threshold:" << m_threshold << "modbusPort:" << m_modbusPort;
}

void ConfigManager::saveConfig()
{
    m_settings->setValue("Network/IP", m_serverIp);
    m_settings->setValue("Network/Port", m_serverPort);
    m_settings->setValue("Safety/Threshold", m_threshold);
    m_settings->sync();
    qDebug() << "Config Saved!";
}

void ConfigManager::setServerIp(const QString &ip)
{
    if (m_serverIp != ip) {
        m_serverIp = ip;
        emit serverIpChanged();
        saveConfig();
    }
}

void ConfigManager::setServerPort(int port)
{
    if (m_serverPort != port) {
        m_serverPort = port;
        emit serverPortChanged();
        saveConfig();
    }
}

void ConfigManager::setThreshold(double threshold)
{
    if (!qFuzzyCompare(m_threshold, threshold)) {
        m_threshold = threshold;
        emit thresholdChanged();
        saveConfig();
    }
}

void ConfigManager::setModbusPort(const QString &port)
{
    if (m_modbusPort != port) {
        m_modbusPort = port;
        emit modbusPortChanged();
        saveConfig();
    }
}
