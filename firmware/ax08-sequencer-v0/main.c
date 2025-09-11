// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset disabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF      // PLL Enable (4x PLL disabled)
#pragma config STVREN = OFF     // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will not cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>

#define _XTAL_FREQ 32000000

#define BASE_DELAY 12                             // minimum memory packet propagation delay
#define HALF_DELAY ((unsigned int)(BASE_DELAY/2)) // calculated half propagation delay
#define BOARD_DELAY 2                             // minimum board logic propagation delay
#define MS_DELAY_LONG 25

/*
    PIN MAPPING:
        RA0: MODE (in)
        RA1: ACTION (in)
        RA2: CYCLE (in)
        RA3: STEP (in)
        RA4: DELAY (in)
        RA5, RA6: UART RTS (in), UART CTS (out)
        RA7: BREAK (in)

        RB1, RB2: UART RX (in), UART TX (out)
        RB0: PC_SOURCE_INC (out)
        RB3: FREEZE_WRD (out)
        RB4: FREEZE_OP (out)
        RB5: HOLD (out)
        RB6: AD_RGSET_OVERRIDE (out)
        RB7: STORE (out)
*/

void stepdelay() {
    __delay_us(BOARD_DELAY);
    if ((PORTA & 0b00010000) == 0) { //DELAY check
        __delay_ms(MS_DELAY_LONG);
    }
    return;
}

void packetdelay() {
    __delay_us(BASE_DELAY);
    return;
}

void halfpacketdelay() {
    __delay_us(HALF_DELAY);
    return;
}

int main() {

    // Setup the system clock.
    OSCCONbits.SPLLEN = 1;    // Enable 4x PLL.
    OSCCONbits.IRCF = 0b1110; // 32 MHz clock frequency.
    while (!OSCSTATbits.PLLR) // Wait for the clock to be initialized.
        ;

    // Enable global weak pull ups.
    OPTION_REGbits.nWPUEN = false;

    // Initialize A pins.
    PORTA  = 0b00000000;
    LATA   = 0b00000000;
    ANSELA = 0b00000000;
    WPUA   = 0b00100000;
    TRISA  = 0b11111111;

    // Initialize B pins.
    PORTB  = 0b00000000;
    LATB   = 0b00000000;
    ANSELB = 0b00000000;
    WPUB   = 0b00000000;
    TRISB  = 0b00000010;

    uint8_t state = 0;
    uint8_t prev_mode = 0;

    __delay_ms(500);
    RB6 = 1;
    __delay_ms(1);
    RB6 = 0;
    __delay_ms(10);
    RB6 = 1;
    __delay_ms(1);
    RB6 = 0;
    __delay_ms(100);

    // Main loop
    while (true) {
        if ((PORTA & 0b00000001) == 0) { // MODE check
            LATB = LATB & 0b00000100;
            state = 0;

            if (prev_mode == 1) {
                __delay_ms(100);
                RB6 = 1;
                __delay_ms(1);
                RB6 = 0;
                __delay_ms(100);
            }

            if ((PORTA & 0b00000010) == 0) { // ACTION button check
                __delay_ms(50);
                if ((PORTA & 0b00000010) != 0) goto act0_end;
                while ((PORTA & 0b00000010) == 0);
                __delay_ms(50);
                if ((PORTA & 0b00000010) == 0) goto act0_end;

                RB6 = 1;
                __delay_ms(1);
                RB6 = 0;
                __delay_ms(10);
            }
            act0_end:

            prev_mode = 0;
        }
        else {
            if (((PORTA & 0b10000000) == 0) && (state != 0)) { // BREAK check
                __delay_ms(50);
                if ((PORTA & 0b10000000) != 0) goto brk0_end;

                state = 0;
                __delay_ms(100);
            }
            brk0_end:

            if (prev_mode == 0) {
                __delay_ms(500);
                prev_mode = 1;
            }

            if (state == 0) {
                stpmode_start:
                if ((PORTA & 0b00000010) == 0) { // ACTION button check
                    __delay_ms(50);
                    if ((PORTA & 0b00000010) != 0) goto stpmode_start;
                    while ((PORTA & 0b00000010) == 0);
                    __delay_ms(50);
                    if ((PORTA & 0b00000010) == 0) goto stpmode_start;

                    state = 1;
                }
                else if ((PORTA & 0b00000100) == 0) { // CYCLE button check
                    __delay_ms(50);
                    if ((PORTA & 0b00000100) != 0) goto stpmode_start;
                    while ((PORTA & 0b00000100) == 0);
                    __delay_ms(50);
                    if ((PORTA & 0b00000100) == 0) goto stpmode_start;

                    state = 2;
                }
                else if ((PORTA & 0b00001000) == 0) { // STEP button check
                    __delay_ms(50);
                    if ((PORTA & 0b00001000) != 0) goto stpmode_start;
                    while ((PORTA & 0b00001000) == 0);
                    __delay_ms(50);
                    if ((PORTA & 0b00001000) == 0) goto stpmode_start;

                    state = 3;
                }
            }
            if (state == 1) {
                LATB = LATB & 0b00000100;

                LATB = LATB | 0b00001000;
                stepdelay();
                halfpacketdelay();

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00010000;
                stepdelay();
                halfpacketdelay();

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00100001;
                stepdelay();

                LATB = LATB & 0b00000101;
                LATB = LATB | 0b01000000;
                stepdelay();

                LATB = LATB & 0b00000100;
                stepdelay();

                LATB = LATB | 0b10000000;
                stepdelay();
                packetdelay();

                act1_start:
                if ((PORTA & 0b00000010) == 0) { // ACTION button check
                    __delay_ms(50);
                    if ((PORTA & 0b00000010) != 0) goto act1_start;
                    while ((PORTA & 0b00000010) == 0);
                    __delay_ms(50);
                    if ((PORTA & 0b00000010) == 0) goto act1_start;

                    state = 0;
                }

                LATB = LATB & 0b00000100;
            }
            else if (state == 2) {
                LATB = LATB & 0b00000100;

                LATB = LATB | 0b00001000;
                stepdelay();
                halfpacketdelay();

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00010000;
                stepdelay();
                halfpacketdelay();

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00100001;
                stepdelay();

                LATB = LATB & 0b00000101;
                LATB = LATB | 0b01000000;
                stepdelay();

                LATB = LATB & 0b00000100;
                stepdelay();

                LATB = LATB | 0b10000000;
                stepdelay();
                packetdelay();

                LATB = LATB & 0b00000100;

                state = 0;
            }
            else if (state == 3) {
                LATB = LATB & 0b00000100;

                LATB = LATB | 0b00001000;
                __delay_us(BASE_DELAY);

                uint8_t loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00010000;
                __delay_us(BASE_DELAY);

                loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB & 0b00000100;
                LATB = LATB | 0b00100001;
                __delay_us(BASE_DELAY);

                loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB & 0b00000101;
                LATB = LATB | 0b01000000;
                __delay_us(BASE_DELAY);

                loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB & 0b00000100;
                __delay_us(BASE_DELAY);

                loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB | 0b10000000;
                __delay_us(BASE_DELAY);

                loopvar = 0;
                while (loopvar == 0) {
                    if ((PORTA & 0b00001000) == 0) { // STEP button check
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) != 0) continue;
                        while ((PORTA & 0b00001000) == 0);
                        __delay_ms(50);
                        if ((PORTA & 0b00001000) == 0) continue;

                        loopvar = 1;
                    }
                }

                LATB = LATB & 0b00000100;
                state = 0;
            }
            else {
                //debug
            }
        }
    }
}
