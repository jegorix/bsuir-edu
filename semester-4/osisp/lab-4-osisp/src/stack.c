#define _XOPEN_SOURCE 500

#include "stack.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

Stack initStack(void)	// инициализация стека
{
	Stack stack;
	
	stack.head = NULL;	// пустой стек не содержит элементов
	stack.size = 0;		// начальный размер стека
	
	return stack;
}

void push(Stack* stack, pid_t child)	// добавление элемента в стек
{
	struct StackElem* elem = (struct StackElem*)malloc(sizeof(struct StackElem));	// выделить память под новый узел
	if(elem == NULL)
	{
		perror("Can't allocate memory");
		exit(1);
	}
	
	elem->value = child;			// сохранить PID процесса
	elem->next = stack->head;		// новый элемент указывает на текущую вершину
	stack->head = elem;			// сделать новый элемент вершиной стека
	++stack->size;
}

pid_t pop(Stack* stack)	// удаление элемента из стека
{	
	if(stack->size == 0)
	{
		printf("\nStack is empty\n\n");
		return 0;
	}
	
	struct StackElem* temp = stack->head;
	pid_t value = temp->value;		// сохранить значение вершины
	stack->head = stack->head->next;	// сдвинуть вершину к следующему элементу
	temp->next = NULL;			// обнулить ссылку перед освобождением
	free(temp);
	
	--stack->size;
	
	return value;
}

void clear(Stack* stack)	// полное удаление стека
{
	while (stack->size != 0)
	{
		pid_t pid = pop(stack);		// извлечь PID из вершины стека
		if (pid > 0)
		{
			kill(pid, SIGTERM);		// отправить сигнал завершения процессу
			waitpid(pid, NULL, 0);		// дождаться завершения процесса
		}
	}
}
