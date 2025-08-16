#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "ax08/memory_unit.h"
#include "bridge/protocol.h"

#define BAUD_RATE 115200

// uint64_t timestamp_last_rx = 0;

// void uart0_rx_irq_handler() {
//     timestamp_last_rx = time_us_64();
// }

// void uart1_rx_irq_handler() {
//     while (uart_is_readable(uart1)) {
//         uint8_t data = uart_getc(uart1);
//         if (uart_is_writable(uart0)) {
//             uart_putc_raw(uart0, data);
//         }
//     }
// }

int main() {
    stdio_init_all();

    // Set up our UART
    uart_init(uart1, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(4, GPIO_FUNC_UART); // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART); // UART1 RX

    // uart_set_hw_flow(uart1, false, false);
    // uart_set_fifo_enabled(uart1, false);

    // irq_set_exclusive_handler(UART1_IRQ, uart1_rx_irq_handler);
    // irq_set_enabled(UART1_IRQ, true);
    // uart_set_irqs_enabled(uart1, true, false);

    // Initialize bridge protocol module.
    bridge_protocol_init();

    while (true) {
        const PacketBuffer *received_packet = bridge_read_packet();
        if (received_packet != NULL) {
            bridge_send_packet(received_packet);
        }
    }
}
