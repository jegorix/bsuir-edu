#define _POSIX_C_SOURCE 200809L

#include "sort.h"

#include "barrier_compat.h"
#include "file.h"
#include "heap.h"
#include "index.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct SortContext SortContext;

typedef struct
{
    SortContext* context;
    size_t thread_id;
} WorkerInfo;

struct SortContext
{
    pthread_barrier_t barrier;
    pthread_mutex_t mutex;
    atomic_bool shutdown;
    int barrier_initialized;
    int mutex_initialized;
    size_t thread_count;
    size_t block_count;
    size_t mem_size;
    size_t block_bytes;
    size_t records_per_block;
    size_t records_per_chunk;
    IndexRecord* current_records;
    int* sort_map;
    int* merge_map;
};

static int init_context(SortContext* context, size_t mem_size, size_t block_count, size_t thread_count)
{
    memset(context, 0, sizeof(*context));
    atomic_init(&context->shutdown, 0);

    context->thread_count = thread_count;
    context->block_count = block_count;
    context->mem_size = mem_size;
    context->block_bytes = mem_size / block_count;
    context->records_per_block = context->block_bytes / sizeof(IndexRecord);
    context->records_per_chunk = mem_size / sizeof(IndexRecord);
    context->sort_map = (int*)calloc(block_count, sizeof(int));
    context->merge_map = (int*)calloc(block_count / 2U, sizeof(int));
    if (context->sort_map == NULL || context->merge_map == NULL)
    {
        fprintf(stderr, "Failed to allocate synchronization maps.\n");
        return -1;
    }

    if (pthread_barrier_init(&context->barrier, NULL, (unsigned)thread_count) != 0)
    {
        fprintf(stderr, "Failed to initialize barrier.\n");
        return -1;
    }
    context->barrier_initialized = 1;
    if (pthread_mutex_init(&context->mutex, NULL) != 0)
    {
        fprintf(stderr, "Failed to initialize mutex.\n");
        return -1;
    }
    context->mutex_initialized = 1;

    return 0;
}

static void destroy_context(SortContext* context)
{
    if (context->mutex_initialized)
    {
        pthread_mutex_destroy(&context->mutex);
    }
    if (context->barrier_initialized)
    {
        pthread_barrier_destroy(&context->barrier);
    }
    free(context->sort_map);
    free(context->merge_map);
}

static void prepare_chunk(SortContext* context, IndexRecord* records)
{
    context->current_records = records;
    memset(context->sort_map, 0, context->block_count * sizeof(int));
    for (size_t index = 0; index < context->thread_count; ++index)
    {
        context->sort_map[index] = 1;
    }
}

static int claim_next_sort_block(SortContext* context, size_t* block_index)
{
    int found = 0;

    pthread_mutex_lock(&context->mutex);
    for (size_t index = 0; index < context->block_count; ++index)
    {
        if (context->sort_map[index] == 0)
        {
            context->sort_map[index] = 1;
            *block_index = index;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&context->mutex);

    return found;
}

static void sort_phase(SortContext* context, size_t thread_id)
{
    size_t block_index = thread_id;

    qsort(context->current_records + block_index * context->records_per_block,
          context->records_per_block,
          sizeof(IndexRecord),
          compare_index_records);

    while (claim_next_sort_block(context, &block_index))
    {
        qsort(context->current_records + block_index * context->records_per_block,
              context->records_per_block,
              sizeof(IndexRecord),
              compare_index_records);
    }

    pthread_barrier_wait(&context->barrier);
}

static void merge_pair(SortContext* context, size_t pair_index, size_t run_blocks)
{
    size_t left_block = pair_index * run_blocks * 2U;
    size_t run_records = run_blocks * context->records_per_block;
    size_t total_records = run_records * 2U;
    IndexRecord* left = context->current_records + left_block * context->records_per_block;
    IndexRecord* right = left + run_records;
    IndexRecord* temp = (IndexRecord*)malloc(total_records * sizeof(IndexRecord));
    size_t left_index = 0;
    size_t right_index = 0;
    size_t out_index = 0;

    if (temp == NULL)
    {
        fprintf(stderr, "Failed to allocate merge buffer.\n");
        exit(EXIT_FAILURE);
    }

    while (left_index < run_records && right_index < run_records)
    {
        if (compare_index_records(&left[left_index], &right[right_index]) <= 0)
        {
            temp[out_index++] = left[left_index++];
        }
        else
        {
            temp[out_index++] = right[right_index++];
        }
    }
    while (left_index < run_records)
    {
        temp[out_index++] = left[left_index++];
    }
    while (right_index < run_records)
    {
        temp[out_index++] = right[right_index++];
    }

    memcpy(left, temp, total_records * sizeof(IndexRecord));
    free(temp);
}

static int claim_next_merge_pair(SortContext* context, size_t pair_count, size_t* pair_index)
{
    int found = 0;

    pthread_mutex_lock(&context->mutex);
    for (size_t index = 0; index < pair_count; ++index)
    {
        if (context->merge_map[index] == 0)
        {
            context->merge_map[index] = 1;
            *pair_index = index;
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&context->mutex);

    return found;
}

static void merge_phase(SortContext* context, size_t thread_id)
{
    for (size_t run_blocks = 1; run_blocks < context->block_count; run_blocks <<= 1U)
    {
        size_t pair_count = context->block_count / (run_blocks * 2U);

        if (pair_count == 1)
        {
            if (thread_id == 0)
            {
                merge_pair(context, 0, run_blocks);
            }
            pthread_barrier_wait(&context->barrier);
            continue;
        }

        if (pair_count >= context->thread_count)
        {
            for (size_t pair_index = thread_id; pair_index < pair_count; pair_index += context->thread_count)
            {
                merge_pair(context, pair_index, run_blocks);
            }
            pthread_barrier_wait(&context->barrier);
            continue;
        }

        if (thread_id == 0)
        {
            memset(context->merge_map, 0, pair_count * sizeof(int));
        }
        pthread_barrier_wait(&context->barrier);

        {
            size_t pair_index;
            while (claim_next_merge_pair(context, pair_count, &pair_index))
            {
                merge_pair(context, pair_index, run_blocks);
            }
        }

        pthread_barrier_wait(&context->barrier);
    }
}

static void process_current_chunk(SortContext* context, size_t thread_id)
{
    sort_phase(context, thread_id);
    merge_phase(context, thread_id);
}

static void* worker_main(void* arg)
{
    WorkerInfo* info = (WorkerInfo*)arg;
    SortContext* context = info->context;

    while (1)
    {
        pthread_barrier_wait(&context->barrier);
        if (atomic_load(&context->shutdown))
        {
            break;
        }
        process_current_chunk(context, info->thread_id);
    }

    return NULL;
}

static void cleanup_run_files(const char* filename, size_t run_count)
{
    size_t name_length = strlen(filename) + 32U;
    char* run_name = (char*)malloc(name_length);

    if (run_name == NULL)
    {
        return;
    }

    for (size_t index = 0; index < run_count; ++index)
    {
        make_run_filename(run_name, name_length, filename, index);
        remove(run_name);
    }

    free(run_name);
}

static int merge_run_files(const char* filename, uint64_t records, size_t run_count)
{
    FILE** runs = NULL;
    FILE* output = NULL;
    MinHeap* heap = NULL;
    char* run_name = NULL;
    char* temp_name = NULL;
    int result = -1;

    runs = (FILE**)calloc(run_count, sizeof(FILE*));
    if (runs == NULL)
    {
        fprintf(stderr, "Failed to allocate run handles.\n");
        goto cleanup;
    }

    run_name = (char*)malloc(strlen(filename) + 32U);
    temp_name = (char*)malloc(strlen(filename) + 32U);
    if (run_name == NULL || temp_name == NULL)
    {
        fprintf(stderr, "Failed to allocate temporary file names.\n");
        goto cleanup;
    }

    for (size_t index = 0; index < run_count; ++index)
    {
        make_run_filename(run_name, strlen(filename) + 32U, filename, index);
        runs[index] = fopen(run_name, "rb");
        if (runs[index] == NULL)
        {
            perror("fopen");
            goto cleanup;
        }
    }

    snprintf(temp_name, strlen(filename) + 32U, "%s.sorted.tmp", filename);
    output = fopen(temp_name, "wb");
    if (output == NULL)
    {
        perror("fopen");
        goto cleanup;
    }

    if (fwrite(&records, sizeof(records), 1, output) != 1)
    {
        perror("fwrite");
        goto cleanup;
    }

    heap = create_heap(run_count, runs);
    if (heap == NULL)
    {
        fprintf(stderr, "Failed to create merge heap.\n");
        goto cleanup;
    }

    while (heap->size > 0)
    {
        MinHeapNode node = extract_min_node(heap);

        if (fwrite(&node.value, sizeof(IndexRecord), 1, output) != 1)
        {
            perror("fwrite");
            goto cleanup;
        }

        if (fread(&node.value, sizeof(IndexRecord), 1, runs[node.run_index]) == 1)
        {
            insert_node(heap, node);
        }
    }

    if (fclose(output) != 0)
    {
        perror("fclose");
        output = NULL;
        goto cleanup;
    }
    output = NULL;

    for (size_t index = 0; index < run_count; ++index)
    {
        fclose(runs[index]);
        runs[index] = NULL;
    }

    cleanup_run_files(filename, run_count);
    if (rename(temp_name, filename) != 0)
    {
        perror("rename");
        goto cleanup;
    }

    result = 0;

cleanup:
    if (output != NULL)
    {
        fclose(output);
        remove(temp_name);
    }
    if (runs != NULL)
    {
        for (size_t index = 0; index < run_count; ++index)
        {
            if (runs[index] != NULL)
            {
                fclose(runs[index]);
            }
        }
    }
    destroy_heap(heap);
    free(runs);
    free(run_name);
    free(temp_name);

    return result;
}

int sort_index_file(size_t mem_size, size_t block_count, size_t thread_count, const char* filename)
{
    SortContext context;
    WorkerInfo* workers = NULL;
    pthread_t* handles = NULL;
    size_t created_threads = 0;
    int file_descriptor = -1;
    uint64_t records = 0;
    size_t data_bytes = 0;
    size_t run_count;
    long page_size_value = sysconf(_SC_PAGESIZE);
    size_t page_size = (page_size_value > 0) ? (size_t)page_size_value : 4096U;
    int result = -1;

    if (open_index_file(filename, &file_descriptor, &records, &data_bytes) != 0)
    {
        return -1;
    }

    if (data_bytes < mem_size || (data_bytes % mem_size) != 0)
    {
        fprintf(stderr, "Index data size must be a multiple of memsize and at least one full chunk.\n");
        close(file_descriptor);
        return -1;
    }

    if ((mem_size % block_count) != 0 || ((mem_size / block_count) % sizeof(IndexRecord)) != 0)
    {
        fprintf(stderr, "memsize/blocks must be a multiple of sizeof(IndexRecord).\n");
        close(file_descriptor);
        return -1;
    }

    if (init_context(&context, mem_size, block_count, thread_count) != 0)
    {
        close(file_descriptor);
        return -1;
    }

    workers = (WorkerInfo*)calloc(thread_count, sizeof(WorkerInfo));
    handles = (pthread_t*)calloc(thread_count, sizeof(pthread_t));
    if (workers == NULL || handles == NULL)
    {
        fprintf(stderr, "Failed to allocate thread metadata.\n");
        goto cleanup;
    }

    for (size_t index = 0; index < thread_count; ++index)
    {
        workers[index].context = &context;
        workers[index].thread_id = index;
    }

    for (size_t index = 1; index < thread_count; ++index)
    {
        int error = pthread_create(&handles[index], NULL, worker_main, &workers[index]);

        if (error != 0)
        {
            fprintf(stderr, "Failed to create worker thread.\n");
            goto cleanup;
        }
        ++created_threads;
    }

    run_count = data_bytes / mem_size;
    for (size_t run_index = 0; run_index < run_count; ++run_index)
    {
        MappedChunk chunk;
        char* run_name = (char*)malloc(strlen(filename) + 32U);

        if (run_name == NULL)
        {
            fprintf(stderr, "Failed to allocate run file name.\n");
            goto cleanup;
        }

        if (map_index_chunk(file_descriptor, run_index * mem_size, mem_size, page_size, &chunk) != 0)
        {
            free(run_name);
            goto cleanup;
        }

        prepare_chunk(&context, chunk.records);
        pthread_barrier_wait(&context.barrier);
        process_current_chunk(&context, 0);

        make_run_filename(run_name, strlen(filename) + 32U, filename, run_index);
        if (write_run_file(run_name, chunk.records, chunk.record_count) != 0)
        {
            free(run_name);
            sync_and_unmap_chunk(&chunk);
            goto cleanup;
        }
        free(run_name);

        if (sync_and_unmap_chunk(&chunk) != 0)
        {
            goto cleanup;
        }
    }

    atomic_store(&context.shutdown, 1);
    pthread_barrier_wait(&context.barrier);
    for (size_t index = 1; index <= created_threads; ++index)
    {
        pthread_join(handles[index], NULL);
    }

    close(file_descriptor);
    file_descriptor = -1;
    if (merge_run_files(filename, records, run_count) != 0)
    {
        goto cleanup_after_threads;
    }

    result = 0;

cleanup_after_threads:
    cleanup_run_files(filename, run_count);
    created_threads = 0;

cleanup:
    if (result != 0 && created_threads > 0)
    {
        atomic_store(&context.shutdown, 1);
        pthread_barrier_wait(&context.barrier);
        for (size_t index = 1; index <= created_threads; ++index)
        {
            pthread_join(handles[index], NULL);
        }
    }
    if (file_descriptor != -1)
    {
        close(file_descriptor);
    }
    destroy_context(&context);
    free(handles);
    free(workers);

    return result;
}
