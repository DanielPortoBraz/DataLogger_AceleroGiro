# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(DataLogger_IMU C CXX ASM)

pico_sdk_init()

# ----------------------
# Biblioteca: FatFs_SPI
# ----------------------
add_subdirectory(lib/FatFs_SPI)

# Biblioteca: lib_peripherals
add_library(peripherals STATIC
    lib_peripherals/ledrgb.c
    lib_peripherals/buzzer.c
    lib_peripherals/ssd1306.c
)
target_include_directories(peripherals PUBLIC
    lib_peripherals
)
target_link_libraries(peripherals
    hardware_pwm
    hardware_i2c
    hardware_adc
    pico_stdlib
)

# Biblioteca: lib_sensors
file(GLOB SENSOR_SOURCES "lib_sensors/*.c")

add_library(sensors STATIC
    ${SENSOR_SOURCES}
)
target_include_directories(sensors PUBLIC
    lib_sensors
)
target_link_libraries(sensors
    hardware_i2c
    hardware_adc
    pico_stdlib
)

# --------------------------
# Executável principal
# --------------------------
add_executable(DataLogger_IMU
    main.c
    hw_config.c
)

target_link_libraries(DataLogger_IMU
    FatFs_SPI
    peripherals
    sensors
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_pwm
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(DataLogger_IMU)
