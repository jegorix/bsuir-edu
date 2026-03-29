#pragma once

#include "message.h"

void* producerSem(void* arg);
void* producerCond(void* arg);
Message* produceMessage(unsigned int* seed);
