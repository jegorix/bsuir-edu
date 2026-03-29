#pragma once

#include <stddef.h>
#include <stdint.h>

#define MAX_MESSAGE_DATA_LENGTH 256U
#define MAX_ALIGNED_DATA_LENGTH (((MAX_MESSAGE_DATA_LENGTH) + 3U) / 4U * 4U)
#define WORKER_MESSAGE_LIMIT 10

typedef struct __attribute__((packed))
{
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    uint8_t data[MAX_ALIGNED_DATA_LENGTH];
} Message;

_Static_assert(offsetof(Message, type) == 0, "Message.type must be at offset 0");
_Static_assert(offsetof(Message, hash) == 1, "Message.hash must be at offset 1");
_Static_assert(offsetof(Message, size) == 3, "Message.size must be at offset 3");
_Static_assert(offsetof(Message, data) == 4, "Message.data must be at offset 4");

size_t getMessageDataLength(const Message* message);
size_t getAlignedDataLength(const Message* message);
uint16_t calculateHash(const Message* message);
