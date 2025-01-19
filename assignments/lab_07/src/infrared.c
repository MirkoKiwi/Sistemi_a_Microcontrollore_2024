#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"


#define BUFFER_SIZE 32

// Macro per il timer
#define TIMER_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID
#define TmrCtrNumber           0
#define TIMER_CTRL_RESET	   0x56
#define TIMER_CLOCK_FREQ_HZ    100000000 // Frequenza del timer (100 MHz)
#define US_TO_TICKS(us)        ((us) * (TIMER_CLOCK_FREQ_HZ / 1000000)) // Conversione da microsecondi a tick

// Dichiarazione del GPIO come puntatore volatile
volatile int *AXI_GPIO_IR = (int *)XPAR_GPIO_IR_BASEADDR;

// Variabili Globali
volatile u32 data[BUFFER_SIZE] = {0};
volatile int currentIndex = 0;  // Current position buffer
volatile int captureFlag = 0;


// Prototipi funzioni
void init_timer(int counterValue);
void timerEnable();
void timerReset();
void captureRawIR();
void PrintSequence(u32 data, u32 durations[]);
void decodeAndPrintNECData(u32 data);


int main() {
    init_platform();

    xil_printf("Ciao PIAGGIO\n");

    init_timer(TmrCtrNumber);

    while(1) {
        captureRawIR();
    }

    cleanup_platform();
    return 0;
}



void init_timer(int counterValue) {
    // Configurazione Timer
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET); 	// Configura Status Register (SR)
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);  				// Load register impostato su counterValue
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);       				// Inizializza il timer

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);

    // Attiva il timer
    timerEnable();
}

void timerEnable(void) {
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

void timerReset() {
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, 0);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}


void captureRawIR() {
    int start = 0;
    u32 high, low;

    while(!start) {
        u32 value;

        while(!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while(*AXI_GPIO_IR);
        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        value = high - low;

        timerReset();

        if ( ( value > 400000 ) && ( value < 600000 ) )
            start = 1;
    }

    for ( int i = 0; i < 32; i++ ) {

        while(!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while(*AXI_GPIO_IR);

        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        data[i] = high - low;

        timerReset();

        PrintSequence(high - low, data);
        decodeAndPrintNECData(high - low);
    }
}


// Funzione per stampare la sequenza
void PrintSequence(u32 data, u32 durations[]) {
    xil_printf("Bit | Numero | Durata (us)\n");
    for (int i = 31; i >= 0; i--) {
        int bit = (data >> i) & 1;
        xil_printf("%2d  | Bit %2d | %10d\n", bit, 32 - i, durations[31 - i] / (TIMER_CLOCK_FREQ_HZ / 1000000));
    }
}

// Decodifica e stampa dati NEC
void decodeAndPrintNECData(u32 data) {
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
