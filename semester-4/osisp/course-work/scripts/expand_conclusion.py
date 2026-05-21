from __future__ import annotations

import copy
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-conclusion-expand.docx"

NS = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
for prefix, uri in NS.items():
    ET.register_namespace(prefix, uri)


def qn(name: str) -> str:
    prefix, tag = name.split(":")
    return f"{{{NS[prefix]}}}{tag}"


def p_text(p: ET.Element) -> str:
    return "".join(t.text or "" for t in p.findall(".//w:t", NS))


def body_children(body: ET.Element) -> list[ET.Element]:
    return list(body)


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


def make_para(text: str, template: ET.Element) -> ET.Element:
    p = ET.Element(qn("w:p"))
    ppr = template.find("w:pPr", NS)
    if ppr is not None:
        p.append(copy.deepcopy(ppr))
    rpr = first_run_props(template)
    run = ET.SubElement(p, qn("w:r"))
    if rpr is not None:
        set_black(rpr)
        run.append(rpr)
    t = ET.SubElement(run, qn("w:t"))
    t.text = text
    return p


def make_blank(template: ET.Element) -> ET.Element:
    p = ET.Element(qn("w:p"))
    ppr = template.find("w:pPr", NS)
    if ppr is not None:
        p.append(copy.deepcopy(ppr))
    return p


CONCLUSION = [
    "В результате выполнения курсовой работы разработан программный инструмент ext4tool, предназначенный для анализа ext4-образов и безопасного редактирования отдельных метаданных файловой системы. Цель работы достигнута: программа обеспечивает чтение основных служебных структур ext4, поддерживает просмотр superblock, резервных superblock, group descriptor, inode и записей каталогов, а также предоставляет средства навигации и поиска объектов внутри файлового образа.",
    "В ходе разработки были изучены особенности внутренней организации ext4, включая расположение superblock, зависимость вычисления смещений от размера блока, роль таблиц inode, структуру записей каталогов и использование extent-механизма. Полученные сведения были применены при проектировании модулей чтения, интерпретации и отображения метаданных. Это позволило реализовать не простой просмотр бинарных данных, а инструмент, который выводит пользователю уже обработанную и понятную информацию о состоянии файловой системы.",
    "Системная часть программы построена с учётом требований дисциплины «Операционные системы и системное программирование». В проекте используются язык C, POSIX-интерфейсы и низкоуровневые операции с файлом-образом. Работа с данными выполняется через позиционное чтение и запись, что позволяет обращаться к нужным областям образа без загрузки файла целиком. Такой подход соответствует характеру системной утилиты и демонстрирует практическое применение механизмов операционной системы.",
    "Особое внимание в проекте уделено безопасности операций записи. Разделение режимов readonly и write, явное включение режима редактирования, проверка совместимости ext4, контроль допустимости изменяемых полей, автоматическое создание резервной копии и постзаписная верификация снижают риск повреждения образа. Благодаря этому программа показывает не только возможность изменения метаданных, но и обязательный контроль корректности результата после выполнения записи.",
    "Функциональная часть программы объединяет несколько практических сценариев работы с ext4-образом. Пользователь может просматривать параметры файловой системы, анализировать inode, выполнять листинг каталогов, искать объекты по абсолютному пути, имени или номеру inode, получать статистическую сводку и переходить к редактированию разрешённых полей. Наличие единого ncurses-интерфейса делает эти операции последовательными и удобными для выполнения в терминальной среде.",
    "Практическая ценность проекта заключается в сочетании наглядности, воспроизводимости и защитных механизмов. Программа может использоваться как учебное средство при изучении файловой системы ext4, поскольку позволяет наблюдать связь между superblock, group descriptor, inode и каталоговыми записями. Одновременно разработанный инструмент демонстрирует инженерный подход к работе с потенциально опасными операциями: изменение выполняется только после проверок и сопровождается резервным копированием.",
    "В рамках курсового проекта также подготовлен графический материал, отражающий основные стороны разработанного решения. Схема алгоритма безопасного редактирования показывает ключевой сценарий изменения метаданных, схема функциональной структуры раскрывает состав модулей, диаграмма классов описывает основные структуры данных, а схема взаимодействия демонстрирует связь программы с пользователем, терминалом, ext4-образом и резервной копией. Эти материалы дополняют пояснительную записку и делают архитектуру проекта более наглядной.",
    "Дальнейшее развитие программы может включать расширение набора редактируемых полей, более подробную диагностику повреждённых структур, журналирование действий пользователя, дополнительные проверки целостности и поддержку большего числа вариантов конфигурации ext4. Также перспективным направлением является расширение статистических экранов и добавление режимов сравнения состояния образа до и после редактирования.",
    "Таким образом, текущая версия ext4tool решает поставленную задачу и может рассматриваться как завершённое прикладное средство в рамках курсового проекта. Разработанная программа демонстрирует работу с бинарным образом файловой системы, применение POSIX-механизмов, модульную организацию системного кода и безопасный подход к изменению критически важных метаданных.",
]


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    body = document.find("w:body", NS)
    if body is None:
        raise RuntimeError("word/body not found")

    children = body_children(body)
    start = next(i for i, child in enumerate(children) if child.tag == qn("w:p") and p_text(child).strip() == "ЗАКЛЮЧЕНИЕ")
    end = next(i for i in range(start + 1, len(children)) if children[i].tag == qn("w:p") and p_text(children[i]).strip() == "СПИСОК ИСПОЛЬЗОВАННЫХ ИСТОЧНИКОВ")

    normal_template = next(
        child for child in children[start + 1 : end]
        if child.tag == qn("w:p") and p_text(child).strip()
    )

    for _ in range(end - start - 1):
        body.remove(body_children(body)[start + 1])

    new_nodes = [make_blank(normal_template), *[make_para(text, normal_template) for text in CONCLUSION], make_blank(normal_template)]
    for offset, node in enumerate(new_nodes, 1):
        body.insert(start + offset, node)

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
