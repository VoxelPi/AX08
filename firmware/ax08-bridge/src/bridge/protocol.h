#pragma once

#include <stdint.h>
#include "hardware/sha256.h"

#define BRIDGE_PACKET_HEADER_SIZE 32 + 4
#define MAX_PACKET_PAYLOAD_SIZE 2048
#define N_RX_PACKET_BUFFERS 10

typedef struct PacketBuffer {
    sha256_result_t hash;
    uint32_t size;
    uint8_t data[MAX_PACKET_PAYLOAD_SIZE];
} PacketBuffer;

void bridge_protocol_init();

PacketBuffer* bridge_read_packet();

void bridge_send_packet(const PacketBuffer *buffer);

void bridge_calclate_packet_hash(PacketBuffer *buffer);
