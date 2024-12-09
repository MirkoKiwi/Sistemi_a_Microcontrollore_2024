#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include <stdlib.h>

// Definizione Catodi
#define CA  0b00000001
#define CB  0b00000010
#define CC  0b00000100
#define CD  0b00001000
#define CE  0b00010000
#define CF  0b00100000
#define CG  0b01000000
#define CDP 0b10000000

// Definizioni Macro Timer
#define TmrCtrNumber         0
#define TIMER_INT_SRC        0b0100
#define TIMER_CTRL_RESET     0x56
#define TIMER_T0INT_MASK     0x100      // Timer interrupt mask
#define TIMER_COUNTER_VALUE  250000

// Indirizzi
// #define LED_BASE_ADDR     XPAR_AXI_16LEDS_GPIO_BASEADDR
// #define SWITCH_BASE_ADDR  XPAR_AXI_SWITHES_GPIO_BASEADDR    // Typo nell'implementazione originale ( switChes -> swithes )
#define INTC_BASE_ADDR      XPAR_AXI_INTC_0_BASEADDR
#define TIMER_BASE_ADDR     XPAR_AXI_TIMER_0_BASEADDR
#define SEV_SEG_BASE_ADDR   XPAR_AXI_7SEGS_GPIO_BASEADDR
#define ANODE_BASE_ADDR     XPAR_AXI_7SEGSAN_GPIO_BASEADDR

// Interrupt Controller Registers
#define IAR 0x0C 	// Interrupt Acknowledge Register
#define IER 0x08 	// Interrupt Enable Register
#define MER 0x1C	// Master Enable Register

/*
// GPIO Peripheral Registers
#define GIER 0x011C				                    // Global Interrupt Enable Register
#define ISCR 0x0120				                    // Interrupt Status Clear Register
#define Peripheral_IER 0x0128	                    // Interrupt Enable Register
*/

// Indirizzi display 7 segmenti
#define AN_7SEG_OUTPUT_REG      ((volatile int*) XPAR_AXI_7SEGSAN_GPIO_BASEADDR)
#define DIGIT_7SEG_OUTPUT_REG   ((volatile int*) XPAR_AXI_7SEGS_GPIO_BASEADDR)


// Variabili globali
char *stringSevSeg;             // Mappa le stringhe al display
int currentAnode = 0;           // Tiene conto dell'anodo attualmente attivo
int lastAnodeIdx = 7;           // Indice dell'ultimo anodo attivo
u8 leftmostAnodeMask = 0xFE;    // Maschera per l'anodo più a sinistra

const float timerStep = 1000;	// Velocità conteggio sul display
								// (valore piu' basso, conteggio piu' veloce)


// Prototipi Funzioni
u8 sevSegDigitMap(char ch);
void write_digit(u8 digit, u8 dotted);
void init_interruptCtrl();
void init_timer(int valueCounter);
void timer0IntAck();
void timerISR(void) __attribute__((interrupt_handler));
void anodeShift(void);
void displaySingleVal(void);
void intToString(int num, char *destination);
u8 calculateLeftmostAnode(int lastAnIdx);


int main() {
    init_platform();

    char sevSegBuffer[8] = {0}; // Implementazione statica, dato che includere stdlib potrebbe causare errori di memoria nella scheda
    stringSevSeg = sevSegBuffer;

	// Inizializzazione anodo più a destra
    *AN_7SEG_OUTPUT_REG = ~1;
    leftmostAnodeMask = calculateLeftmostAnode(lastAnodeIdx);

    // Inizializza interrupt e timer
    init_interruptCtrl();
    init_timer(TIMER_COUNTER_VALUE);


    int counter = 0;

    while (1) {
        intToString(counter / timerStep, sevSegBuffer);
        counter++;
    }

    cleanup_platform();
    return 0;
}

u8 sevSegDigitMap(char ch) {
    // Mappaggio dei caratteri per il display a 7 segmenti
    const u8 digitMap[] = {
        0b00111111,   // 0
        0b00000110,   // 1
        0b01011011,   // 2
        0b01001111,   // 3
        0b01100110,   // 4
        0b01101101,   // 5
        0b01111101,   // 6
        0b00000111,   // 7
        0b01111111,   // 8
        0b01101111,   // 9
        0b01110111,   // A
        0b01111100,   // B
        0b00111001,   // C
        0b00101110,   // D
        0b01111001,   // E
        0b01110001    // F
    };

    if ( ch >= '0' && ch <= '9' ) return ~digitMap[ch - '0'];
    if ( ch >= 'A' && ch <= 'F' ) return ~digitMap[ch - 'A' + 10];
    return 0xFF; // Default ( tutti i segmenti spenti )
}

void write_digit(u8 digit, u8 dotted) {
    *DIGIT_7SEG_OUTPUT_REG = digit | ( dotted ? CDP : 0 );
}

void init_interruptCtrl() {
    // Abilita interrupt controller
    *(int *)(INTC_BASE_ADDR + MER) = 0b11;         // Abilita MER
    *(int *)(INTC_BASE_ADDR + IER) = 0b110;        // Abilita IER

	// Abilita interrupt nel processore
    microblaze_enable_interrupts();
}

void init_timer(int valueCounter) {
	init_interruptCtrl();

    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, TIMER_CTRL_RESET);
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, valueCounter);
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber);

    u32 ctrlStatus = XTmrCtr_GetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber);
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, ctrlStatus & (~XTC_CSR_LOAD_MASK));
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);
}

void timer0IntAck(void) {
    // Acknowledge interrupt del timer
    *(int*)TIMER_BASE_ADDR |= TIMER_T0INT_MASK;

    // Acknowledge Interrupt IAR
    *(int*)(INTC_BASE_ADDR + IAR) = TIMER_INT_SRC;	// Acknowledge Global Interrupt
}

void timerISR() {
    // Timer Interrupt Service Routine
    if (*(int *)INTC_BASE_ADDR & TIMER_INT_SRC) {
        anodeShift();
        displaySingleVal();

        timer0IntAck();
    }
}

void anodeShift() {
    // Shifta al prossimo anodo
    if ( *AN_7SEG_OUTPUT_REG == leftmostAnodeMask ) {
        *AN_7SEG_OUTPUT_REG = ~1;
        currentAnode = 0;
    }
    else {
        *AN_7SEG_OUTPUT_REG = ( *AN_7SEG_OUTPUT_REG << 1 ) | 1;
        currentAnode++;
    }
}

void displaySingleVal() {
    // Mostra la cifra corrispondente all'anodo
    write_digit(sevSegDigitMap(stringSevSeg[lastAnodeIdx - currentAnode]), 0);
}

void intToString(int num, char *destination) {
    if ( num > 99999999 ) { 	// Tutto il display mostra E per mostrare un overflow del valore
		for ( int i = 7; i >= 0; i-- ) {
		    destination[i] = 'E';
		}
		return;
	}

    // Converte un int in una stringa di 8 valori
    for ( int i = 7; i >= 0; i-- ) {
        destination[i] = ( num > 0 || i == 7 ) ? '0' + ( num % 10 ) : ' ';
        num /= 10;
    }
}

u8 calculateLeftmostAnode(int lastAnIdx) {
    return ~( 1 << lastAnIdx );
}

