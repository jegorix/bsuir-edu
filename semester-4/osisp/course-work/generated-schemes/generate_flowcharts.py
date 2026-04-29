from __future__ import annotations

import math
import textwrap
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle


OUT_DIR = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Shape:
    code: str
    kind: str
    col: float
    row: str
    text: str
    width: float = 1.05
    height: float = 0.68


class Flowchart:
    def __init__(self, filename: str, title: str, cols: int, rows: str, figsize: tuple[float, float]):
        self.filename = filename
        self.title = title
        self.cols = cols
        self.rows = rows
        self.figsize = figsize
        self.shapes: dict[str, Shape] = {}
        self.edges: list[tuple[str, str, str | None, tuple[float, float]]] = []

    def add(self, shape: Shape) -> None:
        self.shapes[shape.code] = shape

    def edge(self, start: str, end: str, label: str | None = None, offset: tuple[float, float] = (0, 0)) -> None:
        self.edges.append((start, end, label, offset))

    def point(self, shape: Shape) -> tuple[float, float]:
        row_index = self.rows.index(shape.row)
        return shape.col, len(self.rows) - row_index

    def setup(self):
        fig, ax = plt.subplots(figsize=self.figsize)
        fig.patch.set_facecolor("white")
        ax.set_facecolor("white")
        ax.set_xlim(0.1, self.cols + 1.1)
        ax.set_ylim(0.2, len(self.rows) + 1.2)
        ax.set_aspect("equal")
        ax.axis("off")

        header_y = len(self.rows) + 0.55
        ax.add_patch(Rectangle((1.0, header_y - 0.18), self.cols, 0.28, fill=False, linewidth=0.8))
        for col in range(1, self.cols + 1):
            ax.text(col + 0.5, header_y - 0.04, f"{col:02d}", ha="center", va="center",
                    fontsize=8, fontstyle="italic")
            ax.plot([col, col], [header_y - 0.18, header_y - 0.28], color="black", linewidth=0.6)
        ax.plot([self.cols + 1, self.cols + 1], [header_y - 0.18, header_y - 0.28],
                color="black", linewidth=0.6)

        ax.add_patch(Rectangle((0.33, 0.55), 0.25, len(self.rows), fill=False, linewidth=0.8))
        for i, row in enumerate(self.rows):
            y = len(self.rows) - i
            ax.text(0.46, y, row, ha="center", va="center", fontsize=9, fontstyle="italic")
            ax.plot([0.58, 0.66], [y - 0.5, y - 0.5], color="black", linewidth=0.6)

        ax.text(1.0, 0.27, self.title, ha="left", va="bottom", fontsize=8)
        return fig, ax

    @staticmethod
    def wrapped(text: str, width: int = 15) -> str:
        return "\n".join(textwrap.wrap(text, width=width, break_long_words=False))

    def draw_shape(self, ax, shape: Shape) -> None:
        x, y = self.point(shape)
        w, h = shape.width, shape.height
        face = "white"
        edge = "black"
        lw = 1.1

        if shape.kind == "start":
            patch = FancyBboxPatch((x - w / 2, y - h / 2), w, h,
                                   boxstyle="round,pad=0.02,rounding_size=0.18",
                                   linewidth=lw, facecolor=face, edgecolor=edge)
            ax.add_patch(patch)
        elif shape.kind == "decision":
            patch = Polygon([(x, y + h / 2), (x + w / 2, y), (x, y - h / 2), (x - w / 2, y)],
                            closed=True, linewidth=lw, facecolor=face, edgecolor=edge)
            ax.add_patch(patch)
        elif shape.kind == "io":
            slant = w * 0.16
            patch = Polygon([(x - w / 2 + slant, y + h / 2), (x + w / 2, y + h / 2),
                             (x + w / 2 - slant, y - h / 2), (x - w / 2, y - h / 2)],
                            closed=True, linewidth=lw, facecolor=face, edgecolor=edge)
            ax.add_patch(patch)
        elif shape.kind == "predef":
            ax.add_patch(Rectangle((x - w / 2, y - h / 2), w, h, linewidth=lw,
                                   facecolor=face, edgecolor=edge))
            ax.plot([x - w / 2 + 0.10, x - w / 2 + 0.10], [y - h / 2, y + h / 2],
                    color=edge, linewidth=lw)
            ax.plot([x + w / 2 - 0.10, x + w / 2 - 0.10], [y - h / 2, y + h / 2],
                    color=edge, linewidth=lw)
        else:
            ax.add_patch(Rectangle((x - w / 2, y - h / 2), w, h, linewidth=lw,
                                   facecolor=face, edgecolor=edge))

        ax.text(x - w / 2 + 0.03, y + h / 2 - 0.02, shape.code, ha="left", va="top",
                fontsize=5.5, fontstyle="italic")
        ax.text(x, y, self.wrapped(shape.text), ha="center", va="center", fontsize=5.6,
                fontstyle="italic")

    def draw_edge(self, ax, start: str, end: str, label: str | None, offset: tuple[float, float]) -> None:
        start_shape = self.shapes[start]
        end_shape = self.shapes[end]
        sx, sy = self.point(start_shape)
        ex, ey = self.point(end_shape)

        if abs(sx - ex) < 1e-6 or abs(sy - ey) < 1e-6:
            center_points = [(sx, sy), (ex, ey)]
        else:
            mid_x = (sx + ex) / 2
            center_points = [(sx, sy), (mid_x, sy), (mid_x, ey), (ex, ey)]

        points = center_points[:]
        if len(points) >= 2:
            points[0] = self._edge_anchor(start_shape, points[0], points[1], outbound=True)
            points[-1] = self._edge_anchor(end_shape, points[-1], points[-2], outbound=False)

        for i in range(len(points) - 2):
            ax.plot(
                [points[i][0], points[i + 1][0]],
                [points[i][1], points[i + 1][1]],
                color="black",
                linewidth=0.9,
            )

        if len(points) >= 2:
            ax.annotate(
                "",
                xy=points[-1],
                xytext=points[-2],
                arrowprops=dict(arrowstyle="->", linewidth=0.9, color="black"),
            )

        if label:
            mx, my = self._polyline_midpoint(points)
            mx += offset[0]
            my += offset[1]
            ax.text(mx, my, label, fontsize=6, fontstyle="italic", ha="center", va="center",
                    bbox=dict(facecolor="white", edgecolor="none", pad=0.3))

    @staticmethod
    def _polyline_midpoint(points: list[tuple[float, float]]) -> tuple[float, float]:
        if len(points) < 2:
            return points[0] if points else (0.0, 0.0)

        lengths = []
        total = 0.0
        for i in range(len(points) - 1):
            x1, y1 = points[i]
            x2, y2 = points[i + 1]
            seg = math.hypot(x2 - x1, y2 - y1)
            lengths.append(seg)
            total += seg

        if total == 0:
            return points[0]

        target = total / 2
        traversed = 0.0
        for i, seg in enumerate(lengths):
            if traversed + seg >= target:
                ratio = (target - traversed) / seg if seg else 0.0
                x1, y1 = points[i]
                x2, y2 = points[i + 1]
                return x1 + (x2 - x1) * ratio, y1 + (y2 - y1) * ratio
            traversed += seg
        return points[-1]

    @staticmethod
    def _edge_anchor(shape: Shape,
                     center: tuple[float, float],
                     neighbor: tuple[float, float],
                     outbound: bool) -> tuple[float, float]:
        cx, cy = center
        nx, ny = neighbor
        w2 = shape.width / 2
        h2 = shape.height / 2
        dx = nx - cx
        dy = ny - cy

        # For start of line, direction is center -> neighbor.
        # For end of line, direction is neighbor -> center.
        if not outbound:
            dx = cx - nx
            dy = cy - ny

        if abs(dx) >= abs(dy):
            return (cx + w2 if dx > 0 else cx - w2, cy)
        return (cx, cy + h2 if dy > 0 else cy - h2)

    def render(self) -> None:
        fig, ax = self.setup()
        for _, shape in self.shapes.items():
            self.draw_shape(ax, shape)
        for start, end, label, offset in self.edges:
            self.draw_edge(ax, start, end, label, offset)

        pdf = OUT_DIR / f"{self.filename}.pdf"
        png = OUT_DIR / f"{self.filename}.png"
        fig.savefig(pdf, bbox_inches="tight")
        fig.savefig(png, dpi=180, bbox_inches="tight")
        plt.close(fig)


def build_lookup_path() -> Flowchart:
    fc = Flowchart(
        "schema_ext4_lookup_path",
        "Схема алгоритма функции ext4_lookup_path",
        cols=11,
        rows="ABCDEFGHIJKLM",
        figsize=(16.54, 11.69),
    )
    for s in [
        Shape("A02", "start", 2.0, "A", "Начало", 0.9, 0.45),
        Shape("B02", "io", 2.0, "B", "Получить path и out_inode", 1.18, 0.72),
        Shape("C02", "decision", 2.0, "C", "Путь начинается с '/'?", 1.15, 0.78),
        Shape("C04", "io", 4.0, "C", "Вернуть ошибку пути", 1.18, 0.72),
        Shape("D04", "start", 4.0, "D", "Конец", 0.9, 0.45),
        Shape("D02", "decision", 2.0, "D", "Путь равен '/'?", 1.15, 0.78),
        Shape("E04", "io", 4.0, "E", "Вернуть inode 2", 1.12, 0.65),
        Shape("F04", "start", 4.0, "F", "Конец", 0.9, 0.45),
        Shape("E02", "process", 2.0, "E", "Создать копию path", 1.12, 0.65),
        Shape("F02", "decision", 2.0, "F", "Копия создана?", 1.08, 0.72),
        Shape("G04", "io", 4.0, "G", "Вернуть ошибку памяти", 1.18, 0.72),
        Shape("H04", "start", 4.0, "H", "Конец", 0.9, 0.45),
        Shape("G02", "process", 2.0, "G", "current = ROOT_INODE", 1.18, 0.65),
        Shape("H02", "process", 2.0, "H", "Выделить следующую компоненту", 1.25, 0.72),
        Shape("I02", "decision", 2.0, "I", "Компонента есть?", 1.08, 0.72),
        Shape("I05", "io", 5.0, "I", "Вернуть current inode", 1.18, 0.72),
        Shape("J05", "start", 5.0, "J", "Конец", 0.9, 0.45),
        Shape("H05", "predef", 5.0, "H", "Найти компоненту в текущем каталоге", 1.35, 0.70),
        Shape("G08", "decision", 8.0, "G", "Запись найдена?", 1.08, 0.72),
        Shape("I08", "io", 8.0, "I", "Вернуть ошибку поиска", 1.18, 0.72),
        Shape("J08", "start", 8.0, "J", "Конец", 0.9, 0.45),
        Shape("E08", "process", 8.0, "E", "current = inode найденной записи", 1.35, 0.72),
        Shape("D08", "process", 8.0, "D", "Перейти к следующей компоненте", 1.25, 0.72),
    ]:
        fc.add(s)
    for e in [
        ("A02", "B02", None, (0, 0)),
        ("B02", "C02", None, (0, 0)),
        ("C02", "C04", "нет", (0, 0.18)),
        ("C04", "D04", None, (0, 0)),
        ("C02", "D02", "да", (0.25, 0)),
        ("D02", "E04", "да", (0.05, 0.15)),
        ("E04", "F04", None, (0, 0)),
        ("D02", "E02", "нет", (0.25, 0)),
        ("E02", "F02", None, (0, 0)),
        ("F02", "G04", "нет", (0, 0.15)),
        ("G04", "H04", None, (0, 0)),
        ("F02", "G02", "да", (0.25, 0)),
        ("G02", "H02", None, (0, 0)),
        ("H02", "I02", None, (0, 0)),
        ("I02", "I05", "нет", (0, 0.15)),
        ("I05", "J05", None, (0, 0)),
        ("I02", "H05", "да", (0, 0.15)),
        ("H05", "G08", None, (0, 0)),
        ("G08", "I08", "нет", (0.2, 0)),
        ("I08", "J08", None, (0, 0)),
        ("G08", "E08", "да", (-0.2, 0)),
        ("E08", "D08", None, (0, 0)),
        ("D08", "H02", None, (0, 0)),
    ]:
        fc.edge(*e)
    return fc


def build_find_inode() -> Flowchart:
    fc = Flowchart(
        "schema_dfs_find_inode",
        "Схема алгоритма функций ext4_find_by_inode и dfs_find_inode",
        cols=9,
        rows="ABCDEFGHIJ",
        figsize=(16.54, 11.69),
    )
    for s in [
        Shape("A01", "start", 1.5, "A", "Начало", 0.9, 0.45),
        Shape("B01", "io", 1.5, "B", "Получить номер искомого inode", 1.22, 0.72),
        Shape("C01", "decision", 1.5, "C", "inode задан и допустим?", 1.12, 0.75),
        Shape("C04", "io", 4.0, "C", "Вернуть ошибку параметров", 1.20, 0.72),
        Shape("D04", "start", 4.0, "D", "Конец", 0.9, 0.45),
        Shape("D01", "decision", 1.5, "D", "inode равен корневому?", 1.12, 0.75),
        Shape("E04", "io", 4.0, "E", "Вернуть путь '/'", 1.15, 0.65),
        Shape("F04", "start", 4.0, "F", "Конец", 0.9, 0.45),
        Shape("E01", "process", 1.5, "E", "current = 2; path = '/'", 1.15, 0.65),
        Shape("F01", "predef", 1.5, "F", "Вызвать dfs_find_inode", 1.25, 0.68),
        Shape("B05", "process", 5.0, "B", "Прочитать записи текущего каталога", 1.30, 0.72),
        Shape("C05", "decision", 5.0, "C", "Есть запись для проверки?", 1.12, 0.75),
        Shape("C08", "io", 8.0, "C", "Вернуть 'inode не найден'", 1.20, 0.72),
        Shape("D08", "start", 8.0, "D", "Конец", 0.9, 0.45),
        Shape("D05", "process", 5.0, "D", "Пропустить '.' и '..'", 1.15, 0.65),
        Shape("E05", "decision", 5.0, "E", "inode записи совпал?", 1.12, 0.75),
        Shape("E08", "io", 8.0, "E", "Вывести найденный путь", 1.20, 0.72),
        Shape("F08", "start", 8.0, "F", "Конец", 0.9, 0.45),
        Shape("F05", "decision", 5.0, "F", "Запись является каталогом?", 1.12, 0.75),
        Shape("G05", "predef", 5.0, "G", "Рекурсивно проверить подкаталог", 1.25, 0.72),
        Shape("H05", "decision", 5.0, "H", "Путь найден рекурсивно?", 1.12, 0.75),
        Shape("H08", "io", 8.0, "H", "Вернуть найденный путь", 1.20, 0.72),
        Shape("I08", "start", 8.0, "I", "Конец", 0.9, 0.45),
    ]:
        fc.add(s)
    for e in [
        ("A01", "B01", None, (0, 0)),
        ("B01", "C01", None, (0, 0)),
        ("C01", "C04", "нет", (0, 0.15)),
        ("C04", "D04", None, (0, 0)),
        ("C01", "D01", "да", (0.25, 0)),
        ("D01", "E04", "да", (0.05, 0.15)),
        ("E04", "F04", None, (0, 0)),
        ("D01", "E01", "нет", (0.25, 0)),
        ("E01", "F01", None, (0, 0)),
        ("F01", "B05", None, (0, 0)),
        ("B05", "C05", None, (0, 0)),
        ("C05", "C08", "нет", (0, 0.15)),
        ("C08", "D08", None, (0, 0)),
        ("C05", "D05", "да", (0.25, 0)),
        ("D05", "E05", None, (0, 0)),
        ("E05", "E08", "да", (0, 0.15)),
        ("E08", "F08", None, (0, 0)),
        ("E05", "F05", "нет", (0.25, 0)),
        ("F05", "G05", "да", (0.25, 0)),
        ("G05", "H05", None, (0, 0)),
        ("H05", "H08", "да", (0, 0.15)),
        ("H08", "I08", None, (0, 0)),
        ("H05", "C05", "нет", (-0.25, 0.10)),
        ("F05", "C05", "нет", (0.25, 0.10)),
    ]:
        fc.edge(*e)
    return fc


def build_metadata_editor() -> Flowchart:
    fc = Flowchart(
        "schema_metadata_editor_apply",
        "Схема алгоритма функции metadata_editor_apply",
        cols=11,
        rows="ABCDEFGHIJKLM",
        figsize=(16.54, 11.69),
    )
    for s in [
        Shape("A02", "start", 2.0, "A", "Начало", 0.9, 0.45),
        Shape("B02", "io", 2.0, "B", "Получить ctx, super, req, result", 1.25, 0.72),
        Shape("C02", "decision", 2.0, "C", "Параметры корректны?", 1.12, 0.75),
        Shape("C04", "io", 4.0, "C", "Вернуть ошибку параметров", 1.20, 0.72),
        Shape("D04", "start", 4.0, "D", "Конец", 0.9, 0.45),
        Shape("D02", "decision", 2.0, "D", "Разрешён режим write?", 1.12, 0.75),
        Shape("E04", "io", 4.0, "E", "Редактирование запрещено", 1.20, 0.72),
        Shape("F04", "start", 4.0, "F", "Конец", 0.9, 0.45),
        Shape("E02", "process", 2.0, "E", "Создать резервную копию", 1.20, 0.65),
        Shape("F02", "decision", 2.0, "F", "Backup создан?", 1.05, 0.72),
        Shape("G04", "io", 4.0, "G", "Вернуть ошибку backup", 1.20, 0.72),
        Shape("H04", "start", 4.0, "H", "Конец", 0.9, 0.45),
        Shape("G02", "decision", 2.0, "G", "Цель редактирования?", 1.15, 0.75),
        Shape("B06", "predef", 6.0, "B", "Проверить запрос superblock", 1.35, 0.68),
        Shape("C06", "process", 6.0, "C", "Прочитать raw superblock", 1.30, 0.65),
        Shape("D06", "process", 6.0, "D", "Изменить разрешённые поля", 1.30, 0.70),
        Shape("E06", "process", 6.0, "E", "Записать superblock", 1.25, 0.65),
        Shape("F06", "process", 6.0, "F", "Повторно считать superblock", 1.32, 0.65),
        Shape("G06", "decision", 6.0, "G", "Значения совпали?", 1.12, 0.75),
        Shape("B09", "predef", 9.0, "B", "Проверить запрос inode", 1.35, 0.68),
        Shape("C09", "process", 9.0, "C", "Записать поля inode", 1.25, 0.65),
        Shape("D09", "process", 9.0, "D", "Повторно считать inode", 1.25, 0.65),
        Shape("E09", "decision", 9.0, "E", "Значения совпали?", 1.12, 0.75),
        Shape("I06", "io", 6.0, "I", "Вернуть ошибку верификации", 1.25, 0.72),
        Shape("F09", "io", 9.0, "F", "Вернуть ошибку верификации", 1.25, 0.72),
        Shape("I08", "process", 8.0, "I", "result.success = true", 1.20, 0.65),
        Shape("J08", "io", 8.0, "J", "Вернуть успешный результат", 1.25, 0.72),
        Shape("K08", "start", 8.0, "K", "Конец", 0.9, 0.45),
    ]:
        fc.add(s)
    for e in [
        ("A02", "B02", None, (0, 0)),
        ("B02", "C02", None, (0, 0)),
        ("C02", "C04", "нет", (0, 0.15)),
        ("C04", "D04", None, (0, 0)),
        ("C02", "D02", "да", (0.25, 0)),
        ("D02", "E04", "нет", (0, 0.15)),
        ("E04", "F04", None, (0, 0)),
        ("D02", "E02", "да", (0.25, 0)),
        ("E02", "F02", None, (0, 0)),
        ("F02", "G04", "нет", (0, 0.15)),
        ("G04", "H04", None, (0, 0)),
        ("F02", "G02", "да", (0.25, 0)),
        ("G02", "B06", "super", (0.1, 0.15)),
        ("B06", "C06", None, (0, 0)),
        ("C06", "D06", None, (0, 0)),
        ("D06", "E06", None, (0, 0)),
        ("E06", "F06", None, (0, 0)),
        ("F06", "G06", None, (0, 0)),
        ("G06", "I06", "нет", (-0.25, 0)),
        ("G06", "I08", "да", (0.1, 0.1)),
        ("G02", "B09", "inode", (0.1, -0.1)),
        ("B09", "C09", None, (0, 0)),
        ("C09", "D09", None, (0, 0)),
        ("D09", "E09", None, (0, 0)),
        ("E09", "F09", "нет", (0.2, 0)),
        ("E09", "I08", "да", (-0.2, 0.1)),
        ("I08", "J08", None, (0, 0)),
        ("J08", "K08", None, (0, 0)),
    ]:
        fc.edge(*e)
    return fc


def main() -> None:
    for chart in [build_lookup_path(), build_find_inode(), build_metadata_editor()]:
        chart.render()


if __name__ == "__main__":
    main()
