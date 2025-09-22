#include "ax08.h"

#include "util/buffer.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#define PACKET_ID_ECHO 0x00
#define PACKET_ID_INFO 0x01
#define PACKET_ID_UPLOAD_PROGRAM_START 0x10
#define PACKET_ID_UPLOAD_PROGRAM_CHUNK 0x11
#define PACKET_ID_UPLOAD_PROGRAM_END 0x12

void ax08_init() {
    ax08_memory_init();
    ax08_sequencer_init();
    ax08_bridge_protocol_init();

    // Turn on state led.gpio_put(PICO_DEFAULT_LED_PIN, led_on);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    while (true) {
        const PacketBuffer *received_packet = ax08_bridge_read_packet();

        if (received_packet != NULL) {
            BufferReader reader;
            ax08_bridge_packet_init_reader(received_packet, &reader);

            uint8_t packet_id;
            buffer_read_uint8(&reader, &packet_id);

            switch (packet_id) {
            case PACKET_ID_ECHO:
                // Echo packet.
                ax08_bridge_send_packet(received_packet);
                break;

            case PACKET_ID_INFO:
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

            case PACKET_ID_UPLOAD_PROGRAM_START:
                ax08_memory_program_handle_upload_start(&reader);
                break;

            case PACKET_ID_UPLOAD_PROGRAM_CHUNK:
                ax08_memory_program_handle_upload_chunk(&reader);
                break;

            case PACKET_ID_UPLOAD_PROGRAM_END:
                ax08_memory_program_handle_upload_end(&reader);
                break;

            default:
                ax08_bridge_send_packet(received_packet);
                break;
            }
        }
    }
}
