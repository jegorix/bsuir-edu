from __future__ import annotations

import copy
import re
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DOCX = ROOT / "course-osisp-nowicki-final.docx"
BACKUP = ROOT / "course-osisp-nowicki-final.before-sections-6-7-polish.docx"

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


def add_fonts(rpr: ET.Element) -> None:
    fonts = ET.SubElement(rpr, qn("w:rFonts"))
    fonts.set(qn("w:ascii"), "Times New Roman")
    fonts.set(qn("w:hAnsi"), "Times New Roman")
    fonts.set(qn("w:cs"), "Times New Roman")


def add_size(rpr: ET.Element, size: int) -> None:
    sz = ET.SubElement(rpr, qn("w:sz"))
    sz.set(qn("w:val"), str(size))
    szcs = ET.SubElement(rpr, qn("w:szCs"))
    szcs.set(qn("w:val"), str(size))


def add_black(rpr: ET.Element) -> None:
    color = ET.SubElement(rpr, qn("w:color"))
    color.set(qn("w:val"), "000000")


def add_lang(rpr: ET.Element) -> None:
    lang = ET.SubElement(rpr, qn("w:lang"))
    lang.set(qn("w:val"), "ru-RU")


def run_props(*, bold: bool = False, size: int = 28) -> ET.Element:
    rpr = ET.Element(qn("w:rPr"))
    add_fonts(rpr)
    if bold:
        ET.SubElement(rpr, qn("w:b"))
        ET.SubElement(rpr, qn("w:bCs"))
    add_black(rpr)
    add_size(rpr, size)
    add_lang(rpr)
    return rpr


def ppr_normal() -> ET.Element:
    ppr = ET.Element(qn("w:pPr"))
    spacing = ET.SubElement(ppr, qn("w:spacing"))
    spacing.set(qn("w:after"), "0")
    spacing.set(qn("w:line"), "240")
    spacing.set(qn("w:lineRule"), "auto")
    ind = ET.SubElement(ppr, qn("w:ind"))
    ind.set(qn("w:firstLine"), "709")
    jc = ET.SubElement(ppr, qn("w:jc"))
    jc.set(qn("w:val"), "both")
    ppr.append(run_props(size=28))
    return ppr


def ppr_chapter() -> ET.Element:
    ppr = ET.Element(qn("w:pPr"))
    style = ET.SubElement(ppr, qn("w:pStyle"))
    style.set(qn("w:val"), "Heading1")
    spacing = ET.SubElement(ppr, qn("w:spacing"))
    spacing.set(qn("w:after"), "0")
    spacing.set(qn("w:line"), "240")
    spacing.set(qn("w:lineRule"), "auto")
    ind = ET.SubElement(ppr, qn("w:ind"))
    ind.set(qn("w:firstLine"), "709")
    outline = ET.SubElement(ppr, qn("w:outlineLvl"))
    outline.set(qn("w:val"), "0")
    ppr.append(run_props(bold=True, size=32))
    return ppr


def ppr_subheading() -> ET.Element:
    ppr = ET.Element(qn("w:pPr"))
    spacing = ET.SubElement(ppr, qn("w:spacing"))
    spacing.set(qn("w:after"), "0")
    spacing.set(qn("w:line"), "240")
    spacing.set(qn("w:lineRule"), "auto")
    ind = ET.SubElement(ppr, qn("w:ind"))
    ind.set(qn("w:firstLine"), "709")
    outline = ET.SubElement(ppr, qn("w:outlineLvl"))
    outline.set(qn("w:val"), "1")
    ppr.append(run_props(bold=True, size=28))
    return ppr


def ppr_table_caption() -> ET.Element:
    ppr = ppr_normal()
    jc = ppr.find("w:jc", NS)
    if jc is not None:
        jc.set(qn("w:val"), "left")
    return ppr


def make_para(text: str = "", *, kind: str = "normal") -> ET.Element:
    p = ET.Element(qn("w:p"))
    if kind == "chapter":
        p.append(ppr_chapter())
        rpr = run_props(bold=True, size=32)
    elif kind == "subheading":
        p.append(ppr_subheading())
        rpr = run_props(bold=True, size=28)
    elif kind == "table_caption":
        p.append(ppr_table_caption())
        rpr = run_props(size=28)
    elif kind == "blank":
        p.append(ppr_normal())
        return p
    else:
        p.append(ppr_normal())
        rpr = run_props(size=28)

    if text:
        run = ET.SubElement(p, qn("w:r"))
        run.append(rpr)
        t = ET.SubElement(run, qn("w:t"))
        if text[:1].isspace() or text[-1:].isspace():
            t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
        t.text = text
    return p


def make_cell(text: str, width: int, *, bold: bool = False) -> ET.Element:
    tc = ET.Element(qn("w:tc"))
    tcpr = ET.SubElement(tc, qn("w:tcPr"))
    tcw = ET.SubElement(tcpr, qn("w:tcW"))
    tcw.set(qn("w:w"), str(width))
    tcw.set(qn("w:type"), "dxa")
    p = ET.SubElement(tc, qn("w:p"))
    ppr = ET.SubElement(p, qn("w:pPr"))
    spacing = ET.SubElement(ppr, qn("w:spacing"))
    spacing.set(qn("w:after"), "0")
    spacing.set(qn("w:line"), "240")
    spacing.set(qn("w:lineRule"), "auto")
    ppr.append(run_props(bold=bold, size=28))
    run = ET.SubElement(p, qn("w:r"))
    run.append(run_props(bold=bold, size=28))
    t = ET.SubElement(run, qn("w:t"))
    t.text = text
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
    raise ValueError(f"Paragraph not found: {text}")


def replace_between(body: ET.Element, start_text: str, end_text: str, nodes: list[ET.Element]) -> None:
    start = find_body_index(body, start_text)
    end = find_body_index(body, end_text)
    body.remove(body_children(body)[start])
    for _ in range(end - start - 1):
        body.remove(body_children(body)[start])
    for offset, node in enumerate(nodes):
        body.insert(start + offset, node)


def figure_pair(body: ET.Element, caption: str) -> list[ET.Element]:
    idx = find_body_index(body, caption)
    children = body_children(body)
    image = copy.deepcopy(children[idx - 1])
    cap = copy.deepcopy(children[idx])
    return [image, cap]


def section_6_nodes() -> list[ET.Element]:
    return [
        make_para("6 РАЗРАБОТКА ГРАФИЧЕСКОГО МАТЕРИАЛА", kind="chapter"),
        make_para(kind="blank"),
        make_para(
            "В соответствии с заданием к курсовому проекту графический материал должен отражать алгоритмическую, структурную и пользовательскую стороны разработанной программы. Для проекта ext4tool в состав графической части включены схема алгоритма, схема функциональной структуры программы, диаграмма классов и схема взаимодействия с пользователем и внешней средой.",
        ),
        make_para(
            "Такой набор материалов позволяет показать программу на разных уровнях представления. Сначала раскрывается логика безопасного редактирования метаданных, затем показывается распределение функций между модулями, после этого описываются основные структуры данных, а в завершение демонстрируются внешние связи с терминалом Linux, ext4-образом, резервной копией и средствами сборки.",
        ),
        make_para(kind="blank"),
        make_para("6.1 Разработка схемы алгоритма", kind="subheading"),
        make_para(kind="blank"),
        make_para(
            "Схема алгоритма является основным графическим материалом данного проекта, поскольку именно она показывает полный принцип выполнения наиболее ответственной операции программы – безопасного редактирования метаданных ext4-образа. Схема алгоритма безопасного редактирования метаданных представлена в приложении А. На данном плакате отражается путь выполнения операции от выбора пользователем режима редактирования до сообщения об успешной записи либо завершения по ошибке.",
        ),
        make_para(
            "С точки зрения оформления схема алгоритма выполнена в виде классической блок-схемы. Действия обозначаются прямоугольниками, условия – ромбами, операции ввода и вывода – параллелограммами, а направление выполнения – стрелками. Особое внимание уделено условным переходам, связанным с запретом записи в режиме readonly, обнаружением неподдерживаемых признаков ext4, ошибкой валидации значения, невозможностью создать backup-файл и сбоем постзаписной проверки.",
        ),
        make_para(
            "Логическая структура плаката включает следующие крупные фазы: получение запроса редактирования, проверка режима write, проверка совместимости файловой системы, контроль допустимости поля и нового значения, создание резервной копии образа, выбор объекта редактирования superblock или inode, выполнение записи, повторное чтение изменённой структуры и сравнение фактического значения с ожидаемым. Таким образом, схема алгоритма охватывает не отдельный вспомогательный поиск, а центральный безопасный сценарий программы.",
        ),
        make_para(
            "Выбор именно этого алгоритма обусловлен тем, что он наиболее полно демонстрирует системный характер проекта. В нём используются низкоуровневые операции чтения и записи, проверяются границы доступа к файлу-образу, задействуется модуль metadata_editor, выполняется создание резервной копии и подтверждается корректность результата. Поэтому схема алгоритма показывает не только последовательность действий, но и инженерную логику защиты данных.",
        ),
        make_para(kind="blank"),
        make_para("6.2 Разработка схемы функциональной структуры программы", kind="subheading"),
        make_para(kind="blank"),
        make_para(
            "Схема функциональной структуры показывает внутреннюю организацию проекта и распределение ответственности между его модулями. Схема функциональной структуры программы представлена в приложении Б. На ней отражены основные компоненты ext4tool: main, dashboard, ext4_io, ext4_super, ext4_inode, ext4_dir, metadata_editor и util.",
        ),
        make_para(
            "Наиболее удобной формой оформления данного плаката является модульная схема. В верхней части отображается пользовательский уровень, связанный с dashboard-интерфейсом и запуском программы. Ниже располагаются подсистемы анализа ext4: чтение superblock, обработка group descriptor, работа с inode и каталогами. Отдельно выделен слой ввода-вывода ext4_io, поскольку через него проходят операции pread, pwrite, fsync и создание резервной копии.",
        ),
        make_para(
            "Схема функциональной структуры важна для пояснительной записки, поскольку она показывает, что программа не является монолитным набором процедур. Каждый модуль имеет ограниченную зону ответственности: main выполняет запуск и первичную инициализацию, dashboard формирует интерфейс, ext4_super извлекает параметры файловой системы, ext4_inode и ext4_dir обеспечивают анализ объектов, а metadata_editor отвечает за безопасное изменение разрешённых полей.",
        ),
        make_para(kind="blank"),
        make_para("6.3 Разработка диаграммы классов", kind="subheading"),
        make_para(kind="blank"),
        make_para(
            "Поскольку проект реализован на языке C и не использует объектно-ориентированные классы в прямом смысле, требование задания в данной работе интерпретируется как построение UML-подобной диаграммы основных структур данных и модулей, выполняющих роль классов в проектной модели. Диаграмма классов программы представлена в приложении В.",
        ),
        make_para("На диаграмме представлены следующие основные сущности:", kind="normal"),
        make_para("– структура Ext4Context хранит состояние открытого образа, файловый дескриптор, размер файла, путь к образу и признаки режима записи;", kind="normal"),
        make_para("– структура Ext4SuperView описывает интерпретированные параметры superblock, включая размер блока, размер inode, количество блоков и набор feature-флагов;", kind="normal"),
        make_para("– структура Ext4InodeView используется для представления inode и содержит режим доступа, идентификаторы владельца, размер, флаги и временные метки;", kind="normal"),
        make_para("– структура DirectoryEntry связывает имя объекта с номером inode и используется при листинге каталогов и поиске по пути;", kind="normal"),
        make_para("– структура MetadataEditRequest описывает запрос на редактирование и связывает выбранный объект, изменяемое поле и новое значение.", kind="normal"),
        make_para(
            "В результате диаграмма классов показывает, какие данные являются общими для нескольких подсистем, а какие используются только внутри отдельных сценариев. Это упрощает понимание того, каким образом пользовательский запрос из dashboard преобразуется в чтение, проверку и изменение конкретной структуры ext4-образа.",
        ),
        make_para(kind="blank"),
        make_para("6.4 Разработка схемы взаимодействия с пользователем и внешней средой", kind="subheading"),
        make_para(kind="blank"),
        make_para(
            "Схема взаимодействия с пользователем и внешней средой должна показывать программу ext4tool как элемент более широкой системы. Схема взаимодействия представлена в приложении Г. В центре плаката располагается исполняемая программа, слева отображается пользователь, а справа и снизу показаны внешние объекты: терминал Linux, файл-образ ext4, резервная копия и инструменты сборки.",
        ),
        make_para("Стрелками на схеме показаны основные потоки данных и команд:", kind="normal"),
        make_para("– пользователь запускает программу в терминале и передаёт путь к ext4-образу;", kind="normal"),
        make_para("– программа открывает образ в режиме readonly или write и считывает служебные структуры файловой системы;", kind="normal"),
        make_para("– результаты просмотра, поиска и диагностики выводятся пользователю через ncurses-интерфейс;", kind="normal"),
        make_para("– при выполнении операции записи создаётся резервная копия исходного образа;", kind="normal"),
        make_para("– после изменения данных выполняется повторное чтение и выводится сообщение о результате операции.", kind="normal"),
        make_para(
            "Следует подчеркнуть, что весь комплект графического материала построен по единому принципу и отражает различные уровни представления программы ext4tool. Схема алгоритма раскрывает динамику выполнения ключевой операции, схема функциональной структуры показывает состав модулей, диаграмма классов описывает используемые структуры данных, а схема взаимодействия фиксирует связь программы с пользователем и внешними объектами.",
        ),
        make_para(
            "В совокупности разработанные графические материалы не только иллюстрируют текст пояснительной записки, но и дополняют его, облегчая восприятие архитектуры, логики работы и ограничений безопасного редактирования ext4-образов.",
        ),
        make_para(kind="blank"),
    ]


def section_7_nodes(body: ET.Element) -> list[ET.Element]:
    fig71 = figure_pair(body, "Рисунок 7.1 – Пример запуска программы")
    fig72 = figure_pair(body, "Рисунок 7.2 – Демонстрация главного меню программы")
    fig73 = figure_pair(body, "Рисунок 7.3 – Пример просмотра метаданных superblock")
    fig74 = figure_pair(body, "Рисунок 7.4 – Пример поиска inode по абсолютному пути")
    fig75 = figure_pair(body, "Рисунок 7.5 – Демонстрация меню редактирования inode")

    return [
        make_para("7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ", kind="chapter"),
        make_para(kind="blank"),
        make_para("7.1 Сборка программы", kind="subheading"),
        make_para(kind="blank"),
        make_para("Для сборки проекта необходимо перейти в каталог editor-for-ext4-filesystem и выполнить команду: make.", kind="normal"),
        make_para("В результате создаётся исполняемый файл build/ext4tool. Для удаления собранного файла и временных артефактов используется команда: make clean.", kind="normal"),
        make_para("Для проверки проекта, помимо компилятора GCC и утилиты make, в системе должна быть доступна библиотека ncurses. Также требуется тестовый ext4-образ, поскольку программа не предназначена для работы с активным смонтированным разделом.", kind="normal"),
        make_para("Минимальные программные зависимости приведены в таблице 7.1.", kind="normal"),
        make_para(kind="blank"),
        make_para("Таблица 7.1 – Минимальные программные зависимости", kind="table_caption"),
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
        make_para(kind="blank"),
        make_para("Следовательно, для полной проверки проекта требуется не только успешная компиляция, но и наличие тестового образа файловой системы. При отсутствии образа можно проверить справку, обработку параметров и сообщения об ошибках, однако основные сценарии просмотра и редактирования требуют корректного ext4-файла.", kind="normal"),
        make_para(kind="blank"),
        make_para("7.2 Формат запуска", kind="subheading"),
        make_para(kind="blank"),
        make_para("Общий формат запуска программы имеет вид: ./build/ext4tool --image <path> [--readonly|--write] [--lang en|ru].", kind="normal"),
        make_para("Параметр --image является обязательным и задаёт путь к файлу-образу. Режим readonly используется по умолчанию и предназначен для безопасного просмотра структур. Режим write включается только явно и должен применяться к копии образа, поскольку он разрешает изменение отдельных полей superblock и inode.", kind="normal"),
        make_para("Назначение основных параметров приведено в таблице 7.2.", kind="normal"),
        make_para(kind="blank"),
        make_para("Таблица 7.2 – Параметры командной строки", kind="table_caption"),
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
        make_para(kind="blank"),
        make_para("Таким образом, формат запуска остаётся компактным, но позволяет явно разделить безопасный режим анализа и режим редактирования. Обязательный параметр определяет исходные данные, а дополнительные параметры задают режим доступа и язык интерфейса.", kind="normal"),
        make_para("Пример запуска программы ext4tool из терминала представлен на рисунке 7.1.", kind="normal"),
        make_para(kind="blank"),
        *fig71,
        make_para(kind="blank"),
        make_para("7.3 Основы работы с интерфейсом", kind="subheading"),
        make_para(kind="blank"),
        make_para("После запуска программа открывает указанный образ, считывает основной superblock и выполняет проверку поддерживаемых признаков файловой системы. Если основной superblock недоступен или повреждён, программа пытается использовать корректную резервную копию.", kind="normal"),
        make_para("Основной интерфейс программы реализован на базе библиотеки ncurses и представлен в виде текстового меню. Пользователь перемещается по пунктам меню с помощью клавиш Up и Down, подтверждает выбор клавишей Enter, а возврат или отмена выполняются клавишей Esc. Для выхода используется клавиша q.", kind="normal"),
        make_para("Главное меню объединяет просмотр superblock, резервных superblock, group descriptor и inode, листинг каталогов, поиск объектов, безопасное редактирование, статистические экраны, справку и переключение языка интерфейса.", kind="normal"),
        make_para("Внешний вид главного меню ext4tool после успешного открытия образа представлен на рисунке 7.2.", kind="normal"),
        make_para(kind="blank"),
        *fig72,
        make_para(kind="blank"),
        make_para("7.4 Просмотр и анализ метаданных", kind="subheading"),
        make_para(kind="blank"),
        make_para("Для просмотра основных параметров файловой системы пользователь выбирает пункт View Superblock. В результате программа отображает ключевые поля superblock, включая размер блока, размер inode, число блоков и inode, имя тома, счётчики монтирования и наборы feature-флагов.", kind="normal"),
        make_para("При выборе пункта Backup Superblocks выполняется поиск и отображение резервных superblock. Эта возможность полезна при диагностике образов, в которых начальная область повреждена или вызывает сомнение. Пункт View Group Descriptor позволяет просмотреть сведения о конкретной группе блоков, включая расположение таблицы inode и счётчики свободных объектов.", kind="normal"),
        make_para("Для более детального анализа пользователь может выбрать пункт View Inode и указать номер inode. В результате программа показывает режим доступа, идентификаторы владельца, размер, временные метки, флаги и признаки, связанные с типом объекта.", kind="normal"),
        make_para("Пример отображения метаданных superblock приведён на рисунке 7.3.", kind="normal"),
        make_para(kind="blank"),
        *fig73,
        make_para(kind="blank"),
        make_para("7.5 Навигация по дереву каталогов и поиск объектов", kind="subheading"),
        make_para(kind="blank"),
        make_para("Для работы с содержимым образа программа предоставляет функции листинга каталогов и поиска объектов. Пункт List Directory позволяет вывести содержимое каталога как по абсолютному пути, так и по записи вида inode:N.", kind="normal"),
        make_para("Пункт Find By Path используется для определения номера inode по абсолютному пути. Пользователь вводит путь к объекту, после чего программа последовательно проходит по дереву каталогов и возвращает найденный inode. Пункт Find By Inode выполняет обратную задачу: по известному номеру inode восстанавливает путь к объекту.", kind="normal"),
        make_para("Поиск по имени позволяет найти первый объект с заданным именем в дереве каталогов. Такие функции удобны при анализе образа, поскольку пользователь может переходить от имени файла к inode, от inode к пути и от каталога к его содержимому.", kind="normal"),
        make_para("Пример выполнения поиска inode по абсолютному пути представлен на рисунке 7.4.", kind="normal"),
        make_para(kind="blank"),
        *fig74,
        make_para(kind="blank"),
        make_para("7.6 Редактирование метаданных", kind="subheading"),
        make_para(kind="blank"),
        make_para("Редактирование метаданных доступно только при запуске программы с параметром --write. Если программа работает в режиме только чтения, попытка перехода к редактированию завершается сообщением о запрете записи. Такое поведение предотвращает случайное изменение образа.", kind="normal"),
        make_para("Для изменения параметров superblock используется пункт Edit Superblock. В текущей реализации допускается редактирование volume name, mount count, max mount count и check interval. Для изменения параметров inode используется пункт Edit Inode, поддерживающий изменение mode, uid, gid, atime, ctime, mtime и flags.", kind="normal"),
        make_para("Перед каждой операцией редактирования автоматически создаётся резервная копия исходного образа. После записи программа повторно считывает изменённую структуру и сравнивает фактически полученные значения с ожидаемыми. Только после успешного завершения всех этапов операция считается выполненной.", kind="normal"),
        make_para("Пример выполнения операции редактирования метаданных представлен на рисунке 7.5.", kind="normal"),
        make_para(kind="blank"),
        *fig75,
        make_para(kind="blank"),
        make_para("7.7 Завершение работы и рекомендации по безопасному использованию", kind="subheading"),
        make_para(kind="blank"),
        make_para("Завершение работы с программой выполняется через пункт Exit или клавишу выхода из главного меню. После завершения сеанса открытый образ корректно закрывается, а пользователь возвращается в терминальную оболочку.", kind="normal"),
        make_para("При практическом использовании программы рекомендуется применять копии ext4-образов, а не единственные экземпляры данных. Перед запуском в режиме записи необходимо убедиться, что выбран именно тестовый файл. При появлении диагностических сообщений следует устранить причину ошибки и только затем повторять операцию.", kind="normal"),
        make_para("Типовые ошибки и рекомендации по их устранению приведены в таблице 7.3.", kind="normal"),
        make_para(kind="blank"),
        make_para("Таблица 7.3 – Типовые ошибки и способы их устранения", kind="table_caption"),
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
        make_para(kind="blank"),
        make_para("Таким образом, программа ext4tool предоставляет пользователю единый интерфейс для просмотра, анализа, навигации и безопасного редактирования ext4-образов. Описанная последовательность работы позволяет использовать программу как учебный и прикладной инструмент, обеспечивающий наглядное взаимодействие с метаданными файловой системы.", kind="normal"),
        make_para(kind="blank"),
    ]


def update_toc(document: ET.Element) -> None:
    replacements = {
        "6.1": "6.1 Схема алгоритма\t20",
        "6.2": "6.2 Схема функциональной структуры программы\t21",
        "6.3": "6.3 Диаграмма классов\t21",
        "6.4": "6.4 Схема взаимодействия с пользователем и внешней средой\t22",
    }
    paragraphs = document.findall(".//w:p", NS)
    start = next(i for i, p in enumerate(paragraphs) if p_text(p).strip() == "СОДЕРЖАНИЕ")
    end = next(i for i in range(start + 1, len(paragraphs)) if p_text(paragraphs[i]).strip() == "ВВЕДЕНИЕ")
    for p in paragraphs[start + 1 : end]:
        text = p_text(p).strip()
        for prefix, repl in replacements.items():
            if text.startswith(prefix):
                set_toc_line(p, repl)


def set_toc_line(p: ET.Element, value: str) -> None:
    ppr = p.find("w:pPr", NS)
    ppr_copy = copy.deepcopy(ppr) if ppr is not None else None
    title, page = value.rsplit("\t", 1)
    p.clear()
    if ppr_copy is not None:
        p.append(ppr_copy)
    r = ET.SubElement(p, qn("w:r"))
    r.append(run_props(size=28))
    t = ET.SubElement(r, qn("w:t"))
    t.text = title
    ET.SubElement(r, qn("w:tab"))
    t2 = ET.SubElement(r, qn("w:t"))
    t2.text = page


def force_black(root: ET.Element) -> None:
    for color in root.findall(".//w:color", NS):
        color.set(qn("w:val"), "000000")
        for attr in ("themeColor", "themeTint", "themeShade"):
            color.attrib.pop(qn(f"w:{attr}"), None)


def main() -> None:
    if not BACKUP.exists():
        shutil.copy2(DOCX, BACKUP)

    with ZipFile(DOCX, "r") as zin:
        entries = {name: zin.read(name) for name in zin.namelist()}

    document = ET.fromstring(entries["word/document.xml"])
    body = document.find("w:body", NS)
    if body is None:
        raise RuntimeError("word/body not found")

    section7 = section_7_nodes(body)
    replace_between(body, "6 РАЗРАБОТКА ГРАФИЧЕСКОГО МАТЕРИАЛА", "7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ", section_6_nodes())
    replace_between(body, "7 РУКОВОДСТВО ПОЛЬЗОВАТЕЛЯ", "ЗАКЛЮЧЕНИЕ", section7)
    update_toc(document)
    force_black(document)

    entries["word/document.xml"] = ET.tostring(document, encoding="utf-8", xml_declaration=True)

    tmp = DOCX.with_suffix(".tmp.docx")
    with ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for name, data in entries.items():
            zout.writestr(name, data)
    tmp.replace(DOCX)


if __name__ == "__main__":
    main()
