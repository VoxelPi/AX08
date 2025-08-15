#pragma once

#include <stdint.h>
#include "hardware/sha256.h"

#define MAX_PACKET_SIZE 2048
#define N_PACKET_BUFFER 4

typedef struct PacketBuffer {
    sha256_result_t hash;
    uint32_t size;
    uint8_t data[MAX_PACKET_SIZE];
} PacketBuffer;

void update_packet_hash(PacketBuffer *buffer);

void send_packet(PacketBuffer *buffer);
