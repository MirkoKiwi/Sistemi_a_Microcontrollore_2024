#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include "sleep.h"


// Definizioni Macro Timer
#define TmrCtrNumber         0
#define TIMER_INT_SRC        0b0100
#define TIMER_CTRL_RESET     0x56
#define CNT_UDT0_MASK 		 0x00000001
#define TIMER_T0INT_MASK     0x100      // Timer interrupt mask
#define TIMER_COUNTER_VALUE  250000

// Indirizzi 
#define LED_BASE_ADDR     	XPAR_AXI_16LEDS_GPIO_BASEADDR
#define SWITCH_BASE_ADDR  	XPAR_AXI_SWITHES_GPIO_BASEADDR    // Typo nell'implementazione originale ( switChes -> swithes )
#define INTC_BASE_ADDR      XPAR_AXI_INTC_0_BASEADDR
#define TIMER_BASE_ADDR     XPAR_AXI_TIMER_0_BASEADDR

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

// LED
#define LED_RIGHT 0b000111
#define LED_LEFT 0b111000

// Indirizzo Registro LED
volatile int* AXI_RGBLEDS = (int*)XPAR_AXI_RGBLEDS_GPIO_BASEADDR;

// Variabili Globali
const int maxDutyCycle = ( 512 / 2 ) / 32;	// Intensita' Colore (Treshold consigliato 256)
int dutyCycleCounter = 0;

int leftRLevel, leftGLevel, leftBLevel;
int rightRLevel, rightGLevel, rightBLevel;



void timerISR(void) __attribute__((interrupt_handler));
void init_interruptCtrl();
void init_timer(int counterValue);
void timer0IntAck(void);



int main() {
    init_platform();

    // Inizializzazione intterupt e timer
    init_interruptCtrl();
    init_timer(3000);

    // Impostazione colori
    leftRLevel = 255;
    leftGLevel = 128;
    leftBLevel = 0;
    rightRLevel = 255;
    rightGLevel = 255;
    rightBLevel = 255;

    while (1) {
        // Background
    }

    cleanup_platform();
    return 0;
}

// Interrupt Service Routine
void timerISR(void) {
    int interruptSource = *(int*)INTC_BASE_ADDR;
    if (interruptSource & TIMER_INT_SRC) {
        dutyCycleCounter = ( dutyCycleCounter < 512) ? (dutyCycleCounter + 1) : 0;

        if ( dutyCycleCounter < maxDutyCycle ) {
            // OR per alzare i bit, XOR ( A and not B ) per abbassarli
            *AXI_RGBLEDS =
                ( ( dutyCycleCounter < leftRLevel ) ? ( *AXI_RGBLEDS | 0b1000 ) : ( *AXI_RGBLEDS & ~0b1000) ) |
                ( ( dutyCycleCounter < leftGLevel ) ? ( *AXI_RGBLEDS | 0b10000 ) : ( *AXI_RGBLEDS & ~0b10000) ) |
                ( ( dutyCycleCounter < leftBLevel ) ? ( *AXI_RGBLEDS | 0b100000 ) : ( *AXI_RGBLEDS & ~0b100000) ) |
                ( ( dutyCycleCounter < rightRLevel ) ? ( *AXI_RGBLEDS | 0b1 ) : ( *AXI_RGBLEDS & ~0b1) ) |
                ( ( dutyCycleCounter < rightGLevel ) ? ( *AXI_RGBLEDS | 0b10 ) : ( *AXI_RGBLEDS & ~0b10) ) |
                ( ( dutyCycleCounter < rightBLevel ) ? ( *AXI_RGBLEDS | 0b100 ) : ( *AXI_RGBLEDS & ~0b100) );
        }
        else {
            *AXI_RGBLEDS = 0; // Spegne i LED se sono fuori dal duty cycle
        }

        timer0IntAck(); // Acknowledge timer
    }
}


void init_interruptCtrl() {
    // Abilita interrupt
    *(int *)(INTC_BASE_ADDR + MER) = 0b11;  // Abilita MER
    *(int *)(INTC_BASE_ADDR + IER) = 0b110; // Abilita IER

    // Abilita interrupt dìnel processore
    microblaze_enable_interrupts();
}

void init_timer(int counterValue) {
    // Configurazione Timer
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, TIMER_CTRL_RESET); 	// Configura Status Register (SR)
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, counterValue);  				// Load register impostato su counterValue
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber);       				// Inizializza il timer

    // Fa partire il timer resettando il bit di LOAD0
    u32 controlStatus = XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, controlStatus & ~XTC_CSR_LOAD_MASK);

    // Attiva il timer
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);
}

void timer0IntAck(void) {
    // Acknowledge interrupt del timer
    *(int*)TIMER_BASE_ADDR |= (1 << 8); // Set 8th bit to acknowledge interrupt

    // Acknowledge Interrupt IAR
    *(int*)(INTC_BASE_ADDR + IAR) = 0b100;
}
