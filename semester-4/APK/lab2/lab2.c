#include <dos.h>
#include <stdio.h>
#include <stdlib.h>

struct VIDEO {
    unsigned char symbol;
    unsigned char attribute;
};


// old_handlers
void interrupt (*old_int08)(void);
void interrupt (*old_int09)(void);
void interrupt (*old_int0A)(void);
void interrupt (*old_int0B)(void);
void interrupt (*old_int0C)(void);
void interrupt (*old_int0D)(void);
void interrupt (*old_int0E)(void);
void interrupt (*old_int0F)(void);

void interrupt (*old_int70)(void);
void interrupt (*old_int71)(void);
void interrupt (*old_int72)(void);
void interrupt (*old_int73)(void);
void interrupt (*old_int74)(void);
void interrupt (*old_int75)(void);
void interrupt (*old_int76)(void);
void interrupt (*old_int77)(void);


// new handlers for vectors 08h-0Fh (Master)
void interrupt new_int08(void); // IRQ0 (08h)
void interrupt new_int09(void); // IRQ1 (09h)
void interrupt new_int0A(void); // IRQ2 (0Ah)
void interrupt new_int0B(void); // IRQ3 (0Bh)
void interrupt new_int0C(void); // IRQ4 (0Ch)
void interrupt new_int0D(void); // IRQ5 (0Dh)
void interrupt new_int0E(void); // IRQ6 (0Eh)
void interrupt new_int0F(void); // IRQ7 (0Fh)

// new handlers for vectors 78h - 7Fh (Slave)
void interrupt new_int78(void); // IRQ8 (78h)
void interrupt new_int79(void); // IRQ9 (79h)
void interrupt new_int7A(void); // IRQ10 (7Ah)
void interrupt new_int7B(void); // IRQ11 (7Bh)
void interrupt new_int7C(void); // IRQ12 (7Ch)
void interrupt new_int7D(void); // IRQ13 (7Dh)
void interrupt new_int7E(void); // IRQ14 (7Eh)
void interrupt new_int7F(void); // IRQ15 (7Fh)


// output status of the controller registers
void print()
{
    unsigned char color = 0x0F;

    // command 0x0A — select query register (IRR)
    unsigned char master_mask = inp(0x21);
    unsigned char slave_mask = inp(0xA1);

    outp(0x20, 0x0A);
    unsigned char master_request = inp(0x20);
    outp(0xA0, 0x0A);
    unsigned char slave_request = inp(0xA0);

    // command 0x0B — select the service register (ISR)
    outp(0x20, 0x0B);
    unsigned char master_service = inp(0x20);
    outp(0xA0, 0x0B);
    unsigned char slave_service = inp(0xA0);

    int x_start = 20;
    int y_mask = 0;
    int y_request = 1;
    int y_service = 2;

    int i;
    unsigned char val;
    VIDEO far* p;

    // MASK output (segment, offset)
    p = (VIDEO far*) MK_FP(0xB800, (y_mask * 80 + x_start) * 2);
    // indicates cell on screen

    val = master_mask;
    for (i = 0; i < 8; i++) 
    {
        p->symbol = (val & 1) ? '1' : '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }

    p->symbol = ' ';
    p->attribute = color;
    p++;

    val = slave_mask;
    for (i = 0; i < 8; i++)
    {
        p->symbol = (val & 1) ? '1': '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }
    
    // Request output
    p = (VIDEO far*) MK_FP(0xB800, (y_request * 80 + x_start) * 2);
    val = master_request;
    for (i = 0; i < 8; i++)
    {
        p->symbol = (val & 1) ? '1' : '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }

    p->symbol = ' ';
    p->attribute = color;
    p++;
    val = slave_request;
    for (i = 0; i < 8; i++)
    {
        p->symbol = (val & 1) ? '1' : '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }

    // SERVICE output
    p = (VIDEO far*) MK_FP(0xB800, (y_service * 80 + x_start) * 2);
    val = master_service;
    for (i = 0; i < 8; i++)
    {
        p->symbol = (val & 1) ? '1' : '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }

    p->symbol = ' ';
    p->attribute = color;
    p++;
    val = slave_service;
    for (i = 0; i < 8; i++)
    {
        p->symbol = (val & 1) ? '1' : '0';
        p->attribute = color;
        p++;
        val >>= 1;
    }

}


// new handlers for vectors 08h-0Fh (Master)
void interrupt new_int08(void) { print(); old_int08(); } // IRQ0 (08h)
void interrupt new_int09(void) { print(); old_int09(); } // IRQ1 (09h)
void interrupt new_int0A(void) { print(); old_int0A(); } // IRQ2 (0Ah)
void interrupt new_int0B(void) { print(); old_int0B(); } // IRQ3 (0Bh)
void interrupt new_int0C(void) { print(); old_int0C(); } // IRQ4 (0Ch)
void interrupt new_int0D(void) { print(); old_int0D(); } // IRQ5 (0Dh)
void interrupt new_int0E(void) { print(); old_int0E(); } // IRQ6 (0Eh)
void interrupt new_int0F(void) { print(); old_int0F(); } // IRQ7 (0Fh)

// new handlers for vectors 78h - 7Fh (Slave)
void interrupt new_int78(void) { print(); old_int70(); } // IRQ8 (78h)
void interrupt new_int79(void) { print(); old_int71(); } // IRQ9 (79h)
void interrupt new_int7A(void) { print(); old_int72(); } // IRQ10 (7Ah)
void interrupt new_int7B(void) { print(); old_int73(); } // IRQ11 (7Bh)
void interrupt new_int7C(void) { print(); old_int74(); } // IRQ12 (7Ch)
void interrupt new_int7D(void) { print(); old_int75(); } // IRQ13 (7Dh)
void interrupt new_int7E(void) { print(); old_int76(); } // IRQ14 (7Eh)
void interrupt new_int7F(void) { print(); old_int77(); } // IRQ15 (7Fh)


void initialize()
{
    old_int08 = getvect(0x08);
    old_int09 = getvect(0x09);
    old_int0A = getvect(0x0A);
    old_int0B = getvect(0x0B);
    old_int0C = getvect(0x0C);
    old_int0D = getvect(0x0D);
    old_int0E = getvect(0x0E);
    old_int0F = getvect(0x0F);

    old_int70 = getvect(0x70);
    old_int71 = getvect(0x71);
    old_int72 = getvect(0x72);
    old_int73 = getvect(0x73);
    old_int74 = getvect(0x74);
    old_int75 = getvect(0x75);
    old_int76 = getvect(0x76);
    old_int77 = getvect(0x77);

    setvect(0x08, new_int08);
    setvect(0x09, new_int09);
    setvect(0x0A, new_int0A);
    setvect(0x0B, new_int0B);
    setvect(0x0C, new_int0C);
    setvect(0x0D, new_int0D);
    setvect(0x0E, new_int0E);
    setvect(0x0F, new_int0F);

    setvect(0x78, new_int78);
    setvect(0x79, new_int79);
    setvect(0x7A, new_int7A);
    setvect(0x7B, new_int7B);
    setvect(0x7C, new_int7C);
    setvect(0x7D, new_int7D);
    setvect(0x7E, new_int7E);
    setvect(0x7F, new_int7F);

    _disable(); // cli

    // Initialization Command Words
    // reprogramming the master controller
    outp(0x20, 0x11);
    outp(0x21, 0x08);
    outp(0x21, 0x04);
    outp(0x21, 0x01);

    // reprogramming the slave controller
    outp(0xA0, 0x11);
    outp(0xA1, 0x78);
    outp(0xA1, 0x02);
    outp(0xA1, 0x01);

    _enable(); // sti

}


int main() 
{
    initialize();
    system("cls");

    printf("MASTER  SLAVE\n");
    printf("MASK:                    \n");
    printf("REQUEST:                 \n");
    printf("SERVICE:                 \n");

    // free environment 
    unsigned far *p;
    FP_SEG(p) = _psp; // Program Segment Prefix
    FP_OFF(p) = 0x2C;
    _dos_freemem(*p); // free memory that used by environment

    // Resident completion
    _dos_keep(0, (_DS - _CS) + (_SP / 16) + 1);
    return 0;
}