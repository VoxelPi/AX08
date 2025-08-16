#include "protocol.h"

#include "hardware/dma.h"
#include "hardware/uart.h"
#include "pico/sha256.h"
#include "pico/stdlib.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define PIN_BRIDGE_UART_TX 0
#define PIN_BRIDGE_UART_RX 1
#define PIN_BRIDGE_UART_CTS 2
#define PIN_BRIDGE_UART_RTS 3

#define BRIDGE_BAUD_RATE 115200

volatile PacketBuffer rx_packets[N_RX_PACKET_BUFFERS];
volatile PacketBuffer *rx_packet = rx_packets;
PacketBuffer *processed_rx_packet = rx_packets;

int dma_ch_rx_header = -1;
int dma_ch_rx_payload = -1;
int dma_ch_tx = -1;

void bridge_init_rx_header() {
    if (dma_channel_is_busy(dma_ch_rx_header)) {
        uart_puts(uart1, "HIRX BAD\n");
    }

    dma_channel_config config = dma_channel_get_default_config(dma_ch_rx_header);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_UART0_RX);

    dma_channel_configure(
        dma_ch_rx_header,
        &config,
        rx_packet,
        &uart_get_hw(uart0)->dr,
        BRIDGE_PACKET_HEADER_SIZE,
        false
    );

    // char hilfe[12];
    // itoa((uint32_t) rx_packet, hilfe, 10);
    // uart_puts(uart1, hilfe);
    // uart_puts(uart1, "\n");
    uart_puts(uart1, "HIRX\n");

    // // Clear any remaining interupts.
    dma_irqn_acknowledge_channel(1, dma_ch_rx_header);
    dma_irqn_set_channel_enabled(1, dma_ch_rx_header, true);

    // // TODO: Probably set some status variable for watchdog.
    dma_channel_start(dma_ch_rx_header);
}

void bridge_init_rx_payload(uint32_t size) {
    if (dma_channel_is_busy(dma_ch_rx_payload)) {
        uart_puts(uart1, "PIRX BAD\n");
    }

    dma_channel_config config = dma_channel_get_default_config(dma_ch_rx_payload);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_UART0_RX);

    dma_channel_configure(
        dma_ch_rx_payload,
        &config,
        rx_packet->data,
        &uart_get_hw(uart0)->dr,
        size,
        false
    );

    // // Clear any remaining interupts.
    dma_irqn_acknowledge_channel(1, dma_ch_rx_payload);
    dma_irqn_set_channel_enabled(1, dma_ch_rx_payload, true);
    dma_channel_start(dma_ch_rx_payload);
    uart_puts(uart1, "PIRX\n");
}

void bridge_handle_rx_header() {
    uart_puts(uart1, "HRX\n");
    // uart_puts(uart1, "RX HEADER\n");

    // char hilfe[10];
    // itoa(rx_packet->size, hilfe, 10);
    // uart_puts(uart1, hilfe);
    // uart_puts(uart1, "\n");

    // Get the size of the packet.
    uint32_t size = rx_packet->size;

    // Check that the size of the packet is valid.
    if (size == 0 || size > MAX_PACKET_PAYLOAD_SIZE) {
        bridge_init_rx_header();
        return;
    }

    // Initialize bridge to receive the payload.
    bridge_init_rx_payload(size);
}

void bridge_handle_rx_payload() {
    uart_puts(uart1, "PRX\n");
    // uart_puts(uart1, rx_packet->data);

    // Increment packet pointer.
    rx_packet += 1;
    if (rx_packet >= rx_packets + N_RX_PACKET_BUFFERS) {
        rx_packet = rx_packets;
    }

    // Initialize bridge to receive the next header.
    bridge_init_rx_header();
}

void bridge_dma_irq1_handler() {
    uart_puts(uart1, "S\n");

    // Check if packet header was received.
    if (dma_irqn_get_channel_status(1, dma_ch_rx_header)) {
        bridge_handle_rx_header();
        dma_irqn_acknowledge_channel(1, dma_ch_rx_header);
    }

    // Check if packet payload was received.
    if (dma_irqn_get_channel_status(1, dma_ch_rx_payload)) {
        bridge_handle_rx_payload();
        dma_irqn_acknowledge_channel(1, dma_ch_rx_payload);
    }

    uart_puts(uart1, "E\n");
}

void bridge_protocol_init() {
    uart_init(uart0, BRIDGE_BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(PIN_BRIDGE_UART_TX, GPIO_FUNC_UART); // UART0 TX
    gpio_set_function(PIN_BRIDGE_UART_RX, GPIO_FUNC_UART); // UART0 RX
    gpio_set_function(PIN_BRIDGE_UART_CTS, GPIO_FUNC_UART); // UART0 CTS (Clear To Send, Pico INPUT, active low)
    gpio_set_function(PIN_BRIDGE_UART_RTS, GPIO_FUNC_UART); // UART0 RTS (Request To Send, Pico OUTPUT, active low)

    // Configure uart.
    uart_set_fifo_enabled(uart0, true);
    uart_set_hw_flow(uart0, true, true);

    // Claim dma channels.
    dma_ch_rx_header = dma_claim_unused_channel(true);
    dma_ch_rx_payload = dma_claim_unused_channel(true);
    dma_ch_tx = dma_claim_unused_channel(true);

    // Configure the rx channels to raise the dma-interrupt-1
    dma_irqn_set_channel_enabled(1, dma_ch_rx_header, true);
    dma_irqn_set_channel_enabled(1, dma_ch_rx_payload, true);

    // Enable the DMA IRQ 1 interrupt.
    irq_set_exclusive_handler(DMA_IRQ_1, bridge_dma_irq1_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // Initialize state machine.
    bridge_init_rx_header();
}

PacketBuffer* bridge_read_packet() {
    // Check if a new packet was received, otherwise, return nullptr.
    if (processed_rx_packet == rx_packet) {
        return NULL;
    }

    PacketBuffer *received_packet = processed_rx_packet;

    // Increment processed packet pointer.
    processed_rx_packet += 1;
    if (processed_rx_packet >= rx_packets + N_RX_PACKET_BUFFERS) {
        processed_rx_packet = rx_packets;
    }

    // Return the address packet.
    return received_packet;
}

void bridge_calclate_packet_hash(PacketBuffer *buffer) {
    pico_sha256_state_t state;
    int result = pico_sha256_start_blocking(&state, SHA256_BIG_ENDIAN, true);
    hard_assert(result == PICO_OK);
    pico_sha256_update_blocking(&state, buffer->data - 4, buffer->size + 4); // Also hash size field.
    pico_sha256_finish(&state, &buffer->hash);
}

void bridge_send_packet(const PacketBuffer *buffer) {
    dma_channel_config channel_config = dma_channel_get_default_config(dma_ch_tx);
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    channel_config_set_dreq(&channel_config, DREQ_UART0_TX);
    channel_config_set_read_increment(&channel_config, true);
    channel_config_set_write_increment(&channel_config, false);

    dma_channel_configure(
        dma_ch_tx,
        &channel_config,
        &uart_get_hw(uart0)->dr,
        buffer,
        buffer->size + sizeof(uint32_t) + sizeof(sha256_result_t),
        true
    );

    dma_channel_wait_for_finish_blocking(dma_ch_tx);

    dma_channel_unclaim(dma_ch_tx);
}
