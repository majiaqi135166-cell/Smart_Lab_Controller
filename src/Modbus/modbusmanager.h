#ifndef MODBUSMANAGER_H
#define MODBUSMANAGER_H

#include <QObject>
#include <QModbusRtuSerialMaster>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSerialPort>
#include <QTimer>
#include <QQueue>
#include <QVector>
#include <QMap>
#include <QElapsedTimer>
#include <QDebug>
#include <QVariant>

#include "ConfigManager.h"

// ── 数据结构定义 ──────────────────────────────────────────────────────────

// 描述一个 Modbus 从站
struct ModbusSlaveConfig {
    int slaveId;            // 从站地址，如 1, 2, 3
    QString name;           // 人类可读的名称，"温湿度计"
    int startAddr;          // 起始寄存器地址
    int registerCount;      // 读取的寄存器数量
    int pollIntervalMs;     // 轮询间隔（毫秒），如 1000

    ModbusSlaveConfig(int id = 0, const QString &n = QString(),
                      int addr = 0, int cnt = 1, int interval = 1000)
        : slaveId(id), name(n), startAddr(addr)
        , registerCount(cnt), pollIntervalMs(interval) {}
};

// 任务队列中的一条请求
struct ModbusTask {
    enum Type { Read, Write };
    enum Priority { Normal = 0, High = 1 };

    Type type;
    Priority priority;
    int slaveId;
    int startAddr;
    int registerCount;
    quint16 writeValue;     // 仅 Write 时有效

    // 静态工厂方法：创建读取任务
    static ModbusTask makeRead(int slaveId, int startAddr, int count,
                                Priority prio = Normal) {
        ModbusTask t;
        t.type = Read;
        t.priority = prio;
        t.slaveId = slaveId;
        t.startAddr = startAddr;
        t.registerCount = count;
        t.writeValue = 0;
        return t;
    }

    // 静态工厂方法：创建写入任务（始终高优先级）
    static ModbusTask makeWrite(int slaveId, int addr, quint16 value) {
        ModbusTask t;
        t.type = Write;
        t.priority = High;          // 写入始终插队
        t.slaveId = slaveId;
        t.startAddr = addr;
        t.registerCount = 1;
        t.writeValue = value;
        return t;
    }
};

// 状态机：保证同一时刻只有一个请求在串口上
enum class QueueState {
    Idle,           // 空闲，可以发送下一个任务
    WaitingReply    // 正在等待从站回复
};


class ModbusManager : public QObject
{
    Q_OBJECT
public:
    explicit ModbusManager(QObject *parent = nullptr);
    ~ModbusManager() override;

    // ── 串口连接 ──
    bool connectToSlave(const QString &portName, int baudRate = 9600);
    void disconnectSlave();
    bool isConnected() const { return m_isConnected; }

    // ── 从站管理 ──
    void addSlave(const ModbusSlaveConfig &config);
    void removeSlave(int slaveId);
    void clearSlaves();
    QVector<ModbusSlaveConfig> slaves() const { return m_slaves; }

    // ── 对外接口（入队，不直接发）──
    void readHoldRegisters(int slaveId, int startAddr, int count);
    void writeSingleRegister(int slaveId, int addr, quint16 value);

signals:
    void dataReceived(int slaveId,int sensorId, double value);
    void taskCompleted(int slaveId, bool success, const QString &errorMsg);

    void connected();
    void disconnected();
    void reconnecting();
    void errorOccurred(const QString &error);

private slots:
    void onReplyFinished();
    void onStateChanged(QModbusDevice::State state);
    void onReconnectTimer();
    void onPollTimer();
    void onReplyTimeout();

private:
    void scheduleReconnect();
    void enqueue(const ModbusTask &task);
    void processQueue();

    // ── 常量 ──
    enum : int { MinReconnectDelayMs = 1000 };
    enum : int { MaxReconnectDelayMs = 30000 };
    enum : int { PollCheckIntervalMs = 200 };     // 每 200ms 检查哪些从站该轮询了
    enum : int { ReplyTimeoutMs = 2000 };          // 回复超时（比 Modbus 内部 timeout 长）

    // ── 串口 ──
    QModbusRtuSerialMaster *m_modbusClient = nullptr;
    bool m_isConnected = false;

    // ── 指数退避重连 ──
    QTimer *m_reconnectTimer = nullptr;
    int m_reconnectDelayMs = MinReconnectDelayMs;
    bool m_autoReconnect = true;
    QString m_lastPortName;
    int m_lastBaudRate = 9600;

    // ── 多从站配置 ──
    QVector<ModbusSlaveConfig> m_slaves;
    QMap<int, qint64> m_lastPollTime;   // slaveId → 上次轮询的时间戳(ms)

    // ── 任务队列 ──
    QQueue<ModbusTask> m_highQueue;     // 高优先级（写入）
    QQueue<ModbusTask> m_normalQueue;   // 普通优先级（轮询读取）
    QueueState m_queueState = QueueState::Idle;
    QModbusReply *m_pendingReply = nullptr;  // 当前在飞的请求
    ModbusTask::Type m_pendingTaskType = ModbusTask::Read;  // ★ 在飞任务的类型

    // ── 定时器 ──
    QTimer *m_pollTimer = nullptr;       // 统一轮询检查定时器
    QTimer *m_replyTimeoutTimer = nullptr; // 回复超时定时器
};

#endif // MODBUSMANAGER_H
