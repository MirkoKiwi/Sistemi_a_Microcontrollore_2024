#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"

// Definitions
#define gpioIRBaseAddr XPAR_GPIO_0_BASEADDR // Base address of GPIO
#define timerDeviceId XPAR_TMRCTR_0_DEVICE_ID


#define US_TO_TICKS(us)        ((us) * (TIMER_CLOCK_FREQ_HZ / 1000000))

// Function Prototypes
void decodeNecProtocol(u32 *packet);
u32 measurePulseDuration(void);
u32 readGPIOPin(void);

// Global Variables
XTmrCtr timerInstance;

int main() {
    init_platform();

    // Initialize Timer
    XTmrCtr_Initialize(&timerInstance, timerDeviceId);
    XTmrCtr_SetResetValue(&timerInstance, 0, 0x0);
    XTmrCtr_Start(&timerInstance, 0);

    u32 necPacket = 0;

    while (1) {
        // Decode NEC protocol
        decodeNecProtocol(&necPacket);

        // Display Address and Command
        u8 address = ( necPacket >> 24 ) & 0xFF;
        u8 command = ( necPacket >> 16 ) & 0xFF;
        xil_printf("Address: 0x%02X, Command: 0x%02X\n", address, command);
    }

    cleanup_platform();
    return 0;
}

void decodeNecProtocol(u32 *packet) {
    u32 timing;
    u32 necData = 0;

    xil_printf("Waiting for Start Pulse...\n");

    // Wait for start pulse
    timing = measurePulseDuration();
    xil_printf("Start Pulse High: %lu us\n", timing);
    if ( timing < US_TO_TICKS(8000) || timing > US_TO_TICKS(10000) ) {
        xil_printf("Invalid Start Pulse High\n");
        return;
    }

    timing = measurePulseDuration();
    xil_printf("Start Pulse Low: %lu us\n", timing);
    if ( timing < US_TO_TICKS(3500) || timing > US_TO_TICKS(5500) ) {
        xil_printf("Invalid Start Pulse Low\n");
        return;
    }

    xil_printf("Decoding 32-bit Packet...\n");

    // Decode 32 bits
    for ( int i = 0; i < 32; i++ ) {
        timing = measurePulseDuration();
        if ( timing < US_TO_TICKS(500) || timing > US_TO_TICKS(700) ) {
            xil_printf("Invalid Pulse for Bit %d\n", i);
            return;
        }

        timing = measurePulseDuration();
        if ( timing >= US_TO_TICKS(1400) && timing <= US_TO_TICKS(1900) ) {
            necData = (necData << 1) | 1;
            xil_printf("Bit %d: 1\n", i);
        } 
        else if ( timing >= US_TO_TICKS(400) && timing <= US_TO_TICKS(800) ) {
            necData = (necData << 1);
            xil_printf("Bit %d: 0\n", i);
        } 
        else {
            xil_printf("Invalid Gap for Bit %d\n", i);
            return;
        }
    }

    xil_printf("Decoded Packet: 0x%08X\n", necData);

    u8 address = (necData >> 24) & 0xFF;
    u8 invAddress = (necData >> 16) & 0xFF;
    u8 command = (necData >> 8) & 0xFF;
    u8 invCommand = necData & 0xFF;

    if ( ( address ^ invAddress ) != 0xFF || ( command ^ invCommand ) != 0xFF ) {
        xil_printf("Data Integrity Check Failed\n");
        return;
    }

    xil_printf("Address: 0x%02X, Command: 0x%02X\n", address, command);
    *packet = necData;
}

/*
void decodeNecProtocol(u32 *packet) {
    u32 timing;
    u32 necData = 0;

    // Wait for start pulse (9 ms high, 4.5 ms low)
    timing = measurePulseDuration(); // High duration
    if ( timing < 8500 || timing > 9500 ) {
        return; // Invalid start pulse
    }

    timing = measurePulseDuration(); // Low duration
    if ( timing < 4000 || timing > 5000 ) {
        return; // Invalid gap
    }

    // Decode 32 bits
    for ( int i = 0; i < 32; i++ ) {
        timing = measurePulseDuration(); // High duration (560 μs)
        if ( timing < 500 || timing > 700 ) {
            return; // Invalid pulse for bit
        }

        timing = measurePulseDuration(); // Low duration (logical 0 or 1)
        if ( timing >= 1500 && timing <= 1800 ) {
            necData = (necData << 1) | 1; // Logical 1
        } 
        else if ( timing >= 500 && timing <= 700 ) {
            necData = (necData << 1);   // Logical 0
        } 
        else {
            return; // Invalid gap
        }
    }

    // Validate data (check inverse address and command)
    u8 address =    ( necData >> 24 ) & 0xFF;
    u8 invAddress = ( necData >> 16 ) & 0xFF;
    u8 command =    ( necData >> 8 ) & 0xFF;
    u8 invCommand = necData & 0xFF;

    if ( ( address ^ invAddress ) != 0xFF || ( command ^ invCommand ) != 0xFF ) {
        return; // Data integrity check failed
    }

    *packet = necData; // Store the decoded packet
}
*/

u32 measurePulseDuration(void) {
    u32 startTime, endTime, pulseDuration;

    // Wait for signal edge
    while ( readGPIOPin() == 0 );
    startTime = XTmrCtr_GetValue(&timerInstance, 0);

    while ( readGPIOPin() == 1 );
    endTime = XTmrCtr_GetValue(&timerInstance, 0);

    // Calculate pulse duration
    pulseDuration = endTime - startTime;
    return pulseDuration / ( XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ / 1000000 ); // Convert to microseconds
}

u32 readGPIOPin(void) {
    // Read the GPIO input pin (bit 0 of GPIO base address)
    return *((volatile u32 *)gpioIRBaseAddr) & 0x1;
}

