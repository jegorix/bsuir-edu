from __future__ import annotations

import copy
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-menu-name-fix.docx"

NS = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
for prefix, uri in NS.items():
    ET.register_namespace(prefix, uri)


def qn(name: str) -> str:
    prefix, tag = name.split(":")
    return f"{{{NS[prefix]}}}{tag}"


def p_text(p: ET.Element) -> str:
    return "".join(t.text or "" for t in p.findall(".//w:t", NS))


def first_run_props(p: ET.Element) -> ET.Element | None:
    run = p.find("w:r", NS)
    if run is None:
        return None
    rpr = run.find("w:rPr", NS)
    return copy.deepcopy(rpr) if rpr is not None else None


def set_black(rpr: ET.Element) -> None:
    color = rpr.find("w:color", NS)
    if color is None:
        color = ET.SubElement(rpr, qn("w:color"))
    color.set(qn("w:val"), "000000")
    for attr in ("themeColor", "themeTint", "themeShade"):
        color.attrib.pop(qn(f"w:{attr}"), None)


def set_p_text(p: ET.Element, text: str) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    rpr = first_run_props(p)
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    run = ET.SubElement(p, qn("w:r"))
    if rpr is not None:
        set_black(rpr)
        run.append(rpr)
    t = ET.SubElement(run, qn("w:t"))
    t.text = text


REPLACEMENTS = {
    "Для просмотра основных параметров файловой системы пользователь выбирает пункт View Superblock. В результате программа отображает ключевые поля superblock, включая размер блока, размер inode, число блоков и inode, имя тома, счётчики монтирования и наборы feature-флагов.":
        "Для просмотра основных параметров файловой системы пользователь выбирает пункт Superblock View. В результате программа отображает ключевые поля superblock, включая размер блока, размер inode, число блоков и inode, имя тома, счётчики монтирования и наборы feature-флагов.",
    "При выборе пункта Backup Superblocks выполняется поиск и отображение резервных superblock. Эта возможность полезна при диагностике образов, в которых начальная область повреждена или вызывает сомнение. Пункт View Group Descriptor позволяет просмотреть сведения о конкретной группе блоков, включая расположение таблицы inode и счётчики свободных объектов.":
        "При выборе пункта Backup Superblocks выполняется поиск и отображение резервных superblock. Эта возможность полезна при диагностике образов, в которых начальная область повреждена или вызывает сомнение. Пункт Group Descriptor позволяет просмотреть сведения о конкретной группе блоков, включая расположение таблицы inode и счётчики свободных объектов.",
    "Для более детального анализа пользователь может выбрать пункт View Inode и указать номер inode. В результате программа показывает режим доступа, идентификаторы владельца, размер, временные метки, флаги и признаки, связанные с типом объекта.":
        "Для более детального анализа пользователь может выбрать пункт Inode View и указать номер inode. В результате программа показывает режим доступа, идентификаторы владельца, размер, временные метки, флаги и признаки, связанные с типом объекта.",
    "Для работы с содержимым образа программа предоставляет функции листинга каталогов и поиска объектов. Пункт List Directory позволяет вывести содержимое каталога как по абсолютному пути, так и по записи вида inode:N.":
        "Для работы с содержимым образа программа предоставляет функции листинга каталогов и поиска объектов. Пункт Directory Browser позволяет вывести содержимое каталога как по абсолютному пути, так и по записи вида inode:N.",
    "Пункт Find By Path используется для определения номера inode по абсолютному пути. Пользователь вводит путь к объекту, после чего программа последовательно проходит по дереву каталогов и возвращает найденный inode. Пункт Find By Inode выполняет обратную задачу: по известному номеру inode восстанавливает путь к объекту.":
        "Пункт Resolve Path используется для определения номера inode по абсолютному пути. Пользователь вводит путь к объекту, после чего программа последовательно проходит по дереву каталогов и возвращает найденный inode. Пункт Resolve Inode выполняет обратную задачу: по известному номеру inode восстанавливает путь к объекту.",
}


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    for p in document.findall(".//w:p", NS):
        text = p_text(p).strip()
        if text in REPLACEMENTS:
            set_p_text(p, REPLACEMENTS[text])

    for color in document.findall(".//w:color", NS):
        color.set(qn("w:val"), "000000")
        for attr in ("themeColor", "themeTint", "themeShade"):
            color.attrib.pop(qn(f"w:{attr}"), None)

    entries["word/document.xml"] = ET.tostring(document, encoding="utf-8", xml_declaration=True)
    tmp = DOCX.with_suffix(".tmp.docx")
    with ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for name, data in entries.items():
            zout.writestr(name, data)
    tmp.replace(DOCX)


if __name__ == "__main__":
    main()
