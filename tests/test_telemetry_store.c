#include "test_framework.h"
#include "mocks/mock_critical.h"
#include "itm/services/telemetry_ring.h"
#include "itm/services/telemetry_store.h"

#include <limits.h>
#include <string.h>

static itm_telemetry_store_t create_store(mock_critical_t *critical)
{
    itm_telemetry_store_t store;
    mock_critical_init(critical);
    (void)itm_telemetry_store_init(&store, mock_critical_port(critical));
    return store;
}

bool test_telemetry_update_read_and_validation(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store = create_store(&critical);
    itm_signal_runtime_t value;
    itm_telemetry_statistics_t statistics;

    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_AV_BATT,
                               ITM_SIGNAL_AV_BATT_VOLTAGE,
                               ITM_SIGNAL_TYPE_U32, 28000, 10U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_AV_BATT_VOLTAGE, 10U,
                               &value));
    TEST_ASSERT_EQ(28000, value.value);
    TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_VALID, value.quality);
    TEST_ASSERT_EQ(1U, value.update_counter);

    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_AV_BATT,
                       ITM_SIGNAL_AV_BATT_VOLTAGE, ITM_SIGNAL_TYPE_I32,
                       28000, 11U));
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_AV_BATT,
                       ITM_SIGNAL_AV_BATT_VOLTAGE, ITM_SIGNAL_TYPE_U32,
                       50000, 11U));
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_AV_BATT, ITM_SIGNAL_COUNT,
                       ITM_SIGNAL_TYPE_U32, 1, 11U));
    statistics = itm_telemetry_store_statistics(&store);
    TEST_ASSERT_EQ(1U, statistics.rejected_type);
    TEST_ASSERT_EQ(1U, statistics.rejected_range);
    TEST_ASSERT_EQ(1U, statistics.rejected_identifier);
    TEST_ASSERT_EQ(critical.enter_count, critical.exit_count);
    return true;
}

bool test_telemetry_timestamp_and_quality_transitions(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store = create_store(&critical);
    itm_signal_runtime_t value;

    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_STATE,
                               ITM_SIGNAL_TYPE_BOOL, 1, 100U));
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_STATE,
                       ITM_SIGNAL_TYPE_BOOL, 0, 99U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_CB_STATE, 201U, &value));
    TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_STALE, value.quality);
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_STATE,
                               ITM_SIGNAL_TYPE_BOOL, 0, 202U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_invalidate(
                               &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_STATE,
                               203U));
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_STATE,
                       ITM_SIGNAL_TYPE_BOOL, 1, 202U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_CB_STATE, 203U, &value));
    TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_INVALID, value.quality);
    return true;
}

bool test_telemetry_snapshot_consistency_and_producer_isolation(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store = create_store(&critical);
    itm_telemetry_snapshot_t snapshot;

    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_AV_BATT,
                               ITM_SIGNAL_AV_BATT_SOC,
                               ITM_SIGNAL_TYPE_U32, 7550, 20U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_CEB,
                               ITM_SIGNAL_CEB_TEMPERATURE,
                               ITM_SIGNAL_TYPE_I32, 4200, 20U));
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_store_update(
                       &store, ITM_SOURCE_CEB, ITM_SIGNAL_AV_BATT_SOC,
                       ITM_SIGNAL_TYPE_U32, 1, 21U));
    TEST_ASSERT_EQ(ITM_OK,
                   itm_telemetry_store_snapshot(&store, 20U, &snapshot));
    TEST_ASSERT_EQ(7550, snapshot.values[ITM_SIGNAL_AV_BATT_SOC].value);
    TEST_ASSERT_EQ(4200,
                   snapshot.values[ITM_SIGNAL_CEB_TEMPERATURE].value);
    TEST_ASSERT_EQ(store.sequence, snapshot.sequence);
    TEST_ASSERT_EQ(1U,
                   itm_telemetry_store_statistics(&store).rejected_source);
    TEST_ASSERT_EQ(1U, critical.maximum_active_count);
    return true;
}

bool test_telemetry_counter_and_timestamp_wraparound(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store = create_store(&critical);
    itm_signal_runtime_t value;

    store.values[ITM_SIGNAL_CB_FAULT].update_counter = UINT32_MAX;
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_FAULT,
                               ITM_SIGNAL_TYPE_BOOL, 0, UINT32_MAX - 2U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_CB, ITM_SIGNAL_CB_FAULT,
                               ITM_SIGNAL_TYPE_BOOL, 1, 2U));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_CB_FAULT, 2U, &value));
    TEST_ASSERT_EQ(1U, value.update_counter);
    TEST_ASSERT_EQ(2U, value.timestamp_ms);
    return true;
}

bool test_telemetry_serialization_is_deterministic(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store = create_store(&critical);
    itm_telemetry_snapshot_t snapshot;
    uint8_t serialized[ITM_SIGNAL_COUNT * 16U];
    uint8_t second[ITM_SIGNAL_COUNT * 16U];
    size_t written = 0U;
    size_t second_written = 0U;

    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &store, ITM_SOURCE_AV_BATT,
                               ITM_SIGNAL_AV_BATT_VOLTAGE,
                               ITM_SIGNAL_TYPE_U32, 0x1234, 0x01020304UL));
    TEST_ASSERT_EQ(ITM_OK,
                   itm_telemetry_store_snapshot(&store, 0x01020304UL,
                                                &snapshot));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_snapshot_serialize(
                               &snapshot, serialized, sizeof(serialized),
                               &written));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_snapshot_serialize(
                               &snapshot, second, sizeof(second),
                               &second_written));
    TEST_ASSERT_EQ(sizeof(serialized), written);
    TEST_ASSERT_EQ(written, second_written);
    TEST_ASSERT(memcmp(serialized, second, written) == 0);
    TEST_ASSERT_EQ(0x04U, serialized[4]);
    TEST_ASSERT_EQ(0x03U, serialized[5]);
    TEST_ASSERT_EQ(0x02U, serialized[6]);
    TEST_ASSERT_EQ(0x01U, serialized[7]);
    TEST_ASSERT_EQ(0x34U, serialized[8]);
    TEST_ASSERT_EQ(0x12U, serialized[9]);
    return true;
}

bool test_telemetry_static_memory_budget(void)
{
    printf("  RAM sizes (host): store=%zu snapshot=%zu ring=%zu catalog=%zu\n",
           sizeof(itm_telemetry_store_t), sizeof(itm_telemetry_snapshot_t),
           sizeof(itm_telemetry_ring_t), sizeof(itm_telemetry_catalog));
    TEST_ASSERT(sizeof(itm_telemetry_store_t) < 2048U);
    TEST_ASSERT(sizeof(itm_telemetry_snapshot_t) < 1024U);
    return true;
}
