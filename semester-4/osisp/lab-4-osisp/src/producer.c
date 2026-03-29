#define _XOPEN_SOURCE 500

#include "producer.h"
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

extern MessageQueue* queue; // очередь сообщений
static volatile sig_atomic_t stopProducer = 0; // флаг мягкого завершения producer-процесса

static void producerSignalHandler(int signo)
{
    (void)signo;
    stopProducer = 1; // выставить флаг и завершить цикл после текущей итерации
}

static void initProducerSignals(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = producerSignalHandler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL); // остановка при завершении из родителя
    sigaction(SIGINT, &action, NULL);  // остановка при Ctrl+C
}

void createProducer(Stack* producers, int* semId)   // создать производителя
{
	fflush(stdout); // сбросить буфер, чтобы после fork не было дублирования вывода
	pid_t producer = fork();
	switch (producer) 
    {
        case 0: 
            break;
        case -1:
            perror("fork");
            exit(errno);
        default:
			push(producers, producer);   // добавить производителя в стек
            printf("Producer (PID=%d) created successfully. Current producers count: %d\n", producer, producers->size);
            return;
    }

    int addedCount;
    int producedMessages = 0;
    Message msg;
    srand((unsigned int)(time(NULL) ^ getpid())); // индивидуальный seed на процесс
    initProducerSignals(); // подключить обработчики SIGTERM/SIGINT

    while (!stopProducer) 
    {
        produceMessage(&msg);       // сгенерировать сообщение
        if (semTryDown(semId, FREE_SLOTS_SEM) != 0) // уменьнить счётчик семафора свободных мест
        {
            if (stopProducer)
            {
                break;
            }
            usleep(100000); // защита от активного ожидания при пустом/полном буфере
            continue;
        }
        if (stopProducer)
        {
            semUp(semId, FREE_SLOTS_SEM); // вернуть зарезервированный слот перед выходом
            break;
        }
        if (semTryDown(semId, MUTEX_SEM) != 0)      // занять мьютекс
        {
            semUp(semId, FREE_SLOTS_SEM); // откатить свободный слот, если мьютекс не захвачен
            if (stopProducer)
            {
                break;
            }
            usleep(100000);
            continue;
        }
        if (stopProducer)
        {
            semUp(semId, MUTEX_SEM);      // освободить мьютекс
            semUp(semId, FREE_SLOTS_SEM); // вернуть зарезервированный слот перед выходом
            break;
        }
        addedCount = putMessage(&msg);
        semUp(semId, MUTEX_SEM);        // освободить мьютекс
        if (addedCount >= 0)
        {
            semUp(semId, QUEUED_ITEMS_SEM); // увеличить счётчик семафора занятых мест
        }
        else
        {
            semUp(semId, FREE_SLOTS_SEM); // откатить семафор, если сообщение не добавилось
        }

        if (addedCount >= 0)
        {
            ++producedMessages;
            printf("Pid: %d produce message: type = %02X | hash = %04X | size = %03d | addedCount = %d\n", getpid(), msg.type, msg.hash, msg.size, addedCount);
            fflush(stdout);
        }
        if (stopProducer)
        {
            break;
        }
        if (producedMessages >= WORKER_MESSAGE_LIMIT)
        {
            printf("Producer (PID=%d) reached limit of %d messages and stopped.\n", getpid(), WORKER_MESSAGE_LIMIT);
            fflush(stdout);
            break;
        }

        sleep(1);
    }
    _exit(0); // дочерний процесс завершает работу без повторного выполнения кода родителя
}

void deleteProducer(Stack* producers)   // удалить производителя
{
    pid_t pid = pop(producers);
    if (pid > 0)
    {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        printf("Producer (PID=%d) removed successfully. Current producers count: %d\n", pid, producers->size);
    }
}

void produceMessage(Message* msg)   // произвести сообщение
{
    size_t alignedSize;

    memset(msg, 0, sizeof(Message));      // обнулить всю структуру

    msg->type = (uint8_t)(rand() % 256);   // вычислить тип сообщения
    msg->size = (uint8_t)(rand() % 256);   // сохранить размер данных в диапазоне [0, 255]
    alignedSize = getAlignedDataLength(msg);

    for (size_t i = 0; i < alignedSize; ++i) // data имеет длину, кратную 4 байтам
    {
        msg->data[i] = (uint8_t)(rand() % 256);   // заполнить данные сообщения случайными байтами
    }
    msg->hash = 0;                      // при вычислении контрольных данных hash равен нулю
    msg->hash = calculateHash(msg);    // вычислить контрольные данные сообщения
}
