#pragma once

#include "index.h"

#include <stddef.h>
#include <stdio.h>

typedef struct
{
    IndexRecord value;
    size_t run_index;
} MinHeapNode;

typedef struct
{
    MinHeapNode* nodes;
    size_t size;
    size_t capacity;
} MinHeap;

MinHeap* create_heap(size_t run_count, FILE* runs[]);
MinHeapNode extract_min_node(MinHeap* heap);
void insert_node(MinHeap* heap, MinHeapNode node);
void destroy_heap(MinHeap* heap);
