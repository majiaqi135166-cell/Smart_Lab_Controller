"""
混沌测试服务器 —— 模拟工业现场四种典型故障：
  1. 随机断开  2. 假死（不发数据，不断开）
  3. 连接拒绝  4. 间歇性恢复

用法：
  python chaos_test_server.py [host] [port] [场景编号]
  场景 1: 随机断开模式
  场景 2: 假死模式（TCP连着但不发数据）
  场景 3: 连接拒绝模式（模拟设备忙/过载）
  场景 4: 间歇性抽风（混合所有故障）
  默认: 随机切换场景
"""

import socket
import struct
import json
import time
import threading
import random
import sys

MAGIC = 0xABCD
VERSION = 1

# CRC16 table (same as client)
CRC_TABLE = [
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
]

def compute_crc16(msg_type, payload):
    crc = 0xFFFF
    payload_len = len(payload)
    def update(byte):
        nonlocal crc
        crc = ((crc << 8) ^ CRC_TABLE[((crc >> 8) ^ byte) & 0xFF]) & 0xFFFF
    update(msg_type & 0xFF)
    update((payload_len >> 24) & 0xFF)
    update((payload_len >> 16) & 0xFF)
    update((payload_len >> 8) & 0xFF)
    update(payload_len & 0xFF)
    for b in payload:
        update(b)
    return crc

def make_packet(msg_type, payload_dict=None):
    if payload_dict is None:
        payload_dict = {}
    payload = json.dumps(payload_dict).encode('utf-8')
    if len(payload) > 262144:
        raise ValueError(f"Payload too large: {len(payload)} bytes")
    crc16 = compute_crc16(msg_type, payload)
    header = struct.pack('>HBBIH', MAGIC, VERSION, msg_type, len(payload), crc16)
    return header + payload

def log(msg):
    """带时间戳的日志"""
    t = time.strftime("%H:%M:%S")
    print(f"[{t}] {msg}")


# ═══════════════════════════════════════════════════════════════
# 故障模拟服务器
# ═══════════════════════════════════════════════════════════════
class ChaosServer:
    def __init__(self, host='127.0.0.1', port=8080, scenario=0):
        self.host = host
        self.port = port
        self.scenario = scenario
        self.reject_count = 0          # 拒绝连接计数
        self.should_die_silently = False  # 假死标记
        self.server_running = True

    def start(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((self.host, self.port))
        server.listen(5)
        server.settimeout(1.0)

        log(f"混沌测试服务器启动 {self.host}:{self.port}")
        if self.scenario == 0:
            log("模式: 自动随机切换故障")
        elif self.scenario == 1:
            log("模式: ★ 随机断开 —— 不定时主动断开连接")
        elif self.scenario == 2:
            log("模式: ★ 假死模式 —— TCP连着但不再发数据")
        elif self.scenario == 3:
            log("模式: ★ 连接拒绝 —— 模拟设备过载/忙")
        elif self.scenario == 4:
            log("模式: ★ 间歇性抽风 —— 混合所有故障")
        log("观察你的客户端如何应对这些故障...\n")

        while self.server_running:
            try:
                client, addr = server.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            log(f"═══ 新连接: {addr} ═══")
            t = threading.Thread(target=self._handle_client, args=(client, addr))
            t.daemon = True
            t.start()

    def _pick_scenario(self):
        """选择当前故障场景"""
        if self.scenario != 0:
            return self.scenario
        return random.choices(
            [1, 2, 3, 4],
            weights=[3, 2, 2, 3]  # 权重：断开/假死/拒绝/抽风
        )[0]

    def _handle_client(self, client, addr):
        scenario = self._pick_scenario()
        log(f"  → 本轮故障场景: {scenario}")

        try:
            # 阶段1: 接收握手
            client.settimeout(10)
            try:
                data = client.recv(4096)
                if data:
                    log("  ← 收到握手请求，回复握手确认")
                    client.send(make_packet(4, {"role": "server", "status": "ok"}))
            except socket.timeout:
                log("  ⚠ 握手超时，关闭连接")
                client.close()
                return

            # ═══════════════════════════════════════════════════════
            # 场景3: 连接拒绝 —— 握手后立刻断开，模拟"刚接上就被踢"
            # ═══════════════════════════════════════════════════════
            if scenario == 3:
                self.reject_count += 1
                if self.reject_count <= 5:
                    log(f"  💀 连接拒绝 #{self.reject_count}: 踢掉客户端（模拟设备过载）")
                    time.sleep(0.5)
                    client.close()
                    return

            # 正常阶段: 发几次心跳和命令
            client.settimeout(1.0)

            for cycle in range(8):
                # 发一个心跳
                try:
                    hb = make_packet(1, {"type": "heartbeat"})
                    client.send(hb)
                    log(f"  [心跳 #{cycle+1}] 已发送")
                except (BrokenPipeError, ConnectionResetError):
                    log("  ⚡ 发送失败，客户端已断开")
                    return

                # 偶尔发一条控制命令
                if random.random() < 0.3:
                    cmd = random.choice([
                        ("laser", "set_power", random.randint(10, 180)),
                        ("fan", "switch", random.choice([True, False])),
                    ])
                    try:
                        client.send(make_packet(2, {
                            "target": cmd[0],
                            "action": cmd[1],
                            "value" if isinstance(cmd[2], int) else "status": cmd[2]
                        }))
                        log(f"  [命令] {cmd[0]}/{cmd[1]}={cmd[2]}")
                    except (BrokenPipeError, ConnectionResetError):
                        return

                # ═══════════════════════════════════════════════════
                # 场景1: 随机断开 —— 模拟网线被拔/设备断电
                # ═══════════════════════════════════════════════════
                if scenario == 1 and random.random() < 0.35:
                    log("  💀 模拟故障: 随机断开连接 (相当于拔网线/断电)")
                    client.close()
                    return

                # ═══════════════════════════════════════════════════
                # 场景4: 间歇性抽风 —— 随机断开或假死
                # ═══════════════════════════════════════════════════
                if scenario == 4:
                    r = random.random()
                    if r < 0.30:
                        log("  💀 间歇性故障: 断开连接")
                        client.close()
                        return
                    elif r < 0.45:
                        log("  🧟 间歇性假死: 进程卡住 6 秒...")
                        time.sleep(6)  # 故意不干活，但不断开

                time.sleep(random.uniform(1.5, 3.0))

            # ═══════════════════════════════════════════════════════
            # 场景2: 假死模式 —— TCP连接活着，但不发任何数据
            # ═══════════════════════════════════════════════════════
            if scenario == 2 or (scenario == 4 and random.random() < 0.5):
                log("  🧟 进入假死模式: TCP连接保持，但服务器不再发送任何数据")
                log("     客户端的心跳超时检测应该生效 (10秒) ...")
                time.sleep(15)  # 超过心跳超时(10s)
                log("  🧟 假死中...又过去15秒...")
                time.sleep(15)
                # 假死后可能恢复，也可能断开
                if random.random() < 0.5:
                    log("  🧟 假死结束: 断开连接")
                    try:
                        client.close()
                    except:
                        pass
                    return
                else:
                    log("  ✅ 假死结束: 恢复通信，发送追赶心跳")
                    try:
                        client.send(make_packet(1, {"type": "heartbeat"}))
                    except:
                        return

            # 正常结束
            log("  ✅ 本轮测试完成，正常关闭连接")
            try:
                client.send(make_packet(1, {"type": "heartbeat"}))
                time.sleep(0.5)
                client.close()
            except:
                pass

        except (ConnectionResetError, BrokenPipeError, OSError) as e:
            log(f"  ⚡ 连接异常: {e}")
        except Exception as e:
            log(f"  ⚡ 未知错误: {e}")
        finally:
            try:
                client.close()
            except:
                pass
            log(f"  ← 连接关闭\n")


# ═══════════════════════════════════════════════════════════════
# 观察者：打印客户端 reconnect 间隔
# ═══════════════════════════════════════════════════════════════
def print_observer_guide():
    print("""
╔══════════════════════════════════════════════════════════════╗
║              观察客户端行为指南                               ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  在客户端控制台观察：                                         ║
║                                                              ║
║  1. 断开后，reconnect 间隔应该是:                             ║
║     第1次重连: 断开后 ~1秒                                    ║
║     第2次重连: 如果失败，再等 ~2秒                            ║
║     第3次重连: ~4秒                                           ║
║     第4次重连: ~8秒                                           ║
║     第5次重连: ~16秒                                          ║
║     此后: ~30秒 (上限)                                        ║
║                                                              ║
║  2. 连接成功后，下次断开时，间隔应重置为 1秒                   ║
║                                                              ║
║  3. 假死模式下，客户端应在 10秒内心跳超时                      ║
║                                                              ║
║  4. sendQueue 会在离线时缓存消息，恢复后批量发送               ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
""")


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    scenario = int(sys.argv[3]) if len(sys.argv) > 3 else 0

    print_observer_guide()

    server = ChaosServer(host, port, scenario)
    try:
        server.start()
    except KeyboardInterrupt:
        log("服务器已停止")
