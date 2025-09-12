# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ax08-bridge C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Make it available as a define to the compiler
add_compile_definitions(AX08_GIT_VERSION=\"${GIT_VERSION}\")

# Add executable.
add_executable(
    ax08-bridge
    src/main.c
    src/ax08/ax08.c
    src/ax08/memory/memory_unit.c
    src/ax08/bridge/protocol.c
    src/ax08/sequencer/sequencer.c
    src/ax08/util/buffer.c
)

pico_set_program_name(ax08-bridge "ax08-bridge")
pico_set_program_version(ax08-bridge "0.1")

# Generate PIO header
pico_generate_pio_header(ax08-bridge ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)
pico_generate_pio_header(ax08-bridge ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ax08-bridge 0)
pico_enable_stdio_usb(ax08-bridge 0)

# Add the standard library to the build
target_link_libraries(ax08-bridge
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(ax08-bridge PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(ax08-bridge
        hardware_dma
        hardware_pio
        hardware_sha256
        pico_multicore
        pico_sha256
        )

pico_add_extra_outputs(ax08-bridge)
