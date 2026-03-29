#pragma once

#include <sys/types.h>

struct StackElem	// структура элемента стека
{
	pid_t value;
	struct StackElem* next;
};

typedef struct // структура самого стека
{
	struct StackElem* head;
	int size;
} Stack;

Stack initStack(void);	// инициализировать стек
void push(Stack* stack, pid_t child);	// добавить элемент в стек
pid_t pop(Stack* stack);	// удалить элемент из стека
void clear(Stack* stack);	// полность удалить стек
