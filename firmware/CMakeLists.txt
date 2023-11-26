cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(PROJECT_NAME the_pico_sid)

project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Initialize the SDK

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

pico_sdk_init()

add_executable(${PROJECT_NAME}
  main.cpp /
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
  version.h
)

pico_generate_pio_header(${PROJECT_NAME}
	${CMAKE_CURRENT_LIST_DIR}/pio/write_sid_reg.pio
)

pico_generate_pio_header(${PROJECT_NAME}
	${CMAKE_CURRENT_LIST_DIR}/pio/read_sid_reg.pio
)

pico_generate_pio_header(${PROJECT_NAME}
	${CMAKE_CURRENT_LIST_DIR}/pio/dma_read.pio
)

# Make the stdio functions use USB rather than the serial pins
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# Pull in our pico_stdlib which pulls in commonly used features
# target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pio hardware_pwm hardware_dma hardware_timer hardware_clocks)
target_link_libraries(${PROJECT_NAME} pico_stdlib pico_multicore hardware_pio hardware_dma hardware_pwm hardware_adc)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
