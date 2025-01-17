#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"

// Definitions
#define gpioIrBaseAddr XPAR_GPIO_0_BASEADDR // Base address of GPIO
#define timerDeviceId XPAR_TMRCTR_0_DEVICE_ID

// Function Prototypes
void decodeNecProtocol(uint32_t *packet);
uint32_t measurePulseDuration(void);
uint32_t readGpioPin(void);

// Global Variables
XTmrCtr timerInstance;

int main() {
    init_platform();

    // Initialize Timer
    XTmrCtr_Initialize(&timerInstance, timerDeviceId);
    XTmrCtr_SetResetValue(&timerInstance, 0, 0x0);
    XTmrCtr_Start(&timerInstance, 0);

    uint32_t necPacket = 0;

    while (1) {
        // Decode NEC protocol
        decodeNecProtocol(&necPacket);

        // Display Address and Command
        uint8_t address = (necPacket >> 24) & 0xFF;
        uint8_t command = (necPacket >> 16) & 0xFF;
        xil_printf("Address: 0x%02X, Command: 0x%02X\n", address, command);
    }

    cleanup_platform();
    return 0;
}

void decodeNecProtocol(uint32_t *packet) {
    uint32_t timing;
    uint32_t necData = 0;

    // Wait for start pulse (9 ms high, 4.5 ms low)
    timing = measurePulseDuration(); // High duration
    if (timing < 8500 || timing > 9500) {
        return; // Invalid start pulse
    }

    timing = measurePulseDuration(); // Low duration
    if (timing < 4000 || timing > 5000) {
        return; // Invalid gap
    }

    // Decode 32 bits
    for (int i = 0; i < 32; i++) {
        timing = measurePulseDuration(); // High duration (560 μs)
        if (timing < 500 || timing > 700) {
            return; // Invalid pulse for bit
        }

        timing = measurePulseDuration(); // Low duration (logical 0 or 1)
        if (timing >= 1500 && timing <= 1800) {
            necData = (necData << 1) | 1; // Logical 1
        } else if (timing >= 500 && timing <= 700) {
            necData = (necData << 1); // Logical 0
        } else {
            return; // Invalid gap
        }
    }

    // Validate data (check inverse address and command)
    uint8_t address = (necData >> 24) & 0xFF;
    uint8_t invAddress = (necData >> 16) & 0xFF;
    uint8_t command = (necData >> 8) & 0xFF;
    uint8_t invCommand = necData & 0xFF;

    if ((address ^ invAddress) != 0xFF || (command ^ invCommand) != 0xFF) {
        return; // Data integrity check failed
    }

    *packet = necData; // Store the decoded packet
}

uint32_t measurePulseDuration(void) {
    uint32_t startTime, endTime, pulseDuration;

    // Wait for signal edge
    while (readGpioPin() == 0);
    startTime = XTmrCtr_GetValue(&timerInstance, 0);

    while (readGpioPin() == 1);
    endTime = XTmrCtr_GetValue(&timerInstance, 0);

    // Calculate pulse duration
    pulseDuration = endTime - startTime;
    return pulseDuration / (XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ / 1000000); // Convert to microseconds
}

uint32_t readGpioPin(void) {
    // Read the GPIO input pin (bit 0 of GPIO base address)
    return *((volatile uint32_t *)gpioIrBaseAddr) & 0x1;
}
