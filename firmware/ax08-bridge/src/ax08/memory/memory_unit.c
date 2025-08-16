#include "memory_unit.h"

#include "hardware/pio.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

// Instruction address word pins.
#define PIN_MEMORY_INSTRUCTION_ADDRESS0 14
#define PIN_MEMORY_INSTRUCTION_ADDRESS1 15

// Instruction word pins.
#define PIN_MEMORY_INSTRUCTION0 10
#define PIN_MEMORY_INSTRUCTION1 11
#define PIN_MEMORY_INSTRUCTION2 12
#define PIN_MEMORY_INSTRUCTION3 13

// Data address word pins.
#define PIN_MEMORY_DATA_ADDRESS0 18
#define PIN_MEMORY_DATA_ADDRESS1 19

// Data word pins.
#define PIN_MEMORY_DATA_IN 21
#define PIN_MEMORY_DATA_OUT 20

// Extra pins
#define PIN_MEMORY_HOLD  26
#define PIN_MEMORY_OPCODE 27

volatile AX08MemoryUnit memory_unit;

void memory_init_instruction_pio() {
    int tx_program_offset = pio_add_program(pio0, &uart_tx_program);
}

void memory_init_address_pio() {
    int rx_program_offset = pio_add_program(pio1, &uart_rx_program);
}

void memory_init_data() {
    int rx_program_offset = pio_add_program(pio2, &uart_rx_program);
    int tx_program_offset = pio_add_program(pio2, &uart_tx_program);
}

void ax08_memory_init() {
    memory_init_instruction_pio();
    memory_init_address_pio();
    memory_init_data();
}
