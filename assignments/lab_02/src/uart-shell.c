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
