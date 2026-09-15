# -*- coding: utf-8 -*-
"""核心代码选段 PDF（语法高亮，作品集用）
展示文件：modbusmanager.cpp（串口通信封装·任务队列·状态机）
        databaseworker.cpp（多线程·事务批量落库）"""
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfgen import canvas
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Preformatted,
                                Table, TableStyle)
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from pygments import lex
from pygments.lexers import CLexer
from pygments.token import Token

pdfmetrics.registerFont(TTFont("Hei", "C:/Windows/Fonts/simhei.ttf"))
pdfmetrics.registerFont(TTFont("YaHei", "C:/Windows/Fonts/msyh.ttc", subfontIndex=0))
pdfmetrics.registerFont(TTFont("YaHeiBd", "C:/Windows/Fonts/msyhbd.ttc", subfontIndex=0))

DARK, BLUE, GRAY = HexColor("#2C3E50"), HexColor("#409EFF"), HexColor("#909399")

# Pygments 默认风格 → reportlab 颜色映射
COLORS = {
    Token.Comment:        HexColor("#6A737D"),
    Token.Keyword:        HexColor("#D73A49"),
    Token.String:         HexColor("#032F62"),
    Token.Number:         HexColor("#005CC5"),
    Token.Name.Function:  HexColor("#6F42C1"),
    Token.Name.Class:     HexColor("#6F42C1"),
    Token.Name:           HexColor("#24292E"),
    Token.Operator:       HexColor("#D73A49"),
    Token.Punctuation:    HexColor("#24292E"),
    Token.Text:           HexColor("#24292E"),
    Token.Preproc:        HexColor("#22863A"),
}
FALLBACK = HexColor("#24292E")

def render_code_page(c, code_text, title, subtitle, start_y):
    """在 canvas 上渲染一段高亮代码，返回结束 Y 坐标"""
    c.setFillColor(DARK)
    c.rect(0, start_y - 20, A4[0], 44, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("Hei", 13)
    c.drawString(20 * mm, start_y - 5, title)
    c.setFont("YaHei", 8.5)
    c.drawString(20 * mm, start_y - 16, subtitle)

    # 代码行
    lines = code_text.split("\n")
    y = start_y - 34
    x0 = 14 * mm
    line_h = 8.6
    max_lines_per_page = 0
    # 页眉页脚留白
    usable = y - 16 * mm

    for lineno, line in enumerate(lines, 1):
        # 行号
        c.setFont("YaHei", 6.5)
        c.setFillColor(HexColor("#B0B0B0"))
        c.drawRightString(x0 - 5, y - 2, str(lineno))
        # 语法高亮文本
        x = x0 + 2
        tokens = list(lex(line, CLexer()))
        c.setFont("YaHei", 7)
        for ttype, value in tokens:
            color = COLORS.get(ttype, FALLBACK)
            # 细分：Name.Function / Name.Class 优先
            for specific in (Token.Name.Function, Token.Name.Class):
                if specific in ttype:
                    color = COLORS[specific]
                    break
            if Token.Comment in ttype:
                color = COLORS[Token.Comment]
            c.setFillColor(color)
            c.drawString(x, y - 2, value)
            x += c.stringWidth(value, "YaHei", 7)
        y -= line_h
    return y

def build_pdf():
    out = "Smart_Lab_Controller_核心代码选段.pdf"
    c = canvas.Canvas(out, pagesize=A4)
    c.setTitle("核心代码选段")

    # ═══ 封面 ═══
    c.setFillColor(DARK)
    c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(BLUE)
    c.rect(0, A4[1] - 8, A4[0], 8, stroke=0, fill=1)
    c.setFillColor(white)
    c.setFont("Hei", 30)
    c.drawString(20 * mm, A4[1] * 0.65, "核心代码选段")
    c.setFont("YaHei", 13)
    c.drawString(20 * mm, A4[1] * 0.65 - 30,
                 "Smart Lab Controller — 串口通信封装 · 多线程数据处理")
    c.setFont("YaHei", 10)
    c.drawString(20 * mm, A4[1] * 0.65 - 55,
                 "代码文件：src/Modbus/modbusmanager.cpp  ·  databaseworker.cpp")
    c.drawString(20 * mm, A4[1] * 0.65 - 72,
                 "展示要点：优先级任务队列 · 状态机 · 指数退避 · 事务批量写入 · 线程安全")
    c.showPage()

    # ═══ 文件 1：modbusmanager.cpp ═══
    code1 = open("src/Modbus/modbusmanager.cpp", encoding="utf-8").read()
    # 精简：取关键段落（头注释 + 构造 + 队列核心 + 状态机 + 退避）
    # 完整文件太长，节选最具代表性的函数
    sections = []
    def extract(start_marker, end_marker):
        i = code1.find(start_marker)
        j = code1.find(end_marker, i)
        return code1[i:j + len(end_marker)] if i >= 0 else ""

    sections.append("// ═══ 节选：任务队列核心（enqueue + processQueue + 状态机）═══\n\n")
    sections.append(extract("void ModbusManager::enqueue",
                            "void ModbusManager::onReplyFinished()"))
    sections.append(extract("void ModbusManager::onReplyFinished",
                            "// ════"))
    code_display = "".join(sections)

    y = render_code_page(c, code_display,
                         "modbusmanager.cpp — 串口通信封装类",
                         "优先级双队列 · 状态机防半双工冲突 · 2s 超时看门狗 · 读写回复区分",
                         A4[1] - 30)
    # 页脚
    c.setFont("YaHei", 8)
    c.setFillColor(GRAY)
    c.drawCentredString(A4[0] / 2, 14, "1")
    c.drawRightString(A4[0] - 20, 14, "src/Modbus/modbusmanager.cpp")
    c.showPage()

    # ═══ 文件 2：databaseworker.cpp ═══
    code2 = open("databaseworker.cpp", encoding="utf-8").read()
    sections2 = []
    sections2.append("// ═══ 节选：批量落库（缓冲 + 事务 + execBatch）═══\n\n")
    i = code2.find("void DatabaseWorker::logSensorData")
    j = code2.find("// ═══", i)
    sections2.append(code2[i:j])
    code_display2 = "".join(sections2)

    render_code_page(c, code_display2,
                     "databaseworker.cpp — 多线程数据处理类",
                     "内存缓冲（100条/5秒双触发）· transaction + execBatch · 失败回滚不清空缓冲",
                     A4[1] - 30)
    c.setFont("YaHei", 8)
    c.setFillColor(GRAY)
    c.drawCentredString(A4[0] / 2, 14, "2")
    c.drawRightString(A4[0] - 20, 14, "databaseworker.cpp")

    c.save()
    print(f"[OK] 代码选段 PDF 已生成: {out}")

if __name__ == "__main__":
    build_pdf()
