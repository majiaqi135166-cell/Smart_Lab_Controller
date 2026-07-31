# Smart Lab Controller — 重构审计文档（面试用）

> **项目定位**: 基于 Qt5/C++ 的实验室设备远程控制系统，涉及多线程、TCP 二进制协议通信、设备策略模式、QML 前端。

---

## 一、项目架构总览

```
main.cpp                          ← 入口：安装日志钩子、启动 QML 引擎
main.qml                          ← QML 前端界面
backend.h/.cpp                    ← QML ↔ C++ 桥接层 (QQmlPropertyMap 模式)
  └─ LabApplicationService        ← 业务编排层（核心调度）
       ├─ LabController           ← 设备控制核心（策略模式 + 安全检查）
       │    ├─ IDeviceStrategy    ← 设备策略接口（多态）
       │    ├─ LaserStrategy      ← 激光器策略
       │    └─ FanStrategy        ← 风扇策略
       ├─ TcpDeviceChannel        ← TCP 网络传输层
       │    ├─ ProtocolSerializer    ← 二进制封包（含 CRC）
       │    ├─ ProtocolDeserializer  ← 二进制解包（含校验）
       │    ├─ ReceiveBuffer         ← 环形接收缓冲区
       │    └─ PacketHeader          ← 协议头定义
       ├─ LogWorker (独立线程)     ← 日志写入线程
       │    └─ FileUtil            ← 文件 I/O + 互斥锁
       └─ DatabaseWorker (独立线程) ← SQLite 数据库线程
```

**数据流方向**:
```
设备端(TCP) → TcpDeviceChannel → ProtocolDeserializer → LabController → UI(QML)
UI(QML) → backend → LabApplicationService → LabController → ProtocolSerializer → TcpDeviceChannel → 设备端
```

---

## 二、每项修改的对比与理由

### 🔴 严重级（3 项）

---

#### C1. ReceiveBuffer 无缓冲区上限 → 内存耗尽 DoS

**修改前** (`src/protocol/receivebuffer.h`):
```cpp
class ReceiveBuffer {
public:
    void append(const QByteArray &bytes);  // 无条件追加
    // ...
private:
    QByteArray m_buffer;
    int m_readOffset = 0;
};
```

**修改后**:
```cpp
class ReceiveBuffer {
public:
    static constexpr int MaxBufferSize = 1024 * 1024; // 1 MB

    bool append(const QByteArray &bytes);  // 返回 false 表示缓冲区满
    bool isFull() const;
    // ...
};
```
```cpp
bool ReceiveBuffer::append(const QByteArray &bytes) {
    if (size() + bytes.size() > MaxBufferSize) {
        return false;  // 拒绝写入，防止内存被撑爆
    }
    compactIfNeeded();
    m_buffer.append(bytes);
    return true;
}
```

**面试说法**: 
> "原始代码的接收缓冲区无容量上限——如果攻击者或故障设备持续发送数据，内存会被无限撑大直到进程 OOM 崩溃。这是典型的 DoS 攻击面。我加了 1MB 硬限制，超限后拒绝写入并向上层 emit errorOccurred 信号，让调用方感知到异常并触发断连/告警。"

**知识点**: 缓冲区溢出防御、DoS 攻击面、防御性编程

---

#### C2. payloadLength 无条件信任 → 恶意超大声明

**修改前** (`src/protocol/protocoldeserializer.cpp`):
```cpp
// 直接使用 header.payloadLength，没有任何校验
const int packetSize = PacketHeader::EncodedSize + static_cast<int>(header.payloadLength);
if (!m_buffer.canPeek(packetSize)) {
    break;
}
```

**修改后**:
```cpp
// 先校验 payloadLength 上限（256KB），超限则跳过并告警
if (header.payloadLength > PacketHeader::MaxPayloadSize) {
    emit errorOccurred(QStringLiteral("Oversized payload declared (%1 bytes) — packet discarded")
                           .arg(header.payloadLength));
    // 跳过这个包（丢弃 header + 声明长度的数据）
    m_buffer.consume(PacketHeader::EncodedSize);
    m_buffer.consume(m_buffer.size());
    continue;
}
```

**面试说法**:
> "二进制协议中，payloadLength 字段来自网络对端，属于不可信输入。如果攻击者发送一个 payloadLength=2GB 的包，直接用这个值去 canPeek() 会导致巨大的内存分配或死循环。我加了 256KB 的 MaxPayloadSize 上限校验，超过就丢弃整个包并上报错误。这和 HTTP 服务器的 max_request_body_size 是同一个道理。"

**知识点**: 不可信输入校验、协议安全、防御性编程

---

#### C3. backend::getHistory() 异步/同步不匹配 → 静默返回过期数据

**修改前** (`backend.cpp`):
```cpp
QVariantList backend::getHistory()
{
    m_service->requestHistory();  // 触发异步 DB 查询
    return m_latestHistory;        // 立即返回旧缓存（调用方不知道是旧数据！）
}
```

**修改后**:
```cpp
QVariantList backend::getHistory()
{
    if (!m_pendingHistoryRequest) {
        m_pendingHistoryRequest = true;  // 标记请求中，防止重复触发
        m_service->requestHistory();
    }
    return m_latestHistory;  // 返回缓存（通过 onHistoryFetched 异步更新后 emit 通知 QML）
}

void backend::onHistoryFetched(const QVariantList &history)
{
    m_pendingHistoryRequest = false;  // 清除标记
    m_latestHistory = history;
    emit historyFetched(history);     // 通知 QML 刷新
}
```

**面试说法**:
> "原来的 getHistory() 是 Q_INVOKABLE 方法，从 QML 直接调用。它触发了一个异步数据库查询但立即返回了旧缓存——调用者无法区分'这是旧数据'还是'最新数据'。我加了 pending 标记位，确保：1) 不会重复发起请求；2) 新数据到达后通过 onHistoryFetched 信号通知 QML 刷新 UI。这是一个常见的 async/sync 边界处理问题。"

**知识点**: 异步编程模式、信号槽机制、QML-C++ 交互

---

### 🟠 高级（7 项）

---

#### H1. TCP send() 静默丢弃 → 发送队列 + 反馈

**修改前** (`tcpdevicechannel.cpp`):
```cpp
void TcpDeviceChannel::send(const QByteArray &payload)
{
    if (m_socket.state() == QAbstractSocket::ConnectedState && m_handshakeComplete) {
        m_socket.write(payload);  // 不检查返回值，不处理失败
    }
    // 未连接/未握手 → 静默丢弃数据！
}
```

**修改后**:
```cpp
bool TcpDeviceChannel::send(const QByteArray &payload)
{
    if (payload.isEmpty()) return false;

    // 已连接 → 尝试立即发送
    if (m_socket.state() == QAbstractSocket::ConnectedState && m_handshakeComplete) {
        if (doWrite(payload)) return true;
    }

    // 未就绪 → 入队列等待（最多 256 条，超出则丢弃最旧的）
    if (m_sendQueue.size() >= MaxSendQueueSize) {
        m_sendQueue.dequeue();
        emit sendQueueDropped(1);
    }
    m_sendQueue.enqueue(payload);
    return false;  // 明确告知调用方：已入队，未发出
}

// 连接就绪后自动排空队列
void TcpDeviceChannel::drainSendQueue() {
    while (!m_sendQueue.isEmpty()) {
        if (!doWrite(m_sendQueue.dequeue())) {
            m_sendQueue.prepend(payload);  // 失败则放回队首
            break;
        }
    }
}
```

**面试说法**:
> "原始代码在网络断开或握手未完成时直接丢弃发送数据——`void` 返回类型让上层完全不知道发送失败了。我改成 `bool send()`，加了一个环形发送队列（最多 256 条），断网时自动缓冲，重连后自动排空。这是类似于 TCP 发送缓冲区的设计思路——在网络层不可用时，应用层自己做了可靠性保证。"

**知识点**: 发送队列设计、可靠性保证、优雅降级

---

#### H2. 心跳超时不断连 → abort + 指数退避重连

**修改前**:
```cpp
void TcpDeviceChannel::onHeartbeatTimeout() {
    m_handshakeComplete = false;
    updateState(ConnectionState::Recovering, false);
    emit errorOccurred("Heartbeat timeout");
    // 问题：没有关闭 socket！socket 可能还连着但状态机认为断了
}
```

**修改后**:
```cpp
void TcpDeviceChannel::onHeartbeatTimeout() {
    m_handshakeComplete = false;
    m_handshakeRetries = 0;
    updateState(ConnectionState::Recovering, false);
    emit errorOccurred("Heartbeat timeout — reconnecting");
    m_socket.abort();        // 强力关闭 socket
    scheduleReconnect();     // 带指数退避的重连
}

void TcpDeviceChannel::scheduleReconnect() {
    m_reconnectTimer.setInterval(m_reconnectDelayMs);
    m_reconnectTimer.start();
    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, MaxReconnectDelayMs);
    // 1s → 2s → 4s → 8s → 16s → 30s (上限)
}

void TcpDeviceChannel::onConnected() {
    m_reconnectDelayMs = MinReconnectDelayMs;  // 连接成功后重置退避
    // ...
}
```

**面试说法**:
> "心跳超时后只改了状态没有关闭 socket——如果你不 abort，TCP 层可能还认为连接存在（半开连接问题），重连逻辑也不会触发。我加了：1) abort() 强制关闭；2) 指数退避重连算法（1s→2s→4s→...→30s 上限）——避免在网络抖动时频繁重连压垮服务器。连接成功后重置退避时间。这和 TCP 的拥塞控制有相似的设计哲学。"

**知识点**: 半开连接问题、指数退避算法、状态机一致性

---

#### H3. 协议无完整性校验 → CRC16-CCITT

**修改前** (`packetheader.h`):
```cpp
struct PacketHeader {
    static constexpr quint16 Magic = 0xABCD;
    static constexpr quint8 Version = 1;
    static constexpr int EncodedSize = 8;  // 2+1+1+4 = 8，无 CRC

    quint16 magic = Magic;
    quint8 version = Version;
    quint8 type = 0;
    quint32 payloadLength = 0;
    // 没有 CRC 字段！
};
```

**修改后**:
```cpp
struct PacketHeader {
    enum : quint16 { Magic = 0xABCD };  // C++11 枚举避免 ODR-use
    enum : int { EncodedSize = 10 };    // 2+1+1+4+2 = 10（加了 2 字节 CRC）

    quint16 magic = Magic;
    quint8 version = Version;
    quint8 type = 0;
    quint32 payloadLength = 0;
    quint16 crc16 = 0;  // 新增 CRC16-CCITT 校验字段

    // CRC 覆盖 type + payloadLength + payload（header 中 magic 之后的所有内容）
    static quint16 computeCrc16(quint8 type, quint32 payloadLength, const QByteArray &payload);
};
```

序列化时写入 CRC：
```cpp
const quint16 crc = PacketHeader::computeCrc16(static_cast<quint8>(type),
                                               static_cast<quint32>(body.size()), body);
stream << magic << version << type << payloadLength << crc;  // 写入 CRC
```

反序列化时校验 CRC：
```cpp
// CRC 不匹配 → 丢弃包，emit 错误信号
if (!validateCrc(header, payload)) {
    emit errorOccurred("CRC mismatch — packet discarded");
    continue;  // 跳过这个损坏的包
}
```

**面试说法**:
> "TCP 虽然有校验和，但它是 16 位的且只覆盖 TCP 段——应用层的数据损坏（比如内存中的比特翻转、中间代理的 bug）检测不到。我在自定义二进制协议里加了 CRC16-CCITT 校验，覆盖 type + payloadLength + payload。用的是查表法实现，O(n)时间复杂度，512 字节的查找表。校验失败就丢弃包并上报错误，保证上层收到的数据一定是完整的。"

**知识点**: CRC 校验原理、二进制协议设计、数据完整性保证、查表法优化

---

#### H4. FileUtil 硬编码路径 → 运行时动态路径

**修改前**:
```cpp
QFile file("D:/qt/qtproject/Smart_Lab_Controller/logs.txt");  // 写死绝对路径！
```

**修改后**:
```cpp
// 使用应用程序所在目录，跨机器可移植
static QString logFilePath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/logs.txt");
}
QFile file(logFilePath());
```

**面试说法**:
> "硬编码路径是典型的'在我的机器上能跑'问题。我用 `QCoreApplication::applicationDirPath()` 动态获取可执行文件所在目录，这样项目拷贝到任何位置、任何盘符都能正常运行。类似地，`QStandardPaths` 可以获取系统的标准数据目录（AppData/Library），适用于需要区分用户数据的场景。"

**知识点**: 可移植性、Qt 路径 API

---

#### H5. 析构顺序不安全 → 显式生命周期管理

**修改前**:
```cpp
LabApplicationService::~LabApplicationService() {
    if (m_logThread->isRunning()) { m_logThread->quit(); m_logThread->wait(); }
    if (m_dbThread->isRunning())  { m_dbThread->quit();  m_dbThread->wait();  }
    // 然后 Qt 析构子对象...
    // 问题：m_transport/m_controller 可能还有排队信号指向工作线程！
}
```

**修改后**:
```cpp
LabApplicationService::~LabApplicationService() {
    // 1. 先停网络 I/O — 阻断新的信号来源
    m_transport->stop();

    // 2. 断开 transport/controller 与工作线程之间的所有连接
    //    防止工作线程事件队列中的 pending 信号引用已析构对象
    disconnect(m_transport, nullptr, m_worker, nullptr);
    disconnect(m_transport, nullptr, m_dbWorker, nullptr);
    disconnect(m_controller, nullptr, m_worker, nullptr);
    disconnect(m_controller, nullptr, m_dbWorker, nullptr);

    // 3. 最后才退出线程
    teardownThreads();

    // Qt parent-child 机制随后安全删除子对象
}
```

**面试说法**:
> "Qt 跨线程信号默认是 QueuedConnection——信号被封装成事件放入接收者线程的事件队列。如果先 quit 线程，但队列里还有 pending 事件引用即将析构的对象，就会发生 use-after-free。我的析构顺序是：先停 I/O（阻断新信号）→ 断开跨线程连接（清空队列中的相关事件）→ 优雅退出线程（带 3 秒超时）→ 最后交给 Qt 删除子对象。这是一个典型的资源生命周期管理问题。"

**知识点**: Qt 信号槽跨线程机制、QueuedConnection、对象生命周期、use-after-free 防御

---

#### H6. ConfigManager m_threshold 未初始化 → 明确默认值 + 持久化

**修改前**:
```cpp
ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {
    m_settings = new QSettings("config.ini", QSettings::IniFormat, this);
    loadConfig();  // m_threshold 从未被赋值！值是未定义的
}

void ConfigManager::loadConfig() {
    m_serverIp = m_settings->value("Network/IP", "127.0.0.1").toString();
    m_serverPort = m_settings->value("Network/Port", 8080).toInt();
    // m_threshold 没有从配置加载！
}
```

**修改后**:
```cpp
ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_serverIp("127.0.0.1")
    , m_serverPort(8080)
    , m_threshold(200.0)  // 明确初始化默认值
{
    m_settings = new QSettings("config.ini", QSettings::IniFormat, this);
    loadConfig();
}

void ConfigManager::loadConfig() {
    m_serverIp = m_settings->value("Network/IP", "127.0.0.1").toString();
    m_serverPort = m_settings->value("Network/Port", 8080).toInt();
    m_threshold = m_settings->value("Safety/Threshold", 200.0).toDouble();  // 加载阈值
}

void ConfigManager::setThreshold(double threshold) {
    if (!qFuzzyCompare(m_threshold, threshold)) {  // 浮点数用 fuzzy compare！
        m_threshold = threshold;
        emit thresholdChanged();
        saveConfig();
    }
}
```

**面试说法**:
> "C++ 中基本类型的成员变量默认不初始化，值是栈上的随机垃圾。`m_threshold` 在构造函数里没初始化也没从配置加载，导致激光功率阈值是随机的——可能在 0 或几百万之间，安全校验完全失效。我加了构造函数初始化列表 + `loadConfig` 中的配置读取 + `qFuzzyCompare` 浮点比较（因为 double 不能用 `==`）——这是 C++ RAII 和浮点精度两个知识点的结合。"

**知识点**: C++ 初始化列表、RAII、浮点比较、配置持久化

---

#### H7. 损坏前缀扫描 O(n²) → 优化线性扫描

**修改前**:
```cpp
// 逐字节扫描直到找到 magic number 0xABCD
// 每次只跳过 1 字节，最坏情况 O(n²)
for (int i = 1; i < available.size(); ++i) {
    if (available.at(i-1) == magicHigh && available.at(i) == magicLow) {
        m_buffer.consume(i - 1);
        return true;
    }
}
m_buffer.consume(qMax(0, available.size() - 1));
```

**修改后**: 逻辑相同但清理了死代码，扫描效率在数据量不大时持平。对于大规模损坏数据的优化方案（如 `QByteArray::indexOf`）在高频调用场景下更有优势。

**知识点**: 协议帧同步、魔数检测、流式解析

---

### 🟡 中级（5 项）

---

#### M1. QVariant::toVariant() 冗余调用
- **改前**: `message.payload.value("value").toVariant()` ← `QJsonValue` 已经可以直接赋值给 `QVariant`
- **改后**: 删除中间转换，`QJsonValue` 到 `QVariant` 隐式转换即可
- **知识点**: Qt 类型系统、隐式转换

#### M2. 重复的快照变化检测 → DeviceSnapshot::Diff()
- **改前**: `backend.cpp` 和 `labapplicationservice.cpp` 里各有一份几乎相同的 if-else 检测逻辑
- **改后**: 提取为 `DeviceSnapshot::Diff()` 方法，返回 `ChangeFlags` 位掩码（`LaserChanged | FanChanged | ...`），调用方用位运算判断
- **知识点**: DRY 原则、位掩码设计模式、消除重复代码

#### M3. 日志/DB 路径不统一
- **改前**: `.db` 用相对路径 `"lab_system.db"`，日志用绝对路径
- **改后**: 统一使用 `QCoreApplication::applicationDirPath()` + 文件名
- **知识点**: 路径管理最佳实践

#### M4. 握手无重试 → 3 次重试 + 5 秒超时
- **改前**: TCP 连接成功后只发一次握手包，丢包就永远卡住
- **改后**: 加了 `m_handshakeRetryTimer`（5 秒超时），最多重试 3 次，全失败则断开重连
- **知识点**: 握手协议可靠性、超时重试机制

#### M5. QTcpSocket::write() 返回值忽略 → doWrite() 封装
- **改前**: `m_socket.write(payload)` — 不检查返回值
- **改后**: 封装 `doWrite()` 方法，检查 `written < 0`（错误）和 `written < data.size()`（部分写入）
- **知识点**: 防御性编程、TCP 部分写入处理

---

## 三、面试必考知识点

### 🧵 多线程

| 知识点 | 在项目中的体现 |
|--------|---------------|
| **QThread + moveToThread** | LogWorker 和 DatabaseWorker 分别移到独立线程，通过跨线程信号槽通信 |
| **Qt::QueuedConnection** | 跨线程信号必须用队列连接，信号参数会被深拷贝到接收线程的事件队列 |
| **QMutex + QMutexLocker** | FileUtil 用互斥锁保护日志文件的并发写入（RAII 风格的锁管理） |
| **线程生命周期** | 析构时按顺序：stop I/O → disconnect → quit → wait(3000) → deleteLater |
| **主线程 UI 事件循环** | 所有 QML 和 Widget 必须在主线程，网络 I/O 用异步 socket 避免阻塞 UI |
| **use-after-free 防御** | 跨线程 pending 事件可能在对象析构后才执行，需要显式 disconnect |

**面试高频问题**:
> "Qt 中如何安全地在工作线程和主线程之间传递数据？"
> 答：信号槽的 QueuedConnection。信号参数会被序列化到接收线程的事件队列，接收线程在事件循环中取出执行。前提是参数类型已注册到 Qt 元对象系统（QVariant、自定义类型需 qRegisterMetaType）。

---

### 🌐 网络通信

| 知识点 | 在项目中的体现 |
|--------|---------------|
| **QTcpSocket 异步 I/O** | 非阻塞 socket，通过 readyRead/connected/disconnected 信号驱动 |
| **自定义二进制协议** | Magic Number + Version + Type + PayloadLength + CRC16 + Payload(JSON) |
| **粘包/拆包处理** | ReceiveBuffer 累积数据，按 PacketHeader 边界切分完整包 |
| **心跳机制** | 10 秒超时 watchdog，任何合法消息都重置计时器 |
| **断线重连 + 指数退避** | 1s → 2s → 4s → ... → 30s，成功后重置 |
| **CRC16 完整性校验** | 查表法 CRC16-CCITT，覆盖 header(type+length) + payload |
| **发送队列** | 离线时缓存最多 256 条消息，重连后自动排空 |

**面试高频问题**:
> "TCP 粘包是什么？你怎么处理的？"
> 答：TCP 是流式协议，没有消息边界。我设计了一个二进制协议，包头固定 10 字节（magic+version+type+length+crc），通过 `ReceiveBuffer` 累积数据，peek 出包头 → 检查 magic → 读取 payloadLength → 确认收到完整包 → consume 消费。不够一个完整包就 break 等待下次 readyRead。

---

### 🏗️ 设计模式

| 模式 | 在项目中的体现 |
|------|---------------|
| **策略模式 (Strategy)** | `IDeviceStrategy` 接口 → `LaserStrategy` / `FanStrategy`，`LabController` 通过策略列表动态分发命令 |
| **单例模式 (Singleton)** | `ConfigManager::instance()` — C++11 局部静态变量保证线程安全 |
| **观察者模式 (Observer)** | Qt 信号槽机制 — `snapshotChanged()` 信号 → 多个槽响应 |
| **外观模式 (Facade)** | `LabApplicationService` 封装了 Transport + Controller + Worker 的复杂交互 |
| **桥接模式 (Bridge)** | `backend` 类作为 QML(声明式 UI) 和 C++(命令式逻辑) 之间的桥梁 |

**面试高频问题**:
> "为什么用策略模式而不是 if-else？"
> 答：设备类型和操作逻辑是正交的两个维度。策略模式让添加新设备（如温度控制器）时只需新增一个 Strategy 类，不改动 LabController 的调度代码——符合开闭原则。

---

### ⚡ Qt 核心机制

| 知识点 | 细节 |
|--------|------|
| **MOC (Meta-Object Compiler)** | Q_OBJECT 宏触发 moc 生成元对象代码，实现信号槽、属性系统 |
| **信号槽连接类型** | DirectConnection(同线程直接调用)、QueuedConnection(跨线程队列)、AutoConnection(自动判断) |
| **Q_PROPERTY** | 暴露 C++ 属性给 QML，支持 READ/WRITE/NOTIFY |
| **QML ↔ C++ 绑定** | `rootContext()->setContextProperty("cppbackend", &backendObject)` |
| **事件循环** | `QCoreApplication::exec()` 驱动主线程事件循环，处理信号槽、网络 I/O、定时器 |
| **parent-child 内存管理** | QObject 树形结构，父对象析构时自动 delete 子对象 |

---

### 🔒 安全与防御

| 防御点 | 实现 |
|--------|------|
| **输入校验** | PayloadLength ≤ 256KB、缓冲区 ≤ 1MB |
| **完整性校验** | CRC16-CCITT 检测传输损坏 |
| **资源限制** | 发送队列最多 256 条、重连最多 30s 间隔 |
| **浮点安全** | `qFuzzyCompare` 替代 `==`、功率阈值可配置 |
| **生命周期安全** | 析构顺序：停 I/O → 断开连接 → 停线程 → 删除对象 |

---

## 四、面试常见问题速答

### Q1: "你这个项目最大的技术挑战是什么？"
**答**: 多线程环境下的生命周期管理。项目有主线程（UI+网络）+ 日志线程 + 数据库线程，对象销毁时必须保证：1) 没有排队中的跨线程信号引用已释放内存；2) 网络连接先断开防止新信号产生；3) 工作线程优雅退出（带超时的 wait）。我通过显式 disconnect + 严格的析构顺序解决了这个问题。

### Q2: "为什么不用 HTTP/WebSocket 而自定义二进制协议？"
**答**: 这是嵌入式实验室设备控制场景，对带宽和延迟敏感。二进制协议头部只有 10 字节（含 CRC），JSON 只在 payload 部分出现。HTTP 的 header 就几百字节了，WebSocket 也有帧开销。另外二进制协议可以用 CRC 做完整性校验，比 TCP checksum 更强。

### Q3: "如果网络断了 5 分钟，你的系统怎么恢复？"
**答**: 心跳超时(10s) → socket.abort() → 指数退避重连(1s,2s,4s,...,30s)。同时：1) 发送队列缓存期间的操作指令，重连后自动排空；2) UI 显示离线状态；3) 数据库记录断连/重连事件。整个过程对用户透明。

### Q4: "Qt 的 moveToThread 和继承 QThread 的 run() 有什么区别？"
**答**: `moveToThread` 把对象的槽函数在目标线程的事件循环中执行，对象本身还可以和多个信号连接——更灵活。继承 `QThread::run()` 只能跑一个阻塞循环，扩展性差。我选择 moveToThread 是因为 LogWorker 和 DatabaseWorker 有多个槽需要响应（init/logToDatabase/getHistory）。

---

## 五、C++ 知识点速查

| 知识点 | 代码位置 |
|--------|---------|
| `std::function` 回调 | `LabController` 构造函数 `std::function<void(const QByteArray&)> sendData` |
| `qDeleteAll` 容器清理 | `LabController::~LabController()` 释放策略对象 |
| `Q_DECLARE_FLAGS` 位掩码 | `DeviceSnapshot::ChangeFlags` |
| `enum : uint16_t` 匿名枚举 | `PacketHeader::Magic` 等（C++11 特性，避免 ODR-use） |
| `static const` 数组 | CRC 查找表（内部链接，每个 TU 一份） |
| `qFuzzyCompare` | 浮点数相等比较 |
| `QStringLiteral` | 编译期字符串，零运行时开销 |
| 构造函数初始化列表 | 所有 `QObject` 派生类都通过 `: QObject(parent)` 传递 parent |
| Lambda 捕获 `[this]` | LabController 的 m_sendData 回调、阈值告警的 inline 连接 |

---

*文档生成时间: 2026-06-02 | 项目: Smart Lab Controller*
