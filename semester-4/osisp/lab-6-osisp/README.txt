ЗАДАНИЕ

Лабораторная работа 6 посвящена кооперации потоков для высокопроизводительной обработки больших файлов с использованием `mmap()` и барьеров.

ПРОГРАММЫ

В проекте собираются три утилиты:
- `gen` — генерация индексного файла
- `view` — просмотр индексного файла
- `sort_index` — многопоточная сортировка индексного файла

Дополнительно собирается консольное меню:
- `lab6_menu` — единая интерактивная оболочка для запуска `gen`, `view`, `sort_index`

СБОРКА

Перейти в каталог проекта:
`cd /Users/macbook/Documents/bsuir/osisp/lab-6-osisp`

Собрать debug:
`make`

Собрать release:
`make MODE=release`

Очистить сборку:
`make clean`

ЗАПУСК

Консольное меню:
`./build/debug/lab6_menu`

Генерация файла:
`./build/debug/gen filename records`

Пример:
`./build/debug/gen sample.bin 4096`

Требования к `records`:
- положительное число
- кратно `256`

Просмотр файла:
`./build/debug/view filename [all|first10|last10]`

Пример:
`./build/debug/view sample.bin`

Примеры режимов:
- `./build/debug/view sample.bin all`
- `./build/debug/view sample.bin first10`
- `./build/debug/view sample.bin last10`

Сортировка файла:
`./build/debug/sort_index memsize blocks threads filename`

Пример:
`./build/debug/sort_index 16384 32 8 sample.bin`

Требования к параметрам:
- `memsize` — положительное число, кратное размеру страницы
- `blocks` — степень двойки
- `blocks >= 4 * threads`
- `threads` — от количества ядер до `8 * cores`
- `memsize / blocks` должно содержать целое число записей `IndexRecord`
- размер индексных данных в файле должен быть кратен `memsize`

ТЕСТИРОВАНИЕ

Команды ниже предполагают, что текущий каталог уже:
`/Users/macbook/Documents/bsuir/osisp/lab-6-osisp`

Минимально корректный сценарий:
`./build/debug/gen test-1024.bin 1024`
`./build/debug/sort_index 16384 32 8 test-1024.bin`
`./build/debug/view test-1024.bin first10`

Рабочий сценарий нормального размера:
`./build/debug/gen test-65536.bin 65536`
`./build/debug/sort_index 16384 32 8 test-65536.bin`
`./build/debug/view test-65536.bin first10`
`./build/debug/view test-65536.bin last10`

Проверка, что файл действительно отсортирован по `time_mark`:
`./build/debug/view test-65536.bin | tail -n +4 | awk '{print $1}' | sort -c`

Если сортировка корректна, последняя команда не должна ничего выводить и должна завершаться успешно.

Негативный сценарий:
`./build/debug/gen test-512.bin 512`
`./build/debug/sort_index 16384 32 8 test-512.bin`

В этом случае сортировка должна завершиться ошибкой, потому что `512` записей недостаточно для `memsize = 16384`.

ФОРМАТ ФАЙЛА

Файл состоит из:
- `uint64_t records`
- массива записей `IndexRecord`

Структура записи:
- `double time_mark`
- `uint64_t recno`
