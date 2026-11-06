#include "test_framework.h"
bool test_update_rollback(void);
bool test_update_interruptions(void);
bool test_update_shared_metadata(void);
bool test_update_legacy_metadata_migration(void);
bool test_update_metadata_read_fallback(void);
bool test_update_canopen_configuration_journal(void);
bool test_update_rejects_invalid(void);
bool test_xcp_update(void);
bool test_n7space_wire(void);

bool test_bc_supervisor_trips_after_configured_attempts(void);
bool test_dbcc_decodes_independent_vectors(void);
bool test_dbcc_covers_all_dbc_messages(void);
bool test_dbcc_generated_pack_unpack_roundtrip(void);
bool test_dbcc_rejects_bad_frames_and_float_values(void);
bool test_dbcc_ingest_isolates_buses_and_timestamps(void);
bool test_dbcc_snapshot_contains_all_signals_from_both_batteries(void);
bool test_dbcc_statistics_and_counter_wrap(void);
bool test_ceb_tlm_decodes_full_snapshot(void);
bool test_ceb_tlm_rejects_corruption_and_resynchronizes(void);
bool test_itm_rs422_tlm_complete_frame(void);
bool test_itm_rs422_tlm_rejects_bad_values(void);
bool test_itm_dds_snapshot_contract(void);
bool test_itm_dds_power_cdr_contract(void);
bool test_itm_dds_power_rejects_malformed_command(void);
bool test_power_authority_prioritizes_bc_and_expires(void);
bool test_power_authority_rejects_stale_and_can_disable_bench(void);
bool test_bc_supervisor_recovers_on_valid_frame(void);
bool test_cb_monitor_debounces_and_matches_expected_state(void);
bool test_cb_monitor_reports_indeterminate_voltage(void);
bool test_cb_monitor_truth_table(void);
bool test_cb_monitor_invalid_sample_resets_debounce(void);
bool test_cb_voltage_nominal_scale_and_linear_limit(void);
bool test_canopen_identity_uses_registered_vendor_id(void);
bool test_canopen_exposes_raw_adc_read_only(void);
bool test_canopen_uses_itm_default_node_id(void);
bool test_canopen_rejects_identity_write(void);
bool test_canopen_persists_node_id_on_save_command(void);
bool test_canopen_validates_node_id_range(void);
bool test_canopen_management_from_nonvolatile_sources(void);
bool test_canopen_captures_telemetry_through_snapshot_port(void);
bool test_canopen_ceb_diagnostics_are_read_only(void);
bool test_canopen_ceb_snapshot_is_atomic_and_read_only(void);
bool test_canopen_full_snapshot_includes_both_batteries_and_ceb(void);
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
    failures += itm_run_test("N7 Space real SDO and heartbeat", test_n7space_wire);
    failures += itm_run_test("Firmware trial and rollback", test_update_rollback);
    failures += itm_run_test("Firmware interrupted writes", test_update_interruptions);
    failures += itm_run_test("Shared boot and configuration metadata", test_update_shared_metadata);
    failures += itm_run_test("Legacy boot journal migration", test_update_legacy_metadata_migration);
    failures += itm_run_test("Metadata unreadable-sector fallback", test_update_metadata_read_fallback);
    failures += itm_run_test("CANopen save into shared journal", test_update_canopen_configuration_journal);
    failures += itm_run_test("Firmware rejects invalid images", test_update_rejects_invalid);
    failures += itm_run_test("XCP programming frames", test_xcp_update);
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
    failures += itm_run_test("DBCC complete battery snapshots",
                             test_dbcc_snapshot_contains_all_signals_from_both_batteries);
    failures += itm_run_test("DBCC statistics wrap",
                             test_dbcc_statistics_and_counter_wrap);
    failures += itm_run_test("CEB serial complete snapshot",
                             test_ceb_tlm_decodes_full_snapshot);
    failures += itm_run_test("RS-422 70-signal frame",
                             test_itm_rs422_tlm_complete_frame);
    failures += itm_run_test("RS-422 rejects invalid values",
                             test_itm_rs422_tlm_rejects_bad_values);
    failures += itm_run_test("DDS snapshot CDR contract",
                             test_itm_dds_snapshot_contract);
    failures += itm_run_test("DDS power CDR contract",
                             test_itm_dds_power_cdr_contract);
    failures += itm_run_test("DDS power malformed command",
                             test_itm_dds_power_rejects_malformed_command);
    failures += itm_run_test("DDS power BC priority and lease",
                             test_power_authority_prioritizes_bc_and_expires);
    failures += itm_run_test("DDS power bench disable and stale commands",
                             test_power_authority_rejects_stale_and_can_disable_bench);
    failures += itm_run_test("CEB serial CRC and resynchronization",
                             test_ceb_tlm_rejects_corruption_and_resynchronizes);
    failures += itm_run_test("BC trips after attempts",
                             test_bc_supervisor_trips_after_configured_attempts);
    failures += itm_run_test("BC recovers on valid frame",
                             test_bc_supervisor_recovers_on_valid_frame);
    failures += itm_run_test("CB debounce and truth table",
                             test_cb_monitor_debounces_and_matches_expected_state);
    failures += itm_run_test("CB indeterminate voltage",
                             test_cb_monitor_reports_indeterminate_voltage);
    failures += itm_run_test("CB eight logic combinations",
                             test_cb_monitor_truth_table);
    failures += itm_run_test("CB invalid sample resets debounce",
                             test_cb_monitor_invalid_sample_resets_debounce);
    failures += itm_run_test("CB ADC nominal scaling and limit",
                             test_cb_voltage_nominal_scale_and_linear_limit);
    failures += itm_run_test("CANopen registered vendor identity",
                             test_canopen_identity_uses_registered_vendor_id);
    failures += itm_run_test("CANopen raw ADC diagnostic is read-only",
                             test_canopen_exposes_raw_adc_read_only);
    failures += itm_run_test("CANopen ITM default node-ID",
                             test_canopen_uses_itm_default_node_id);
    failures += itm_run_test("CANopen identity is read-only",
                             test_canopen_rejects_identity_write);
    failures += itm_run_test("CANopen persistent node-ID",
                             test_canopen_persists_node_id_on_save_command);
    failures += itm_run_test("CANopen node-ID range",
                             test_canopen_validates_node_id_range);
    failures += itm_run_test("CANopen management information",
                             test_canopen_management_from_nonvolatile_sources);
    failures += itm_run_test(
        "CANopen telemetry snapshot port",
        test_canopen_captures_telemetry_through_snapshot_port);
    failures += itm_run_test("CANopen CEB receive diagnostics",
                             test_canopen_ceb_diagnostics_are_read_only);
    failures += itm_run_test("CANopen CEB complete snapshot",
                             test_canopen_ceb_snapshot_is_atomic_and_read_only);
    failures += itm_run_test("CANopen all-source snapshot",
                             test_canopen_full_snapshot_includes_both_batteries_and_ceb);
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
