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

#include <xc.h>
#include <stdbool.h>

#define _XTAL_FREQ 32000000

/* 
    PIN MAPPING
    RA0 - RA3: [OUTPUT] output bits 0..3
    RB4 - RB7: [OUTPUT] output bits 4..7
    RB1:       [INPUT]  UART RX

    UNUSED: RA4 - RA7, RB0, RB2, RB3
*/

typedef union {
    struct {
        unsigned bit0 :1;
        unsigned bit1 :1;
        unsigned bit2 :1;
        unsigned bit3 :1;
        unsigned bit4 :1;
        unsigned bit5 :1;
        unsigned bit6 :1;
        unsigned bit7 :1;
    };
    unsigned byte :8;
} bitwise_byte_t;

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
    TRISA  = 0b11110000;
    
    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b00001111; // RB2 is a UART TX output

    // Configure UART baud rate.
    BAUDCONbits.BRG16 = 1; // Enable 16bit baud rate mode.
    TXSTAbits.BRGH = 1;    // Enable speed mode.
    SPBRGH = 0;  // Configure a baud rate of
    SPBRGL = 68; // ~ 115200

    // Configure UART.
    RCSTAbits.SPEN = 1; // Serial port enabled. (Configures RX and TX pins as serial port pins)
    TXSTAbits.SYNC = 0; // Asynchronous mode.
    RCSTAbits.CREN = 1; // Enable receive.

    // Configure Interrupts.
    PIE1bits.RCIE = 1;   // Enable UART receive interrupt.
    INTCONbits.PEIE = 1; // Enable peripheral interrupts.
    INTCONbits.GIE = 1;  // Enable interrupts.

    // Main loop
    while (true)
        ;
}

void __interrupt() ISR() {
    // Check receive interrupt.
    if (RCIF) {
        bitwise_byte_t data;
        data.byte = RCREG;

        RA0 = data.bit0;
        RA1 = data.bit1;
        RA2 = data.bit2;
        RA3 = data.bit3;
        RB4 = data.bit4;
        RB5 = data.bit5;
        RB6 = data.bit6;
        RB7 = data.bit7;
    }
}