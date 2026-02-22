#include "command.h"

#include <xc.h>

#include "state.h"

volatile uint8_t cmd_rx_buffer_data[BRIDGE_UART_BUFFER_SIZE];
volatile ring_buffer_8_t cmd_rx_buffer = {
    .i_read = 0,
    .i_write = 0,
    .data = cmd_rx_buffer_data,
};

volatile uint8_t cmd_tx_buffer_data[BRIDGE_UART_BUFFER_SIZE];
volatile ring_buffer_8_t cmd_tx_buffer = {
    .i_read = 0,
    .i_write = 0,
    .data = cmd_tx_buffer_data,
};

volatile bool poll_command = false;
volatile bool reset_command = false;

typedef enum ax08_command_rx_state {
    AX08_CMD_RX_STATE_ID = 0,
    AX08_CMD_RX_STATE_LENGTH = 1,
    AX08_CMD_RX_STATE_PAYLOAD = 2,
} ax08_command_rx_state_t;
ax08_command_rx_state_t command_rx_state; // Command RX state.
uint8_t command_id = 0;                   // The id of the current command.
uint8_t command_payload_length = 0;       // The length of the payload of the current command.
uint8_t command_payload_start = 0;        // The index of the start of the payload of the current command.
uint8_t remaining_payload = 0;            // How much of the commands payload is remaining, only valid in the PAYLOAD RX state.

#define AX08_COMMAND_TOOGLE_DEBUG_MODE 0x01
#define AX08_COMMAND_RUN_INSTRUCTION 0x02
#define AX08_COMMAND_RUN_STEP 0x03
#define AX08_COMMAND_SELECT_TIMER 0x70
#define AX08_COMMAND_UPLOAD_TIMER_CONFIG 0x80

#define AX08_RESPONSE_ERROR_UNKNOWN_COMMAND 0x80
#define AX08_RESPONSE_ERROR_TIMEOUT 0x81
#define AX08_RESPONSE_ERROR_INVALID_ARGS 0x82
#define AX08_RESPONSE_ACKNOWLEDGE 0x01

void ax08_seq_command_handle(void);

void ax08_seq_command_init() {
    // Configure UART baud rate
    BAUDCONbits.BRG16 = 1; // Enable 16bit baud rate mode.
    TXSTAbits.BRGH = 1;    // Enable speed mode.
    SPBRGH = 0;            // Configure a baud rate of
    SPBRGL = 68;           // 115200 kHz

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
        reset_command = false;

        // Disable reset timer.
        TMR6ON = false;
        TMR6IE = false;

        // Reset command rx pipeline.
        cmd_rx_buffer.i_read = cmd_rx_buffer.i_write;
        command_rx_state = AX08_CMD_RX_STATE_ID;
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ERROR_TIMEOUT);
        return;
    }

    while (cmd_rx_buffer.i_read != cmd_rx_buffer.i_write) {

        switch (command_rx_state) {
        case AX08_CMD_RX_STATE_ID:
            // Received command id.
            command_id = cmd_rx_buffer.data[cmd_rx_buffer.i_read];
            ++cmd_rx_buffer.i_read;

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
            command_payload_length = cmd_rx_buffer.data[cmd_rx_buffer.i_read];
            remaining_payload = cmd_rx_buffer.data[cmd_rx_buffer.i_read];
            ++cmd_rx_buffer.i_read;
            command_payload_start = cmd_rx_buffer.i_read;

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
            ++cmd_rx_buffer.i_read;

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
    uint8_t new_n_configs;

    // Handle select timer command family.
    if ((command_id & 0xF0) == AX08_COMMAND_SELECT_TIMER) {
        uint8_t i_timer = command_id & 0x0F;
        ax08_seq_action_select_timer(i_timer);

        // Send acknowledge response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ACKNOWLEDGE);
        return;
    }

    switch (command_id) {
    case AX08_COMMAND_TOOGLE_DEBUG_MODE:
        if (ax08_seq_enabled) {
            ax08_seq_action_toggle_debug_mode();
        } else {
            ax08_seq_action_reset();
        }

        // Send acknowledge response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ACKNOWLEDGE);

        break;

    case AX08_COMMAND_RUN_INSTRUCTION:
        ax08_seq_action_run_instruction();

        // Send acknowledge response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ACKNOWLEDGE);
        break;

    case AX08_COMMAND_RUN_STEP:
        ax08_seq_action_run_step();

        // Send acknowledge response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ACKNOWLEDGE);
        break;

    case AX08_COMMAND_UPLOAD_TIMER_CONFIG:
        if ((command_payload_length < 4) || ((command_payload_length & 1) != 0)) {
            // Missing or incomplete arguments.
            AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ERROR_INVALID_ARGS);
            break;
        }

        // Load new number of clock speeds.
        new_n_configs = (command_payload_length - 2) / 2;
        if (new_n_configs > N_MAX_STATE_TIMER_CONFIGS) {
            new_n_configs = N_MAX_STATE_TIMER_CONFIGS;
        }

        // Load clock speeds.
        uint8_t i_arg = command_payload_start;
        uint16_t new_sw_periods[N_MAX_STATE_TIMER_CONFIGS];
        i_arg += 2; // Skip first two arguments. (TODO: update timer configuration)
        for (uint8_t i_speed = 0; i_speed < n_state_timer_configs; ++i_speed) {
            uint16_t sw_period = cmd_rx_buffer.data[i_arg];
            i_arg += 1;
            sw_period |= ((uint16_t)(cmd_rx_buffer.data[i_arg])) << 8;
            i_arg += 1;
            new_sw_periods[i_speed] = sw_period;
        }

        // Update config
        ax08_seq_update_timer_config(new_n_configs, new_sw_periods);

        // Send acknowledge response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ACKNOWLEDGE);
        break;

    default:
        // Do nothing on invalid command and send error response.
        AX08_SEQ_SEND_BYTE(AX08_RESPONSE_ERROR_UNKNOWN_COMMAND);
        break;
    }
}
