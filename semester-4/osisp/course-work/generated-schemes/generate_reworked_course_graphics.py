from __future__ import annotations

from pathlib import Path

from generate_course_graphics import Svg, convert_svg


def build_metadata_edit_algorithm() -> Path:
    s = Svg(
        "course_diagram_algorithm",
        "Схема алгоритма безопасного редактирования метаданных",
        "БГУИР КП 6-05-0611-05 220 СА",
    )
    s.text(28, 30, "Алгоритм безопасного редактирования метаданных ext4", size=6.2, weight="bold")

    s.box(58, 52, 52, 14, "Начало", kind="start", code="A01")
    s.box(52, 78, 64, 20, "Получить запрос редактирования", kind="io", code="B01")
    s.box(48, 112, 72, 28, "Режим write разрешён?", kind="decision", code="C01")
    s.box(48, 158, 72, 26, "Проверить совместимость ext4", kind="decision", code="D01")
    s.box(48, 204, 72, 24, "Проверить поле и новое значение", code="E01")
    s.box(48, 244, 72, 24, "Создать backup образа", code="F01")
    s.box(48, 286, 72, 26, "Backup создан?", kind="decision", code="G01")

    s.box(196, 112, 82, 22, "Сообщить о запрете записи", kind="io", code="C04")
    s.box(196, 158, 82, 22, "Перейти в readonly или отказать", kind="io", code="D04")
    s.box(196, 204, 82, 22, "Сообщить об ошибке значения", kind="io", code="E04")
    s.box(196, 286, 82, 22, "Сообщить об ошибке backup", kind="io", code="G04")

    s.box(340, 72, 84, 26, "Определить объект: superblock или inode", kind="decision", code="H07")
    s.box(304, 126, 78, 22, "Прочитать raw superblock", code="I06")
    s.box(420, 126, 78, 22, "Прочитать inode", code="I09")
    s.box(304, 172, 78, 24, "Изменить разрешённые поля superblock", code="J06")
    s.box(420, 172, 78, 24, "Изменить разрешённые поля inode", code="J09")
    s.box(304, 220, 78, 22, "Записать superblock", code="K06")
    s.box(420, 220, 78, 22, "Записать inode", code="K09")
    s.box(304, 266, 78, 22, "Повторно считать superblock", code="L06")
    s.box(420, 266, 78, 22, "Повторно считать inode", code="L09")

    s.box(344, 314, 110, 28, "Повторно считанные значения совпали?", kind="decision", code="M08")
    s.box(222, 352, 86, 22, "Сообщить об ошибке верификации", kind="io", code="N04")
    s.box(420, 352, 86, 22, "Вывести сообщение об успехе", kind="io", code="N09")
    s.box(444, 388, 48, 14, "Конец", kind="start", code="Z09")

    s.arrow_between((84, 66), (84, 78))
    s.arrow_between((84, 98), (84, 112))
    s.arrow_between((120, 126), (196, 123), label="нет")
    s.arrow_between((84, 140), (84, 158), label="да")
    s.arrow_between((120, 171), (196, 169), label="нет")
    s.arrow_between((84, 184), (84, 204), label="да")
    s.arrow_between((120, 216), (196, 215), label="нет")
    s.arrow_between((84, 228), (84, 244), label="да")
    s.arrow_between((84, 268), (84, 286))
    s.arrow_between((120, 299), (196, 297), label="нет")
    s.polyline([(84, 312), (84, 330), (340, 330), (340, 98)], arrow=True)
    s.text(110, 326, "да", size=3.4, anchor="middle")

    for start, end in [((278, 123), (444, 389)), ((278, 169), (444, 389)), ((278, 215), (444, 389)), ((278, 297), (444, 389))]:
        s.arrow_between(start, end, bend=(278, end[1]))

    s.arrow_between((382, 85), (343, 126), label="super")
    s.arrow_between((424, 85), (459, 126), label="inode")
    s.arrow_between((343, 148), (343, 172))
    s.arrow_between((459, 148), (459, 172))
    s.arrow_between((343, 196), (343, 220))
    s.arrow_between((459, 196), (459, 220))
    s.arrow_between((343, 242), (343, 266))
    s.arrow_between((459, 242), (459, 266))
    s.arrow_between((343, 288), (382, 314))
    s.arrow_between((459, 288), (418, 314))
    s.arrow_between((344, 328), (308, 363), label="нет")
    s.arrow_between((454, 328), (463, 352), label="да")
    s.arrow_between((506, 363), (444, 395))
    s.arrow_between((308, 363), (444, 395), bend=(308, 395))

    s.text(28, 310, "Примечание – запись выполняется только после backup и завершается повторным чтением изменённой структуры.", size=4.0)
    return s.save()


def build_class_diagram() -> Path:
    s = Svg(
        "course_diagram_state",
        "Диаграмма классов программы",
        "БГУИР КП 6-05-0611-05 220 ДК",
    )
    s.text(28, 30, "Диаграмма классов и основных структур ext4tool", size=6.2, weight="bold")

    def cls(x: float, y: float, w: float, h: float, name: str, fields: list[str], methods: list[str]) -> None:
        s.rect(x, y, w, h, fill="white", stroke="#111", sw=0.55)
        s.line(x, y + 10, x + w, y + 10, sw=0.35)
        s.line(x, y + 10 + len(fields) * 5.2 + 3, x + w, y + 10 + len(fields) * 5.2 + 3, sw=0.35)
        s.text(x + w / 2, y + 7, name, size=4.2, anchor="middle", weight="bold")
        yy = y + 16
        for field in fields:
            s.text(x + 3, yy, field, size=3.25)
            yy += 5.2
        yy += 4
        for method in methods:
            s.text(x + 3, yy, method, size=3.25)
            yy += 5.2

    cls(36, 54, 92, 58, "Ext4Context", ["fd", "image_path", "image_size", "write_mode", "readonly_forced"], ["open_image()", "read_at()", "write_at()", "backup()"])
    cls(168, 54, 102, 58, "Ext4SuperView", ["offset", "block_size", "inode_size", "blocks_count", "features"], ["read_primary()", "find_backup()", "check_features()"])
    cls(320, 54, 96, 52, "Ext4GroupDescView", ["group_index", "inode_table", "free_blocks", "free_inodes"], ["read_group_desc()"])
    cls(36, 164, 100, 62, "Ext4InodeView", ["inode_no", "mode", "uid", "gid", "size", "flags", "timestamps"], ["read_inode()", "write_inode_fields()"])
    cls(182, 164, 110, 62, "DirectoryEntry", ["inode", "rec_len", "name_len", "file_type", "name"], ["list_dir()", "lookup_path()", "find_by_name()"])
    cls(338, 164, 112, 68, "MetadataEditRequest", ["target", "field", "new_value", "inode_no"], ["validate()", "apply_super()", "apply_inode()", "verify()"])
    cls(112, 276, 106, 54, "Dashboard", ["language", "selected_item", "readonly/write"], ["show_menu()", "show_dialog()", "show_result()"])
    cls(300, 276, 90, 48, "Util", ["le16/le32/le64", "parse_uint"], ["read_le()", "format_time()", "parse_input()"])

    s.arrow_between((128, 83), (168, 83), label="uses")
    s.arrow_between((270, 83), (320, 80), label="defines")
    s.arrow_between((219, 112), (86, 164), label="inode params", bend=(219, 142))
    s.arrow_between((86, 226), (182, 195), label="blocks")
    s.arrow_between((394, 164), (86, 195), label="edit inode", bend=(394, 140))
    s.arrow_between((394, 164), (219, 112), label="edit super", bend=(394, 122))
    s.arrow_between((165, 276), (86, 226), label="view")
    s.arrow_between((165, 276), (237, 226), label="search")
    s.arrow_between((218, 303), (338, 198), label="edit")
    s.arrow_between((390, 300), (450, 198), label="parse")
    s.arrow_between((82, 112), (165, 276), label="context", bend=(56, 252))

    s.text(34, 348, "Примечание – диаграмма отражает основные структуры данных и модули C-программы, выполняющие роль классов в проектной модели.", size=4.0)
    return s.save()


def main() -> None:
    for svg in [build_metadata_edit_algorithm(), build_class_diagram()]:
        convert_svg(svg)


if __name__ == "__main__":
    main()
