#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
    Variables related to the clock speed.
*/
#define STATE_TIMER_HW_PERIOD 127    // Timer2 period in 1/8 µs.
#define N_MAX_STATE_TIMER_CONFIGS 16 // Max. number of state timer configurations.

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

typedef enum ax08_instruction_step {
    AX08_INSTRUCTION_STEP_IDLE = 0,         // No instruction is currently active.
    AX08_INSTRUCTION_STEP_FETCH = 1,        // The instruction word is being fetched.
    AX08_INSTRUCTION_STEP_EXECUTE = 2,      // The instruction is executed.
    AX08_INSTRUCTION_STEP_PC_INCREMENT = 3, // The program counter is incremented.
    AX08_INSTRUCTION_STEP_PC_STORE = 4,     // The program counter is stored.
    AX08_INSTRUCTION_STEP_PC_PAUSE = 5,     // A pause between the program counter and result stores.
    AX08_INSTRUCTION_STEP_STORE = 6,        // The instruction result is stored.
    AX08_INSTRUCTION_STEP_RESET = 7,        // A reset instruction is executed.
} ax08_instruction_step_t;

extern uint8_t n_state_timer_configs;                              // Number of sw post scalers.
extern uint16_t state_timer_sw_periods[N_MAX_STATE_TIMER_CONFIGS]; // Number of post scalers.
extern uint8_t i_selected_timer_config;                            // The selected sw timer period.
extern volatile uint16_t state_timer_sw_value;                     // The value of the sw timer.

extern bool ax08_seq_enabled;                          // If the computer is currently enabled.
extern ax08_seq_state_t ax08_seq_mode;                 // The current sequencer state.
extern ax08_instruction_step_t ax08_instruction_state; // The current instruction step.

/**
* Initializes the hardware used by the state module.
*/
void ax08_seq_state_init(void);

/**
* Update the state timer configuration.
*/
void ax08_seq_update_timer_config(uint8_t n_configs, uint16_t *sw_periods);

/**
    Performs a sequencer state update.
*/
void ax08_seq_update_state(void);

/**
    Runs a full cycle as fast as possible.
*/
void ax08_seq_run_instruction_turbo(void);

/**
    Handles the event when the computer state switches from enabled to disabled.
*/
void ax08_seq_handle_disable(void);


#pragma region actions

/**
    Resets the computer.
    Should only be called if the computer is currently disabled.
*/
void ax08_seq_action_reset(void);

/**
    Toggles the debug mode of the computer.
    Should only be called if the computer is currently enabled.
*/
void ax08_seq_action_toggle_debug_mode(void);

/**
    Runs the next instruction.
    If the computer is currently running, it pauses at the end of the current instruction.
    If the computer is currently in debug-mode and in the middle of an instruction, the current instruction is finished.
 */
void ax08_seq_action_run_instruction(void);

/**
    Runs the next substep of the current instruction.
    If the computer is currently running, it pauses at the end of the current substep of the current instruction.
 */
void ax08_seq_action_run_step(void);

/**
    Selects the timer that should be used for the computer.
*/
void ax08_seq_action_select_timer(uint8_t i_timer);

#pragma endregion actions
