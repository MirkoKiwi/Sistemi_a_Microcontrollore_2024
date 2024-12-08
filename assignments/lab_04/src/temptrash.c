#include <stdint.h>
#include <stdbool.h>

// Define base addresses for GPIO and Timer (replace with platform-specific values)
#define GPIO_BASE_SEGMENTS  0x40020000  // Base address for 7-segment cathodes
#define GPIO_BASE_ANODES    0x40020004  // Base address for 7-segment anodes
#define TIMER_BASE          0x40010000  // Base address for Timer

// Segment patterns for digits 0-9 (Common Anode configuration)
const uint8_t segment_patterns[10] = {
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

// Global variables
volatile uint8_t digits[8] = {1, 2, 3, 4, 5, 6, 7, 8}; // Array of digits to display
volatile uint8_t current_digit = 0; // Tracks the current digit for multiplexing

// Function to write to 7-segment display
void write_digit(uint8_t digit, bool dotted) {
    uint8_t pattern = segment_patterns[digit];
    if (dotted) {
        pattern |= 0b10000000; // Add DP (decimal point)
    }
    *((volatile uint32_t *)GPIO_BASE_SEGMENTS) = pattern; // Write to segment cathodes
}

// Timer Interrupt Service Routine
void timer_isr() {
    // Update active anode
    *((volatile uint32_t *)GPIO_BASE_ANODES) = ~(1 << current_digit); // Activate current digit

    // Update segment cathodes
    write_digit(digits[current_digit], false);

    // Cycle to next digit
    current_digit = (current_digit + 1) % 8;
}

// Initialize Timer for periodic interrupts
void timer_init(uint32_t frequency) {
    volatile uint32_t *timer_control = (volatile uint32_t *)(TIMER_BASE);
    volatile uint32_t *timer_counter = (volatile uint32_t *)(TIMER_BASE + 0x4);

    *timer_control = 0x0;                          // Disable timer
    *timer_counter = 100000000 / frequency;        // Set counter (assuming 100 MHz clock)
    *timer_control = 0x1;                          // Enable timer
}

// Main program
int main() {
    init_platform();
    microblaze_enable_interrupts();
    // Initialize GPIO
    *((volatile uint32_t *)GPIO_BASE_SEGMENTS) = 0x0; // Clear segment outputs
    *((volatile uint32_t *)GPIO_BASE_ANODES) = 0xFF;  // Turn off all anodes
    
    // Initialize Timer
    timer_init(1000); // Set refresh rate to 1 kHz

    // Main loop
    while (1) {
        // Optional: Dynamically update digits array in real-time
        // Example: digits[0] = (digits[0] + 1) % 10;
    }
    
    cleanup_platform();
    return 0;
}

// Timer interrupt vector (platform-specific, adapt if needed)
void __attribute__((interrupt)) Timer_ISR_Handler() {
    timer_isr();
}
