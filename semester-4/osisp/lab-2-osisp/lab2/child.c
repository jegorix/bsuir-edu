#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char *argv[], char *envp[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <+ or *>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Child name: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    if (strcmp(argv[1], "+") == 0)
    {
        printf("Environment (via environ):\n");
        for (char **env = environ; *env != NULL; env++)
        {
            printf("%s\n", *env);
        }
    }

    else if (strcmp(argv[1], "*") == 0)
    {
        printf("Environment (via envp):\n");
        for (char **env = envp; *env != NULL; env++)
        {
            printf("%s\n", *env);
        }
    }

    else
    {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        return EXIT_FAILURE;
    }


    pid_t parent_pid = getppid();

    while (getppid() == parent_pid)
    {
        sleep(1);
    }

    return 0;
}
