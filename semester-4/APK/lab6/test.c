/*
    Дескриптор сегмента GDT.

    Он занимает ровно 8 байт.

    В нём процессор хранит:
    - базовый адрес сегмента;
    - лимит сегмента;
    - права доступа;
    - режим 16/32-bit;
    - признак code/data.
*/
struct GDTEntry
{
    u16 limit_low;     /* Младшие 16 бит лимита */
    u16 base_low;      /* Младшие 16 бит базы */
    u8  base_middle;   /* Средние 8 бит базы */
    u8  access;        /* Права доступа */
    u8  granularity;   /* Старшие биты лимита + флаги */
    u8  base_high;     /* Старшие 8 бит базы */
};

/*
    Указатель GDTR.

    Команда LGDT загружает именно такую структуру:
    - limit: размер таблицы минус 1
    - base: линейный адрес GDT
*/
struct GDTPtr
{
    u16 limit;
    u32 base;
};

/*
    Дескриптор IDT.

    Он тоже занимает 8 байт.

    В нём хранится:
    - адрес обработчика;
    - селектор сегмента кода;
    - тип gate;
    - права доступа.
*/
struct IDTEntry
{
    u16 offset_low;    /* Младшие 16 бит адреса обработчика */
    u16 selector;      /* Селектор сегмента кода */
    u8  zero;          /* Всегда 0 */
    u8  type_attr;     /* Тип и права */
    u16 offset_high;   /* Старшие 16 бит адреса обработчика */
};

/*
    Указатель IDTR.
*/
struct IDTPtr
{
    u16 limit;
    u32 base;
};


/*
    Наша GDT:
    0 — null descriptor
    1 — code segment
    2 — data segment
*/
struct GDTEntry gdt[3];
struct GDTPtr gdt_ptr;

/*
    IDT на 256 прерываний.
*/
struct IDTEntry idt[256];
struct IDTPtr idt_ptr;