#define _POSIX_C_SOURCE 200809L

#include "file.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int open_index_file(const char* filename, int* file_descriptor, uint64_t* records, size_t* data_bytes)
{
    struct stat file_info;
    ssize_t bytes_read;
    size_t expected_data_bytes;

    *file_descriptor = open(filename, O_RDWR);
    if (*file_descriptor == -1)
    {
        perror("open");
        return -1;
    }

    if (fstat(*file_descriptor, &file_info) != 0)
    {
        perror("fstat");
        close(*file_descriptor);
        *file_descriptor = -1;
        return -1;
    }

    if ((size_t)file_info.st_size < sizeof(uint64_t))
    {
        fprintf(stderr, "File \"%s\" is too small to contain an index header.\n", filename);
        close(*file_descriptor);
        *file_descriptor = -1;
        return -1;
    }

    bytes_read = pread(*file_descriptor, records, sizeof(*records), 0);
    if (bytes_read != (ssize_t)sizeof(*records))
    {
        perror("pread");
        close(*file_descriptor);
        *file_descriptor = -1;
        return -1;
    }

    if (*records == 0 || *records > (SIZE_MAX / sizeof(IndexRecord)))
    {
        fprintf(stderr, "File \"%s\" contains an invalid record count.\n", filename);
        close(*file_descriptor);
        *file_descriptor = -1;
        return -1;
    }

    expected_data_bytes = (size_t)(*records) * sizeof(IndexRecord);
    if ((size_t)file_info.st_size != sizeof(uint64_t) + expected_data_bytes)
    {
        fprintf(stderr, "File \"%s\" size does not match the index header.\n", filename);
        close(*file_descriptor);
        *file_descriptor = -1;
        return -1;
    }

    *data_bytes = expected_data_bytes;
    return 0;
}

int map_index_chunk(int file_descriptor, size_t data_offset, size_t chunk_bytes, size_t page_size, MappedChunk* chunk)
{
    off_t file_offset = (off_t)(sizeof(uint64_t) + data_offset);
    off_t map_offset = file_offset & ~((off_t)page_size - 1);
    size_t prefix = (size_t)(file_offset - map_offset);
    size_t map_length = prefix + chunk_bytes;
    void* mapping;

    memset(chunk, 0, sizeof(*chunk));
    mapping = mmap(NULL, map_length, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, map_offset);
    if (mapping == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    chunk->mapping = mapping;
    chunk->mapping_length = map_length;
    chunk->records = (IndexRecord*)((char*)mapping + prefix);
    chunk->record_count = chunk_bytes / sizeof(IndexRecord);
    return 0;
}

int sync_and_unmap_chunk(MappedChunk* chunk)
{
    if (chunk->mapping == NULL)
    {
        return 0;
    }

    if (msync(chunk->mapping, chunk->mapping_length, MS_SYNC) != 0)
    {
        perror("msync");
        return -1;
    }
    if (munmap(chunk->mapping, chunk->mapping_length) != 0)
    {
        perror("munmap");
        return -1;
    }

    memset(chunk, 0, sizeof(*chunk));
    return 0;
}

int write_run_file(const char* run_filename, const IndexRecord* records, size_t record_count)
{
    FILE* file = fopen(run_filename, "wb");

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    if (fwrite(records, sizeof(IndexRecord), record_count, file) != record_count)
    {
        perror("fwrite");
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0)
    {
        perror("fclose");
        return -1;
    }

    return 0;
}

void make_run_filename(char* buffer, size_t buffer_size, const char* filename, size_t run_index)
{
    snprintf(buffer, buffer_size, "%s.run.%zu", filename, run_index);
}
