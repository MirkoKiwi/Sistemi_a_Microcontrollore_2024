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
#define INTC_BASE_ADDR    XPAR_AXI_INTC_0_BASEADDR
#define TIMER_BASE_ADDR   XPAR_AXI_TIMER_0_BASEADDR

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// GPIO Peripheral Registers
#define GIER 0x011C				                    // Global Interrupt Enable Register
#define ISCR 0x0120				                    // Interrupt Status Clear Register
#define Peripheral_IER 0x0128	                    // Interrupt Enable Register


const u8 segmentDigitsMap[15] = {
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

void write_digit(u8 digit, u8 position) {
	u32 anodeMask = 1 << position;
	u32 segmentData = segmentPatterns[digit];
	*(volatile u32 *)(SEV_SEG_BASE_ADDR + 0x00) = ~anodeMask;
	*(volatile u32 *)(SEV_SEG_BASE_ADDR + 0x04) = segmentData;
}




int main() {
	init_platform();



	while(1) {
		// Background
	}

	cleanup_platform();
	return 0;
}
