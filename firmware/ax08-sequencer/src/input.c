#include "input.h"

#include <xc.h>
#include "state.h"

uint8_t input_buffer[INPUT_BUFFER_SIZE];
uint8_t i_next_input_state = 0;
uint8_t previous_input_state = INPUT_MASK;
volatile bool poll_input = true;

void ax08_seq_input_init() {
    // Initialize the input buffer.
    for (uint8_t i = 0; i < INPUT_BUFFER_SIZE; ++i) {
        input_buffer[i] = INPUT_MASK;
    }

    // Configure timer4 (input timer)
    // This config results in an input poll every ~5ms
    PIE3bits.TMR4IE = true;       // Enable match interrupts.
    PR4 = 39;                     // Configure timer period.
    T4CONbits.T4OUTPS = 0b1111;   // Use a postscaler of 1:16
    T4CONbits.T4CKPS = 0b11;      // Use a prescaler of 1:64
    T4CONbits.TMR4ON = true;      // Enable the timer.
}

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
            ax08_seq_action_toggle_debug_mode();
        } else {
            ax08_seq_action_reset();
        }
    }
    if (input_run_instruction) {
        ax08_seq_action_run_instruction();
    }
    if (input_run_step) {
        ax08_seq_action_run_step();
    }
    if (input_change_speed) {
        ax08_seq_action_select_timer(i_selected_timer_config + 1);
    }
}
