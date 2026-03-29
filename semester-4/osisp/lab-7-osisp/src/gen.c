#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "record.h"

#define MIN_RECORDS_COUNT 10
#define MAX_RECORDS_COUNT 100

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Неверный запуск программы.\n");
        fprintf(stderr, "Использование: %s <number_of_records>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int records_count = atoi(argv[1]);
    if (records_count < MIN_RECORDS_COUNT || records_count > MAX_RECORDS_COUNT)
    {
        fprintf(stderr, "Количество записей должно быть в диапазоне [%d; %d].\n",
            MIN_RECORDS_COUNT, MAX_RECORDS_COUNT);
        return EXIT_FAILURE;
    }

    srand(time(NULL));

    struct Record *records = malloc(records_count * sizeof(struct Record));
    if (!records)
    {
        perror("Не удалось выделить память");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < records_count; i++)     // заполнение массива записей
    {
        memset(&records[i], 0, sizeof(struct Record));
        snprintf(records[i].name, sizeof(records[i].name), "Student %3d", i + 1);
        snprintf(records[i].address, sizeof(records[i].address), "Address %3d", i + 1);
        records[i].semester = (rand() % 8) + 1;
    }

    FILE *file = fopen(FILENAME, "wb");     // открытие/создание файла для записи
    if (!file)
    {
        perror("Не удалось открыть файл для записи");
        free(records);
        return EXIT_FAILURE;
    }


    int written = fwrite(records, sizeof(struct Record), records_count, file);       // Запись данных в файл
    if (written != records_count)
    {
        perror("Не удалось записать данные в файл");
        fclose(file);
        free(records);
        return EXIT_FAILURE;
    }

    fclose(file);
    free(records);
    printf("\nФайл \"%s\" успешно создан.\n", FILENAME);
    printf("Количество записей: %d\n", records_count);
    return EXIT_SUCCESS;
}
