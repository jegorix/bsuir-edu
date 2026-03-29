#pragma once

#include "queue.h"
#include "stack.h"

void createConsumer(Stack* consumers, int* semId);  // создать потребителя
void deleteConsumer(Stack* consumers);  // удалить потребителя

void consumeMessage(Message *msg);  // потребить сообщение
