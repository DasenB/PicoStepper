cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(picostepper VERSION 1.0.0)
 
# Select files to compile
add_executable(picostepper)

target_include_directories(picostepper PRIVATE 
        src
        src/picostepper
        src/picostepper/driver
)

target_sources(picostepper PRIVATE 
        src/main.c 
        src/picostepper/picostepper.c
)

target_link_libraries(picostepper PRIVATE 
        pico_stdlib
        hardware_pio
        hardware_dma
)
 
# Initalise the SDK
pico_sdk_init()
 
# Enable USB, UART output
pico_enable_stdio_usb(picostepper 1)
pico_enable_stdio_uart(picostepper 1)
 
# Compile the PIO-programs and include it into the project
pico_add_extra_outputs(picostepper)
pico_generate_pio_header(picostepper ${CMAKE_CURRENT_LIST_DIR}/src/picostepper/driver/four_wire.pio)

