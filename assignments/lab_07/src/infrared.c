#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"


#define BUFFER_SIZE 64

// Macro per il timer
#define TIMER_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_COUNTER_0        0
#define TIMER_CLOCK_FREQ_HZ    100000000 // Frequenza del timer (100 MHz)
#define US_TO_TICKS(us)        ((us) * (TIMER_CLOCK_FREQ_HZ / 1000000)) // Conversione da microsecondi a tick

// Dichiarazione del GPIO come puntatore volatile
volatile int *AXI_GPIO_IR = (int *)XPAR_GPIO_IR_BASEADDR;

// Variabili Globali
volatile u32 data[BUFFER_SIZE] = {0};
volatile int currentIndex = 0;  // Current position buffer
volatile int captureFlag = 0;



// Timer
XTmrCtr TimerInstance;

// Prototipi funzioni
void init_timer();
void timerEnable();
void captureRawIR();
void PrintSequence(unsigned long long data, int durations[]);
void decodeAndPrintNECData(unsigned long long data);


int main() {
    init_platform();

    xil_printf("Ciao PIAGGIO\n");

    init_timer();

    while(1) {
        captureRawIR();
    }

    cleanup_platform();
    return 0;
}



void init_timer() {
    // Inizializza il timer
    XTmr_Initialize(&TimerInstance, TIMER_DEVICE_ID);

    // Configura il timer senza auto-reload
    XTmrCtr_SetOptions(&TimerInstance, TIMER_COUNTER_0, 0);
    XTmrCtr_Reset(&TimerInstance, TIMER_COUNTER_0);
    XTmrCtr_Start(&TimerInstance, TIMER_COUNTER_0);
}

void timerEnable(void) {
    XTmrCtr_Enable(&TimerInstance, TIMER_COUNTER_0);
}

// TODO: Sostituire address Timer e macro counter 
void timerReset(XTmrCtr* TimerInstance, int TimerCounter) {
    XTmrCtr_Stop(&TimerInstance, TimerCounter);
    XTmrCtr_Reset(&TimerInstance, TimerCounter);
    XTmrCtr_Start(&TimerInstance, TimerCounter);
}


void captureRawIR() {
    int start = 0;

    while(!start) {
        u32 high, low;
        u32 value;

        while(!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);

        while(*AXI_GPIO_IR);

        // timerReset();
        high = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);

        value = high - low;

        if ( ( value > 400000 ) && ( value < 600000 ) )
            start = 1;
    }


    for ( int i = 0; i < 32; i++ ) {

        while(!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);

        while(*AXI_GPIO_IR);

        high = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);

        data[i] = high - low;

        // timerReset();
    }
}


// Funzione per stampare la sequenza
void PrintSequence(unsigned long long data, int durations[]) {
    xil_printf("Bit | Numero | Durata (us)\n");
    for (int i = 31; i >= 0; i--) {
        int bit = (data >> i) & 1;
        xil_printf("%2d  | Bit %2d | %10d\n", bit, 32 - i, durations[31 - i] / (TIMER_CLOCK_FREQ_HZ / 1000000));
    }
}

// Decodifica e stampa dati NEC
void decodeAndPrintNECData(unsigned long long data) {
    unsigned char address = (data >> 24) & 0xFF;
    unsigned char address_inv = (data >> 16) & 0xFF;
    unsigned char command = (data >> 8) & 0xFF;
    unsigned char command_inv = data & 0xFF;

    xil_printf("Dati Decodificati:\n");
    xil_printf("Indirizzo: 0x%02X\n", address);
    xil_printf("Indirizzo Inverso: 0x%02X\n", !address_inv);
    xil_printf("Comando: 0x%02X\n", command);
    xil_printf("Comando Inverso: 0x%02X\n", !command_inv);
}
