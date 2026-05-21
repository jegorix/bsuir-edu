from __future__ import annotations

import copy
import re
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-new-resources.docx"
DIAGRAMS = [
    ("course_diagram_algorithm.png", "Схема алгоритма поиска inode по абсолютному пути"),
    ("course_diagram_functional_structure.png", "Схема функциональной структуры программы"),
    ("course_diagram_state.png", "Диаграмма состояний программы"),
    ("course_diagram_interaction.png", "Схема взаимодействия с пользователем и внешней средой"),
]

NS = {
    "w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    "wp": "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing",
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "pic": "http://schemas.openxmlformats.org/drawingml/2006/picture",
    "rel": "http://schemas.openxmlformats.org/package/2006/relationships",
}

for prefix, uri in NS.items():
    ET.register_namespace(prefix if prefix != "rel" else "", uri)


def qn(name: str) -> str:
    prefix, tag = name.split(":")
    return f"{{{NS[prefix]}}}{tag}"


def p_text(p: ET.Element) -> str:
    return "".join(t.text or "" for t in p.findall(".//w:t", NS))


def text_nodes(root: ET.Element) -> list[ET.Element]:
    return root.findall(".//w:t", NS)


def first_run_props(p: ET.Element) -> ET.Element | None:
    run = p.find("w:r", NS)
    if run is None:
        return None
    rpr = run.find("w:rPr", NS)
    return copy.deepcopy(rpr) if rpr is not None else None


def set_p_text(p: ET.Element, text: str) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    rpr = first_run_props(p)
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    run = ET.SubElement(p, qn("w:r"))
    if rpr is not None:
        run.append(rpr)
    t = ET.SubElement(run, qn("w:t"))
    if text[:1].isspace() or text[-1:].isspace():
        t.set(qn("xml:space") if False else "{http://www.w3.org/XML/1998/namespace}space", "preserve")
    t.text = text


def make_run(text: str, *, bold: bool = False, size: int = 28) -> ET.Element:
    run = ET.Element(qn("w:r"))
    rpr = ET.SubElement(run, qn("w:rPr"))
    fonts = ET.SubElement(rpr, qn("w:rFonts"))
    fonts.set(qn("w:ascii"), "Times New Roman")
    fonts.set(qn("w:hAnsi"), "Times New Roman")
    fonts.set(qn("w:cs"), "Times New Roman")
    if bold:
        ET.SubElement(rpr, qn("w:b"))
        ET.SubElement(rpr, qn("w:bCs"))
    sz = ET.SubElement(rpr, qn("w:sz"))
    sz.set(qn("w:val"), str(size))
    szcs = ET.SubElement(rpr, qn("w:szCs"))
    szcs.set(qn("w:val"), str(size))
    t = ET.SubElement(run, qn("w:t"))
    if text[:1].isspace() or text[-1:].isspace():
        t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    t.text = text
    return run


def make_para(text: str = "", template: ET.Element | None = None, *, bold: bool = False, align: str | None = None, page_break: bool = False) -> ET.Element:
    p = ET.Element(qn("w:p"))
    if template is not None:
        ppr = template.find("w:pPr", NS)
        if ppr is not None:
            p.append(copy.deepcopy(ppr))
    if align:
        ppr = p.find("w:pPr", NS)
        if ppr is None:
            ppr = ET.SubElement(p, qn("w:pPr"))
        jc = ppr.find("w:jc", NS)
        if jc is None:
            jc = ET.SubElement(ppr, qn("w:jc"))
        jc.set(qn("w:val"), align)
    if page_break:
        run = ET.SubElement(p, qn("w:r"))
        br = ET.SubElement(run, qn("w:br"))
        br.set(qn("w:type"), "page")
    if text:
        p.append(make_run(text, bold=bold))
    return p


def make_cell(text: str, width: int, *, bold: bool = False) -> ET.Element:
    tc = ET.Element(qn("w:tc"))
    tcpr = ET.SubElement(tc, qn("w:tcPr"))
    tcw = ET.SubElement(tcpr, qn("w:tcW"))
    tcw.set(qn("w:w"), str(width))
    tcw.set(qn("w:type"), "dxa")
    tc.append(make_para(text, bold=bold))
    return tc


def make_table(headers: list[str], rows: list[list[str]], widths: list[int]) -> ET.Element:
    tbl = ET.Element(qn("w:tbl"))
    tblpr = ET.SubElement(tbl, qn("w:tblPr"))
    borders = ET.SubElement(tblpr, qn("w:tblBorders"))
    for side in ["top", "left", "bottom", "right", "insideH", "insideV"]:
        border = ET.SubElement(borders, qn(f"w:{side}"))
        border.set(qn("w:val"), "single")
        border.set(qn("w:sz"), "4")
        border.set(qn("w:space"), "0")
        border.set(qn("w:color"), "000000")
    grid = ET.SubElement(tbl, qn("w:tblGrid"))
    for width in widths:
        col = ET.SubElement(grid, qn("w:gridCol"))
        col.set(qn("w:w"), str(width))

    for values, bold in [(headers, True), *[(row, False) for row in rows]]:
        tr = ET.SubElement(tbl, qn("w:tr"))
        for value, width in zip(values, widths):
            tr.append(make_cell(value, width, bold=bold))
    return tbl


def make_image_para(rid: str, descr: str, docpr_id: int) -> ET.Element:
    cx = 5_940_000
    cy = 4_200_000
    p = ET.Element(qn("w:p"))
    ppr = ET.SubElement(p, qn("w:pPr"))
    jc = ET.SubElement(ppr, qn("w:jc"))
    jc.set(qn("w:val"), "center")
    run = ET.SubElement(p, qn("w:r"))
    drawing = ET.SubElement(run, qn("w:drawing"))
    inline = ET.SubElement(drawing, qn("wp:inline"))
    inline.set("distT", "0")
    inline.set("distB", "0")
    inline.set("distL", "0")
    inline.set("distR", "0")
    extent = ET.SubElement(inline, qn("wp:extent"))
    extent.set("cx", str(cx))
    extent.set("cy", str(cy))
    effect = ET.SubElement(inline, qn("wp:effectExtent"))
    effect.set("l", "0")
    effect.set("t", "0")
    effect.set("r", "0")
    effect.set("b", "0")
    docpr = ET.SubElement(inline, qn("wp:docPr"))
    docpr.set("id", str(docpr_id))
    docpr.set("name", descr)
    docpr.set("descr", descr)
    cnv = ET.SubElement(inline, qn("wp:cNvGraphicFramePr"))
    ET.SubElement(cnv, qn("a:graphicFrameLocks")).set("noChangeAspect", "1")
    graphic = ET.SubElement(inline, qn("a:graphic"))
    graphic_data = ET.SubElement(graphic, qn("a:graphicData"))
    graphic_data.set("uri", NS["pic"])
    pic = ET.SubElement(graphic_data, qn("pic:pic"))
    nv = ET.SubElement(pic, qn("pic:nvPicPr"))
    cnvpr = ET.SubElement(nv, qn("pic:cNvPr"))
    cnvpr.set("id", str(docpr_id))
    cnvpr.set("name", descr)
    cnvpr.set("descr", descr)
    ET.SubElement(nv, qn("pic:cNvPicPr"))
    blip_fill = ET.SubElement(pic, qn("pic:blipFill"))
    blip = ET.SubElement(blip_fill, qn("a:blip"))
    blip.set(qn("r:embed"), rid)
    stretch = ET.SubElement(blip_fill, qn("a:stretch"))
    ET.SubElement(stretch, qn("a:fillRect"))
    sppr = ET.SubElement(pic, qn("pic:spPr"))
    xfrm = ET.SubElement(sppr, qn("a:xfrm"))
    off = ET.SubElement(xfrm, qn("a:off"))
    off.set("x", "0")
    off.set("y", "0")
    ext = ET.SubElement(xfrm, qn("a:ext"))
    ext.set("cx", str(cx))
    ext.set("cy", str(cy))
    geom = ET.SubElement(sppr, qn("a:prstGeom"))
    geom.set("prst", "rect")
    ET.SubElement(geom, qn("a:avLst"))
    ln = ET.SubElement(sppr, qn("a:ln"))
    ET.SubElement(ln, qn("a:noFill"))
    return p


def find_body_index(body: ET.Element, text: str) -> int:
    for i, child in enumerate(list(body)):
        if child.tag == qn("w:p") and p_text(child).strip() == text:
            return i
    raise ValueError(f"Paragraph not found: {text}")


def insert_after(body: ET.Element, target_text: str, nodes: list[ET.Element]) -> None:
    idx = find_body_index(body, target_text)
    for offset, node in enumerate(nodes, 1):
        body.insert(idx + offset, node)


def next_rids(rels_root: ET.Element, count: int) -> list[str]:
    max_id = 0
    for rel in rels_root:
        rid = rel.attrib.get("Id", "")
        if rid.startswith("rId") and rid[3:].isdigit():
            max_id = max(max_id, int(rid[3:]))
    return [f"rId{max_id + i}" for i in range(1, count + 1)]


def replace_chapter_six(body: ET.Element) -> None:
    children = list(body)
    start = find_body_index(body, "6 РАЗРАБОТКА ГРАФИЧЕСКОГО МАТЕРИАЛА")
    end = find_body_index(body, "7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ")
    normal_template = children[start + 1]
    sub_template = next(p for p in children if p.tag == qn("w:p") and p_text(p).strip().startswith("6.1 "))
    list_template = next(p for p in children if p.tag == qn("w:p") and p_text(p).strip().startswith("– схема алгоритма"))

    new_items = [
        make_para("Графический материал курсовой работы разработан для наглядного представления ключевых процессов и структуры программы ext4tool. В состав графической части включены материалы, соответствующие листу задания: схема алгоритма, схема функциональной структуры, диаграмма состояний и схема взаимодействия с пользователем и внешней средой.", normal_template),
        make_para("Каждый графический материал выполнен как самостоятельный плакат с рамкой и основной надписью. Такой состав позволяет показать не только отдельный алгоритм, но и общую архитектуру программного средства, режимы его работы и связи с внешними объектами.", normal_template),
        make_para("6.1 Состав графического материала", sub_template),
        make_para("В графическую часть курсовой работы включены следующие материалы:", normal_template),
        make_para("– схема алгоритма поиска inode по абсолютному пути;", list_template),
        make_para("– схема функциональной структуры программы;", list_template),
        make_para("– диаграмма состояний программы;", list_template),
        make_para("– схема взаимодействия с пользователем и внешней средой.", list_template),
        make_para("Выбор указанных материалов обусловлен тем, что они отражают основные стороны разработанного решения: алгоритмическую обработку пути, модульную организацию исходного кода, последовательность переходов между режимами и взаимодействие пользователя с ext4-образом.", normal_template),
        make_para("6.2 Схема алгоритма поиска inode по абсолютному пути", sub_template),
        make_para("Схема алгоритма поиска inode по абсолютному пути описывает преобразование пользовательского пути в номер inode. На ней показаны проверка корректности входного пути, переход к корневому inode, последовательная обработка компонент пути, поиск записи в текущем каталоге и возврат диагностического результата при ошибке. Схема алгоритма представлена в приложении А.", normal_template),
        make_para("6.3 Схема функциональной структуры программы", sub_template),
        make_para("Схема функциональной структуры отражает состав основных модулей ext4tool и связи между ними. На схеме выделены пользовательский интерфейс dashboard, модуль запуска main, подсистемы разбора superblock, inode и каталогов, слой ввода-вывода ext4_io, модуль безопасного редактирования metadata_editor и вспомогательный модуль util. Схема функциональной структуры представлена в приложении Б.", normal_template),
        make_para("6.4 Диаграмма состояний программы", sub_template),
        make_para("Диаграмма состояний показывает жизненный цикл программы от запуска до завершения работы. На ней отражены разбор аргументов, открытие образа, чтение superblock, переход в главное меню, просмотр метаданных, поиск, редактирование, обработка ошибок и постзаписная верификация. Диаграмма состояний представлена в приложении В.", normal_template),
        make_para("6.5 Схема взаимодействия с пользователем и внешней средой", sub_template),
        make_para("Схема взаимодействия с пользователем и внешней средой показывает, какие действия выполняет пользователь через ncurses-интерфейс и с какими внешними объектами работает программа. К таким объектам относятся терминал Linux, файл-образ ext4, резервная копия образа и средства сборки. Схема взаимодействия представлена в приложении Г.", normal_template),
        make_para("Таким образом, разработанный графический материал охватывает алгоритмическую, структурную и поведенческую стороны программы ext4tool и дополняет пояснительную записку визуальными моделями.", normal_template),
    ]

    for _ in range(end - start - 1):
        body.remove(list(body)[start + 1])
    for offset, item in enumerate(new_items, 1):
        body.insert(start + offset, item)


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    rels = ET.fromstring(entries["word/_rels/document.xml.rels"])
    body = document.find("w:body", NS)
    if body is None:
        raise RuntimeError("word/body not found")

    replacements = {
        "Минск 2026": "МИНСК 2026",
        "3.3 Требования к безопасности и надёжности": "3.3 Требования к безопасности и надёжности",
        " – Обеспечить открытие образа в режимах readonly и write с проверкой того, что источник является регулярным файлом.": "– обеспечить открытие образа в режимах readonly и write с проверкой того, что источник является регулярным файлом;",
        "– Обеспечить чтение и разбор superblock, дескрипторов групп и inode с учётом размера блоков и особенностей ext4.": "– обеспечить чтение и разбор superblock, дескрипторов групп и inode с учётом размера блоков и особенностей ext4;",
        "– Организовать просмотр каталогов и поиск объектов по абсолютному пути, номеру inode и имени.": "– организовать просмотр каталогов и поиск объектов по абсолютному пути, номеру inode и имени;",
        "– Реализовать безопасное редактирование ограниченного набора полей superblock и inode.": "– реализовать безопасное редактирование ограниченного набора полей superblock и inode;",
        "– Предусмотреть создание резервной копии образа перед каждой операцией записи.": "– предусмотреть создание резервной копии образа перед каждой операцией записи;",
        "– Выполнять повторное чтение и верификацию изменённых данных после записи.": "– выполнять повторное чтение и верификацию изменённых данных после записи;",
        "– Разработать удобный ncurses-интерфейс для последовательной работы со всеми функциями программы.": "– разработать удобный ncurses-интерфейс для последовательной работы со всеми функциями программы.",
        "– открытие образа в режимах только чтения и чтения-записи;": "– открытие образа в режимах только чтения и чтения-записи;",
        "– отображение результатов работы в текстовом интерфейсе на основе ncurses.": "– отображение результатов работы в текстовом интерфейсе на основе ncurses.",
        "– выполнение операций записи только при явном выборе режима --write;": "– выполнение операций записи только при явном выборе режима --write;",
        "– отклонение операций редактирования при отсутствии допустимых полей или при передаче некорректных параметров.": "– отклонение операций редактирования при отсутствии допустимых полей или при передаче некорректных параметров.",
        "6.3 Схема алгоритма рекурсивного поиска пути по inode\t21": "6.3 Схема функциональной структуры программы\t21",
        "6.4 Схема алгоритма безопасного редактирования метаданных\t21": "6.4 Диаграмма состояний программы\t21",
        "6.5 Диаграмма состояний программы\t22": "6.5 Схема взаимодействия с пользователем и внешней средой\t22",
        "Рисунок 7.3 – Пример просмотра методанных superblock": "Рисунок 7.3 – Пример просмотра метаданных superblock",
        "Схема алгоритма рекурсивного поиска пути по inode": "Схема функциональной структуры программы",
        "Схема алгоритма безопасного редактирования метаданных": "Диаграмма состояний программы",
        "[2] Робачевский А. М., Немнюгин С. А., Стесик О. Л. Операционная система UNIX. 2-е изд. СПб.: БХВ-Петербург, 2010.": "[2] Робачевский, А. М. Операционная система UNIX / А. М. Робачевский, С. А. Немнюгин, О. Л. Стесик. – 2-е изд. – СПб. : БХВ-Петербург, 2010.",
        "[3] Иванов Н. Н. Программирование в Linux. Самоучитель. 2-е изд. СПб.: БХВ-Петербург, 2015.": "[3] Иванов, Н. Н. Программирование в Linux. Самоучитель / Н. Н. Иванов. – 2-е изд. – СПб. : БХВ-Петербург, 2015.",
        "[4] Таненбаум Э., Бос Х. Современные операционные системы. 4-е изд. СПб.: Питер, 2022.": "[4] Таненбаум, Э. Современные операционные системы / Э. Таненбаум, Х. Бос. – 4-е изд. – СПб. : Питер, 2022.",
        "[5] Столлингс В. Операционные системы. Внутреннее устройство и принципы проектирования. 8-е изд. М.: Вильямс, 2019.": "[5] Столлингс, В. Операционные системы. Внутреннее устройство и принципы проектирования / В. Столлингс. – 8-е изд. – М. : Вильямс, 2019.",
    }

    for child in body.findall(".//w:p", NS):
        text = p_text(child)
        if text in replacements:
            set_p_text(child, replacements[text])

    for t in text_nodes(document):
        if not t.text:
            continue
        t.text = t.text.replace("—", "–").replace("методанных", "метаданных")
        t.text = re.sub(r"(\d) (?=(?:байт(?:а|ов)?|%|мм|см|пт|с\.|г\.))", lambda m: m.group(1) + "\u00A0", t.text)

    intro_phrase = (
        "Данный курсовой проект выполнен мной лично, проверен на заимствования, "
        "процент оригинальности составляет ХХ % (отчет о проверке на заимствования "
        "прилагается в приложении Е)."
    )
    if intro_phrase not in "\n".join(p_text(p) for p in body.findall(".//w:p", NS)):
        target = next(p_text(p) for p in body.findall(".//w:p", NS) if p_text(p).startswith("Цель работы заключается"))
        target_idx = find_body_index(body, target)
        body.insert(target_idx + 1, make_para(intro_phrase, list(body)[target_idx]))

    if not any(p_text(p).startswith("Таблица 3.1") for p in body.findall(".//w:p", NS)):
        p32_end = "Дополнительно интерфейс должен отображать текущий режим работы, ключевые параметры образа и результаты выполненных операций. При выполнении записи пользователю необходимо получать информацию о создании резервной копии и успешности последующей проверки изменений, что делает поведение программы более прозрачным и удобным для использования в учебных целях."
        insert_after(body, p32_end, [
            make_para("Сводная характеристика основных функциональных возможностей приведена в таблице 3.1."),
            make_para("Таблица 3.1 – Функциональные возможности программы"),
            make_table(
                ["Группа функций", "Операции", "Модули реализации"],
                [
                    ["Первичная обработка", "открытие образа, проверка режима доступа, чтение superblock", "main, ext4_io, ext4_super"],
                    ["Просмотр метаданных", "вывод superblock, group descriptor, inode и каталогов", "ext4_super, ext4_inode, ext4_dir"],
                    ["Навигация и поиск", "поиск по абсолютному пути, имени и номеру inode", "ext4_dir"],
                    ["Редактирование", "изменение разрешённых полей, backup, верификация", "metadata_editor, ext4_io"],
                ],
                [2600, 4300, 2500],
            ),
        ])

    if not any(p_text(p).startswith("Таблица 3.2") for p in body.findall(".//w:p", NS)):
        p33_end = "Соблюдение указанных требований особенно критично при работе с такими структурами, как superblock и inode, поскольку ошибки в них могут привести к нарушению интерпретации всей файловой системы. В связи с этим программа должна не только выполнять операции записи, но и предварительно оценивать их допустимость, а при выявлении потенциально опасных условий автоматически переходить к консервативному режиму работы, исключающему изменение данных."
        insert_after(body, p33_end, [
            make_para("Основные защитные механизмы режима записи приведены в таблице 3.2."),
            make_para("Таблица 3.2 – Защитные механизмы режима записи"),
            make_table(
                ["Механизм", "Назначение", "Результат"],
                [
                    ["Явный режим --write", "исключение случайной записи при обычном запуске", "запись блокируется в readonly"],
                    ["Контроль границ", "проверка смещений и размера образа перед доступом", "ошибочный запрос отклоняется"],
                    ["Резервное копирование", "сохранение исходного состояния перед изменением", "доступен ручной откат"],
                    ["Постзаписная проверка", "повторное чтение изменённых структур", "подтверждается факт записи"],
                ],
                [2600, 4300, 2500],
            ),
        ])

    replace_chapter_six(body)

    # Update appendix titles after chapter replacement.
    for p in body.findall(".//w:p", NS):
        txt = p_text(p).strip()
        if txt == "Схема функциональной структуры программы":
            set_p_text(p, "Схема функциональной структуры программы")
        elif txt == "Диаграмма состояний программы":
            set_p_text(p, "Диаграмма состояний программы")

    if not any(p_text(p).strip().startswith("Приложение Е") for p in body.findall(".//w:p", NS)):
        try:
            g_idx = find_body_index(body, "Приложение Г\t34")
            body.insert(g_idx + 1, make_para("Приложение Е\t35", list(body)[g_idx]))
        except ValueError:
            pass

    # Add a placeholder appendix for the originality report referenced in the introduction.
    if not any(p_text(p).strip() == "ПРИЛОЖЕНИЕ Е" for p in body.findall(".//w:p", NS)):
        sectpr = list(body)[-1]
        insert_pos = len(body) - 1 if sectpr.tag == qn("w:sectPr") else len(body)
        template = next(p for p in body.findall(".//w:p", NS) if p_text(p).strip() == "ПРИЛОЖЕНИЕ Г")
        body.insert(insert_pos, make_para("", page_break=True))
        body.insert(insert_pos + 1, make_para("ПРИЛОЖЕНИЕ Е", template, bold=True, align="center"))
        body.insert(insert_pos + 2, make_para("(обязательное)", template, align="center"))
        body.insert(insert_pos + 3, make_para("Отчет о проверке на заимствования", template, align="center"))

    # Add diagram media relationships and insert images into appendices.
    rel_ids = next_rids(rels, len(DIAGRAMS))
    image_targets = {}
    for (filename, descr), rid in zip(DIAGRAMS, rel_ids):
        target = f"media/{filename}"
        image_targets[descr] = (rid, target)
        rel = ET.SubElement(rels, qn("rel:Relationship"))
        rel.set("Id", rid)
        rel.set("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image")
        rel.set("Target", target)

    appendix_titles = [
        "Схема алгоритма поиска inode по абсолютному пути",
        "Схема функциональной структуры программы",
        "Диаграмма состояний программы",
        "Схема взаимодействия с пользователем и внешней средой",
    ]
    docpr = 30_001
    for title in appendix_titles:
        if title in image_targets:
            rid, _ = image_targets[title]
            idx = find_body_index(body, title)
            body.insert(idx + 1, make_image_para(rid, title, docpr))
            docpr += 1

    # Re-read after paragraph-level edits and normalize all new text too.
    for t in text_nodes(document):
        if not t.text:
            continue
        t.text = t.text.replace("—", "–").replace("методанных", "метаданных")
        t.text = re.sub(r"(\d) (?=(?:байт(?:а|ов)?|%|мм|см|пт|с\.|г\.))", lambda m: m.group(1) + "\u00A0", t.text)

    entries["word/document.xml"] = ET.tostring(document, encoding="utf-8", xml_declaration=True)
    entries["word/_rels/document.xml.rels"] = ET.tostring(rels, encoding="utf-8", xml_declaration=True)
    for filename, _descr in DIAGRAMS:
        entries[f"word/media/{filename}"] = (ROOT / "generated-schemes" / filename).read_bytes()

    tmp = DOCX.with_suffix(".tmp.docx")
    with ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for name, data in entries.items():
            zout.writestr(name, data)
    tmp.replace(DOCX)


if __name__ == "__main__":
    main()
