#define _POSIX_C_SOURCE 200809L

#include "consumer.h"

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

static int waitForFilledSlotSem(const ThreadControl* control)
{
    while (!threadShouldStop(control))
    {
        if (sem_trywait(queueSem.filledSlots) == 0)
        {
            return 1;
        }
        if (errno != EAGAIN && errno != EINTR)
        {
            perror("Consumer sem_trywait");
            exit(EXIT_FAILURE);
        }
        sleepMilliseconds(POLL_DELAY_MS);
    }

    return 0;
}

void* consumerSem(void* arg)
{
    ThreadControl* control = (ThreadControl*)arg;
    int consumedMessages = 0;

    printf("Consumer %09" PRIuPTR ": Started.\n", threadToId(pthread_self()));

    while (!threadShouldStop(control) && consumedMessages < WORKER_MESSAGE_LIMIT)
    {
        int extractedCount = 0;
        Message* message;

        if (!waitForFilledSlotSem(control))
        {
            break;
        }

        pthread_mutex_lock(&queueSem.mutex);
        message = getFromQueueSem();
        extractedCount = queueSem.extractedCount;
        pthread_mutex_unlock(&queueSem.mutex);

        sem_post(queueSem.emptySlots);
        processPendingShrinkSem();

        consumeMessage(message, extractedCount);
        ++consumedMessages;
        free(message);
        pauseBetweenCycles(control);
    }

    if (consumedMessages >= WORKER_MESSAGE_LIMIT)
    {
        printf("Consumer %09" PRIuPTR ": Reached limit %d and stopped.\n",
               threadToId(pthread_self()),
               WORKER_MESSAGE_LIMIT);
    }
    atomic_store(&control->finished, 1);
    return NULL;
}

void* consumerCond(void* arg)
{
    ThreadControl* control = (ThreadControl*)arg;
    int consumedMessages = 0;

    printf("Consumer %09" PRIuPTR ": Started.\n", threadToId(pthread_self()));

    while (!threadShouldStop(control) && consumedMessages < WORKER_MESSAGE_LIMIT)
    {
        int extractedCount = 0;
        Message* message = NULL;

        pthread_mutex_lock(&queueCond.mutex);
        while (queueCond.size == 0 && !threadShouldStop(control))
        {
            waitOnConditionWithTimeout(&queueCond.notEmpty, &queueCond.mutex);
        }

        if (threadShouldStop(control))
        {
            pthread_mutex_unlock(&queueCond.mutex);
            break;
        }

        message = getFromQueueCond();
        extractedCount = queueCond.extractedCount;
        processPendingShrinkCondLocked();
        if (queueCond.pendingShrink == 0)
        {
            pthread_cond_signal(&queueCond.notFull);
        }
        pthread_mutex_unlock(&queueCond.mutex);

        consumeMessage(message, extractedCount);
        ++consumedMessages;
        free(message);
        pauseBetweenCycles(control);
    }

    if (consumedMessages >= WORKER_MESSAGE_LIMIT)
    {
        printf("Consumer %09" PRIuPTR ": Reached limit %d and stopped.\n",
               threadToId(pthread_self()),
               WORKER_MESSAGE_LIMIT);
    }
    atomic_store(&control->finished, 1);
    return NULL;
}

void consumeMessage(const Message* message, int extractedCount)
{
    Message copy = *message;
    uint16_t expectedHash = copy.hash;
    uint16_t actualHash;

    copy.hash = 0;
    actualHash = calculateHash(&copy);

    if (expectedHash == actualHash)
    {
        printf("Consumer %09" PRIuPTR ": Got message (type = %02X, hash = %04X, size = %03u). Total Extracted: %d\n",
               threadToId(pthread_self()),
               copy.type,
               expectedHash,
               (unsigned)copy.size,
               extractedCount);
    }
    else
    {
        fprintf(stderr,
                "Consumer %09" PRIuPTR ": HASH MISMATCH! Expected %04X, Got %04X\n",
                threadToId(pthread_self()),
                expectedHash,
                actualHash);
    }
}
