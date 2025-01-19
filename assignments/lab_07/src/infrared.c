#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"


// Timer Macro
#define TIMER_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID
#define TmrCtrNumber           0
#define TIMER_CTRL_RESET	   0
#define TIMER_CLOCK_FREQ_HZ    100000000 // Frequenza del timer (100 MHz)


// Dichiarazione del GPIO come puntatore volatile
volatile int *AXI_GPIO_IR = (int *)XPAR_GPIO_IR_BASEADDR;



// ********************** Variabili Globali ********************** //
volatile u32 data[32] = {0};
typedef enum {
	POWER_ON = 0xA2, FUNC = 0xE2,
	VOL_UP = 0x62, VOL_DOWN = 0xA8, PREV = 0x22, PAUSE = 0x02, NEXT = 0xC2,
	ARROW_UP = 0x90, ARROW_DOWN = 0xE0, EQ = 0x98, ST_REPT = 0xB0,
	ZERO = 0x68, ONE = 0x30, TWO = 0x18, THREE = 0x7A, FOUR = 0x10, FIVE = 0x38, SIX = 0x5A, SEVEN = 0x42, EIGHT = 0x4A, NINE = 0x52
} RemoteButtons;


// ********************** Prototipi funzioni ********************** //
// Funzioni Timer
void init_timer(int counterValue);
void timerEnable();
void timerReset();

// Funzioni cattura e gestione input IR
void captureRawIR();
void printData(u32 data[]);
void decodeAndPrintNECData(u32 data[]);

u32 convertToDec(u32 data[], u32 size);




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
    // Reset the timer and load the initial value
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}

void timerEnable(void) {
    // Enable the timer
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

}

void timerReset() {
    // Disable and reset the timer
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, 0);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}


void captureRawIR() {

	u32 data[32] = {0};

    int start = 0;
    u32 high, low;

    while (!start) {
        u32 value;

        while (!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while (*AXI_GPIO_IR);
        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        value = high - low;

        timerReset();
        if ( (value > 400000) && (value < 600000) ) {
            start = 1;
        }
    }

    for ( int i = 0; i < 32; i++ ) {
        while (!(*AXI_GPIO_IR));

        timerEnable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while (*AXI_GPIO_IR);

        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        int bitValue = ( ( high - low ) > 100000 ) ? 1 : 0;		// Converte in un 1 o uno 0 in base al treshold

        data[i] = bitValue;

        timerReset();
    }
    decodeAndPrintNECData(data);

    // DEBUG
    // printData(data);
}

// Funzione per stampare la sequenza (DEBUG)
void printData(u32 data[]) {
    xil_printf("Bit | Numero\n");
    for (int i = 0; i < 32; i++) {
        xil_printf("%2d  | Bit %2d\n", i + 1, data[i]);
    }
    decodeAndPrintNECData(data);
}

// Decodifica e stampa dati NEC
void decodeAndPrintNECData(u32 data[]) {
	u32 decData = convertToDec(data, 32);

	// Debug
	// xil_printf("%02X\n", decData);

	unsigned char address = ( decData ) & 0xFF;
	unsigned char addressInverse = ( decData >> 8 ) & 0xFF;
	unsigned char command = ( decData >> 16 ) & 0xFF;
	unsigned char commandInverse = ( decData >> 24 ) & 0xFF;


    xil_printf("Dati Decodificati:\n");
    xil_printf("Indirizzo: 0x%02X\n", address);
    xil_printf("Indirizzo Inverso: 0x%02X\n", addressInverse);
    xil_printf("Comando: 0x%02X\n", command);
    xil_printf("Comando Inverso: 0x%02X\n", commandInverse);
}

// Prende in input un array di 32 bit e lo converte in un valore decimale u32
u32 convertToDec(u32 data[], u32 size) {
	u32 result = 0;

	for ( int i = size; i >= 0; i-- ) {
		result *= 2;
		result += data[i];
		xil_printf("Res: %d\nIteration: %d\nN Data: %d\n\n", result, i, data[i]);
	}

	return result;
}
