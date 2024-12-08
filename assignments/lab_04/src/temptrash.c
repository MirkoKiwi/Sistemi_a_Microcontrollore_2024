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
#define ANODE_BASE_ADDR     XPAR_GPIO_1_BASEADDR

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// GPIO Peripheral Registers
#define GIER 0x011C				                    // Global Interrupt Enable Register
#define ISCR 0x0120				                    // Interrupt Status Clear Register
#define Peripheral_IER 0x0128	                    // Interrupt Enable Register


const u8 segmentDigitsMap[16] = {
		0b00111111,		// 0
		0b00000110,		// 1
		0b01011011,		// 2
		0b01001111,		// 3
		0b01100110,		// 4
		0b01101101,		// 5
		0b01111101,		// 6
		0b00000111,		// 7
		0b01111111,		// 8
		0b01101111,		// 9
		0b01110111,		// A
		0b01111100,		// B
		0b00111001,		// C
		0b00101110,		// D
		0b01111001,		// E
		0b01110001		// F
};

volatile u8 digits[8] = {1, 2, 3, 4, 5, 6, 7, 8};
volatile u8 currentDigit = 0;


// Function prototypes
void timerISR();
void write_digit(u8 digit, u8 position);
void init_interrupt();
void init_timer();


int main() {
	
    init_platform();
    init_interrupts();
    init_timer();

	while(1) {
		// Background
	}

	cleanup_platform();
	return 0;
}

void timerISR() {
    static int digit_idx = 0;
    u8 digit_val = digits[digit_idx];
    u8 anode_pattern = ~(1 << digit_idx);

    write_digit(digit_val, digit_idx); 
    Xil_Out32(ANODE_BASE_ADDR, anode_pattern);

    digit_idx = (digit_idx + 1) % 8; // Vai al prossimo digit

    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber) | XTC_CSR_INT_OCCURED_MASK);

    *(volatile u32 *)(INTC_BASE_ADDR + IAR) = TIMER_INT_SRC;
}

void write_digit(u8 digit, u8 position) {
    u32 anodeMask = 1 << position;
    u32 segmentData = segmentDigitsMap[digit];

    Xil_Out32(SEV_SEG_BASE_ADDR, segmentData);
    Xil_Out32(ANODE_BASE_ADDR, ~anodeMask);
}

void init_interrupts() {
    // Initialize interrupt controller
    *(volatile u32 *)(INTC_BASE_ADDR + MER) = 0b11; // Enable Master Enable Register (MER)
    *(volatile u32 *)(INTC_BASE_ADDR + IER) = 0b110; // Enable interrupts for INT[2] and INT[1] (Timer & Switch)

    // Set ISR for timer
    microblaze_register_handler((XInterruptHandler)TimerISR, NULL); 
    microblaze_enable_interrupts();
}

void init_timer() {
    
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, 0x56); // Set timer control status register
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, ONE_SECOND_PERIOD / 1000); // Set timer load register for 1 ms period
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber); 

    // Start the timer 
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);
}
