#include "thread.h"
#include "thread.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

ThreadInfo* initThreadInfo(int threadCount, int blockSize, int blocksCount, int indexesInBlock)	// создаёт и заполняет массив структур ThreadInfo для всех потоков
{
	ThreadInfo* info = (ThreadInfo*)calloc(threadCount, sizeof(ThreadInfo));
	if(info == NULL)
	{
		perror("Не удалось выделить память");
		exit(EXIT_FAILURE);
	}
	
	for(int i = 0; i < threadCount; ++i)
	{
		info[i].blockSize = blockSize;
		info[i].blocksCount = blocksCount;
		info[i].indexesInBlock = indexesInBlock;
		info[i].id = i;
	}
	
	return info;
}

pthread_t* initThreads(int threadCount)	// выделение памяти под массив id потоков
{
	pthread_t* threads = (pthread_t*)malloc(threadCount * sizeof(pthread_t));
	if(threads == NULL)
	{
		perror("Не удалось выделить память");
		exit(EXIT_FAILURE);
	}
	
	return threads;
}

void joinThreads(pthread_t* threads, int threadCount)	// ожидание завершения всех созданных потоков
{
	for(int i = 1; i < threadCount; ++ i)	// для всех потоков
	{
		int error = pthread_join(threads[i], NULL);
		if(error != 0)	// ожидание завершения
		{
			fprintf(stderr, "Не удалось дождаться завершения потока: %s\n", strerror(error));
			exit(EXIT_FAILURE);
		}
	}
}
