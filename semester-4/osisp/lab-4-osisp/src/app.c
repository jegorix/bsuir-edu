#define _XOPEN_SOURCE 500

#include "semaphores.h"
#include "producer.h"
#include "consumer.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

MessageQueue* queue = NULL;
static volatile sig_atomic_t stopRequested = 0;

void printStatus(Stack producers, Stack consumers);	// вывести статистику
void printMenu(Stack producers, Stack consumers);	// вывести меню управления
void handleAppSignal(int sig);                     // обработать завершение основного процесса
void installAppSignals(void);                     // установить обработчики SIGINT/SIGTERM

int main(void) 
{
	Stack producers = initStack();	// инициализировать стек производителей
	Stack consumers = initStack();	// инициализировать стек потребителей
	int semId = -1;
	char option = '\0';
	char input[32];
	setvbuf(stdout, NULL, _IONBF, 0);	// отключить буферизацию stdout для корректного вывода из процессов
	installAppSignals();
	initSemaphores(&semId);	// инициализировать семафор
	    
    while (!stopRequested)
    {
		printMenu(producers, consumers);	// вывести меню и текущую сводку
		if (fgets(input, sizeof(input), stdin) == NULL)
		{
			if (ferror(stdin) && errno == EINTR)
			{
				clearerr(stdin);
				continue;
			}
			option = 'q';	// завершить работу при EOF
		}
		else
		{
			option = input[0];
		}
	              
		switch (option) 
		{
			case '1':
			case 'p':
			case 'P':
				createProducer(&producers, &semId);	// создать производителя
				break;	
			case '2':
			case 'o':
			case 'O':
				if (producers.size > 0) 
				{
					deleteProducer(&producers);	// удалить производителя
				} 
				else 
				{
					printf("No producers to remove\n");
				}
				break;
			case '3':
			case 'c':
			case 'C':
				createConsumer(&consumers, &semId);	// создать потребителя
				break;
			case '4':
			case 'x':
			case 'X':
				if (consumers.size > 0) 
				{
					deleteConsumer(&consumers);	// удалить потребителя
				} 
				else 
				{
					printf("No consumers to remove\n");
				}
				break;
			case '5':
			case 's':
			case 'S':
			case 'l':
			case 'L':
				printStatus(producers, consumers);	// вывести статистику
				break;
			case '0':
			case 'q':
			case 'Q':
				stopRequested = 1;
				break;
			case '\n':
			case '\0':
				break;
			default:
				printf("Invalid option\n");
				break;
		}
		        
    }
	    
    deleteSemaphores(&producers, &consumers, &semId);	// освободить память
    return 0;
}

void handleAppSignal(int sig)
{
	(void)sig;
	stopRequested = 1;
}

void installAppSignals(void)
{
	struct sigaction action;

	action.sa_handler = handleAppSignal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
}

void printMenu(Stack producers, Stack consumers)
{
	printf("\n&&= Producer/Consumer Menu =&&\n");
	printf("Status: producers=%d, consumers=%d, queue=%d/%d, free=%d\n",
		producers.size,
		consumers.size,
		queue->messageCount,
		MAX_MESSAGES,
		queue->freeCount);
	printf("1 / p. Add producer\n");
	printf("2 / o. Remove producer\n");
	printf("3 / c. Add consumer\n");
	printf("4 / x. Remove consumer\n");
	printf("5 / s. Show runtime stats\n");
	printf("0 / q. Exit\n");
	printf("Choice: ");
}

void printStatus(Stack producers, Stack consumers)	// вывести статистику
{
	printf("\nRuntime stats:\n");
	printf("Producers: %d\n", producers.size);
	printf("Consumers: %d\n", consumers.size);
	printf("Queue capacity: %d\n", MAX_MESSAGES);
	printf("Queue used: %d\n", queue->messageCount);
	printf("Queue free: %d\n", queue->freeCount);
	printf("Created messages: %d\n", queue->addedCount);
	printf("Received messages: %d\n\n", queue->extractedCount);
}
