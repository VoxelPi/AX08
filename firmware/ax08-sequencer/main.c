/*
    Sequencer version 0.2.5
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
#include <stdbool.h>
#include <stdint.h>

#define _XTAL_FREQ 32000000

#pragma region Pin Definition
/*
    PIN MAPPING:

    RA0, IN:  MODE              (active high)
    RA1, IN:  toggle debug mode (button, active low)
    RA2, IN:  run instruction   (button, active low)
    RA3, IN:  run step          (button, active low)
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

/**
    Optional bug fixes.
 */
// Fix https://github.com/VoxelPi/AX08/issues/2
// Only relevant on the AX08L boards.
#define BUGFIX_SKIP_BREAK_STORE

#pragma region Global State

/*
    Variables related to the clock speed.
    Three clock speed preset are defined:
    - 5kHz
    - 100 Hz
    - 1 Hz
*/
#define STATE_TIMER_HW_PERIOD 127                                                                       // Timer2 period in 1/8 µs.
const uint16_t STATE_TIMER_SW_PERIOD[] = { 9, 9, 90, 9000 };                            // Clock postscalers.  (20kHz tubro, 1kHz, 100Hz, 1Hz)
const uint8_t N_STATE_TIMER_CONFIGS = sizeof(STATE_TIMER_SW_PERIOD) / sizeof(STATE_TIMER_SW_PERIOD[0]); // Number of post scalers.
uint8_t i_selected_timer_config = 0;                                                                    // The selected sw timer period.
volatile uint16_t state_timer_sw_value = 0;                                                             // The value of the sw timer.

/*
    Variables related to the state machine.
*/
typedef enum ax08_seq_state {
    AX08_SEQ_STATE_RUN_TURBO = 0,       // The sequencer is running in turbo mode.
    AX08_SEQ_STATE_RUN = 1,             // The sequencer is running.
    AX08_SEQ_STATE_RUN_INSTRUCTION = 2, // The sequencer is running one instruction.
    AX08_SEQ_STATE_RUN_STEP = 3,        // The sequencer is running one step.
    AX08_SEQ_STATE_IDLE = 4,            // The sequencer is waiting for further instructions.
} ax08_seq_state_t;

bool enabled = false;
ax08_seq_state_t state = AX08_SEQ_STATE_IDLE; // The current state.
uint8_t cycle_state = 0;                      // A number in [0, 5], representing the current cycle state.
bool state_changed = true;                    // If a new state is available to be processed by the main loop.
bool previous_break_state = false;            // Previous state of the break pin.
bool reset_scheduled = true;                  // Schedule a reset during sequencer initialization.

/*
    Variables related to user input.
*/
#define INPUT_BUFFER_SIZE 4
#define INPUT_MASK 0b00011111
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
    input_buffer[i_next_input_state++] = PORTA;
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
    uint8_t input_state_change = input_state ^ previous_input_state;
    uint8_t input_falling_events = input_state_change & previous_input_state;
    previous_input_state = input_state;

    // Check falling events.
    bool input_mode_disabled   = (input_falling_events & 0b00000001) != 0;
    bool input_debug_mode      = (input_falling_events & 0b00000010) != 0;
    bool input_run_instruction = (input_falling_events & 0b00000100) != 0;
    bool input_run_step        = (input_falling_events & 0b00001000) != 0;
    bool input_change_speed    = (input_falling_events & 0b00010000) != 0;

    // Update state pins
    enabled = (input_state & 0b00000001) != 0;

    // Handle events.
    if (input_mode_disabled) {
        // Schedule a reset.
        reset_scheduled = true;

        // Reset state.
        LATB &= 0b00000100;
        state = AX08_SEQ_STATE_RUN_INSTRUCTION;
        cycle_state = 0;

        // Enable state timer & state timer interrupts.
        TMR2IE = true;
        T2CONbits.TMR2ON = true;
    }
    if (input_debug_mode) {
        if (enabled) {
            // Toggle debug mode.
            switch (state) {
                case AX08_SEQ_STATE_RUN:
                    // Finish current instruction and then switch to idle.
                    state = AX08_SEQ_STATE_RUN_INSTRUCTION;
                    break;

                case AX08_SEQ_STATE_RUN_TURBO:
                    // Switch to idle state.
                    state = AX08_SEQ_STATE_IDLE;

                    // Enable state timer & state timer interrupts.
                    TMR2IE = true;
                    T2CONbits.TMR2ON = true;
                    break;

                default:
                    // Always switch to normal run state first, as the cycle state is not known
                    // and the turbo run mode assumes the previously instruction to be fully processed.
                    // We therefore switch to turbo run in the state update function.
                    state = AX08_SEQ_STATE_RUN;
                    break;
            }
        } else {
            // Schedule reset
            reset_scheduled = true;
        }
    }
    if (input_run_instruction) {
        // Enable debug mode if not already active,
        // and configure the statemachine to execute exactly one instruction.
        if (enabled) {
            state = AX08_SEQ_STATE_RUN_INSTRUCTION;

            // Enable state timer & state timer interrupts.
            TMR2IE = true;
            T2CONbits.TMR2ON = true;
        }
    }
    if (input_run_step) {
        // Enable debug mode if not already active,
        // and configure the statemachine to execute exactly one step.
        if (enabled) {
            state = AX08_SEQ_STATE_RUN_STEP;

            // Enable state timer & state timer interrupts.
            TMR2IE = true;
            T2CONbits.TMR2ON = true;
        }
    }
    if (input_change_speed) {
        // Cycle state timer post scaler.
        ++i_selected_timer_config;
        if (i_selected_timer_config >= N_STATE_TIMER_CONFIGS) {
            i_selected_timer_config = 0;
        }

        // We need to handle two special cases when switching the clock speed:
        //
        //   1. If the sequencer is currently in turbo run mode and we switch away from the turbo clock speed,
        //      we need to switch back to normal run mode.
        //
        //   2. If the sequencer is currently in normal run mode and we switch to the turbo clock speed,
        //      we need to switch to the turbo run mode. - This step is handled by the state
        //
        // Step 2 is handled by the state update function as the state machine needs to be in cycle state 0
        // for the turbo mode to be enabled.
        if (i_selected_timer_config != 0 && state == AX08_SEQ_STATE_RUN_TURBO) {
            // Switch to run state.
            state = AX08_SEQ_STATE_RUN;

            // Enable state timer & state timer interrupts.
            TMR2IE = true;
            T2CONbits.TMR2ON = true;
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
            // Handle break opcode.
            if (!PIN_BREAK) {
                // Switch to run instruction state so that the current instruction is full executed
                // before entering the idle state.
                state = AX08_SEQ_STATE_RUN_INSTRUCTION;
            }

            PIN_HOLD_OUTPUT = false;
            _delay(4);
            PIN_STORE_PC = true;
            break;
        case 4:
            PIN_INCREMENT_PC = false;
            PIN_STORE_PC = false;
            break;
        case 5:
            // FIX for hardware bug on AX08L: https://github.com/VoxelPi/AX08/issues/2
            // Skip the store pulse, as it would clear a register.
            #ifdef BUGFIX_SKIP_BREAK_STORE
            if (!PIN_BREAK) {
                break;
            }
            #endif

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
    ++cycle_state;
    if (cycle_state >= 7) {
        cycle_state = 0;
    }
}

/**
    Runs a full cycle as fast as possible.
*/
void ax08_seq_run_instruction_turbo() {
    // Deactivate interrupts during the cycle so that the timings are perfect.
    // The method only takes ~55µs, so this does not affect other functions in a meaningful way.
    GIE = false;

    // FREEZE INSTRUCTION
    PIN_STORE_OUTPUT = false;
    PIN_FREEZE_WORD = true;
    __delay_us(2);

    // FREEZE OPCODE
    PIN_FREEZE_WORD = false;
    PIN_FREEZE_OPCODE = true;
    __delay_us(14);

    // FREEZE RESULT
    PIN_FREEZE_OPCODE = false;
    PIN_HOLD_OUTPUT = true;
    PIN_INCREMENT_PC = true;
    __delay_us(13);

    // INCREMENT PC
    if (!PIN_BREAK) {
        // Handle break opcode.
        // Switch to idle state but finish running this instruction.
        state = AX08_SEQ_STATE_IDLE;

        // Enable state timer & state timer interrupts.
        TMR2IE = true;
        T2CONbits.TMR2ON = true;
    }
    PIN_HOLD_OUTPUT = false;
    _delay(4);
    PIN_STORE_PC = true;
    _delay(4);

    // STORE PC
    PIN_INCREMENT_PC = false;
    PIN_STORE_PC = false;
    _delay(4);

    // STORE RESULT
    #ifdef BUGFIX_SKIP_BREAK_STORE
    if (PIN_BREAK) {
        PIN_STORE_OUTPUT = true;
    }
    #else
        PIN_STORE_OUTPUT = true;
    #endif
    __delay_us(13);
    _delay(6);

    // RESET
    PIN_STORE_OUTPUT = false;

    // Re-activate interrupts.
    GIE = true;
}

/**
    Performs a sequencer state update.
*/
void ax08_seq_update_state() {
    // Handle reset.
    if (reset_scheduled) {
        reset_scheduled = false;

        // Write reset state.
        LATB &= 0b00000100;
        PIN_STORE_PC = true;

        // Next step should be a clear.
        cycle_state = 6;
        state = AX08_SEQ_STATE_RUN_INSTRUCTION;

        // Skip remaining handler.
        return;
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
            // Check if we can enter the turbo run mode.
            // This is the case if we are currently in cycle state 0 and have selected the turbo clock (0).
            if (cycle_state == 0 && i_selected_timer_config == 0) {
                // Disable state timer interrupts.
                T2CONbits.TMR2ON = false;
                TMR2IE = false;

                // Enable turbo run state.
                state = AX08_SEQ_STATE_RUN_TURBO;
            } else {
                // Run a single step.
                ax08_seq_run_step();
            }
            break;

        case AX08_SEQ_STATE_RUN_TURBO:
            // This should never happen
            state = AX08_SEQ_STATE_IDLE;
            break;
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

    // Initialize input buffer.
    for (uint8_t i = 0; i < INPUT_BUFFER_SIZE; ++i) {
        input_buffer[i] = INPUT_MASK;
    }

    // Configure timer2 (state timer)
    PIE1bits.TMR2IE = true;          // Enable match interrupts.
    PR2 = STATE_TIMER_HW_PERIOD - 1; // Configure timer period.
    T2CONbits.T2OUTPS = 0b0000;      // Use a postscaler of 1:1
    T2CONbits.T2CKPS = 0b00;         // Use a prescaler of 1:1
    T2CONbits.TMR2ON = true;         // Enable the timer.

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

        if (state == AX08_SEQ_STATE_RUN_TURBO) {
            ax08_seq_run_instruction_turbo();
        } else {
            // Handle state timer event.
            if (state_timer_sw_value >= STATE_TIMER_SW_PERIOD[i_selected_timer_config]) {
                state_timer_sw_value = 0;

                // Update the sequencer state.
                ax08_seq_update_state();
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
        buffer_data[i_write++] = RCREG;
        if (i_write >= BRIDGE_UART_BUFFER_SIZE) {
            i_write = 0;
        }

        return;
    }
}
