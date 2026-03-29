#pragma once

#include <pthread.h>

#if defined(__APPLE__) && !defined(PTHREAD_BARRIER_SERIAL_THREAD)
typedef int pthread_barrierattr_t;

#define PTHREAD_BARRIER_SERIAL_THREAD 1

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned threshold;
    unsigned count;
    unsigned generation;
} pthread_barrier_t;

static inline int pthread_barrier_init(pthread_barrier_t* barrier,
                                       const pthread_barrierattr_t* attr,
                                       unsigned count)
{
    (void)attr;
    if (count == 0)
    {
        return -1;
    }

    if (pthread_mutex_init(&barrier->mutex, NULL) != 0)
    {
        return -1;
    }
    if (pthread_cond_init(&barrier->condition, NULL) != 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }

    barrier->threshold = count;
    barrier->count = 0;
    barrier->generation = 0;
    return 0;
}

static inline int pthread_barrier_destroy(pthread_barrier_t* barrier)
{
    int rc = pthread_cond_destroy(&barrier->condition);
    int mutexRc = pthread_mutex_destroy(&barrier->mutex);

    return (rc != 0) ? rc : mutexRc;
}

static inline int pthread_barrier_wait(pthread_barrier_t* barrier)
{
    unsigned generation;

    pthread_mutex_lock(&barrier->mutex);
    generation = barrier->generation;
    ++barrier->count;

    if (barrier->count == barrier->threshold)
    {
        barrier->count = 0;
        ++barrier->generation;
        pthread_cond_broadcast(&barrier->condition);
        pthread_mutex_unlock(&barrier->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }

    while (generation == barrier->generation)
    {
        pthread_cond_wait(&barrier->condition, &barrier->mutex);
    }

    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}
#endif
