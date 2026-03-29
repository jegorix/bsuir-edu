#define _XOPEN_SOURCE 500

#include "semaphores.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

union semun // объединение для управления семафорами
{
    int val;                // установка значения семафора
    struct semid_ds *buf;   // получение информации о семафоре
    unsigned short *array;  // для операций над массивом семафоров
};

extern MessageQueue* queue; // очередь сообщений

void initSemaphores(int* semId) 
{
	int shmid = shmget(SHM_KEY, sizeof(MessageQueue), SHM_FLAGS);  // создание разделяемой памяти для хранения очереди сообщений
    if (shmid == -1) 
    {
        perror("shmget");
        exit(errno);
    }

    queue = (MessageQueue*)shmat(shmid, NULL, 0);   // присоединение разделяемой памяти
    if (queue == (void*)-1) 
    {
        perror("shmat");
        exit(errno);
    }
    *queue = initQueue();      // инициализация очереди сообщений
	
    *semId = semget(SEM_KEY, 3, SEM_FLAGS); // создание 3 семафоров
    if (*semId == -1) 
    {
        perror("semget");
        exit(errno);
    }

    union semun arg;
    // 0 - Мьютекс
    arg.val = 1;  // установка доступности: 1 == true == доступен
    if (semctl(*semId, MUTEX_SEM, SETVAL, arg) == -1) 
    {  
        perror("semctl mutex");
        exit(errno);
    }

    // 1 - Свободные слоты
    arg.val = MAX_MESSAGES; // установка начального знаачения
    if (semctl(*semId, FREE_SLOTS_SEM, SETVAL, arg) == -1) 
    {
        perror("semctl free_space");
        exit(errno);
    }

    // 2 - Сообщения в очереди
    arg.val = 0;    // установкак начального значения
    if (semctl(*semId, QUEUED_ITEMS_SEM, SETVAL, arg) == -1) 
    {
        perror("semctl items");
        exit(errno);
    }
}

void deleteSemaphores(Stack* producers, Stack* consumers, int* semId)   // освобождение всей памяти
{
	while (producers->size != 0)  // удаление производителей
	{
		pid_t pid = pop(producers);
		if (pid > 0)
		{
			kill(pid, SIGTERM);
			waitpid(pid, NULL, 0);
		}
	}
	while (consumers->size != 0) // удаление потребителей
	{
		pid_t pid = pop(consumers);
		if (pid > 0)
		{
			kill(pid, SIGTERM);
			waitpid(pid, NULL, 0);
		}
	}

	int shmid = shmget(SHM_KEY, 0, 0);
	if (queue != NULL && queue != (void*)-1)
	{
        shmdt(queue);  // отсоединить разделяемую память от текущего процесса
        queue = NULL;
    }

	if (shmid != -1) 
	{
		shmctl(shmid, IPC_RMID, NULL);    // удаление разделяемой памяти
	}
	if (*semId != -1) 
	{
		semctl(*semId, 0, IPC_RMID);  // удаление семафоров
		*semId = -1;
	}
}
