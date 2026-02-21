/*
    Sequencer version 0.2.6
 */

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
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

#include "state.h"
#include "command.h"
#include "input.h"


#pragma region Main Function

/**
    Main loop.
*/
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
    TRISB  = 0b00000010;

    // Initialize hardware modules.
    ax08_seq_state_init();
    ax08_seq_input_init();
    ax08_seq_command_init();

    // Enable interrupts.
    INTCONbits.PEIE = true;  // Enable peripheral interrupts.
    INTCONbits.GIE = true;   // Enable interrupts.

    /**
        Main loop
    */
    while (true) {
        // Hande input poll event.
        if (poll_input) {
            poll_input = false;

            // Poll and handle input.
            uint8_t input_state = ax08_seq_poll_input();
            ax08_seq_handle_input(input_state);
        }

        // Handle command events.
        if (poll_command) {
            ax08_seq_command_update();
        }

        // Handle state events.
        if (ax08_seq_mode == AX08_SEQ_STATE_RUN_TURBO) {
            ax08_seq_run_instruction_turbo();
        } else if (state_timer_sw_value >= state_timer_sw_periods[i_selected_timer_config]) {
            state_timer_sw_value = 0;

            // Update the sequencer state.
            ax08_seq_update_state();
        }
    }
}


#pragma region Interrupts

/**
    Interrupt service routine.
*/
void __interrupt() ISR() {
    // Handle timer2 match interrupt. (State timer).
    if (TMR2IE && TMR2IF) {
        // Increment software timer.
        ++state_timer_sw_value;

        // Clear interrupt flag.
        TMR2IF = 0;

        return;
    }

    // Handle timer4 match interrupt. (Input timer).
    if (TMR4IF) {
        // Mark new input to be polled.
        poll_input = true;

        // Clear interrupt flag.
        TMR4IF = 0;

        return;
    }

    // Handle UART RX interrupts. (Bridge communication).
    if (RCIF) {
        // Insert received data into buffer.
        cmd_rx_buffer[i_cmd_rx_write++] = RCREG;
        #if BRIDGE_UART_BUFFER_SIZE < 256
        if (i_cmd_rx_write >= BRIDGE_UART_BUFFER_SIZE) {
            i_cmd_rx_write = 0;
        }
        #endif
        poll_command = true;

        return;
    }
}
