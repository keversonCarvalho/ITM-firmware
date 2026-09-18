#include "test_framework.h"

bool test_bc_supervisor_trips_after_configured_attempts(void);
bool test_dbcc_decodes_independent_vectors(void);
bool test_dbcc_covers_all_dbc_messages(void);
bool test_dbcc_generated_pack_unpack_roundtrip(void);
bool test_dbcc_rejects_bad_frames_and_float_values(void);
bool test_dbcc_ingest_isolates_buses_and_timestamps(void);
bool test_dbcc_statistics_and_counter_wrap(void);
bool test_bc_supervisor_recovers_on_valid_frame(void);
bool test_cb_monitor_debounces_and_matches_expected_state(void);
bool test_cb_monitor_reports_indeterminate_voltage(void);
bool test_canopen_identity_uses_registered_vendor_id(void);
bool test_canopen_uses_itm_default_node_id(void);
bool test_canopen_rejects_identity_write(void);
bool test_canopen_persists_node_id_on_save_command(void);
bool test_canopen_validates_node_id_range(void);
bool test_canopen_captures_telemetry_through_snapshot_port(void);
bool test_diagnostics_keeps_recent_events(void);
bool test_persistence_uses_latest_valid_slot(void);
bool test_persistence_survives_interrupted_write(void);
bool test_power_control_sequences_outputs(void);
bool test_power_control_fails_safe(void);
bool test_telemetry_update_read_and_validation(void);
bool test_telemetry_kv_contract_uses_static_keys_and_values(void);
bool test_telemetry_timestamp_and_quality_transitions(void);
bool test_telemetry_snapshot_consistency_and_producer_isolation(void);
bool test_telemetry_counter_and_timestamp_wraparound(void);
bool test_telemetry_serialization_is_deterministic(void);
bool test_telemetry_static_memory_budget(void);
bool test_telemetry_ring_drops_oldest(void);
bool test_telemetry_ring_rejects_newest(void);
bool test_telemetry_ring_signals_fault(void);
bool test_telemetry_normalizer_uses_integer_affine_rule(void);

int itm_run_test(const char *name, itm_test_fn_t function)
{
    const bool passed = function();
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);
    return passed ? 0 : 1;
}

int main(void)
{
    int failures = 0;
    failures += itm_run_test("DBCC independent vectors",
                             test_dbcc_decodes_independent_vectors);
    failures += itm_run_test("DBCC all DBC messages",
                             test_dbcc_covers_all_dbc_messages);
    failures += itm_run_test("DBCC generated roundtrip",
                             test_dbcc_generated_pack_unpack_roundtrip);
    failures += itm_run_test("DBCC invalid frames and floats",
                             test_dbcc_rejects_bad_frames_and_float_values);
    failures += itm_run_test("DBCC bus and timestamp isolation",
                             test_dbcc_ingest_isolates_buses_and_timestamps);
    failures += itm_run_test("DBCC statistics wrap",
                             test_dbcc_statistics_and_counter_wrap);
    failures += itm_run_test("BC trips after attempts",
                             test_bc_supervisor_trips_after_configured_attempts);
    failures += itm_run_test("BC recovers on valid frame",
                             test_bc_supervisor_recovers_on_valid_frame);
    failures += itm_run_test("CB debounce and truth table",
                             test_cb_monitor_debounces_and_matches_expected_state);
    failures += itm_run_test("CB indeterminate voltage",
                             test_cb_monitor_reports_indeterminate_voltage);
    failures += itm_run_test("CANopen registered vendor identity",
                             test_canopen_identity_uses_registered_vendor_id);
    failures += itm_run_test("CANopen ITM default node-ID",
                             test_canopen_uses_itm_default_node_id);
    failures += itm_run_test("CANopen identity is read-only",
                             test_canopen_rejects_identity_write);
    failures += itm_run_test("CANopen persistent node-ID",
                             test_canopen_persists_node_id_on_save_command);
    failures += itm_run_test("CANopen node-ID range",
                             test_canopen_validates_node_id_range);
    failures += itm_run_test(
        "CANopen telemetry snapshot port",
        test_canopen_captures_telemetry_through_snapshot_port);
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
    failures += itm_run_test("telemetry update and validation",
                             test_telemetry_update_read_and_validation);
    failures += itm_run_test("telemetry static KV contract",
                             test_telemetry_kv_contract_uses_static_keys_and_values);
    failures += itm_run_test("telemetry quality transitions",
                             test_telemetry_timestamp_and_quality_transitions);
    failures += itm_run_test(
        "telemetry consistent snapshot and producer isolation",
        test_telemetry_snapshot_consistency_and_producer_isolation);
    failures += itm_run_test("telemetry counter and timestamp wraparound",
                             test_telemetry_counter_and_timestamp_wraparound);
    failures += itm_run_test("telemetry deterministic serialization",
                             test_telemetry_serialization_is_deterministic);
    failures += itm_run_test("telemetry static memory budget",
                             test_telemetry_static_memory_budget);
    failures += itm_run_test("telemetry ring drops oldest",
                             test_telemetry_ring_drops_oldest);
    failures += itm_run_test("telemetry ring rejects newest",
                             test_telemetry_ring_rejects_newest);
    failures += itm_run_test("telemetry ring signals fault",
                             test_telemetry_ring_signals_fault);
    failures += itm_run_test("telemetry integer normalization",
                             test_telemetry_normalizer_uses_integer_affine_rule);

    printf("\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
