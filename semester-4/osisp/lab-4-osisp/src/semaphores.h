#pragma once

#include <sys/ipc.h>

#include "queue.h"
#include "stack.h"

#define SHM_KEY 0x1234  // ключ разделяемой памяти
#define SEM_KEY 0x5678  // ключ семофоров
#define SHM_FLAGS (IPC_CREAT | 0600)    // флаг для разделяемой памяти
#define SEM_FLAGS (IPC_CREAT | 0600)    // флаг для семафоров

void initSemaphores(int* semId);    // создание разделяемой памяти, иницилизация очереди сообщений, создание набора семафоров
void deleteSemaphores(Stack* producers, Stack* consumers, int* semId);  // освобождение всей памяти
