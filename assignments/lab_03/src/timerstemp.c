#include "xparameters.h"
#include "xil_exception.h"
#include "xscugic.h"
#include "xtmrctr_l.h"

#define TIMER_DEVICE_ID XPAR_AXI_TIMER_0_DEVICE_ID
#define INTC_DEVICE_ID XPAR_SCUGIC_SINGLE_DEVICE_ID
#define TIMER_IRPT_INTR XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR
#define LED_BASEADDR XPAR_AXI_GPIO_LED_BASEADDR

#define SWITCH_BASEADDR XPAR_AXI_GPIO_SWITCHES_BASEADDR

XScuGic InterruptController;  // Interrupt controller instance
XTmrCtr TimerInstance;        // Timer instance

void TimerISR(void *CallBackRef);

int main(void) {
    int Status;

    // Initialize the interrupt controller
    XScuGic_Config *IntcConfig;
    IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
    XScuGic_CfgInitialize(&InterruptController, IntcConfig, IntcConfig->CpuBaseAddress);

    // Connect the timer interrupt handler
    XScuGic_Connect(&InterruptController, TIMER_IRPT_INTR, (Xil_ExceptionHandler)TimerISR, (void *)&TimerInstance);

    // Enable the interrupt for the timer
    XScuGic_Enable(&InterruptController, TIMER_IRPT_INTR);

    // Enable interrupts in the processor
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &InterruptController);
    Xil_ExceptionEnable();

    // Initialize and start the timer
    XTmrCtr_SetResetValue(&TimerInstance, 0, XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ);
    XTmrCtr_SetOptions(&TimerInstance, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_Start(&TimerInstance, 0);

    while (1) {
        // Monitor switches to change timer period
        int switch_value = Xil_In32(SWITCH_BASEADDR);
        XTmrCtr_SetResetValue(&TimerInstance, 0, switch_value * XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ);
    }

    return 0;
}

void TimerISR(void *CallBackRef) {
    static int led_state = 0;
    XTmrCtr *TimerInstancePtr = (XTmrCtr *)CallBackRef;

    if (XTmrCtr_IsExpired(TimerInstancePtr, 0)) {
        XTmrCtr_Reset(TimerInstancePtr, 0);

        // Toggle the LED state
        led_state = !led_state;
        Xil_Out32(LED_BASEADDR, led_state);
    }
}
