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
#define UART_ID uart1
#define BAUD_RATE 115200

int main() {
    stdio_init_all();

    // Set up our UART
    uart_init(uart0, BAUD_RATE);
    uart_init(uart1, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(0, GPIO_FUNC_UART); // UART0 TX
    gpio_set_function(1, GPIO_FUNC_UART); // UART0 RX
    gpio_set_function(4, GPIO_FUNC_UART); // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART); // UART1 RX

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART

    // Send out a string, with CR/LF conversions
    // uart_puts(UART_ID, " Hello, UART!\n");

    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    char text[256];
    char c;

    while (true) {
        c = getc(stdin);
        printf("Hello, World from USB '%c' HELP (%d)!\n", c, c);
        uart_puts(uart0, "Hello, World from uart0!\n");
        uart_putc(uart1, c);
        // sleep_ms(1000);
    }
}
