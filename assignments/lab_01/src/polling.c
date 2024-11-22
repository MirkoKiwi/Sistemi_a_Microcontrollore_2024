///*
*  axi_gpio_1 0x40010000 
*  axi_intc_0 0x41200000 
*  axi_gpio_0 0x40000000 
*  axi_uartlite_0 0x40600000 
*  microblaze_0_local_memory_dlmb_bram_if_cntlr 0x00000000
*  axi_gpio_2 0x40020000 
//*/

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"

#define	axi_gpio_2_addr 0x40020000 	// switches
#define	axi_gpio_1_addr 0x40010000 	// buttons
#define	axi_gpio_0_addr 0x40000000 	// leds


int peek (int* location);
void poke (int* location, int value);

void myISR (void) __attribute__ ((interrupt_handler));


int main()
{
    init_platform();

    // Polling
    while (1) {
    	int switchINPUT = peek( (int *)axi_gpio_2_addr);

    	if ( 1 & switchINPUT )
    		poke( (int *)axi_gpio_0_addr, ( switchINPUT & 0xFFFE ) );
    	else
    		poke( (int *)axi_gpio_0_addr, ~( switchINPUT & 0xFFFE ) );
    }

    cleanup_platform();
    return 0;
}


int peek(int* location) { return *location; }
void poke(int* location, int value) { (*location) = value; }
