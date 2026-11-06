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

find_package(Python3 COMPONENTS Interpreter REQUIRED)
include(cmake/n7space.cmake)

if(Python3_Interpreter_FOUND)
    add_custom_target(itm_verify_n7space_source
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_n7space_source.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_dependencies(itm_n7space itm_verify_n7space_source)
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

add_library(itm_core STATIC
    generated/can/battery_dbc_generated.c
    src/app/cb_monitor.c
    src/app/cb_voltage.c
    src/app/power_control.c
    src/app/telemetry_catalog.c
    src/protocols/itm_dbcc.c
    src/protocols/ceb_tlm.c
    src/protocols/itm_rs422_tlm.c
    src/protocols/itm_dds_snapshot.c
    src/protocols/itm_dds_power.c
    src/services/bc_supervisor.c
    src/services/canopen_ret.c
    src/services/diagnostics.c
    src/services/power_authority.c
    src/services/persistence.c
    src/services/firmware_update.c
    src/protocols/xcp_update.c
    src/services/telemetry_ring.c
    src/services/telemetry_normalizer.c
    src/services/telemetry_store.c
)

add_library(itm_canopen STATIC src/protocols/n7space.c)
target_include_directories(itm_canopen PUBLIC include PRIVATE generated/canopen)
target_link_libraries(itm_canopen PUBLIC itm_core PRIVATE itm_n7space)
if(Python3_Interpreter_FOUND)
    add_custom_target(itm_n7space_od
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/generate_n7space_od.py"
        VERBATIM)
    add_custom_target(itm_check_n7space_od
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/generate_n7space_od.py" --check
        VERBATIM)
endif()

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
