#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
    Variables related to the UART bridge link.
*/
#define BRIDGE_UART_BUFFER_SIZE 256 // The size of the receive buffer.

typedef struct ring_buffer_8 {
    volatile uint8_t i_read;
    volatile uint8_t i_write;
    volatile uint8_t *data;
} ring_buffer_8_t;

extern volatile ring_buffer_8_t cmd_rx_buffer; // The receive buffer.
extern volatile ring_buffer_8_t cmd_tx_buffer; // The transmit buffer.
extern volatile bool poll_command;             // If there is a command event to be processed.
extern volatile bool reset_command;            // If the command state should be reset.

/**
    Initializes the hardware used by the command module.
*/
void ax08_seq_command_init(void);

/**
    Updates the command module state.
*/
void ax08_seq_command_update(void);

/**
    Sends a byte to the bridge.
*/
#define AX08_SEQ_SEND_BYTE(_data_)                          \
    do {                                                    \
        const uint8_t _tmp = (uint8_t)(_data_);             \
        di();                                               \
        cmd_tx_buffer.data[cmd_tx_buffer.i_write++] = _tmp; \
        TXIE = 1;                                           \
        ei();                                               \
    } while (0)
