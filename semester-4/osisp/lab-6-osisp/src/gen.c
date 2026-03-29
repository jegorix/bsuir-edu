#define _POSIX_C_SOURCE 200809L

#include "index.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void print_usage(const char* program_name)
{
    fprintf(stderr, "Usage: %s filename records\n", program_name);
    fprintf(stderr, "records must be a positive number divisible by 256.\n");
}

int main(int argc, char* argv[])
{
    char* end = NULL;
    uint64_t records;
    unsigned int seed;
    FILE* file;

    if (argc != 3)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    records = strtoull(argv[2], &end, 10);
    if (argv[2][0] == '\0' || end == NULL || *end != '\0' || records == 0 || (records % 256U) != 0)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    file = fopen(argv[1], "wb");
    if (file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    if (fwrite(&records, sizeof(records), 1, file) != 1)
    {
        perror("fwrite");
        fclose(file);
        return EXIT_FAILURE;
    }

    seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    for (uint64_t index = 0; index < records; ++index)
    {
        IndexRecord record;

        record.time_mark = generate_random_time_mark(&seed);
        record.recno = index + 1U;
        if (fwrite(&record, sizeof(record), 1, file) != 1)
        {
            perror("fwrite");
            fclose(file);
            return EXIT_FAILURE;
        }
    }

    if (fclose(file) != 0)
    {
        perror("fclose");
        return EXIT_FAILURE;
    }

    printf("Generated \"%s\" with %" PRIu64 " records.\n", argv[1], records);
    return EXIT_SUCCESS;
}
