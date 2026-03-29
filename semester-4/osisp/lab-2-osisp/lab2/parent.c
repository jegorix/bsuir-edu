#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <locale.h>

extern char **environ;
void print_sorted_environ(void);
char **create_child_env_from_file(const char *filename, int *size);
void free_child_env(char **env_array, int size);
static int cmp(const void *a, const void *b);

int main(void)
{
    setlocale(LC_COLLATE, "C");

    print_sorted_environ();

    int child_env_size;
    char **child_env = create_child_env_from_file("env", &child_env_size);

    char *child_dir = getenv("CHILD_PATH");
    char child_path[512];

    if (child_dir)
    {
        snprintf(child_path, sizeof(child_path), "%s/child", child_dir);
    }
    else
    {
        snprintf(child_path, sizeof(child_path), "./child");
    }

    int child_counter = 0;
    int c;

    printf("Enter commands: '+' for child (environ), '*' for child (envp), 'q' to quit.\n");

    while ((c = getchar()) != EOF)
    {
        if (c == 'q')
        {
            break;
        }

        if (c != '+' && c != '*')
        {
            continue;
        }

        char prog_name[16];
        snprintf(prog_name, sizeof(prog_name), "child_%02d", child_counter);
        child_counter = (child_counter + 1) % 100;

        char *child_argv[] = {prog_name, (c == '+') ? "+" : "*", NULL};

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            continue;
        }

        if (pid == 0)
        {
            execve(child_path, child_argv, child_env);
            perror("execve");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("Spawned child with PID %d\n", pid);
        }
    }

    free_child_env(child_env, child_env_size);

    return 0;
}

void print_sorted_environ(void)
{
    int count = 0;
    while (environ[count] != NULL)
    {
        count++;
    }

    char **sorted = malloc((count + 1) * sizeof(char *));

    if (!sorted)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < count; i++)
    {
        sorted[i] = environ[i];
    }

    sorted[count] = NULL;

    qsort(sorted, count, sizeof(char *), cmp);

    printf("Parent environment (sorted by LC_COLLATE=C):\n");
    for (int i = 0; i < count; i++)
    {
        printf("%s\n", sorted[i]);
    }

    printf("\n");

    free(sorted);
}

static int cmp(const void *a, const void *b)
{
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcoll(sa, sb);
}

char **create_child_env_from_file(const char *filename, int *size)
{
    FILE *f = fopen(filename, "r");

    if (!f)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char **env_array = NULL;
    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        char *value = getenv(line);
        if (value)
        {
            char *entry;
            if (asprintf(&entry, "%s=%s", line, value) == -1)
            {
                perror("asprintf");
                exit(EXIT_FAILURE);
            }

            char **new_array = realloc(env_array, (count + 2) * sizeof(char *));
            if (!new_array)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }

            env_array = new_array;
            env_array[count] = entry;
            count++;
        }
    }

    fclose(f);

    char **new_array = realloc(env_array, (count + 1) * sizeof(char *));
    if (!new_array)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    env_array = new_array;
    env_array[count] = NULL;
    *size = count;

    return env_array;
}

void free_child_env(char **env_array, int size)
{
    for (int i = 0; i < size; i++)
    {
        free(env_array[i]);
    }

    free(env_array);
}
