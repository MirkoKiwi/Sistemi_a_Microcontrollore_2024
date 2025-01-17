#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

// BUTTON MASKS
#define BUTTON_MASK_LEFT  0b00010
#define BUTTON_MASK_RIGHT 0b01000
#define BUTTON_MASK_HAZARD 0b10000

// LED MASKS
#define LEFT_BLINKER_MASK   0xF800
#define RIGHT_BLINKER_MASK  0x1F
#define HAZARD_MASK (LEFT_BLINKER_MASK | RIGHT_BLINKER_MASK)

// TIMER
#define TIMER_CTRL_RESET    0x56
#define TIMER_T0INT_MASK    0x100
#define TIMER_INT_SRC       0b0100
#define TmrCtrNumber        0

// INTERRUPT CONTROLLER
#define IAR 0x0C // Interrupt Acknowledge Register
#define IER 0x08 // Interrupt Enable Register
#define MER 0x1C // Master Enable Register

// REGISTRI
volatile int *buttonsReg =  (int *)XPAR_AXI_BUTTONS_GPIO_BASEADDR;
volatile int *leds16Reg =   (int *)XPAR_AXI_16LEDS_GPIO_BASEADDR;

// GLOBAL VARIABLES
volatile int isrSignal = 0;
int timerCounter = 6000000;
int activeFlag = 0;
int resetFlag = 1;

// ENUMERATIONS
typedef enum { PRESSED, IDLE } debounceState_t;
typedef enum { 
    B_IDLE, B_LEFT_ON, B_LEFT_OFF, 
    B_RIGHT_ON, B_RIGHT_OFF, 
    B_HAZARD_ON, B_HAZARD_OFF
} blinkersState_t;

// FUNCTION DECLARATIONS
void init_timer(int valueCounter);
void timerResetCounter(int value);
void timerEnable(void);
void timer0IntAck(void);
int fsmDebounce(int buttons);
void fsmBlinkers(int buttons);
void blinkISR(void) __attribute__((interrupt_handler));

// MAIN FUNCTION
int main() {
    init_platform();
    microblaze_enable_interrupts();

    *leds16Reg = 0x00;
    init_timer(timerCounter);

    while (1) {
        int buttons = fsmDebounce(*buttonsReg);
        fsmBlinkers(buttons);
    }

    cleanup_platform();
    return 0;
}

// DEBOUNCE FSM
int fsmDebounce(int buttons) {
    static debounceState_t state = IDLE;
    static int debouncedButtons = 0;

    switch (state) {
        case IDLE:
            if (buttons != 0) {
                state = PRESSED;
                debouncedButtons = buttons;
            }
            break;
        case PRESSED:
            if (buttons == 0) {
                state = IDLE;
                debouncedButtons = 0;
            }
            break;
    }
    return debouncedButtons;
}

// BLINKER FSM
void fsmBlinkers(int buttons) {
    static blinkersState_t state = B_IDLE;
    static int ledOutput = 0;

    int leftPressed = buttons & BUTTON_MASK_LEFT;
    int rightPressed = buttons & BUTTON_MASK_RIGHT;
    int hazardPressed = buttons & BUTTON_MASK_HAZARD;

    switch (state) {
        case B_IDLE:
            timerResetCounter(timerCounter);
            ledOutput = 0;
            resetFlag = 1;
            activeFlag = 0;

            if (leftPressed) {
                state = B_LEFT_ON;
                timerEnable();
                resetFlag = 0;
                activeFlag = 1;
            } else if (rightPressed) {
                state = B_RIGHT_ON;
                timerEnable();
                resetFlag = 0;
                activeFlag = 1;
            } else if (hazardPressed) {
                state = B_HAZARD_ON;
                timerEnable();
                resetFlag = 0;
                activeFlag = 1;
            }
            break;

        case B_LEFT_ON:
            ledOutput |= LEFT_BLINKER_MASK >> isrSignal;
            if (isrSignal > 4 || !leftPressed) {
                state = leftPressed ? B_IDLE : B_LEFT_OFF;
                resetFlag = 1;
            }
            break;

        case B_LEFT_OFF:
            ledOutput = 0;
            if (!leftPressed) {
                state = B_LEFT_ON;
                resetFlag = 1;
            } else {
                state = B_IDLE;
            }
            break;

        case B_RIGHT_ON:
            ledOutput |= RIGHT_BLINKER_MASK << isrSignal;
            if (isrSignal > 4 || !rightPressed) {
                state = rightPressed ? B_IDLE : B_RIGHT_OFF;
                resetFlag = 1;
            }
            break;

        case B_RIGHT_OFF:
            ledOutput = 0;
            if (!rightPressed) {
                state = B_RIGHT_ON;
                resetFlag = 1;
            } else {
                state = B_IDLE;
            }
            break;

        case B_HAZARD_ON:
            ledOutput |= HAZARD_MASK;
            if (isrSignal > 4 || !hazardPressed) {
                state = hazardPressed ? B_IDLE : B_HAZARD_OFF;
                resetFlag = 1;
            }
            break;

        case B_HAZARD_OFF:
            ledOutput = 0;
            if (!hazardPressed) {
                state = B_HAZARD_ON;
                resetFlag = 1;
            } else {
                state = B_IDLE;
            }
            break;
    }
    *leds16Reg = ledOutput;
}

// TIMER FUNCTIONS
void init_timer(int counterValue) {
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + MER) = 0x3;  // Enable MER
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + IER) = TIMER_INT_SRC; // Enable timer interrupt

    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    timerEnable();
}

void timerResetCounter(int value) {
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, value);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

void timerEnable(void) {
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

void timer0IntAck(void) {
    *(int *)XPAR_AXI_TIMER_0_BASEADDR |= TIMER_T0INT_MASK;
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + IAR) = TIMER_INT_SRC;
}

// INTERRUPT SERVICE ROUTINE
void blinkISR(void) {
    if (resetFlag) {
        isrSignal = 0;
    } else {
        isrSignal++;
    }
    timer0IntAck();
}
