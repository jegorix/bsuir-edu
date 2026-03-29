#include "program.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INT_DIGIT 10 // максимум символов в 32 битном числе int (-2 147 483 648)

static void clearInputBuffer(void)
{
	int ch;

	while((ch = getchar()) != '\n' && ch != EOF) {}
}

void executeProg(char option, char* fileName, int memSize)	// запуск gen или view в зависимости от выбора пользователя
{
	int indexSize;
	char indexSizeStr[MAX_INT_DIGIT + 1];
	char* argv[] = {"prog", fileName, indexSizeStr, NULL};	// формирование аргументов командной строки
	
	if(option == '1')	// 1: генерация
	{
		printf("\nВведите размер индексного файла в байтах.\n");
		printf("Требования: число больше %d, кратно 256 и %d.\n", memSize, memSize);
		printf("Размер файла: ");
		fflush(stdout);
		while(scanf("%d", &indexSize) != 1 || indexSize <= memSize || indexSize % memSize != 0)	// ввод корректного размера генерируемого файла (кратен memSize)
		{
			printf("\nНекорректный ввод.\n");
			printf("Размер файла должен быть целым положительным числом,");
			printf(" больше %d и кратным %d.\n", memSize, memSize);
			printf("Повторите ввод: ");
			fflush(stdout);
			clearInputBuffer();
		}
		clearInputBuffer();
		
		snprintf(indexSizeStr, sizeof(indexSizeStr), "%d", indexSize);
	}
	else	// 2: просмотр
	{
		argv[2] = NULL;
	}
	
	if(execve(option == '1' ? "./gen" : "./view", argv, NULL) == -1)	// запуск
	{
		perror("Не удалось запустить дочернюю программу");
		exit(EXIT_FAILURE);
	}
}

void waitForProg(pid_t prog)	// ожидание завершения запущенного процесса
{
	if((waitpid(prog, NULL, 0)) == -1)
	{
		perror("Не удалось дождаться завершения дочернего процесса");
		exit(6);
	}
}
