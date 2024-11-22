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
