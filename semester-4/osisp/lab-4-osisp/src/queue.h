#pragma once

#include <stddef.h>
#include <stdint.h>

#define MAX_DATA (((256 + 3) / 4) * 4)
#define MAX_MESSAGES 10
#define WORKER_MESSAGE_LIMIT 10

#define MUTEX_SEM 0         // семафор-мьютекс
#define FREE_SLOTS_SEM 1    // семафор свободных мест
#define QUEUED_ITEMS_SEM 2  // семафор занятых мест

typedef struct __attribute__((packed)) // структура сообщения
{
    uint8_t type;           // тип сообщения
    uint16_t hash;          // контрольные данные
    uint8_t size;           // длина данных в байтах (от 0 до 255)
    uint8_t data[MAX_DATA]; // данные сообщения
} Message;

_Static_assert(offsetof(Message, type) == 0, "Message.type must be at offset 0");
_Static_assert(offsetof(Message, hash) == 1, "Message.hash must be at offset 1");
_Static_assert(offsetof(Message, size) == 3, "Message.size must be at offset 3");
_Static_assert(offsetof(Message, data) == 4, "Message.data must be at offset 4");

typedef struct  // структура очереди сообщений
{
    int addedCount;     // количество добавленных сообщений
    int extractedCount; // количество извлеченных сообщений
    int messageCount;   // количество сообщений в очереди
    int freeCount;      // количество свободных мест в очереди
    int head;           // позиция для добавления нового указателя
    int tail;           // позиция для извлечения указателя
    int freeTop;        // вершина стека свободных индексов в storage
    Message* buffer[MAX_MESSAGES];   // кольцевой буфер указателей на сообщения
    Message storage[MAX_MESSAGES];   // пул сообщений в разделяемой памяти
    int freeIndexes[MAX_MESSAGES];   // стек свободных индексов пула
} MessageQueue;

size_t getMessageDataLength(const Message* msg);      // получить реальный размер данных (size + 1)
size_t getAlignedDataLength(const Message* msg);      // получить размер данных, выровненный по 4 байтам
uint16_t calculateHash(const Message* msg);           // вычислить контрольные данные
MessageQueue initQueue(void);                         // инициализировать очередь сообщений

int putMessage(Message* msg);   // добавить сообщение в очередь
int getMessage(Message* msg);   // извлечь сообщение из очереди
void rollbackGetMessage(void);  // откатить последнее извлечение сообщения под мьютексом

void semDown(int* semId, int semNum);   // "захват семафора" (уменьшение значения)
int semTryDown(int* semId, int semNum); // неблокирующая попытка уменьшить семафор
void semUp(int* semId, int semNum);     // "освобождение семафора" (увеличение значения)
