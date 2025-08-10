#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

// Data will be copied from src to dst
const char src[] = "Hello, world! (from DMA)";
char dst[count_of(src)];

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
// #define UART_ID uart1
#define BAUD_RATE 115200

// void uart0_rx_irq_handler() {
//     while (uart_is_readable(uart0)) {
//         uint8_t data = uart_getc(uart0);
//         if (uart_is_writable(uart1)) {
//             uart_putc_raw(uart1, data);
//         }
//     }
// }

void uart1_rx_irq_handler() {
    while (uart_is_readable(uart1)) {
        uint8_t data = uart_getc(uart1);
        if (uart_is_writable(uart0)) {
            uart_putc_raw(uart0, data);
        }
    }
}

int main() {
    stdio_init_all();

    // Set up our UART
    uart_init(uart0, BAUD_RATE);
    uart_init(uart1, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(0, GPIO_FUNC_UART); // UART0 TX
    gpio_set_function(1, GPIO_FUNC_UART); // UART0 RX
    gpio_set_function(2, GPIO_FUNC_UART); // UART0 CTS (Clear To Send, Pico INPUT, active low)
    gpio_set_function(3, GPIO_FUNC_UART); // UART0 RTS (Request To Send, Pico OUTPUT, active low)

    gpio_set_function(4, GPIO_FUNC_UART); // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART); // UART1 RX

    uart_set_hw_flow(uart0, true, true);
    uart_set_hw_flow(uart1, false, false);
    // uart_set_fifo_enabled(uart0, true);
    // uart_set_fifo_enabled(uart1, false);

    // irq_set_exclusive_handler(UART0_IRQ, uart0_rx_irq_handler);
    irq_set_exclusive_handler(UART1_IRQ, uart1_rx_irq_handler);

    // irq_set_enabled(UART0_IRQ, true);
    irq_set_enabled(UART1_IRQ, true);

    // uart_set_irqs_enabled(uart0, true, false);
    uart_set_irqs_enabled(uart1, true, false);

    char c;
    while (true) {
        if (uart_is_readable(uart0)) {
            c = uart_getc(uart0);
            uart_putc_raw(uart1, c);
        }
        // c = getc(stdin);
        // printf("Hello, World from USB '%c' HELP (%d)!\n", c, c);
        // uart_puts(uart0, "Hello, World from uart0!\n");
        // uart_putc(uart1, c);
        // uart_putc_raw(uart1, c);
        sleep_ms(1);
    }
}
