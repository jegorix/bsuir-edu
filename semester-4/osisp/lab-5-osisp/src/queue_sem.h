#pragma once

#include "message.h"

#include <pthread.h>
#include <semaphore.h>

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
    sem_t* emptySlots;
    sem_t* filledSlots;
    char emptyName[64];
    char filledName[64];
} QueueSem;

void addToQueueSem(Message* message);
Message* getFromQueueSem(void);
void initQueueSem(int capacity);
void deleteQueueSem(void);
void resizeQueueSem(int difference);
void processPendingShrinkSem(void);
