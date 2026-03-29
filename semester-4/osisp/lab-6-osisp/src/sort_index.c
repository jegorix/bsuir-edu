#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "index.h"
#include "sort.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

enum
{
    MAX_THREADS_PER_CORE = 8
};

static int is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1U)) == 0;
}

static size_t get_cpu_cores(void)
{
#if defined(__APPLE__)
    int cores = 1;
    size_t size = sizeof(cores);

    if (sysctlbyname("hw.logicalcpu", &cores, &size, NULL, 0) != 0 || cores <= 0)
    {
        return 1;
    }
    return (size_t)cores;
#else
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores <= 0)
    {
        return 1;
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

static void print_usage(const char* program_name, size_t page_size, size_t min_threads, size_t max_threads)
{
    fprintf(stderr, "Usage: %s memsize blocks threads filename\n", program_name);
    fprintf(stderr, "memsize must be positive and divisible by page size (%zu).\n", page_size);
    fprintf(stderr, "blocks must be a power of two and at least 4 * threads.\n");
    fprintf(stderr, "threads must be in range [%zu, %zu].\n", min_threads, max_threads);
}

int main(int argc, char* argv[])
{
    char* end = NULL;
    size_t mem_size;
    size_t blocks;
    size_t threads;
    size_t min_threads = get_cpu_cores();
    size_t max_threads = min_threads * MAX_THREADS_PER_CORE;
    size_t page_size = get_page_size_value();

    if (argc != 5)
    {
        print_usage(argv[0], page_size, min_threads, max_threads);
        return EXIT_FAILURE;
    }

    mem_size = strtoull(argv[1], &end, 10);
    if (argv[1][0] == '\0' || end == NULL || *end != '\0' || mem_size == 0 || (mem_size % page_size) != 0)
    {
        print_usage(argv[0], page_size, min_threads, max_threads);
        return EXIT_FAILURE;
    }

    blocks = strtoull(argv[2], &end, 10);
    if (argv[2][0] == '\0' || end == NULL || *end != '\0' || !is_power_of_two(blocks))
    {
        print_usage(argv[0], page_size, min_threads, max_threads);
        return EXIT_FAILURE;
    }

    threads = strtoull(argv[3], &end, 10);
    if (argv[3][0] == '\0' || end == NULL || *end != '\0' || threads < min_threads || threads > max_threads)
    {
        print_usage(argv[0], page_size, min_threads, max_threads);
        return EXIT_FAILURE;
    }

    if (blocks < threads * 4U)
    {
        print_usage(argv[0], page_size, min_threads, max_threads);
        return EXIT_FAILURE;
    }
    if ((mem_size % blocks) != 0 || ((mem_size / blocks) % sizeof(IndexRecord)) != 0)
    {
        fprintf(stderr, "memsize/blocks must contain a whole number of records.\n");
        return EXIT_FAILURE;
    }

    if (sort_index_file(mem_size, blocks, threads, argv[4]) != 0)
    {
        return EXIT_FAILURE;
    }

    printf("Sorted \"%s\" successfully.\n", argv[4]);
    return EXIT_SUCCESS;
}
