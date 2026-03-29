#pragma once

#include <stdint.h>

typedef struct
{
    double time_mark;
    uint64_t recno;
} IndexRecord;

typedef struct
{
    uint64_t records;
    IndexRecord idx[];
} IndexHeader;

double generate_random_time_mark(unsigned int* seed);
int compare_index_records(const void* left, const void* right);
