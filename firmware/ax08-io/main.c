// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is software controlled)
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
#include <stdint.h>

#define _XTAL_FREQ 32000000

#define BUFFER_SIZE 512

// PINS
// RA0: [I/O]    DATA.0
// RA1: [I/O]    DATA.1
// RA2: [I/O]    DATA.2
// RA3: [I/O]    DATA.3
// RA4: [INPUT]  HOLD
// RA5: [INPUT]  CTS
// RA6: [OUTPUT] RTS
// RA7: [INPUT]  OPCODE POLL
// RB0: [INPUT]  OPCODE READ
// RB1: [INPUT]  UART RX
// RB2: [OUTPUT] UART TX
// RB3: [INPUT]  OPCODE WRITE
// RB4: [I/O]    DATA.4
// RB5: [I/O]    DATA.5
// RB6: [I/O]    DATA.6
// RB7: [I/O]    DATA.7

#define PIN_HOLD RA4     // INPUT
#define PIN_OP_POLL RA7  // INPUT
#define PIN_OP_READ RB0  // INPUT
#define PIN_OP_WRITE RB3 // INPUT
#define PIN_UART_CTS RA5 // INPUT
#define PIN_UART_RTS LATA6 // OUTPUT

volatile uint16_t i_read;
volatile uint16_t i_write;
volatile char buffer_data[BUFFER_SIZE];

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

// Sends a single character over UART.
void send_character(uint8_t data) {
    // Wait for the buffer to be empty.
    while (!TXSTAbits.TRMT)
        ;

    // Send data to UART TX.
    TXREG = data;
}

// Sends a string over UART.
void send_string(const char* message) {
    for (const char *p = message; *p; ++p) {
        send_character(*p);
    }
}

uint8_t read_word() {
    return (PORTA & 0x0F) | (PORTB & 0xF0);
}

void write_word(const uint8_t word) {
    LATA &= (word | 0xF0);
    LATA |= (word & 0x0F);
    LATB &= (word | 0x0F);
    LATB |= (word & 0xF0);
}

int main() {

    // Setup the system clock.
    OSCCONbits.SPLLEN = 1;    // Enable 4x PLL.
    OSCCONbits.IRCF = 0b1110; // 32 MHz clock frequency.
    while (!OSCSTATbits.PLLR) // Wait for the clock to be initialized.
        ;

    // Enable global weak pull ups.
    OPTION_REGbits.nWPUEN = false;

    // Initialize A pins.
    PORTA  = 0b00000000;
    LATA   = 0b00000000;
    ANSELA = 0b00000000;
    WPUA   = 0b00100000;
    TRISA  = 0b10111111;

    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b11111011;

    // Configure UART.
    BAUDCONbits.BRG16 = 1; // Enable 16bit baud rate mode.
    TXSTAbits.BRGH = 1;    // Enable speed mode.
    SPBRGH = 0;  // Configure a baud rate of
    SPBRGL = 68; // 115200 kHz

    RCSTAbits.SPEN = 1; // Serial port enabled. (Configures RX and TX pins as serial port pins)
    TXSTAbits.SYNC = 0; // Asynchronous mode.
    TXSTAbits.TXEN = 1; // Enable transmit.
    RCSTAbits.CREN = 1; // Enable receive.

    // Configure Interrupts.

    PIE1bits.RCIE = 1;   // Enable UART receive interrupt.
    // PIE1bits.TXIE = 1;   // Enable UART transmit interrupt.
    INTCONbits.PEIE = 1; // Enable peripheral interrupts.
    INTCONbits.GIE = 1;  // Enable interrupts.

    // Enable RTS
    PIN_UART_RTS = true;

    // Send initialization message.
    send_string("\n");
    send_string(" _____ __ __     ___ ___ \n");
    send_string("|  _  |  |  |___|   | . |\n");
    send_string("|     |-   -|___| | | . |\n");
    send_string("|__|__|__|__|   |___|___|\n");
    send_string("                         \n");

    // Main loop
    while (true) {
        statemachine_entrypoint:
        // Configure data pins as inputs.
        TRISA  = 0b10111111;
        TRISB  = 0b11111011;

        // POLL OPERATION.
        if (!PIN_OP_POLL) {
            // Configure data pins as outputs.
            TRISA  = 0b10110000;
            TRISB  = 0b00001011;

            // Write 1 if data is available in the rx buffer.
            bool is_rx_available = i_read != i_write;
            write_word(is_rx_available);

            // Wait for hold to be set.
            while (!PIN_HOLD) {
                // Check if opcode is no longer active.
                if (PIN_OP_POLL) {
                    goto statemachine_entrypoint;
                }
            }

            // Wait for hold to be cleared.
            while (PIN_HOLD)
                ;

            // Reset state machine.
            goto statemachine_entrypoint;
        }

        // READ OPERATION.
        if (!PIN_OP_READ) {
            // Configure data pins as outputs.
            TRISA  = 0b10110000;
            TRISB  = 0b00001011;

            // Write next rx data from buffer if available.
            bool is_rx_available = i_read != i_write;
            if (is_rx_available) {
                uint8_t word = buffer_data[i_read];
                write_word(word);
            } else {
                write_word(0);
            }

            // Wait for hold to be set.
            while (!PIN_HOLD) {
                // Check if opcode is no longer active.
                if (PIN_OP_READ) {
                    goto statemachine_entrypoint;
                }
            }

            // Acknowledge read.
            if (is_rx_available) {
                ++i_read;
                i_read &= 0b111111111;
            }

            // Wait for hold to be cleared.
            while (PIN_HOLD)
                ;

            // Reset state machine.
            goto statemachine_entrypoint;
        }

        // WRITE OPERATION.
        if (!PIN_OP_WRITE) {
            // Wait for hold instruction.
            while (!PIN_HOLD) {
                // Check if opcode is no longer active.
                if (PIN_OP_WRITE) {
                    goto statemachine_entrypoint;
                }
            }

            uint8_t data = read_word();
            send_character(data);

            // Wait for hold to be cleared.
            while (PIN_HOLD)
                ;

            // Reset state machine.
            goto statemachine_entrypoint;
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
