enable_language(ASM)

set(STM32_CUBE_H7_PATH "$ENV{STM32_CUBE_H7_PATH}" CACHE PATH
    "Path to STM32Cube_FW_H7_V1.13.0")

if(NOT STM32_CUBE_H7_PATH AND DEFINED ENV{USERPROFILE})
    set(_itm_cube_default
        "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0")
    if(EXISTS "${_itm_cube_default}/Drivers")
        set(STM32_CUBE_H7_PATH "${_itm_cube_default}" CACHE PATH "" FORCE)
    endif()
endif()

if(NOT EXISTS "${STM32_CUBE_H7_PATH}/Drivers/STM32H7xx_HAL_Driver")
    message(FATAL_ERROR
        "STM32CubeH7 V1.13.0 not found. Set STM32_CUBE_H7_PATH.")
endif()

set(STM32_HAL_DIR "${STM32_CUBE_H7_PATH}/Drivers/STM32H7xx_HAL_Driver")
set(STM32_CMSIS_DIR "${STM32_CUBE_H7_PATH}/Drivers/CMSIS")
set(STM32_DEVICE_DIR "${STM32_CMSIS_DIR}/Device/ST/STM32H7xx")

add_executable(itm_stm32h735g_dk.elf
    src/main.c
    src/itm_dk_board.c
    src/itm_dk_ports.c
    src/stm32h7xx_it.c
    "${STM32_DEVICE_DIR}/Source/Templates/system_stm32h7xx.c"
    "${STM32_DEVICE_DIR}/Source/Templates/gcc/startup_stm32h735xx.s"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_cortex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_fdcan.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_flash.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_flash_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_gpio.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_pwr.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_rcc.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_uart.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_uart_ex.c"
)

target_link_libraries(itm_stm32h735g_dk.elf PRIVATE itm_core)
target_include_directories(itm_stm32h735g_dk.elf PRIVATE
    include
    "${STM32_HAL_DIR}/Inc"
    "${STM32_CMSIS_DIR}/Include"
    "${STM32_DEVICE_DIR}/Include"
)
target_compile_definitions(itm_stm32h735g_dk.elf PRIVATE
    STM32H735xx
    USE_HAL_DRIVER
    HSE_VALUE=25000000U
)
target_compile_options(itm_stm32h735g_dk.elf PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Release>:-Os>
)

set(ITM_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32H735IGKX_FLASH.ld")
set(ITM_MAP_FILE "${CMAKE_CURRENT_BINARY_DIR}/itm_stm32h735g_dk.map")

target_link_options(itm_stm32h735g_dk.elf PRIVATE
    "-T${ITM_LINKER_SCRIPT}"
    "-Wl,-Map=${ITM_MAP_FILE}"
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -specs=nano.specs
    -specs=nosys.specs
)
set_target_properties(itm_stm32h735g_dk.elf PROPERTIES
    LINK_DEPENDS "${ITM_LINKER_SCRIPT}")

add_custom_command(TARGET itm_stm32h735g_dk.elf POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex
            "$<TARGET_FILE:itm_stm32h735g_dk.elf>" itm_stm32h735g_dk.hex
    COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:itm_stm32h735g_dk.elf>" itm_stm32h735g_dk.bin
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:itm_stm32h735g_dk.elf>"
    VERBATIM
)

find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if(POWERSHELL_EXECUTABLE)
    add_custom_target(verify_stm32h735g_dk_pinmap
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${PROJECT_SOURCE_DIR}/tools/verify_stm32h735g_dk_pinmap.ps1"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VERBATIM
    )
    add_dependencies(itm_stm32h735g_dk.elf verify_stm32h735g_dk_pinmap)
endif()
