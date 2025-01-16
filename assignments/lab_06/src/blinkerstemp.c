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
* XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

/* Defines */

// MASKS
#define BUTTON_MASK_0 0b00001
#define BUTTON_MASK_1 0b00010
#define BUTTON_MASK_2 0b00100
#define BUTTON_MASK_3 0b01000
#define BUTTON_MASK_4 0b10000

#define U32_MOST_SIGNIFICANT_BIT 0x80000000

// LEDS
#define LEFT_BLINKER_MASK 0xF800
#define RIGHT_BLINKER_MASK 0x1F

// TIMERS
#define TIMER_NUMBER 0 // Timer 0
#define CNT_UDT0_MASK 0x00000001
#define TMR_T0INT_MASK 0x100
#define TIMER_INT_SRC 0b0100

/* Registers */
volatile int* buttonsReg = (int*)XPAR_AXI_BUTTONS_GPIO_BASEADDR;
volatile int* leds16Reg = (int*)XPAR_AXI_16LEDS_GPIO_BASEADDR;

/* Global variables */
int isrSignal;
int timerCounter = 7500000;
int flagContinue = 0;
int resetFlag = 1;
int returnIdle = 0;

/* Type definitions */
typedef enum { PRESSED, IDLE } debounceState_t;
typedef enum { 
    B_IDLE, B_L_ON, B_L_OFF, 
    B_R_ON, B_R_OFF, 
    B_H_ON, B_H_OFF
} blinkersState_t;

/* FSM functions */
int fsmDebounce(int buttons);
void fsmBlinkers(int buttonsNb);

/* Timer library */
void timerInit(int valueCounter, int noEnable);
void timer0IntAck(void);
void timerResetCounter(int value);
void timerEnable(void);

/* Interrupt Service Routine */
void blinkISR(void) __attribute__((interrupt_handler));

int main() {
    init_platform();

    /* Enable Microblaze interrupts */
    microblaze_enable_interrupts();

    /* Initialize variables */
    *leds16Reg = 0x00;
    int buttonsNb = 0;

    timerInit(timerCounter, 1); // Timer initialized but not started

    while (1) {
        buttonsNb = fsmDebounce(*buttonsReg);
        fsmBlinkers(buttonsNb);
    }

    cleanup_platform();
    return 0;
}

/* FSM (Finite State Machines) functions */
int fsmDebounce(int buttons) {
    static int debouncedButtons;
    static debounceState_t currentState = IDLE;

    switch (currentState) {
        case IDLE:
            debouncedButtons = buttons;
            if (buttons != 0) {
                currentState = PRESSED;
            }
            break;

        case PRESSED:
            debouncedButtons = 0;
            if (buttons == 0) {
                currentState = IDLE;
            }
            break;
    }

    return debouncedButtons;
}

void fsmBlinkers(int buttonsNb) {
    static u32 ledOutput;
    static blinkersState_t currentState = B_IDLE;

    int leftButton = buttonsNb & BUTTON_MASK_1;
    int rightButton = buttonsNb & BUTTON_MASK_3;
    int hazardButton = buttonsNb & BUTTON_MASK_4;

    switch (currentState) {
        case B_IDLE:
            timerResetCounter(timerCounter);
            flagContinue = 0;
            ledOutput = 0;
            resetFlag = 1;
            returnIdle = 0;

            if (leftButton) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_L_ON;
            } else if (rightButton) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_R_ON;
            } else if (hazardButton) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_H_ON;
            }
            break;

        case B_L_ON:
            flagContinue = 1;

            if (leftButton) {
                returnIdle = 1;
            }

            if (!leftButton && isrSignal > 5) {
                currentState = B_L_OFF;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }

            ledOutput |= (1 << isrSignal) << 10;
            break;

        case B_L_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if (leftButton || returnIdle) {
                currentState = B_IDLE;
            } else if (!leftButton && isrSignal > 2) {
                currentState = B_L_ON;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }
            break;

        case B_R_ON:
            flagContinue = 1;

            if (rightButton) {
                returnIdle = 1;
            }

            if (!rightButton && isrSignal > 5) {
                currentState = B_R_OFF;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }

            ledOutput |= (U32_MOST_SIGNIFICANT_BIT >> isrSignal) >> 26;
            break;

        case B_R_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if (rightButton || returnIdle) {
                currentState = B_IDLE;
            } else if (!rightButton && isrSignal > 2) {
                currentState = B_R_ON;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }
            break;

        case B_H_ON:
            flagContinue = 1;

            if (hazardButton) {
                returnIdle = 1;
            }

            if (!hazardButton && isrSignal > 5) {
                currentState = B_H_OFF;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }

            ledOutput |= ((1 << isrSignal) << 10) | ((U32_MOST_SIGNIFICANT_BIT >> isrSignal) >> 26);
            break;

        case B_H_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if (hazardButton || returnIdle) {
                currentState = B_IDLE;
            } else if (!hazardButton && isrSignal > 2) {
                currentState = B_H_ON;
                resetFlag = 1;
                flagContinue = 0;
                while (isrSignal);
                resetFlag = 0;
            }
            break;
    }

    *leds16Reg = ledOutput;
}

/* Timer library */
void timerInit(int valueCounter, int noEnable) {
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x1C) = 3;
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x08) = 0b110;

    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, 0x56);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, valueCounter);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);

    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, controlStatus & (~XTC_CSR_LOAD_MASK));

    if (!noEnable) {
        XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    }
}

void timer0IntAck(void) {
    *(int*)XPAR_AXI_TIMER_0_BASEADDR = (1 << 8) | (*(int*)XPAR_AXI_TIMER_0_BASEADDR);
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x0C) = 0b100;
}

void timerResetCounter(int value) {
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, value);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);

    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, controlStatus & (~XTC_CSR_LOAD_MASK));
}

void timerEnable(void) {
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
}

/* Interrupt Service Routine */
void blinkISR(void) {
    int interruptSource = *(int*)XPAR_AXI_INTC_0_BASEADDR;

    if (interruptSource & TIMER_INT_SRC) {
        if (resetFlag) {
            isrSignal = 0;
        } else if (flagContinue) {
            isrSignal++;
        }

        timer0IntAck();
    }
}
