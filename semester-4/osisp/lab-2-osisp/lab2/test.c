// Подключение стандартных заголовочных файлов
#include <stdio.h>   // для printf, perror, fopen, fgets, etc.
#include <stdlib.h>  // для malloc, free, qsort, exit, getenv, etc.
#include <unistd.h>  // для fork, execve, getenv, etc.
#include <string.h>  // для strcspn, strlen, etc.
#include <sys/wait.h> // для wait
#include <locale.h>   // для setlocale

// Глобальная переменная окружения, определена в libc
extern char **environ;

// Прототипы функций
void print_sorted_environ(void);                       // вывод отсортированного окружения родителя
char **create_child_env_from_file(const char *filename, int *size); // создание окружения для потомка из файла
void free_child_env(char **env_array, int size);       // освобождение памяти, выделенной под окружение потомка
static int cmp(const void *a, const void *b);          // функция сравнения для qsort

int main(void)
{
    // Устанавливаем локаль для сортировки (LC_COLLATE=C), чтобы strcoll работала как strcmp
    setlocale(LC_COLLATE, "C");

    // Выводим отсортированное окружение родительского процесса
    print_sorted_environ();

    // Переменная для хранения размера создаваемого окружения
    int child_env_size;
    // Создаём окружение для потомка на основе файла "env"
    char **child_env = create_child_env_from_file("env", &child_env_size);

    // Пытаемся получить переменную окружения CHILD_PATH, чтобы определить путь к исполняемому файлу child
    char *child_dir = getenv("CHILD_PATH");
    char child_path[512]; // буфер для полного пути

    if (child_dir)
    {
        // Если CHILD_PATH задан, формируем путь: <CHILD_PATH>/child
        snprintf(child_path, sizeof(child_path), "%s/child", child_dir);
    }
    else
    {
        // Иначе используем "./child" (текущая директория)
        snprintf(child_path, sizeof(child_path), "./child");
    }

    // Счётчик для уникальных имён потомков (используется в argv[0])
    int child_counter = 0;
    int c; // для чтения символов из stdin

    // Выводим подсказку пользователю
    printf("Enter commands: '+' for child (environ), '*' for child (envp), 'q' to quit.\n");

    // Цикл чтения команд до конца файла или ввода 'q'
    while ((c = getchar()) != EOF)
    {
        if (c == 'q')
        {
            break; // выход из цикла по запросу
        }

        // Игнорируем все символы, кроме '+' и '*'
        if (c != '+' && c != '*')
        {
            continue;
        }

        // Формируем имя программы (argv[0]) вида child_00, child_01, ... (циклически 0-99)
        char prog_name[16];
        snprintf(prog_name, sizeof(prog_name), "child_%02d", child_counter);
        child_counter = (child_counter + 1) % 100; // увеличиваем счётчик по модулю 100

        // Аргументы для execve:
        // - argv[0] = prog_name (имя программы)
        // - argv[1] = "+" или "*" (передаём символ, чтобы потомок знал, какое окружение использовать)
        // - argv[2] = NULL (завершающий NULL)
        char *child_argv[] = {prog_name, (c == '+') ? "+" : "*", NULL};

        // Создаём новый процесс
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            continue; // при ошибке пропускаем и продолжаем
        }

        if (pid == 0)
        {
            // Код потомка: заменяем образ процесса на child_path с переданными аргументами и окружением child_env
            execve(child_path, child_argv, child_env);
            // Если execve вернулся, значит произошла ошибка
            perror("execve");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Родитель: выводим PID созданного потомка
            printf("Spawned child with PID %d\n", pid);
        }
    }

    // Ожидаем завершения всех дочерних процессов
    while (wait(NULL) > 0)
    {
        // пустой цикл: wait возвращает PID завершённого потомка или -1, когда потомков нет
    }

    // Освобождаем память, выделенную под окружение потомка
    free_child_env(child_env, child_env_size);

    return 0;
}

// Функция вывода отсортированного окружения родителя
void print_sorted_environ(void)
{
    // Считаем количество строк в environ
    int count = 0;
    while (environ[count] != NULL)
    {
        count++;
    }

    // Выделяем память под массив указателей (count+1 элемент)
    char **sorted = malloc((count + 1) * sizeof(char *));

    if (!sorted)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Копируем указатели из environ в sorted
    for (int i = 0; i < count; i++)
    {
        sorted[i] = environ[i];
    }
    sorted[count] = NULL; // завершающий NULL

    // Сортируем массив с помощью qsort, используя функцию cmp (strcoll)
    qsort(sorted, count, sizeof(char *), cmp);

    // Выводим отсортированные строки
    printf("Parent environment (sorted by LC_COLLATE=C):\n");
    for (int i = 0; i < count; i++)
    {
        printf("%s\n", sorted[i]);
    }
    printf("\n");

    // Освобождаем временный массив
    free(sorted);
}

// Функция сравнения для qsort; ожидает указатели на указатели на char
static int cmp(const void *a, const void *b)
{
    const char *sa = *(const char * const *)a; // первый элемент
    const char *sb = *(const char * const *)b; // второй элемент
    return strcoll(sa, sb); // сравнение с учётом локали
}

// Создаёт массив строк окружения (в формате "ИМЯ=ЗНАЧЕНИЕ") на основе файла.
// Файл содержит имена переменных окружения (по одному на строку).
// Для каждого имени из файла получаем текущее значение через getenv.
// Если переменная существует, добавляем запись "ИМЯ=ЗНАЧЕНИЕ" в результирующий массив.
char **create_child_env_from_file(const char *filename, int *size)
{
    // Открываем файл для чтения
    FILE *f = fopen(filename, "r");

    if (!f)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char **env_array = NULL; // массив для хранения записей
    int count = 0;           // количество добавленных записей
    char line[256];          // буфер для чтения строк

    // Читаем файл построчно
    while (fgets(line, sizeof(line), f))
    {
        // Удаляем символы перевода строки (\r, \n)
        line[strcspn(line, "\r\n")] = '\0';
        // Пропускаем пустые строки
        if (line[0] == '\0')
        {
            continue;
        }

        // Получаем значение переменной окружения с именем line
        char *value = getenv(line);
        if (value)
        {
            // Формируем строку "ИМЯ=ЗНАЧЕНИЕ" с помощью asprintf
            char *entry;
            if (asprintf(&entry, "%s=%s", line, value) == -1)
            {
                perror("asprintf");
                exit(EXIT_FAILURE);
            }

            // Увеличиваем размер массива на одну запись (плюс будущий NULL)
            char **new_array = realloc(env_array, (count + 2) * sizeof(char *));
            if (!new_array)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            env_array = new_array;
            env_array[count] = entry; // сохраняем строку
            count++;                  // увеличиваем счётчик
        }
    }

    fclose(f);

    // Добавляем завершающий NULL в конец массива
    char **new_array = realloc(env_array, (count + 1) * sizeof(char *));
    if (!new_array)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    env_array = new_array;
    env_array[count] = NULL; // завершающий NULL
    *size = count;           // возвращаем количество элементов через указатель

    return env_array;
}

// Освобождает память, выделенную под окружение потомка
void free_child_env(char **env_array, int size)
{
    // Освобождаем каждую строку "ИМЯ=ЗНАЧЕНИЕ"
    for (int i = 0; i < size; i++)
    {
        free(env_array[i]);
    }
    // Освобождаем сам массив указателей
    free(env_array);
}