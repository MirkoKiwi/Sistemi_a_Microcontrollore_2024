#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr_l.h"
#include <stdlib.h>

/* Definitions for Cathodes*/

#define CA 0b00000001
#define CB 0b00000010
#define CC 0b00000100
#define CD 0b00001000
#define CE 0b00010000
#define CF 0b00100000
#define CG 0b01000000
#define CDP 0b10000000

/* Define for timers */
#define TmrCtrNumber 0 // Usiamo il timer 0
#define CNT_UDT0_MASK 0x00000001
#define TMR_T0INT_MASK 0x100

#define TIMER_INT_SRC 0b0100

//#define XPAR_AXI_16LEDS_GPIO_BASEADDR 0x40000000

//#define MOST_LEFT_ANODE_ON 0b01111111
//#define LAST_ANODE_INDX 7 // Va da 0 a 7





///* Definitions for peripheral AXI_7SEGSAN_GPIO */
//#define XPAR_AXI_7SEGSAN_GPIO_BASEADDR 0x40050000
//#define XPAR_AXI_7SEGSAN_GPIO_HIGHADDR 0x4005FFFF
//#define XPAR_AXI_7SEGSAN_GPIO_DEVICE_ID 1
//#define XPAR_AXI_7SEGSAN_GPIO_INTERRUPT_PRESENT 0
//#define XPAR_AXI_7SEGSAN_GPIO_IS_DUAL 0
//
//
///* Definitions for peripheral AXI_7SEGS_GPIO */
//#define XPAR_AXI_7SEGS_GPIO_BASEADDR 0x40040000
//#define XPAR_AXI_7SEGS_GPIO_HIGHADDR 0x4004FFFF
//#define XPAR_AXI_7SEGS_GPIO_DEVICE_ID 2
//#define XPAR_AXI_7SEGS_GPIO_INTERRUPT_PRESENT 0
//#define XPAR_AXI_7SEGS_GPIO_IS_DUAL 0


/* FUNCTIONS */

u8 sevseg_digitMapping(char c); // Mappa il carattere ai catodi corrispondenti

char int_to_char(int val); // Converte l'intero a carattere

void write_digit (u8 digit, u8 dotted); // Scrive sul 7 segmenti

void timerInit(int valueCounter); // Wrapping per init timer (con interrupt)

void timer0_IntAck(void); // Wrapping per l'ack dei timer

void anodeShift (void); // Attiva l'anodo successivo

void displaySingleVal(void); // Legge la global var CURRENT_AN_ON e la utilizza per leggere un valore dall'array mappato da stringSevSeg (in ordine inverso)


void IntToString (int val, char* dst); // Funzione aggiuntiva: permette di convertire un intero in un array di caratteri col corretto mapping

u8 lastAN_ON_string (int lastAN_indx); // Funzione che crea la stringa dell'AN on

/* Registers  */

volatile int* AN_7SEG_Output = (int*)XPAR_AXI_7SEGSAN_GPIO_BASEADDR; // Registro Anodi 7Seg
volatile int* DIGIT_7SEG_Output = (int*)XPAR_AXI_7SEGS_GPIO_BASEADDR; // Registro Catodi 7Seg

/* Interrupt Service Routine*/

void timerISR (void) __attribute__ ((interrupt_handler));


/* GLOBAL VARIABLES */


char* stringSevSeg; // Puntatore che permette il mapping tra main e ISR per la stringa da stampare
int* CURRENT_AN_ON; // Puntatore ad un contatore globale che tiene traccia dell'anodo acceso.
int* LAST_ANODE_INDX; // Counter per displaySingleVal
u8* MOST_LEFT_ANODE_ON; // ptr a stringa di comparazione per anodeShift



int main()
{
    init_platform();

    // Init 7SEG

    // Mi assicuro di dedicare un'area di memoria ai counter e  ai puntatori

    int counter;

    CURRENT_AN_ON = &counter;

    int local_LAST_ANODE_INDX;

    LAST_ANODE_INDX = &local_LAST_ANODE_INDX;


    // Init 7Seg


	*AN_7SEG_Output=0xFE;
	*CURRENT_AN_ON = 0;


	int counterValue = 250000;


    /* SETTING INTERRUPTS AND TIMER */


    microblaze_enable_interrupts(); // Attiva gli interrupt nel processore

    timerInit(counterValue); //Inizializzo il counter a 20000 per avere 50Hz




    // MAIN PROCEDURE


	//Uncomment to use static assignation

//    char string[]= {'1','2','3','6','7','8'};

	//Uncomment to use intToString

    char string[8];

    stringSevSeg = string; // Mappo le due aree di memoria



    // Nota: Se si inizializza una stringa da stampare di dimensione inferiore a 8, allora il programma calcoler� in modo autonomo la dimensione degli anodi utili per accendere o spegnere il display.
    // Es. se si usa char string[] = {'1','2'}; verranno accesi solamente gli an[0] e an[1].



    *LAST_ANODE_INDX = (sizeof(string)/sizeof(string[0]))-1; // -1 perch� parte da 0
    *MOST_LEFT_ANODE_ON = lastAN_ON_string(*LAST_ANODE_INDX); // Calcola la stringa di comparazione per l'ultimo an acceso



    int i = 0;

    while (1) {
    	// Background



    	// Funzione che permette di creare un counter e caricare il valore sull'array che verr� visualizzato
    	// Comment to use the static example


    	i++;
    	IntToString(i/10000,string);


    }




    cleanup_platform();
    return 0;
}



void write_digit (u8 digit, u8 dotted) { // Scrive sul 7SEG la digit (combinazione di catodi da accendere)

	*DIGIT_7SEG_Output = digit | (dotted & CDP);

}





u8 sevseg_digitMapping(char c) { // mappa il carattere agli catodi da accendere
	u8 cathodes_output = 0;

	switch (c) {

	case '0':
		cathodes_output = 0x3F;
		break;
	case '1':
		cathodes_output = 0x06;
		break;
	case '2':
		cathodes_output = 0x5B;
		break;

	case '3':
		cathodes_output = 0x4F;
		break;


	case '4':
		cathodes_output = 0x66;
		break;


	case '5':
		cathodes_output = 0x6D;
		break;


	case '6':
		cathodes_output = 0x7D;
		break;

	case '7':
		cathodes_output = 0x07;
		break;

	case '8':
		cathodes_output = 0x7F;
		break;

	case '9':
		cathodes_output = 0x6F;
		break;

	case 'A':
		cathodes_output = 0x77;
		break;

	case 'B':
		cathodes_output = 0x7C;
		break;

	case 'C':
		cathodes_output = 0x39;
		break;

	case 'D':
		cathodes_output = 0x5E;
		break;

	case 'E':
		cathodes_output = 0x79;
		break;

	case 'F':
		cathodes_output = 0x71;
		break;

	default:
		cathodes_output = 0;
	}


	return ~(cathodes_output); // Accendi i segmenti corrispondenti

}

void timerInit(int valueCounter) { // Wrapping per inizializzazione timer
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

	// Il timer inizia ora

    XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, TmrCtrNumber); // Abilito il timer

}

char int_to_char(int val) { // Converte l'intero ad un carattere
	if (val < 0 || val > 9) return 0x7E; // val non e' un numero

	return val+48; // 48 per trasformare in un numero


}


void timerISR (void) { // Interrupt Service Routine

	int interrupt_source = *(int*)XPAR_AXI_INTC_0_BASEADDR;


	if (interrupt_source & TIMER_INT_SRC) { // inutile in quanto l'interrupt non pu� arrivare da altre fonti

		anodeShift(); // Accende il successivo anodo

		displaySingleVal(); // Mostra il corrispettivo valore


		timer0_IntAck(); // Fa l'ack del timer

		}
}


void anodeShift (void) { // Cambia l'anodo acceso aggiornando la variabile globale CURRENT_AN_ON
	if (*AN_7SEG_Output == (*MOST_LEFT_ANODE_ON)) {
		*AN_7SEG_Output = ~1; // Sposta lo 0 a destra
	*CURRENT_AN_ON = 0;

	}
	else { *AN_7SEG_Output = (*AN_7SEG_Output<<1) | 1; // Sposta a sinistra lo 0 aggiungendo un 1
	(*CURRENT_AN_ON)++;
	}
}


void timer0_IntAck(void) { // Wrapping del timer0 ack
	/* Timer Acknowledgment*/

	*(int*)XPAR_AXI_TIMER_0_BASEADDR = (1<<8) | (*(int*)XPAR_AXI_TIMER_0_BASEADDR); // OR tra l'ottavo bit a 1 e lascio invariati gli altri bit


	// IAR [INT 2]

	*(int*)(XPAR_AXI_INTC_0_BASEADDR + 0x0C) = 0b100;

}


void displaySingleVal (void)  { // Scrive il valore sui 7seg in base all'anodo acceso al momento

	int i = *CURRENT_AN_ON; // In base all'anodo acceso, scrivo il valore
	write_digit(sevseg_digitMapping(*(stringSevSeg+(*LAST_ANODE_INDX)-i)), 0); // Legge l'array partendo dalla fine (l'an[0] � quello pi� a destra)
	i++;


}




void IntToString (int val, char* dst) { // OPZIONALE: Trasforma un intero in una stringa (dimensione fissa)

	/* Note:
	 * E' possibile creare una funzione che alloca dinamicamente la stringa in base al valore da convertire, tuttavia non � conveniente per 2 motivi.
	 * 1. Non � utilizzabile in un ciclo, in quanto non si preoccupa di deallocare la memoria, portando in fretta ad un esaurimento di RAM.
	 * 2. Non � caricabile senza aumentare l'heap, in quanto fa uso dell'allocazione dinamica.
	 * La soluzione sarebbe caricare la stringa in una struct che ne indica la dimensione corrente, avendo un dimensionamento fittizio (lo spazio usato rimane tale).
	 * La scelta implementativa (pi� semplice ed efficace) � stata quella di mappare gli zeri di padding a sinistra della cifra a 'n',
	 * che non viene riconosciuto da un mapping, spegnendo i catodi e ottenendo l'effetto desiderato.
	 * Il calcolo dinamico della dimensione persiste nel caso di stringhe definite staticamente dal main.
	 */

	int flag = 0;
	char local_val;
	if (val > 99999999) { // Printa tutto E per dire error (se si cicla continuando a sommare prima o poi va in overflow)
		for (int i = 7; i>=0; i--) {
		dst[i] = 'E';

		}
		return;
	}
	for (int i = 7; i>=0; i--) { // Converte ciascuna cifra alla posizione corrispondente
		(local_val) = int_to_char(val % 10);
		if (!val && i!=7) flag = 1; // indica che sono finite le cifre !=0 (rimangono gli zero di padding a sinistra)

		val = val / 10;

		if (flag) dst[i] = 'n'; // Carattere fittizio (spegne i catodi nel mapping)
		else dst[i] = local_val;


	}
}


u8 lastAN_ON_string (int lastAN_indx) { // Utilizzata per creare la stringa di comparazione per anodeShift
	return ~(1<<lastAN_indx);
}
