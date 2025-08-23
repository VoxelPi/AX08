#include "ax08.h"

#include "util/buffer.h"

void ax08_init() {
    ax08_memory_init();
    ax08_sequencer_init();
    ax08_bridge_protocol_init();

    while (true) {
        const PacketBuffer *received_packet = ax08_bridge_read_packet();
        if (received_packet != NULL) {
            BufferReader reader;
            ax08_bridge_packet_init_reader(received_packet, &reader);

            uint8_t packet_id;
            buffer_read_uint8(&reader, &packet_id);

            switch (packet_id) {
            case 0:
                // Echo packet.
                ax08_bridge_send_packet(received_packet);
                break;

            case 1:
                // Info packet.
                PacketBuffer response;
                BufferWriter writer;
                ax08_bridge_packet_init_writer(&response, &writer);

                buffer_write_uint8(&writer, 1);
                buffer_write_uint32(&writer, AX08_PROTOCOL_VERSION);
                buffer_write_str16(&writer, AX08_VERSION);
                buffer_write_str16(&writer, AX08_GIT_VERSION);

                ax08_bridge_packet_close_writer(&response, &writer);
                ax08_bridge_packet_update_sha256(&response);
                ax08_bridge_send_packet(&response);
                break;

            default:
                ax08_bridge_send_packet(received_packet);
                break;
            }
        }
    }
}
