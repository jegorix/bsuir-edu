#include <sys/types.h>

void executeProg(char option, char* fileName, int memSize); // запуск gen или view в зависимости от выбора пользователя
void waitForProg(pid_t prog);   // ожидание завершения запущенного процесса
