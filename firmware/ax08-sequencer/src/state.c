#include "state.h"

#include <xc.h>

#include "config.h"

__eeprom uint8_t eeprom_n_state_timer_configs = 5;
__eeprom uint16_t eeprom_state_timer_sw_periods[N_MAX_STATE_TIMER_CONFIGS] = {
    9,
    9,    // 1000 Hz
    90,   // 100  Hz
    900,  // 10   Hz
    9000, // 1    Hz
};

uint8_t n_state_timer_configs = 0;
uint16_t state_timer_sw_periods[N_MAX_STATE_TIMER_CONFIGS];
uint8_t i_selected_timer_config = 0;
volatile uint16_t state_timer_sw_value = 0;

bool ax08_seq_enabled = false;
ax08_seq_state_t ax08_seq_mode = AX08_SEQ_STATE_IDLE;
ax08_instruction_step_t ax08_instruction_state = AX08_INSTRUCTION_STEP_IDLE;
bool reset_scheduled = true; // Schedule a reset during sequencer initialization.

bool previous_break_state = false; // Previous state of the break pin.
bool state_changed = true; // If a new state is available to be processed by the main loop.

void ax08_seq_state_init() {
    // Load the initial configuration from the EEPROM.
    n_state_timer_configs = eeprom_n_state_timer_configs;
    for (int i = 0; i < n_state_timer_configs; ++i) {
        state_timer_sw_periods[i] = eeprom_state_timer_sw_periods[i];
    }

    // Configure timer2 (state timer)
    PIE1bits.TMR2IE = true;          // Enable match interrupts.
    PR2 = STATE_TIMER_HW_PERIOD - 1; // Configure timer period.
    T2CONbits.T2OUTPS = 0b0000;      // Use a postscaler of 1:1
    T2CONbits.T2CKPS = 0b00;         // Use a prescaler of 1:1
    T2CONbits.TMR2ON = true;         // Enable the timer.

    // Update enabled state
    ax08_seq_enabled = RA0;
}


void ax08_seq_update_timer_config(uint8_t n_configs, uint16_t *sw_periods) {
    // Update timer configs.
    n_state_timer_configs = n_configs;
    eeprom_n_state_timer_configs = n_configs;
    for (uint8_t i = 0; i < n_configs; ++i) {
        state_timer_sw_periods[i] = sw_periods[i];
        eeprom_state_timer_sw_periods[i] = sw_periods[i];
    }

    // Make sure selected timer config stays in bounds.
    if (i_selected_timer_config >= n_state_timer_configs) {
        i_selected_timer_config = 0;
    }
}

void ax08_seq_run_instruction_turbo() {
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
    __delay_us(10);

    // INCREMENT PC
    if (!PIN_BREAK) {
        // Handle break opcode.
        // Switch to idle state but finish running this instruction.
        ax08_seq_mode = AX08_SEQ_STATE_IDLE;

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
    __delay_us(16);
    _delay(6);

    // RESET
    PIN_STORE_OUTPUT = false;
}

void ax08_seq_run_step() {
    // Execute next cycle step.
    switch (ax08_instruction_state) {
        case AX08_INSTRUCTION_STEP_IDLE:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_FETCH;
            PIN_STORE_OUTPUT = false;
            PIN_FREEZE_WORD = true;
            break;

        case AX08_INSTRUCTION_STEP_FETCH:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_EXECUTE;
            PIN_FREEZE_WORD = false;
            PIN_FREEZE_OPCODE = true;
            break;

        case AX08_INSTRUCTION_STEP_EXECUTE:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_PC_INCREMENT;
            PIN_FREEZE_OPCODE = false;
            PIN_HOLD_OUTPUT = true;
            PIN_INCREMENT_PC = true;
            break;

        case AX08_INSTRUCTION_STEP_PC_INCREMENT:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_PC_STORE;

            // Handle break opcode.
            if (!PIN_BREAK) {
                // Switch to run instruction state so that the current instruction is fully executed
                // before entering the idle state.
                ax08_seq_mode = AX08_SEQ_STATE_RUN_INSTRUCTION;
            }

            PIN_HOLD_OUTPUT = false;
            _delay(4);
            PIN_STORE_PC = true;
            break;

        case AX08_INSTRUCTION_STEP_PC_STORE:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_PC_PAUSE;
            PIN_INCREMENT_PC = false;
            PIN_STORE_PC = false;
            break;

        case AX08_INSTRUCTION_STEP_PC_PAUSE:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_STORE;

            // FIX for hardware bug on AX08L: https://github.com/VoxelPi/AX08/issues/2
            // Skip the store pulse, as it would clear a register.
            #ifdef BUGFIX_SKIP_BREAK_STORE
            if (!PIN_BREAK) {
                break;
            }
            #endif

            PIN_STORE_OUTPUT = true;
            break;

        case AX08_INSTRUCTION_STEP_STORE:
        case AX08_INSTRUCTION_STEP_RESET:
        default:
            ax08_instruction_state = AX08_INSTRUCTION_STEP_IDLE;
            PIN_FREEZE_WORD = false;
            PIN_FREEZE_OPCODE = false;
            PIN_HOLD_OUTPUT = false;
            PIN_INCREMENT_PC = false;
            PIN_STORE_PC = false;
            PIN_STORE_OUTPUT = false;
            break;
    }
}

void ax08_seq_update_state() {
    // Handle reset.
    if (reset_scheduled) {
        reset_scheduled = false;

        // Write reset state.
        LATB &= 0b00000100;
        PIN_STORE_PC = true;

        // Next step should be a clear.
        ax08_instruction_state = AX08_INSTRUCTION_STEP_RESET;
        ax08_seq_mode = AX08_SEQ_STATE_RUN_INSTRUCTION;

        // Skip remaining handler.
        return;
    }

    // Handle state updates.
    switch (ax08_seq_mode) {
        case AX08_SEQ_STATE_IDLE:
            // Do nothing.
            break;

        case AX08_SEQ_STATE_RUN_STEP:
            // Run a single step and change state to idle.
            ax08_seq_run_step();
            ax08_seq_mode = AX08_SEQ_STATE_IDLE;
            break;

        case AX08_SEQ_STATE_RUN_INSTRUCTION:
            // Run a single step. If that finishes an instruction (= new cycle state is 0),
            // change the state to idle.
            ax08_seq_run_step();
            if (ax08_instruction_state == AX08_INSTRUCTION_STEP_IDLE) {
                ax08_seq_mode = AX08_SEQ_STATE_IDLE;
            }
            break;

        case AX08_SEQ_STATE_RUN:
            // Check if we can enter the turbo run mode.
            // This is the case if we are currently in cycle state 0 and have selected the turbo clock (0).
            if (ax08_instruction_state == AX08_INSTRUCTION_STEP_IDLE && i_selected_timer_config == 0) {
                // Disable state timer interrupts.
                T2CONbits.TMR2ON = false;
                TMR2IE = false;

                // Enable turbo run state.
                ax08_seq_mode = AX08_SEQ_STATE_RUN_TURBO;
            } else {
                // Run a single step.
                ax08_seq_run_step();
            }
            break;

        case AX08_SEQ_STATE_RUN_TURBO:
            // This should never happen
            ax08_seq_mode = AX08_SEQ_STATE_IDLE;
            break;
    }
}

void ax08_seq_handle_disable(void) {
    // Schedule a reset.
    reset_scheduled = true;

    // Reset state.
    LATB &= 0b00000100;
    ax08_seq_mode = AX08_SEQ_STATE_RUN_INSTRUCTION;
    ax08_instruction_state = AX08_INSTRUCTION_STEP_IDLE;

    // Enable state timer & state timer interrupts.
    TMR2IE = true;
    T2CONbits.TMR2ON = true;
}



#pragma region actions

void ax08_seq_action_reset(void) {
    // Schedule a reset
    reset_scheduled = true;
}

void ax08_seq_action_toggle_debug_mode(void) {
    // Toggle debug mode.
    switch (ax08_seq_mode) {
        case AX08_SEQ_STATE_RUN:
            // Finish current instruction and then switch to idle.
            ax08_seq_mode = AX08_SEQ_STATE_RUN_INSTRUCTION;
            break;

        case AX08_SEQ_STATE_RUN_TURBO:
            // Switch to idle state.
            ax08_seq_mode = AX08_SEQ_STATE_IDLE;

            // Enable state timer & state timer interrupts.
            TMR2IE = true;
            T2CONbits.TMR2ON = true;
            break;

        default:
            // Always switch to normal run state first, as the cycle state is not known
            // and the turbo run mode assumes the previously instruction to be fully processed.
            // We therefore switch to turbo run in the state update function.
            ax08_seq_mode = AX08_SEQ_STATE_RUN;
            break;
    }
}

void ax08_seq_action_run_instruction(void) {
    // Enable debug mode if not already active,
    // and configure the statemachine to execute exactly one instruction.
    if (ax08_seq_enabled) {
        ax08_seq_mode = AX08_SEQ_STATE_RUN_INSTRUCTION;

        // Enable state timer & state timer interrupts.
        TMR2IE = true;
        T2CONbits.TMR2ON = true;
    }
}

void ax08_seq_action_run_step(void) {
    // Enable debug mode if not already active,
    // and configure the statemachine to execute exactly one step.
    if (ax08_seq_enabled) {
        ax08_seq_mode = AX08_SEQ_STATE_RUN_STEP;

        // Enable state timer & state timer interrupts.
        TMR2IE = true;
        T2CONbits.TMR2ON = true;
    }
}

void ax08_seq_action_select_timer(uint8_t i_timer) {
    // Set state timer post scaler.
    i_selected_timer_config = i_timer;
    if (i_selected_timer_config >= n_state_timer_configs) {
        i_selected_timer_config = 0;
    }

    // We need to handle two special cases when switching the clock speed:
    //
    //   1. If the sequencer is currently in turbo run mode and we switch away from the turbo clock speed,
    //      then we need to switch back to normal run mode.
    //
    //   2. If the sequencer is currently in normal run mode and we switch to the turbo clock speed,
    //      then we need to switch to the turbo run mode.
    //
    // Step 2 is handled by the state update function as the state machine needs to be in cycle state 0
    // for the turbo mode to be enabled.
    if (i_selected_timer_config != 0 && ax08_seq_mode == AX08_SEQ_STATE_RUN_TURBO) {
        // Switch to run state.
        ax08_seq_mode = AX08_SEQ_STATE_RUN;

        // Enable state timer & state timer interrupts.
        TMR2IE = true;
        T2CONbits.TMR2ON = true;
    }
}

#pragma endregion actions
