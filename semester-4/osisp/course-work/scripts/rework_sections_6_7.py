from __future__ import annotations

import copy
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-sections-6-7-rework.docx"
SCHEMES = ROOT / "generated-schemes"

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


def all_paragraphs(root: ET.Element) -> list[ET.Element]:
    return root.findall(".//w:p", NS)


def body_children(body: ET.Element) -> list[ET.Element]:
    return list(body)


def first_run_props(p: ET.Element) -> ET.Element | None:
    run = p.find("w:r", NS)
    if run is None:
        return None
    rpr = run.find("w:rPr", NS)
    return copy.deepcopy(rpr) if rpr is not None else None


def set_black_color(rpr: ET.Element) -> None:
    color = rpr.find("w:color", NS)
    if color is None:
        color = ET.SubElement(rpr, qn("w:color"))
    color.set(qn("w:val"), "000000")
    for attr in ("themeColor", "themeTint", "themeShade"):
        color.attrib.pop(qn(f"w:{attr}"), None)


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
    set_black_color(rpr)
    t = ET.SubElement(run, qn("w:t"))
    if text[:1].isspace() or text[-1:].isspace():
        t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    t.text = text
    return run


def make_para(text: str, template: ET.Element | None = None, *, bold: bool = False, align: str | None = None) -> ET.Element:
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
    if text:
        p.append(make_run(text, bold=bold))
    return p


def set_p_text(p: ET.Element, text: str) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    rpr = first_run_props(p)
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    run = ET.SubElement(p, qn("w:r"))
    if rpr is not None:
        set_black_color(rpr)
        run.append(rpr)
    t = ET.SubElement(run, qn("w:t"))
    t.text = text


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


def find_body_index(body: ET.Element, text: str) -> int:
    for i, child in enumerate(body_children(body)):
        if child.tag == qn("w:p") and p_text(child).strip() == text:
            return i
    raise ValueError(f"Body paragraph not found: {text}")


def find_body_index_prefix(body: ET.Element, prefix: str) -> int:
    for i, child in enumerate(body_children(body)):
        if child.tag == qn("w:p") and p_text(child).strip().startswith(prefix):
            return i
    raise ValueError(f"Body paragraph prefix not found: {prefix}")


def replace_between(body: ET.Element, start_text: str, end_text: str, nodes: list[ET.Element]) -> None:
    start = find_body_index(body, start_text)
    end = find_body_index(body, end_text)
    for _ in range(end - start - 1):
        body.remove(body_children(body)[start + 1])
    for offset, node in enumerate(nodes, 1):
        body.insert(start + offset, node)


def figure_pair(body: ET.Element, caption: str) -> list[ET.Element]:
    idx = find_body_index(body, caption)
    children = body_children(body)
    image_idx = idx - 1
    while image_idx >= 0 and p_text(children[image_idx]).strip():
        image_idx -= 1
    # In the current document the image paragraph is immediately before the caption.
    image_idx = idx - 1
    return [copy.deepcopy(children[image_idx]), copy.deepcopy(children[idx])]


def make_section_6(sub: ET.Element, normal: ET.Element) -> list[ET.Element]:
    return [
        make_para(
            "Графический материал курсовой работы разработан для наглядного представления алгоритма безопасного редактирования метаданных, структуры программы ext4tool, основных проектных сущностей и взаимодействия пользователя с внешней средой.",
            normal,
        ),
        make_para(
            "В качестве основного алгоритма выбран сценарий безопасного редактирования метаданных. Он является более показательным для данного проекта, чем поиск inode по абсолютному пути, поскольку объединяет проверку режима write, валидацию входных данных, создание резервной копии, запись и постзаписную верификацию.",
            normal,
        ),
        make_para("6.1 Схема алгоритма", sub),
        make_para(
            "Схема алгоритма безопасного редактирования метаданных описывает последовательность действий от получения пользовательского запроса до подтверждения результата. На ней показаны отказ от записи в режиме readonly, проверка совместимости ext4, контроль допустимости изменяемого поля, создание backup-файла, выбор объекта superblock или inode, выполнение записи и повторное чтение изменённой структуры. Схема алгоритма представлена в приложении А.",
            normal,
        ),
        make_para("6.2 Схема функциональной структуры программы", sub),
        make_para(
            "Схема функциональной структуры отражает состав основных модулей ext4tool и связи между ними. На схеме выделены пользовательский интерфейс dashboard, модуль запуска main, подсистемы разбора superblock, inode и каталогов, слой ввода-вывода ext4_io, модуль безопасного редактирования metadata_editor и вспомогательный модуль util. Схема функциональной структуры представлена в приложении Б.",
            normal,
        ),
        make_para("6.3 Диаграмма классов", sub),
        make_para(
            "Диаграмма классов используется как проектная модель для C-программы и показывает основные структуры данных, выполняющие роль классов в архитектурном описании. На диаграмме представлены Ext4Context, Ext4SuperView, Ext4GroupDescView, Ext4InodeView, DirectoryEntry, MetadataEditRequest, Dashboard и Util, а также связи между ними при просмотре, поиске и редактировании метаданных. Диаграмма классов представлена в приложении В.",
            normal,
        ),
        make_para("6.4 Схема взаимодействия с пользователем и внешней средой", sub),
        make_para(
            "Схема взаимодействия с пользователем и внешней средой показывает, какие действия выполняет пользователь через ncurses-интерфейс и с какими внешними объектами работает программа. К таким объектам относятся терминал Linux, файл-образ ext4, резервная копия образа и средства сборки. Схема взаимодействия представлена в приложении Г.",
            normal,
        ),
        make_para(
            "Таким образом, разработанный графический материал охватывает алгоритмическую, структурную и пользовательскую стороны программы ext4tool и дополняет пояснительную записку визуальными моделями.",
            normal,
        ),
    ]


def make_section_7(body: ET.Element, sub: ET.Element, normal: ET.Element) -> list[ET.Element]:
    fig71 = figure_pair(body, "Рисунок 7.1 – Пример запуска программы")
    fig72 = figure_pair(body, "Рисунок 7.2 – Демонстрация главного меню программы")
    fig73 = figure_pair(body, "Рисунок 7.3 – Пример просмотра метаданных superblock")
    fig74 = figure_pair(body, "Рисунок 7.4 – Пример поиска inode по абсолютному пути")
    fig75 = figure_pair(body, "Рисунок 7.5 – Демонстрация меню редактирования inode")

    return [
        make_para("7.1 Сборка программы", sub),
        make_para(
            "Для сборки проекта необходимо перейти в каталог editor-for-ext4-filesystem и выполнить команду make. В результате создаётся исполняемый файл build/ext4tool. Для удаления собранных объектных файлов и исполняемого файла используется команда make clean.",
            normal,
        ),
        make_para(
            "Программа ориентирована на POSIX-совместимую Linux-среду и не требует графической оболочки. Основные зависимости связаны с компиляцией C-кода, работой ncurses-интерфейса и доступом к тестовому ext4-образу.",
            normal,
        ),
        make_para("Минимальные программные зависимости приведены в таблице 7.1.", normal),
        make_para("Таблица 7.1 – Минимальные программные зависимости", normal),
        make_table(
            ["Компонент", "Назначение"],
            [
                ["GCC", "Компиляция исходного кода на языке C."],
                ["make", "Автоматизация сборки и очистки проекта."],
                ["ncurses", "Формирование полноэкранного терминального интерфейса."],
                ["POSIX-совместимая Linux-среда", "Доступ к системным вызовам открытия, чтения, записи и синхронизации."],
                ["ext4-образ", "Тестовый файл-образ файловой системы для просмотра и редактирования."],
            ],
            [3300, 6200],
        ),
        make_para("7.2 Формат запуска", sub),
        make_para(
            "Общий формат запуска программы имеет вид: ./build/ext4tool --image <path> [--readonly|--write] [--lang en|ru]. Параметр --image является обязательным, а режим readonly используется по умолчанию, если пользователь явно не указал --write.",
            normal,
        ),
        make_para(
            "Режим записи следует использовать только для работы с копией ext4-образа. Даже при запуске с параметром --write программа выполняет дополнительные проверки и создаёт резервную копию перед изменением метаданных.",
            normal,
        ),
        make_para("Назначение основных параметров командной строки приведено в таблице 7.2.", normal),
        make_para("Таблица 7.2 – Параметры командной строки", normal),
        make_table(
            ["Параметр", "Назначение"],
            [
                ["--image <path>", "Путь к ext4-образу; параметр обязателен."],
                ["--readonly", "Запуск в режиме только чтения; используется по умолчанию."],
                ["--write", "Явное разрешение операций записи и безопасного редактирования."],
                ["--lang en|ru", "Выбор языка интерфейса."],
                ["-h, --help", "Вывод справки по использованию программы."],
            ],
            [3300, 6200],
        ),
        make_para("Пример запуска программы ext4tool из терминала представлен на рисунке 7.1.", normal),
        *fig71,
        make_para("7.3 Основы работы с интерфейсом", sub),
        make_para(
            "После запуска программа открывает указанный образ, считывает основной superblock и выполняет проверку поддерживаемых признаков файловой системы. Если основной superblock недоступен или повреждён, программа пытается использовать корректную резервную копию.",
            normal,
        ),
        make_para(
            "Основной интерфейс программы реализован на базе библиотеки ncurses и представлен в виде текстового меню. Пользователь перемещается по пунктам меню с помощью клавиш Up и Down, подтверждает выбор клавишей Enter, а возврат или отмена выполняются клавишей Esc. Для выхода используется клавиша q.",
            normal,
        ),
        make_para(
            "Главное меню объединяет просмотр superblock, резервных superblock, group descriptor и inode, листинг каталогов, поиск объектов, безопасное редактирование, статистические экраны, справку и переключение языка интерфейса.",
            normal,
        ),
        make_para("Внешний вид главного меню ext4tool после успешного открытия образа представлен на рисунке 7.2.", normal),
        *fig72,
        make_para("7.4 Просмотр и анализ метаданных", sub),
        make_para(
            "Для просмотра основных параметров файловой системы пользователь выбирает пункт View Superblock. В результате программа отображает ключевые поля superblock, включая размер блока, размер inode, число блоков и inode, имя тома, счётчики монтирования и наборы feature-флагов.",
            normal,
        ),
        make_para(
            "При выборе пункта Backup Superblocks выполняется поиск и отображение резервных superblock. Пункт View Group Descriptor позволяет просмотреть сведения о конкретной группе блоков, а пункт View Inode выводит подробные сведения о выбранном inode.",
            normal,
        ),
        make_para("Пример отображения метаданных superblock приведён на рисунке 7.3.", normal),
        *fig73,
        make_para("7.5 Навигация по дереву каталогов и поиск объектов", sub),
        make_para(
            "Для работы с содержимым образа программа предоставляет функции листинга каталогов и поиска объектов. Пункт List Directory позволяет вывести содержимое каталога как по абсолютному пути, так и по записи вида inode:N.",
            normal,
        ),
        make_para(
            "Пункт Find By Path используется для определения номера inode по абсолютному пути. Пункт Find By Inode выполняет обратную задачу: по известному номеру inode восстанавливает путь к объекту. Поиск по имени позволяет найти первый объект с заданным именем в дереве каталогов.",
            normal,
        ),
        make_para("Пример выполнения поиска inode по абсолютному пути представлен на рисунке 7.4.", normal),
        *fig74,
        make_para("7.6 Редактирование метаданных", sub),
        make_para(
            "Редактирование метаданных доступно только при запуске программы с параметром --write. Если программа работает в режиме только чтения, попытка перехода к редактированию завершается сообщением о запрете записи.",
            normal,
        ),
        make_para(
            "Для изменения параметров superblock используется пункт Edit Superblock. В текущей реализации допускается редактирование volume name, mount count, max mount count и check interval. Для изменения параметров inode используется пункт Edit Inode, поддерживающий изменение mode, uid, gid, atime, ctime, mtime и flags.",
            normal,
        ),
        make_para(
            "Перед каждой операцией редактирования автоматически создаётся резервная копия исходного образа. После записи программа повторно считывает изменённую структуру и сравнивает фактически полученные значения с ожидаемыми.",
            normal,
        ),
        make_para("Пример выполнения операции редактирования метаданных представлен на рисунке 7.5.", normal),
        *fig75,
        make_para("7.7 Завершение работы и рекомендации по безопасному использованию", sub),
        make_para(
            "Завершение работы с программой выполняется через пункт Exit или клавишу выхода из главного меню. После завершения сеанса открытый образ корректно закрывается, а пользователь возвращается в терминальную оболочку.",
            normal,
        ),
        make_para(
            "При практическом использовании программы рекомендуется применять копии ext4-образов, а не единственные экземпляры данных. Перед запуском в режиме записи необходимо убедиться, что выбран именно тестовый файл. При появлении диагностических сообщений следует устранить причину ошибки и только затем повторять операцию.",
            normal,
        ),
        make_para("Типовые ошибки и рекомендации по их устранению приведены в таблице 7.3.", normal),
        make_para("Таблица 7.3 – Типовые ошибки и способы их устранения", normal),
        make_table(
            ["Сообщение/ситуация", "Возможная причина", "Рекомендация"],
            [
                ["--image is required", "Не указан путь к файлу-образу.", "Повторить запуск с параметром --image <path>."],
                ["open error", "Файл отсутствует, недоступен или не является корректным источником.", "Проверить путь, права доступа и наличие ext4-образа."],
                ["superblock error", "Основной и резервные superblock не удалось распознать.", "Проверить, что выбранный файл действительно содержит ext4-образ."],
                ["write disabled", "Программа запущена без параметра --write.", "Для изменения метаданных явно выбрать режим записи."],
                ["inode not found", "Указан несуществующий номер inode или некорректный путь.", "Проверить значение inode, путь и результат листинга каталога."],
                ["verification failed", "Повторно считанное значение не совпало с ожидаемым.", "Не продолжать редактирование и использовать backup-файл для анализа состояния образа."],
            ],
            [3300, 3300, 3600],
        ),
        make_para(
            "Таким образом, программа ext4tool предоставляет пользователю единый интерфейс для просмотра, анализа, навигации и безопасного редактирования ext4-образов.",
            normal,
        ),
    ]


def update_toc(document: ET.Element) -> None:
    entries = {
        "6 Разработка графического материала": "6 Разработка графического материала\t20",
        "6.1": "6.1 Схема алгоритма\t20",
        "6.2": "6.2 Схема функциональной структуры программы\t21",
        "6.3": "6.3 Диаграмма классов\t21",
        "6.4": "6.4 Схема взаимодействия с пользователем и внешней средой\t22",
        "7 Руководство пользователя": "7 Руководство пользователя\t23",
        "7.1": "7.1 Сборка программы\t23",
        "7.2": "7.2 Формат запуска\t23",
        "7.3": "7.3 Основы работы с интерфейсом\t24",
        "7.4": "7.4 Просмотр и анализ метаданных\t25",
        "7.5": "7.5 Навигация по дереву каталогов и поиск объектов\t26",
        "7.6": "7.6 Редактирование метаданных\t27",
        "7.7": "7.7 Завершение работы и рекомендации по безопасному использованию\t28",
    }

    paragraphs = all_paragraphs(document)
    start = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "СОДЕРЖАНИЕ")
    end = next(i for i in range(start + 1, len(paragraphs)) if p_text(paragraphs[i]).strip() == "ВВЕДЕНИЕ")
    for p in paragraphs[start + 1 : end]:
        text = p_text(p).strip().replace("\u00a0", " ")
        new_text = None
        for key, value in entries.items():
            if text.startswith(key):
                new_text = value
                break
        if new_text:
            replace_p_with_toc_text(p, new_text)

    parents = {child: parent for parent in document.iter() for child in parent}

    # Remove the old 6.5 line if it remains after renumbering.
    for p in list(paragraphs[start + 1 : end]):
        if p_text(p).strip().startswith("6.5 "):
            parent = parents.get(p)
            if parent is not None:
                parent.remove(p)

    # Clean up an empty paragraph left between 6.4 and 7 after older rewrites.
    paragraphs = all_paragraphs(document)
    start = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "СОДЕРЖАНИЕ")
    end = next(i for i in range(start + 1, len(paragraphs)) if p_text(paragraphs[i]).strip() == "ВВЕДЕНИЕ")
    parents = {child: parent for parent in document.iter() for child in parent}
    for i in range(start + 1, end - 1):
        if (
            not p_text(paragraphs[i]).strip()
            and p_text(paragraphs[i - 1]).strip().startswith("6.4 ")
            and p_text(paragraphs[i + 1]).strip().startswith("7 ")
        ):
            parent = parents.get(paragraphs[i])
            if parent is not None:
                parent.remove(paragraphs[i])
            break


def replace_p_with_toc_text(p: ET.Element, text: str) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    if not text:
        return
    title, page = text.rsplit("\t", 1)
    run = make_run(title)
    run.append(ET.Element(qn("w:tab")))
    t = ET.SubElement(run, qn("w:t"))
    t.text = page
    p.append(run)


def update_appendix_titles(body: ET.Element) -> None:
    replacements = {
        "Схема алгоритма поиска inode по абсолютному пути": "Схема алгоритма безопасного редактирования метаданных",
        "Диаграмма состояний программы": "Диаграмма классов программы",
    }
    for child in body.findall(".//w:p", NS):
        text = p_text(child).strip()
        if text in replacements:
            set_p_text(child, replacements[text])


def force_black(root: ET.Element) -> None:
    for rpr in root.findall(".//w:rPr", NS):
        set_black_color(rpr)


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    body = document.find("w:body", NS)
    if body is None:
        raise RuntimeError("word/body not found")

    sub_template = body_children(body)[find_body_index_prefix(body, "6.3 ")]
    normal_template = body_children(body)[find_body_index_prefix(body, "Графический материал курсовой работы")]

    replace_between(body, "6 РАЗРАБОТКА ГРАФИЧЕСКОГО МАТЕРИАЛА", "7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ", make_section_6(sub_template, normal_template))
    replace_between(body, "7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ", "ЗАКЛЮЧЕНИЕ", make_section_7(body, sub_template, normal_template))

    update_toc(document)
    update_appendix_titles(body)
    force_black(document)

    entries["word/document.xml"] = ET.tostring(document, encoding="utf-8", xml_declaration=True)
    entries["word/media/course_diagram_algorithm.png"] = (SCHEMES / "course_diagram_algorithm.png").read_bytes()
    entries["word/media/course_diagram_state.png"] = (SCHEMES / "course_diagram_state.png").read_bytes()

    tmp = DOCX.with_suffix(".tmp.docx")
    with ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for name, data in entries.items():
            zout.writestr(name, data)
    tmp.replace(DOCX)


if __name__ == "__main__":
    main()
