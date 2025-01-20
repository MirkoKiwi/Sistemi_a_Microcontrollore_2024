#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"


// Timer Macro
#define TmrCtrNumber            0
#define TIMER_CTRL_RESET	    0
#define TIMER_CTRL_RESET_CCW    0x56


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

unsigned char address;
unsigned char addressInverse;
unsigned char command;
unsigned char commandInverse;


// ********************** Prototipi funzioni ********************** //
// Funzioni Timer
void init_timer0(int counterValue);
void timer0Enable();
void timer0Reset();

void init_timer1(int counterValue);
void timer1Enable();
void timer1Reset();

// Funzioni cattura e gestione input IR
unsigned char decode_NEC();
void printData(u32 data[]);
void decodeAndPrintNECData(u32 data[]);

u32 convertToDec(u32 data[], u32 size);



// ********************** Motor Driver ********************** //
volatile int *ledGPaddr = (volatile int *)0x40000000	// channel 0
volatile int *RGBaddr = (volatile int *)0x40000008	    // channel 1

void my_ISR(void) __attribute__ ((interrupt_handler));

int cnt = 0;
int pwmA = 0;	// Increase
int pwmB = 0;	// Decrease
int dirA = 0;	// 1 if CW
int dirB = 0;	// 1 if CCW

void manage_pwm(int cnt, int pwmA, int pwmB, volatile int *RGBaddr);
void set_dir(int dirA, int dirB, volatile int *ledGPaddr);
void clear_interrupt();



int main() {
    init_platform();

    init_timer0(TmrCtrNumber);


//    // ********************** Motor Driver ********************** //
//    enable_interrupts();
//    init_timer0();			enable_timer_channel_0();
//    init_timer1();			enable_timer_channel_1();
//
//	  while(1) {
//    		command = decode_NEC(timer);
//    		manage_command(pwmA, pwmB, dirA, dirB);    // Increase, decrease, switch dir based on the command
//    // ********************************************************** //


    while(1) {
        decode_NEC();
    }

    cleanup_platform();
    return 0;
}



void init_timer0(int counterValue) {
    // Reset the timer and load the initial value
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}

void timer0Enable(void) {
    // Enable the timer
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

}

void timer0Reset() {
    // Disable and reset the timer
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, 0);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}

void init_timer1(int counterValue) {
    // Reset the timer and load the initial value
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET_CCW);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber, counterValue);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}

void timer1Enable(void) {
    // Enable the timer
    XTmrCtr_Enable(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);

}

void timer1Reset() {
    // Disable and reset the timer
    XTmrCtr_Disable(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber, 0);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_1_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);
}



unsigned char decode_NEC() {

	u32 data[32] = {0};

    int start = 0;
    u32 high, low;

    while (!start) {
        u32 value;

        while (!(*AXI_GPIO_IR));

        timer0Enable();
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

        timer0Enable();
        low = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        while (*AXI_GPIO_IR);

        high = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

        int bitValue = ( ( high - low ) > 100000 ) ? 1 : 0;		// Converte in un 1 o uno 0 in base al treshold

        data[i] = bitValue;

        timerReset();
    }
    // Decodifica segnale
    u32 decData = convertToDec(data, 32);

    address = ( decData ) & 0xFF;
	addressInverse = ( decData >> 8 ) & 0xFF;
	command = ( decData >> 16 ) & 0xFF;
	commandInverse = ( decData >> 24 ) & 0xFF;


    // DEBUG
    // decodeAndPrintNECData(data);
    // printData(data);
    printButton(command);

    return command;
}



// Funzione per stampare la sequenza (DEBUG)
void printData(u32 data[]) {
    xil_printf("Bit | Numero\n");
    for (int i = 0; i < 32; i++) {
        xil_printf("%2d  | Bit %2d\n", i + 1, data[i]);
    }
    decodeAndPrintNECData(data);
}

// Decodifica e stampa dati NEC (DEBUG)
void decodeAndPrintNECData(u32 data[]) {
	u32 decData = convertToDec(data, 32);

	// Debug
	// xil_printf("%02X\n", decData);

	address = ( decData ) & 0xFF;
	addressInverse = ( decData >> 8 ) & 0xFF;
	command = ( decData >> 16 ) & 0xFF;
	commandInverse = ( decData >> 24 ) & 0xFF;


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

// Riconoscimento comandi (DEBUG)
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


// ********************** Motor Driver ********************** //

void my_ISR(void) {
	manage_pwm(cnt, pwmA, pwmB, RGBAddr);	// 0x40000008 - channel 1
	set_dir(dirA, dirB, ledGPaddr);			// 0x40000000 - channel 0
	clear_interrupt();
}


void manage_pwm(int cnt, int pwmA, int pwmB, volatile int *RGBaddr) {
    if ( cnt == VOL_UP ) pwmA += 10;
    if ( cnt == VOL_DOWN ) pwmB -= 10;

    pwmA = ( pwmA > 255 ) ? 255 : ( ( pwmA < 0 ) ? 0 : pwmA );
    pwmB = ( pwmB > 255 ) ? 255 : ( ( pwmB < 0 ) ? 0 : pwmB );

    *RGBaddr = (pwmA << 16) | pwmB;
}


void manage_command(int *pwmA, int *pwmB, int *dirA, int *dirB) {
    switch (command) 
    {
        case VOL_UP:
            *pwmA += 10;
            if ( *pwmA > 255 ) 
                *pwmA = 255; // Cap at max PWM value
            break;

        case VOL_DOWN:
            *pwmB -= 10;
            if ( *pwmB < 0 ) 
                *pwmB = 0; // Cap at min PWM value
            break;

        case ARROW_UP:  // Intensita' varia in maniera crescente
            *dirA = 1; 
            *dirB = 0; 
            break;

        case ARROW_DOWN: // Intensita' varia in maniera decrescente
            *dirA = 0;   
            *dirB = 1; 
            break;

        default:
            break;
    }
}


// CONTROL IF IT WORKS AND FIX EVENTUALLY!!!
void set_dir(int dirA, int dirB, volatile int *ledGPaddr)
{
    // Assuming the GPIO address maps the direction bits
    int gpio_val = 0;

    if (dirA == 1) gpio_val |= 0x01; // Set bit 0 for dirA
    if (dirB == 1) gpio_val |= 0x02; // Set bit 1 for dirB

    *ledGPaddr = gpio_val; // Write the value to the GPIO address
}

void clear_interrupt() {
	volatile int *peripheral = (volatile int *)
    *peripheral = 1;
}
