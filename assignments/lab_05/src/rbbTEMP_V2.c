#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include "sleep.h"

// Defines
/* Timers */
#define TIMER_NUMBER 0 // Use timer 0
#define CNT_UDT0_MASK 0x00000001
#define TMR_T0INT_MASK 0x100
#define TIMER_INT_SRC 0b0100
/* LEDs */
#define LED_RIGHT 0b000111
#define LED_LEFT 0b111000
// Registers
volatile int* AXI_RGBLEDS = (int*)0x40030000;
// Global Counter and Variables
int dutyCycleCounter = 0;
int leftRLevel = 0, leftGLevel = 0, leftBLevel = 0;
int rightRLevel = 0, rightGLevel = 0, rightBLevel = 0;
int colorStep = 1; // Step for incrementing or decrementing color intensity
// Function Declarations
void timerInit(int counterValue);
void timer0IntAck(void);
void timerISR(void) __attribute__((interrupt_handler));
void updateColors(void);
int main() {
    init_platform();
    timerInit(3000);
    microblaze_enable_interrupts(); // Enable interrupts in the processor
    leftRLevel = 255;
    leftGLevel = 0;
    leftBLevel = 0;
    rightRLevel = 0;
    rightGLevel = 255;
    rightBLevel = 0;
    while (1) {
        // Main loop
    }
    cleanup_platform();
    return 0;
}
// Interrupt Service Routine
void timerISR(void) {
    int interruptSource = *(int*)XPAR_AXI_INTC_0_BASEADDR;
    if (interruptSource & TIMER_INT_SRC) {
        dutyCycleCounter = (dutyCycleCounter < 512) ? (dutyCycleCounter + 1) : 0;
        if (dutyCycleCounter < 256) {
            // Set or clear bits for LEDs based on duty cycle
            *AXI_RGBLEDS =
                ((dutyCycleCounter < leftRLevel) ? (*AXI_RGBLEDS | 0b1000) : (*AXI_RGBLEDS & ~0b1000)) |
                ((dutyCycleCounter < leftGLevel) ? (*AXI_RGBLEDS | 0b10000) : (*AXI_RGBLEDS & ~0b10000)) |
                ((dutyCycleCounter < leftBLevel) ? (*AXI_RGBLEDS | 0b100000) : (*AXI_RGBLEDS & ~0b100000)) |
                ((dutyCycleCounter < rightRLevel) ? (*AXI_RGBLEDS | 0b1) : (*AXI_RGBLEDS & ~0b1)) |
                ((dutyCycleCounter < rightGLevel) ? (*AXI_RGBLEDS | 0b10) : (*AXI_RGBLEDS & ~0b10)) |
                ((dutyCycleCounter < rightBLevel) ? (*AXI_RGBLEDS | 0b100) : (*AXI_RGBLEDS & ~0b100));
        } else {
            *AXI_RGBLEDS = 0; // Turn off LEDs outside the duty cycle range
        }
        if (dutyCycleCounter == 0) {
            updateColors(); // Update colors when a full cycle is complete
        }
        timer0IntAck(); // Acknowledge timer interrupt
    }
}
void timerInit(int counterValue) {
    // Timer Interrupt Configuration
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x1C) = 3; // Enable Master Enable Register (MER)
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x08) = 0b110; // Enable Interrupt Enable Register (IER) for INT[2] and INT[1]
    // Timer Configuration
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, 0x56); // Configure Status Register (SR)
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, counterValue);  // Set counter value
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);       // Load counter register
    // Clear LOAD0 bit to allow the counter to proceed
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, controlStatus & ~XTC_CSR_LOAD_MASK);
    // Enable the timer
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
}
void timer0IntAck(void) {
    // Acknowledge Timer Interrupt
    *(int*)XPAR_AXI_TIMER_0_BASEADDR |= (1 << 8); // Set 8th bit to acknowledge interrupt
    // Acknowledge Interrupt in IAR (Interrupt Acknowledge Register)
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x0C) = 0b100;
}
// Function to update colors in a spinning pattern
void updateColors(void) {
    // Left LED rotation
    if (leftRLevel == 255 && leftGLevel < 255 && leftBLevel == 0) {
        leftGLevel += colorStep;
    } else if (leftGLevel == 255 && leftRLevel > 0 && leftBLevel == 0) {
        leftRLevel -= colorStep;
    } else if (leftGLevel == 255 && leftBLevel < 255) {
        leftBLevel += colorStep;
    } else if (leftBLevel == 255 && leftGLevel > 0) {
        leftGLevel -= colorStep;
    } else if (leftBLevel == 255 && leftRLevel < 255) {
        leftRLevel += colorStep;
    } else if (leftRLevel == 255 && leftBLevel > 0) {
        leftBLevel -= colorStep;
    }
    // Right LED rotation
    if (rightRLevel == 255 && rightGLevel < 255 && rightBLevel == 0) {
        rightGLevel += colorStep;
    } else if (rightGLevel == 255 && rightRLevel > 0 && rightBLevel == 0) {
        rightRLevel -= colorStep;
    } else if (rightGLevel == 255 && rightBLevel < 255) {
        rightBLevel += colorStep;
    } else if (rightBLevel == 255 && rightGLevel > 0) {
        rightGLevel -= colorStep;
    } else if (rightBLevel == 255 && rightRLevel < 255) {
        rightRLevel += colorStep;
    } else if (rightRLevel == 255 && rightBLevel > 0) {
        rightBLevel -= colorStep;
    }
}
