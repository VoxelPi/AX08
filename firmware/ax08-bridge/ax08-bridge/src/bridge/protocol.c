#include "protocol.h"

#include "hardware/dma.h"
#include "hardware/uart.h"

PacketBuffer packets[N_PACKET_BUFFER];

void send_packet(PacketBuffer *buffer) {
    int channel = dma_claim_unused_channel(true);

    dma_channel_config channel_config = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    channel_config_set_dreq(&channel_config, DREQ_UART0_TX);
    channel_config_set_read_increment(&channel_config, true);
    channel_config_set_write_increment(&channel_config, false);

    dma_channel_configure(
        channel,
        &channel_config,
        &uart_get_hw(uart0)->dr,
        buffer,
        buffer->size + sizeof(uint16_t) + sizeof(sha256_result_t),
        true
    );

    dma_channel_wait_for_finish_blocking(channel);

    dma_channel_unclaim(channel);
}
