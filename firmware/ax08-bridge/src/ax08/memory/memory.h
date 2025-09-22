#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "../util/buffer.h"

#define AX08_N_INSTRUCTION_WORDS 0x10000
#define AX08_N_DATA_WORDS 0x8000

#define AX08_PROGRAM_CHUNK_SIZE 1024
#define AX08_PROGRAM_CHUNK_COUNT (4 * AX08_N_INSTRUCTION_WORDS / AX08_PROGRAM_CHUNK_SIZE)

typedef struct AX08MemoryUnit {
    uint32_t instruction_words[AX08_N_INSTRUCTION_WORDS];
    uint8_t data_words[AX08_N_DATA_WORDS];
    bool active;
    bool valid_program;
    uint32_t program_chunk_state[AX08_PROGRAM_CHUNK_COUNT / 32];
} AX08MemoryUnit;

volatile extern AX08MemoryUnit ax08_memory_unit;

void ax08_memory_init();

void ax08_memory_program_handle_upload_start(BufferReader *buffer);

void ax08_memory_program_handle_upload_chunk(BufferReader *buffer);

void ax08_memory_program_handle_upload_end(BufferReader *buffer);
