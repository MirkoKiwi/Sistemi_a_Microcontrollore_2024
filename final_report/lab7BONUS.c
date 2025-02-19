#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"


// Timer Macro
#define TmrCtrNumber            0
#define TIMER_CTRL_RESET	    0
#define TIMER_CTRL_RESET_CCW    0x56

#define TIMER_T0INT_MASK        0x100      // Timer interrupt mask
#define TIMER_INT_SRC           0b10


// Dichiarazione del GPIO come puntatore volatile
volatile int *AXI_GPIO_IR = (int *)XPAR_GPIO_IR_BASEADDR;



// ********************** Variabili Globali ********************** //
typedef enum {
    POWER_ON = 0xA2,
	FUNC     = 0xE2,
    VOL_UP   = 0x62,
	VOL_DOWN = 0xA8,
	PREV     = 0x22,
	PAUSE    = 0x02,
	NEXT     = 0xC2,
    ARROW_UP = 0x90,
	ARROW_DOWN = 0xE0,
	EQ      = 0x98,
	ST_REPT = 0xB0,
    ZERO    = 0x68,
	ONE     = 0x30,
	TWO     = 0x18,
	THREE   = 0x7A,
	FOUR    = 0x10,
	FIVE    = 0x38,
	SIX     = 0x5A,
	SEVEN   = 0x42,
	EIGHT   = 0x4A,
	NINE    = 0x52
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
#define INTC_BASE_ADDR XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

volatile int *ledGPaddr = (volatile int *)XPAR_GP_LEDS_BASEADDR;	// channel 0
volatile int *RGBaddr   = (volatile int *)0x40000008;	    // channel 1


typedef enum {
	S_BRAKE, CCW, CW, STOP, STANDBY
} MODE;


void my_ISR(void) __attribute__ ((interrupt_handler));

#define cntTreshold 512

int cnt;
int pwmA;	// Increase
int pwmB;	// Decrease
int dirA;	// 1 if CW
int dirB;	// 1 if CCW

int STBY;
int currMODE;

void enable_interrupts();
void clear_interrupt();

void manage_pwm();

void manage_mode();
void printMode();

void manage_command();
void set_dir();



int main() {
    init_platform();

    init_timer0(TmrCtrNumber);

    enable_interrupts();
    init_timer1(3000);
    

    *RGBaddr = 0b111;
    while(1) {
        command = decode_NEC();
        manage_command();
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

    timer1Enable();
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

        timer0Reset();
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

        int bitValue = ( ( high - low ) > 160000 ) ? 1 : 0;		// Converte in un 1 o uno 0 in base al treshold

        data[i] = bitValue;

        timer0Reset();
    }
    // Decodifica segnale
    u32 decData = convertToDec(data, 32);

    address = ( decData >> 24 ) & 0xFF;
	addressInverse = ( decData >> 16 ) & 0xFF;
	command = ( decData >> 8 ) & 0xFF;
	commandInverse = ( decData ) & 0xFF;


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

	address = ( decData >> 24 ) & 0xFF;
	addressInverse = ( decData >> 16 ) & 0xFF;
	command = ( decData >> 8 ) & 0xFF;
	commandInverse = ( decData ) & 0xFF;


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

	for ( int i = 0; i < size; i++ ) {
		result = ( data[i] == 1 ) ? ( result << 1 ) | 1 : ( result << 1 );
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
	manage_pwm();	    // 0x40000008 - channel 1
	set_dir();			// 0x40000000 - channel 0
	clear_interrupt();
}


void enable_interrupts(void) {
    // Abilita interrupt
    *(int *)(INTC_BASE_ADDR + MER) = 0b11;  // Abilita MER
    *(int *)(INTC_BASE_ADDR + IER) = 0b110; // Abilita IER

    microblaze_enable_interrupts();
}

void clear_interrupt() {
    // Acknowledge interrupt del timer
    *(int *)XPAR_AXI_TIMER_1_BASEADDR |= TIMER_T0INT_MASK;

    // Acknowledge Interrupt IAR
    *(int *)(INTC_BASE_ADDR + IAR) = TIMER_INT_SRC;	// Acknowledge Global Interrupt
}


void manage_pwm() {
	int interruptSource = *(int *)INTC_BASE_ADDR;

	if ( interruptSource & TIMER_INT_SRC) {
		if ( cnt < cntTreshold )
			cnt++;
		else
			cnt = 10;
		}

	if ( cnt > pwmA ) {
		*RGBaddr |= 0b001;
	}
	else {
		*RGBaddr &= (~(0b001));
	}

	if ( cnt > pwmB ) {
		*RGBaddr |= 0b010;
	}
	else {
		*RGBaddr &= (~(0b010));
	}

}

void manage_mode() {
	if ( STBY == 0 ) 	    { currMODE = STANDBY; }
	else if ( ( dirA == 1 ) && ( dirB == 1 ) )	{ currMODE = S_BRAKE; }
	else if ( ( dirA == 0 ) && ( dirB == 1 ) )	{ currMODE = (pwmA < pwmB) ? CCW : S_BRAKE; }
	else if ( ( dirA == 1 ) && ( dirB == 0 ) )	{ currMODE = (pwmA > pwmB) ? CW : S_BRAKE; }
	else if ( ( dirA == 0 ) && ( dirB == 0 ) )	{ currMODE = STOP;}

	printMode();
}

void printMode() {
    switch (currMODE) {

        case S_BRAKE:	xil_printf("Current mode: S_BRAKE (Short-Brake)\n");	break;
        case CCW:		xil_printf("Current mode: CCW (Counter-Clockwise)\n");	break;
        case CW:		xil_printf("Current mode: CW (Clockwise)\n"); 			break;
        case STOP:		xil_printf("Current mode: STOP\n");						break;
        case STANDBY:	xil_printf("Current mode: STANDBY\n");					break;

        default:		xil_printf("Invalid mode!\n");							break;
    }
    xil_printf("\n\n");
}


void manage_command() {

	switch (command)
    {
        case VOL_UP:
            if ( dirA && pwmA < cntTreshold )   pwmA += cntTreshold / 16;
            if ( dirB && pwmB < cntTreshold )   pwmB += cntTreshold / 16;
            break;

        case VOL_DOWN:
        	if ( dirA && pwmA > 0 )   pwmA -= cntTreshold / 16;
            if ( dirB && pwmB > 0 )   pwmB -= cntTreshold / 16;
        	break;

        case ARROW_UP:  // Intensita' varia in maniera crescente
        	dirA = ( dirA == 0 ) ? 1 : 0;
            break;

        case ARROW_DOWN: // Intensita' varia in maniera decrescente
        	dirB = ( dirB == 0 ) ? 1 : 0;
            break;

        case POWER_ON:
        	// Spegne tutto
        	STBY = ( STBY == 0 ) ? 1 : 0;

        default:
            break;
    }

   	xil_printf("STBY: %d \ndirA: %d | dirB: %d\n", STBY, dirA, dirB);
	xil_printf("pA: %d | pB: %d\n", pwmA, pwmB);
	manage_mode();
}


void set_dir() {
	
    if ( dirA == 0 && dirB == 0 ) *ledGPaddr = 0b0000; // LDs OFF
	if ( dirA == 1 && dirB == 0 ) *ledGPaddr = 0b0001; // LD1 ON
	if ( dirA == 0 && dirB == 1 ) *ledGPaddr = 0b0010; // LD2 ON
	if ( dirA == 1 && dirB == 1 ) *ledGPaddr = 0b0011; // LDs ON
}
