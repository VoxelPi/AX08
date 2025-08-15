#pragma once

#include <stdint.h>

#define INSTRUCTION_MEMORY_SIZE 0x10000
#define DATA_MEMORY_SIZE 0x8000

typedef struct AX08MemoryUnit {
    uint32_t instructions[INSTRUCTION_MEMORY_SIZE];
    uint8_t memory[DATA_MEMORY_SIZE];
} AX08MemoryUnit;

volatile extern AX08MemoryUnit memory_unit;
