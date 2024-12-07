#include <stdio.h>
#include "platform.h"
#include "xtmrctr_l.h"
#include "xil_printf.h"
#include "xparameters.h"

// RIVEDERE

// Definitions
#define TmrCtrNumber   0               // Use timer 0
#define CNT_UDT0_MASK  0x00000001      // Timer count up/down mask
#define TMR_T0INT_MASK 0x100           // Timer interrupt mask
#define TIMER_INT_SRC  0b0100          // Timer interrupt source in INTC
#define SWITCH_INT_SRC 0b0010          // Switch interrupt source in INTC

// Base addresses
#define LED_BASE_ADDR     XPAR_AXI_16LEDS_GPIO_BASEADDR
#define SWITCH_BASE_ADDR  XPAR_AXI_SWITHES_GPIO_BASEADDR    // Typo in original implementation ( switChes -> swithes )
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


void ledISR(void) __attribute__((interrupt_handler));

int main() {
    init_platform();

    u32 ControlStatus;

    // Reset LEDs
    *(int *)LED_BASE_ADDR = 0;

    /* Configure Interrupts */
    microblaze_enable_interrupts(); // Enable interrupts on the processor

    // Enable interrupt controller
    *(int *)(INTC_BASE_ADDR + MER) = 0b11;         // Enable Master Enable Register (MER)
    *(int *)(INTC_BASE_ADDR + IER) = 0b110;        // Enable interrupts for INT[2] and INT[1] (Timer & Switch)

    // Enable switches global interrupt and channel interrupts
    *(int *)(SWITCH_BASE_ADDR + GIER) = 1 << 31;    // Global interrupt enable
    *(int *)(SWITCH_BASE_ADDR + Peripheral_IER) = 0b11;       // Enable channel interrupts

    /* Configure Timer */
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, 0x56); // Set timer control status register
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, 100000000);     // Set timer load register for 1-second period
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber);       // Initialize timer counter

    // Start timer by resetting LOAD0 bit
    ControlStatus = XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, ControlStatus & (~XTC_CSR_LOAD_MASK));

    // Enable the timer
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);

    while (1) {
        // Infinite loop: wait for interrupts
    }

    cleanup_platform();
    return 0;
}


void ledISR(void) {
    // Read interrupt source
    int interruptSource = *(int *)INTC_BASE_ADDR;

    // Handle timer interrupt
    if ( interruptSource & TIMER_INT_SRC ) {
        // Toggle LEDs
        *(int *)LED_BASE_ADDR = ~(*(int *)LED_BASE_ADDR);

        // Acknowledge timer interrupt
        *(int *)TIMER_BASE_ADDR = (1 << 8) | (*(int *)TIMER_BASE_ADDR); // Set T0INT bit in control register

        // Acknowledge interrupt in INTC (INT[2])
        *(int *)(INTC_BASE_ADDR + IAR) = 0b100;
    }

    // Handle switch interrupt
    if (interruptSource & SWITCH_INT_SRC) {
        // Read switches state
        int switchInput = ~(*(int *)SWITCH_BASE_ADDR);
        int newCounterValue = 100000000;

        // Adjust timer period based on switches
        if (switchInput & 0xFF) {
            // Increase period (based on rightmost 8 switches)
            newCounterValue = 100000000 * (switchInput & 0xFF);
        } 
        else if (switchInput & 0xFF00) {
            // Decrease period (based on leftmost 8 switches)
            int inputSwitch = (switchInput & 0xFF00) >> 8; // Extract leftmost 8 bits
            newCounterValue = 100000000 / inputSwitch;
        }

        // Update timer with new period
        XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, newCounterValue);

        // Reset GPIO interrupts
        *(int *)(SWITCH_BASE_ADDR + ISCR) = 0b11;

        // Acknowledge interrupt in INTC (INT[1])
        *(int *)(INTC_BASE_ADDR + 0x0C) = 0b10;
    }
}
