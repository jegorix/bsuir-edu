// Подключение стандартных заголовочных файлов
#include <stdio.h>   // для printf, fprintf, stderr
#include <stdlib.h>  // для EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>  // для strcmp
#include <unistd.h>  // для getpid, getppid, sleep

// Объявление глобальной переменной environ (массив строк окружения)
extern char **environ;

// Точка входа в программу: argc - количество аргументов, argv - массив аргументов,
// envp - массив строк окружения, переданный от execve (третий аргумент)
int main(int argc, char *argv[], char *envp[])
{
    // Проверка: должен быть передан хотя бы один аргумент (кроме имени программы)
    if (argc < 2)
    {
        // Выводим сообщение об ошибке в stderr, показывая правильное использование
        fprintf(stderr, "Usage: %s <+ or *>\n", argv[0]);
        // Завершаем программу с кодом ошибки
        return EXIT_FAILURE;
    }

    // Выводим имя потомка (argv[0] установлено родителем), PID и PPID
    printf("Child name: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    // Если первый аргумент равен "+"
    if (strcmp(argv[1], "+") == 0)
    {
        // Выводим заголовок
        printf("Environment (via environ):\n");
        // Проходим по массиву environ (глобальная переменная) до NULL
        for (char **env = environ; *env != NULL; env++)
        {
            // Выводим каждую строку окружения
            printf("%s\n", *env);
        }
    }
    // Иначе если первый аргумент равен "*"
    else if (strcmp(argv[1], "*") == 0)
    {
        // Выводим заголовок
        printf("Environment (via envp):\n");
        // Проходим по массиву envp (третий параметр main) до NULL
        for (char **env = envp; *env != NULL; env++)
        {
            // Выводим каждую строку окружения
            printf("%s\n", *env);
        }
    }
    // Если аргумент не "+" и не "*"
    else
    {
        // Сообщаем об ошибке
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        // Завершаем с ошибкой
        return EXIT_FAILURE;
    }

    // Приостанавливаем выполнение на 5 секунд (чтобы родитель успел выполнить wait)
    sleep(5);
    // Успешное завершение
    return 0;
}