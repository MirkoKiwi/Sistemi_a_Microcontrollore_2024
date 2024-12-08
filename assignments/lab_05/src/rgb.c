/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

// TEMP

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

int leftRLevel, leftGLevel, leftBLevel;
int rightRLevel, rightGLevel, rightBLevel;

// Function Declarations
void timerInit(int counterValue);
void timer0IntAck(void);
void timerISR(void) __attribute__((interrupt_handler));

int main() {
    init_platform();

    timerInit(3000);

    microblaze_enable_interrupts(); // Enable interrupts in the processor

    leftRLevel = 0;
    leftGLevel = 128;
    leftBLevel = 0;
    rightRLevel = 128;
    rightGLevel = 0;
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
