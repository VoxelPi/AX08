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



#pragma region Config

#define _XTAL_FREQ 32000000

#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 256

#define STR_(X) #X
#define STR(X) STR_(X)

// The initialization message that is send whenever the io module is initialized.
const char *INIT_MESSAGE =
"\n _____ __ __     ___ ___ "
"\n|  _  |  |  |___|   | . |   IO UNIT"
"\n|     |-   -|___| | | . |   Version: 0.4.0"
"\n|__|__|__|__|   |___|___|   Commit: " STR(AX08_IO_FW_GIT_COMMIT)
"\n"
"\n";



#pragma region Pin Definition

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


#pragma region Global State

typedef struct ring_buffer_8 {
    volatile uint8_t i_read;
    volatile uint8_t i_write;
    volatile char *data;
} ring_buffer_8_t;

typedef struct ring_buffer_16 {
    volatile uint16_t i_read;
    volatile uint16_t i_write;
    volatile char *data;
} ring_buffer_16_t;

volatile char rx_buffer_data[RX_BUFFER_SIZE];
volatile ring_buffer_16_t rx_buffer = {
    .i_read = 0,
    .i_write = 0,
    .data = rx_buffer_data,
};

volatile char tx_buffer_data[TX_BUFFER_SIZE];
volatile ring_buffer_8_t tx_buffer = {
    .i_read = 0,
    .i_write = 0,
    .data = tx_buffer_data,
};



#pragma region Library Functions

// Sends a single character over UART.
inline void send_character(uint8_t data) {
    // Push the new data to the TX buffer.
    tx_buffer.data[tx_buffer.i_write++] = data;
    #if TX_BUFFER_SIZE != 256
    if (tx_buffer.i_write >= TX_BUFFER_SIZE) { // Always false, because buffer size is exactly the 8bit uint limit.
        tx_buffer.i_write = 0;
    }
    #endif

    // Enable UART TX interrupts. If the flag was previously cleared
    // then this will cause the processor to immediately jump into the ISR.
    TXIE = true;
}

// Sends a string over UART.
void send_string(const char* message) {
    for (const char *p = message; *p; ++p) {
        send_character(*p);
    }
}



#pragma region Main Function

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
    PIE1bits.TXIE = 0;   // Disable UART transmit interrupt (until data is there to be send).
    INTCONbits.PEIE = 1; // Enable peripheral interrupts.
    INTCONbits.GIE = 1;  // Enable interrupts.

    // Enable RTS
    PIN_UART_RTS = true;

    // Send initialization message.
    send_string(INIT_MESSAGE);

    // Main loop
    while (true) {
        statemachine_entrypoint:

        if (!PIN_OP_WRITE) {
            // Wait for hold to be set.
            while (!PIN_HOLD) {
                if (PIN_OP_WRITE) {
                    // Reset state machine.
                    goto statemachine_entrypoint;
                }
            }

            // Read the current data word.
            uint8_t data = (PORTA & 0x0F) | (PORTB & 0xF0);

            // Send the data word.
            send_character(data);

            // Wait for hold to be cleared.
            while (PIN_HOLD)
                ;

            // Reset state machine.
            continue;
        }

        if (!PIN_OP_READ) {
            // Configure data pins as outputs.
            TRISA  = 0b10110000;
            TRISB  = 0b00001011;

            // Write next rx data from buffer if available.
            uint8_t word = 0xFF;
            bool is_rx_available = rx_buffer.i_read != rx_buffer.i_write;
            if (is_rx_available) {
                word = rx_buffer.data[rx_buffer.i_read];
            }

            // Write the word.
            LATA &= 0xF0;
            LATA |= (word & 0x0F);
            LATB &= 0x0F;
            LATB |= (word & 0xF0);

            // Wait for hold to be set.
            while (!PIN_HOLD) {
                if (PIN_OP_READ) {
                    // Reset state machine.
                    TRISA  = 0b10111111;
                    TRISB  = 0b11111011;
                    goto statemachine_entrypoint;
                }
            }

            // Read was acknowledged, increment the read pointer.
            ++rx_buffer.i_read;
            if (rx_buffer.i_read >= RX_BUFFER_SIZE) {
                rx_buffer.i_read = 0;
            }

            // Wait for hold to be cleared.
            while (PIN_HOLD)
                ;

            // Configure data pins as inputs.
            TRISA  = 0b10111111;
            TRISB  = 0b11111011;

            // Reset state machine.
            continue;
        }

        if (!PIN_OP_POLL) {
            // Configure data pins as outputs.
            TRISA  = 0b10110000;
            TRISB  = 0b00001011;

            LATA &= 0xF0;
            LATB &= 0x0F;
            while (!PIN_OP_POLL) {
                // Write 1 if data is available in the rx buffer.
                bool is_rx_available = rx_buffer.i_read != rx_buffer.i_write;
                LATA0 = is_rx_available;
            }

            // Configure data pins as inputs.
            TRISA  = 0b10111111;
            TRISB  = 0b11111011;

            continue;
        }
    }
}



#pragma region Interrupts

void __interrupt(__flags(RCIF)) isr_uart_rx() {
    // Check receive interrupt.
    rx_buffer.data[rx_buffer.i_write++] = RCREG;
    if (rx_buffer.i_write >= RX_BUFFER_SIZE) {
        rx_buffer.i_write = 0;
    }
}

void __interrupt(__flags(TXIE, TXIF)) isr_uart_tx() {
    // Check transmit interrupt.
    TXREG = tx_buffer.data[tx_buffer.i_read++];
    #if TX_BUFFER_SIZE != 256
    if (tx_buffer.i_read >= TX_BUFFER_SIZE) { // Always false, because buffer size is exactly the 8bit uint limit.
        tx_buffer.i_read = 0;
    }
    #endif

    // Update interrupt enabled setting, depending if there is still data to be send. (Datasheet 26.1.1.3)
    TXIE = tx_buffer.i_read != tx_buffer.i_write;
}
