add_executable(itm_tests
    test_main.c
    test_battery_dbc.c
    test_bc_supervisor.c
    test_cb_monitor.c
    test_canopen_ret.c
    test_diagnostics.c
    test_persistence.c
    test_power_control.c
    test_telemetry_ring.c
    test_telemetry_normalizer.c
    test_telemetry_store.c
    mocks/mock_critical.c
    mocks/mock_flash.c
    mocks/mock_ports.c
)

target_link_libraries(itm_tests PRIVATE itm_core)
target_include_directories(itm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(itm_tests PRIVATE c_std_11)

if(MSVC)
    target_compile_options(itm_tests PRIVATE /W4 /WX)
else()
    target_compile_options(itm_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME itm_unit_tests COMMAND itm_tests)
