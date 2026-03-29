# BSUIR Semester 4

Репозиторий с материалами 4 семестра: лабораторными, исходным кодом, отчётами, листингами и курсовой работой.

Основная часть кода находится в каталоге `semester-4`, а внутри него материалы разбиты по дисциплинам.

## Структура

```text
semester-4/
├── APK/
├── ASM/
├── java/
├── osisp/
└── stoler/
```

### Что где лежит

- `semester-4/APK` - лабораторные по АПК, в основном исходники и отчёты.
- `semester-4/ASM` - материалы по ассемблеру, в основном готовые документы.
- `semester-4/java` - отчёты и материалы по Java.
- `semester-4/osisp` - самый насыщенный раздел: лабораторные на C, документация, листинги и курсовой проект.
- `semester-4/stoler` - документы по смежным дисциплинам, в основном `.docx`.

## Основной раздел: OSISP

Каталог [`semester-4/osisp`](./semester-4/osisp) содержит:

- `lab-1-osisp` ... `lab-8-osisp` - лабораторные работы.
- `course-work/editor-for-ext4-filesystem` - курсовой проект `ext4tool`.
- `docs/reports` - собранные отчёты в `pdf/docx`.
- `LISTING` - листинги исходного кода.
- `quick_start.md` - краткий сценарий запуска и демонстрации всех лабораторных.

Если нужно быстро понять, как запускать лабораторные, начинайте с файла [`semester-4/osisp/quick_start.md`](./semester-4/osisp/quick_start.md).

Если интересует курсовой проект, полезные точки входа:

- [`semester-4/osisp/course-work/PROJECT_OVERVIEW.md`](./semester-4/osisp/course-work/PROJECT_OVERVIEW.md)
- [`semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md`](./semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md)

## Быстрый старт

### Навигация по репозиторию

```bash
cd semester-4
ls
```

### Пример: открыть раздел OSISP

```bash
cd semester-4/osisp
ls
```

### Пример: собрать одну из лабораторных OSISP

```bash
cd semester-4/osisp/lab-4-osisp
make
./build/debug/semaphores
```

### Пример: открыть quick start по OSISP

```bash
sed -n '1,120p' semester-4/osisp/quick_start.md
```

## Требования

В репозитории нет единой системы сборки для всех предметов сразу. Нужные инструменты зависят от конкретного раздела.

Чаще всего используются:

- `gcc`
- `make`
- POSIX-окружение
- `pthread` для части лабораторных OSISP
- `ncurses` для курсового проекта `ext4tool`

Для разделов с отчётами отдельная сборка не нужна: там лежат готовые `.docx` и `.pdf`.

## Что стоит учитывать

- Это учебный репозиторий, а не единый production-проект.
- В разных каталогах встречаются как исходники, так и уже собранные артефакты, отчёты и вспомогательные файлы.
- Некоторые инструкции внутри вложенных файлов могут быть написаны относительно каталога конкретной лабораторной, а не от корня репозитория.
- Для запуска команд из `semester-4/osisp/quick_start.md` от корня репозитория обычно нужно просто добавить префикс `semester-4/`.

## Полезные точки входа

- [`README.md`](./README.md) - обзор всего репозитория.
- [`semester-4/osisp/quick_start.md`](./semester-4/osisp/quick_start.md) - запуск и демонстрация лабораторных OSISP.
- [`semester-4/osisp/course-work/PROJECT_OVERVIEW.md`](./semester-4/osisp/course-work/PROJECT_OVERVIEW.md) - обзор курсового проекта.
- [`semester-4/osisp/docs/reports`](./semester-4/osisp/docs/reports) - готовые отчёты.
