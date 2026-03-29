#pragma once

#include "queue.h"
#include "stack.h"

void createProducer(Stack* producers, int* semId);  // создать производителя
void deleteProducer(Stack* producers);  // удалить производителя

void produceMessage(Message *msg);  // произвести сообщение
