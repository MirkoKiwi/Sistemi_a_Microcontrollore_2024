#include <stdio.h>
#include "platform.h"
#include "xtmrctr_l.h"
#include "xil_printf.h"
#include "xparameters.h"


// Definizioni macro
#define TmrCtrNumber   0               // Usiamo timer 0
#define ONE_SECOND_PERIOD 100000000	   // Macro per definire un secondo
#define CNT_UDT0_MASK  0x00000001      // Timer count up/down mask
#define TMR_T0INT_MASK 0x100           // Timer interrupt mask
#define TIMER_INT_SRC  0b0100          // Timer interrupt source in INTC
#define SWITCH_INT_SRC 0b0010          // Switch interrupt source in INTC

// Indirizzi
#define LED_BASE_ADDR     XPAR_AXI_16LEDS_GPIO_BASEADDR
#define SWITCH_BASE_ADDR  XPAR_AXI_SWITHES_GPIO_BASEADDR    // Typo nell'implementazione originale ( switChes -> swithes )
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
void init_interruptCtrl();
void init_peripheralInterrupt(int address);
void init_timer(int counterValue);
void timer0IntAck(void);


int main() {
    init_platform();

	// Abilita interrupt
	init_interruptCtrl();

	// Inizializza timer ad 1 secondo
	init_timer(ONE_SECOND_PERIOD);

    // Reset LED
    *(int *)LED_BASE_ADDR = 0;

    // Abilita interrupt switch
    init_peripheralInterrupt(SWITCH_BASE_ADDR);


    while (1) {
        // Background
    }

    cleanup_platform();
    return 0;
}


void ledISR(void) {
    // Legge l'interrupt dall'indirizzo della fonte
    int interruptSource = *(int *)INTC_BASE_ADDR;

    // Verifica se l'interrupt arriva dal timer
    if ( interruptSource & TIMER_INT_SRC ) {
        // Intermittenza dei LED
        *(int *)LED_BASE_ADDR = ~(*(int *)LED_BASE_ADDR);

		timer0IntAck();
    }

    // Verifica eventuali interrupt dagli switch
    if ( interruptSource & SWITCH_INT_SRC ) {
        // Legge lo stato degli switch
        int switchInput = ~(*(int *)SWITCH_BASE_ADDR);

        int newCounterValue = ONE_SECOND_PERIOD;

        /*
        // Cambia il periodo del timer in base a quale gruppo di switch viene attivato
        if ( switchInput & 0x00FF ) {
            // Aumenta il periodo di ciclo del timer in base all'attivazione degli 8 switch piu' a destra
            int inputSwitch = ( switchInput & 0x00FF );
        	newCounterValue = ONE_SECOND_PERIOD * ( inputSwitch );
        }
        else if ( switchInput & 0xFF00 ) {
        	// Diminuisce il periodo in base all'attivazione degli 8 switch piu' a sinistra
        	int inputSwitch = (( switchInput & 0xFF00 ) >> 8 );
        	newCounterValue = ONE_SECOND_PERIOD / ( inputSwitch );
        }
		*/


        // Cambia il periodo del timer in base a quale gruppo di switch viene attivato, l'intensita' dell'effetto aumenta man mano che si scorre verso sinistra
        if ( switchInput & 0x000F ) {
            // Quadruplica il periodo di ciclo del timer in base all'attivazione dei 4 switch piu' a destra
            int inputSwitch = ( switchInput & 0x000F );
        	newCounterValue = ONE_SECOND_PERIOD * inputSwitch * 4;
        }
        else if ( switchInput & 0x00F0 ) {
        	// Raddoppia il periodo di ciclo in base all'attivazione dei 4 switch successivi (verso destra)
        	int inputSwitch = (( switchInput & 0x00F0 ) >> 4 );
        	newCounterValue = ONE_SECOND_PERIOD * inputSwitch * 2;
        }
        else if ( switchInput & 0x0F00 ) {
        	// Dimezza il periodo in base all'attivazione dei 4 switch successivi (verso sinistra)
        	int inputSwitch = (( switchInput & 0xF00 ) >> 8 );
        	newCounterValue = ONE_SECOND_PERIOD / ( inputSwitch * 2 );
        }
        else if ( switchInput & 0xF000 ) {
            // Divide per 4 il periodo quando i 4 switch piu' a sinistra vengono attivati
        	int inputSwitch = (( switchInput & 0xF000 ) >> 12 );
        	newCounterValue = ONE_SECOND_PERIOD / ( inputSwitch * 4 );
        }



        // Aggiorna il timer in base al nuovo valore impostato dagli switch
        XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, newCounterValue);

        // Reset interrupt del GPIO
        *(int *)(SWITCH_BASE_ADDR + ISCR) = 0b11;

        // Acknowledge dell'interrupt in INTC (INT[1])
        *(int *)(INTC_BASE_ADDR + IAR) = 0b10;
    }
}

/* Inizializza l'interrupt del processore */
void init_interruptCtrl() {
    // Abilita interrupt controller
    *(int *)(INTC_BASE_ADDR + MER) = 0b11;         // Abilita MER
    *(int *)(INTC_BASE_ADDR + IER) = 0b110;        // Abilita IER per INT[2] e INT[1] (Timer e Switch)

	// Abilita interrupt nel processore
    microblaze_enable_interrupts();
}

/* Inizializza l'interrupt per una qualsiasi periferica */
void init_peripheralInterrupt(int address) {
	// Abilita interrupt degli switch
	*(int *)(SWITCH_BASE_ADDR + GIER) = 1 << 31;    		// Global Interrupt
	*(int *)(SWITCH_BASE_ADDR + Peripheral_IER) = 0b11;     // Abilita Channel Interrupt
}


void init_timer(int counterValue) {
    /* Configurazione TIMER */
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, 0x56); // Imposta il timer control status register
    																  // Il valore 0x56 ci permette di attivare rispettivamente il bit
    																  // 5, che permette di caricare il valore dal registro di carico TLR1 nel registro del counter TCR1
    																  // il bit 3 che permette al timer di sincronizzarsi con segnali esterni (ISR in questo caso)
    																  // il bit 1 permette al timer di conteggiare "verso il basso"

    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, ONE_SECOND_PERIOD);   // Imposta il load-register ad 1 secondo
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber);       		// Initializza il timer, facendolo iniziare a contare


	u32 controlStatus;
    // Fa partire il timer resettando il bit di LOAD0
    controlStatus = XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, controlStatus & (~XTC_CSR_LOAD_MASK));

    // Attiva il timer
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);
}

void timer0IntAck(void) {
	// Acknowledge interrupt del timer
	*(int *)TIMER_BASE_ADDR = (1 << 8) | (*(int *)TIMER_BASE_ADDR); // Imposta il bit T0INT ad 1 nel control register

	// Acknowledge dell'interrupt in INTC (INT[2])
	*(int *)(INTC_BASE_ADDR + IAR) = 0b100;
}
