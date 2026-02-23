#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
    Variables related to user input.
*/
#define INPUT_BUFFER_SIZE 4
#define INPUT_MASK 0b00011111

extern uint8_t input_buffer[INPUT_BUFFER_SIZE];
extern uint8_t i_next_input_state;
extern uint8_t previous_input_state;
extern volatile bool poll_input;

/**
    Initializes the hardware used by the input module.
*/
void ax08_seq_input_init(void);

/**
    Pushes the current input state into the buffer,
    and generates the new stabilized input state from the buffer entries.

    @return The current stabilized input state.
*/
uint8_t ax08_seq_poll_input(void);

/**
    Update the statemachine using the new input state.

    @param input_state The new input state.
*/
void ax08_seq_handle_input(uint8_t input_state);
