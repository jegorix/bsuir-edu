#define _XOPEN_SOURCE 500

#include "consumer.h"
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern MessageQueue* queue; // очередь сообщений
static volatile sig_atomic_t stopConsumer = 0; // флаг мягкого завершения consumer-процесса

static void consumerSignalHandler(int signo)
{
    (void)signo;
    stopConsumer = 1; // завершить цикл после текущей итерации
}

static void initConsumerSignals(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = consumerSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL); // остановка при завершении из родителя
    sigaction(SIGINT, &action, NULL);  // остановка при Ctrl+C
}

void createConsumer(Stack* consumers, int* semId)   // создать потребителя
{  
	fflush(stdout); // сбросить буфер, чтобы после fork не было дублирования вывода
	pid_t consumer = fork();
    switch (consumer) 
    {
        case 0:
            break;
        case -1:
            perror("fork");
            exit(errno);
        default:
            push(consumers, consumer);  // добавить потребителя в стек
            printf("Consumer (PID=%d) created successfully. Current consumers count: %d\n", consumer, consumers->size);
            return;
    }

    Message msg;
    int extractedCount;
    int consumedMessages = 0;
    initConsumerSignals(); // подключить обработчики SIGTERM/SIGINT
    while (!stopConsumer) 
    {
        if (semTryDown(semId, QUEUED_ITEMS_SEM) != 0)   // уменьшить счётчик семафора занятых мест
        {
            if (stopConsumer)
            {
                break;
            }
            usleep(100000); // защита от активного ожидания
            continue;
        }
        if (stopConsumer)
        {
            semUp(semId, QUEUED_ITEMS_SEM); // вернуть зарезервированный слот перед выходом
            break;
        }
        if (semTryDown(semId, MUTEX_SEM) != 0)          // занять мьютекс
        {
            semUp(semId, QUEUED_ITEMS_SEM); // откатить занятый слот, если мьютекс не получен
            if (stopConsumer)
            {
                break;
            }
            usleep(100000);
            continue;
        }
        if (stopConsumer)
        {
            semUp(semId, MUTEX_SEM);        // освободить мьютекс
            semUp(semId, QUEUED_ITEMS_SEM); // вернуть зарезервированный слот перед выходом
            break;
        }
        extractedCount = getMessage(&msg);
        if (stopConsumer && extractedCount >= 0)
        {
            rollbackGetMessage();           // вернуть сообщение в очередь без искажения счётчиков
            semUp(semId, MUTEX_SEM);        // освободить мьютекс
            semUp(semId, QUEUED_ITEMS_SEM); // восстановить счётчик занятых слотов
            break;
        }
        semUp(semId, MUTEX_SEM);            // освободить мьютекс
        if (extractedCount >= 0)
        {
            semUp(semId, FREE_SLOTS_SEM);       // увеличить счётчик семафора свободных мест
        }
        else
        {
            semUp(semId, QUEUED_ITEMS_SEM); // откатить семафор, если извлечение не выполнилось
        }
        if (extractedCount >= 0)
        {
            consumeMessage(&msg);
            ++consumedMessages;
            printf("Pid: %d consume message: type = %02X | hash = %04X | size = %03d | extractedCount = %d\n", getpid(), msg.type, msg.hash, msg.size, extractedCount);
            fflush(stdout);
        }
        if (stopConsumer)
        {
            break;
        }
        if (consumedMessages >= WORKER_MESSAGE_LIMIT)
        {
            printf("Consumer (PID=%d) reached limit of %d messages and stopped.\n", getpid(), WORKER_MESSAGE_LIMIT);
            fflush(stdout);
            break;
        }
        sleep(1);
    }
    _exit(0); // дочерний процесс завершает работу без возврата в код родителя
}

void deleteConsumer(Stack* consumers)   // удалить потребителя
{
    pid_t pid = pop(consumers);
    if (pid > 0)
    {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        printf("Consumer (PID=%d) removed successfully. Current consumers count: %d\n", pid, consumers->size);
    }
}

void consumeMessage(Message *msg)   // потребить сообщение
{
    uint16_t msgHash = msg->hash;
    msg->hash = 0;
    uint16_t checkSum = calculateHash(msg);
    if (msgHash != checkSum) // проверка на совпадение контрольных данных при перерасчёте
    {
        fprintf(stderr, "checkSum = %X not equal msgHash = %X\n", checkSum, msgHash);
    }
    msg->hash = msgHash;
}
