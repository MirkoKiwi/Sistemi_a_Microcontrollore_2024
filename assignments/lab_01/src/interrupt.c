#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

#define	AXI_GPIO_2_ADDR 0x40020000 	// switches
#define	AXI_GPIO_1_ADDR 0x40010000 	// buttons
#define	AXI_GPIO_0_ADDR 0x40000000 	// leds
#define AXI_INTC_bADDR 0x41200000	// interrupt controller

#define GPIO_1 0b01		// buttons ( 1 )
#define GPIO_2 0b10		// switches ( 2 )

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// GPIO Peripheral Registers
#define GIER 0x011C				// Global Interrupt Enable Register
#define ISCR 0x0120				// Interrupt Status Clear Register
#define Peripheral_IER 0x0128	// Interrupt Enable Register


int peek (int *location);
void poke (int *location, int value);

void myISR (void) __attribute__ ((interrupt_handler));


int main()
{
    init_platform();


    // Interrupt Controller
    poke( (int *)(AXI_INTC_bADDR + MER), (int)0b11); 				// Master Enable Register
    poke( (int *)(AXI_INTC_bADDR + IER), (int)0b11); 				// Enable channel int1 and int0

    // Switches
    poke( (int *)(AXI_GPIO_2_ADDR + GIER), (int)1<<31); 			// Enable global interrupt
    poke( (int *)(AXI_GPIO_2_ADDR + Peripheral_IER), (int)0b11); 	// Enable peripheral interrupt

    // Buttons
    poke( (int *)(AXI_GPIO_1_ADDR + GIER), (int)1<<31); 			// Enable global interrupt
    poke( (int *)(AXI_GPIO_1_ADDR + Peripheral_IER), (int)0b11); 	// Enable peripheral interrupt

    microblaze_enable_interrupts();

    poke( (int *)AXI_GPIO_0_ADDR, (int)0);

    while (1) {

    	// Background
    };


    cleanup_platform();
    return 0;
}


int peek(int *location) { return *location; }
void poke(int *location, int value) { (*location) = value; }

void myISR (void) {
	int peripheral_GPIO_INTC = peek( (int *)AXI_INTC_bADDR) & 0b11;	// Check peripheral input

	if ( peripheral_GPIO_INTC == GPIO_1 ) {	// Buttons Priority
		poke( (int *)AXI_GPIO_0_ADDR, (int)0); // Interrupt Status Register

		// Clear Interrupt
		poke( (int *)(AXI_GPIO_1_ADDR + ISCR), (int)0b11); 	// Reset GPIO
		poke( (int *)(AXI_INTC_bADDR + IAR), (int)0b01); 	// Interrupt Acknowledge Register
	}
	else {
		poke( (int *)AXI_GPIO_0_ADDR, (int)0xFFFF); // Interrupt Status Register

		// Clear Interrupt
		poke( (int *)(AXI_GPIO_2_ADDR + ISCR), (int)0b11); 	// Reset GPIO
		poke((int *)(AXI_INTC_bADDR + IAR), (int)0b10);		// Interrupt Acknowledge Register
	}
}
