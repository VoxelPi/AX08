#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "../util/buffer.h"

#define AX08_N_INSTRUCTION_WORDS 0x10000
#define AX08_N_DATA_WORDS 0x8000

typedef struct AX08MemoryUnit {
    uint32_t instruction_words[AX08_N_INSTRUCTION_WORDS];
    uint8_t data_words[AX08_N_DATA_WORDS];
    bool active;
} AX08MemoryUnit;

volatile extern AX08MemoryUnit ax08_memory_unit;

void ax08_memory_init();

void ax08_memory_program_handle_chunk_update(BufferReader *buffer);
