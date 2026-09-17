cmake_minimum_required(VERSION 3.20)

project(itm_firmware VERSION 0.1.0 LANGUAGES C)

option(ITM_BUILD_TESTS "Build host tests" ON)

find_program(ITM_DCF2C_EXECUTABLE NAMES dcf2c)

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
    src/app/cb_monitor.c
    src/app/power_control.c
    src/app/telemetry_catalog.c
    src/services/bc_supervisor.c
    src/services/canopen_ret.c
    src/services/diagnostics.c
    src/services/persistence.c
    src/services/telemetry_ring.c
    src/services/telemetry_normalizer.c
    src/services/telemetry_store.c
)

target_include_directories(itm_core PUBLIC include)
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
