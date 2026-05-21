from __future__ import annotations

import html
import shutil
import subprocess
import textwrap
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent

PAGE_W = 594.0
PAGE_H = 420.0
PX_W = 3508
PX_H = 2480
MARGIN = 12.0
TITLE_X = 370.0
TITLE_Y = 336.0
TITLE_W = 210.0
TITLE_H = 66.0
CONTENT_BOTTOM = TITLE_Y - 10


class Svg:
    def __init__(self, filename: str, title: str, code: str, sheet: str = "А2"):
        self.filename = filename
        self.title = title
        self.code = code
        self.sheet = sheet
        self.items: list[str] = []
        self._defs()
        self._page()

    def _defs(self) -> None:
        self.items.append(
            """
<defs>
  <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5"
          markerWidth="5" markerHeight="5" orient="auto-start-reverse">
    <path d="M 0 0 L 10 5 L 0 10 z" fill="#111"/>
  </marker>
</defs>
""".strip()
        )

    def _page(self) -> None:
        self.rect(0, 0, PAGE_W, PAGE_H, fill="white", stroke="none")
        self.rect(MARGIN, MARGIN, PAGE_W - 2 * MARGIN, PAGE_H - 2 * MARGIN, fill="none", stroke="#111", sw=0.6)
        self.rect(MARGIN + 5, MARGIN + 5, PAGE_W - 2 * (MARGIN + 5), PAGE_H - 2 * (MARGIN + 5), fill="none", stroke="#111", sw=0.35)
        self._title_block()

    def _title_block(self) -> None:
        x, y, w, h = TITLE_X, TITLE_Y, TITLE_W, TITLE_H
        self.rect(x, y, w, h, fill="white", stroke="#111", sw=0.45)
        for yy in [y + 12, y + 30, y + 48]:
            self.line(x, yy, x + w, yy, sw=0.35)
        for xx in [x + 28, x + 56, x + 92, x + 128, x + 160]:
            self.line(xx, y + 48, xx, y + h, sw=0.3)
        for xx in [x + 142, x + 176]:
            self.line(xx, y, xx, y + 12, sw=0.3)

        self.text(x + 4, y + 8, self.code, size=3.7, weight="bold")
        self.text(x + 146, y + 8, "Лит.", size=3.5)
        self.text(x + 180, y + 8, f"Формат {self.sheet}", size=3.5)
        self.multiline_text(x + 4, y + 18, self.title, width=56, size=4.2, anchor="start", weight="bold", line_h=4.8)
        self.text(x + 4, y + 40, "ext4tool", size=4.1)
        self.text(x + 4, y + 56, "Изм.", size=3.2)
        self.text(x + 30, y + 56, "Лист", size=3.2)
        self.text(x + 58, y + 56, "N докум.", size=3.2)
        self.text(x + 96, y + 56, "Подп.", size=3.2)
        self.text(x + 132, y + 56, "Дата", size=3.2)
        self.text(x + 164, y + 56, "Лист 1", size=3.2)
        self.text(x + 164, y + 64, "Листов 1", size=3.2)
        self.text(x + 4, y + 64, "Разраб. Новицкий", size=3.2)
        self.text(x + 96, y + 64, "Пров. Благиных", size=3.2)

    def save(self) -> Path:
        content = "\n".join(self.items)
        svg = (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{PX_W}" height="{PX_H}" '
            f'viewBox="0 0 {PAGE_W} {PAGE_H}">\n'
            f"{content}\n</svg>\n"
        )
        path = OUT_DIR / f"{self.filename}.svg"
        path.write_text(svg, encoding="utf-8")
        return path

    @staticmethod
    def esc(value: str) -> str:
        return html.escape(value, quote=True)

    def rect(self, x: float, y: float, w: float, h: float, *, fill: str = "white", stroke: str = "#111", sw: float = 0.55, rx: float = 0) -> None:
        self.items.append(
            f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx:.2f}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{sw:.2f}"/>'
        )

    def ellipse(self, cx: float, cy: float, rx: float, ry: float, *, fill: str = "white", stroke: str = "#111", sw: float = 0.55) -> None:
        self.items.append(
            f'<ellipse cx="{cx:.2f}" cy="{cy:.2f}" rx="{rx:.2f}" ry="{ry:.2f}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{sw:.2f}"/>'
        )

    def circle(self, cx: float, cy: float, r: float, *, fill: str = "white", stroke: str = "#111", sw: float = 0.55) -> None:
        self.items.append(
            f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw:.2f}"/>'
        )

    def polygon(self, points: list[tuple[float, float]], *, fill: str = "white", stroke: str = "#111", sw: float = 0.55) -> None:
        pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
        self.items.append(f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" stroke-width="{sw:.2f}"/>')

    def line(self, x1: float, y1: float, x2: float, y2: float, *, sw: float = 0.45, arrow: bool = False, dash: bool = False) -> None:
        extra = ' marker-end="url(#arrow)"' if arrow else ""
        dash_attr = ' stroke-dasharray="3 2"' if dash else ""
        self.items.append(
            f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="#111" stroke-width="{sw:.2f}"{extra}{dash_attr}/>'
        )

    def polyline(self, points: list[tuple[float, float]], *, sw: float = 0.45, arrow: bool = True, dash: bool = False) -> None:
        pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
        extra = ' marker-end="url(#arrow)"' if arrow else ""
        dash_attr = ' stroke-dasharray="3 2"' if dash else ""
        self.items.append(
            f'<polyline points="{pts}" fill="none" stroke="#111" stroke-width="{sw:.2f}"{extra}{dash_attr}/>'
        )

    def text(self, x: float, y: float, value: str, *, size: float = 4.0, anchor: str = "start", weight: str = "normal", style: str = "normal") -> None:
        self.items.append(
            f'<text x="{x:.2f}" y="{y:.2f}" font-family="Times New Roman, serif" '
            f'font-size="{size:.2f}" font-weight="{weight}" font-style="{style}" '
            f'text-anchor="{anchor}" fill="#111">{self.esc(value)}</text>'
        )

    def multiline_text(
        self,
        x: float,
        y: float,
        value: str,
        *,
        width: int = 22,
        size: float = 4.0,
        anchor: str = "middle",
        weight: str = "normal",
        line_h: float | None = None,
    ) -> None:
        line_h = line_h or size * 1.18
        for i, line in enumerate(self.wrap_lines(value, width)):
            self.text(x, y + i * line_h, line, size=size, anchor=anchor, weight=weight)

    @staticmethod
    def wrap_lines(value: str, width: int) -> list[str]:
        lines: list[str] = []
        for part in value.split("\n"):
            lines.extend(textwrap.wrap(part, width=width, break_long_words=False) or [""])
        return lines

    def box(self, x: float, y: float, w: float, h: float, text: str, *, kind: str = "process", code: str | None = None, size: float = 4.0) -> None:
        if kind == "start":
            self.rect(x, y, w, h, rx=h / 2, sw=0.6)
        elif kind == "io":
            s = min(8, w * 0.16)
            self.polygon([(x + s, y), (x + w, y), (x + w - s, y + h), (x, y + h)], sw=0.6)
        elif kind == "decision":
            self.polygon([(x + w / 2, y), (x + w, y + h / 2), (x + w / 2, y + h), (x, y + h / 2)], sw=0.6)
        elif kind == "predef":
            self.rect(x, y, w, h, sw=0.6)
            self.line(x + 5, y, x + 5, y + h, sw=0.35)
            self.line(x + w - 5, y, x + w - 5, y + h, sw=0.35)
        elif kind == "state":
            self.rect(x, y, w, h, rx=5, sw=0.6)
        else:
            self.rect(x, y, w, h, sw=0.6)
        if code:
            self.text(x + 2, y + 4, code, size=2.8, style="italic")
        lines = self.wrap_lines(text, max(9, int(w / 2.5)))
        line_h = size * 1.18
        start_y = y + h / 2 - (len(lines) - 1) * line_h / 2 + size * 0.35
        for i, line in enumerate(lines):
            self.text(x + w / 2, start_y + i * line_h, line, size=size, anchor="middle")

    def arrow_between(self, a: tuple[float, float], b: tuple[float, float], *, label: str | None = None, label_pos: float = 0.5, bend: tuple[float, float] | None = None) -> None:
        if bend:
            points = [a, bend, b]
            self.polyline(points, arrow=True)
            lx = a[0] + (b[0] - a[0]) * label_pos
            ly = a[1] + (b[1] - a[1]) * label_pos
        else:
            self.line(a[0], a[1], b[0], b[1], arrow=True)
            lx = a[0] + (b[0] - a[0]) * label_pos
            ly = a[1] + (b[1] - a[1]) * label_pos
        if label:
            self.rect(lx - 7, ly - 4, 14, 6, fill="white", stroke="none", sw=0)
            self.text(lx, ly + 1.5, label, size=3.3, anchor="middle")


def build_algorithm() -> Path:
    s = Svg(
        "course_diagram_algorithm",
        "Схема алгоритма поиска inode по абсолютному пути",
        "БГУИР КП 6-05-0611-05 220 СА",
    )
    s.text(28, 30, "Алгоритм функции ext4_lookup_path()", size=6.2, weight="bold")

    # Main vertical path.
    s.box(82, 44, 52, 14, "Начало", kind="start", code="A01")
    s.box(77, 70, 62, 20, "Получить path и out_inode", kind="io", code="B01")
    s.box(78, 104, 60, 28, "Путь абсолютный?", kind="decision", code="C01")
    s.box(80, 150, 56, 20, "current = inode 2", code="D01")
    s.box(80, 188, 56, 22, "Выделить следующую компоненту", code="E01")
    s.box(78, 226, 60, 28, "Компонента существует?", kind="decision", code="F01")

    # Return and loop body.
    s.box(420, 230, 66, 20, "Вернуть current inode", kind="io", code="F08")
    s.box(520, 233, 42, 14, "Конец", kind="start", code="F10")
    s.box(220, 224, 72, 28, "Текущий inode является каталогом?", kind="decision", code="G04")
    s.box(220, 178, 72, 22, "Найти имя в записях каталога", kind="predef", code="H04")
    s.box(220, 132, 72, 28, "Запись найдена?", kind="decision", code="I04")
    s.box(220, 90, 72, 20, "current = inode найденной записи", code="J04")
    s.box(220, 54, 72, 20, "Перейти к следующей компоненте", code="K04")

    # Error branches.
    s.box(420, 92, 78, 20, "Вернуть ошибку пути", kind="io", code="C08")
    s.box(420, 136, 78, 20, "Вернуть ошибку поиска", kind="io", code="I08")
    s.box(420, 190, 78, 20, "Вернуть ошибку типа объекта", kind="io", code="G08")
    s.box(520, 151, 42, 14, "Конец", kind="start", code="Z01")

    # Arrows.
    s.arrow_between((108, 58), (108, 70))
    s.arrow_between((108, 90), (108, 104))
    s.arrow_between((138, 118), (420, 102), label="нет", bend=(360, 118))
    s.arrow_between((108, 132), (108, 150), label="да")
    s.arrow_between((108, 170), (108, 188))
    s.arrow_between((108, 210), (108, 226))
    s.polyline([(138, 240), (180, 240), (180, 270), (453, 270), (453, 250)], arrow=True)
    s.text(166, 236, "нет", size=3.5, anchor="middle")
    s.arrow_between((486, 240), (520, 240))
    s.arrow_between((138, 240), (220, 238), label="да")
    s.arrow_between((292, 238), (420, 200), label="нет", bend=(348, 238))
    s.arrow_between((256, 224), (256, 200), label="да")
    s.arrow_between((256, 178), (256, 160))
    s.arrow_between((292, 146), (420, 146), label="нет")
    s.arrow_between((256, 132), (256, 110), label="да")
    s.arrow_between((256, 90), (256, 74))
    s.polyline([(220, 64), (178, 64), (178, 199), (136, 199)], arrow=True)
    s.arrow_between((498, 102), (520, 155), bend=(520, 102))
    s.arrow_between((498, 146), (520, 155))
    s.arrow_between((498, 200), (520, 165), bend=(520, 200))

    s.text(28, 315, "Примечание – возврат к следующей компоненте выполняется до обработки всех элементов абсолютного пути.", size=4.0)
    return s.save()


def build_functional_structure() -> Path:
    s = Svg(
        "course_diagram_functional_structure",
        "Схема функциональной структуры программы",
        "БГУИР КП 6-05-0611-05 220 СФ",
    )
    s.text(28, 30, "Функциональная структура ext4tool", size=6.2, weight="bold")

    s.box(40, 58, 72, 28, "Пользователь\nклавиатура, терминал", kind="state", code="U")
    s.box(152, 50, 82, 36, "dashboard\nncurses-интерфейс", kind="state", code="UI")
    s.box(274, 50, 82, 36, "main\nзапуск и инициализация", kind="state", code="M")
    s.box(410, 54, 82, 30, "Linux / POSIX\nopen, pread, pwrite", kind="state", code="OS")

    s.box(78, 128, 92, 34, "ext4_super\nsuperblock и группы", code="S")
    s.box(200, 128, 92, 34, "ext4_inode\nчтение и запись inode", code="I")
    s.box(322, 128, 92, 34, "ext4_dir\nкаталоги и поиск", code="D")

    s.box(78, 212, 92, 34, "metadata_editor\nвалидация и запись", code="E")
    s.box(200, 212, 92, 34, "ext4_io\nконтекст образа и backup", code="IO")
    s.box(322, 212, 92, 34, "util\nlittle-endian и ввод", code="T")

    s.box(460, 132, 74, 28, "ext4-образ", kind="io", code="F1")
    s.box(460, 218, 74, 28, "backup-образ", kind="io", code="F2")

    s.arrow_between((112, 72), (152, 68))
    s.arrow_between((274, 68), (234, 68), label="запуск")
    s.arrow_between((356, 68), (410, 69), label="системные вызовы")
    s.arrow_between((193, 86), (124, 128))
    s.arrow_between((193, 86), (246, 128))
    s.arrow_between((193, 86), (368, 128))
    s.arrow_between((193, 86), (124, 212))
    s.arrow_between((124, 162), (246, 212), label="чтение")
    s.arrow_between((246, 162), (246, 212), label="смещения")
    s.arrow_between((368, 162), (246, 212), label="каталоги")
    s.arrow_between((170, 229), (200, 229), label="запись")
    s.arrow_between((292, 229), (460, 146), label="pread/pwrite")
    s.arrow_between((292, 229), (460, 232), label="копия")
    s.line(322, 229, 414, 229, arrow=False, dash=True)
    s.text(368, 225, "общие функции", size=3.2, anchor="middle")

    s.text(34, 284, "Связи отражают основные вызовы между модулями и внешними объектами обработки.", size=4.1)
    return s.save()


def build_state_diagram() -> Path:
    s = Svg(
        "course_diagram_state",
        "Диаграмма состояний программы",
        "БГУИР КП 6-05-0611-05 220 ДС",
    )
    s.text(28, 30, "Диаграмма состояний ext4tool", size=6.2, weight="bold")

    s.circle(46, 66, 5, fill="#111")
    s.box(72, 54, 62, 24, "Разбор аргументов", kind="state")
    s.box(162, 54, 62, 24, "Открытие образа", kind="state")
    s.box(252, 54, 70, 24, "Чтение superblock", kind="state")
    s.box(352, 50, 78, 32, "Проверка признаков ext4", kind="decision")
    s.box(462, 54, 70, 24, "Главное меню", kind="state")

    s.box(70, 128, 78, 28, "Просмотр метаданных", kind="state")
    s.box(188, 128, 82, 28, "Навигация и поиск", kind="state")
    s.box(316, 128, 76, 28, "Запрос редактирования", kind="state")
    s.box(446, 126, 82, 32, "Проверка режима write", kind="decision")

    s.box(70, 206, 76, 28, "Сообщение об ошибке", kind="state")
    s.box(188, 206, 82, 28, "Создание backup", kind="state")
    s.box(316, 206, 76, 28, "Запись данных", kind="state")
    s.box(446, 204, 82, 32, "Постзаписная верификация", kind="state")

    s.box(254, 286, 72, 22, "Завершение", kind="state")
    s.circle(354, 297, 6, fill="white", sw=0.7)
    s.circle(354, 297, 3.8, fill="#111")

    s.arrow_between((51, 66), (72, 66))
    s.arrow_between((134, 66), (162, 66))
    s.arrow_between((224, 66), (252, 66))
    s.arrow_between((322, 66), (352, 66))
    s.arrow_between((430, 66), (462, 66), label="да")
    s.arrow_between((391, 82), (391, 206), label="ошибка", bend=(58, 108))
    s.arrow_between((497, 78), (110, 128), label="просмотр", bend=(497, 108))
    s.arrow_between((497, 78), (229, 128), label="поиск", bend=(497, 114))
    s.arrow_between((497, 78), (354, 128), label="edit", bend=(497, 120))
    s.arrow_between((497, 78), (290, 286), label="exit", bend=(560, 214))

    s.arrow_between((110, 156), (110, 206), label="ошибка")
    s.arrow_between((148, 142), (462, 70), label="результат", bend=(140, 96))
    s.arrow_between((270, 142), (462, 70), label="результат", bend=(284, 96))
    s.arrow_between((392, 142), (446, 142))
    s.arrow_between((487, 158), (229, 206), label="да", bend=(487, 186))
    s.arrow_between((446, 142), (146, 220), label="нет", bend=(440, 180))
    s.arrow_between((270, 220), (316, 220))
    s.arrow_between((392, 220), (446, 220))
    s.arrow_between((487, 236), (487, 254), label="успех")
    s.polyline([(487, 254), (462, 274), (497, 274), (497, 78)], arrow=True)
    s.arrow_between((146, 220), (462, 70), label="возврат", bend=(146, 268))
    s.arrow_between((487, 204), (146, 220), label="сбой", bend=(420, 184))
    s.arrow_between((326, 297), (348, 297))

    return s.save()


def build_interaction() -> Path:
    s = Svg(
        "course_diagram_interaction",
        "Схема взаимодействия с пользователем и внешней средой",
        "БГУИР КП 6-05-0611-05 220 СВ",
    )
    s.text(28, 30, "Взаимодействие ext4tool с пользователем и внешней средой", size=6.2, weight="bold")

    # Actor.
    s.circle(54, 88, 8)
    s.line(54, 96, 54, 124)
    s.line(34, 108, 74, 108)
    s.line(54, 124, 38, 150)
    s.line(54, 124, 70, 150)
    s.text(54, 164, "Пользователь", size=4.5, anchor="middle")

    # System boundary.
    s.rect(112, 50, 255, 222, fill="none", stroke="#111", sw=0.6, rx=4)
    s.text(126, 62, "ext4tool", size=5.4, weight="bold")

    use_cases = [
        (176, 88, "Запуск с параметрами"),
        (286, 88, "Просмотр superblock,\ninode и каталогов"),
        (176, 148, "Поиск по пути,\ninode и имени"),
        (286, 148, "Безопасное\nредактирование"),
        (176, 210, "Получение справки"),
        (286, 210, "Создание backup\nи верификация"),
    ]
    for cx, cy, label in use_cases:
        s.ellipse(cx, cy, 47, 18)
        s.multiline_text(cx, cy - 3, label, width=18, size=3.8)

    for cx, cy, _ in use_cases[:5]:
        s.line(74, 110, cx - 47, cy, arrow=False)
    s.line(286, 166, 286, 192, arrow=True, dash=True)
    s.text(302, 182, "include", size=3.2)

    # External environment.
    s.box(422, 68, 88, 30, "Терминал Linux\nstdin/stdout, ncurses", kind="state")
    s.box(422, 130, 88, 30, "Файл-образ ext4\nчтение/запись", kind="io")
    s.box(422, 192, 88, 30, "Резервная копия\nобраза", kind="io")
    s.box(422, 254, 88, 28, "Makefile и GCC\nсборка программы", kind="state")

    s.arrow_between((223, 88), (422, 83), label="I/O")
    s.arrow_between((333, 88), (422, 145), label="чтение")
    s.arrow_between((333, 148), (422, 145), label="write")
    s.arrow_between((333, 210), (422, 207), label="backup")
    s.arrow_between((223, 210), (422, 268), label="сборка", bend=(300, 260))

    s.text(34, 296, "Схема показывает пользовательские сценарии и внешние объекты, с которыми взаимодействует программа.", size=4.1)
    return s.save()


def convert_svg(svg: Path) -> None:
    png = OUT_DIR / f"{svg.stem}.png"
    pdf = OUT_DIR / f"{svg.stem}.pdf"
    sips = shutil.which("sips")
    if sips:
        subprocess.run([sips, "-s", "format", "png", str(svg), "--out", str(png)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([sips, "-s", "format", "pdf", str(png), "--out", str(pdf)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main() -> None:
    svgs = [
        build_algorithm(),
        build_functional_structure(),
        build_state_diagram(),
        build_interaction(),
    ]
    for svg in svgs:
        convert_svg(svg)
    print("Generated:")
    for svg in svgs:
        print(f"- {svg}")
        for ext in (".png", ".pdf"):
            path = svg.with_suffix(ext)
            if path.exists():
                print(f"  {path}")


if __name__ == "__main__":
    main()
