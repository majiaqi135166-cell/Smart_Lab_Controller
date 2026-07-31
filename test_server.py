"""
Smart Lab Controller — Test Server
Implements the binary packet protocol used by the C++ client.
Packet format (big-endian):
  [2 bytes magic 0xABCD] [1 byte version] [1 byte type] [4 bytes payload_len] [2 bytes CRC16] [payload_len bytes JSON]

Message types: 0=Unknown, 1=Heartbeat, 2=Command, 3=Snapshot, 4=Handshake

CRC16-CCITT covers [type] [payload_len (4 bytes BE)] [payload bytes].
"""
import socket
import struct
import json
import time
import threading
import sys

MAGIC = 0xABCD
VERSION = 1
ENC_SIZE = 10  # 2(magic) + 1(ver) + 1(type) + 4(len) + 2(crc)

# CRC16-CCITT lookup table
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
    """CRC16-CCITT over [type] [payload_len BE] [payload]."""
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
    """Build a binary packet for the given message type and optional JSON payload."""
    if payload_dict is None:
        payload_dict = {}
    payload = json.dumps(payload_dict).encode('utf-8')

    if len(payload) > 262144:
        raise ValueError(f"Payload too large: {len(payload)} bytes")

    crc16 = compute_crc16(msg_type, payload)
    header = struct.pack('>HBBIH', MAGIC, VERSION, msg_type, len(payload), crc16)
    return header + payload


def make_heartbeat():
    """Type 1: Heartbeat."""
    return make_packet(1, {"type": "heartbeat"})


def make_command(target, action, value=None, status=None):
    """Type 2: Command."""
    payload = {"target": target, "action": action}
    if value is not None:
        payload["value"] = value
    if status is not None:
        payload["status"] = status
    return make_packet(2, payload)


def make_snapshot(laser_power, brightness, fan_active, online):
    """Type 3: Snapshot."""
    return make_packet(3, {
        "laser_power": laser_power,
        "brightness": brightness,
        "fan_active": fan_active,
        "online": online
    })


def make_handshake(role="lab-controller"):
    """Type 4: Handshake."""
    return make_packet(4, {"role": role})


def start_server(host='127.0.0.1', port=8080):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((host, port))
    server_socket.listen(1)
    print(f"Server listening on {host}:{port}...")

    while True:
        client_socket, addr = server_socket.accept()
        print(f"Connection from {addr}")

        try:
            # Heartbeat sender thread
            def send_heartbeat():
                while True:
                    try:
                        pkt = make_heartbeat()
                        client_socket.send(pkt)
                        print(f"  [HEARTBEAT] sent ({len(pkt)} bytes)")
                        time.sleep(5)
                    except Exception:
                        break

            hb_thread = threading.Thread(target=send_heartbeat, daemon=True)
            hb_thread.start()

            # Send laser power control command
            time.sleep(1)
            pkt1 = make_command("laser", "set_power", value=150.5)
            client_socket.send(pkt1)
            print(f"  [CMD] laser/set_power=150.5 ({len(pkt1)} bytes)")

            time.sleep(2)

            # Send fan ON command
            pkt2 = make_command("fan", "switch", status=True)
            client_socket.send(pkt2)
            print(f"  [CMD] fan/switch=True ({len(pkt2)} bytes)")

            time.sleep(5)

            # Send fan OFF command
            pkt3 = make_command("fan", "switch", status=False)
            client_socket.send(pkt3)
            print(f"  [CMD] fan/switch=False ({len(pkt3)} bytes)")

            # Keep connection alive
            while True:
                time.sleep(1)

        except (ConnectionResetError, BrokenPipeError, OSError) as e:
            print(f"  Connection error: {e}")
        except Exception as e:
            print(f"  Error: {e}")
        finally:
            client_socket.close()
            print("  Client disconnected.")


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    start_server(host, port)
