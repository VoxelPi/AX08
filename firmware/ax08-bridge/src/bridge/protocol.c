#include "protocol.h"

#include "hardware/dma.h"
#include "hardware/uart.h"
#include "pico/sha256.h"
#include "pico/stdlib.h"

#include <string.h>

#define BRIDGE_BAUD_RATE 115200

PacketBuffer packets[N_PACKET_BUFFER];

int dma_ch_rx_header = -1;
int dma_ch_rx_payload = -1;
int dma_ch_tx = -1;

void bridge_protocol_init() {
    uart_init(uart0, BRIDGE_BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(0, GPIO_FUNC_UART); // UART0 TX
    gpio_set_function(1, GPIO_FUNC_UART); // UART0 RX
    gpio_set_function(2, GPIO_FUNC_UART); // UART0 CTS (Clear To Send, Pico INPUT, active low)
    gpio_set_function(3, GPIO_FUNC_UART); // UART0 RTS (Request To Send, Pico OUTPUT, active low)

    uart_set_hw_flow(uart0, true, true);

    // irq_set_exclusive_handler(UART0_IRQ, uart0_rx_irq_handler);
    // irq_set_enabled(UART0_IRQ, true);
    // uart_set_irqs_enabled(uart0, true, false);

    dma_ch_rx_header = dma_claim_unused_channel(true);
    dma_ch_rx_payload = dma_claim_unused_channel(true);
    dma_ch_tx = dma_claim_unused_channel(true);
}

void update_packet_hash(PacketBuffer *buffer) {
    pico_sha256_state_t state;
    int result = pico_sha256_start_blocking(&state, SHA256_BIG_ENDIAN, true);
    hard_assert(result == PICO_OK);
    pico_sha256_update_blocking(&state, buffer->data - 4, buffer->size + 4); // Also hash size field.
    pico_sha256_finish(&state, &buffer->hash);
}

void send_packet(PacketBuffer *buffer) {
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
