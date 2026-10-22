enable_language(ASM)

set(STM32_CUBE_H7_PATH "$ENV{STM32_CUBE_H7_PATH}" CACHE PATH
    "Path to STM32Cube_FW_H7_V1.13.0")

if(NOT STM32_CUBE_H7_PATH)
    if(DEFINED ENV{USERPROFILE})
        set(_itm_cube_default
            "$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0")
        if(EXISTS "${_itm_cube_default}/Drivers")
            set(STM32_CUBE_H7_PATH "${_itm_cube_default}" CACHE PATH "" FORCE)
        endif()
    endif()
endif()

if(NOT EXISTS "${STM32_CUBE_H7_PATH}/Drivers/STM32H7xx_HAL_Driver")
    message(FATAL_ERROR
        "STM32CubeH7 V1.13.0 not found. Set STM32_CUBE_H7_PATH.")
endif()

set(STM32_HAL_DIR "${STM32_CUBE_H7_PATH}/Drivers/STM32H7xx_HAL_Driver")
set(STM32_CMSIS_DIR "${STM32_CUBE_H7_PATH}/Drivers/CMSIS")
set(STM32_DEVICE_DIR "${STM32_CMSIS_DIR}/Device/ST/STM32H7xx")

add_executable(itm_stm32h723.elf
    src/main.c
    src/system_clock.c
    src/itm_board.c
    src/itm_stm32_ports.c
    src/stm32h7xx_it.c
    "${STM32_DEVICE_DIR}/Source/Templates/system_stm32h7xx.c"
    "${STM32_DEVICE_DIR}/Source/Templates/gcc/startup_stm32h723xx.s"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_adc.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_adc_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_cortex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_flash.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_flash_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_gpio.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_pwr.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_rcc.c"
    "${STM32_HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c"
)

target_link_libraries(itm_stm32h723.elf PRIVATE itm_core)
target_include_directories(itm_stm32h723.elf PRIVATE
    include
    "${STM32_HAL_DIR}/Inc"
    "${STM32_CMSIS_DIR}/Include"
    "${STM32_DEVICE_DIR}/Include"
)
target_compile_definitions(itm_stm32h723.elf PRIVATE
    STM32H723xx
    USE_HAL_DRIVER
    HSE_VALUE=8000000U
)
target_compile_options(itm_stm32h723.elf PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Release>:-Os>
)

set(ITM_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32H723VGTX_FLASH.ld")
set(ITM_MAP_FILE "${CMAKE_CURRENT_BINARY_DIR}/itm_stm32h723.map")

target_link_options(itm_stm32h723.elf PRIVATE
    "-T${ITM_LINKER_SCRIPT}"
    "-Wl,-Map=${ITM_MAP_FILE}"
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -specs=nano.specs
    -specs=nosys.specs
)
set_target_properties(itm_stm32h723.elf PROPERTIES LINK_DEPENDS "${ITM_LINKER_SCRIPT}")

add_custom_command(TARGET itm_stm32h723.elf POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex
            "$<TARGET_FILE:itm_stm32h723.elf>" itm_stm32h723.hex
    COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:itm_stm32h723.elf>" itm_stm32h723.bin
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:itm_stm32h723.elf>"
    VERBATIM
)

find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if(POWERSHELL_EXECUTABLE)
    add_custom_target(verify_stm32_pinmap
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${PROJECT_SOURCE_DIR}/tools/verify_stm32_pinmap.ps1"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VERBATIM
    )
    add_dependencies(itm_stm32h723.elf verify_stm32_pinmap)
endif()

option(ITM_BUILD_UPDATE_BENCH "Build protected boot and A/B applications (CRC-only bench policy)" OFF)
if(ITM_BUILD_UPDATE_BENCH)
    # This explicit option does not claim authenticated or flight-qualified boot.
    set(update_common
        src/system_clock.c src/itm_board.c src/stm32h7xx_it.c src/update_platform.c
        "${STM32_DEVICE_DIR}/Source/Templates/system_stm32h7xx.c"
        "${STM32_DEVICE_DIR}/Source/Templates/gcc/startup_stm32h723xx.s")
    foreach(module hal hal_adc hal_adc_ex hal_cortex hal_flash hal_flash_ex hal_gpio hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_fdcan hal_dma hal_uart hal_uart_ex)
        list(APPEND update_common "${STM32_HAL_DIR}/Src/stm32h7xx_${module}.c")
    endforeach()
    foreach(image boot app_a app_b)
        if(image STREQUAL "boot")
            set(ITM_IMAGE_ORIGIN 0x08000000)
            set(ITM_IMAGE_LENGTH 128K)
            set(entry src/boot_main.c)
        else()
            set(ITM_IMAGE_LENGTH "256K - 512")
            set(entry src/main.c src/update_app.c src/itm_stm32_ports.c)
            if(image STREQUAL "app_a")
                set(ITM_IMAGE_ORIGIN 0x08060200)
                set(slot 0)
            else()
                set(ITM_IMAGE_ORIGIN 0x080A0200)
                set(slot 1)
            endif()
        endif()
        configure_file(linker/update.ld.in "${CMAKE_CURRENT_BINARY_DIR}/${image}.ld" @ONLY)
        set(target itm_${image}.elf)
        add_executable(${target} ${entry} ${update_common})
        target_include_directories(${target} PRIVATE include "${STM32_HAL_DIR}/Inc"
            "${STM32_CMSIS_DIR}/Include" "${STM32_DEVICE_DIR}/Include")
        target_compile_definitions(${target} PRIVATE STM32H723xx USE_HAL_DRIVER HSE_VALUE=8000000U ITM_UPDATE_BENCH_CRC_ONLY=1)
        if(NOT image STREQUAL "boot")
            target_compile_definitions(${target} PRIVATE ITM_UPDATE_APPLICATION=1 ITM_APP_SLOT=${slot})
        endif()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -Os -ffunction-sections -fdata-sections)
        target_link_libraries(${target} PRIVATE itm_core itm_canopen)
        target_link_options(${target} PRIVATE "-T${CMAKE_CURRENT_BINARY_DIR}/${image}.ld"
            "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${image}.map" -Wl,--gc-sections
            -Wl,--print-memory-usage -specs=nano.specs -specs=nosys.specs)
        set_target_properties(${target} PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${image}.ld")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${target}>" "${CMAKE_CURRENT_BINARY_DIR}/itm_${image}.bin"
            COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${target}>" "${CMAKE_CURRENT_BINARY_DIR}/itm_${image}.hex"
            COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${target}>" VERBATIM)
    endforeach()
endif()
