#define _POSIX_C_SOURCE 200809L

#include "consumer.h"
#include "producer.h"
#include "queue_cond.h"
#include "queue_sem.h"
#include "stack.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INITIAL_QUEUE_CAPACITY 10
#define MAX_THREADS 100
#define STATUS_BOX_INNER_WIDTH 62
#define STATUS_BAR_WIDTH 24

typedef enum
{
    MODE_SEM = 1,
    MODE_COND = 2
} LabMode;

static void showStatusSem(void);
static void showStatusCond(void);
static void runLab(void (*initFunc)(int),
                   void* (*producerFunc)(void*),
                   void* (*consumerFunc)(void*),
                   void (*showStatusFunc)(void),
                   void (*resizeFunc)(int),
                   void (*deleteFunc)(void),
                   LabMode mode,
                   const char* modeName);
static void reapFinishedThreads(void);
static void reapFinishedThreadStack(Stack* stack, const char* role);

atomic_bool keepRunning = ATOMIC_VAR_INIT(1);

Stack producerThreads;
Stack consumerThreads;

QueueSem queueSem;
QueueCond queueCond;

static void printMainMenu(void)
{
    printf("\n+================================================================+\n");
    printf("|                    LAB 05 - THREAD QUEUE LAB                   |\n");
    printf("|               producers, consumers, synchronization            |\n");
    printf("+================================================================+\n");
    printf("| Select mode:                                                   |\n");
    printf("|   [1] Lab 5.1  POSIX semaphores + mutex                        |\n");
    printf("|   [2] Lab 5.2  Condition variables + mutex                     |\n");
    printf("|   [q] Exit                                                     |\n");
    printf("+----------------------------------------------------------------+\n");
    printf("[Select] > ");
}

static void printLabMenu(const char* modeName, LabMode mode)
{
    int capacity = 0;
    int occupied = 0;
    int pendingShrink = 0;
    char line[128];

    if (mode == MODE_SEM)
    {
        pthread_mutex_lock(&queueSem.mutex);
        capacity = queueSem.capacity;
        occupied = queueSem.size;
        pendingShrink = queueSem.pendingShrink;
        pthread_mutex_unlock(&queueSem.mutex);
    }
    else
    {
        pthread_mutex_lock(&queueCond.mutex);
        capacity = queueCond.capacity;
        occupied = queueCond.size;
        pendingShrink = queueCond.pendingShrink;
        pthread_mutex_unlock(&queueCond.mutex);
    }

    printf("\n+================================================================+\n");
    printf("| %-62s |\n", modeName);
    printf("+----------------------------------------------------------------+\n");
    snprintf(line, sizeof(line), "Queue: %d/%d   Free: %d   Pending shrink: %d",
             occupied, capacity, capacity - occupied, pendingShrink);
    printf("| %-62s |\n", line);
    snprintf(line, sizeof(line), "Threads: producers=%d consumers=%d", producerThreads.size, consumerThreads.size);
    printf("| %-62s |\n", line);
    printf("+----------------------------------------------------------------+\n");
    printf("| Commands:                                                      |\n");
    printf("|   [p] add producer      [P] remove producer                    |\n");
    printf("|   [c] add consumer      [C] remove consumer                    |\n");
    printf("|   [+] grow queue by 1   [-] shrink queue by 1                  |\n");
    printf("|   [s] detailed status   [q] return to main menu                |\n");
    printf("+----------------------------------------------------------------+\n");
}

static void wakeWaitingThreads(LabMode mode)
{
    if (mode == MODE_COND)
    {
        pthread_mutex_lock(&queueCond.mutex);
        pthread_cond_broadcast(&queueCond.notFull);
        pthread_cond_broadcast(&queueCond.notEmpty);
        pthread_mutex_unlock(&queueCond.mutex);
    }
}

static void requestStop(ThreadControl* control)
{
    atomic_store(&control->stopRequested, 1);
}

static void destroyPoppedControl(Stack* stack)
{
    ThreadControl* control = popStack(stack);

    destroyThreadControl(control);
}

static void reapFinishedThreadStack(Stack* stack, const char* role)
{
    StackElem* current = stack->head;
    StackElem* previous = NULL;

    while (current != NULL)
    {
        ThreadControl* control = current->control;

        if (!atomic_load(&control->finished))
        {
            previous = current;
            current = current->next;
            continue;
        }

        StackElem* finishedElem = current;
        StackElem* next = current->next;
        uintptr_t id = threadToId(control->thread);

        if (previous == NULL)
        {
            stack->head = next;
        }
        else
        {
            previous->next = next;
        }
        current = next;
        --stack->size;

        if (pthread_join(control->thread, NULL) != 0)
        {
            perror("Error joining completed thread");
            free(finishedElem);
            destroyThreadControl(control);
            exit(EXIT_FAILURE);
        }

        printf("%s %09" PRIuPTR " completed %d messages and was joined\n",
               role,
               id,
               WORKER_MESSAGE_LIMIT);
        destroyThreadControl(control);
        free(finishedElem);
    }
}

static void reapFinishedThreads(void)
{
    reapFinishedThreadStack(&producerThreads, "Producer");
    reapFinishedThreadStack(&consumerThreads, "Consumer");
}

static void stopAndJoinThread(ThreadControl* control, const char* role, LabMode mode)
{
    void* result = NULL;
    uintptr_t id = threadToId(control->thread);

    requestStop(control);
    wakeWaitingThreads(mode);

    if (pthread_join(control->thread, &result) != 0)
    {
        perror("Error joining thread");
        destroyThreadControl(control);
        exit(EXIT_FAILURE);
    }

    printf("%s %09" PRIuPTR " was terminated\n", role, id);
    destroyThreadControl(control);
}

static void stopAllThreads(Stack* stack, const char* role, LabMode mode)
{
    ThreadControl* control;

    while ((control = popStack(stack)) != NULL)
    {
        stopAndJoinThread(control, role, mode);
    }
}

static void printStatusBorder(void)
{
    printf("+----------------------------------------------------------------+\n");
}

static void printStatusLine(const char* text)
{
    printf("| %-*s |\n", STATUS_BOX_INNER_WIDTH, text);
}

static void printLoadLine(int occupied, int capacity)
{
    char bar[STATUS_BAR_WIDTH + 1];
    char line[128];
    int percent = 0;
    int filled = 0;

    if (capacity > 0)
    {
        filled = (occupied * STATUS_BAR_WIDTH) / capacity;
        percent = (occupied * 100) / capacity;
    }
    if (filled < 0)
    {
        filled = 0;
    }
    if (filled > STATUS_BAR_WIDTH)
    {
        filled = STATUS_BAR_WIDTH;
    }

    for (int index = 0; index < STATUS_BAR_WIDTH; ++index)
    {
        bar[index] = (index < filled) ? '#' : '.';
    }
    bar[STATUS_BAR_WIDTH] = '\0';

    snprintf(line, sizeof(line), "Load: [%s] %d/%d (%d%%)", bar, occupied, capacity, percent);
    printStatusLine(line);
}

int main(void)
{
    srand((unsigned int)time(NULL));

    while (1)
    {
        char choice[16];

        printMainMenu();
        if (fgets(choice, sizeof(choice), stdin) == NULL)
        {
            break;
        }

        if (choice[0] == '1')
        {
            runLab(initQueueSem,
                   producerSem,
                   consumerSem,
                   showStatusSem,
                   resizeQueueSem,
                   deleteQueueSem,
                   MODE_SEM,
                   "Lab 5.1 - POSIX semaphores");
        }
        else if (choice[0] == '2')
        {
            runLab(initQueueCond,
                   producerCond,
                   consumerCond,
                   showStatusCond,
                   resizeQueueCond,
                   deleteQueueCond,
                   MODE_COND,
                   "Lab 5.2 - condition variables");
        }
        else if (choice[0] == 'q' || choice[0] == 'Q')
        {
            break;
        }
        else
        {
            printf("Incorrect option. Please try again.\n");
        }
    }

    return 0;
}

static void showStatusSem(void)
{
    char line[128];

    pthread_mutex_lock(&queueSem.mutex);
    printStatusBorder();
    printStatusLine("Status snapshot: POSIX semaphores");
    printStatusBorder();

    snprintf(line, sizeof(line), "Capacity: %d", queueSem.capacity);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Occupied: %d", queueSem.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Free slots: %d", queueSem.capacity - queueSem.size);
    printStatusLine(line);
    printLoadLine(queueSem.size, queueSem.capacity);
    snprintf(line, sizeof(line), "Total added: %d", queueSem.addedCount);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Total extracted: %d", queueSem.extractedCount);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Producers: %d", producerThreads.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Consumers: %d", consumerThreads.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Pending shrink: %d", queueSem.pendingShrink);
    printStatusLine(line);
    printStatusBorder();
    pthread_mutex_unlock(&queueSem.mutex);
}

static void showStatusCond(void)
{
    char line[128];

    pthread_mutex_lock(&queueCond.mutex);
    printStatusBorder();
    printStatusLine("Status snapshot: condition variables");
    printStatusBorder();

    snprintf(line, sizeof(line), "Capacity: %d", queueCond.capacity);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Occupied: %d", queueCond.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Free slots: %d", queueCond.capacity - queueCond.size);
    printStatusLine(line);
    printLoadLine(queueCond.size, queueCond.capacity);
    snprintf(line, sizeof(line), "Total added: %d", queueCond.addedCount);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Total extracted: %d", queueCond.extractedCount);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Producers: %d", producerThreads.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Consumers: %d", consumerThreads.size);
    printStatusLine(line);
    snprintf(line, sizeof(line), "Pending shrink: %d", queueCond.pendingShrink);
    printStatusLine(line);
    printStatusBorder();
    pthread_mutex_unlock(&queueCond.mutex);
}

static void runLab(void (*initFunc)(int),
                   void* (*producerFunc)(void*),
                   void* (*consumerFunc)(void*),
                   void (*showStatusFunc)(void),
                   void (*resizeFunc)(int),
                   void (*deleteFunc)(void),
                   LabMode mode,
                   const char* modeName)
{
    producerThreads = initStack();
    consumerThreads = initStack();
    atomic_store(&keepRunning, 1);

    initFunc(INITIAL_QUEUE_CAPACITY);
    printf("Queue initialized with capacity %d\n", INITIAL_QUEUE_CAPACITY);

    while (atomic_load(&keepRunning))
    {
        char line[32];
        char cmd = '\0';

        reapFinishedThreads();
        printLabMenu(modeName, mode);
        printf("[Command] > ");
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            atomic_store(&keepRunning, 0);
            break;
        }
        if (sscanf(line, " %c", &cmd) != 1)
        {
            continue;
        }

        reapFinishedThreads();

        switch (cmd)
        {
            case 'p':
            {
                ThreadControl* producer;

                if (producerThreads.size >= MAX_THREADS)
                {
                    printf("Max number of producers reached.\n");
                    break;
                }

                producer = pushStack(&producerThreads);
                producer->seed = (unsigned int)rand();
                if (pthread_create(&producer->thread, NULL, producerFunc, producer) != 0)
                {
                    perror("Error creating producer thread");
                    destroyPoppedControl(&producerThreads);
                    exit(EXIT_FAILURE);
                }
                printf("Producer %09" PRIuPTR " was created\n", threadToId(producer->thread));
                break;
            }

            case 'c':
            {
                ThreadControl* consumer;

                if (consumerThreads.size >= MAX_THREADS)
                {
                    printf("Max number of consumers reached.\n");
                    break;
                }

                consumer = pushStack(&consumerThreads);
                consumer->seed = (unsigned int)rand();
                if (pthread_create(&consumer->thread, NULL, consumerFunc, consumer) != 0)
                {
                    perror("Error creating consumer thread");
                    destroyPoppedControl(&consumerThreads);
                    exit(EXIT_FAILURE);
                }
                printf("Consumer %09" PRIuPTR " was created\n", threadToId(consumer->thread));
                break;
            }

            case 'P':
            {
                ThreadControl* producer = popStack(&producerThreads);

                if (producer == NULL)
                {
                    printf("There are no active producers to remove.\n");
                    break;
                }
                stopAndJoinThread(producer, "Producer", mode);
                break;
            }

            case 'C':
            {
                ThreadControl* consumer = popStack(&consumerThreads);

                if (consumer == NULL)
                {
                    printf("There are no active consumers to remove.\n");
                    break;
                }
                stopAndJoinThread(consumer, "Consumer", mode);
                break;
            }

            case '+':
                resizeFunc(1);
                break;

            case '-':
                resizeFunc(-1);
                break;

            case 's':
                showStatusFunc();
                break;

            case 'q':
                atomic_store(&keepRunning, 0);
                break;

            default:
                printf("Unknown command. Use p/P/c/C/+/-/s/q.\n");
                break;
        }
    }

    atomic_store(&keepRunning, 0);
    wakeWaitingThreads(mode);
    stopAllThreads(&producerThreads, "Producer", mode);
    stopAllThreads(&consumerThreads, "Consumer", mode);

    deleteFunc();
    printf("All threads (%s) are completed.\n", modeName);
}
