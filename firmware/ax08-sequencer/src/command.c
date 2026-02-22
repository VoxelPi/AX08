#include "command.h"

#include <xc.h>

#include "state.h"

volatile uint8_t cmd_rx_buffer[BRIDGE_UART_BUFFER_SIZE];
uint8_t i_cmd_rx_read = 0;
volatile uint8_t i_cmd_rx_write = 0;
volatile bool poll_command = false;
volatile bool reset_command = false;

typedef enum ax08_command_rx_state {
    AX08_CMD_RX_STATE_ID = 0,
    AX08_CMD_RX_STATE_LENGTH = 1,
    AX08_CMD_RX_STATE_PAYLOAD = 2,
} ax08_command_rx_state_t;
ax08_command_rx_state_t command_rx_state; // Command RX state.
uint8_t command_id = 0;
uint8_t command_payload_length = 0;
uint8_t command_payload_start = 0;
uint8_t remaining_payload = 0;            // How much of the commands payload is remaining, only valid in the PAYLOAD RX state.

#define AX08_COMMAND_TOOGLE_DEBUG_MODE 0x01
#define AX08_COMMAND_RUN_INSTRUCTION 0x02
#define AX08_COMMAND_RUN_STEP 0x03
#define AX08_COMMAND_SELECT_TIMER 0x04
#define AX08_COMMAND_UPLOAD_TIMER_CONFIG 0x80

void ax08_seq_command_handle(void);

void ax08_seq_command_init() {
    // Configure UART
    PIE1bits.RCIE = true;   // Enable UART RX interrupts.
    RCSTAbits.SPEN = true;  // Enable serial port. (Configures RX and TX pins as serial port pins)
    TXSTAbits.SYNC = false; // Asynchronous mode.
    RCSTAbits.CREN = true;  // Enable receive.
    TXSTAbits.TXEN = true;  // Enable transmit.

    // Configure timer6 (command reset timer)
    // This config results in a timer interrupt every ~32ms.
    PR6 = 255;                  // Configure timer period to max.
    PIE3bits.TMR6IE = true;     // Enable match interrupts.
    T6CONbits.T6OUTPS = 0b1111; // Use a postscaler of 1:16
    T6CONbits.T6CKPS = 0b11;    // Use a prescaler of 1:64
    T6CONbits.TMR6ON = false;   // Keep the timer disabled, only enabled while a command is being received.
}

void ax08_seq_command_update(void) {
    // Handle reset events.
    if (reset_command) {
        i_cmd_rx_read = i_cmd_rx_write;
        command_rx_state = AX08_CMD_RX_STATE_ID;
        return;
    }

    while (i_cmd_rx_read != i_cmd_rx_write) {

        switch (command_rx_state) {
        case AX08_CMD_RX_STATE_ID:
            // Received command id.
            command_id = cmd_rx_buffer[i_cmd_rx_read];
            ++i_cmd_rx_read;

            if ((command_id & 0x80) == 0) {
                // Handle command.
                ax08_seq_command_handle();
            } else {
                // Command has arguments.
                command_rx_state = AX08_CMD_RX_STATE_LENGTH;

                // Reset and enable the command reset timer.
                TMR6 = 0;
                TMR6IF = false;
                TMR6IE = true;
                TMR6ON = true;
            }
            break;

        case AX08_CMD_RX_STATE_LENGTH:
            // Receive payload length.
            command_rx_state = AX08_CMD_RX_STATE_PAYLOAD;
            command_payload_length = cmd_rx_buffer[i_cmd_rx_read];
            remaining_payload = cmd_rx_buffer[i_cmd_rx_read];
            ++i_cmd_rx_read;
            command_payload_start = i_cmd_rx_read;

            // Handle commands with a payload size of 0.
            if (remaining_payload == 0) {
                // Disable reset timer.
                TMR6ON = false;
                TMR6IE = false;

                // Handle command.
                command_rx_state = AX08_CMD_RX_STATE_ID;
                ax08_seq_command_handle();
            }

            break;

        case AX08_CMD_RX_STATE_PAYLOAD:
            // Receive payload element.
            --remaining_payload;
            ++i_cmd_rx_read;

            // Check if command is fully received.
            if (remaining_payload == 0) {
                // Disable reset timer.
                TMR6ON = false;
                TMR6IE = false;

                // Handle command.
                command_rx_state = AX08_CMD_RX_STATE_ID;
                ax08_seq_command_handle();
            }
            break;
        }
    }
}

void ax08_seq_command_handle(void) {
    // Disable interrupts.
    di();
    switch (command_id) {
    case AX08_COMMAND_TOOGLE_DEBUG_MODE:
        if (ax08_seq_enabled) {
            ax08_seq_action_toggle_debug_mode();
        } else {
            ax08_seq_action_reset();
        }
        break;

    case AX08_COMMAND_RUN_INSTRUCTION:
        ax08_seq_action_run_instruction();
        break;

    case AX08_COMMAND_RUN_STEP:
        ax08_seq_action_run_step();
        break;

    case AX08_COMMAND_SELECT_TIMER:
        if (command_payload_length < 1) {
            // Missing clock index argument.
            break;
        }
        ax08_seq_action_select_timer(cmd_rx_buffer[(uint8_t)(command_payload_start + 0)]);
        break;

    default:
        // Do nothing on invalid command, read next command.
        break;
    }
    ei();
}
