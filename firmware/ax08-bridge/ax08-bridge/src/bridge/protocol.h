#pragma once

#include <stdint.h>
#include "hardware/sha256.h"

#define MAX_PACKET_SIZE 2048
#define N_PACKET_BUFFER 4

typedef struct PacketBuffer {
    sha256_result_t hash;
    uint16_t size;
    uint8_t data[MAX_PACKET_SIZE];
} PacketBuffer;

void send_packet(PacketBuffer *buffer);
