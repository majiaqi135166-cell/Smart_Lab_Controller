# Smart Lab Controller — 智能实验室自动化控制器

基于 Qt5/C++14 的实验室设备远程控制系统。支持通过 TCP 连接远端设备，实时监控激光功率、光源亮度、排风系统状态，具备弱网自愈、安全阈值告警、操作日志与历史数据管理能力。

---

## 目录

- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [快速开始](#快速开始)
- [项目架构](#项目架构)
- [三层线程隔离架构](#三层线程隔离架构)
- [指数退避重连流程](#指数退避重连流程)
- [二进制协议](#二进制协议)
- [核心设计模式](#核心设计模式)
- [目录结构](#目录结构)
- [测试工具](#测试工具)

---

## 功能特性

| 模块 | 功能 |
|---|---|
| 设备控制 | 激光功率（0-200W）、光源亮度（0-100%）、排风开关、紧急停止 |
| 状态监控 | 实时数值曲线（100ms 采样）、在线/离线指示灯、设备状态面板 |
| 通信协议 | 自定义二进制协议，含魔数帧同步、CRC16 校验、握手、心跳 |
| 弱网自愈 | 心跳超时检测、指数退避重连（1s→30s）、离线消息队列（256条上限） |
| 安全保护 | 功率阈值告警、高亮度下风扇未开告警、紧急停止一键复位 |
| 数据管理 | SQLite 操作日志、CSV 导出、QML 弹窗告警与 Toast 通知 |
| 配置管理 | QSettings 持久化服务器 IP/端口/阈值，GUI 内直接修改 |

---

## 技术栈

| 层级 | 技术 |
|---|---|
| UI | Qt Quick (QML) + Qt Charts |
| C++ 标准 | C++14 |
| 构建 | CMake 3.5+ |
| 网络 | QTcpSocket（自定义二进制协议） |
| 数据库 | SQLite（Qt SQL 模块） |
| 配置持久化 | QSettings（INI 格式） |
| 测试服务器 | Python 3（`test_server.py` / `chaos_test_server.py`） |

---

## 快速开始

### 环境要求

- Qt 5.12+（Core / Quick / Network / Sql / Charts）
- CMake 3.5+
- 支持 C++14 的编译器（MinGW 7.3+ / MSVC 2017+ / GCC 5+）
- Python 3（仅用于测试服务器）

### 编译

```bash
cd Smart_Lab_Controller
mkdir build && cd build
cmake ..
make -j$(nproc)      # Linux/macOS
# 或
mingw32-make -j4     # Windows MinGW
```

### 运行

```bash
# 启动测试服务器（终端1）
python test_server.py

# 启动客户端（终端2）
./Smart_Lab_Controller
```

---

## 项目架构

采用 **6 层分层架构**，每层只依赖下一层：

```mermaid
graph TB
    subgraph UI["🎨 UI 层"]
        QML["main.qml<br/>SensorCard.qml"]
    end

    subgraph Bridge["🔌 UI 桥接层"]
        Backend["backend<br/>Q_PROPERTY / Q_INVOKABLE"]
        Config["ConfigManager<br/>单例·QSettings"]
    end

    subgraph App["⚙️ 应用编排层"]
        Service["LabApplicationService<br/>线程调度·信号编排"]
    end

    subgraph Business["🧠 业务层"]
        Controller["LabController<br/>设备快照·命令分发·安全检查"]
        Domain["Domain Models<br/>DeviceSnapshot·ControlCommand"]
        Strategy["Device Strategies<br/>LaserStrategy·FanStrategy"]
    end

    subgraph Transport["📡 通信层"]
        Channel["TcpDeviceChannel<br/>TCP连接·心跳·握手·指数退避"]
        Serializer["ProtocolSerializer<br/>命令→字节流"]
        Deserializer["ProtocolDeserializer<br/>字节流→消息·CRC16"]
        Buffer["ReceiveBuffer<br/>有界缓冲区·半包拼合"]
    end

    subgraph Device["🖥️ 远端设备/服务器"]
        Server["TCP Server<br/>协议通信·命令执行"]
    end

    QML --> Backend
    QML --> Config
    Backend --> Service
    Service --> Controller
    Service --> Channel
    Controller --> Domain
    Controller --> Strategy
    Channel --> Serializer
    Channel --> Deserializer
    Deserializer --> Buffer
    Channel <==>|TCP| Server
```

### 数据流方向

```text
【用户操作】QML 滑块拖动
  → backend::setLaserPower()            Q_PROPERTY WRITE
  → LabApplicationService               去重 + 编排
  → LabController::applyCommand()       策略分发
  → LaserStrategy::apply()              更新 DeviceSnapshot
  → ProtocolSerializer                   序列化为二进制包
  → TcpDeviceChannel::send()            TCP 发送
  → 远端设备

【远程指令】远端设备发来 Command
  → QTcpSocket::readyRead               触发信号
  → ProtocolDeserializer                字节→ProtocolMessage
  → LabController::processMessage()     解析命令
  → applyCommand(remote=true)           安全检查 + 阈值告警
  → emit snapshotChanged                向上冒泡
  → LabApplicationService               写数据库 + 转发
  → backend                             属性变更通知
  → QML 属性绑定自动刷新 UI
```

---

## 三层线程隔离架构

主线程不直接做任何 I/O 操作——所有阻塞工作分散到 3 个专用线程：

```mermaid
graph TB
    subgraph MainThread["🟢 主线程 (GUI)"]
        QML_UI["QML 界面渲染"]
        Backend_Obj["backend"]
        Service_Obj["LabApplicationService"]
        Controller_Obj["LabController"]
    end

    subgraph IOThread["🔵 I/O 线程 (m_IOThread)"]
        Transport["TcpDeviceChannel"]
        Socket["QTcpSocket"]
        HB["心跳定时器<br/>10s超时"]
        Reconnect["重连定时器<br/>指数退避"]
    end

    subgraph DBThread["🟡 数据库线程 (m_dbThread)"]
        DBWorker["DatabaseWorker"]
        SQLite["SQLite 读写"]
        CSV["CSV 文件导出"]
    end

    subgraph LogThread["🟠 日志线程 (m_logThread)"]
        LogWorker["LogWorker"]
        FileIO["FileUtil<br/>QMutex 保护"]
    end

    MainThread -->|"QueuedConnection<br/>dbLogRequested"| DBThread
    MainThread -->|"QueuedConnection<br/>saveLogRequested"| LogThread
    MainThread -->|"BlockingQueued<br/>invokeMethod(exportToCSV)"| DBThread
    MainThread -->|"QueuedConnection<br/>invokeMethod(start)"| IOThread
    IOThread -->|"QueuedConnection<br/>frameReceived"| MainThread
    IOThread -->|"QueuedConnection<br/>statusChanged"| MainThread
    DBThread -->|"QueuedConnection<br/>historyReady"| MainThread
```

### 线程隔离规则

| 规则 | 说明 |
|---|---|
| 主线程禁止 I/O | 日志写入、SQLite 操作、TCP 收发均在子线程 |
| 跨线程只用信号槽 | 主线程 emit 信号 → Qt 自动将 slot 投递到目标线程事件循环 |
| BlockingQueuedConnection | CSV 导出需要同步拿返回值，用此方式在 DB 线程执行后回传结果 |
| Worker moveToThread | LogWorker / DatabaseWorker 不继承 QThread，而是 moveToThread 到已有线程 |

---

## 指数退避重连流程

```mermaid
flowchart TD
    Normal["✅ 正常通信中"] -->|"10s 无数据"| HB_Timeout["⏰ 心跳超时"]
    Normal -->|"TCP 断开"| Disconnected["🔌 onDisconnected"]
    HB_Timeout --> Abort["m_socket.abort()"]
    Disconnected --> Abort

    Abort --> Schedule["scheduleReconnect()"]
    Schedule --> SetTimer["QTimer::setInterval(delay)<br/>delay = 1s → 2s → 4s → … → 30s"]
    SetTimer --> Delay["delay *= 2 (翻倍)"]
    Delay --> StartTimer["QTimer::start()"]

    StartTimer -->|"定时器触发"| TryConnect["onReconnectTimer()<br/>connectToHost()"]

    TryConnect -->|"连接成功"| OnConnected["onConnected()"]
    OnConnected --> ResetDelay["m_reconnectDelayMs = 1000<br/>★ 重置为最小值"]
    ResetDelay --> Handshake["sendHandshake()<br/>发送握手包"]
    Handshake -->|"5s内收到回复"| Ready["✅ Ready<br/>drainSendQueue()"]
    Handshake -->|"超时(最多3次)"| Abort

    TryConnect -->|"连接失败<br/>QTcpSocket::disconnected"| Schedule
```

### 为什么是指数退避？

| 固定间隔重连（❌） | 指数退避重连（✅） |
|---|---|
| 所有设备同时重连 → 网络风暴 | 各设备等待时间逐渐散开 |
| CPU 空转 busy-wait | QTimer sleep，CPU 休眠 |
| 服务器永远恢复不了 | 给服务器喘息时间 |

**退避区间**：`1000ms → 2000ms → 4000ms → 8000ms → 16000ms → 30000ms(上限)`

**离线消息队列**：离线期间控制命令进入 `QQueue<QByteArray>`（上限 256 条，满了丢弃最旧保留最新），恢复连接后 `drainSendQueue()` 批量发出。

---

## 二进制协议

### 包格式（大端序）

```text
┌──────────┬─────────┬──────────┬──────────────┬────────┬──────────────┐
│  Magic   │ Version │  Type    │ Payload Len  │ CRC16  │   Payload    │
│  2 bytes │ 1 byte  │ 1 byte   │   4 bytes    │ 2 bytes│ 0~256 KB     │
│  0xABCD  │  0x01   │          │              │        │   (JSON)     │
└──────────┴─────────┴──────────┴──────────────┴────────┴──────────────┘
│←────────────────── 包头 10 bytes ──────────────────→│←── 载荷 ──→│
```

### 消息类型

| Type | 值 | 方向 | 说明 |
|---|---|---|---|
| Heartbeat | 1 | 双向 | 心跳保活，载荷含 `{"type":"heartbeat"}` |
| Command | 2 | 双向 | 控制指令，载荷含 `{"target":"laser","action":"set_power","value":150.5}` |
| Snapshot | 3 | 双向 | 设备状态快照，载荷含全部字段值 |
| Handshake | 4 | 双向 | 应用层握手，"你是不是合法设备？" |

### CRC16-CCITT 校验

- 查表法计算（256 项预计算表）
- 覆盖范围：`[type] [payloadLength(4B BE)] [payload]`
- 不覆盖 magic 和 version（这两项在 tryReadHeader 时已校验）

### 反序列化流程

```text
socket 收到字节
  → deserializer.append(bytes)         存入 ReceiveBuffer（上限 1MB）
  → takeMessages()                     循环提取
    ├─ tryReadHeader()                  peek 10字节，验 0xABCD + version=1
    ├─ canPeek(完整包)                  不够 → break（等下次 readyRead 拼包）
    ├─ peek(载荷)                       只读不取
    ├─ consume(完整包)                  数据有效，才真正移走
    ├─ validateCrc()                    CRC 不通过 → 丢弃该包
    └─ parseMessage()                   JSON → ProtocolMessage
```

---

## 核心设计模式

| 模式 | 应用位置 | 说明 |
|---|---|---|
| **分层架构** | 整个 `src/` 目录结构 | UI→桥接→编排→业务→通信，每层单向依赖 |
| **策略模式** | `IDeviceStrategy` / `LaserStrategy` / `FanStrategy` | 各设备独立处理命令，新增设备只需新增 strategy |
| **门面模式** | `backend` | 对 QML 隐藏内部调用链，暴露 4 个属性 + 7 个方法 |
| **命令模式** | `ControlCommand` | 本地操作和远程指令统一为命令对象 |
| **单例模式** | `ConfigManager` | C++11 Magic Statics 线程安全懒汉单例 |
| **依赖注入** | `LabController(std::function)` | Controller 不依赖 Transport，通过回调注入发送行为 |
| **Worker Thread** | `LogWorker` / `DatabaseWorker` | moveToThread，不继承 QThread |

---

## 目录结构

```text
Smart_Lab_Controller/
├── main.cpp                          # 程序入口，QML 上下文注入
├── CMakeLists.txt                    # CMake 构建配置 (Qt5, C++14)
│
├── main.qml                          # 主控制面板 UI
├── SensorCard.qml                    # 传感器卡片组件
├── qml.qrc                           # Qt 资源注册
├── fan.png                           # 风扇图标
│
├── backend.h / backend.cpp           # QML ↔ C++ 桥接层 (Q_PROPERTY / Q_INVOKABLE)
├── ConfigManager.h / .cpp            # 配置单例 (QSettings 持久化)
│
├── src/
│   ├── controller/
│   │   ├── labapplicationservice.h/cpp   # 应用编排中心（线程调度·信号连接）
│   │   └── labcontroller.h/cpp           # 设备控制核心（命令分发·安全检查）
│   │
│   ├── domain/
│   │   ├── models/
│   │   │   ├── devicesnapshot.h          # 设备状态快照 + ChangeFlags 变更检测
│   │   │   ├── controlcommand.h          # 控制命令 DTO
│   │   │   ├── telemetryframe.h          # 遥测帧 DTO
│   │   │   └── connectionstate.h         # 连接状态枚举
│   │   └── devices/
│   │       ├── idevicestrategy.h         # 设备策略接口
│   │       ├── laserstrategy.h           # 激光控制策略
│   │       └── fanstrategy.h             # 风扇控制策略
│   │
│   ├── infrastructure/transport/
│   │   ├── itransportchannel.h           # 传输通道接口
│   │   ├── transporttypes.h              # 传输层数据类型
│   │   └── tcpdevicechannel.h/cpp        # TCP 实现（心跳·握手·指数退避·发送队列）
│   │
│   └── protocol/
│       ├── protocolmessage.h             # 协议消息结构
│       ├── packetheader.h                # 包头定义 + CRC16 查表
│       ├── protocolserializer.h/cpp      # 序列化器（命令→二进制包）
│       ├── protocoldeserializer.h/cpp    # 反序列化器（二进制流→消息）
│       └── receivebuffer.h/cpp           # 有界接收缓冲区（1MB·惰性压缩·半包拼合）
│
├── logworker.h / logworker.cpp        # 异步日志 Worker
├── databaseworker.h / .cpp            # 异步数据库 Worker（SQLite + CSV导出）
├── FileUtil.h / FileUtil.cpp          # 文件工具（QMutex 保护的日志写入）
│
├── basedevice.h / basedevice.cpp      # [旧] 设备基类
├── FanDevice.h                        # [旧] 风扇设备类
├── LaserDevice.h                      # [旧] 激光设备类
│
├── test_server.py                     # 协议测试服务器
└── chaos_test_server.py               # 混沌故障模拟服务器（4种故障场景）
```

---

## 测试工具

### 标准测试服务器

```bash
python test_server.py [host] [port]
```

发送预设的激光功率和风扇控制指令，验证协议通信。

### 混沌故障模拟服务器

```bash
# 场景1: 随机断开
python chaos_test_server.py 127.0.0.1 8080 1

# 场景2: 假死模式（TCP保持但沉默）
python chaos_test_server.py 127.0.0.1 8080 2

# 场景3: 连接拒绝（模拟设备过载）
python chaos_test_server.py 127.0.0.1 8080 3

# 场景4: 间歇性抽风（混合所有故障）
python chaos_test_server.py 127.0.0.1 8080 4

# 不指定场景：自动随机切换
python chaos_test_server.py
```

观察客户端应对各种故障的行为：心跳超时检测、指数退避重连、离线队列缓存、恢复后批量排空。

---

## 许可证

MIT License

---

## 作者

大三暑期实训项目，用于学习 Qt/C++ 分层架构、多线程编程、工业通信协议设计与弱网自愈机制。
