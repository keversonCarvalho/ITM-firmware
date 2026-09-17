cmake_minimum_required(VERSION 3.20)

project(itm_firmware VERSION 0.1.0 LANGUAGES C)

option(ITM_BUILD_TESTS "Build host tests" ON)

add_library(itm_core STATIC
    src/app/cb_monitor.c
    src/app/power_control.c
    src/services/bc_supervisor.c
    src/services/diagnostics.c
    src/services/persistence.c
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
