#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"

// Definizioni
#define gpioIrBaseAddr XPAR_GPIO_0_BASEADDR // Base address GPIO
#define samplingInterval 50                 // Intervallo di campionamento (50 µs)
#define maxSamples 64                       // Numero massimo di campioni
#define START_PULSE_MIN 8500                // Minimo inizio impulso (9 ms)
#define START_PULSE_MAX 11500               // Massimo inizio impulso (11.5 ms)

// Dichiarazione del GPIO come puntatore volatile
volatile int* AXI_GPIO_IR = (int*)XPAR_GPIO_IR_BASEADDR;

// Timer
XTmrCtr TimerInstance;

// Prototipi delle funzioni
void decodeNecProtocol(void);
uint32_t readGpioPin(void);
void delayMicroseconds(uint32_t microseconds);
void CaptureIRSignal();
void DecodeNECProtocol(int signal_duration);
void DecodeAndPrintNECData(unsigned long long data);

// Variabili globali
static int previous_state = 0;
static int signal_duration = 0;
static int capturing = 0;
static int bit_count = 0;
static unsigned long long data = 0;

// Helper Functions
void delayMicroseconds(uint32conds) {
    volatile uint32_t count = (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 1000000) * microseconds / 5;
    while (count--) {
        asm("nop");
    }
}

uint32_t readGpioPin(void) {
    return *((volatile uint32_t *)gpioIrBaseAddr) & 0x1;
}

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
        xil_printf("Stato: %d -> %d, Durata: %d us\n", previous_state, current_state, duration_us);

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

void DecodeNECProtocol(int signal_duration) {
    int duration_us = signal_duration / (TIMER_CLOCK_FREQ_HZ / 1000000);

    if (duration_us > 500 && duration_us < 700) {
        return;
    } else if (duration_us > 1500 && duration_us < 1700) {
        data = (data << 1) | 1;
    } else if (duration_us > 400 && duration_us < 700) {
        data = (data << 1);
    } else {
        capturing = 0;
        xil_printf("Errore durante la cattura del segnale\n");
        return;
    }

    bit_count++;
    if (bit_count == 32) {
        xil_printf("Dati ricevuti: 0x%08llX\n", data);
        DecodeAndPrintNECData(data);

        capturing = 0;
        bit_count = 0;
        data = 0;
    }
}

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

    if ((address ^ address_inv) == 0xFF && (command ^ command_inv) == 0xFF) {
        xil_printf("  Validazione: OK\n");
    } else {
        xil_printf("  Validazione: ERRORE\n");
    }
}

// Main Function
int main() {
    init_platform();
    xil_printf("Decodifica segnale NEC IR\n");

    if (XTmrCtr_Initialize(&TimerInstance, TIMER_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("Errore inizializzazione timer\n");
        return -1;
    }

    XTmrCtr_SetOptions(&TimerInstance, TIMER_COUNTER_0, 0);
    XTmrCtr_Reset(&TimerInstance, TIMER_COUNTER_0);
    XTmrCtr_Start(&TimerInstance, TIMER_COUNTER_0);

    while (1) {
        CaptureIRSignal();
        delayMicroseconds(50000);
    }

    cleanup_platform();
    return 0;
}
