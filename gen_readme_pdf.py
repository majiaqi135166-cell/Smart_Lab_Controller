# -*- coding: utf-8 -*-
"""README.md → 排版精美的 PDF（作品集用）
支持：标题/表格/代码块/列表/粗体/链接/分隔线；mermaid 图块以说明框替代。"""
import re
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfgen import canvas
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                TableStyle, Preformatted, HRFlowable)
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.enums import TA_LEFT

pdfmetrics.registerFont(TTFont("Hei", "C:/Windows/Fonts/simhei.ttf"))
pdfmetrics.registerFont(TTFont("YaHei", "C:/Windows/Fonts/msyh.ttc", subfontIndex=0))
pdfmetrics.registerFont(TTFont("YaHeiBd", "C:/Windows/Fonts/msyhbd.ttc", subfontIndex=0))

DARK, BLUE, GRAY = HexColor("#2C3E50"), HexColor("#409EFF"), HexColor("#909399")

def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def md_bold(s):
    return re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", s)

def md_code(s):
    return re.sub(r"`(.+?)`", r'<font face="YaHei" color="#C7254E" backColor="#F9F2F4">\1</font>', s)

def inline(s):
    return md_code(md_bold(esc(s)))

def build_story(md_path):
    styles = {
        "h1": ParagraphStyle("h1", fontName="Hei", fontSize=21, leading=30,
                             textColor=DARK, spaceBefore=6, spaceAfter=14),
        "h2": ParagraphStyle("h2", fontName="Hei", fontSize=15, leading=22,
                             textColor=BLUE, spaceBefore=18, spaceAfter=8),
        "h3": ParagraphStyle("h3", fontName="YaHeiBd", fontSize=12, leading=18,
                             textColor=DARK, spaceBefore=10, spaceAfter=6),
        "body": ParagraphStyle("body", fontName="YaHei", fontSize=10, leading=17,
                               textColor=HexColor("#333333"), spaceAfter=6),
        "li": ParagraphStyle("li", fontName="YaHei", fontSize=10, leading=16,
                             textColor=HexColor("#333333"), leftIndent=14,
                             bulletIndent=4, spaceAfter=3),
        "code": ParagraphStyle("code", fontName="YaHei", fontSize=8, leading=11.5,
                               textColor=HexColor("#24292E")),
        "caption": ParagraphStyle("caption", fontName="YaHei", fontSize=8.5,
                                  textColor=GRAY, leading=13, spaceBefore=4, spaceAfter=10),
    }
    story = []
    lines = open(md_path, encoding="utf-8").read().split("\n")
    i, in_code, code_buf = 0, False, []
    in_table, table_rows = False, []

    def flush_table():
        nonlocal table_rows
        if not table_rows: return
        header = table_rows[0]
        rows = table_rows[1:]
        data = [[Paragraph(inline(c), styles["body"]) for c in header]]
        for r in rows:
            data.append([Paragraph(inline(c), styles["body"]) for c in r])
        t = Table(data, colWidths=None)
        t.setStyle(TableStyle([
            ("BACKGROUND", (0, 0), (-1, 0), DARK),
            ("TEXTCOLOR", (0, 0), (-1, 0), white),
            ("FONTNAME", (0, 0), (-1, 0), "YaHeiBd"),
            ("FONTSIZE", (0, 0), (-1, 0), 9.5),
            ("FONTNAME", (0, 1), (-1, -1), "YaHei"),
            ("FONTSIZE", (0, 1), (-1, -1), 9),
            ("GRID", (0, 0), (-1, -1), 0.5, HexColor("#DCDFE6")),
            ("ROWBACKGROUNDS", (0, 1), (-1, -1), [white, HexColor("#F5F7FA")]),
            ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
            ("TOPPADDING", (0, 0), (-1, -1), 5),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ]))
        story.append(t)
        story.append(Spacer(1, 10))
        table_rows = []

    while i < len(lines):
        line = lines[i]
        # 代码块
        if line.strip().startswith("```"):
            if in_code:
                code_text = "\n".join(code_buf)
                story.append(Preformatted(code_text, styles["code"]))
                story.append(Spacer(1, 8))
                in_code, code_buf = False, []
            else:
                in_code = True
                # mermaid 图块：用说明框替代
                if line.strip() == "```mermaid":
                    i += 1
                    mermaid_lines = []
                    while i < len(lines) and not lines[i].strip().startswith("```"):
                        mermaid_lines.append(lines[i]); i += 1
                    note = ("（此处为 Mermaid 架构图，GitHub 在线版可渲染；"
                            "本 PDF 版架构图见 PPT《Smart_Lab_Controller_项目介绍.pptx》第 3 页）")
                    story.append(Paragraph(inline(note), styles["caption"]))
                    in_code = False
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue
        # 表格
        if line.strip().startswith("|"):
            if not in_table:
                in_table, table_rows = True, []
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if all(re.fullmatch(r":?-{2,}:?", c) for c in cells):
                i += 1
                continue  # 分隔行
            table_rows.append(cells)
            i += 1
            continue
        if in_table:
            in_table = False
            flush_table()
        # 标题
        m = re.match(r"^(#{1,3})\s+(.*)", line)
        if m:
            level = len(m.group(1))
            story.append(Paragraph(inline(m.group(2)), styles[f"h{level}"]))
            i += 1
            continue
        # 分隔线
        if line.strip() == "---":
            story.append(HRFlowable(width="100%", thickness=0.8,
                                    color=HexColor("#DCDFE6"), spaceAfter=12))
            i += 1
            continue
        # 列表
        m = re.match(r"^\s*-\s+(.*)", line)
        if m:
            story.append(Paragraph(inline(m.group(1)), styles["li"], bulletText="•"))
            i += 1
            continue
        # 正文
        if line.strip():
            story.append(Paragraph(inline(line), styles["body"]))
        i += 1

    flush_table()
    return story

def footer(canvas_obj, doc):
    canvas_obj.saveState()
    canvas_obj.setFont("YaHei", 8)
    canvas_obj.setFillColor(GRAY)
    canvas_obj.drawCentredString(A4[0] / 2, 14, f"第 {doc.page} 页")
    canvas_obj.drawRightString(A4[0] - 20, 14,
                               "Smart Lab Controller · GitHub: majiaqi135166-cell")
    canvas_obj.restoreState()

def main():
    story = build_story("README.md")
    doc = SimpleDocTemplate("Smart_Lab_Controller_README.pdf", pagesize=A4,
                            leftMargin=20 * mm, rightMargin=20 * mm,
                            topMargin=18 * mm, bottomMargin=18 * mm,
                            title="Smart Lab Controller — README")
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print("[OK] README PDF 已生成")

if __name__ == "__main__":
    main()
