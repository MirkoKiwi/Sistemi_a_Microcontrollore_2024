#include <stdio.h>
#include "platform.h"
#include "xtmrctr_l.h"
#include "xil_printf.h"
#include "xparameters.h"

// Definitions
#define TmrCtrNumber 0                     // Timer 0
#define ONE_SECOND_PERIOD 100000000       // Timer counts for 1 second
#define CNT_UDT0_MASK 0x00000001          // Timer count up/down mask
#define TMR_T0INT_MASK 0x100              // Timer interrupt mask
#define TIMER_INT_SRC 0b0100              // Timer interrupt source in INTC

#define INTC_BASE_ADDR XPAR_AXI_INTC_0_BASEADDR
#define TIMER_BASE_ADDR XPAR_AXI_TIMER_0_BASEADDR
#define SEV_SEG_BASE_ADDR XPAR_AXI_7SEGS_GPIO_BASEADDR // Adjust based on your design

// Global Variables
volatile u8 digits[8] = {0};              // Digits to be displayed
volatile u8 currentDigit = 0;             // Current active digit (0-7)

// Segment mapping for characters 0-9, A-F
const u8 segmentDigitsMap[16] = {
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111, // 9
    0b01110111, // A
    0b01111100, // B
    0b00111001, // C
    0b00101110, // D
    0b01111001, // E
    0b01110001  // F
};

// Function Prototypes
void write_digit(u8 digit, u8 position);
void timerISR(void) __attribute__((interrupt_handler));
void initialize_timer(int counterValue);
void acknowledge_timer_interrupt(void);
void initialize_display(void);
void display_number(int number);

// Main Function
int main() {
    init_platform();
    
    initialize_display();  // Initialize the seven-segment display
    initialize_timer(250000); // Timer for 50 Hz refresh rate

    microblaze_enable_interrupts();

    int counter = 0;
    while (1) {
        // Increment counter and display it on the 7-segment
        display_number(counter++);
        if (counter > 99999999) counter = 0; // Reset after 8 digits
    }

    cleanup_platform();
    return 0;
}

// Function Implementations

// Write a digit to the specific position on the display
void write_digit(u8 digit, u8 position) {
    u32 anodeMask = ~(1 << position);  // Activate the selected anode
    u32 segmentData = ~segmentDigitsMap[digit]; // Map digit to segments
    
    // Write to the display hardware registers
    *(volatile u32 *)(SEV_SEG_BASE_ADDR) = anodeMask;
    *(volatile u32 *)(SEV_SEG_BASE_ADDR + 0x4) = segmentData;
}

// Timer interrupt service routine
void timerISR(void) {
    write_digit(digits[currentDigit], currentDigit); // Display the current digit
    currentDigit = (currentDigit + 1) % 8;           // Move to the next digit
    acknowledge_timer_interrupt();                   // Acknowledge the timer interrupt
}

// Initialize the timer for interrupts
void initialize_timer(int counterValue) {
    // Enable interrupts in the interrupt controller
    *(volatile u32 *)(INTC_BASE_ADDR + 0x1C) = 0x3;  // Master enable
    *(volatile u32 *)(INTC_BASE_ADDR + 0x08) = TIMER_INT_SRC; // Enable timer interrupt

    // Timer configuration
    XTmrCtr_SetControlStatusReg(TIMER_BASE_ADDR, TmrCtrNumber, 0x56); // Enable interrupt and auto-reload
    XTmrCtr_SetLoadReg(TIMER_BASE_ADDR, TmrCtrNumber, counterValue);  // Load counter value
    XTmrCtr_LoadTimerCounterReg(TIMER_BASE_ADDR, TmrCtrNumber);       // Load counter
    XTmrCtr_Enable(TIMER_BASE_ADDR, TmrCtrNumber);                    // Start timer
}

// Acknowledge the timer interrupt
void acknowledge_timer_interrupt(void) {
    *(volatile u32 *)(TIMER_BASE_ADDR) |= TMR_T0INT_MASK; // Clear timer interrupt flag
    *(volatile u32 *)(INTC_BASE_ADDR + 0x0C) = TIMER_INT_SRC; // Acknowledge interrupt
}

// Initialize the display to default state
void initialize_display(void) {
    *(volatile u32 *)(SEV_SEG_BASE_ADDR) = ~0; // Turn off all anodes
    *(volatile u32 *)(SEV_SEG_BASE_ADDR + 0x4) = 0; // Clear all segments
}

// Convert a number to its digit representation and store in `digits` array
void display_number(int number) {
    for (int i = 7; i >= 0; i--) {
        digits[i] = number % 10; // Extract each digit
        number /= 10;            // Move to the next
    }
}
