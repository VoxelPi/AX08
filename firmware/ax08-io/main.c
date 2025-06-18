// CONFIG1
#include <language_support.h>
#include <stdint.h>
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

#include <builtins.h>
#include <pic16f1847.h>
#include <xc.h>
#include <stdbool.h>

#define _XTAL_FREQ 32000000

#define BUFFER_SIZE 512

volatile uint16_t i_read;
volatile uint16_t i_write;
volatile char buffer_data[BUFFER_SIZE];

// PINS
// RB1: [INPUT]  UART RX
// RB2: [OUTPUT] UART TX

// Sends a single character over UART.
void send_character(char c) {
    // Wait for the buffer to be empty.
    while (!TXSTAbits.TRMT)
        ;

    // Send data to UART TX.
    TXREG = c;
}

// Sends a string over UART.
void send_string(const char* message) {
    for (const char *p = message; *p; ++p) {
        send_character(*p);
    }
}

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
    TRISA  = 0b00000000;
    
    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b00000010; // RB1 is a UART RX input

    // Configure UART.
    BAUDCONbits.BRG16 = 1; // Enable 16bit baud rate mode.
    TXSTAbits.BRGH = 1;    // Enable speed mode.
    SPBRGH = 3;  // Configure a baud rate of
    SPBRGL = 64; // 9600

    RCSTAbits.SPEN = 1; // Serial port enabled. (Configures RX and TX pins as serial port pins)
    TXSTAbits.SYNC = 0; // Asynchronous mode.
    TXSTAbits.TXEN = 1; // Enable transmit.
    RCSTAbits.CREN = 1; // Enable receive.

    // Configure Interrupts.

    PIE1bits.RCIE = 1;   // Enable UART receive interrupt.
    // PIE1bits.TXIE = 1;   // Enable UART transmit interrupt.
    INTCONbits.PEIE = 1; // Enable peripheral interrupts.
    INTCONbits.GIE = 1;  // Enable interrupts.

    // Send initialization message.
    send_string(" _____ __ __     ___ ___ \n");
    send_string("|  _  |  |  |___|   | . |\n");
    send_string("|     |-   -|___| | | . |\n");
    send_string("|__|__|__|__|   |___|___|\n");
    send_string("                         \n");

    // Main loop
    while (true) {
        // Echo all received characters back.
        while (i_read != i_write) {
            // Get next character from the buffer.
            char c = buffer_data[i_read];
            ++i_read;
            i_read &= 0b111111111;

            // Send the character.
            send_character(c);
            if (c == 0x0D) {
                send_character('\n');
            }
        }
    }
}

void __interrupt() ISR() {
    // Check receive interrupt.
    if (RCIF) {
        buffer_data[i_write] = RCREG;
        ++i_write;
        i_write &= 0b111111111; // Mask the first 9 bits.
    }
}