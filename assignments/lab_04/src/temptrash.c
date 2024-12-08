#include <stdint.h>
#include <stdbool.h>

// Define GPIO addresses and constants (replace these with board-specific values)
#define GPIO_BASE        0x40000000  // Base address for GPIO (example, replace as needed)
#define GPIO_SEGMENT_OUT *(volatile uint32_t *)(GPIO_BASE + 0x00) // Segment cathodes
#define GPIO_ANODE_OUT   *(volatile uint32_t *)(GPIO_BASE + 0x04) // Digit anodes
#define TIMER_BASE       0x40001000  // Base address for timer
#define TIMER_CONTROL    *(volatile uint32_t *)(TIMER_BASE + 0x00)
#define TIMER_COUNTER    *(volatile uint32_t *)(TIMER_BASE + 0x04)
#define TIMER_INTERRUPT  *(volatile uint32_t *)(TIMER_BASE + 0x08)

// Segment patterns for digits 0-9 (common anode, adjust for cathode if needed)
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

// Function to initialize the GPIO
void gpio_init() {
    // Set GPIO direction to output (implementation depends on your platform)
    // Example: Configure the segment and anode pins as outputs
    // GPIO_CONFIG = <set direction for GPIO_SEGMENT_OUT and GPIO_ANODE_OUT>;
}

// Function to initialize the timer
void timer_init(uint32_t frequency) {
    // Configure the timer to trigger interrupts at the desired frequency
    TIMER_CONTROL = 0x0; // Disable timer during configuration
    TIMER_COUNTER = 100000000 / frequency; // Example: 100 MHz clock / desired frequency
    TIMER_INTERRUPT = 0x1; // Enable interrupts
    TIMER_CONTROL = 0x1;   // Start timer
}

// Function to write a digit to the display
void write_digit(uint8_t digit, bool dotted) {
    uint8_t pattern = segment_patterns[digit];
    if (dotted) {
        pattern |= 0b10000000; // Add DP bit
    }
    GPIO_SEGMENT_OUT = pattern; // Send pattern to segment cathodes
}

// Timer interrupt service routine (ISR)
void timer_isr() {
    // Disable interrupt (clear flag if needed, depends on platform)
    TIMER_INTERRUPT = 0x0;

    // Update the display for the current digit
    GPIO_ANODE_OUT = ~(1 << current_digit); // Enable the current digit's anode (invert for active low)
    write_digit(digits[current_digit], false); // Display the current digit (without DP)

    // Move to the next digit
    current_digit = (current_digit + 1) % 8;

    // Re-enable interrupt
    TIMER_INTERRUPT = 0x1;
}

// Main function
int main() {
    // Initialize GPIO and timer
    gpio_init();
    timer_init(1000); // Set timer frequency to 1 kHz for refresh

    // Main loop
    while (1) {
        // Optionally update the digits array dynamically
        // Example: digits[0] = (digits[0] + 1) % 10; // Increment the first digit
    }

    return 0;
}

// Timer interrupt vector (platform-specific, replace with correct ISR setup)
void __attribute__((interrupt)) Timer_ISR_Handler() {
    timer_isr();
}
