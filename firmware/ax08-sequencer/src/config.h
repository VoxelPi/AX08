#pragma once

#define _XTAL_FREQ 32000000

/*
    PIN MAPPING:

    RA0, IN:  MODE              (active high)
    RA1, IN:  toggle debug mode (button, active low)
    RA2, IN:  run instruction   (button, active low)
    RA3, IN:  run step          (button, active low)
    RA4, IN:  cycle clock speed (button, active low)
    RA5, IN:  UART CTS          (currently unused)
    RA6, OUT: UART RTS          (currently unused)
    RA7, IN:  BREAK             (active low)

    RB0, OUT: state PC_SOURCE_INC
    RB1, IN:  UART RX
    RB2, OUT: UART TX
    RB3, OUT: state FREEZE_WRD
    RB4, OUT: state FREEZE_OP
    RB5, OUT: state HOLD
    RB6, OUT: state AD_RGSET_OVERRIDE
    RB7, OUT: state STORE
*/
#define PIN_FREEZE_WORD LATB3
#define PIN_FREEZE_OPCODE LATB4
#define PIN_HOLD_OUTPUT LATB5
#define PIN_INCREMENT_PC LATB0
#define PIN_STORE_PC LATB6
#define PIN_STORE_OUTPUT LATB7

#define PIN_BREAK RA7
#define PIN_ENABLE RA0



#pragma region feature toggles

// Enable the uart command system.
#define FEATURE_UART_COMMANDS



#pragma region bugfixes

// Fix https://github.com/VoxelPi/AX08/issues/2
// Only relevant on the AX08L boards.
#define BUGFIX_SKIP_BREAK_STORE
