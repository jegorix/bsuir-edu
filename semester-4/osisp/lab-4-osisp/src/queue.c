#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>


extern MessageQueue* queue; // очередь сообщений

size_t getMessageDataLength(const Message* msg)
{
    return (size_t)msg->size + 1U;
}

size_t getAlignedDataLength(const Message* msg)
{
    size_t dataLength = getMessageDataLength(msg);
    return ((dataLength + 3U) / 4U) * 4U;
}

uint16_t calculateHash(const Message* msg)    // вычислить контрольные данные
{
    Message normalized = *msg;
    const uint8_t* bytes = (const uint8_t*)&normalized;
    uint16_t hash = 0;
    size_t meaningfulLength = offsetof(Message, data) + getMessageDataLength(&normalized);

    normalized.hash = 0;

    // В расчёт входят только осмысленные байты сообщения: заголовок и
    // фактические данные длиной size + 1, без хвостового выравнивания.
    for (size_t i = 0; i < meaningfulLength; ++i) {
        hash = (hash * 33) ^ bytes[i];
    }
    return hash;
}

MessageQueue initQueue(void)    // инициализировать очередь сообщений
{
	MessageQueue queue;

    queue.addedCount = 0;
    queue.extractedCount = 0;
    queue.messageCount = 0;
    queue.freeCount = MAX_MESSAGES;
    queue.head = 0;
    queue.tail = 0;
    queue.freeTop = MAX_MESSAGES; // на старте свободны все ячейки пула
    for (int i = 0; i < MAX_MESSAGES; ++i)
    {
        queue.buffer[i] = NULL; // NULL означает пустую позицию в кольцевом буфере
        queue.freeIndexes[i] = MAX_MESSAGES - i - 1; // индексы складываются как стек
    }
    memset(queue.storage, 0, sizeof(queue.storage));
    memset(queue.buffer, 0, sizeof(queue.buffer));
    return queue;
}

int putMessage(Message* msg)    // добавить сообщение в очередь
{
    if (queue->freeTop == 0)
    {
        return -1; // внутренняя защита: в пуле нет свободных ячеек
    }

    int freeIndex = queue->freeIndexes[queue->freeTop - 1]; // взять свободную ячейку из пула
    --queue->freeTop;                                        // снять ячейку со стека свободных
    queue->storage[freeIndex] = *msg;                        // скопировать сообщение в shared memory

    if (queue->head == MAX_MESSAGES) 
    {
        queue->head = 0; // переход в начало кольцевого буфера
    }
    queue->buffer[queue->head] = &queue->storage[freeIndex]; // положить в кольцо указатель на сообщение
    ++queue->head;
    ++queue->messageCount; // стало на одно сообщение больше
    --queue->freeCount;    // свободных слотов стало на один меньше
    ++queue->addedCount;   // общий счетчик добавленных сообщений
    return queue->addedCount;
}

int getMessage(Message* msg)    // извлечь сообщение из очереди
{
    if (queue->tail == MAX_MESSAGES) 
    {
        queue->tail = 0; // переход в начало кольцевого буфера
    }

    Message* queuedMessage = queue->buffer[queue->tail];
    if (queuedMessage == NULL)
    {
        return -1; // внутренняя защита: в очереди нет указателя на сообщение
    }

    *msg = *queuedMessage;              // скопировать сообщение вызывающему коду
    queue->buffer[queue->tail] = NULL;  // позиция в кольце стала пустой

    int freeIndex = (int)(queuedMessage - queue->storage); // восстановить индекс ячейки пула по указателю
    if (queue->freeTop < MAX_MESSAGES)
    {
        queue->freeIndexes[queue->freeTop] = freeIndex; // вернуть ячейку в стек свободных
        ++queue->freeTop;
    }

    ++queue->tail;
    --queue->messageCount;   // стало на одно сообщение меньше
    ++queue->freeCount;      // свободных слотов стало на один больше
    ++queue->extractedCount; // общий счетчик извлеченных сообщений

    return queue->extractedCount;
}

void rollbackGetMessage(void)
{
    if (queue->freeTop <= 0)
    {
        return; // откатывать нечего
    }

    if (queue->tail == 0)
    {
        queue->tail = MAX_MESSAGES;
    }

    --queue->tail;

    int freeIndex = queue->freeIndexes[queue->freeTop - 1];
    --queue->freeTop;
    queue->buffer[queue->tail] = &queue->storage[freeIndex];
    ++queue->messageCount;
    --queue->freeCount;
    --queue->extractedCount;
}

void semDown(int* semId, int semNum)    // "захват" семафора
{
    struct sembuf op = {semNum, -1, 0}; // уменьшить семафор на 1
    if (semop(*semId, &op, 1)) 
    {
        perror("semop down");
        exit(EXIT_FAILURE);
    }
}

int semTryDown(int* semId, int semNum)
{
    struct sembuf op = {semNum, -1, IPC_NOWAIT};
    if (semop(*semId, &op, 1) == 0)
    {
        return 0; // успешно уменьшили семафор
    }
    if (errno == EAGAIN || errno == EINTR)
    {
        return 1; // семафор недоступен или операция прервана, пробуем позже
    }
    // Все остальные ошибки считаются критическими для работы программы.
    perror("semop try down");
    exit(EXIT_FAILURE);
}

void semUp(int* semId, int semNum) // "освобождение" семафора
{
    struct sembuf op = {semNum, 1, 0};  // увеличить семафор
    if (semop(*semId, &op, 1)) 
    {
        perror("semop up");
        exit(EXIT_FAILURE);
    }
}
