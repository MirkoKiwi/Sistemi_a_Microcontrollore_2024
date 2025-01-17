#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"

// Macro per il timer
#define TIMER_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_COUNTER_0        0
#define TIMER_CLOCK_FREQ_HZ    100000000 // Frequenza del timer (100 MHz)
#define US_TO_TICKS(us)        ((us) * (TIMER_CLOCK_FREQ_HZ / 1000000)) // Conversione da microsecondi a tick

// Dichiarazione del GPIO come puntatore volatile
volatile int* AXI_GPIO_IR = (int*)XPAR_GPIO_IR_BASEADDR;

// Timer
XTmrCtr TimerInstance;

// Prototipi delle funzioni
void CaptureIRSignal();

// Variabili di debug
static int previous_state = 0;
static int signal_duration = 0;

int main() {
    init_platform();

    xil_printf("Debug: Cattura segnale IR\n");

    // Inizializza il timer
    if (XTmrCtr_Initialize(&TimerInstance, TIMER_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("Errore inizializzazione timer\n");
        return -1;
    }

    // Configura il timer senza auto-reload
    XTmrCtr_SetOptions(&TimerInstance, TIMER_COUNTER_0, XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_Reset(&TimerInstance, TIMER_COUNTER_0);
    XTmrCtr_Start(&TimerInstance, TIMER_COUNTER_0);

    while (1) {
        CaptureIRSignal();
    }

    cleanup_platform();
    return 0;
}

// Funzione per catturare il segnale IR
void CaptureIRSignal() {
    static int last_timer_value = 0;

    int current_state = *AXI_GPIO_IR;  // Legge lo stato del GPIO
    int current_timer_value = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);  // Legge il valore del timer

    if (current_state != previous_state) {
        // Cambiamento di stato rilevato
        if (current_timer_value >= last_timer_value) {
            signal_duration = current_timer_value - last_timer_value;
        } else {
            signal_duration = (0xFFFFFFFF - last_timer_value) + current_timer_value + 1;
        }

        // Conversione della durata in microsecondi
        int duration_us = signal_duration / (TIMER_CLOCK_FREQ_HZ / 1000000);

        xil_printf("Stato: %d -> %d, Durata: %d us\n", previous_state, current_state, duration_us);

        // Aggiorna lo stato e il timer
        previous_state = current_state;
        last_timer_value = current_timer_value;
    }
}
