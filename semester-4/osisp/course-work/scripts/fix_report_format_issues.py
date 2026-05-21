from __future__ import annotations

import copy
import re
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-format-fixes.docx"

NS = {
    "w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main",
}

for prefix, uri in NS.items():
    ET.register_namespace(prefix, uri)


def qn(name: str) -> str:
    prefix, tag = name.split(":")
    return f"{{{NS[prefix]}}}{tag}"


def p_text(p: ET.Element) -> str:
    return "".join(t.text or "" for t in p.findall(".//w:t", NS))


def p_text_with_tabs(p: ET.Element) -> str:
    out: list[str] = []

    def walk(node: ET.Element) -> None:
        if node.tag == qn("w:t"):
            out.append(node.text or "")
        elif node.tag == qn("w:tab"):
            out.append("\t")
        for child in list(node):
            walk(child)

    walk(p)
    return "".join(out)


def clear_theme_color(color: ET.Element) -> None:
    for attr in ["themeColor", "themeTint", "themeShade"]:
        color.attrib.pop(qn(f"w:{attr}"), None)


def set_black_color(rpr: ET.Element) -> None:
    color = rpr.find("w:color", NS)
    if color is None:
        color = ET.SubElement(rpr, qn("w:color"))
    color.set(qn("w:val"), "000000")
    clear_theme_color(color)


def set_all_text_black(root: ET.Element) -> None:
    for rpr in root.findall(".//w:rPr", NS):
        set_black_color(rpr)


def remove_hyperlink_blue(styles: ET.Element) -> None:
    for color in styles.findall(".//w:color", NS):
        color.set(qn("w:val"), "000000")
        clear_theme_color(color)

    for style in styles.findall(".//w:style", NS):
        style_id = style.get(qn("w:styleId"), "")
        name = style.find("w:name", NS)
        name_value = name.get(qn("w:val"), "") if name is not None else ""
        if style_id in {"Hyperlink", "FollowedHyperlink"} or name_value in {"Hyperlink", "FollowedHyperlink"}:
            rpr = style.find("w:rPr", NS)
            if rpr is None:
                rpr = ET.SubElement(style, qn("w:rPr"))
            set_black_color(rpr)
            underline = rpr.find("w:u", NS)
            if underline is None:
                underline = ET.SubElement(rpr, qn("w:u"))
            underline.set(qn("w:val"), "none")


def make_plain_toc_run(title: str, page: str) -> ET.Element:
    r = ET.Element(qn("w:r"))
    t1 = ET.SubElement(r, qn("w:t"))
    t1.text = title
    ET.SubElement(r, qn("w:tab"))
    t2 = ET.SubElement(r, qn("w:t"))
    t2.text = page
    return r


def make_plain_run(text: str) -> ET.Element:
    r = ET.Element(qn("w:r"))
    rpr = ET.SubElement(r, qn("w:rPr"))
    fonts = ET.SubElement(rpr, qn("w:rFonts"))
    fonts.set(qn("w:ascii"), "Times New Roman")
    fonts.set(qn("w:hAnsi"), "Times New Roman")
    fonts.set(qn("w:cs"), "Times New Roman")
    sz = ET.SubElement(rpr, qn("w:sz"))
    sz.set(qn("w:val"), "28")
    szcs = ET.SubElement(rpr, qn("w:szCs"))
    szcs.set(qn("w:val"), "28")
    set_black_color(rpr)
    t = ET.SubElement(r, qn("w:t"))
    if text[:1].isspace() or text[-1:].isspace():
        t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    t.text = text
    return r


def replace_p_with_run(p: ET.Element, run: ET.Element) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    p.append(run)


def fix_toc(document: ET.Element) -> None:
    paragraphs = document.findall(".//w:p", NS)
    start = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "СОДЕРЖАНИЕ")
    end = next(i for i in range(start + 1, len(paragraphs)) if p_text(paragraphs[i]).strip() == "ВВЕДЕНИЕ")

    for p in paragraphs[start + 1 : end]:
        raw = p_text_with_tabs(p).replace("\u00a0", " ").strip()
        if not raw:
            continue
        raw = re.sub(r"\s+", " ", raw)
        m = re.match(r"^(.*?)[\t ]*(\d+)$", raw)
        if not m:
            continue
        title = m.group(1).strip()
        page = m.group(2)
        replace_p_with_run(p, make_plain_toc_run(title, page))


def move_citations_to_paragraph_end(document: ET.Element) -> None:
    paragraphs = document.findall(".//w:p", NS)
    try:
        toc_end = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "ВВЕДЕНИЕ")
        bibliography = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "СПИСОК ИСПОЛЬЗОВАННЫХ ИСТОЧНИКОВ")
    except StopIteration:
        return

    citation_re = re.compile(r"\s*\[(\d+(?:,\s*\d+)*)\]")

    for p in paragraphs[toc_end + 1 : bibliography]:
        text = p_text(p).strip()
        if not text or text.startswith("["):
            continue

        matches = citation_re.findall(text)
        if not matches:
            continue

        refs: list[str] = []
        for match in matches:
            for ref in re.split(r",\s*", match):
                if ref not in refs:
                    refs.append(ref)

        without_refs = citation_re.sub("", text)
        without_refs = re.sub(r"\s+([.,;:])", r"\1", without_refs)
        without_refs = re.sub(r"\s{2,}", " ", without_refs).strip()

        if re.search(rf"\[{', '.join(refs)}\]\.?$", text):
            continue

        punctuation = "."
        if without_refs and without_refs[-1] in ".!?":
            punctuation = without_refs[-1]
            without_refs = without_refs[:-1].rstrip()
        new_text = f"{without_refs} [{', '.join(refs)}]{punctuation}"
        replace_p_with_run(p, make_plain_run(new_text))


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    styles = ET.fromstring(entries["word/styles.xml"])

    fix_toc(document)
    move_citations_to_paragraph_end(document)
    set_all_text_black(document)
    remove_hyperlink_blue(styles)

    entries["word/document.xml"] = ET.tostring(document, encoding="utf-8", xml_declaration=True)
    entries["word/styles.xml"] = ET.tostring(styles, encoding="utf-8", xml_declaration=True)

    tmp = DOCX.with_suffix(".tmp.docx")
    with ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for name, data in entries.items():
            zout.writestr(name, data)
    tmp.replace(DOCX)


if __name__ == "__main__":
    main()
