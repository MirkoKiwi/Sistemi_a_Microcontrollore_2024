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
void PrintSequence(unsigned long long data, int durations[]);
void DecodeAndPrintNECData(unsigned long long data);

// Variabili per la decodifica
static int previous_state = 0;
static int signal_duration = 0;
static int bit_count = 0;
static unsigned long long data = 0;
static int capturing = 0;
static int bit_durations[32] = {0};
static int start_sequence = 0;

int main() {
    init_platform();

    xil_printf("Hello World!\n");

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

        if (!capturing && signal_duration > US_TO_TICKS(9000) && signal_duration < US_TO_TICKS(9500)) {
            // Impulso di start rilevato (9 ms)
            start_sequence++;
        }
        else if (start_sequence == 1 && signal_duration > US_TO_TICKS(4500) && signal_duration < US_TO_TICKS(5000)) {
            // Secondo impulso di start rilevato (4.5 ms)
            capturing = 1;
            bit_count = 0;
            data = 0;
            start_sequence = 0;
        }
        else {
            start_sequence = 0;
        }

        if (capturing) {
            DecodeNECProtocol(signal_duration);
        }

        previous_state = current_state;
        last_timer_value = current_timer_value;
    }
}

// Funzione per decodificare il protocollo NEC
void DecodeNECProtocol(int signal_duration) {
    if (bit_count < 32) {
        if (signal_duration > US_TO_TICKS(1600)) {
            // "1" logico
            data = (data << 1) | 1;
        } else {
            // "0" logico
            data = (data << 1);
        }
        bit_durations[bit_count] = signal_duration;
        bit_count++;
    }

    if (capturing && bit_count == 32) {
        // Sequenza completa
        PrintSequence(data, bit_durations);
        DecodeAndPrintNECData(data);

        // Reset delle variabili
        capturing = 0;
        bit_count = 0;
        data = 0;
    }
}

// Funzione per stampare la sequenza
void PrintSequence(unsigned long long data, int durations[]) {
    xil_printf("Bit | Numero | Durata (us)\n");
    for (int i = 31; i >= 0; i--) {
        int bit = (data >> i) & 1;
        xil_printf("%2d  | Bit %2d | %10d\n", bit, 32 - i, durations[31 - i] / (TIMER_CLOCK_FREQ_HZ / 1000000));
    }
}

// Funzione per decodificare e stampare i dati NEC
void DecodeAndPrintNECData(unsigned long long data) {
    unsigned char address = (data >> 24) & 0xFF;
    unsigned char address_inv = (data >> 16) & 0xFF;
    unsigned char command = (data >> 8) & 0xFF;
    unsigned char command_inv = data & 0xFF;

    xil_printf("Dati Decodificati:\n");
    xil_printf("Indirizzo: 0x%02X\n", address);
    xil_printf("Indirizzo Inverso: 0x%02X\n", !address_inv);
    xil_printf("Comando: 0x%02X\n", command);
    xil_printf("Comando Inverso: 0x%02X\n", !command_inv);

    if ((address ^ address_inv) == 0xFF && (command ^ command_inv) == 0xFF) {
        xil_printf("Validazione: OK\n");
    } else {
        xil_printf("Validazione: ERRORE\n");
    }
}
