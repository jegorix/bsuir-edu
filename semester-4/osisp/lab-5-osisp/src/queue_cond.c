#define _POSIX_C_SOURCE 200809L

#include "queue_cond.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern QueueCond queueCond;

static void lockQueueCond(void)
{
    int rc = pthread_mutex_lock(&queueCond.mutex);

    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed (QueueCond): %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static void unlockQueueCond(void)
{
    int rc = pthread_mutex_unlock(&queueCond.mutex);

    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_unlock failed (QueueCond): %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static void freeQueuedMessagesLocked(void)
{
    for (int index = 0; index < queueCond.size; ++index)
    {
        int slot = (queueCond.head + index) % queueCond.capacity;
        free(queueCond.buffer[slot]);
        queueCond.buffer[slot] = NULL;
    }
}

static void rebuildQueueCondBuffer(int newCapacity)
{
    Message** newBuffer = (Message**)calloc((size_t)newCapacity, sizeof(Message*));

    if (newBuffer == NULL)
    {
        perror("Failed to allocate resized queue buffer (Cond)");
        exit(EXIT_FAILURE);
    }

    for (int index = 0; index < queueCond.size; ++index)
    {
        newBuffer[index] = queueCond.buffer[(queueCond.head + index) % queueCond.capacity];
    }

    free(queueCond.buffer);
    queueCond.buffer = newBuffer;
    queueCond.capacity = newCapacity;
    queueCond.head = 0;
    queueCond.tail = (queueCond.capacity == 0) ? 0 : queueCond.size % queueCond.capacity;
}

void initQueueCond(int capacity)
{
    queueCond.buffer = (Message**)calloc((size_t)capacity, sizeof(Message*));
    if (queueCond.buffer == NULL)
    {
        perror("Failed to allocate queue buffer (Cond)");
        exit(EXIT_FAILURE);
    }

    queueCond.capacity = capacity;
    queueCond.head = 0;
    queueCond.tail = 0;
    queueCond.size = 0;
    queueCond.addedCount = 0;
    queueCond.extractedCount = 0;
    queueCond.pendingShrink = 0;

    if (pthread_mutex_init(&queueCond.mutex, NULL) != 0)
    {
        perror("Mutex initialization failed (Cond)");
        free(queueCond.buffer);
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&queueCond.notFull, NULL) != 0)
    {
        perror("Condition variable notFull initialization failed (Cond)");
        pthread_mutex_destroy(&queueCond.mutex);
        free(queueCond.buffer);
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_init(&queueCond.notEmpty, NULL) != 0)
    {
        perror("Condition variable notEmpty initialization failed (Cond)");
        pthread_cond_destroy(&queueCond.notFull);
        pthread_mutex_destroy(&queueCond.mutex);
        free(queueCond.buffer);
        exit(EXIT_FAILURE);
    }
}

void processPendingShrinkCondLocked(void)
{
    while (queueCond.pendingShrink > 0 && queueCond.capacity > 1 && queueCond.size < queueCond.capacity)
    {
        rebuildQueueCondBuffer(queueCond.capacity - 1);
        --queueCond.pendingShrink;
    }
}

void resizeQueueCond(int difference)
{
    if (difference == 0)
    {
        return;
    }

    lockQueueCond();
    if (difference > 0)
    {
        int newCapacity = queueCond.capacity + difference;

        rebuildQueueCondBuffer(newCapacity);
        pthread_cond_broadcast(&queueCond.notFull);
        printf("Queue was expanded. New capacity: %d\n", newCapacity);
        unlockQueueCond();
        return;
    }

    {
        int request = -difference;
        int maxPossible = queueCond.capacity - 1 - queueCond.pendingShrink;

        if (maxPossible < 0)
        {
            maxPossible = 0;
        }
        if (request > maxPossible)
        {
            request = maxPossible;
        }

        if (request == 0)
        {
            printf("Shrink request ignored: minimal capacity is already reserved.\n");
            unlockQueueCond();
            return;
        }

        queueCond.pendingShrink += request;
        processPendingShrinkCondLocked();
        pthread_cond_broadcast(&queueCond.notFull);
        printf("Shrink request accepted: %d. Pending shrink: %d\n", request, queueCond.pendingShrink);
        unlockQueueCond();
    }
}

void deleteQueueCond(void)
{
    lockQueueCond();
    if (queueCond.buffer != NULL)
    {
        freeQueuedMessagesLocked();
        free(queueCond.buffer);
    }
    queueCond.buffer = NULL;
    queueCond.capacity = 0;
    queueCond.size = 0;
    queueCond.head = 0;
    queueCond.tail = 0;
    queueCond.pendingShrink = 0;
    unlockQueueCond();

    if (pthread_cond_destroy(&queueCond.notFull) != 0)
    {
        perror("Failed to destroy notFull condition variable (Cond)");
    }
    if (pthread_cond_destroy(&queueCond.notEmpty) != 0)
    {
        perror("Failed to destroy notEmpty condition variable (Cond)");
    }
    if (pthread_mutex_destroy(&queueCond.mutex) != 0)
    {
        perror("Failed to destroy mutex (Cond)");
    }
}

void addToQueueCond(Message* message)
{
    queueCond.buffer[queueCond.tail] = message;
    queueCond.tail = (queueCond.tail + 1) % queueCond.capacity;
    ++queueCond.size;
    ++queueCond.addedCount;
}

Message* getFromQueueCond(void)
{
    Message* message = queueCond.buffer[queueCond.head];

    queueCond.buffer[queueCond.head] = NULL;
    queueCond.head = (queueCond.head + 1) % queueCond.capacity;
    --queueCond.size;
    ++queueCond.extractedCount;
    return message;
}
