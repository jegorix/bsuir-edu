#include <stdio.h>
#include <dos.h>
#include <conio.h>

// base frequency of system timer 
#define PIT_BASE_FREQUENCY 1193180UL

// ports of system timer and system speaker
#define PIT_CHANNEL0_PORT 0x40
#define PIT_CHANNEL1_PORT 0x41
#define PIT_CHANNEL2_PORT 0x42
#define PIT_CONTROL_PORT  0x43
// which channel set up / which mode choose // how read or write data
#define SPEAKER_PORT      0x61

typedef struct 
{
    unsigned int frequency;
    unsigned int duration;
    unsigned int pause;
} Note;


// prepare 2 channel
void setSpeakerFrequency(unsigned int frequency)
{
    unsigned int divider;
    unsigned char lowByte;
    unsigned char highByte;

    if (frequency == 0)
    {
        return;
    }

    divider = (unsigned int)(PIT_BASE_FREQUENCY / frequency);

    lowByte = (unsigned char)(divider & 0xFF); 
    highByte = (unsigned char)((divider >> 8) & 0xFF);

    outp(PIT_CONTROL_PORT, 0xB6); // 10(channel) 11(low-high) 011(mode - square wave) 0 (bin)b

    outp(PIT_CHANNEL2_PORT, lowByte);
    outp(PIT_CHANNEL2_PORT, highByte);

}

void speakerOn(void)
{
    unsigned char value;

    value = inp(SPEAKER_PORT);

    value = value | 0x03; // allow signal of 2 channel / turn on speaker

    outp(SPEAKER_PORT, value);
}


void speakerOff(void)
{
    unsigned char value;

    value = inp(SPEAKER_PORT);

    value = value & 0xFC;

    outp(SPEAKER_PORT, value);
}


void playNote(unsigned int frequency, unsigned int duration, unsigned int pause)
{
    if (frequency == 0)
    {
        delay(duration);
        delay(pause);
        return;
    }

    setSpeakerFrequency(frequency);
    speakerOn();
    delay(duration);
    speakerOff();
    delay(pause);

}

void playTestMelody(void)
{
    Note melody[] = 
        {
        {440, 300, 100},
        {494, 300, 100},
        {523, 300, 100},
        {587, 300, 100},
        {659, 500, 150}
    };

    int i;
    int count = sizeof(melody) / sizeof(melody[0]);

    printf("Playing test melody...\n");

    for (i = 0; i < count; i++)
    {
        playNote(melody[i].frequency, melody[i].duration, melody[i].pause);
    }

}


void playMasterOfPuppets(void)
{
    Note melody[] = 
    {

        {329, 180, 40},  
        {329, 180, 40},  
        {329, 180, 40},  
        {311, 180, 40},   
        {329, 180, 40},  
        {392, 250, 60},  

        {293, 180, 40},  
        {293, 180, 40},  
        {293, 180, 40},  
        {277, 180, 40},   
        {293, 180, 40},  
        {349, 250, 60},  

        {329, 180, 40},  
        {329, 180, 40},  
        {392, 250, 60},  
        {440, 300, 80},  

        {0,   200, 0},    /* pause */

        {392, 180, 40},  
        {349, 180, 40},  
        {329, 250, 60},  
        {293, 300, 80}   

    };

    int i;
    int count = sizeof(melody) / sizeof(melody[0]);

    printf("Playing: Metallica - Master of Puppets\n");

    for (i = 0; i < count; i++)
    {
        playNote(melody[i].frequency, melody[i].duration, melody[i].pause);
    }
}

void playMelody10(void)
{
    Note melody[] =
    {
        {349, 600, 50},
        {392, 300, 50},
        {440, 600, 50},
        {349, 300, 50},
        {440, 300, 50},
        {440, 300, 50},
        {392, 300, 50},
        {349, 300, 50},
        {392, 300, 50}
    };

    int i;
    int count = sizeof(melody) / sizeof(melody[0]);

    printf("Playing melody 10\n");

    for (i = 0; i < count; i++)
    {
        playNote(melody[i].frequency, melody[i].duration, melody[i].pause);
    }
}



void printBinaryByte(unsigned char value)
{
    int i;

    for(i = 7; i >= 0; i--)
    {
        if(value & (1 << i))
        {
            printf("1");
        }
        else
        {
            printf("0");
        }
    }
}



void printStatusWords(void)
{
    unsigned char status;
    int ports[3] = {PIT_CHANNEL0_PORT, PIT_CHANNEL1_PORT, PIT_CHANNEL2_PORT};

    unsigned char controlWords[3] = {0xE2, 0xE4, 0xE8}; // request status word for channels
    // send to control port
    int channel;

    // b7 status out / b6 sign null count / b5-4 read/wr format / b3 - mode / b0 - bin

    printf("\nStatus words of timer channels:\n");

    for (channel = 0; channel < 3; channel++)
    {
        outp(PIT_CONTROL_PORT, controlWords[channel]);

        status = inp(ports[channel]);

        printf("Channel %d: ", channel);
        printBinaryByte(status);
        printf("\n");
    }

}

unsigned int readCounterValue(int channelPort, unsigned char latchCommand)
{
    unsigned char lowByte;
    unsigned char highByte;
    unsigned int value;

    outp(PIT_CONTROL_PORT, latchCommand); // capture current state

    lowByte = inp(channelPort);
    highByte = inp(channelPort);

    // assemble 16bit value
    value = ((unsigned int) highByte << 8) | lowByte;

    return value;

}


void printCounterValues(void)
{
    int ports[3] = {PIT_CHANNEL0_PORT, PIT_CHANNEL1_PORT, PIT_CHANNEL2_PORT};

    // latch commands for channels
    unsigned char latchCommands[3] = {0x00, 0x40, 0x80};
    
    unsigned int counterValue;
    int channel;

    printf("\nCounter values (CE) in hexadecimal:\n");

    for (channel = 0; channel < 3; channel++)
    {
        counterValue = readCounterValue(ports[channel], latchCommands[channel]);
        printf("Channel %d: %04Xh\n", channel, counterValue);

    }

}



int showMenu(void)
{
    int option;

    printf("\n==============================\n");
    printf("Programmable Interval Timer Lab\n");
    printf("==============================\n");
    printf("1. Play test melody\n");
    printf("2. Play Metallica - Master of Puppets\n");
    printf("3. Play melody 10\n");
    printf("4. Show status words\n");
    printf("5. Show counter values (CE)\n");
    printf("6. Exit\n");
    printf("Choose option: ");

    scanf("%d", &option);

    return option;
}


int main(void)
{
    int option;

    clrscr();

    while (1)
    {
        option = showMenu();

        switch (option)
        {
            case 1:
                clrscr();
                playTestMelody();
                break;

            case 2:
                clrscr();
                playMasterOfPuppets();
                break;

            case 3:
                clrscr();
                playMelody10();
                break;

            case 4:
                clrscr();
                printStatusWords();
                break;

            case 5:
                clrscr();
                printCounterValues();
                break;

            case 6:
                clrscr();
                speakerOff();
                return 0;

            default:
                clrscr();
                printf("Invalid option. Try again.\n");
                break;
        }
    }
}
