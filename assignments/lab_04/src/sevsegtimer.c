#include <stdint.h>
#include <stdbool.h>
#include "xparameters.h"  // System parameters
#include "xgpio.h"       // GPIO driver
#include "xtmrctr.h"     // Timer driver
#include "xil_exception.h"

#define GPIO_DEVICE_ID XPAR_GPIO_0_DEVICE_ID
#define TIMER_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define TIMER_COUNTER 0

// GPIO instance
XGpio Gpio;
// Timer instance
XTmrCtr Timer;

// Segment patterns for digits 0-9 (common anode, active LOW)
uint8_t segment_map[10] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

// Array to store the digits to be displayed
uint8_t digits[8] = {1, 2, 3, 4, 5, 6, 7, 8};

// Variables for multiplexing
volatile int current_digit = 0;

// Function prototypes
void write_digit(uint8_t digit, bool dotted);
void TimerInterruptHandler(void *CallBackRef);
void initialize_display();
void initialize_timer();

int main() {
    int status;

    // Initialize GPIO
    status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // Set GPIO direction: output
    XGpio_SetDataDirection(&Gpio, 1, 0x00); // Channel 1, all output

    // Initialize display and timer
    initialize_display();
    initialize_timer();

    // Enable interrupts
    Xil_ExceptionEnable();

    // Main loop (nothing to do here, handled by interrupt)
    while (1) {}

    return 0;
}

void write_digit(uint8_t digit, bool dotted) {
    // Get segment pattern for the digit
    uint8_t pattern = segment_map[digit];
    if (dotted) {
        pattern |= 0b10000000; // Add decimal point
    }

    // Write pattern to GPIO (assumes channel 1 is for segments)
    XGpio_DiscreteWrite(&Gpio, 1, pattern);
}

void TimerInterruptHandler(void *CallBackRef) {
    // Clear interrupt flag (required for the timer to keep functioning)
    XTmrCtr_Reset(&Timer, TIMER_COUNTER);

    // Update the current digit's display
    write_digit(digits[current_digit], false);

    // Activate only the current digit (assumes channel 2 controls anode activation)
    XGpio_DiscreteWrite(&Gpio, 2, 1 << current_digit);

    // Move to the next digit
    current_digit = (current_digit + 1) % 8;
}

void initialize_display() {
    // Initialize all digits to OFF
    XGpio_DiscreteWrite(&Gpio, 1, 0x00); // Turn off all segments
    XGpio_DiscreteWrite(&Gpio, 2, 0x00); // Turn off all digit anodes
}

void initialize_timer() {
    int status;

    // Initialize timer
    status = XTmrCtr_Initialize(&Timer, TIMER_DEVICE_ID);
    if (status != XST_SUCCESS) {
        return;
    }

    // Configure timer
    XTmrCtr_SetHandler(&Timer, TimerInterruptHandler, &Timer);
    XTmrCtr_SetOptions(&Timer, TIMER_COUNTER, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(&Timer, TIMER_COUNTER, XPAR_CPU_CORE_CLOCK_FREQ_HZ / 1000); // 1ms interval

    // Start timer
    XTmrCtr_Start(&Timer, TIMER_COUNTER);
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XTmrCtr_InterruptHandler, &Timer);
}
