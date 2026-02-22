#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
    Variables related to the UART bridge link.
*/
#define BRIDGE_UART_BUFFER_SIZE 256 // The size of the receive buffer.

extern volatile uint8_t cmd_rx_buffer[BRIDGE_UART_BUFFER_SIZE]; // The receive buffer.
extern volatile uint8_t i_cmd_rx_write;                         // The location of the write pointer.
extern volatile bool poll_command;                              // If there is a command event to be processed.
extern volatile bool reset_command;                             // If the command state should be reset.

/**
    Initializes the hardware used by the command module.
*/
void ax08_seq_command_init(void);

/**
    Updates the command module state.
*/
void ax08_seq_command_update(void);
