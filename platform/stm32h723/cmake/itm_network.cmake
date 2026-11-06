set(_itm_freertos "${STM32_CUBE_H7_PATH}/Middlewares/Third_Party/FreeRTOS/Source")
set(_itm_lwip "${STM32_CUBE_H7_PATH}/Middlewares/Third_Party/LwIP")
set(_itm_xrce "${PROJECT_SOURCE_DIR}/third_party/microxrcedds_client")
set(_itm_cdr "${PROJECT_SOURCE_DIR}/third_party/microcdr")

add_custom_target(itm_verify_xrce_source
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/verify_xrce_source.py"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" VERBATIM)

add_library(itm_freertos STATIC
    "${_itm_freertos}/tasks.c" "${_itm_freertos}/queue.c"
    "${_itm_freertos}/list.c" "${_itm_freertos}/timers.c"
    "${_itm_freertos}/event_groups.c" "${_itm_freertos}/stream_buffer.c"
    "${_itm_freertos}/portable/MemMang/heap_4.c"
    "${_itm_freertos}/portable/GCC/ARM_CM7/r0p1/port.c")
target_include_directories(itm_freertos PUBLIC
    "${_itm_freertos}/include"
    "${_itm_freertos}/portable/GCC/ARM_CM7/r0p1"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${STM32_CMSIS_DIR}/Include" "${STM32_DEVICE_DIR}/Include")
target_compile_definitions(itm_freertos PUBLIC STM32H723xx)

file(GLOB _itm_lwip_core "${_itm_lwip}/src/core/*.c"
    "${_itm_lwip}/src/core/ipv4/*.c" "${_itm_lwip}/src/api/*.c"
    "${_itm_lwip}/src/netif/ppp/*.c")
add_library(itm_lwip STATIC ${_itm_lwip_core}
    "${_itm_lwip}/src/netif/ethernet.c"
    "${_itm_lwip}/contrib/ports/freertos/sys_arch.c")
target_include_directories(itm_lwip PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${_itm_lwip}/src/include"
    "${_itm_lwip}/contrib/ports/freertos/include"
    "${_itm_lwip}/system")
target_link_libraries(itm_lwip PUBLIC itm_freertos)
target_compile_definitions(itm_lwip PRIVATE
    mem_free=lwip_mem_free mem_malloc=lwip_mem_malloc
    mem_calloc=lwip_mem_calloc)

add_library(itm_microcdr STATIC
    "${_itm_cdr}/src/c/common.c" "${_itm_cdr}/src/c/types/basic.c"
    "${_itm_cdr}/src/c/types/string.c" "${_itm_cdr}/src/c/types/array.c"
    "${_itm_cdr}/src/c/types/sequence.c")
target_include_directories(itm_microcdr PUBLIC
    "${_itm_cdr}/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(_itm_xrce_sources
    core/session/stream/input_best_effort_stream.c
    core/session/stream/input_reliable_stream.c
    core/session/stream/output_best_effort_stream.c
    core/session/stream/output_reliable_stream.c
    core/session/stream/stream_storage.c
    core/session/stream/stream_id.c
    core/session/stream/seq_num.c
    core/session/session.c core/session/session_info.c
    core/session/submessage.c core/session/object_id.c
    core/serialization/xrce_types.c core/serialization/xrce_header.c
    core/serialization/xrce_subheader.c
    util/ping.c core/session/common_create_entities.c
    core/session/create_entities_ref.c core/session/create_entities_xml.c
    core/session/create_entities_bin.c core/session/read_access.c
    core/session/write_access.c
    profile/transport/custom/custom_transport.c)
list(TRANSFORM _itm_xrce_sources PREPEND "${_itm_xrce}/src/c/")
add_library(itm_xrce STATIC ${_itm_xrce_sources} src/uxr_time.c)
target_include_directories(itm_xrce PUBLIC
    "${_itm_xrce}/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(itm_xrce PRIVATE
    "${STM32_HAL_DIR}/Inc" "${STM32_CMSIS_DIR}/Include"
    "${STM32_DEVICE_DIR}/Include")
target_compile_definitions(itm_xrce PRIVATE STM32H723xx USE_HAL_DRIVER)
target_link_libraries(itm_xrce PUBLIC itm_microcdr)
add_dependencies(itm_xrce itm_verify_xrce_source)
