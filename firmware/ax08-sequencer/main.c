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
#include <stdbool.h>
#include <stdint.h>

#define _XTAL_FREQ 32000000

#pragma region Pin Definition
/*
    PIN MAPPING:

    RA0, IN:  MODE              (active high)
    RA1, IN:  toggle debug mode (button, active low)
    RA2, IN:  run step          (button, active low)
    RA3, IN:  run instruction   (button, active low)
    RA4, IN:  cycle clock speed (button, active low)
    RA5, IN:  UART CTS          (currently unused)
    RA6, OUT: UART RTS          (currently unused)
    RA7, IN:  BREAK             (active low)

    RB0, OUT: state PC_SOURCE_INC
    RB1, IN:  UART RX
    RB2, OUT: UART TX
    RB3, OUT: state FREEZE_WRD
    RB4, OUT: state FREEZE_OP
    RB5, OUT: state HOLD
    RB6, OUT: state AD_RGSET_OVERRIDE
    RB7, OUT: state STORE
*/
#define PIN_FREEZE_WORD LATB3
#define PIN_FREEZE_OPCODE LATB4
#define PIN_HOLD_OUTPUT LATB5
#define PIN_INCREMENT_PC LATB0
#define PIN_STORE_PC LATB6
#define PIN_STORE_OUTPUT LATB7

#define PIN_BREAK RA7
#define PIN_ENABLE RA0

#pragma region Global State

/*
    Variables related to the clock speed.
*/
#define STATE_TIMER_PERIOD 50                                                        // Timer2 period in µs
const uint16_t STATE_TIMER_PS[] = { 5, 200, 20000 };                     // Clock postscalers.
const uint8_t N_STATE_TIMER_PS = sizeof(STATE_TIMER_PS) / sizeof(STATE_TIMER_PS[0]); // Number of post scalers.
uint8_t i_selected_postscaler = 0;                                                   // The selected postscaler.
volatile uint16_t unscaled_time = 0;                                                 // The unscaled time

/*
    Variables related to the state machine.
*/
typedef enum ax08_seq_state {
    AX08_SEQ_STATE_IDLE,            // The sequencer is waiting for further instructions.
    AX08_SEQ_STATE_RUN_STEP,        // The sequencer is running one step.
    AX08_SEQ_STATE_RUN_INSTRUCTION, // The sequencer is running one instruction.
    AX08_SEQ_STATE_RUN,             // The sequencer is running.
} ax08_seq_state_t;

bool enabled = true;
bool debug_mode = true;                      // If the sequencer is currently in debug mode.
ax08_seq_state_t state = AX08_SEQ_STATE_RUN; // The current state.
uint8_t cycle_state = 0;                     // A number in [0, 5], representing the current cycle state.
bool state_changed = true;                   // If a new state is available to be processed by the main loop.
bool previous_break_state = false;           // Previous state of the break pin.
bool reset_scheduled = false;

/*
    Variables related to user input.
*/
#define INPUT_BUFFER_SIZE 4
#define INPUT_MASK 0b00011110
uint8_t input_buffer[INPUT_BUFFER_SIZE];
uint8_t i_next_input_state = 0;
uint8_t previous_input_state = INPUT_MASK;
volatile bool poll_input = true;

/**
    Variables related to the UART bridge link.
*/
#define BRIDGE_UART_BUFFER_SIZE 32                  // The size of the receive buffer.
volatile char buffer_data[BRIDGE_UART_BUFFER_SIZE]; // The receive buffer.
uint16_t i_read;                                    // The location of the read pointer.
volatile uint16_t i_write;                          // The location of the write pointer.

#pragma region Library Functions

/**
    Pushes the current input state into the buffer,
    and generates the new stabalized input state from the buffer entries.

    @return The current stabalized input state.
*/
uint8_t ax08_seq_poll_input() {
    // Insert new value into input buffer.
    input_buffer[i_next_input_state] = PORTA;
    i_next_input_state += 1;
    if (i_next_input_state >= INPUT_BUFFER_SIZE) {
        i_next_input_state = 0;
    }

    // Calculate the new input state.
    uint8_t input_state = INPUT_MASK;
    for (uint8_t i = 0; i < INPUT_BUFFER_SIZE; ++i) {
        input_state &= input_buffer[i];
    }

    // Return the new input state.
    return input_state;
}

/**
    Update the statemachine using the nw input state.

    @param input_state The new input state.
*/
void ax08_seq_handle_input(uint8_t input_state) {
    // Check for changes in the input state.
    if (previous_input_state == input_state) {
        return;
    }

    // Calculate the input event mask. A 1 bit represents a falling edge in that bit.
    uint8_t input_events = (input_state ^ previous_input_state) & previous_input_state;
    previous_input_state = input_state;

    // Check inputs.
    bool input_debug_mode      = (input_events & 0b00000010) != 0;
    bool input_run_step        = (input_events & 0b00000100) != 0;
    bool input_run_instruction = (input_events & 0b00001000) != 0;
    bool input_change_speed    = (input_events & 0b00010000) != 0;

    // Handle events.
    if (input_debug_mode) {
        if (enabled) {
            // Toggle debug mode.
            debug_mode = !debug_mode;
            if (debug_mode) {
                state = AX08_SEQ_STATE_RUN_INSTRUCTION;
            } else {
                state = AX08_SEQ_STATE_RUN;
            }
        } else {
            // Schedule reset
            reset_scheduled = true;
        }
    }
    if (input_run_step) {
        // Enable debug mode if not already active,
        // and configure the statemachine to execute exactly one step.
        debug_mode = true;
        state = AX08_SEQ_STATE_RUN_STEP;
    }
    if (input_run_instruction) {
        // Enable debug mode if not already active,
        // and configure the statemachine to execute exactly one instruction.
        debug_mode = true;
        state = AX08_SEQ_STATE_RUN_INSTRUCTION;
    }
    if (input_change_speed) {
        // Cycle state timer post scaler.
        i_selected_postscaler += 1;
        if (i_selected_postscaler >= N_STATE_TIMER_PS) {
            i_selected_postscaler = 0;
        }
    }
}

/**
    Runs a single cycle step.
*/
void ax08_seq_run_step() {
    // Execute next cycle step.
    switch (cycle_state) {
        case 0:
            PIN_STORE_OUTPUT = false;
            PIN_FREEZE_WORD = true;
            break;
        case 1:
            PIN_FREEZE_WORD = false;
            PIN_FREEZE_OPCODE = true;
            break;
        case 2:
            PIN_FREEZE_OPCODE = false;
            PIN_HOLD_OUTPUT = true;
            PIN_INCREMENT_PC = true;
            break;
        case 3:
            PIN_HOLD_OUTPUT = false;
            PIN_STORE_PC = true;
            break;
        case 4:
            PIN_INCREMENT_PC = false;
            PIN_STORE_PC = false;
            break;
        case 5:
            PIN_STORE_OUTPUT = true;
            break;
        case 6: // Reset mode.
            PIN_FREEZE_WORD = false;
            PIN_FREEZE_OPCODE = false;
            PIN_HOLD_OUTPUT = false;
            PIN_INCREMENT_PC = false;
            PIN_STORE_PC = false;
            PIN_STORE_OUTPUT = false;
        default:
            break;
    }

    // Increment cycle state.
    cycle_state += 1;
    if (cycle_state >= 7) {
        cycle_state = 0;
    }
}

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

    // Initialize A pins.
    PORTA  = 0b00000000;
    LATA   = 0b00000000;
    ANSELA = 0b00000000;
    WPUA   = 0b00000000;
    TRISA  = 0b10111111;

    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b00000010;

    // Initialize input buffer.
    for (uint8_t i = 0; i < INPUT_BUFFER_SIZE; ++i) {
        input_buffer[i] = INPUT_MASK;
    }

    // Configure timer2 (state timer)
    PIE1bits.TMR2IE = true;       // Enable match interrupts.
    PR2 = STATE_TIMER_PERIOD - 1; // Configure timer period.
    T2CONbits.T2OUTPS = 0b0111;   // Use a postscaler of 1:8
    T2CONbits.T2CKPS = 0b00;      // Use a prescaler of 1:1
    T2CONbits.TMR2ON = true;      // Enable the timer.

    // Configure timer4 (input timer) (This config results in an input poll every ~5ms)
    PIE3bits.TMR4IE = true;       // Enable match interrupts.
    PR4 = 39;                     // Configure timer period.
    T4CONbits.T4OUTPS = 0b1111;   // Use a postscaler of 1:16
    T4CONbits.T4CKPS = 0b11;      // Use a prescaler of 1:64
    T4CONbits.TMR4ON = true;      // Enable the timer.

    // Configure UART
    PIE1bits.RCIE = true;   // Enable UART RX interrupts.
    RCSTAbits.SPEN = true;  // Enable serial port. (Configures RX and TX pins as serial port pins)
    TXSTAbits.SYNC = false; // Asynchronous mode.
    RCSTAbits.CREN = true;  // Enable receive.
    TXSTAbits.TXEN = true;  // Enable transmit.

    // Configure interrupts.
    INTCONbits.PEIE = true;  // Enable peripheral interrupts.
    INTCONbits.GIE = true;   // Enable interrupts.

    // Schedule a state reset.
    reset_scheduled = true;

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

        // Check for a break instruction
        if (!debug_mode && !PIN_BREAK && (previous_break_state != PIN_BREAK)) {
            debug_mode = true;
            state = AX08_SEQ_STATE_RUN_INSTRUCTION;
        }
        previous_break_state = PIN_BREAK;

        // Check if sequencer was enabled / disabled.
        if (PIN_ENABLE != enabled) {
            enabled = PIN_ENABLE;

            if (!enabled) {
                // Sequencer was disabled.
                reset_scheduled = true;

                // Reset state.
                LATB &= 0b00000100;
                debug_mode = true;
                cycle_state = 0;
            }
        }

        // Handle timer post scale.
        if (unscaled_time >= STATE_TIMER_PS[i_selected_postscaler]) {
            unscaled_time = 0;

            // Handle reset.
            if (reset_scheduled) {
                reset_scheduled = false;

                // Write reset state.
                LATB &= 0b00000100;
                PIN_STORE_PC = true;
                continue;
            }

            // Handle disabled sequencer.
            if (!enabled) {
                LATB &= 0b00000100;
                continue;
            }

            // Handle state updates.
            switch (state) {
                case AX08_SEQ_STATE_IDLE:
                    // Do nothing.
                    break;

                case AX08_SEQ_STATE_RUN_STEP:
                    // Run a sigle step and change state to idle.
                    ax08_seq_run_step();
                    state = AX08_SEQ_STATE_IDLE;
                    break;

                case AX08_SEQ_STATE_RUN_INSTRUCTION:
                    // Run a single step. If that finishes an instruction (= new cycle state is 0),
                    // change the state to idle.
                    ax08_seq_run_step();
                    if (cycle_state == 0) {
                        state = AX08_SEQ_STATE_IDLE;
                    }
                    break;

                case AX08_SEQ_STATE_RUN:
                    // Skip reset step in run state.
                    if (cycle_state == 6) {
                        cycle_state = 0;
                    }

                    // Run a single step.
                    ax08_seq_run_step();
                    break;
            }
        }
    }
}

#pragma region Interrupts

/**
    Interrupt service routine.
*/
void __interrupt() ISR() {
    // Handle timer2 match interrupt. (State timer).
    if (TMR2IF) {
        // Increment time.
        unscaled_time += 1;

        // Clear interrupt flag.
        TMR2IF = 0;
    }

    // Handle timer4 match interrupt. (Input timer).
    if (TMR4IF) {
        // Mark new input to be polled.
        poll_input = true;

        // Clear interrupt flag.
        TMR4IF = 0;
    }

    // Handle UART RX interrupts. (Bridge communication).
    if (RCIF) {
        // Insert received data into buffer.
        buffer_data[i_write] = RCREG;
        i_write += 1;
        if (i_write >= BRIDGE_UART_BUFFER_SIZE) {
            i_write = 0;
        }
    }
}
