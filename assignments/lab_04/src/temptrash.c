#include "xintc.h"
#include "xtmrctr.h"
#include "xil_exception.h"

#define TIMER_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define INTC_DEVICE_ID XPAR_INTC_0_DEVICE_ID
#define TIMER_IRPT_INTR XPAR_INTC_0_TMRCTR_0_VEC_ID

XIntc InterruptController;
XTmrCtr TimerInstance;

void TimerISR(void *CallBackRef, u8 TmrCtrNumber) {
    static int digit_idx = 0;
    u8 digit_val = digits[digit_idx];
    u8 anode_pattern = ~(1 << digit_idx);
    
    write_digit(digit_val, 0);
    Xil_Out32(ANODE_BASEADDR, anode_pattern);
    
    digit_idx = (digit_idx + 1) % 8; // Vai al prossimo digit
}

int main() {
    init_platform();

    XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
    XIntc_Connect(&InterruptController, TIMER_IRPT_INTR, (Xil_ExceptionHandler)TimerISR, (void *)&TimerInstance);
    XIntc_Start(&InterruptController, XIN_REAL_MODE);
    XIntc_Enable(&InterruptController, TIMER_IRPT_INTR);
    
    XTmrCtr_Initialize(&TimerInstance, TIMER_DEVICE_ID);
    XTmrCtr_SetHandler(&TimerInstance, TimerISR, &TimerInstance);
    XTmrCtr_SetOptions(&TimerInstance, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(&TimerInstance, 0, ONE_SECOND_PERIOD / 1000);
    XTmrCtr_Start(&TimerInstance, 0);
    
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_InterruptHandler, &InterruptController);
    Xil_ExceptionEnable();
    
    while (1) {
        // Background
    }

    cleanup_platform();
    return 0;
}
