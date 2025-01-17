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
void DecodeNECProtocol(int signal_duration);
void DecodeAndPrintNECData(unsigned long long data);

// Variabili globali
static int previous_state = 0;
static int signal_duration = 0;
static int capturing = 0;
static int bit_count = 0;
static unsigned long long data = 0;

int main() {
    init_platform();

    xil_printf("Decodifica segnale NEC IR\n");

    // Inizializza il timer
    if (XTmrCtr_Initialize(&TimerInstance, TIMER_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("Errore inizializzazione timer\n");
        return -1;
    }

    // Configura il timer senza auto-reload
    XTmrCtr_SetOptions(&TimerInstance, TIMER_COUNTER_0, 0);
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

    int current_state = *AXI_GPIO_IR;
    int current_timer_value = XTmrCtr_GetValue(&TimerInstance, TIMER_COUNTER_0);

    if (current_state != previous_state) {
        // Calcola la durata basandoti sulla differenza
        if (current_timer_value >= last_timer_value) {
            signal_duration = current_timer_value - last_timer_value;
        } else {
            signal_duration = (0xFFFFFFFF - last_timer_value) + current_timer_value + 1;
        }

        // Conversione in microsecondi
        int duration_us = signal_duration / (TIMER_CLOCK_FREQ_HZ / 1000000);

        // Rilevamento sequenza di start
        if (!capturing && duration_us > 8500 && duration_us < 9500) {
            // Sequenza di start (9 ms)
            capturing = 1;
            bit_count = 0;
            data = 0;
            xil_printf("Inizio cattura dati NEC\n");
        } else if (capturing) {
            DecodeNECProtocol(signal_duration);
        }

        previous_state = current_state;
        last_timer_value = current_timer_value;
    }
}

// Funzione per decodificare i bit del protocollo NEC
void DecodeNECProtocol(int signal_duration) {
    // Conversione in microsecondi
    int duration_us = signal_duration / (TIMER_CLOCK_FREQ_HZ / 1000000);

    if (duration_us > 500 && duration_us < 700) {
        // Ignora impulsi di 560 µs (parte del bit)
        return;
    } else if (duration_us > 1500 && duration_us < 1700) {
        // Bit "1"
        data = (data << 1) | 1;
    } else if (duration_us > 400 && duration_us < 700) {
        // Bit "0"
        data = (data << 1);
    } else {
        // Sequenza interrotta o fine non valida
        capturing = 0;
        xil_printf("Errore durante la cattura del segnale\n");
        return;
    }

    bit_count++;

    if (bit_count == 32) {
        // Sequenza completa di 32 bit
        xil_printf("Dati ricevuti: 0x%08llX\n", data);
        DecodeAndPrintNECData(data);

        // Reset
        capturing = 0;
        bit_count = 0;
        data = 0;
    }
}

// Funzione per decodificare e stampare i dati NEC
void DecodeAndPrintNECData(unsigned long long data) {
    unsigned char address = (data >> 24) & 0xFF;
    unsigned char address_inv = (data >> 16) & 0xFF;
    unsigned char command = (data >> 8) & 0xFF;
    unsigned char command_inv = data & 0xFF;

    xil_printf("Decodifica completata:\n");
    xil_printf("  Indirizzo: 0x%02X\n", address);
    xil_printf("  Indirizzo Inverso: 0x%02X\n", address_inv);
    xil_printf("  Comando: 0x%02X\n", command);
    xil_printf("  Comando Inverso: 0x%02X\n", command_inv);

    // Validazione
    if ((address ^ address_inv) == 0xFF && (command ^ command_inv) == 0xFF) {
        xil_printf("  Validazione: OK\n");
    } else {
        xil_printf("  Validazione: ERRORE\n");
    }
}
