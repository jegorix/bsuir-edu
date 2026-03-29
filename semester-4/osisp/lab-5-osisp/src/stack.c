#include "stack.h"

#include <stdio.h>
#include <stdlib.h>

Stack initStack(void)
{
    Stack stack;

    stack.head = NULL;
    stack.size = 0;

    return stack;
}

ThreadControl* pushStack(Stack* stack)
{
    StackElem* elem = (StackElem*)malloc(sizeof(StackElem));
    ThreadControl* control = (ThreadControl*)malloc(sizeof(ThreadControl));

    if (elem == NULL || control == NULL)
    {
        free(elem);
        free(control);
        perror("Can't allocate memory");
        exit(EXIT_FAILURE);
    }

    control->thread = (pthread_t)0;
    atomic_init(&control->stopRequested, 0);
    atomic_init(&control->finished, 0);
    control->seed = 0;

    elem->control = control;
    elem->next = stack->head;
    stack->head = elem;
    ++stack->size;

    return control;
}

ThreadControl* popStack(Stack* stack)
{
    StackElem* elem;
    ThreadControl* control;

    if (stack->size == 0)
    {
        return NULL;
    }

    elem = stack->head;
    control = elem->control;
    stack->head = elem->next;
    free(elem);
    --stack->size;

    return control;
}

void destroyThreadControl(ThreadControl* control)
{
    free(control);
}
