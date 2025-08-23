#pragma once

#include <stdint.h>
#include "hardware/sha256.h"
#include "../util/buffer.h"

#define BRIDGE_PACKET_HEADER_SIZE 32 + 4
#define MAX_PACKET_PAYLOAD_SIZE 2048
#define N_RX_PACKET_BUFFERS 10

typedef struct PacketBuffer {
    sha256_result_t hash;
    uint32_t size;
    uint8_t data[MAX_PACKET_PAYLOAD_SIZE];
} PacketBuffer;

/**
 * Initializes the protocol.
 */
void ax08_bridge_protocol_init();

/**
 * Reads a packet from the external computer.
 */
PacketBuffer* ax08_bridge_read_packet();

/**
 * Sends the given packet to the external computer.
 *
 * @param buffer The packet that should be send.
 */
void ax08_bridge_send_packet(const PacketBuffer *buffer);

/**
 * Updates the sha256 hash of the given packet.
 */
void ax08_bridge_packet_update_sha256(PacketBuffer *buffer);

/**
 * Initializes a packet reader to read from the data buffer of the given packet.
 */
void ax08_bridge_packet_init_reader(const PacketBuffer *buffer, BufferReader *reader);

/**
 * Initializes a packet writer to write to the data buffer of the given packet.
 */
void ax08_bridge_packet_init_writer(PacketBuffer *buffer, BufferWriter *writer);

/**
 * Closes a packet writer.
 */
void ax08_bridge_packet_close_writer(PacketBuffer *buffer, const BufferWriter *writer);
