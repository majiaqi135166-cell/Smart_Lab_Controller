# -*- coding: utf-8 -*-
"""生成秋招项目展示 PDF（4页：封面/截图/架构图/技术亮点）"""
import os

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader

# ── 字体注册（Windows 系统字体）──
pdfmetrics.registerFont(TTFont("Hei", "C:/Windows/Fonts/simhei.ttf"))
pdfmetrics.registerFont(TTFont("YaHei", "C:/Windows/Fonts/msyh.ttc", subfontIndex=0))
pdfmetrics.registerFont(TTFont("YaHeiBd", "C:/Windows/Fonts/msyhbd.ttc", subfontIndex=0))

# ── 配色（与技术文档风格一致）──
DARK   = HexColor("#2C3E50")   # 深蓝（标题栏同款）
BLUE   = HexColor("#409EFF")   # 主蓝
GREEN  = HexColor("#67C23A")   # 成功绿
ORANGE = HexColor("#E6A23C")   # 告警黄
RED    = HexColor("#F56C6C")   # 危险红
GRAY   = HexColor("#909399")   # 辅助灰
BG     = HexColor("#F5F7FA")   # 背景灰

PAGE_W, PAGE_H = A4  # 595 x 842 pt

def draw_header(c, title, subtitle=""):
    """统一的页眉"""
    c.setFillColor(DARK)
    c.rect(0, PAGE_H - 46, PAGE_W, 46, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("Hei", 16)
    c.drawString(24, PAGE_H - 31, title)
    if subtitle:
        c.setFont("YaHei", 9)
        c.drawString(24, PAGE_H - 44, subtitle)

def draw_footer(c, page_no):
    c.setFillColor(GRAY)
    c.setFont("YaHei", 8)
    c.drawCentredString(PAGE_W / 2, 18, f"第 {page_no} 页 / 共 4 页")

def draw_tech_chips(c, y, chips):
    """技术栈标签"""
    x = 24
    for chip in chips:
        c.setFont("YaHei", 9)
        w = c.stringWidth(chip) + 16
        c.setFillColor(BLUE)
        c.roundRect(x, y, w, 20, 10, stroke=0, fill=1)
        c.setFillColor(white)
        c.drawString(x + 8, y + 6, chip)
        x += w + 8

def page1_cover(c):
    draw_footer(c, 1)
    # 背景装饰
    c.setFillColor(DARK)
    c.rect(0, PAGE_H * 0.55, PAGE_W, PAGE_H * 0.45, stroke=0, fill=1)

    # 左上角装饰线
    c.setFillColor(BLUE)
    c.rect(0, PAGE_H - 6, PAGE_W, 6, stroke=0, fill=1)

    # 项目名
    c.setFillColor(DARK)
    c.setFont("Hei", 34)
    c.drawString(24, PAGE_H * 0.82, "智能实验室控制器")

    # 英文名
    c.setFillColor(GRAY)
    c.setFont("YaHei", 13)
    c.drawString(24, PAGE_H * 0.82 - 26, "Smart Lab Controller — 上位机监控与控制软件")

    # 一句话定位（大字）
    c.setFillColor(BLUE)
    c.setFont("Hei", 15)
    c.drawString(24, PAGE_H * 0.72, "基于 C++14 / Qt 5.12 的实验室设备上位机软件")
    c.setFillColor(DARK)
    c.setFont("YaHei", 11)
    c.drawString(24, PAGE_H * 0.72 - 20,
                 "串口 Modbus RTU 多从站采集 + TCP 远程控制 + SQLite 落库，多线程零阻塞 UI")

    # 技术栈
    draw_tech_chips(c, PAGE_H * 0.63,
                    ["C++14", "Qt 5.12", "QML", "Modbus RTU", "TCP 自定义协议",
                     "SQLite", "多线程", "Qt Charts"])

    # 下半部分：核心数据
    y0 = PAGE_H * 0.45
    c.setFillColor(white)
    c.setFont("Hei", 13)
    c.drawString(24, y0, "核心能力一览")

    stats = [
        ("3 台", "Modbus 从站并行采集\n（温湿度/风机/激光器）"),
        ("2 条", "通信链路\n（RS-485 串口 + TCP）"),
        ("4 个", "工作线程协同\n（UI/日志/数据库）"),
        ("500x", "传感器入库吞吐提升\n（事务批量写入）"),
    ]
    x = 24
    for num, desc in stats:
        c.setFillColor(HexColor("#3A506B"))
        c.roundRect(x, y0 - 88, 128, 76, 8, stroke=0, fill=1)
        c.setFillColor(BLUE)
        c.setFont("Hei", 20)
        c.drawString(x + 12, y0 - 34, num)
        c.setFillColor(white)
        c.setFont("YaHei", 8.5)
        for i, line in enumerate(desc.split("\n")):
            c.drawString(x + 12, y0 - 52 - i * 13, line)
        x += 140

    # 页脚信息
    c.setFillColor(white)
    c.setFont("YaHei", 10)
    c.drawString(24, PAGE_H * 0.12, "作者：majiaqi135166-cell")
    c.drawString(24, PAGE_H * 0.12 - 16, "代码仓库：github.com/majiaqi135166-cell/Smart_Lab_Controller")

def page2_screenshot(c):
    draw_header(c, "软件运行界面", "Qt/QML 界面设计 — 设备控制 · 实时监控 · 数据库历史 · 传感器趋势")
    draw_footer(c, 2)

    img_path = "screenshot.png"
    if os.path.exists(img_path):
        img = ImageReader(img_path)
        iw, ih = img.getSize()
        # 目标区域：页眉下方，保留页脚
        area_w = PAGE_W - 48
        area_h = PAGE_H - 46 - 60
        scale = min(area_w / iw, area_h / ih)
        dw, dh = iw * scale, ih * scale
        x = (PAGE_W - dw) / 2
        y = PAGE_H - 46 - 20 - dh
        # 阴影效果
        c.setFillColor(HexColor("#00000022"))
        c.roundRect(x + 4, y - 4, dw, dh, 10, stroke=0, fill=1)
        c.drawImage(img_path, x, y, dw, dh)
        # 边框
        c.setStrokeColor(HexColor("#DCDFE6"))
        c.setLineWidth(1)
        c.roundRect(x, y, dw, dh, 10, stroke=1, fill=0)
    else:
        c.setFont("Hei", 18)
        c.setFillColor(RED)
        c.drawCentredString(PAGE_W / 2, PAGE_H / 2, "（截图缺失，请补充 screenshot.png）")

    # 布局亮点标注
    c.setFont("YaHei", 10)
    c.setFillColor(GRAY)
    notes = [
        "· 顶部：设备在线状态指示 + 状态卡片",
        "· 中部：激光功率/风扇/亮度控制面板（Slider/Switch 双向绑定 Q_PROPERTY）",
        "· Modbus 面板：串口配置、三色状态灯（绿/黄呼吸/红）、寄存器实时值、写入控制",
        "· 传感器趋势图：DateTimeAxis 真实时间轴，最近 5 分钟温度/湿度双曲线",
        "· 底部：数据库历史（最近10条）、实时数值监控图（100ms 采样）",
    ]
    y = PAGE_H - 46 - 20 - (area_h if os.path.exists(img_path) else 0) - 16
    for note in notes:
        c.drawString(24, y, note)
        y -= 15

def page3_architecture(c):
    draw_header(c, "软件架构", "三层解耦 · 四线程协同 · 双链路通信")
    draw_footer(c, 3)

    # 画布区域
    top = PAGE_H - 46 - 30
    cx = PAGE_W / 2

    def box(x, y, w, h, title, subtitle, color, font_size=12):
        c.setFillColor(color)
        c.roundRect(x, y, w, h, 8, stroke=0, fill=1)
        c.setFillColor(white)
        c.setFont("Hei", font_size)
        c.drawCentredString(x + w / 2, y + h - 22, title)
        c.setFont("YaHei", 8.5)
        for i, line in enumerate(subtitle.split("\n")):
            c.drawCentredString(x + w / 2, y + h - 38 - i * 12, line)

    def arrow(x1, y1, x2, y2, label=""):
        c.setStrokeColor(GRAY)
        c.setLineWidth(1.2)
        c.line(x1, y1, x2, y2)
        # 箭头
        import math
        ang = math.atan2(y2 - y1, x2 - x1)
        ar = 5
        c.setFillColor(GRAY)
        p = c.beginPath()
        p.moveTo(x2, y2)
        p.lineTo(x2 - ar * math.cos(ang - 0.4), y2 - ar * math.sin(ang - 0.4))
        p.lineTo(x2 - ar * math.cos(ang + 0.4), y2 - ar * math.sin(ang + 0.4))
        p.close()
        c.drawPath(p, stroke=0, fill=1)
        if label:
            c.setFont("YaHei", 8)
            c.setFillColor(GRAY)
            c.drawCentredString((x1 + x2) / 2, (y1 + y2) / 2 + 6, label)

    # ── UI 层 ──
    box(cx - 160, top - 60, 320, 56, "UI 层（QML · 主线程）",
        "设备控制面板 / Modbus 监控 / Qt Charts 实时曲线\n告警弹窗 / 三色状态灯 / 日志列表", DARK, 13)

    # ── 桥接层 ──
    arrow(cx, top - 60, cx, top - 92, "Q_INVOKABLE / 信号槽")
    box(cx - 160, top - 148, 320, 46, "桥接层（backend）",
        "Q_PROPERTY 双向绑定 · 信号转发 · 配置持久化", BLUE)

    # ── 业务逻辑层 ──
    arrow(cx, top - 148, cx, top - 180)
    box(cx - 160, top - 236, 320, 46, "业务逻辑层（LabApplicationService）",
        "设备状态聚合 · 阈值告警/迟滞/冷却 · 硬件联动 · 线程编排", GREEN)

    # ── 双分支：左 TCP，右 Modbus ──
    arrow(cx - 80, top - 236, cx - 110, top - 280)
    arrow(cx + 80, top - 236, cx + 110, top - 280)

    box(cx - 235, top - 336, 170, 46, "LabController", "Fan/Laser 策略\nControlCommand 统一指令", BLUE, 11)
    box(cx + 65, top - 336, 170, 46, "ModbusManager", "多从站轮询调度\n优先级任务队列+状态机", ORANGE, 11)

    arrow(cx - 150, top - 336, cx - 150, top - 380)
    arrow(cx + 150, top - 336, cx + 150, top - 380)

    box(cx - 235, top - 436, 170, 46, "TcpDeviceChannel", "自定义二进制协议\n心跳看门狗/粘包处理", BLUE, 11)
    box(cx + 65, top - 436, 170, 46, "QModbusRtuSerialMaster", "Modbus RTU\nRS-485 总线 3 从站", ORANGE, 11)

    arrow(cx - 150, top - 436, cx - 150, top - 470)
    arrow(cx + 150, top - 436, cx + 150, top - 470)

    c.setFont("YaHei", 10)
    c.setFillColor(GRAY)
    c.drawCentredString(cx - 150, top - 485, "TCP 远程服务器")
    c.drawCentredString(cx + 150, top - 485, "RS-485 从站 ×3")

    # ── 线程模型（右侧栏）──
    tx = PAGE_W - 195
    c.setFillColor(BG)
    c.roundRect(tx - 12, 60, 195, PAGE_H - 46 - 140, 8, stroke=1, fill=1)
    c.setStrokeColor(HexColor("#DCDFE6"))
    c.setFillColor(DARK)
    c.setFont("Hei", 11)
    c.drawString(tx, 60 + (PAGE_H - 46 - 140) - 24, "线程模型（4 线程）")

    threads = [
        ("主线程", "UI 渲染 + Modbus 轮询 + TCP\n事件循环零阻塞", BLUE),
        ("LogWorker 线程", "文件日志异步落盘\nQueuedConnection 投递", GREEN),
        ("DatabaseWorker 线程", "SQLite 事务批量写入\n100条/5秒 触发", ORANGE),
        ("（重连）", "指数退避 1s→30s\n单发定时器", RED),
    ]
    y = 60 + (PAGE_H - 46 - 140) - 48
    for name, desc, color in threads:
        c.setFillColor(color)
        c.roundRect(tx, y - 30, 170, 34, 6, stroke=0, fill=1)
        c.setFillColor(white)
        c.setFont("Hei", 9.5)
        c.drawString(tx + 10, y - 10, name)
        c.setFont("YaHei", 7.5)
        for i, line in enumerate(desc.split("\n")):
            c.drawString(tx + 10, y - 22 - i * 10, line)
        y -= 44

    # 底部说明
    c.setFont("YaHei", 8.5)
    c.setFillColor(GRAY)
    c.drawString(24, 46, "设计模式：单例（ConfigManager）· 策略（IDeviceStrategy）· 门面（backend）· 命令（ControlCommand）· 观察者（信号槽）")

def page4_highlights(c):
    draw_header(c, "技术亮点攻坚", "针对真实工程问题的设计决策与量化收益")
    draw_footer(c, 4)

    items = [
        ("串口粘包/半包处理", DARK,
         "TCP 是字节流协议，一次 read 可能收到半帧或多帧。ReceiveBuffer 采用 peek/consume 双游标模型："
         "先扫描完整帧头(0xABCD)与 CRC16 校验，校验通过才 consume 消费；校验失败则保留缓冲等待后续字节。"
         "延迟压缩策略：仅当空闲区超过阈值才 memmove 压缩，避免每次读都搬内存。",
         "成果：任意拆包/粘包组合下协议帧解析零错误，内存占用有界"),
        ("多线程零阻塞 UI", BLUE,
         "UI/日志/数据库三线程隔离：LogWorker 与 DatabaseWorker 以 moveToThread 模式迁移到工作线程，"
         "主线程仅通过 Qt::QueuedConnection 投递任务后立即返回。数据库写入采用内存缓冲 + 事务批量提交"
         "（100 条或 5 秒触发），将每秒 N 次 fsync 压缩为 1 次。",
         "成果：入库吞吐提升约 500 倍；主线程事件循环零阻塞，界面 60FPS 流畅"),
        ("优先级任务队列防串口冲突", ORANGE,
         "Modbus RTU 是半双工总线，多从站轮询与用户写入存在并发冲突风险。设计双队列（高优先级写入/"
         "普通轮询）+ 状态机（Idle/WaitingReply）：同一时刻仅一个请求在总线上飞行，用户写入命令自动"
         "插队先行；2 秒回复超时看门狗防止故障从站卡死队列。",
         "成果：3 从站 × 差异化轮询间隔（1s/2s/3s）稳定运行，写入延迟 < 100ms"),
        ("指数退避重连", RED,
         "断线后按 1s→2s→4s→8s→16s→30s 递增间隔重连，成功即重置；区分意外断开与用户主动断开"
         "（m_autoReconnect 标志），避免主动断开后死循环重连。",
         "成果：服务器重启后 1~2 秒内自动恢复通信，无人工干预"),
        ("阈值告警与硬件联动", GREEN,
         "温度超阈值触发四级联动：Modbus 写入开启物理排风机 → TCP 同步设备状态 → QML 弹窗+背景闪红"
         "→ CRITICAL 日志双写（文件+数据库）。迟滞带（40℃ 触发/35℃ 恢复）防止阈值边缘抖动误报，"
         "30 秒冷却窗口防止告警刷屏。",
         "成果：告警响应链路 < 1 个轮询周期；边缘抖动零误报"),
    ]

    y = PAGE_H - 46 - 26
    for title, color, desc, result in items:
        # 标题栏
        c.setFillColor(color)
        c.roundRect(24, y - 22, PAGE_W - 48, 26, 6, stroke=0, fill=1)
        c.setFillColor(white)
        c.setFont("Hei", 12)
        c.drawString(34, y - 14, title)
        y -= 30

        # 描述
        c.setFillColor(HexColor("#333333"))
        c.setFont("YaHei", 9)
        # 简单中文换行
        lines = wrap_text(c, desc, 42)
        for line in lines:
            c.drawString(30, y, line)
            y -= 13

        # 成果
        c.setFillColor(GREEN)
        c.setFont("YaHei", 9)
        c.drawString(30, y, "▶ " + result)
        y -= 24

def wrap_text(c, text, max_chars):
    """按字符数简单折行（中文按1字符）"""
    lines, cur = [], ""
    for ch in text:
        cur += ch
        if len(cur) >= max_chars:
            lines.append(cur)
            cur = ""
    if cur:
        lines.append(cur)
    return lines

def main():
    out = "Smart_Lab_Controller_项目介绍.pdf"
    c = canvas.Canvas(out, pagesize=A4)
    c.setTitle("智能实验室控制器 — 项目介绍")

    page1_cover(c)
    c.showPage()

    page2_screenshot(c)
    c.showPage()

    page3_architecture(c)
    c.showPage()

    page4_highlights(c)
    c.save()
    print(f"[OK] PDF 已生成: {os.path.abspath(out)}")

if __name__ == "__main__":
    main()
