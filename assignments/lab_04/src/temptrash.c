#include <stdio.h>
#include "platform.h"
#include "xtmrctr_l.h"
#include "xil_printf.h"
#include "xparameters.h"


// Definizioni macro
#define TmrCtrNumber   0               // Usiamo timer 0
#define ONE_SECOND_PERIOD 100000000	   // Macro per definire un secondo
#define CNT_UDT0_MASK  0x00000001      // Timer count up/down mask
#define TMR_T0INT_MASK 0x100           // Timer interrupt mask
#define TIMER_INT_SRC  0b0100          // Timer interrupt source in INTC
// #define SWITCH_INT_SRC 0b0010          // Switch interrupt source in INTC

// Indirizzi
// #define LED_BASE_ADDR     XPAR_AXI_16LEDS_GPIO_BASEADDR
// #define SWITCH_BASE_ADDR  XPAR_AXI_SWITHES_GPIO_BASEADDR    // Typo nell'implementazione originale ( switChes -> swithes )
#define INTC_BASE_ADDR      XPAR_AXI_INTC_0_BASEADDR
#define TIMER_BASE_ADDR     XPAR_AXI_TIMER_0_BASEADDR
#define SEV_SEG_BASE_ADDR   XPAR_AXI_7SEGS_GPIO_BASEADDR
#define ANODE_BASE_ADDR     XPAR_AXI_7SEGSAN_GPIO_BASEADDR


// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// GPIO Peripheral Registers
#define GIER 0x011C				                    // Global Interrupt Enable Register
#define ISCR 0x0120				                    // Interrupt Status Clear Register
#define Peripheral_IER 0x0128	                    // Interrupt Enable Register

// Definizione Catodi
#define CA  0b00000001
#define CB  0b00000010
#define CC  0b00000100
#define CD  0b00001000
#define CE  0b00010000
#define CF  0b00100000
#define CG  0b01000000
#define CDP 0b10000000


typedef struct {
    char *stringSevSeg;
    int currentAnON;
    int lastAnodeIDX;
    u8 mostLeftAnodeON;
} sevenSegState;


volatile u8 digits[8] = {1, 2, 3, 4, 5, 6, 7, 8};
volatile u8 currentDigit = 0;


// Function prototypes
void setDigit(int value);
void setAnode(int value);

char intToChar(int num);
void timerISR(void) __attribute__((interrupt_handler));
u8 segmentDigitsMap(char ch);
void write_digit(u8 digit, u8 position);
void init_interrupts();
void init_timer();
u8 lastAN_ON_string(int lastAnodeIDX);


int main() {
	
    init_platform();
    init_interrupts();
    init_timer();



    char string[8];
    sevenSegState.stringSevSeg = string;

    sevenSegState.*lastAnodeIDX = (sizeof(string) / sizeoef(string[0])) - 1;
    sevenSegState.*mostLeftAnodeON = lastAnodeOnString(sevenSegState.*lastAnodeIDX);

    int i = 0;

	while(1) {
		// Background

	}

	cleanup_platform();
	return 0;
}


void setDigit(int value) { 
    volatile int *digitSevSegOutput = (int *)SEV_SEG_BASE_ADDR;
    *digitSevSegOutput = value;
}
void setAnode(int value) { 
    volatile int *anodesOutput = (int *)ANODE_BASE_ADDR;
    *anodesOutput = value;
}


char intToChar(int number) { return ( number < 0 || number > 9 ) ? -1 : number  + '0'; }

/*
char *intToString(int number) { 
    int length = my_snprintf(NULL, 0, "%d", number);
    char *str = (char *)my_malloc(length + 1);

    if ( str == NULL ) return NULL;

    my_snprintf(str, "%d", number);
    return str;
}
*/

void timerISR(void) {
    static int digitIdx = 0;
    u8 digitVal = digits[digitIdx];
    u8 anodePattern = ~(1 << digitIdx);

    write_digit(digitVal, digitIdx); 
    *(volatile u32 *)(ANODE_BASE_ADDR) = anode_pattern;

    digitIdx = (digitIdx + 1) % 8;   // Vai al prossimo numero

    // Acknowledge the timer interrupt
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber) | XTC_CSR_INT_OCCURED_MASK);

    // Acknowledge the interrupt in the interrupt controller
    *(volatile u32 *)(INTC_BASE_ADDR + IAR) = TIMER_INT_SRC;
}

u8 segmentDigitsMap(char ch) {
	u8 cathodesOutput = 0;
        
    switch(ch) {
        case '0': cathodesOutput = 0b00111111; break;	    // 0
        case '1': cathodesOutput = 0b00000110; break;		// 1
        case '2': cathodesOutput = 0b01011011; break;		// 2
        case '3': cathodesOutput = 0b01001111; break;		// 3
        case '4': cathodesOutput = 0b01100110; break;		// 4
        case '5': cathodesOutput = 0b01101101; break;		// 5
        case '6': cathodesOutput = 0b01111101; break;		// 6
        case '7': cathodesOutput = 0b00000111; break;		// 7
        case '8': cathodesOutput = 0b01111111; break;		// 8
        case '9': cathodesOutput = 0b01101111; break;		// 9
        case 'A': cathodesOutput = 0b01110111; break;		// A
        case 'B': cathodesOutput = 0b01111100; break;		// B
        case 'C': cathodesOutput = 0b00111001; break;		// C
        case 'D': cathodesOutput = 0b00101110; break;		// D
        case 'E': cathodesOutput = 0b01111001; break;		// E
        case 'F': cathodesOutput = 0b01110001; break;       // F

        default: cathodesOutput = 0b0;
    }   

    // Accende i segmenti richiesti
    return ~cathodesOutput; 
}

void write_digit(u8 digit, u8 dotted) {
    *DIGIT_7SEG_Output = digit | (dotted & CDP);
}

void init_interrupts() {
    microblaze_enable_interrupts();
    
    // Initialize interrupt controller
    *(volatile u32 *)(INTC_BASE_ADDR + MER) = 0b11; // Enable Master Enable Register (MER)
    *(volatile u32 *)(INTC_BASE_ADDR + IER) = 0b110; // Enable interrupts for INT[2] and INT[1] (Timer & Switch)
}

void init_timer() {
    
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, 0x56); // Set timer control status register
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, ONE_SECOND_PERIOD / 1000); // Set timer load register for 1 ms period
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber); 

    // LOAD0 reset
    u32 controlStatus;
    controlStatus = XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber);
    XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, controlStatus & ~(XTC_CSR_LOAD_MASK));

    // Start the timer 
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);
}


u8 lastAnodeOnString(int lastAnodeIDX) { 
	return ~( 1 << lastAnodeIDX );
}
