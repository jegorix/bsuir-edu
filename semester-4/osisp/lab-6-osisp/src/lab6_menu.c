#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "index.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

enum
{
    MAX_THREADS_PER_CORE = 8
};

static size_t get_cpu_cores(void)
{
#if defined(__APPLE__)
    int cores = 1;
    size_t size = sizeof(cores);

    if (sysctlbyname("hw.logicalcpu", &cores, &size, NULL, 0) != 0 || cores <= 0)
    {
        return 1U;
    }
    return (size_t)cores;
#else
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores <= 0)
    {
        return 1U;
    }
    return (size_t)cores;
#endif
}

static size_t get_page_size_value(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0)
    {
        return 4096U;
    }
    return (size_t)page_size;
}

static size_t next_power_of_two(size_t value)
{
    size_t result = 1U;

    while (result < value)
    {
        result <<= 1U;
    }
    return result;
}

static int is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

static void strip_newline(char* line)
{
    size_t length = strlen(line);

    if (length > 0U && line[length - 1U] == '\n')
    {
        line[length - 1U] = '\0';
    }
}

static int read_line(const char* prompt, char* buffer, size_t buffer_size)
{
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(buffer, (int)buffer_size, stdin) == NULL)
    {
        return 0;
    }

    strip_newline(buffer);
    return 1;
}

static int prompt_string(const char* label, const char* default_value, char* output, size_t output_size)
{
    char line[PATH_MAX];
    char prompt[PATH_MAX + 64];

    for (;;)
    {
        snprintf(prompt, sizeof(prompt), "%s [%s]: ", label, default_value);
        if (!read_line(prompt, line, sizeof(line)))
        {
            return 0;
        }

        if (line[0] == '\0')
        {
            if (snprintf(output, output_size, "%s", default_value) >= (int)output_size)
            {
                fprintf(stderr, "Path is too long.\n");
                continue;
            }
            return 1;
        }

        if (snprintf(output, output_size, "%s", line) >= (int)output_size)
        {
            fprintf(stderr, "Path is too long.\n");
            continue;
        }
        return 1;
    }
}

static int prompt_size_value(const char* label,
                             size_t default_value,
                             size_t* output,
                             int (*validator)(size_t, void*),
                             void* context,
                             const char* error_text)
{
    char line[128];
    char prompt[160];

    for (;;)
    {
        char* end = NULL;
        unsigned long long parsed = 0ULL;

        snprintf(prompt, sizeof(prompt), "%s [%zu]: ", label, default_value);
        if (!read_line(prompt, line, sizeof(line)))
        {
            return 0;
        }

        if (line[0] == '\0')
        {
            *output = default_value;
            return 1;
        }

        errno = 0;
        parsed = strtoull(line, &end, 10);
        if (errno != 0 || line[0] == '\0' || end == NULL || *end != '\0')
        {
            fprintf(stderr, "Enter a positive integer.\n");
            continue;
        }

        *output = (size_t)parsed;
        if (validator == NULL || validator(*output, context))
        {
            return 1;
        }

        fprintf(stderr, "%s\n", error_text);
    }
}

static int validate_records(size_t value, void* context)
{
    (void)context;
    return value > 0U && (value % 256U) == 0U;
}

static int validate_memsize(size_t value, void* context)
{
    size_t page_size = *(const size_t*)context;

    return value > 0U && (value % page_size) == 0U && (value % sizeof(IndexRecord)) == 0U;
}

typedef struct
{
    size_t min_threads;
    size_t max_threads;
} ThreadRange;

static int validate_blocks(size_t value, void* context)
{
    const ThreadRange* range = (const ThreadRange*)context;

    return is_power_of_two(value) && value >= range->min_threads * 4U;
}

static int validate_threads(size_t value, void* context)
{
    const ThreadRange* range = (const ThreadRange*)context;

    return value >= range->min_threads && value <= range->max_threads;
}

static int get_executable_directory(char* buffer, size_t buffer_size)
{
#if defined(__APPLE__)
    uint32_t size = (uint32_t)buffer_size;
    char resolved[PATH_MAX];

    if (_NSGetExecutablePath(buffer, &size) != 0)
    {
        return -1;
    }
    if (realpath(buffer, resolved) == NULL)
    {
        return -1;
    }
    if (snprintf(buffer, buffer_size, "%s", resolved) >= (int)buffer_size)
    {
        return -1;
    }
#else
    ssize_t length = readlink("/proc/self/exe", buffer, buffer_size - 1U);

    if (length < 0)
    {
        return -1;
    }
    buffer[length] = '\0';
#endif

    {
        char* slash = strrchr(buffer, '/');

        if (slash == NULL)
        {
            return -1;
        }
        *slash = '\0';
    }

    return 0;
}

static int derive_project_root(const char* executable_dir, char* project_root, size_t project_root_size)
{
    char* slash = NULL;

    if (snprintf(project_root, project_root_size, "%s", executable_dir) >= (int)project_root_size)
    {
        return -1;
    }

    slash = strrchr(project_root, '/');
    if (slash == NULL)
    {
        return -1;
    }
    *slash = '\0';

    slash = strrchr(project_root, '/');
    if (slash == NULL)
    {
        return -1;
    }
    *slash = '\0';

    return 0;
}

static int run_program(const char* executable_dir, const char* program_name, char* const argv[])
{
    char program_path[PATH_MAX];
    pid_t pid;
    int status = 0;

    if (snprintf(program_path, sizeof(program_path), "%s/%s", executable_dir, program_name) >= (int)sizeof(program_path))
    {
        fprintf(stderr, "Program path is too long.\n");
        return -1;
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        execv(program_path, argv);
        perror("execv");
        _exit(EXIT_FAILURE);
    }

    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
    {
        if (WEXITSTATUS(status) == 0)
        {
            return 0;
        }

        fprintf(stderr, "\"%s\" exited with code %d.\n", program_name, WEXITSTATUS(status));
        return -1;
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr, "\"%s\" was terminated by signal %d.\n", program_name, WTERMSIG(status));
    }
    return -1;
}

static int run_generate_menu(const char* executable_dir)
{
    char filename[PATH_MAX];
    char records_text[32];
    char* argv[] = {NULL, filename, records_text, NULL};
    size_t records = 4096U;

    printf("\n[Generate]\n");
    printf("records must be positive and divisible by 256.\n");

    if (!prompt_string("Filename", "sample.bin", filename, sizeof(filename)))
    {
        return -1;
    }
    if (!prompt_size_value("Records",
                           4096U,
                           &records,
                           validate_records,
                           NULL,
                           "records must be positive and divisible by 256."))
    {
        return -1;
    }

    snprintf(records_text, sizeof(records_text), "%zu", records);
    argv[0] = "gen";
    return run_program(executable_dir, "gen", argv);
}

static int run_view_menu(const char* executable_dir)
{
    char filename[PATH_MAX];
    char mode[32];
    char* argv[] = {NULL, filename, mode, NULL};

    printf("\n[View]\n");

    if (!prompt_string("Filename", "sample.bin", filename, sizeof(filename)))
    {
        return -1;
    }
    for (;;)
    {
        char line[32];

        printf("View mode:\n");
        printf("  [1] show all\n");
        printf("  [2] show first 10\n");
        printf("  [3] show last 10\n");
        if (!read_line("Mode [1]: ", line, sizeof(line)))
        {
            return -1;
        }

        if (line[0] == '\0' || strcmp(line, "1") == 0)
        {
            snprintf(mode, sizeof(mode), "all");
            break;
        }
        if (strcmp(line, "2") == 0)
        {
            snprintf(mode, sizeof(mode), "first10");
            break;
        }
        if (strcmp(line, "3") == 0)
        {
            snprintf(mode, sizeof(mode), "last10");
            break;
        }

        fprintf(stderr, "Unknown mode. Use 1, 2 or 3.\n");
    }

    argv[0] = "view";
    return run_program(executable_dir, "view", argv);
}

static int run_sort_menu(const char* executable_dir, size_t page_size, size_t cores)
{
    char filename[PATH_MAX];
    char memsize_text[32];
    char blocks_text[32];
    char threads_text[32];
    char* argv[] = {NULL, memsize_text, blocks_text, threads_text, filename, NULL};
    ThreadRange range;
    size_t mem_size = page_size;
    size_t blocks = 0;
    size_t threads = cores;

    range.min_threads = cores;
    range.max_threads = cores * MAX_THREADS_PER_CORE;
    blocks = next_power_of_two(range.min_threads * 4U);

    printf("\n[Sort]\n");
    printf("page size: %zu, logical cores: %zu, allowed threads: [%zu, %zu]\n",
           page_size,
           cores,
           range.min_threads,
           range.max_threads);
    printf("Requirements: memsize %% page_size == 0, blocks is power of two, blocks >= 4 * threads.\n");

    if (!prompt_string("Filename", "sample.bin", filename, sizeof(filename)))
    {
        return -1;
    }
    if (!prompt_size_value("Memsize",
                           page_size,
                           &mem_size,
                           validate_memsize,
                           &page_size,
                           "memsize must be positive, divisible by page size and fit whole records."))
    {
        return -1;
    }
    if (!prompt_size_value("Blocks",
                           blocks,
                           &blocks,
                           validate_blocks,
                           &range,
                           "blocks must be a power of two and at least 4 * threads_min."))
    {
        return -1;
    }
    if (!prompt_size_value("Threads",
                           threads,
                           &threads,
                           validate_threads,
                           &range,
                           "threads must be within the allowed range."))
    {
        return -1;
    }

    if (blocks < threads * 4U)
    {
        fprintf(stderr, "blocks must be at least 4 * threads.\n");
        return -1;
    }
    if ((mem_size % blocks) != 0U || ((mem_size / blocks) % sizeof(IndexRecord)) != 0U)
    {
        fprintf(stderr, "memsize/blocks must contain a whole number of IndexRecord entries.\n");
        return -1;
    }

    snprintf(memsize_text, sizeof(memsize_text), "%zu", mem_size);
    snprintf(blocks_text, sizeof(blocks_text), "%zu", blocks);
    snprintf(threads_text, sizeof(threads_text), "%zu", threads);
    argv[0] = "sort_index";

    return run_program(executable_dir, "sort_index", argv);
}

static void print_menu(size_t page_size, size_t cores)
{
    printf("\n+===============================================================+\n");
    printf("|                       LAB 6 CONSOLE MENU                     |\n");
    printf("+---------------------------------------------------------------+\n");
    printf("| Tools: gen / view / sort_index                               |\n");
    printf("| Page size: %-10zu Cores: %-10zu                         |\n", page_size, cores);
    printf("+---------------------------------------------------------------+\n");
    printf("| [1] Generate index file                                       |\n");
    printf("| [2] View index file                                           |\n");
    printf("| [3] Sort index file                                           |\n");
    printf("| [q] Exit                                                      |\n");
    printf("+---------------------------------------------------------------+\n");
}

int main(void)
{
    char executable_dir[PATH_MAX];
    char project_root[PATH_MAX];
    char line[32];
    size_t page_size = get_page_size_value();
    size_t cores = get_cpu_cores();

    if (get_executable_directory(executable_dir, sizeof(executable_dir)) != 0)
    {
        fprintf(stderr, "Failed to resolve executable directory.\n");
        return EXIT_FAILURE;
    }
    if (derive_project_root(executable_dir, project_root, sizeof(project_root)) != 0)
    {
        fprintf(stderr, "Failed to resolve project root.\n");
        return EXIT_FAILURE;
    }
    if (chdir(project_root) != 0)
    {
        perror("chdir");
        return EXIT_FAILURE;
    }

    for (;;)
    {
        print_menu(page_size, cores);
        if (!read_line("[Select] > ", line, sizeof(line)))
        {
            break;
        }

        switch (line[0])
        {
            case '1':
                run_generate_menu(executable_dir);
                break;

            case '2':
                run_view_menu(executable_dir);
                break;

            case '3':
                run_sort_menu(executable_dir, page_size, cores);
                break;

            case 'q':
            case 'Q':
                return EXIT_SUCCESS;

            default:
                printf("Unknown option. Use 1, 2, 3 or q.\n");
                break;
        }
    }

    return EXIT_SUCCESS;
}
