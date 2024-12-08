#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include <stdlib.h>

/* Definitions for Cathodes */
#define CA  0b00000001
#define CB  0b00000010
#define CC  0b00000100
#define CD  0b00001000
#define CE  0b00010000
#define CF  0b00100000
#define CG  0b01000000
#define CDP 0b10000000

/* Timer and Interrupt Definitions */
#define TIMER_NUMBER         0
#define TIMER_INT_SRC        0b0100
#define TIMER_CTRL_RESET     0x56
#define TIMER_INT_ACK_MASK   0x100
#define TIMER_COUNTER_VALUE  250000

/* GPIO Base Addresses */
#define AN_7SEG_OUTPUT_REG   ((volatile int*) XPAR_AXI_7SEGSAN_GPIO_BASEADDR)
#define DIGIT_7SEG_OUTPUT_REG ((volatile int*) XPAR_AXI_7SEGS_GPIO_BASEADDR)

/* Function Prototypes */
u8 sevseg_digitMapping(char c);
void write_digit(u8 digit, u8 dotted);
void timerInit(int valueCounter);
void timerISR(void) __attribute__((interrupt_handler));
void anodeShift(void);
void displaySingleVal(void);
void intToString(int val, char *dst);
u8 calculateLeftmostAnode(int lastAnIdx);

/* Global Variables */
char *stringSevSeg;      // Maps string to display
int currentAnode = 0;    // Tracks the active anode
int lastAnodeIdx = 7;    // Last active anode index
u8 leftmostAnodeMask = 0xFE; // Mask for leftmost anode

int main() {
    init_platform();

    /* Initialize Global Variables */
    static char displayString[8] = {0}; // Static allocation to avoid dynamic memory overhead
    stringSevSeg = displayString;

    *AN_7SEG_OUTPUT_REG = ~1; // Initialize anode to the rightmost
    leftmostAnodeMask = calculateLeftmostAnode(lastAnodeIdx);

    /* Enable Interrupts and Timer */
    microblaze_enable_interrupts();
    timerInit(TIMER_COUNTER_VALUE);

    /* Main Loop */
    int counter = 0;
    while (1) {
        intToString(counter / 10000, displayString);
        counter++;
    }

    cleanup_platform();
    return 0;
}

/* Helper Functions */
u8 sevseg_digitMapping(char c) {
    /* Map character to 7-segment display cathodes */
    const u8 digitMap[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, // '0' - '7'
        0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71  // '8' - 'F'
    };
    if (c >= '0' && c <= '9') return ~digitMap[c - '0'];
    if (c >= 'A' && c <= 'F') return ~digitMap[c - 'A' + 10];
    return 0x7E; // Default (all segments off)
}

void write_digit(u8 digit, u8 dotted) {
    *DIGIT_7SEG_OUTPUT_REG = digit | (dotted ? CDP : 0);
}

void timerInit(int valueCounter) {
    /* Initialize Timer */
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x1C) = 3;  // Enable MER
    *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x08) = 0b110; // Enable IER

    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, valueCounter);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);

    u32 ctrlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER, ctrlStatus & (~XTC_CSR_LOAD_MASK));
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TIMER_NUMBER);
}

void timerISR() {
    /* Handle Timer Interrupt */
    if (*(int*)XPAR_AXI_INTC_0_BASEADDR & TIMER_INT_SRC) {
        anodeShift();
        displaySingleVal();
        *(int*)XPAR_AXI_TIMER_0_BASEADDR |= TIMER_INT_ACK_MASK; // Acknowledge Timer Interrupt
        *(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x0C) = TIMER_INT_SRC; // Acknowledge Global Interrupt
    }
}

void anodeShift() {
    /* Shift to the next anode */
    if (*AN_7SEG_OUTPUT_REG == leftmostAnodeMask) {
        *AN_7SEG_OUTPUT_REG = ~1;
        currentAnode = 0;
    } else {
        *AN_7SEG_OUTPUT_REG = (*AN_7SEG_OUTPUT_REG << 1) | 1;
        currentAnode++;
    }
}

void displaySingleVal() {
    /* Display the digit corresponding to the active anode */
    write_digit(sevseg_digitMapping(stringSevSeg[lastAnodeIdx - currentAnode]), 0);
}

void intToString(int val, char *dst) {
    /* Convert integer to 8-character string */
    for (int i = 7; i >= 0; i--) {
        dst[i] = (val > 0 || i == 7) ? '0' + (val % 10) : ' ';
        val /= 10;
    }
}

u8 calculateLeftmostAnode(int lastAnIdx) {
    return ~(1 << lastAnIdx);
}
