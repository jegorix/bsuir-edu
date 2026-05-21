# BSUIR Semester 4

Учебный репозиторий с материалами 4 семестра БГУИР: лабораторными работами, исходным кодом, отчётами, листингами и курсовым проектом.

Основной рабочий каталог: [`semester-4`](./semester-4).

## Что внутри

```text
semester-4/
├── APK/
├── ASM/
├── infbez&ouis/
├── java/
├── marketing/
├── osisp/
└── schemota/
```

## Карта репозитория

### `APK`

Материалы по АПК: лабораторные по отдельным темам, исходники и связанные файлы по каталогам `lab1` ... `lab6`.

### `ASM`

Раздел с материалами по ассемблеру. В основном здесь лежат готовые документы и сопутствующие файлы.

### `infbez&ouis`

Документы по двум дисциплинам:

- `infbez` - практические и лабораторные по основам информационной безопасности.
- `ouis` - материалы по ОУИС.

Раздел в основном состоит из готовых `.docx`-файлов.

### `java`

Материалы и отчёты по Java.

### `marketing`

Готовая презентация по маркетингу + текст к презентации.

### `osisp`

Самый насыщенный раздел репозитория. Здесь собраны:

- лабораторные `lab-1-osisp` ... `lab-8-osisp`
- курсовая работа `course-work`
- готовые отчёты в `docs/reports`
- листинги в `LISTING`
- сценарий быстрого запуска в `quick_start.md`
- вспомогательные скрипты, включая генерацию listing-документов

Если нужен быстрый вход в раздел, начинайте с [`semester-4/osisp/quick_start.md`](./semester-4/osisp/quick_start.md).

### `schemota`

Отчёты и дополнительные материалы по схемотехнике: в корне лежат в основном готовые PDF, в `additional` - дополнительные `.docx` и `.pdf`.

## Главное в `osisp`

Ключевые точки входа:

- [`semester-4/osisp/quick_start.md`](./semester-4/osisp/quick_start.md) - короткий сценарий запуска и демонстрации всех лабораторных.
- [`semester-4/osisp/course-work/PROJECT_OVERVIEW.md`](./semester-4/osisp/course-work/PROJECT_OVERVIEW.md) - обзор курсовой работы.
- [`semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md`](./semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md) - README проекта `ext4tool`.
- [`semester-4/osisp/docs/reports`](./semester-4/osisp/docs/reports) - собранные отчёты в `.pdf` и `.docx`.
- [`semester-4/osisp/LISTING`](./semester-4/osisp/LISTING) - готовые листинги по лабораторным.

Примеры содержимого раздела:

- `lab-1-osisp/lab1` - рекурсивный обход каталогов
- `lab-4-osisp` - producer/consumer на процессах
- `lab-5-osisp` - producer/consumer на потоках
- `lab-6-osisp` - генерация, просмотр и сортировка файлов записей
- `lab-8-osisp` - TCP-сервер и клиент
- `course-work/editor-for-ext4-filesystem` - курсовой проект

## Быстрый старт

### Открыть структуру семестра

```bash
cd semester-4
ls
```

### Перейти в OSISP

```bash
cd semester-4/osisp
ls
```

### Собрать одну из лабораторных

```bash
cd semester-4/osisp/lab-4-osisp
make
./build/debug/semaphores
```

### Открыть сценарий запуска всех лабораторных

```bash
sed -n '1,160p' semester-4/osisp/quick_start.md
```

## Что важно учитывать

- Это не единый production-проект, а учебный архив по нескольким дисциплинам.
- В репозитории смешаны исходники, отчёты, листинги, PDF, DOCX и вспомогательные артефакты.
- Не для всех разделов нужна сборка: во многих каталогах уже лежат готовые документы.
- Инструкции внутри вложенных README и `.md` могут быть написаны относительно каталога конкретной лабораторной, а не от корня репозитория.
- Для части OSISP-лабораторных нужны `gcc`, `make`, POSIX-окружение, а для отдельных задач ещё `pthread` и `ncurses`.

## Полезные ссылки

- [`README.md`](./README.md) - обзор репозитория.
- [`semester-4/osisp/quick_start.md`](./semester-4/osisp/quick_start.md) - запуск лабораторных OSISP.
- [`semester-4/osisp/course-work/PROJECT_OVERVIEW.md`](./semester-4/osisp/course-work/PROJECT_OVERVIEW.md) - обзор курсовой.
- [`semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md`](./semester-4/osisp/course-work/editor-for-ext4-filesystem/README.md) - документация `ext4tool`.
- [`semester-4/osisp/docs/reports`](./semester-4/osisp/docs/reports) - готовые отчёты.
