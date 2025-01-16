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

#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

/* Defines */

// MASKS
#define bmask_0 0b00001
#define bmask_1 0b00010
#define bmask_2 0b00100
#define bmask_3 0b01000
#define bmask_4 0b10000

#define u32_mostSignificantBit_1 0x80000000
// LEDS

#define LEFT_BLINKER_m 0xF800
#define RIGHT_BLINKER_m 0x1F

// TIMERS

#define TmrCtrNumber 0 // Usiamo il timer 0
#define CNT_UDT0_MASK 0x00000001
#define TMR_T0INT_MASK 0x100

#define TIMER_INT_SRC 0b0100



/* Registers */

volatile int* buttons_Reg = (int*)XPAR_AXI_BUTTONS_GPIO_BASEADDR;
volatile int* LEDS_16_Reg = (int*)XPAR_AXI_16LEDS_GPIO_BASEADDR;


/* Global variables */

int ISR_SIGNAL;
int timerCounter = 7500000;
int flag_continue = 0;
int reset = 1;
int returnIdle = 0;

/* Type defs */

typedef enum {pressed, idle} debounce_state_t;
typedef enum {	B_IDLE,
				B_L_ON,B_L_OFF,
				B_R_ON,B_R_OFF,
				B_H_ON,B_H_OFF
} blinkers_state_t;

/* FSM functions */

int FSM_debounce(int buttons);
void FSM_blinkers (int buttons_nb);


/* TIMER LIBRARY */

void timerInit(int valueCounter, int NO_ENABLE);
void timer0_IntAck(void);
void timer_ResetCounter(int value);
void timer_enable(void); // Wrapping

/* Interrupt Service Routine */

void blinkISR (void) __attribute__ ((interrupt_handler));



int main()
{
    init_platform();

    /* Enable Microblaze interrupts */

    microblaze_enable_interrupts();

   /* Variable declaration and initialisation */

    *LEDS_16_Reg = 0x00;

    int buttons_nb = 0;


    timerInit(timerCounter, 1); // Se il secondo param � a 1, il timer non inizia




    while (1) {

    	buttons_nb =  FSM_debounce(*buttons_Reg);
    	FSM_blinkers(buttons_nb);

    }



    cleanup_platform();
    return 0;
}

/*FSM (Finite State Machines) functions */

int FSM_debounce(int buttons) {

	static int debounced_buttons;

	static debounce_state_t currentState = idle;

	switch (currentState) {
	case idle:
		debounced_buttons = buttons;
		if (buttons!=0) currentState = pressed;
	break;

	case pressed:
		debounced_buttons = 0;
		if (buttons==0) currentState = idle;
	break;

	}

	return debounced_buttons;
}


void FSM_blinkers (int buttons_nb) {

	static u32 led_output;
	static blinkers_state_t currentState = B_IDLE;

	int left_button = buttons_nb & bmask_1;
	int right_button = buttons_nb & bmask_3;
	int hazard_button = buttons_nb & bmask_4;


	switch (currentState) {

	case B_IDLE:

		timer_ResetCounter(timerCounter);
		flag_continue = 0;
		led_output = 0;
		reset = 1;
		returnIdle = 0;

		if (left_button) {
			timer_enable();
			reset = 0;
			flag_continue = 1;
			currentState = B_L_ON;
		}

		else if (right_button) {
			timer_enable();
			reset = 0;
			flag_continue = 1;
			currentState = B_R_ON;

		} else if (hazard_button) {
			timer_enable();
			reset = 0;
			flag_continue = 1;
			currentState = B_H_ON;
		}



		break;

/* LEFT STATES */


	case B_L_ON:
		flag_continue = 1;

		if (left_button) { // Ci permette di terminare il ciclo di luci prima di spegnerle
			returnIdle = 1;
		}

		if (((!left_button) && (ISR_SIGNAL > 5))) {  // Se non premo il bottone, vado in off per far ripartire la sequenza
				currentState = B_L_OFF;
				reset = 1;
				flag_continue = 0;
				while (ISR_SIGNAL); // Attendo che il segnale si sia resettato
				reset = 0;

			}

		led_output = ((led_output) | (1<<ISR_SIGNAL) << 10); // Algebra booleana che permette di creare l'uscita a 32 bit dei led

		break;



	case B_L_OFF:
		flag_continue = 1;

		led_output = 0;

		if (left_button || returnIdle) { // sia che noi clicchiamo il tasto sia che l'abbiamo cliccato prima, torniamo in idle
			currentState = B_IDLE;
		}

		if ((!left_button) && (ISR_SIGNAL >2)) { // Stessa logica del B_L_ON
			currentState = B_L_ON;
			reset = 1; // Resetto il counter globale
			flag_continue = 0;
			while (ISR_SIGNAL); // Attendo che il segnale si sia resettato

			reset = 0;

		}

		break;

/* RIGHT STATES */

	case B_R_ON: // Stessa logica descritta dai commenti nel LEFT

		flag_continue = 1;

		if (right_button) {
			returnIdle = 1;
		}

		if (((!right_button) && (ISR_SIGNAL > 5))) {
				currentState = B_R_OFF;
				reset = 1;
				flag_continue = 0;
				while (ISR_SIGNAL);
				reset = 0;

			}

		led_output = (led_output) | ((u32_mostSignificantBit_1>>ISR_SIGNAL)>>26); // Algebra booleana che permette di produrre l'uscita dei led




		break;

	case B_R_OFF:

		flag_continue = 1;

		led_output = 0;

		if (right_button || returnIdle) {
			currentState = B_IDLE;
		}

		if ((!right_button) && (ISR_SIGNAL >2)) {
			currentState = B_R_ON;
			reset = 1;
			flag_continue = 0;
			while (ISR_SIGNAL); // Attendo che il segnale si sia resettato

			reset = 0;

		}

		break;


/* HAZARD STATES */

	case B_H_ON:
		flag_continue = 1;

		if (hazard_button) {
			returnIdle = 1;
		}

		if (((!hazard_button) && (ISR_SIGNAL > 5))) {
				currentState = B_H_OFF;
				reset = 1;
				flag_continue = 0;
				while (ISR_SIGNAL); // Attendo che il segnale si sia resettato
				reset = 0;

			}

		led_output |= ((1<<ISR_SIGNAL) << 10) | ((u32_mostSignificantBit_1>>ISR_SIGNAL)>>26); // Combinazione logica delle due uscite di LEFT e RIGHT


		break;

	case B_H_OFF:

		flag_continue = 1;

		led_output = 0;

		if (hazard_button || returnIdle) {
			currentState = B_IDLE;
		}

		if ((!hazard_button) && (ISR_SIGNAL >2)) {
			currentState = B_H_ON;
			reset = 1;
			flag_continue = 0;
			while (ISR_SIGNAL); // Attendo che il segnale si sia resettato

			reset = 0;

		}


		break;


	}


	*LEDS_16_Reg = led_output;
}





/* TIMER LIBRARY */

void timerInit(int valueCounter, int NO_ENABLE) { // Wrapping per inizializzazione timer

	/* TIMER INTERRUPT*/

	*(int*)(XPAR_AXI_INTC_0_BASEADDR+0x1C) = (int)3; // Abilito MER

	*(int*)(XPAR_AXI_INTC_0_BASEADDR+0x08) = 0b110; //Abilito IER per INT[2] e INT[1]


    /* SETTING TIMER */


	XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, 0x56); // Setto il SR a 56 (combinazione di bit che ci interessa)

	XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, valueCounter); // Setto il counter a 100000000 (pulse ogni sec)
	XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Inizializzo il counter tramite il corrispettivo bit nello status register


	// Resetto LOAD0 in modo che il counter possa andare avanti
    u32 ControlStatus;

	ControlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Leggo il valore dello status register
	XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, ControlStatus & (~XTC_CSR_LOAD_MASK)); // Metto il bit 5 (LOAD0) a zero facendo una and

	// Il timer inizia ora se il secondo arg � 0

    if (!NO_ENABLE) XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Abilito il timer

}

void timer0_IntAck(void) { // Wrapping del timer0 ack
	/* Timer Acknowledgment*/

	*(int*)XPAR_AXI_TIMER_0_BASEADDR = (1<<8) | (*(int*)XPAR_AXI_TIMER_0_BASEADDR); // OR tra l'ottavo bit a 1 e lascio invariati gli altri bit


	// IAR [INT 2]

	*(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x0C) = 0b100;

}

void timer_ResetCounter(int value) { // Resetta il counter e lo tiene pronto

	XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
	XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, value); // Setto il counter
	XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Inizializzo il counter tramite il corrispettivo bit nello status register

	u32 ControlStatus;

	ControlStatus = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Leggo il valore dello status register
	XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber, ControlStatus & (~XTC_CSR_LOAD_MASK)); // Metto il bit 5 (LOAD0) a zero facendo una and

}

void timer_enable(void) { // Wrapping
	XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber);
}

/* Interrupt Service Routine */

void blinkISR (void) {
	int interrupt_source = *(int*)XPAR_AXI_INTC_0_BASEADDR;
	if (interrupt_source & TIMER_INT_SRC) {


		if (reset) ISR_SIGNAL = 0;
		else if (flag_continue) {
			ISR_SIGNAL++;
		}



		timer0_IntAck(); // Fa l'ack del timer

	}

}
