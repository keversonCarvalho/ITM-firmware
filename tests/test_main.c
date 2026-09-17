#include "test_framework.h"

bool test_bc_supervisor_trips_after_configured_attempts(void);
bool test_bc_supervisor_recovers_on_valid_frame(void);
bool test_cb_monitor_debounces_and_matches_expected_state(void);
bool test_cb_monitor_reports_indeterminate_voltage(void);
bool test_diagnostics_keeps_recent_events(void);
bool test_persistence_uses_latest_valid_slot(void);
bool test_persistence_survives_interrupted_write(void);
bool test_power_control_sequences_outputs(void);
bool test_power_control_fails_safe(void);

int itm_run_test(const char *name, itm_test_fn_t function)
{
    const bool passed = function();
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);
    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;
    failures += itm_run_test("BC trips after attempts",
                             test_bc_supervisor_trips_after_configured_attempts);
    failures += itm_run_test("BC recovers on valid frame",
                             test_bc_supervisor_recovers_on_valid_frame);
    failures += itm_run_test("CB debounce and truth table",
                             test_cb_monitor_debounces_and_matches_expected_state);
    failures += itm_run_test("CB indeterminate voltage",
                             test_cb_monitor_reports_indeterminate_voltage);
    failures += itm_run_test("diagnostic event history",
                             test_diagnostics_keeps_recent_events);
    failures += itm_run_test("persistence latest slot",
                             test_persistence_uses_latest_valid_slot);
    failures += itm_run_test("persistence interrupted write",
                             test_persistence_survives_interrupted_write);
    failures += itm_run_test("power sequencing",
                             test_power_control_sequences_outputs);
    failures += itm_run_test("power fail-safe",
                             test_power_control_fails_safe);

    printf("\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
