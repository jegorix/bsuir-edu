#include "message.h"

size_t getMessageDataLength(const Message* message)
{
    return (size_t)message->size + 1U;
}

size_t getAlignedDataLength(const Message* message)
{
    size_t dataLength = getMessageDataLength(message);
    return ((dataLength + 3U) / 4U) * 4U;
}

uint16_t calculateHash(const Message* message)
{
    Message normalized = *message;
    const uint8_t* bytes;
    size_t meaningfulLength;
    uint16_t hash = 0;

    normalized.hash = 0;
    meaningfulLength = offsetof(Message, data) + getMessageDataLength(&normalized);
    bytes = (const uint8_t*)&normalized;

    for (size_t index = 0; index < meaningfulLength; ++index)
    {
        hash = (uint16_t)(((uint32_t)hash * 33U) ^ bytes[index]);
    }

    return hash;
}
