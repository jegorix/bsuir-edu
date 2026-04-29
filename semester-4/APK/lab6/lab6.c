#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>

/* Fixed-width aliases required by lab spec. */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;

/* PIC ports. */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

/* Keyboard controller and RTC/CMOS ports. */
#define KBD_DATA_PORT   0x60
#define CMOS_INDEX_PORT 0x70
#define CMOS_DATA_PORT  0x71

/* Segment selectors in GDT. */
#define CODE_SELECTOR   0x08
#define DATA_SELECTOR   0x10
#define STACK_SELECTOR  0x18
#define VIDEO_SELECTOR  0x20

/* Protected-mode interrupt vectors after PIC remap. */
#define INT_IRQ1  0x21
#define INT_IRQ8  0x28

/* RTC defaults: rate=6 means 1024 interrupts/second. */
#define RTC_RATE_VALUE  6
#define RTC_TICKS_PER_SEC 1024UL

#pragma pack(1)

/* One GDT descriptor (8 bytes). */
typedef struct GDTEntry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  gran;
    u8  base_hi;
} GDTEntry;

/* LGDT argument format. */
typedef struct GDTPtr {
    u16 limit;
    u32 base;
} GDTPtr;

/* One IDT gate descriptor (8 bytes). */
typedef struct IDTEntry {
    u16 offset_low;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_high;
} IDTEntry;

/* LIDT argument format. */
typedef struct IDTPtr {
    u16 limit;
    u32 base;
} IDTPtr;

#pragma pack()

/* --- Function Prototypes ------------------------------------------------- */

/* Build the GDT (null + code + data + stack + helper video segment). */
void init_gdt(void);

/* Build and load IDT entries for IRQ1 (keyboard) and IRQ8 (RTC). */
void init_idt(void);

/* Configure RTC periodic interrupt source (IRQ8). */
void setup_rtc(void);

/* Unmask keyboard interrupt line (IRQ1). */
void setup_keyboard(void);

/* Switch CPU from real mode to protected mode. */
void enter_protected_mode(void);

/* Switch CPU back to real mode. */
void leave_protected_mode(void);

/* Continue execution after the protected-mode far jump. */
void pm_resume(void);

/* Continue execution after the real-mode far return. */
void rm_resume(void);

/* IRQ8 protected-mode handler. */
void interrupt far rtc_handler(void);

/* IRQ1 protected-mode handler. */
void interrupt far keyboard_handler(void);

/* Show menu and read required countdown seconds from user. */
void main_menu(void);

/* Program entry point. */
int main(void);

/* RTC handler C body called by asm stub. */
static void rtc_handler_body(void);

/* Keyboard handler C body called by asm stub. */
static void keyboard_handler_body(void);

/* --- Global State -------------------------------------------------------- */

static GDTEntry gdt[5];
static GDTPtr gdt_ptr;

static IDTEntry idt[256];
static IDTPtr idt_ptr;
static IDTPtr old_idt_ptr;

/* Runtime flags and counters must be volatile (lab requirement). */
volatile u8  pm_active = 0;
volatile u8  exit_requested = 0;
volatile u8  timeout_reached = 0;
volatile u8  last_scancode = 0;
volatile u8  key_counter = 0;

volatile u32 rtc_ticks = 0;
volatile u16 rtc_subticks = 0;
volatile u16 rtc_seconds = 0;
volatile u16 limit_seconds = 0;

/* Saved machine state for safe restore. */
static u8 old_pic1_mask = 0;
static u8 old_pic2_mask = 0;
static u8 old_rtc_reg_a = 0;
static u8 old_rtc_reg_b = 0;

static u16 rm_cs = 0;
static u16 rm_ds = 0;
static u16 rm_ss = 0;

/* Real mode IVT descriptor (base=0, limit=0x3FF). */
static IDTPtr realmode_idt_ptr = { 0x03FF, 0x00000000UL };

/* --- Low-Level Helpers --------------------------------------------------- */

/*
 * Write one byte to an I/O port.
 * Port map examples used in this program:
 * - 0x20/0x21: master PIC command/data
 * - 0xA0/0xA1: slave  PIC command/data
 * - 0x60     : keyboard scancode data
 * - 0x70/0x71: CMOS/RTC index and data
 */
static void outb(u16 port, u8 value)
{
    outportb(port, value);
}

/* Read one byte from an I/O port. */
static u8 inb(u16 port)
{
    return (u8)inportb(port);
}

/* Tiny I/O delay for old PIC/RTC programming sequences. */
static void io_wait(void)
{
    outportb(0x80, 0);
}

/* Convert far pointer into linear physical address (segment<<4 + offset). */
static u32 farptr_to_linear(void far *p)
{
    u32 seg = (u32)FP_SEG(p);
    u32 off = (u32)FP_OFF(p);
    return (seg << 4) + off;
}

/* Fill one GDT descriptor. */
static void set_gdt_entry(u16 idx, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[idx].limit_low = (u16)(limit & 0xFFFFUL);
    gdt[idx].base_low  = (u16)(base & 0xFFFFUL);
    gdt[idx].base_mid  = (u8)((base >> 16) & 0xFFUL);
    gdt[idx].access    = access;
    gdt[idx].gran      = (u8)(((limit >> 16) & 0x0FUL) | (gran & 0xF0U));
    gdt[idx].base_hi   = (u8)((base >> 24) & 0xFFUL);
}

/* Fill one IDT gate descriptor. */
static void set_idt_entry(u16 vec, void (interrupt far *handler)(void), u16 selector, u8 type_attr)
{
    idt[vec].offset_low  = (u16)FP_OFF(handler);
    idt[vec].selector    = selector;
    idt[vec].zero        = 0;
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_high = 0;
}

/*
 * Write one text cell directly to VGA memory in protected mode.
 * Uses VIDEO_SELECTOR descriptor with base 0xB8000.
 */
static void pm_put_cell(u8 row, u8 col, char ch, u8 attr)
{
    u16 off = ((u16)row * 80U + (u16)col) * 2U;
    u16 v = ((u16)attr << 8) | (u8)ch;

    asm {
        push es
        push di
        push ax
        mov ax, VIDEO_SELECTOR
        mov es, ax
        mov di, off
        mov ax, v
        mov es:[di], ax
        pop ax
        pop di
        pop es
    }
}

/* Print zero-terminated text at fixed row/column in VGA text buffer. */
static void pm_print_at(u8 row, u8 col, const char *s, u8 attr)
{
    while (*s && col < 80U) {
        pm_put_cell(row, col, *s, attr);
        ++s;
        ++col;
    }
}

/* Print decimal u16 at fixed position in VGA text buffer. */
static void pm_print_dec_at(u8 row, u8 col, u16 value, u8 attr)
{
    char buf[6];
    int i = 0;
    int j;

    if (value == 0U) {
        pm_put_cell(row, col, '0', attr);
        return;
    }

    while (value > 0U && i < 5) {
        buf[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    for (j = i - 1; j >= 0; --j) {
        pm_put_cell(row, col++, buf[j], attr);
    }
}

/* Print byte as two hex symbols. */
static void pm_print_hex8_at(u8 row, u8 col, u8 value, u8 attr)
{
    static const char hex[] = "0123456789ABCDEF";
    pm_put_cell(row, col,     hex[(value >> 4) & 0x0F], attr);
    pm_put_cell(row, col + 1, hex[value & 0x0F], attr);
}

/*
 * Small PC speaker beep.
 * Port map:
 * - 0x43: PIT control
 * - 0x42: PIT channel 2 data
 * - 0x61: speaker gate/data enable bits
 */
static void speaker_beep(u16 hz, u16 ms)
{
    u16 div;
    u8 prev;
    volatile u16 outer;
    volatile u16 inner;

    if (hz == 0U) {
        return;
    }

    div = (u16)(1193180UL / (u32)hz);

    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFFU));
    outb(0x42, (u8)((div >> 8) & 0xFFU));

    prev = inb(0x61);
    outb(0x61, (u8)(prev | 0x03U));

    /* Protected mode must not call DOS/BIOS delay helpers here. */
    for (outer = 0; outer < ms; ++outer) {
        for (inner = 0; inner < 900; ++inner) {
            /* Busy wait for an approximate millisecond delay. */
        }
    }

    outb(0x61, (u8)(prev & (u8)~0x03U));
}

/* Map selected keyboard scancodes to ASCII for screen output. */
static char scancode_to_ascii(u8 sc)
{
    if (sc >= 0x02 && sc <= 0x0B) {
        static const char dig[] = "1234567890";
        return dig[sc - 0x02];
    }
    if (sc >= 0x10 && sc <= 0x19) {
        static const char row1[] = "QWERTYUIOP";
        return row1[sc - 0x10];
    }
    if (sc >= 0x1E && sc <= 0x26) {
        static const char row2[] = "ASDFGHJKL";
        return row2[sc - 0x1E];
    }
    if (sc >= 0x2C && sc <= 0x32) {
        static const char row3[] = "ZXCVBNM";
        return row3[sc - 0x2C];
    }
    if (sc == 0x39) {
        return ' ';
    }
    return '?';
}

/* Show runtime status in protected mode screen. */
static void pm_draw_status(void)
{
    pm_print_at(0, 0,  "Protected mode: RTC IRQ8 demo", 0x1F);
    pm_print_at(1, 0,  "Seconds elapsed:      ", 0x0F);
    pm_print_dec_at(1, 17, rtc_seconds, 0x0E);

    pm_print_at(2, 0,  "Limit seconds:        ", 0x0F);
    pm_print_dec_at(2, 15, limit_seconds, 0x0E);

    pm_print_at(3, 0,  "Last scancode: 0x", 0x0F);
    pm_print_hex8_at(3, 16, last_scancode, 0x0A);

    pm_print_at(4, 0,  "Last key:   ", 0x0F);
    pm_put_cell(4, 10, (char)scancode_to_ascii(last_scancode), 0x0A);

    pm_print_at(6, 0,  "ESC -> exit immediately", 0x07);
}

/* --- Initialization ------------------------------------------------------ */

void init_gdt(void)
{
    u32 cs_base = ((u32)rm_cs) << 4;
    u32 ds_base = ((u32)rm_ds) << 4;
    u32 ss_base = ((u32)rm_ss) << 4;

    /* Null descriptor. */
    set_gdt_entry(0, 0UL, 0UL, 0x00, 0x00);

    /* 16-bit execute/read code segment for current program image. */
    set_gdt_entry(1, cs_base, 0xFFFFUL, 0x9A, 0x00);

    /* 16-bit read/write data segment for program data/stack. */
    set_gdt_entry(2, ds_base, 0xFFFFUL, 0x92, 0x00);

    /* Dedicated protected-mode stack segment. */
    set_gdt_entry(3, ss_base, 0xFFFFUL, 0x92, 0x00);

    /* Helper segment mapped directly to VGA text memory (0xB8000). */
    set_gdt_entry(4, 0x000B8000UL, 0x0FFFUL, 0x92, 0x00);

    gdt_ptr.limit = (u16)(sizeof(gdt) - 1U);
    gdt_ptr.base  = farptr_to_linear(&gdt[0]);
}

void init_idt(void)
{
    u16 i;

    for (i = 0; i < 256; ++i) {
        idt[i].offset_low = 0;
        idt[i].selector = CODE_SELECTOR;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    /* 0x86 = present, DPL0, 16-bit interrupt gate. */
    set_idt_entry(INT_IRQ1, keyboard_handler, CODE_SELECTOR, 0x86);
    set_idt_entry(INT_IRQ8, rtc_handler,      CODE_SELECTOR, 0x86);

    idt_ptr.limit = (u16)(sizeof(idt) - 1U);
    idt_ptr.base  = farptr_to_linear(&idt[0]);
}

/*
 * Reprogram PIC vector offsets.
 * Master offset usually 0x20, slave offset 0x28 in protected mode.
 */
static void pic_remap(u8 master_off, u8 slave_off)
{
    u8 m = inb(PIC1_DATA);
    u8 s = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    outb(PIC1_DATA, master_off);
    io_wait();
    outb(PIC2_DATA, slave_off);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, m);
    outb(PIC2_DATA, s);
}

void setup_keyboard(void)
{
    /* Keep only IRQ1 enabled on the master PIC for now. */
    outb(PIC1_DATA, 0xFD);
    outb(PIC2_DATA, 0xFF);
}

/*
 * RTC periodic interrupt setup.
 * Registers used via ports 0x70/0x71:
 * - Register A (index 0x0A): lower 4 bits = rate selector
 * - Register B (index 0x0B): bit 6 PIE enables periodic IRQ
 * - Register C (index 0x0C): must be read in IRQ handler to ack RTC
 */
void setup_rtc(void)
{
    u8 prev;
    u8 mask;

    /* Program rate in RTC register A. */
    outb(CMOS_INDEX_PORT, 0x8A);
    prev = inb(CMOS_DATA_PORT);
    outb(CMOS_INDEX_PORT, 0x8A);
    outb(CMOS_DATA_PORT, (u8)((prev & 0xF0U) | (RTC_RATE_VALUE & 0x0FU)));

    /* Enable periodic interrupt in RTC register B (PIE bit6). */
    outb(CMOS_INDEX_PORT, 0x8B);
    prev = inb(CMOS_DATA_PORT);
    outb(CMOS_INDEX_PORT, 0x8B);
    outb(CMOS_DATA_PORT, (u8)(prev | 0x40U));

    /* Read register C once to clear any pending IRQ state. */
    outb(CMOS_INDEX_PORT, 0x0C);
    (void)inb(CMOS_DATA_PORT);

    /* Unmask only IRQ8 on the slave PIC and IRQ1/IRQ2 on the master PIC. */
    outb(PIC2_DATA, 0xFE);
    outb(PIC1_DATA, 0xF9);
}

/* --- Mode Transitions ---------------------------------------------------- */

#pragma option -k-
#pragma option -N-

void enter_protected_mode(void)
{
    asm {
        cli

        /* Load protected-mode descriptor tables. */
        db 0x0F,0x01,0x16
        dw offset gdt_ptr
        db 0x0F,0x01,0x1E
        dw offset idt_ptr

        /* Set CR0.PE = 1 using i386+ algorithm. */
        db 0x0F,0x20,0xC0
        db 0x66,0x83,0xC8,0x01
        db 0x0F,0x22,0xC0

        /* Far jump flushes the prefetch queue and loads PM CS. */
        db 0xEA
        dw offset pm_resume
        dw CODE_SELECTOR
    }
}

void leave_protected_mode(void)
{
    asm {
        cli

        /* Restore real-mode IVT layout before clearing PE. */
        db 0x0F,0x01,0x1E
        dw offset realmode_idt_ptr

        db 0x0F,0x20,0xC0
        db 0x66,0x83,0xE0,0xFE
        db 0x0F,0x22,0xC0

        /* Return to the real-mode continuation point. */
        mov ax, rm_cs
        push ax
        mov ax, offset rm_resume
        push ax
        retf
    }
}

void pm_resume(void)
{
    asm {
        mov ax, DATA_SELECTOR
        mov ds, ax
        mov es, ax
        mov ax, STACK_SELECTOR
        mov ss, ax
        mov byte ptr pm_active, 1
        ret
    }
}

void rm_resume(void)
{
    asm {
        mov ax, rm_ds
        mov ds, ax
        mov es, ax
        mov ax, rm_ss
        mov ss, ax
        mov byte ptr pm_active, 0
        ret
    }
}

/* --- Interrupt Handlers -------------------------------------------------- */

void interrupt far rtc_handler(void)
{
    rtc_handler_body();
}

void interrupt far keyboard_handler(void)
{
    keyboard_handler_body();
}

#pragma option -k

static void rtc_handler_body(void)
{
    ++rtc_ticks;
    ++rtc_subticks;

    if (rtc_subticks >= RTC_TICKS_PER_SEC) {
        rtc_subticks = 0;
        ++rtc_seconds;
        pm_draw_status();

        if (rtc_seconds >= limit_seconds && limit_seconds != 0U) {
            timeout_reached = 1;
            exit_requested = 1;
        }
    }

    /* Mandatory RTC acknowledge sequence: read register C. */
    outb(CMOS_INDEX_PORT, 0x0C);
    (void)inb(CMOS_DATA_PORT);

    /* IRQ8 is on slave PIC: EOI to slave then master. */
    outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

static void keyboard_handler_body(void)
{
    u8 sc = inb(KBD_DATA_PORT);
    last_scancode = sc;

    /* Only react to make code (bit7=0). */
    if ((sc & 0x80U) == 0U) {
        ++key_counter;
        pm_draw_status();

        /* ESC make code is 0x01. */
        if (sc == 0x01U) {
            exit_requested = 1;
        }

        /* Short key click beep. */
        speaker_beep(1200, 20);
    }

    outb(PIC1_CMD, PIC_EOI);
}

/* --- UI and Main --------------------------------------------------------- */

void main_menu(void)
{
    clrscr();
    cprintf("================ Protected Mode Lab (RTC / IRQ8) ================\r\n");
    cprintf("Variant: IRQ8 (Real-Time Clock periodic interrupt)\r\n\r\n");
    cprintf("Program flow:\r\n");
    cprintf("  1) Enter protected mode\r\n");
    cprintf("  2) Count seconds using RTC interrupts\r\n");
    cprintf("  3) Show keyboard scancodes\r\n");
    cprintf("  4) ESC exits early\r\n\r\n");

    cprintf("Enter countdown in seconds (1..9999): ");
    scanf("%hu", (u16*)&limit_seconds);

    if (limit_seconds == 0U) {
        limit_seconds = 1U;
    }

    cprintf("\r\nPress any key to switch to protected mode...");
    getch();
}

int main(void)
{
    /* Save real-mode segment context for safe return. */
    asm {
        mov rm_cs, cs
        mov rm_ds, ds
        mov rm_ss, ss
    }

    /* Save old PIC and RTC state. */
    old_pic1_mask = inb(PIC1_DATA);
    old_pic2_mask = inb(PIC2_DATA);

    outb(CMOS_INDEX_PORT, 0x8A);
    old_rtc_reg_a = inb(CMOS_DATA_PORT);
    outb(CMOS_INDEX_PORT, 0x8B);
    old_rtc_reg_b = inb(CMOS_DATA_PORT);

    /* Save old IDTR so DOS stays stable after exit. */
    asm {
        db 0x0F,0x01,0x0E
        dw offset old_idt_ptr
    }

    main_menu();

    init_gdt();
    init_idt();

    /* Remap PIC for protected mode IRQ vectors 0x20..0x2F. */
    pic_remap(0x20, 0x28);

    enter_protected_mode();

    setup_keyboard();
    setup_rtc();
    pm_draw_status();
    asm sti

    /* Idle loop: handlers do all visible work. */
    while (!exit_requested) {
        asm {
            hlt
        }
    }

    if (timeout_reached) {
        speaker_beep(700, 160);
        speaker_beep(900, 160);
    }

    /* Disable RTC periodic IRQ before leaving protected mode. */
    outb(CMOS_INDEX_PORT, 0x8B);
    outb(CMOS_DATA_PORT, (u8)(old_rtc_reg_b & (u8)~0x40U));

    leave_protected_mode();

    /* Restore old IDT. */
    asm {
        db 0x0F,0x01,0x1E
        dw offset old_idt_ptr
    }

    /* Restore RTC registers A/B. */
    outb(CMOS_INDEX_PORT, 0x8A);
    outb(CMOS_DATA_PORT, old_rtc_reg_a);
    outb(CMOS_INDEX_PORT, 0x8B);
    outb(CMOS_DATA_PORT, old_rtc_reg_b);

    /* Restore PIC to BIOS/DOS real-mode offsets and original masks. */
    pic_remap(0x08, 0x70);
    outb(PIC1_DATA, old_pic1_mask);
    outb(PIC2_DATA, old_pic2_mask);
    asm sti

    clrscr();
    cprintf("Program finished safely.\r\n");
    cprintf("Elapsed seconds: %u\r\n", rtc_seconds);
    cprintf("Last scancode : 0x%02X\r\n", last_scancode);

    return 0;
}
