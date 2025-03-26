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
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

set(PROJECT_NAME the_pico_sid)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(the_pico_sid C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(the_pico_sid main.cpp /
    pico_sid.cpp /
    pico_sid.h /
    sid_voice.cpp /
    sid_voice.h /
    sid_wave.cpp /
    sid_wave.h /
    sid_envelope.cpp /
    sid_envelope.h /
    sid_dac.cpp /
    sid_dac.h /
    sid_filter.cpp /
    sid_filter.h /
    sid_extfilter.cpp /
    sid_extfilter.h /
    version.h 
)

pico_set_program_name(the_pico_sid "The Pico SID")

# Generate PIO header
pico_generate_pio_header(the_pico_sid ${CMAKE_CURRENT_LIST_DIR}/pio/dma_read.pio)
pico_generate_pio_header(the_pico_sid ${CMAKE_CURRENT_LIST_DIR}/pio/read_sid_reg.pio)
pico_generate_pio_header(the_pico_sid ${CMAKE_CURRENT_LIST_DIR}/pio/write_sid_reg.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(the_pico_sid 1)
pico_enable_stdio_usb(the_pico_sid 0)

# Add the standard library to the build
target_link_libraries(the_pico_sid
    pico_stdlib pico_multicore hardware_pio hardware_dma hardware_pwm hardware_adc hardware_flash)

# Add the standard include files to the build
target_include_directories(the_pico_sid PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(the_pico_sid)

