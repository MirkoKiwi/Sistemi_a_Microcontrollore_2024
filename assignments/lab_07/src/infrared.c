#include <stdio.h>
#include <xgpio.h>     // GPIO driver
#include <xintc.h>     // Interrupt controller driver
#include <xtmrctr.h>   // Timer driver
#include <xil_exception.h>

#define GPIO_IR_PIN 1    // GPIO pin connected to the IR sensor
#define START_PULSE_MIN 8500  // Minimum time in us for start pulse detection
#define START_PULSE_MAX 9500  // Maximum time in us for start pulse detection
#define GAP_MIN 4000  // Minimum gap time in us after start pulse
#define GAP_MAX 5000  // Maximum gap time in us after start pulse
#define LOGIC_0_MIN 500   // Time for logic 0 pulse in us
#define LOGIC_0_MAX 600   // Time for logic 0 pulse in us
#define LOGIC_1_MIN 1600  // Time for logic 1 pulse in us
#define LOGIC_1_MAX 1800  // Time for logic 1 pulse in us

XGpio Gpio;            // GPIO instance
XTmrCtr Timer;         // Timer instance

volatile unsigned int timer_counter = 0;
volatile unsigned int ir_signal[68];  // Store pulse widths
volatile int signal_index = 0;
volatile int decoding_done = 0;

// Interrupt handler for the timer
void TimerInterruptHandler(void *CallBackRef, u8 TmrCtrNumber) {
    static int last_signal = 0;
    int current_signal = XGpio_DiscreteRead(&Gpio, 1);

    if (signal_index < 68) {
        if (last_signal != current_signal) {
            ir_signal[signal_index++] = timer_counter;
            timer_counter = 0;
        }
        last_signal = current_signal;
    } else {
        decoding_done = 1;
    }
    timer_counter++;
}

// NEC protocol decoding function
void NEC_Decode() {
    if (signal_index < 68) {
        printf("Error: Incomplete signal\n");
        return;
    }

    // Check start pulse timing (9ms pulse + 4.5ms gap)
    if (ir_signal[0] < START_PULSE_MIN || ir_signal[0] > START_PULSE_MAX ||
        ir_signal[1] < GAP_MIN || ir_signal[1] > GAP_MAX) {
        printf("Error: Invalid start pulse\n");
        return;
    }

    // Decode the 32-bit data (Address, Inverse Address, Command, Inverse Command)
    unsigned int address = 0, inv_address = 0, command = 0, inv_command = 0;
    for (int i = 2; i < 34; i++) {
        if (ir_signal[i] >= LOGIC_0_MIN && ir_signal[i] <= LOGIC_0_MAX) {
            if (i < 10) {
                address |= (0 << (i - 2));
            } else if (i < 18) {
                inv_address |= (0 << (i - 10));
            } else if (i < 26) {
                command |= (0 << (i - 18));
            } else {
                inv_command |= (0 << (i - 26));
            }
        } else if (ir_signal[i] >= LOGIC_1_MIN && ir_signal[i] <= LOGIC_1_MAX) {
            if (i < 10) {
                address |= (1 << (i - 2));
            } else if (i < 18) {
                inv_address |= (1 << (i - 10));
            } else if (i < 26) {
                command |= (1 << (i - 18));
            } else {
                inv_command |= (1 << (i - 26));
            }
        } else {
            printf("Error: Invalid bit timing\n");
            return;
        }
    }

    // Validate inverse values
    if ((address ^ inv_address) != 0xFF || (command ^ inv_command) != 0xFF) {
        printf("Error: Data integrity check failed\n");
        return;
    }

    // Display the decoded address and command
    printf("Address: 0x%02X, Command: 0x%02X\n", address, command);
}

// Timer and GPIO setup
void SetupHardware() {
    int Status;

    // Initialize GPIO
    Status = XGpio_Initialize(&Gpio, XPAR_GPIO_0_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        printf("GPIO Initialization Failed\n");
        return;
    }
    XGpio_SetDataDirection(&Gpio, 1, 1);  // Set GPIO as input

    // Initialize Timer
    Status = XTmrCtr_Initialize(&Timer, XPAR_TMRCTR_0_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        printf("Timer Initialization Failed\n");
        return;
    }

    // Set up the timer interrupt handler
    XTmrCtr_SetHandler(&Timer, TimerInterruptHandler, &Timer);
    XTmrCtr_SetOptions(&Timer, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);

    // Start the timer
    XTmrCtr_Start(&Timer, 0);
}

// Main function
int main() {
    SetupHardware();

    printf("Waiting for NEC signal...\n");
    while (!decoding_done) {
        // Wait until the signal is fully received
    }

    NEC_Decode();
    return 0;
}
