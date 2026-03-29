#include "heap.h"

#include <stdlib.h>

static void heapify_down(MinHeap* heap, size_t index)
{
    while (1)
    {
        size_t left = index * 2U + 1U;
        size_t right = index * 2U + 2U;
        size_t smallest = index;

        if (left < heap->size &&
            compare_index_records(&heap->nodes[left].value, &heap->nodes[smallest].value) < 0)
        {
            smallest = left;
        }
        if (right < heap->size &&
            compare_index_records(&heap->nodes[right].value, &heap->nodes[smallest].value) < 0)
        {
            smallest = right;
        }
        if (smallest == index)
        {
            break;
        }

        {
            MinHeapNode temp = heap->nodes[index];
            heap->nodes[index] = heap->nodes[smallest];
            heap->nodes[smallest] = temp;
        }
        index = smallest;
    }
}

static void heapify_up(MinHeap* heap, size_t index)
{
    while (index > 0)
    {
        size_t parent = (index - 1U) / 2U;

        if (compare_index_records(&heap->nodes[index].value, &heap->nodes[parent].value) >= 0)
        {
            break;
        }

        {
            MinHeapNode temp = heap->nodes[index];
            heap->nodes[index] = heap->nodes[parent];
            heap->nodes[parent] = temp;
        }
        index = parent;
    }
}

MinHeap* create_heap(size_t run_count, FILE* runs[])
{
    MinHeap* heap = (MinHeap*)calloc(1, sizeof(MinHeap));

    if (heap == NULL)
    {
        return NULL;
    }

    heap->nodes = (MinHeapNode*)calloc(run_count, sizeof(MinHeapNode));
    if (heap->nodes == NULL)
    {
        free(heap);
        return NULL;
    }

    heap->capacity = run_count;
    for (size_t index = 0; index < run_count; ++index)
    {
        IndexRecord record;

        if (fread(&record, sizeof(record), 1, runs[index]) == 1)
        {
            heap->nodes[heap->size].value = record;
            heap->nodes[heap->size].run_index = index;
            ++heap->size;
        }
    }

    if (heap->size > 0)
    {
        for (size_t index = heap->size / 2U; index > 0; --index)
        {
            heapify_down(heap, index - 1U);
        }
    }

    return heap;
}

MinHeapNode extract_min_node(MinHeap* heap)
{
    MinHeapNode node = heap->nodes[0];

    heap->nodes[0] = heap->nodes[heap->size - 1U];
    --heap->size;
    if (heap->size > 0)
    {
        heapify_down(heap, 0);
    }

    return node;
}

void insert_node(MinHeap* heap, MinHeapNode node)
{
    heap->nodes[heap->size] = node;
    heapify_up(heap, heap->size);
    ++heap->size;
}

void destroy_heap(MinHeap* heap)
{
    if (heap == NULL)
    {
        return;
    }

    free(heap->nodes);
    free(heap);
}
