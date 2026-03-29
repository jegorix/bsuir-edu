#define _POSIX_C_SOURCE 200809L

#include "producer.h"

#include "queue_cond.h"
#include "queue_sem.h"
#include "stack.h"

#include <errno.h>
#include <inttypes.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern QueueSem queueSem;
extern QueueCond queueCond;
extern atomic_bool keepRunning;

enum
{
    POLL_DELAY_MS = 100,
    CYCLE_DELAY_MS = 800
};

static int threadShouldStop(const ThreadControl* control)
{
    return atomic_load(&control->stopRequested) || !atomic_load(&keepRunning);
}

static void sleepMilliseconds(long milliseconds)
{
    struct timespec delay;

    delay.tv_sec = milliseconds / 1000L;
    delay.tv_nsec = (milliseconds % 1000L) * 1000000L;
    nanosleep(&delay, NULL);
}

static void pauseBetweenCycles(const ThreadControl* control)
{
    long remaining = CYCLE_DELAY_MS;

    while (remaining > 0 && !threadShouldStop(control))
    {
        long slice = (remaining > POLL_DELAY_MS) ? POLL_DELAY_MS : remaining;

        sleepMilliseconds(slice);
        remaining -= slice;
    }
}

static void waitOnConditionWithTimeout(pthread_cond_t* condition, pthread_mutex_t* mutex)
{
    struct timespec deadline;
    int rc;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += POLL_DELAY_MS * 1000000L;
    if (deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += deadline.tv_nsec / 1000000000L;
        deadline.tv_nsec %= 1000000000L;
    }

    rc = pthread_cond_timedwait(condition, mutex, &deadline);
    if (rc != 0 && rc != ETIMEDOUT)
    {
        fprintf(stderr, "pthread_cond_timedwait failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

static int waitForEmptySlotSem(const ThreadControl* control)
{
    while (!threadShouldStop(control))
    {
        processPendingShrinkSem();
        if (sem_trywait(queueSem.emptySlots) == 0)
        {
            return 1;
        }
        if (errno != EAGAIN && errno != EINTR)
        {
            perror("Producer sem_trywait");
            exit(EXIT_FAILURE);
        }
        sleepMilliseconds(POLL_DELAY_MS);
    }

    return 0;
}

void* producerSem(void* arg)
{
    ThreadControl* control = (ThreadControl*)arg;
    int producedMessages = 0;

    printf("Producer %09" PRIuPTR ": Started.\n", threadToId(pthread_self()));

    while (!threadShouldStop(control) && producedMessages < WORKER_MESSAGE_LIMIT)
    {
        int addedCount = 0;
        int queued = 0;
        Message* message = produceMessage(&control->seed);

        while (!queued && !threadShouldStop(control))
        {
            if (!waitForEmptySlotSem(control))
            {
                break;
            }

            pthread_mutex_lock(&queueSem.mutex);
            if (queueSem.pendingShrink > 0)
            {
                pthread_mutex_unlock(&queueSem.mutex);
                sem_post(queueSem.emptySlots);
                processPendingShrinkSem();
                sleepMilliseconds(POLL_DELAY_MS);
                continue;
            }

            addToQueueSem(message);
            addedCount = queueSem.addedCount;
            queued = 1;
            pthread_mutex_unlock(&queueSem.mutex);

            sem_post(queueSem.filledSlots);
            ++producedMessages;
            printf("Producer %09" PRIuPTR ": Added message (type = %02X, hash = %04X, size = %03u). Total Added: %d\n",
                   threadToId(pthread_self()),
                   message->type,
                   message->hash,
                   (unsigned)message->size,
                   addedCount);
        }

        if (!queued)
        {
            free(message);
            break;
        }

        pauseBetweenCycles(control);
    }

    if (producedMessages >= WORKER_MESSAGE_LIMIT)
    {
        printf("Producer %09" PRIuPTR ": Reached limit %d and stopped.\n",
               threadToId(pthread_self()),
               WORKER_MESSAGE_LIMIT);
    }
    atomic_store(&control->finished, 1);
    return NULL;
}

void* producerCond(void* arg)
{
    ThreadControl* control = (ThreadControl*)arg;
    int producedMessages = 0;

    printf("Producer %09" PRIuPTR ": Started.\n", threadToId(pthread_self()));

    while (!threadShouldStop(control) && producedMessages < WORKER_MESSAGE_LIMIT)
    {
        int addedCount = 0;
        int queued = 0;
        Message* message = produceMessage(&control->seed);

        pthread_mutex_lock(&queueCond.mutex);
        while (!queued)
        {
            while ((queueCond.size == queueCond.capacity || queueCond.pendingShrink > 0) && !threadShouldStop(control))
            {
                waitOnConditionWithTimeout(&queueCond.notFull, &queueCond.mutex);
            }

            if (threadShouldStop(control))
            {
                break;
            }

            addToQueueCond(message);
            addedCount = queueCond.addedCount;
            queued = 1;
            ++producedMessages;
            pthread_cond_signal(&queueCond.notEmpty);
        }
        pthread_mutex_unlock(&queueCond.mutex);

        if (!queued)
        {
            free(message);
            break;
        }

        printf("Producer %09" PRIuPTR ": Added message (type = %02X, hash = %04X, size = %03u). Total Added: %d\n",
               threadToId(pthread_self()),
               message->type,
               message->hash,
               (unsigned)message->size,
               addedCount);
        pauseBetweenCycles(control);
    }

    if (producedMessages >= WORKER_MESSAGE_LIMIT)
    {
        printf("Producer %09" PRIuPTR ": Reached limit %d and stopped.\n",
               threadToId(pthread_self()),
               WORKER_MESSAGE_LIMIT);
    }
    atomic_store(&control->finished, 1);
    return NULL;
}

Message* produceMessage(unsigned int* seed)
{
    Message* message = (Message*)malloc(sizeof(Message));
    size_t alignedSize;

    if (message == NULL)
    {
        perror("Failed to allocate message");
        exit(EXIT_FAILURE);
    }

    memset(message, 0, sizeof(Message));
    message->type = (uint8_t)(rand_r(seed) % 256U);
    message->size = (uint8_t)(rand_r(seed) % 256U);

    alignedSize = getAlignedDataLength(message);
    for (size_t index = 0; index < alignedSize; ++index)
    {
        message->data[index] = (uint8_t)(rand_r(seed) % 256U);
    }

    message->hash = 0;
    message->hash = calculateHash(message);
    return message;
}
