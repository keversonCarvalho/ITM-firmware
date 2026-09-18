cmake_minimum_required(VERSION 3.20)

project(itm_firmware VERSION 0.1.0 LANGUAGES C)

option(ITM_BUILD_TESTS "Build host tests" ON)
option(ITM_BUILD_STM32H723 "Build the STM32H723VGT6 target" OFF)
set(ITM_HARDWARE "HOST" CACHE STRING
    "Hardware target: HOST, ITM_REV00, or STM32H735G_DK")
set_property(CACHE ITM_HARDWARE PROPERTY STRINGS
    HOST ITM_REV00 STM32H735G_DK)

if(ITM_BUILD_STM32H723)
    if(NOT ITM_HARDWARE STREQUAL "HOST")
        message(FATAL_ERROR
            "Use either ITM_HARDWARE or legacy ITM_BUILD_STM32H723, not both")
    endif()
    set(ITM_HARDWARE "ITM_REV00")
    message(DEPRECATION
        "ITM_BUILD_STM32H723 is deprecated; use ITM_HARDWARE=ITM_REV00")
endif()

set(_itm_hardware_values HOST ITM_REV00 STM32H735G_DK)
if(NOT ITM_HARDWARE IN_LIST _itm_hardware_values)
    message(FATAL_ERROR
        "Invalid ITM_HARDWARE='${ITM_HARDWARE}'. Expected: ${_itm_hardware_values}")
endif()

find_program(ITM_DCF2C_EXECUTABLE NAMES dcf2c)
find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    add_custom_target(itm_generate_battery_dbc
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_battery_dbc.py"
                --write
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_custom_target(itm_check_battery_dbc
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_battery_dbc.py"
                --check
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
endif()

if(ITM_DCF2C_EXECUTABLE)
    set(ITM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(ITM_GENERATED_OD "${ITM_GENERATED_DIR}/itm_canopen_od.c")
    add_custom_command(
        OUTPUT "${ITM_GENERATED_OD}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ITM_GENERATED_DIR}"
        COMMAND "${ITM_DCF2C_EXECUTABLE}" --no-strings
                --output="${ITM_GENERATED_OD}"
                "${CMAKE_CURRENT_SOURCE_DIR}/config/canopen/itm-100.eds"
                itm_canopen_od
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/config/canopen/itm-100.eds"
        COMMENT "Generating the static Lely CANopen object dictionary"
        VERBATIM
    )
    add_custom_target(itm_lely_od DEPENDS "${ITM_GENERATED_OD}")
else()
    message(STATUS "Lely dcf2c not found; itm_lely_od target is unavailable")
endif()

add_library(itm_core STATIC
    generated/can/battery_dbc_generated.c
    src/app/cb_monitor.c
    src/app/power_control.c
    src/app/telemetry_catalog.c
    src/protocols/itm_dbcc.c
    src/services/bc_supervisor.c
    src/services/canopen_ret.c
    src/services/diagnostics.c
    src/services/persistence.c
    src/services/telemetry_ring.c
    src/services/telemetry_normalizer.c
    src/services/telemetry_store.c
)

target_include_directories(itm_core PUBLIC include generated/can)
target_compile_features(itm_core PUBLIC c_std_11)

if(MSVC)
    target_compile_options(itm_core PRIVATE /W4 /WX)
else()
    target_compile_options(itm_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(ITM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ITM_HARDWARE STREQUAL "ITM_REV00")
    add_subdirectory(platform/stm32h723)
elseif(ITM_HARDWARE STREQUAL "STM32H735G_DK")
    add_subdirectory(platform/stm32h735g_dk)
endif()
