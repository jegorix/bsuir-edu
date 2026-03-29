#pragma once

#include "index.h"

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    void* mapping;
    size_t mapping_length;
    IndexRecord* records;
    size_t record_count;
} MappedChunk;

int open_index_file(const char* filename, int* file_descriptor, uint64_t* records, size_t* data_bytes);
int map_index_chunk(int file_descriptor, size_t data_offset, size_t chunk_bytes, size_t page_size, MappedChunk* chunk);
int sync_and_unmap_chunk(MappedChunk* chunk);
int write_run_file(const char* run_filename, const IndexRecord* records, size_t record_count);
void make_run_filename(char* buffer, size_t buffer_size, const char* filename, size_t run_index);
