#include "memory_unit.h"

#include <stdint.h>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#define AX08_MEMORY_BAUD 115200

// Pin mapping
const unsigned int PINS_MEMORY_INSTRUCTION_ADDRESS[2] = {14, 15};
const unsigned int PINS_MEMORY_INSTRUCTION[4] = {10, 11, 12, 13};
const unsigned int PINS_MEMORY_DATA_ADDRESS[2] = {18, 19};

// Data word pins.
#define PIN_MEMORY_DATA_IN 20
#define PIN_MEMORY_DATA_OUT 21

// Extra pins
#define PIN_MEMORY_OPCODE 26
#define PIN_MEMORY_HOLD  27

volatile AX08MemoryUnit memory_unit;

typedef union byteint16_t {
    uint16_t word;
    uint8_t bytes[2];
} byteint16;

typedef union byteint32_t {
    uint32_t word;
    uint8_t bytes[4];
} byteint32;

volatile byteint16 instruction_address_word;
volatile byteint32 instruction_word;
volatile byteint16 data_address_word;
volatile uint8_t data_word_in;
volatile uint8_t data_word_out;

void memory_init_instruction_pio() {
    int tx_program_offset = pio_add_program(pio0, &uart_tx_program);

    // Configure all 4 pio state machines for the 4 bytes of the instruction word.
    for (unsigned int i_part = 0; i_part < 4; ++i_part) {
        unsigned int pin = PINS_MEMORY_INSTRUCTION[i_part];
        unsigned int sm = i_part;

        // Configure the pio state machine.
        pio_sm_claim(pio0, sm);
        uart_tx_program_init(pio0, sm, tx_program_offset, pin, AX08_MEMORY_BAUD);

        // Configure a dma channel to continuously copy the value from the variable to the pio fifo.
        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dma_chan_con = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dma_chan_con, DMA_SIZE_8);
        channel_config_set_read_increment(&dma_chan_con, false);
        channel_config_set_write_increment(&dma_chan_con, false);
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio0, sm, true));
        dma_channel_configure(
            dma_chan,
            &dma_chan_con,
            &pio0->txf[sm],
            instruction_word.bytes + i_part,
            dma_encode_endless_transfer_count(),
            true
        );
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
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio1, sm, false));
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

        // Configure a dma channel to continuously copy the value from the variable to the pio fifo.
        int dma_chan = dma_claim_unused_channel(true);
        dma_channel_config dma_chan_con = dma_channel_get_default_config(dma_chan);
        channel_config_set_transfer_data_size(&dma_chan_con, DMA_SIZE_8);
        channel_config_set_read_increment(&dma_chan_con, false);
        channel_config_set_write_increment(&dma_chan_con, false);
        channel_config_set_dreq(&dma_chan_con, pio_get_dreq(pio2, sm, true));
        dma_channel_configure(
            dma_chan,
            &dma_chan_con,
            &pio2->txf[sm],
            &data_word_out,
            dma_encode_endless_transfer_count(),
            true
        );
    }
}

void ax08_memory_core1_entry() {
    while (true) {
        data_word_out = data_word_in + data_address_word.bytes[0];
        instruction_word.word = instruction_address_word.word;
    }
}

void ax08_memory_init() {
    // Initialize all pio units.
    memory_init_instruction_pio();
    memory_init_address_pio();
    memory_init_data_pio();

    multicore_launch_core1(ax08_memory_core1_entry);
}
