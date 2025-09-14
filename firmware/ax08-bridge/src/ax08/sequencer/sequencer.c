#include "sequencer.h"

#include "hardware/uart.h"
#include "pico/stdlib.h"

#define AX08_SEQUENCER_BAUD_RATE 115200

void ax08_sequencer_init() {
    // Set up our UART.
    uart_init(uart1, AX08_SEQUENCER_BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO.
    gpio_set_function(4, GPIO_FUNC_UART); // UART1 TX
    gpio_set_function(5, GPIO_FUNC_UART); // UART1 RX
    gpio_set_function(6, GPIO_FUNC_UART); // UART1 CTS
    gpio_set_function(7, GPIO_FUNC_UART); // UART1 RTS
}
