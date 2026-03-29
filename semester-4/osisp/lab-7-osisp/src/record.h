#pragma once

#include <stdint.h>
#include <sys/types.h>

#define FILENAME "records.bin"

typedef struct Record
{
    char name[80];
    char address[80];
    uint8_t semester;
} Record;

#define RECORD_SIZE ((off_t)sizeof(Record))
