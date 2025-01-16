#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr_l.h"


// MASKS
#define BUTTON_MASK_0 0b00001
#define BUTTON_MASK_1 0b00010
#define BUTTON_MASK_2 0b00100
#define BUTTON_MASK_3 0b01000
#define BUTTON_MASK_4 0b10000

#define U32_MOST_SIGNIFICANT_BIT 0x80000000

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// LEDS
#define LEFT_BLINKER_MASK   0xF800
#define RIGHT_BLINKER_MASK  0x1F

// TIMERS
#define TmrCtrNumber        0 // Timer 0
#define TIMER_CTRL_RESET    0x56
#define CNT_UDT0_MASK       0x00000001
#define TIMER_T0INT_MASK    0x100
#define TIMER_INT_SRC       0b0100

// Registri
volatile int *buttonsReg =  (int *)XPAR_AXI_BUTTONS_GPIO_BASEADDR;
volatile int *leds16Reg =   (int *)XPAR_AXI_16LEDS_GPIO_BASEADDR;

// Variabili Globali
int isrSignal;
int timerCounter = 7500000;
int flagContinue = 0;
int resetFlag = 1;
int returnIdle = 0;

// Type defs 
typedef enum { PRESSED, IDLE } debounceState_t;
typedef enum { 
    B_IDLE, B_L_ON, B_L_OFF, 
    B_R_ON, B_R_OFF, 
    B_H_ON, B_H_OFF
} blinkersState_t;

// Funzioni FSM
int fsmDebounce(int buttons);
void fsmBlinkers(int buttonsNb);

// Funzioni Timer
void init_timer(int valueCounter);
void timer0IntAck(void);
void timerResetCounter(int value);
void timerEnable(void);

// Interrupt Service Routine 
void blinkISR(void) __attribute__((interrupt_handler));



int main() {
    init_platform();

    microblaze_enable_interrupts();

    /* Initialize variables */
    *leds16Reg = 0x00;
    int buttonsNb = 0;

    init_timer(timerCounter);

    while (1) {
        buttonsNb = fsmDebounce(*buttonsReg);
        fsmBlinkers(buttonsNb);
    }

    cleanup_platform();
    return 0;
}



// FSM (Finite State Machines) function
int fsmDebounce(int buttons) {
    static int debouncedButtons;
    static debounceState_t currentState = IDLE;

    switch (currentState) {
        case IDLE:
            debouncedButtons = buttons;
            if ( buttons != 0 ) {
                currentState = PRESSED;
            }
            break;

        case PRESSED:
            debouncedButtons = 0;
            if ( buttons == 0 ) {
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
        // Idle
        case B_IDLE:
            timerResetCounter(timerCounter);
            flagContinue = 0;
            ledOutput = 0;
            resetFlag = 1;
            returnIdle = 0;

            if ( leftButton ) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_L_ON;
            } 
            else if ( rightButton ) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_R_ON;
            } 
            else if ( hazardButton ) {
                timerEnable();
                resetFlag = 0;
                flagContinue = 1;
                currentState = B_H_ON;
            }
            break;

        // Left
        case B_L_ON:
            flagContinue = 1;

            if ( leftButton ) {
                returnIdle = 1;
            }

            if ( !leftButton && isrSignal > 5 ) {
                currentState = B_L_OFF;
                resetFlag = 1;
                flagContinue = 0;

                while (isrSignal);
                
                resetFlag = 0;
            }

            ledOutput |= ( 1 << isrSignal ) << 10;
            break;

        case B_L_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if ( leftButton || returnIdle ) {
                currentState = B_IDLE;
            } 
            else if ( !leftButton && isrSignal > 2 ) {
                currentState = B_L_ON;
                resetFlag = 1;
                flagContinue = 0;
                
                while (isrSignal);
                
                resetFlag = 0;
            }
            break;

        // Right
        case B_R_ON:
            flagContinue = 1;

            if ( rightButton ) {
                returnIdle = 1;
            }

            if ( !rightButton && isrSignal > 5 ) {
                currentState = B_R_OFF;
                resetFlag = 1;
                flagContinue = 0;
                
                while (isrSignal);
                
                resetFlag = 0;
            }

            ledOutput |= ( U32_MOST_SIGNIFICANT_BIT >> isrSignal ) >> 26;
            break;

        case B_R_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if ( rightButton || returnIdle ) {
                currentState = B_IDLE;
            } 
            else if ( !rightButton && isrSignal > 2 ) {
                currentState = B_R_ON;
                resetFlag = 1;
                flagContinue = 0;
                
                while (isrSignal);
                
                resetFlag = 0;
            }
            break;

        case B_H_ON:
            flagContinue = 1;

            if ( hazardButton ) {
                returnIdle = 1;
            }

            if ( !hazardButton && isrSignal > 5 ) {
                currentState = B_H_OFF;
                resetFlag = 1;
                flagContinue = 0;
                
                while (isrSignal);
                
                resetFlag = 0;
            }

            ledOutput |= ( ( 1 << isrSignal ) << 10 ) | ( ( U32_MOST_SIGNIFICANT_BIT >> isrSignal ) >> 26 );
            break;

        // Hazard
        case B_H_OFF:
            flagContinue = 1;
            ledOutput = 0;

            if ( hazardButton || returnIdle ) {
                currentState = B_IDLE;
            } 
            else if ( !hazardButton && isrSignal > 2 ) {
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

void init_timer(int counterValue) {
    // Abilita interrupt
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + MER) = 0b11;  // Abilita MER
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + IER) = 0b110; // Abilita IER

    // Configurazione Timer
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, TIMER_CTRL_RESET); 	// Configura Status Register (SR)
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, counterValue);  				// Load register impostato su counterValue
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);       				// Inizializza il timer

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);

    // Attiva il timer
    timerEnable();
}

void timer0IntAck(void) {
    // Acknowledge interrupt del timer
    *(int *)XPAR_AXI_TIMER_0_BASEADDR |= TIMER_T0INT_MASK;

    // Acknowledge Interrupt IAR
    *(int *)(XPAR_AXI_INTC_0_BASEADDR + IAR) = TIMER_INT_SRC;	// Acknowledge Global Interrupt
}

void timerResetCounter(int value) {
    XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, value);
    XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);

    u32 controlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, controlStatus & (~XTC_CSR_LOAD_MASK));
}

void timerEnable(void) {
    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

// Interrupt Service Routine 
void blinkISR(void) {
    int interruptSource = *(int *)XPAR_AXI_INTC_0_BASEADDR;

    if ( interruptSource & TIMER_INT_SRC ) {
        if ( resetFlag ) {
            isrSignal = 0;
        } 
        else if ( flagContinue ) {
            isrSignal++;
        }

        timer0IntAck();
    }
}
