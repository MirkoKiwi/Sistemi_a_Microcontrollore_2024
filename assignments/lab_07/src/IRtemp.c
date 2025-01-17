#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"

// Definizioni
#define gpioIrBaseAddr XPAR_GPIO_0_BASEADDR // Base address GPIO
#define samplingInterval 50                // Intervallo di campionamento (50 µs)
#define maxSamples 64                      // Numero massimo di campioni

// Funzioni
void decodeNecProtocol(void);
uint32_t readGpioPin(void);
void delayMicroseconds(uint32_t microseconds);

// Helper Functions
void delayMicroseconds(uint32_t microseconds) {
    volatile uint32_t count = (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 1000000) * microseconds / 5;
    while (count--) {
        asm("nop");
    }
}

uint32_t readGpioPin(void) {
    return *((volatile uint32_t *)gpioIrBaseAddr) & 0x1;
}

// Decodifica NEC
void decodeNecProtocol(void) {
    uint32_t times[maxSamples];
    uint32_t currentState, previousState = readGpioPin();
    int timeIndex = 0;
    uint32_t startTime = 0, currentTime;

    // Acquisizione segnali
    xil_printf("Acquisizione segnali in corso...\n");
    while (timeIndex < maxSamples) {
        currentState = readGpioPin();
        if (currentState != previousState) {
            currentTime = startTime;
            times[timeIndex++] = currentTime;
            previousState = currentState;
        }
        delayMicroseconds(samplingInterval);
        startTime += samplingInterval;
    }

    // Decodifica dati
    xil_printf("Decodifica pacchetto...\n");
    uint32_t necPacket = 0;
    for (int i = 0; i < 32; i++) {
        uint32_t pulseDuration = times[2 * i + 1] - times[2 * i]; // Differenza di tempi
        if (pulseDuration > 1000) { // Threshold per distinguere 1 e 0
            necPacket = (necPacket << 1) | 1; // Bit 1
        } else {
            necPacket = (necPacket << 1); // Bit 0
        }
    }

    // Estrazione dell'indirizzo
    uint8_t invertedAddress = (necPacket >> 8) & 0xFF;

    // Switch dei comandi
    xil_printf("Comando ricevuto: ");
    switch (invertedAddress) {
        case 0xFE:
            xil_printf("Comando 1\n");
            break;
        case 0xFD:
            xil_printf("Comando 2\n");
            break;
        case 0xFB:
            xil_printf("Comando 3\n");
            break;
        default:
            xil_printf("Comando sconosciuto\n");
            break;
    }
}

// Funzione principale
int main() {
    init_platform();

    while (1) {
        decodeNecProtocol();
        delayMicroseconds(50000); // Attesa prima di una nuova decodifica
    }

    cleanup_platform();
    return 0;
}
