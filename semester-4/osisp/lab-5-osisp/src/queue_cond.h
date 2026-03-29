#pragma once

#include "message.h"

#include <pthread.h>

typedef struct
{
    Message** buffer;
    int capacity;
    int head;
    int tail;
    int size;
    int addedCount;
    int extractedCount;
    int pendingShrink;
    pthread_mutex_t mutex;
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;
} QueueCond;

void addToQueueCond(Message* message);
Message* getFromQueueCond(void);
void initQueueCond(int capacity);
void deleteQueueCond(void);
void resizeQueueCond(int difference);
void processPendingShrinkCondLocked(void);
