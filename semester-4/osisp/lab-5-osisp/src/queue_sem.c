#define _POSIX_C_SOURCE 200809L

#include "queue_sem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern QueueSem queueSem;

static unsigned long semaphoreInstance = 0;

static void lockQueueSem(void)
{
    int rc = pthread_mutex_lock(&queueSem.mutex);

    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed (QueueSem): %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static void unlockQueueSem(void)
{
    int rc = pthread_mutex_unlock(&queueSem.mutex);

    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_unlock failed (QueueSem): %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static void freeQueuedMessagesLocked(void)
{
    for (int index = 0; index < queueSem.size; ++index)
    {
        int slot = (queueSem.head + index) % queueSem.capacity;
        free(queueSem.buffer[slot]);
        queueSem.buffer[slot] = NULL;
    }
}

static void rebuildQueueSemBuffer(int newCapacity)
{
    Message** newBuffer = (Message**)calloc((size_t)newCapacity, sizeof(Message*));

    if (newBuffer == NULL)
    {
        perror("Failed to allocate resized queue buffer (Sem)");
        exit(EXIT_FAILURE);
    }

    for (int index = 0; index < queueSem.size; ++index)
    {
        newBuffer[index] = queueSem.buffer[(queueSem.head + index) % queueSem.capacity];
    }

    free(queueSem.buffer);
    queueSem.buffer = newBuffer;
    queueSem.capacity = newCapacity;
    queueSem.head = 0;
    queueSem.tail = (queueSem.capacity == 0) ? 0 : queueSem.size % queueSem.capacity;
}

static void makeSemaphoreName(char* buffer, size_t bufferSize, const char* prefix)
{
    ++semaphoreInstance;
    snprintf(buffer, bufferSize, "/lab5_%s_%ld_%lu", prefix, (long)getpid(), semaphoreInstance);
}

static sem_t* openSemaphore(const char* name, unsigned int initialValue)
{
    sem_t* semaphore;

    sem_unlink(name);
    semaphore = sem_open(name, O_CREAT | O_EXCL, 0600, initialValue);
    if (semaphore == SEM_FAILED)
    {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    return semaphore;
}

static int shrinkOneSlotSem(void)
{
    if (queueSem.pendingShrink <= 0 || queueSem.capacity <= 1)
    {
        return 0;
    }

    if (sem_trywait(queueSem.emptySlots) != 0)
    {
        if (errno == EAGAIN || errno == EINTR)
        {
            return 0;
        }
        perror("sem_trywait failed during shrink (Sem)");
        exit(EXIT_FAILURE);
    }

    lockQueueSem();
    if (queueSem.pendingShrink > 0 && queueSem.capacity > 1)
    {
        rebuildQueueSemBuffer(queueSem.capacity - 1);
        --queueSem.pendingShrink;
        unlockQueueSem();
        return 1;
    }
    unlockQueueSem();

    if (sem_post(queueSem.emptySlots) != 0)
    {
        perror("sem_post rollback failed (Sem)");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void initQueueSem(int capacity)
{
    queueSem.buffer = (Message**)calloc((size_t)capacity, sizeof(Message*));
    if (queueSem.buffer == NULL)
    {
        perror("Failed to allocate queue buffer (Sem)");
        exit(EXIT_FAILURE);
    }

    queueSem.capacity = capacity;
    queueSem.head = 0;
    queueSem.tail = 0;
    queueSem.size = 0;
    queueSem.addedCount = 0;
    queueSem.extractedCount = 0;
    queueSem.pendingShrink = 0;

    if (pthread_mutex_init(&queueSem.mutex, NULL) != 0)
    {
        perror("Mutex initialization failed (Sem)");
        free(queueSem.buffer);
        exit(EXIT_FAILURE);
    }

    makeSemaphoreName(queueSem.emptyName, sizeof(queueSem.emptyName), "empty");
    makeSemaphoreName(queueSem.filledName, sizeof(queueSem.filledName), "filled");
    queueSem.emptySlots = openSemaphore(queueSem.emptyName, (unsigned int)capacity);
    queueSem.filledSlots = openSemaphore(queueSem.filledName, 0U);
}

void processPendingShrinkSem(void)
{
    while (shrinkOneSlotSem() == 1)
    {
    }
}

void resizeQueueSem(int difference)
{
    if (difference == 0)
    {
        return;
    }

    lockQueueSem();
    if (difference > 0)
    {
        int newCapacity = queueSem.capacity + difference;

        rebuildQueueSemBuffer(newCapacity);
        unlockQueueSem();

        for (int index = 0; index < difference; ++index)
        {
            if (sem_post(queueSem.emptySlots) != 0)
            {
                perror("sem_post failed during resize up (Sem)");
                exit(EXIT_FAILURE);
            }
        }
        printf("Queue was expanded. New capacity: %d\n", newCapacity);
        return;
    }

    {
        int request = -difference;
        int maxPossible = queueSem.capacity - 1 - queueSem.pendingShrink;

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
            unlockQueueSem();
            return;
        }

        queueSem.pendingShrink += request;
        unlockQueueSem();

        processPendingShrinkSem();

        lockQueueSem();
        printf("Shrink request accepted: %d. Pending shrink: %d\n", request, queueSem.pendingShrink);
        unlockQueueSem();
    }
}

void deleteQueueSem(void)
{
    lockQueueSem();
    if (queueSem.buffer != NULL)
    {
        freeQueuedMessagesLocked();
        free(queueSem.buffer);
    }
    queueSem.buffer = NULL;
    queueSem.capacity = 0;
    queueSem.head = 0;
    queueSem.tail = 0;
    queueSem.size = 0;
    queueSem.pendingShrink = 0;
    unlockQueueSem();

    if (queueSem.emptySlots != NULL)
    {
        sem_close(queueSem.emptySlots);
        sem_unlink(queueSem.emptyName);
        queueSem.emptySlots = NULL;
    }
    if (queueSem.filledSlots != NULL)
    {
        sem_close(queueSem.filledSlots);
        sem_unlink(queueSem.filledName);
        queueSem.filledSlots = NULL;
    }
    if (pthread_mutex_destroy(&queueSem.mutex) != 0)
    {
        perror("Failed to destroy mutex (Sem)");
    }
}

void addToQueueSem(Message* message)
{
    queueSem.buffer[queueSem.tail] = message;
    queueSem.tail = (queueSem.tail + 1) % queueSem.capacity;
    ++queueSem.size;
    ++queueSem.addedCount;
}

Message* getFromQueueSem(void)
{
    Message* message = queueSem.buffer[queueSem.head];

    queueSem.buffer[queueSem.head] = NULL;
    queueSem.head = (queueSem.head + 1) % queueSem.capacity;
    --queueSem.size;
    ++queueSem.extractedCount;
    return message;
}
