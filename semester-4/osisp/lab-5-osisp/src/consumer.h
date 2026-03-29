#pragma once

#include "message.h"

void* consumerSem(void* arg);
void* consumerCond(void* arg);
void consumeMessage(const Message* message, int extractedCount);
