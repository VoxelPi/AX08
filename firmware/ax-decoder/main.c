// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset disabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF      // PLL Enable (4x PLL disabled)
#pragma config STVREN = OFF     // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will not cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

#include <language_support.h>
#include <builtins.h>
#include <xc.h>
#include <stdbool.h>
#include <stdint.h>

#define _XTAL_FREQ 32000000

typedef struct address_config {
    uint8_t address;
    bool active_low;
} address_config_t;

const address_config_t CONFIG[] = {
    {.address = 0, .active_low = false},
    {.address = 1, .active_low = false},
    {.address = 2, .active_low = false},
    {.address = 3, .active_low = false},
    {.address = 4, .active_low = false},
    {.address = 5, .active_low = false},
    {.address = 6, .active_low = false},
    {.address = 7, .active_low = false},
};

/* 
    PIN MAPPING:
        RA0 - RA4: INPUT, address
        RA5 - RA6: INPUT, unused
        RA7:       INPUT, master enable (active high)
        RB0 - RB7: OUTPUT, selection pins
*/

int main() {

    // Setup the system clock.
    OSCCONbits.SPLLEN = 1;    // Enable 4x PLL.
    OSCCONbits.IRCF = 0b1110; // 32 MHz clock frequency.
    while (!OSCSTATbits.PLLR) // Wait for the clock to be initialized.
        ;

    // Initialize A pins.
    PORTA  = 0b00000000;
    LATA   = 0b00000000;
    ANSELA = 0b00000000;
    WPUA   = 0b00000000;
    TRISA  = 0b11111111;
    
    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b00000000;

    // Calculate state of 
    uint8_t disabled_state = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        address_config_t config = CONFIG[i];
        disabled_state |= config.active_low << i;
    }

    // Main loop
    while (true) {
        // Check if the master enable pin (RA7) is currently hight (active).
        bool enabled = RA7;
        if (!enabled) {
            // Set PORTB to precalculated disabled state.
            PORTB = disabled_state;
            continue;
        }
        
        // Get the current address for RA0-RA4
        int address = PORTA & 0b11111;

        // Enable selected pins.
        uint8_t data = 0;
        for (unsigned int i = 0; i < 8; ++i) {
            address_config_t config = CONFIG[i];
            bool active = (address == config.address) ^ config.active_low;
            data |= active << i;
        }
        PORTB = data;
    }
}