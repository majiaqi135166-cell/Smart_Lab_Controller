# -*- coding: utf-8 -*-
"""生成秋招项目展示 PPT（4页：封面/截图/架构图/技术亮点）
所有元素均为可编辑形状，方便手动修改。"""
import os

from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR
from pptx.oxml.ns import qn
from lxml import etree

# ── 配色（与项目 UI 同款色板）──
DARK   = RGBColor(0x2C, 0x3E, 0x50)
BLUE   = RGBColor(0x40, 0x9E, 0xFF)
GREEN  = RGBColor(0x67, 0xC2, 0x3A)
ORANGE = RGBColor(0xE6, 0xA2, 0x3C)
RED    = RGBColor(0xF5, 0x6C, 0x6C)
GRAY   = RGBColor(0x90, 0x93, 0x99)
WHITE  = RGBColor(0xFF, 0xFF, 0xFF)
TEXT   = RGBColor(0x30, 0x30, 0x30)
BG     = RGBColor(0xF5, 0xF7, 0xFA)

SW, SH = Inches(13.333), Inches(7.5)   # 16:9

def set_font(run, name="微软雅黑", size=12, bold=False, color=TEXT):
    """设置字体（含中文字体 ea 标签）"""
    run.font.name = name
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = color
    rPr = run._r.get_or_add_rPr()
    ea = rPr.find(qn('a:ea'))
    if ea is None:
        ea = etree.SubElement(rPr, qn('a:ea'))
    ea.set('typeface', name)

def add_text(slide, x, y, w, h, text, size=12, bold=False, color=TEXT,
             name="微软雅黑", align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP,
             line_spacing=1.15):
    box = slide.shapes.add_textbox(x, y, w, h)
    tf = box.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    lines = text.split("\n")
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.line_spacing = line_spacing
        run = p.add_run()
        run.text = line
        set_font(run, name=name, size=size, bold=bold, color=color)
    return box

def add_rect(slide, x, y, w, h, fill, radius=0.08, line_color=None):
    shape = slide.shapes.add_shape(
        MSO_SHAPE.ROUNDED_RECTANGLE if radius else MSO_SHAPE.RECTANGLE,
        x, y, w, h)
    shape.fill.solid()
    shape.fill.fore_color.rgb = fill
    if line_color:
        shape.line.color.rgb = line_color
        shape.line.width = Pt(1)
    else:
        shape.line.fill.background()
    if radius:
        shape.adjustments[0] = radius
    return shape

def add_arrow(slide, x1, y1, x2, y2, color=GRAY):
    conn = slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT, x1, y1, x2, y2)
    conn.line.color.rgb = color
    conn.line.width = Pt(1.5)
    ln = conn.line._get_or_add_ln()
    tail = etree.SubElement(ln, qn('a:tailEnd'))
    tail.set('type', 'triangle')
    tail.set('w', 'med')
    tail.set('len', 'med')
    return conn

def add_text_in_shape(shape, text, size=12, bold=False, color=WHITE,
                      name="微软雅黑", align=PP_ALIGN.CENTER):
    """在形状内添加多行文字"""
    tf = shape.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    lines = text.split("\n")
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.line_spacing = 1.1
        run = p.add_run()
        run.text = line
        set_font(run, name=name, size=size, bold=bold, color=color)

def add_footer(slide, no):
    add_text(slide, SW - Inches(1.8), SH - Inches(0.45), Inches(1.5),
             Inches(0.3), f"{no} / 4", size=9, color=GRAY, align=PP_ALIGN.RIGHT)

# ══════════════════════════════════════════════════════════════════════
# 第 1 页：封面
# ══════════════════════════════════════════════════════════════════════
def slide1_cover(prs):
    slide = prs.slides.add_slide(prs.slide_layouts[6])  # 空白布局
    # 顶部装饰条
    add_rect(slide, 0, 0, SW, Inches(0.12), BLUE, radius=0)
    # 下半深色区
    add_rect(slide, 0, SH - Inches(2.6), SW, Inches(2.6), DARK, radius=0)

    # 项目名
    add_text(slide, Inches(0.6), Inches(1.0), Inches(12), Inches(1.0),
             "智能实验室控制器", size=48, bold=True, color=DARK, name="黑体")
    # 英文副标题
    add_text(slide, Inches(0.6), Inches(1.95), Inches(12), Inches(0.5),
             "Smart Lab Controller — 上位机监控与控制软件",
             size=17, color=GRAY)
    # 一句话定位
    add_text(slide, Inches(0.6), Inches(2.55), Inches(12), Inches(0.5),
             "基于 C++14 / Qt 5.12 的实验室设备上位机软件",
             size=20, bold=True, color=BLUE)
    add_text(slide, Inches(0.6), Inches(3.1), Inches(12), Inches(0.4),
             "串口 Modbus RTU 多从站采集 + TCP 远程控制 + SQLite 落库，多线程零阻塞 UI",
             size=14, color=TEXT)

    # 技术栈标签
    chips = ["C++14", "Qt 5.12", "QML", "Modbus RTU", "TCP 自定义协议",
             "SQLite", "多线程", "Qt Charts"]
    x = Inches(0.6)
    for chip in chips:
        w = Inches(0.42 + len(chip) * 0.16)
        r = add_rect(slide, x, Inches(3.75), w, Inches(0.42), BLUE, radius=0.5)
        add_text_in_shape(r, chip, size=12, bold=True)
        x += w + Inches(0.18)

    # 核心数据（深色区上的白色卡片）
    stats = [
        ("3 台", "Modbus 从站并行采集\n（温湿度/风机/激光器）"),
        ("2 条", "通信链路\n（RS-485 串口 + TCP）"),
        ("4 个", "工作线程协同\n（UI/日志/数据库）"),
        ("500x", "传感器入库吞吐提升\n（事务批量写入）"),
    ]
    x = Inches(0.6)
    for num, desc in stats:
        card = add_rect(slide, x, Inches(5.15), Inches(2.85), Inches(1.35),
                        RGBColor(0x3A, 0x50, 0x6B), radius=0.12)
        tf = card.text_frame
        tf.word_wrap = True
        tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        p1 = tf.paragraphs[0]; p1.alignment = PP_ALIGN.LEFT
        r1 = p1.add_run(); r1.text = num
        set_font(r1, name="黑体", size=22, bold=True, color=BLUE)
        p2 = tf.add_paragraph(); p2.alignment = PP_ALIGN.LEFT
        r2 = p2.add_run(); r2.text = desc.replace("\n", " ")
        set_font(r2, size=10, color=WHITE)
        x += Inches(3.02)

    # 底部作者信息
    add_text(slide, Inches(0.6), SH - Inches(2.3), Inches(12), Inches(0.35),
             "作者：majiaqi135166-cell", size=12, color=WHITE)
    add_text(slide, Inches(0.6), SH - Inches(1.95), Inches(12), Inches(0.35),
             "代码仓库：github.com/majiaqi135166-cell/Smart_Lab_Controller",
             size=12, color=WHITE)
    add_footer(slide, 1)

# ══════════════════════════════════════════════════════════════════════
# 第 2 页：运行截图
# ══════════════════════════════════════════════════════════════════════
def slide2_screenshot(prs):
    slide = prs.slides.add_slide(prs.slide_layouts[6])
    # 标题栏
    add_rect(slide, 0, 0, SW, Inches(0.9), DARK, radius=0)
    add_text(slide, Inches(0.5), Inches(0.18), Inches(10), Inches(0.55),
             "软件运行界面", size=22, bold=True, color=WHITE, name="黑体")
    add_text(slide, SW - Inches(5.3), Inches(0.3), Inches(4.8), Inches(0.4),
             "Qt/QML 界面设计 · 实时监控 · 数据库历史",
             size=11, color=WHITE, align=PP_ALIGN.RIGHT)

    img_path = "screenshot.png"
    img_h = Inches(5.6)
    if os.path.exists(img_path):
        from pptx.util import Emu
        # 按比例计算宽度（截图是竖屏 398x758）
        img_w = int(img_h * 398 / 758)
        left = Inches(0.7)
        top = Inches(1.25)
        # 阴影
        add_rect(slide, left + Inches(0.08), top + Inches(0.08),
                 img_w, img_h, RGBColor(0xD0, 0xD0, 0xD0), radius=0.03)
        slide.shapes.add_picture(img_path, left, top, height=img_h)

    # 右侧布局说明
    x = Inches(4.6)
    add_text(slide, x, Inches(1.35), Inches(8.2), Inches(0.45),
             "界面布局设计要点", size=17, bold=True, color=DARK, name="黑体")
    notes = [
        ("设备控制区", "激光功率 Slider / 风扇 Switch / 亮度调节，\nQ_PROPERTY 双向绑定即时生效"),
        ("Modbus 面板", "串口配置、三色状态灯（🟢正常/🟡呼吸闪烁/🔴断开）、\n寄存器实时值、在线写入"),
        ("传感器趋势图", "DateTimeAxis 真实时间轴，最近 5 分钟\n温度/湿度双曲线滚动窗口"),
        ("数据库与图表", "历史记录列表 + CSV 导出，\n100ms 采样实时数值监控"),
        ("告警联动", "温度超阈值 → 弹窗 + 背景闪红 +\n自动开启排风（硬件联动）"),
    ]
    y = Inches(2.0)
    for title, desc in notes:
        dot = add_rect(slide, x, y + Inches(0.08), Inches(0.16), Inches(0.16),
                       BLUE, radius=0.5)
        add_text(slide, x + Inches(0.3), y, Inches(8.0), Inches(0.9),
                 f"{title}：{desc}", size=12, color=TEXT)
        y += Inches(1.0)
    add_footer(slide, 2)

# ══════════════════════════════════════════════════════════════════════
# 第 3 页：架构图
# ══════════════════════════════════════════════════════════════════════
def slide3_architecture(prs):
    slide = prs.slides.add_slide(prs.slide_layouts[6])
    add_rect(slide, 0, 0, SW, Inches(0.9), DARK, radius=0)
    add_text(slide, Inches(0.5), Inches(0.18), Inches(8), Inches(0.55),
             "软件架构", size=22, bold=True, color=WHITE, name="黑体")
    add_text(slide, SW - Inches(5.5), Inches(0.3), Inches(5.0), Inches(0.4),
             "三层解耦 · 四线程协同 · 双链路通信",
             size=11, color=WHITE, align=PP_ALIGN.RIGHT)

    # 主线三层
    layers = [
        (DARK, "UI 层（QML · 主线程）", "设备控制 / Modbus 监控 / 实时曲线 / 告警弹窗", Inches(1.15)),
        (BLUE, "桥接层（backend）", "Q_PROPERTY 双向绑定 · 信号转发", Inches(2.75)),
        (GREEN, "业务逻辑层（LabApplicationService）", "状态聚合 · 阈值告警 · 硬件联动 · 线程编排", Inches(4.35)),
    ]
    bx = Inches(2.9); bw = Inches(3.7)
    for color, title, sub, y in layers:
        r = add_rect(slide, bx, y, bw, Inches(0.95), color, radius=0.12)
        tf = r.text_frame
        tf.word_wrap = True
        tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        p1 = tf.paragraphs[0]; p1.alignment = PP_ALIGN.CENTER
        r1 = p1.add_run(); r1.text = title
        set_font(r1, name="黑体", size=14, bold=True, color=WHITE)
        p2 = tf.add_paragraph(); p2.alignment = PP_ALIGN.CENTER
        r2 = p2.add_run(); r2.text = sub
        set_font(r2, size=9.5, color=WHITE)

    # 主线箭头
    cx = bx + bw / 2
    add_arrow(slide, cx, Inches(2.1), cx, Inches(2.7))
    add_arrow(slide, cx, Inches(3.7), cx, Inches(4.3))

    # 左分支（TCP）
    add_arrow(slide, cx - Inches(1.0), Inches(5.3), Inches(1.7), Inches(5.75))
    for title, sub, color, y in [
        ("LabController", "Fan/Laser 策略 · 统一指令", BLUE, Inches(5.75)),
        ("TcpDeviceChannel", "自定义协议 · 心跳看门狗 · 粘包处理", BLUE, Inches(6.65)),
    ]:
        r = add_rect(slide, Inches(0.35), y, Inches(2.9), Inches(0.75), color, radius=0.12)
        tf = r.text_frame; tf.word_wrap = True; tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        p1 = tf.paragraphs[0]; p1.alignment = PP_ALIGN.CENTER
        r1 = p1.add_run(); r1.text = title; set_font(r1, name="黑体", size=12, bold=True, color=WHITE)
        p2 = tf.add_paragraph(); p2.alignment = PP_ALIGN.CENTER
        r2 = p2.add_run(); r2.text = sub; set_font(r2, size=8.5, color=WHITE)
    add_arrow(slide, Inches(1.8), Inches(6.5), Inches(1.8), Inches(6.65))
    add_text(slide, Inches(0.35), Inches(7.05), Inches(2.9), Inches(0.35),
             "TCP 远程服务器", size=11, color=GRAY, align=PP_ALIGN.CENTER)

    # 右分支（Modbus）
    add_arrow(slide, cx + Inches(1.0), Inches(5.3), Inches(7.8), Inches(5.75))
    for title, sub, color, y in [
        ("ModbusManager", "多从站轮询 · 优先级任务队列 · 状态机", ORANGE, Inches(5.75)),
        ("QModbusRtuSerialMaster", "Modbus RTU · RS-485 总线", ORANGE, Inches(6.65)),
    ]:
        r = add_rect(slide, Inches(6.25), y, Inches(2.9), Inches(0.75), color, radius=0.12)
        tf = r.text_frame; tf.word_wrap = True; tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        p1 = tf.paragraphs[0]; p1.alignment = PP_ALIGN.CENTER
        r1 = p1.add_run(); r1.text = title; set_font(r1, name="黑体", size=12, bold=True, color=WHITE)
        p2 = tf.add_paragraph(); p2.alignment = PP_ALIGN.CENTER
        r2 = p2.add_run(); r2.text = sub; set_font(r2, size=8.5, color=WHITE)
    add_arrow(slide, Inches(7.7), Inches(6.5), Inches(7.7), Inches(6.65))
    add_text(slide, Inches(6.25), Inches(7.05), Inches(2.9), Inches(0.35),
             "RS-485 从站 ×3", size=11, color=GRAY, align=PP_ALIGN.CENTER)

    # 右侧线程模型
    tx = Inches(9.9)
    bg = add_rect(slide, tx, Inches(1.15), Inches(3.1), Inches(5.55), BG, radius=0.06,
                  line_color=RGBColor(0xDC, 0xDF, 0xE6))
    add_text(slide, tx + Inches(0.15), Inches(1.25), Inches(2.8), Inches(0.35),
             "线程模型（4 线程）", size=13, bold=True, color=DARK, name="黑体")
    threads = [
        ("主线程", "UI + Modbus 轮询 + TCP\n事件循环零阻塞", BLUE),
        ("LogWorker 线程", "文件日志异步落盘\nQueuedConnection 投递", GREEN),
        ("DatabaseWorker 线程", "SQLite 事务批量写入\n100条 / 5秒 触发", ORANGE),
        ("重连定时器", "指数退避 1s → 30s\n单发定时器", RED),
    ]
    y = Inches(1.75)
    for name, desc, color in threads:
        r = add_rect(slide, tx + Inches(0.12), y, Inches(2.86), Inches(1.0), color, radius=0.12)
        tf = r.text_frame; tf.word_wrap = True; tf.vertical_anchor = MSO_ANCHOR.MIDDLE
        p1 = tf.paragraphs[0]; p1.alignment = PP_ALIGN.CENTER
        r1 = p1.add_run(); r1.text = name; set_font(r1, name="黑体", size=11, bold=True, color=WHITE)
        for line in desc.split("\n"):
            p = tf.add_paragraph(); p.alignment = PP_ALIGN.CENTER
            run = p.add_run(); run.text = line; set_font(run, size=8.5, color=WHITE)
        y += Inches(1.15)

    # 底部设计模式
    add_text(slide, Inches(0.5), Inches(7.15), Inches(9), Inches(0.3),
             "设计模式：单例（ConfigManager）· 策略（IDeviceStrategy）· 门面（backend）· 命令（ControlCommand）· 观察者（信号槽）",
             size=9, color=GRAY)
    add_footer(slide, 3)

# ══════════════════════════════════════════════════════════════════════
# 第 4 页：技术亮点
# ══════════════════════════════════════════════════════════════════════
def slide4_highlights(prs):
    slide = prs.slides.add_slide(prs.slide_layouts[6])
    add_rect(slide, 0, 0, SW, Inches(0.9), DARK, radius=0)
    add_text(slide, Inches(0.5), Inches(0.18), Inches(8), Inches(0.55),
             "技术亮点攻坚", size=22, bold=True, color=WHITE, name="黑体")
    add_text(slide, SW - Inches(5.5), Inches(0.3), Inches(5.0), Inches(0.4),
             "针对真实工程问题的设计决策与量化收益",
             size=11, color=WHITE, align=PP_ALIGN.RIGHT)

    items = [
        ("串口粘包 / 半包处理", BLUE,
         "TCP 字节流一次 read 可能收到半帧或多帧。ReceiveBuffer 采用 peek/consume 双游标："
         "扫描完整帧头 + CRC16 校验通过才消费；延迟压缩策略避免频繁内存搬移。",
         "任意拆包/粘包组合下协议帧解析零错误，内存占用有界"),
        ("多线程零阻塞 UI", GREEN,
         "LogWorker / DatabaseWorker 以 moveToThread 模式迁移工作线程，主线程通过 "
         "Qt::QueuedConnection 投递任务后立即返回；数据库写入采用内存缓冲 + 事务批量提交"
         "（100 条或 5 秒触发），N 次 fsync 压缩为 1 次。",
         "入库吞吐提升约 500 倍；主线程事件循环零阻塞，界面流畅不卡顿"),
        ("优先级任务队列防串口冲突", ORANGE,
         "Modbus RTU 半双工总线，多从站轮询与用户写入存在并发冲突。双队列（高优先写入/"
         "普通轮询）+ 状态机（Idle / WaitingReply）：同一时刻仅一个请求在飞，写入自动插队；"
         "2 秒超时看门狗防止故障从站卡死队列。",
         "3 从站 × 差异化轮询间隔稳定运行，写入延迟 < 100ms"),
        ("指数退避重连", RED,
         "断线后按 1s→2s→4s→8s→16s→30s 递增间隔重连，成功即重置；"
         "区分意外断开与用户主动断开，避免主动断开后死循环重连。",
         "服务器重启后 1~2 秒自动恢复通信，无人工干预"),
        ("阈值告警与硬件联动", DARK,
         "温度超阈值四级联动：Modbus 写物理排风机 → TCP 同步设备状态 → QML 弹窗+背景闪红 → "
         "CRITICAL 日志双写（文件+数据库）。迟滞带（40℃触发 / 35℃恢复）防边缘抖动，"
         "30 秒冷却防刷屏。",
         "告警响应 < 1 个轮询周期；阈值边缘抖动零误报"),
    ]

    y = Inches(1.05)
    for title, color, desc, result in items:
        # 左侧色条 + 标题
        add_rect(slide, Inches(0.5), y, Inches(0.12), Inches(1.1), color, radius=0)
        add_text(slide, Inches(0.75), y + Inches(0.03), Inches(3.2), Inches(0.4),
                 title, size=13.5, bold=True, color=color, name="黑体")
        # 描述
        add_text(slide, Inches(0.75), y + Inches(0.4), Inches(11.0), Inches(0.42),
                 desc, size=9.5, color=TEXT)
        # 成果
        add_text(slide, Inches(0.75), y + Inches(0.82), Inches(11.5), Inches(0.3),
                 "▶ " + result, size=9.5, bold=True, color=GREEN)
        y += Inches(1.2)
    add_footer(slide, 4)

def main():
    prs = Presentation()
    prs.slide_width = SW
    prs.slide_height = SH

    slide1_cover(prs)
    slide2_screenshot(prs)
    slide3_architecture(prs)
    slide4_highlights(prs)

    out = "Smart_Lab_Controller_项目介绍.pptx"
    prs.save(out)
    print(f"[OK] PPT 已生成: {os.path.abspath(out)}")

if __name__ == "__main__":
    main()
