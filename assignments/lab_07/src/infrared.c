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
	POWER_ON = 0x45, FUNC = 0x47,
	VOL_UP = 0x46, VOL_DOWN = 0x15, PREV = 0x44, PAUSE = 0x40, NEXT = 0x43,
	ARROW_UP = 0x09, ARROW_DOWN = 0x07, EQ = 0x19, ST_REPT = 0x0D,
	ZERO = 0x16, ONE = 0x0C, TWO = 0x18, THREE = 0x5E, FOUR = 0x08, FIVE = 0x1C, SIX = 0x5A, SEVEN = 0x42, EIGHT = 0x52, NINE = 0x4A
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

    printButton(command);
}

// Prende in input un array di 32 bit e lo converte in un valore decimale u32
u32 convertToDec(u32 data[], u32 size) {
	u32 result = 0;

	for ( int i = size; i >= 0; i-- ) {
		result *= 2;
		result += data[i];
	}

	return result;
}

// Riconoscimento comandi
void printButton(RemoteButtons command) {
    switch(command) {
        case POWER_ON:  xil_printf("POWER_ON\n");   break;
        case FUNC:      xil_printf("FUNC\n");       break;
        case VOL_UP:    xil_printf("VOL_UP\n");     break;
        case VOL_DOWN:  xil_printf("VOL_DOWN\n");   break;
        case PREV:      xil_printf("PREV\n");       break;
        case PAUSE:     xil_printf("PAUSE\n");      break;
        case NEXT:      xil_printf("NEXT\n");       break;
        case ARROW_UP:  xil_printf("ARROW_UP\n");   break;
        case ARROW_DOWN:xil_printf("ARROW_DOWN\n"); break;
        case EQ:        xil_printf("EQ\n");         break;
        case ST_REPT:   xil_printf("ST_REPT\n");    break;
        case ZERO:      xil_printf("ZERO\n");       break;
        case ONE:       xil_printf("ONE\n");        break;
        case TWO:       xil_printf("TWO\n");        break;
        case THREE:     xil_printf("THREE\n");      break;
        case FOUR:      xil_printf("FOUR\n");       break;
        case FIVE:      xil_printf("FIVE\n");       break;
        case SIX:       xil_printf("SIX\n");        break;
        case SEVEN:     xil_printf("SEVEN\n");      break;
        case EIGHT:     xil_printf("EIGHT\n");      break;
        case NINE:      xil_printf("NINE\n");       break;
        
        default:        xil_printf("UNKNOWN\n");    break;
    }
}
