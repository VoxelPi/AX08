#include "memory.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "../util/buffer.h"
#include "../bridge/protocol.h"

#define AX08_MEMORY_BAUD 4000000
#define AX08_MEMORY_SIZE 0x10000

// Pin mapping
const unsigned int PINS_MEMORY_INSTRUCTION_ADDRESS[2] = {14, 15};
const unsigned int PINS_MEMORY_INSTRUCTION[4] = {10, 11, 12, 13};
const unsigned int PINS_MEMORY_DATA_ADDRESS[2] = {18, 19};

// Data word pins.
#define PIN_MEMORY_DATA_IN 20
#define PIN_MEMORY_DATA_OUT 21

// Extra pins
#define PIN_MEMORY_OPCODE 26 // Opcode low = Write
#define PIN_MEMORY_HOLD  27 // Hold rising -> take input

volatile AX08MemoryUnit ax08_memory_unit;

typedef union byteint16_t {
    uint16_t word;
    uint8_t bytes[2];
} byteint16;

volatile byteint16 instruction_address_word;
volatile byteint16 data_address_word;
volatile uint8_t data_word_in;

void memory_init_instruction_pio() {
    int tx_program_offset = pio_add_program(pio0, &uart_tx_program);

    // Configure all 4 pio state machines for the 4 bytes of the instruction word.
    for (unsigned int i_part = 0; i_part < 4; ++i_part) {
        unsigned int pin = PINS_MEMORY_INSTRUCTION[i_part];
        unsigned int sm = i_part;

        // Configure the pio state machine.
        pio_sm_claim(pio0, sm);
        uart_tx_program_init(pio0, sm, tx_program_offset, pin, AX08_MEMORY_BAUD);
    }
}

void memory_init_address_pio() {
    int rx_program_offset = pio_add_program(pio1, &uart_rx_program);

    // Configure 2 pio state machines for the 2 bytes of the instruction address word.
    for (unsigned int i_part = 0; i_part < 2; ++i_part) {
        unsigned int pin = PINS_MEMORY_INSTRUCTION_ADDRESS[i_part];
        unsigned int sm = i_part;

        // Configure the pio state machine.
        pio_sm_claim(pio1, sm);
        uart_rx_program_init(pio1, sm, rx_program_offset, pin, AX08_MEMORY_BAUD);

        // Configure a dma channel to continuously copy the value from the variable to the pio fifo.
        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dma_chan_con = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dma_chan_con, DMA_SIZE_8);
        channel_config_set_read_increment(&dma_chan_con, false);
        channel_config_set_write_increment(&dma_chan_con, false);
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio1, sm, false));
        dma_channel_configure(
            dma_chan,
            &dma_chan_con,
            instruction_address_word.bytes + i_part,
            (io_rw_8*)&pio1->rxf[sm] + 3,
            dma_encode_endless_transfer_count(),
            true
        );
    }

    // Configure 2 pio state machines for the 2 bytes of the data address word.
    for (unsigned int i_part = 0; i_part < 2; ++i_part) {
        unsigned int pin = PINS_MEMORY_DATA_ADDRESS[i_part];
        unsigned int sm = i_part + 2;

        // Configure the pio state machine.
        pio_sm_claim(pio1, sm);
        uart_rx_program_init(pio1, sm, rx_program_offset, pin, AX08_MEMORY_BAUD);

        // Configure a dma channel to continuously copy the value from the variable to the pio fifo.
        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dma_chan_con = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dma_chan_con, DMA_SIZE_8);
        channel_config_set_read_increment(&dma_chan_con, false);
        channel_config_set_write_increment(&dma_chan_con, false);
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio1, sm, false));
        dma_channel_configure(
            dma_chan,
            &dma_chan_con,
            data_address_word.bytes + i_part,
            (io_rw_8*)&pio1->rxf[sm] + 3,
            dma_encode_endless_transfer_count(),
            true
        );
    }
}

void memory_init_data_pio() {
    int rx_program_offset = pio_add_program(pio2, &uart_rx_program);
    int tx_program_offset = pio_add_program(pio2, &uart_tx_program);

    {
        unsigned int sm = 0;

        // Configure a pio state machine for the data in byte.
        pio_sm_claim(pio2, sm);
        uart_rx_program_init(pio2, sm, rx_program_offset, PIN_MEMORY_DATA_IN, AX08_MEMORY_BAUD);

        // Configure a dma channel to continuously copy the value from the variable to the pio fifo.
        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dma_chan_con = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dma_chan_con, DMA_SIZE_8);
        channel_config_set_read_increment(&dma_chan_con, false);
        channel_config_set_write_increment(&dma_chan_con, false);
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio2, sm, false));
        dma_channel_configure(
            dma_chan,
            &dma_chan_con,
            &data_word_in,
            (io_rw_8*)&pio2->rxf[sm] + 3,
            dma_encode_endless_transfer_count(),
            true
        );
    }

    {
        unsigned int sm = 1;

        // Configure a pio state machine for the data out byte.
        pio_sm_claim(pio2, sm);
        uart_tx_program_init(pio2, sm, tx_program_offset, PIN_MEMORY_DATA_OUT, AX08_MEMORY_BAUD);
    }
}

void ax08_memory_core1_entry() {
    bool is_prev_hold = false;
    uint32_t previous_instruction_word = 0x00000000;
    uint8_t previous_data_word = 0x00;

    while (true) {
        // Handle data write.
        bool is_write = !gpio_get(PIN_MEMORY_OPCODE);
        bool is_hold = gpio_get(PIN_MEMORY_HOLD);
        if (is_write) {
            if (is_hold && !is_prev_hold) {
                ax08_memory_unit.data_words[data_address_word.word] = data_word_in;
            }
            is_prev_hold = is_hold;
        } else {
            is_prev_hold = false;
        }

        // Update data word.
        uint8_t data_word;
        if (data_address_word.word < AX08_N_DATA_WORDS) {
            data_word = ax08_memory_unit.data_words[data_address_word.word];
        } else {
            data_word = ax08_memory_unit.instruction_words[data_address_word.word] & 0xFF;
        }
        if (data_word != previous_data_word) {
            pio_sm_put(pio2, 1, data_word);
            previous_data_word = data_word;
        }

        // Update instruction word.
        uint32_t instruction_word = ax08_memory_unit.instruction_words[instruction_address_word.word];
        if (ax08_memory_unit.active) {
            if (instruction_word != previous_instruction_word) {
                pio_sm_put(pio0, 0, (instruction_word >>  0) & 0xFF);
                pio_sm_put(pio0, 1, (instruction_word >>  8) & 0xFF);
                pio_sm_put(pio0, 2, (instruction_word >> 16) & 0xFF);
                pio_sm_put(pio0, 3, (instruction_word >> 24) & 0xFF);
                previous_instruction_word = instruction_word;
            }
        }
    }
}

void ax08_memory_init() {
    // Initialize all pio units.
    memory_init_instruction_pio();
    memory_init_address_pio();
    memory_init_data_pio();

    // uart_puts(uart1, "MEMORY INIT\n"); THIS KILL EVERYTHING

    // Clear memory.
    ax08_memory_unit.active = false;
    ax08_memory_unit.valid_program = false;
    memset((void*)ax08_memory_unit.instruction_words, 0, AX08_N_INSTRUCTION_WORDS * sizeof(uint32_t));
    memset((void*)ax08_memory_unit.data_words, 0, AX08_N_DATA_WORDS * sizeof(uint8_t));
    memset((void*)ax08_memory_unit.program_chunk_state, 0xFF, AX08_PROGRAM_CHUNK_COUNT / 8);

    // Configure extra pins.
    gpio_init(PIN_MEMORY_OPCODE);
    gpio_set_dir(PIN_MEMORY_OPCODE, GPIO_IN);
    gpio_init(PIN_MEMORY_HOLD);
    gpio_set_dir(PIN_MEMORY_HOLD, GPIO_IN);

    // Launch memory unit main method on core1.
    multicore_launch_core1(ax08_memory_core1_entry);
}

#pragma region Program Upload

void ax08_memory_program_handle_upload_start(BufferReader *buffer) {
    // Mark the program as not active.
    ax08_memory_unit.active = false;
    ax08_memory_unit.valid_program = true;

    // Output 0 instruction.
    pio_sm_put(pio0, 0, 0);
    pio_sm_put(pio0, 1, 0);
    pio_sm_put(pio0, 2, 0);
    pio_sm_put(pio0, 3, 0);

    // Clear the memory.
    memset((void*)ax08_memory_unit.instruction_words, 0, AX08_N_INSTRUCTION_WORDS * sizeof(uint32_t));
    memset((void*)ax08_memory_unit.data_words, 0, AX08_N_DATA_WORDS * sizeof(uint8_t));

    // Copy over chunk flags (n / 8) bytes. (0 is ready, 1 is pending chunk update)
    buffer_read(buffer, AX08_PROGRAM_CHUNK_COUNT / 8, (void*)(ax08_memory_unit.program_chunk_state));
}

void ax08_memory_program_handle_upload_chunk(BufferReader *buffer) {
    // Read chunk index.
    uint16_t chunk_index;
    buffer_read_uint16(buffer, &chunk_index);

    // Clear chunk flag.
    size_t i_chunk_state_word = chunk_index >> 5;
    size_t i_chunk_state_bit = chunk_index & 0b11111;
    if (!(ax08_memory_unit.program_chunk_state[i_chunk_state_word] & (1 << i_chunk_state_bit))) {
        // A chunk was received that was already marked as ready.
        ax08_memory_unit.valid_program = false;
        return;
    }
    ax08_memory_unit.program_chunk_state[i_chunk_state_word] &= ~(1 << i_chunk_state_bit);

    // Copy data.
    buffer_read(buffer, 1024, ((void*)ax08_memory_unit.instruction_words) + (chunk_index * 1024));
}

void ax08_memory_program_handle_upload_end(BufferReader *buffer) {
    // Loop through all words of the chunk map. If any bit of the flag is still set,
    // Then the computer is still waiting for a chunk and the program is not ready.
    bool ready = true;
    for (unsigned int i = 0; i < AX08_PROGRAM_CHUNK_COUNT / 32; ++i) {
        if (ax08_memory_unit.program_chunk_state[i]) {
            ready = false;
            break;
        }
    }

    // Set the memory unit state to ready if threre is a valid program.
    if (ready && ax08_memory_unit.valid_program) {
        // Mark the program as active.
        ax08_memory_unit.active = true;
    }

    // Send program state packet.
    PacketBuffer response;
    BufferWriter writer;
    ax08_bridge_packet_init_writer(&response, &writer);

    buffer_write_uint8(&writer, 0x13); // Response packet id.
    buffer_write_uint8(&writer, ax08_memory_unit.valid_program); // Valid program.
    buffer_write(&writer, AX08_PROGRAM_CHUNK_COUNT / 8, (void*)(ax08_memory_unit.program_chunk_state));

    ax08_bridge_packet_close_writer(&response, &writer);
    ax08_bridge_packet_update_sha256(&response);
    ax08_bridge_send_packet(&response);
}
