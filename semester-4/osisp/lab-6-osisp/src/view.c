#include "index.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    VIEW_ALL = 0,
    VIEW_FIRST10 = 1,
    VIEW_LAST10 = 2
} ViewMode;

static void print_usage(const char* program_name)
{
    fprintf(stderr, "Usage: %s filename [all|first10|last10]\n", program_name);
}

static int parse_view_mode(const char* arg, ViewMode* mode)
{
    if (arg == NULL || strcmp(arg, "all") == 0)
    {
        *mode = VIEW_ALL;
        return 0;
    }
    if (strcmp(arg, "first10") == 0)
    {
        *mode = VIEW_FIRST10;
        return 0;
    }
    if (strcmp(arg, "last10") == 0)
    {
        *mode = VIEW_LAST10;
        return 0;
    }

    return -1;
}

int main(int argc, char* argv[])
{
    FILE* file;
    uint64_t records;
    uint64_t start_index = 0;
    uint64_t records_to_show = 0;
    ViewMode mode = VIEW_ALL;

    if (argc != 2 && argc != 3)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (parse_view_mode((argc == 3) ? argv[2] : NULL, &mode) != 0)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    file = fopen(argv[1], "rb");
    if (file == NULL)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    if (fread(&records, sizeof(records), 1, file) != 1)
    {
        fprintf(stderr, "Failed to read index header from \"%s\".\n", argv[1]);
        fclose(file);
        return EXIT_FAILURE;
    }

    printf("records: %" PRIu64 "\n", records);
    if (mode == VIEW_FIRST10)
    {
        records_to_show = (records < 10U) ? records : 10U;
        printf("mode: first %" PRIu64 " records\n", records_to_show);
    }
    else if (mode == VIEW_LAST10)
    {
        records_to_show = (records < 10U) ? records : 10U;
        start_index = (records > records_to_show) ? (records - records_to_show) : 0U;
        printf("mode: last %" PRIu64 " records\n", records_to_show);
    }
    else
    {
        records_to_show = records;
        printf("mode: all records\n");
    }
    printf("%-18s %-12s\n", "time_mark", "recno");

    if (start_index > 0U)
    {
        if (fseeko(file,
                   (off_t)(sizeof(records) + start_index * sizeof(IndexRecord)),
                   SEEK_SET) != 0)
        {
            perror("fseeko");
            fclose(file);
            return EXIT_FAILURE;
        }
    }

    for (uint64_t index = 0; index < records_to_show; ++index)
    {
        IndexRecord record;

        if (fread(&record, sizeof(record), 1, file) != 1)
        {
            fprintf(stderr, "Unexpected end of file while reading \"%s\".\n", argv[1]);
            fclose(file);
            return EXIT_FAILURE;
        }
        printf("%-18.8f %-12" PRIu64 "\n", record.time_mark, record.recno);
    }

    fclose(file);
    return EXIT_SUCCESS;
}
