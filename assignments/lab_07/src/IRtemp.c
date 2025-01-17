#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"

// Definizioni
#define gpioIrBaseAddr XPAR_GPIO_0_BASEADDR // Base address GPIO
#define samplingInterval 50                // Intervallo di campionamento (50 µs)
#define maxSamples 64                      // Numero massimo di campioni
#define START_PULSE_MIN 8500               // Minimo inizio impulso (ms)
#define START_PULSE_MAX 11500              // Massimo inizio impulso (ms)

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
    uint32_t times[maxSamples];       // Array per salvare i tempi
    uint32_t currentState, previousState = readGpioPin();
    int timeIndex = 0;
    uint32_t startTime = 0, currentTime;
    int startPulseDetected = 0;

    xil_printf("Inizio acquisizione segnali...\n");

    // Acquisizione dei segnali
    while (timeIndex < maxSamples) {
        currentState = readGpioPin();
        if (currentState != previousState) {
            currentTime = startTime;
            times[timeIndex++] = currentTime;
            xil_printf("Cambio stato: %d, Tempo: %d\n", currentState, currentTime);  // Debug: Visualizzazione dei tempi
            previousState = currentState;
        }
        delayMicroseconds(samplingInterval);
        startTime += samplingInterval;
    }

    xil_printf("Acquisizione terminata, inizio decodifica...\n");

    // Verifica del pulse di avvio (9 ms)
    uint32_t startPulseDuration = times[1] - times[0];
    if (startPulseDuration < START_PULSE_MIN || startPulseDuration > START_PULSE_MAX) {
        xil_printf("Errore: il pulse di inizio non è valido.\n");
        return;
    }

    // Decodifica del pacchetto
    uint32_t necPacket = 0;
    for (int i = 0; i < 32; i++) {  // Contiamo solo i 32 bit del pacchetto NEC
        uint32_t pulseDuration = times[2 * i + 1] - times[2 * i]; // Differenza di tempo tra alto e basso

        xil_printf("Bit %d - Durata impulso: %lu us\n", i, pulseDuration); // Debug: Mostra la durata

        // Se la durata è maggiore di una certa soglia, consideriamo 1, altrimenti 0
        if (pulseDuration > 1000) { // Imposto una soglia arbitraria per distinguere 1 da 0
            necPacket = (necPacket << 1) | 1; // Bit 1
        } else {
            necPacket = (necPacket << 1); // Bit 0
        }
    }

    xil_printf("Pacchetto NEC decodificato: 0x%08X\n", necPacket);

    // Estrazione dell'indirizzo inverso (terzo ottetto)
    uint8_t invertedAddress = (necPacket >> 8) & 0xFF;

    xil_printf("Indirizzo inverso: 0x%02X\n", invertedAddress);

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
