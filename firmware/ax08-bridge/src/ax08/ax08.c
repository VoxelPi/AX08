#include "ax08.h"

void ax08_init() {
    ax08_memory_init();
    ax08_sequencer_init();
    ax08_bridge_protocol_init();

    while (true) {
        const PacketBuffer *received_packet = ax08_bridge_read_packet();
        if (received_packet != NULL) {
            ax08_bridge_send_packet(received_packet);
        }
    }
}
