#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"

// Definitions
#define gpioIrBaseAddr XPAR_GPIO_0_BASEADDR // Base address of GPIO
#define samplingInterval 50                // Sampling interval in microseconds
#define startHighCountThreshold 180        // ~9 ms / 50 µs
#define startLowCountThreshold 90          // ~4.5 ms / 50 µs
#define oneLowCountThreshold 34            // ~1.69 ms / 50 µs
#define zeroLowCountThreshold 11           // ~560 µs / 50 µs

// Function Prototypes
void decodeNecProtocol(uint32_t *packet);
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

// NEC Protocol Decoder
void decodeNecProtocol(uint32_t *packet) {
    uint32_t highCount = 0, lowCount = 0;
    uint32_t necData = 0;
    int bitIndex = 0;
    uint32_t state = readGpioPin();

    xil_printf("Waiting for Start Pulse...\n");

    // Wait for the start pulse
    while (state == 0) {
        state = readGpioPin(); // Wait for high signal
        delayMicroseconds(samplingInterval);
    }

    // Count the high duration
    while (state == 1) {
        highCount++;
        state = readGpioPin();
        delayMicroseconds(samplingInterval);
    }

    // Validate the high duration of the start pulse
    if (highCount < startHighCountThreshold) {
        xil_printf("Invalid Start Pulse High\n");
        return;
    }

    // Count the low duration
    while (state == 0) {
        lowCount++;
        state = readGpioPin();
        delayMicroseconds(samplingInterval);
    }

    // Validate the low duration of the start pulse
    if (lowCount < startLowCountThreshold) {
        xil_printf("Invalid Start Pulse Low\n");
        return;
    }

    xil_printf("Decoding 32-bit Packet...\n");

    // Decode 32 bits
    while (bitIndex < 32) {
        // Measure high duration
        highCount = 0;
        while (state == 1) {
            highCount++;
            state = readGpioPin();
            delayMicroseconds(samplingInterval);
        }

        // High duration validation (should be ~560 µs)
        if (highCount < 10 || highCount > 14) { // Adjust thresholds as needed
            xil_printf("Invalid High Pulse for Bit %d\n", bitIndex);
            return;
        }

        // Measure low duration
        lowCount = 0;
        while (state == 0) {
            lowCount++;
            state = readGpioPin();
            delayMicroseconds(samplingInterval);
        }

        // Decode bit based on low duration
        if (lowCount >= oneLowCountThreshold) {
            necData = (necData << 1) | 1; // Logical 1
            xil_printf("Bit %d: 1\n", bitIndex);
        } else if (lowCount >= zeroLowCountThreshold) {
            necData = (necData << 1); // Logical 0
            xil_printf("Bit %d: 0\n", bitIndex);
        } else {
            xil_printf("Invalid Low Pulse for Bit %d\n", bitIndex);
            return;
        }

        bitIndex++;
    }

    xil_printf("Decoded Packet: 0x%08X\n", necData);

    // Validate data
    uint8_t address = (necData >> 24) & 0xFF;
    uint8_t invAddress = (necData >> 16) & 0xFF;
    uint8_t command = (necData >> 8) & 0xFF;
    uint8_t invCommand = necData & 0xFF;

    if ((address ^ invAddress) != 0xFF || (command ^ invCommand) != 0xFF) {
        xil_printf("Data Integrity Check Failed\n");
        return;
    }

    xil_printf("Address: 0x%02X, Command: 0x%02X\n", address, command);
    *packet = necData;
}

// Main Function
int main() {
    init_platform();

    uint32_t necPacket = 0;

    while (1) {
        decodeNecProtocol(&necPacket);
        delayMicroseconds(50000); // Delay before next decoding
    }

    cleanup_platform();
    return 0;
}
