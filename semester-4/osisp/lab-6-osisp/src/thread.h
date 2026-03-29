#pragma once
#include "index.h"
#include <pthread.h>

typedef struct // структура информации о потоке
{
	Index* address;		// указатель на начало блока данных в памяти
	int blockSize;		// размер блока в байтах
	int blocksCount;	// общее количество блоков
	int indexesInBlock;	// количество записей Index в одном блоке
	int id;
} ThreadInfo;

ThreadInfo* initThreadInfo(int threadCount, int blockSize, int blockCount, int indexesInBlock);	// создаёт и заполняет массив структур ThreadInfo для всех потоков
pthread_t* initThreads(int threadCount);	// выделяет память под массив id потоков

void joinThreads(pthread_t* threads, int threadCount);	// ожидание завершения всех созданных потоков
