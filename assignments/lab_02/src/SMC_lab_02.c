// Loopback

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"

#define AXI_UARTLITE_0_BASEADDR 0x40600000


int main(){
	init_platform();
    
	while(1) {
        	// Riceve dati dalla periferica UARTLite
		char ch = XUartLite_RecvByte(axi_uartlite_0);
		
        	// Rispedisce indietro gli stessi dati ( creando una loopback )
        	XUartLite_SendByte(axi_uartlite_0, ch);
	}

    	cleanup_platform();
    	return 0;
}



// ----------------------------------------------------------------------------------------------



// Uart-Shell

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"


#define AXI_GPIO_LEDS_BASEADDR  0x40000000
#define AXI_UARTLITE_0_BASEADDR 0x40600000


// Converte un carattere numerico di tipo 'char' nel suo equivalente di tipo 'int'
int charNumToInt(char ch);


int main(){

	init_platform();
    

	while(1){
		char ch = XUartLite_RecvByte(AXI_UARTLITE_0_BASEADDR);

        	// Se il carattere in ricezione è 'a', accende tutti i led
		if ( ch == 'a' ) {
            		*(int *)AXI_GPIO_LEDS_BASEADDR = 0xFFFF; 
        	}
		else {
            		// Il carattere viene convertito in numero ed i led vengono accesi secondo il suo valore
			int num = charNumToInt(ch);
			if ( num != -1 )    
                	*(int*)AXI_GPIO_LEDS_BASEADDR = num;
		}
	}

	cleanup_platform();
	return 0;
}


int charNumToInt(char ch) {
    	// Converte numero di tipo 'char' nel suo equivalente di tipo 'int', ritorna -1 se il carattere non è un numero
	return ( ch < 48 || ch > 57 ) ? -1 : ch - '0';
}



// ----------------------------------------------------------------------------------------------



// Led Control with non-blocking UART


#include <stdio.h>  
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xuartlite_l.h"


#define AXI_UARTLITE_0_BASEADDR    0x40600000

#define AXI_GPIO_LEDS_BASEADDR     0x40000000 // led ( gpio_0 )
#define AXI_GPIO_BUTTONS_BASEADDR  0x40010000 // buttons ( gpio_1 )
#define AXI_GPIO_SWITCHES_BASEADDR 0x40020000 // switches ( gpio_2 )


#define UART     (u8)2
#define SWITCHES (u8)1
#define BUTTONS  (u8)0


int charNumToInt(char ch);

u32 my_XUartLite_RecvByte(UINTPTR BaseAddress);

void update_leds(u32 data, u8 mode);


int main() {
	init_platform();

	// Registri di input
	u32 uart = 0;
	u32 buttons = 0;
	u32 switches = 0;
	
	while(1) {
	    // Aggiornamento leds in lettura da buttons
	    buttons = Xil_In32(AXI_GPIO_BUTTONS_BASEADDR);
	    update_leds(buttons, BUTTONS);
	
	    // Aggiornamento leds in lettura dagli switch
	    switches = Xil_In32(AXI_GPIO_SWITCHES_BASEADDR);
	    update_leds(switches, SWITCHES);
	
	    // Aggiornamento leds in lettura da non-blocking UART
	    uart = my_XUartLite_RecvByte(AXI_UARTLITE_0_BASEADDR);
	    update_leds(uart, UART);
	}
	
	
	cleanup_platform();
	return 0;
}



int charNumToInt(char ch) {
	// Converte numero di tipo 'char' nel suo equivalente di tipo 'int', ritorna -1 se il carattere non è un numero
	return ( ch < 48 || ch > 57 ) ? -1 : ch - '0';
}


u32 my_XUartLite_RecvByte(UINTPTR BaseAddress) { 
	// Ritorna END OF TRANSMISSION (EOT) se il bit 0 dello status register = 1 (RX FIFO empty)
	if (XUartLite_IsReceiveEmpty(BaseAddress)) {
	    return 4; // End of Transmission (EOT) marker
	}

	return (u8)XUartLite_ReadReg(BaseAddress, XUL_RX_FIFO_OFFSET);
}



void update_leds(u32 data, u8 mode) {

	u32 currentLedStatus = *(int*)AXI_GPIO_LEDS_BASEADDR; // Lettura stato dei led
	u32 ledMask          = currentLedStatus;	
	u32 newLedOutput     = 0;
	
	char receivedChar = (u8)data; // Prendo gli 8 bit meno significativi del dato in ricezione ( di tipo char )
	
	switch (mode) {
	    case UART:
		if ( receivedChar != 4 ) { // c = 4 implica che la UART sia vuota
		    // Process Data
		    if ( receivedChar == 'a' )
			// Accendo i primi 8 led
			newLedOutput = 0xFF00; 
		    else {
			int number = charNumToInt(receivedChar);
	
			if ( number != -1 ) { 
			    // Controllo i primi 8 led se il numero è valido
			    newLedOutput = ( number << 8 ); 
			}
			else
			    // Output invariato
			    newLedOutput = currentLedStatus; 
		    }
		    // Gli ultimi 8 led rimangono invariati
		    ledMask &= 0x00FF; 
		}
		break;
	
	    case SWITCHES:
		// Primi 12 led invariati
		ledMask &= 0xFFF0; 
		newLedOutput = ( ~(data) & 0xF ); // Accendo solo gli ultimi 4 switch
		break;
	
	    case BUTTONS:
		// Primi 12 led e ultimi 4 led invariati
		ledMask &= 0xFF0F;  
		newLedOutput = ( (data & 0xF) << 4 ); // Accendo solo i rimanenti 4 led in mezzo
		break;
	
	    default:
		return;
	}
	
	// Accendo i led tenendo conto dello stato precedente
	*( (int*)AXI_GPIO_LEDS_BASEADDR ) = ledMask | newLedOutput;
}
