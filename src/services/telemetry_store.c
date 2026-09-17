#include "itm/services/telemetry_store.h"

#include <limits.h>
#include <string.h>

#define ITM_SERIALIZED_SIGNAL_SIZE 16U

static bool valid_id(itm_signal_id_t id)
{
    return ((int)id >= 0) && (id < ITM_SIGNAL_COUNT);
}

static bool timestamp_before(uint32_t candidate, uint32_t reference)
{
    return (int32_t)(candidate - reference) < 0;
}

static bool type_can_hold(itm_signal_type_t type, int64_t value)
{
    switch (type) {
    case ITM_SIGNAL_TYPE_I32:
        return (value >= INT32_MIN) && (value <= INT32_MAX);
    case ITM_SIGNAL_TYPE_U32:
        return (value >= 0) && ((uint64_t)value <= UINT32_MAX);
    case ITM_SIGNAL_TYPE_BOOL:
        return (value == 0) || (value == 1);
    default:
        return false;
    }
}

static void refresh_stale_locked(itm_telemetry_store_t *store,
                                 uint32_t now_ms)
{
    size_t index;

    for (index = 0U; index < ITM_SIGNAL_COUNT; ++index) {
        itm_signal_runtime_t *value = &store->values[index];
        const itm_signal_metadata_t *metadata =
            &itm_telemetry_catalog[index];
        if ((value->quality == ITM_SIGNAL_QUALITY_VALID) &&
            ((now_ms - value->timestamp_ms) > metadata->stale_timeout_ms)) {
            value->quality = ITM_SIGNAL_QUALITY_STALE;
            store->statistics.stale_transitions++;
            store->sequence++;
        }
    }
}

bool itm_telemetry_store_init(itm_telemetry_store_t *store,
                              itm_critical_section_port_t critical)
{
    if ((store == NULL) || (critical.enter == NULL) ||
        (critical.exit == NULL)) {
        return false;
    }
    (void)memset(store, 0, sizeof(*store));
    store->critical = critical;
    return true;
}

itm_result_t itm_telemetry_store_update(itm_telemetry_store_t *store,
                                        itm_signal_source_t producer,
                                        itm_signal_id_t id,
                                        itm_signal_type_t type, int64_t value,
                                        uint32_t timestamp_ms)
{
    const itm_signal_metadata_t *metadata;
    itm_signal_runtime_t *runtime;
    itm_critical_state_t state;

    if (store == NULL) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    state = store->critical.enter(store->critical.context);
    if (!valid_id(id)) {
        store->statistics.rejected_identifier++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    metadata = &itm_telemetry_catalog[id];
    runtime = &store->values[id];
    if (producer != metadata->source) {
        store->statistics.rejected_source++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if ((type != metadata->type) || !type_can_hold(type, value)) {
        store->statistics.rejected_type++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if ((value < metadata->minimum) || (value > metadata->maximum)) {
        store->statistics.rejected_range++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if (runtime->has_timestamp &&
        timestamp_before(timestamp_ms, runtime->timestamp_ms)) {
        store->statistics.rejected_timestamp++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }

    runtime->value = value;
    runtime->timestamp_ms = timestamp_ms;
    runtime->update_counter++;
    runtime->quality = ITM_SIGNAL_QUALITY_VALID;
    runtime->has_timestamp = true;
    store->statistics.accepted_updates++;
    store->sequence++;
    store->critical.exit(store->critical.context, state);
    return ITM_OK;
}

itm_result_t itm_telemetry_store_invalidate(itm_telemetry_store_t *store,
                                            itm_signal_source_t producer,
                                            itm_signal_id_t id,
                                            uint32_t timestamp_ms)
{
    itm_signal_runtime_t *runtime;
    itm_critical_state_t state;

    if (store == NULL) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    state = store->critical.enter(store->critical.context);
    if (!valid_id(id)) {
        store->statistics.rejected_identifier++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if (producer != itm_telemetry_catalog[id].source) {
        store->statistics.rejected_source++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    runtime = &store->values[id];
    if (runtime->has_timestamp &&
        timestamp_before(timestamp_ms, runtime->timestamp_ms)) {
        store->statistics.rejected_timestamp++;
        store->critical.exit(store->critical.context, state);
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    runtime->timestamp_ms = timestamp_ms;
    runtime->quality = ITM_SIGNAL_QUALITY_INVALID;
    runtime->has_timestamp = true;
    store->statistics.invalidations++;
    store->sequence++;
    store->critical.exit(store->critical.context, state);
    return ITM_OK;
}

itm_result_t itm_telemetry_store_read(itm_telemetry_store_t *store,
                                      itm_signal_id_t id, uint32_t now_ms,
                                      itm_signal_runtime_t *value)
{
    itm_critical_state_t state;

    if ((store == NULL) || (value == NULL) || !valid_id(id)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    state = store->critical.enter(store->critical.context);
    refresh_stale_locked(store, now_ms);
    *value = store->values[id];
    store->critical.exit(store->critical.context, state);
    return ITM_OK;
}

itm_result_t itm_telemetry_store_snapshot(itm_telemetry_store_t *store,
                                          uint32_t now_ms,
                                          itm_telemetry_snapshot_t *snapshot)
{
    itm_critical_state_t state;

    if ((store == NULL) || (snapshot == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    state = store->critical.enter(store->critical.context);
    refresh_stale_locked(store, now_ms);
    (void)memcpy(snapshot->values, store->values, sizeof(store->values));
    snapshot->sequence = store->sequence;
    snapshot->captured_at_ms = now_ms;
    store->critical.exit(store->critical.context, state);
    return ITM_OK;
}

itm_telemetry_statistics_t
itm_telemetry_store_statistics(itm_telemetry_store_t *store)
{
    itm_telemetry_statistics_t statistics = {0};
    itm_critical_state_t state;

    if (store == NULL) {
        return statistics;
    }
    state = store->critical.enter(store->critical.context);
    statistics = store->statistics;
    store->critical.exit(store->critical.context, state);
    return statistics;
}

static itm_result_t capture_from_port(void *context, uint32_t now_ms,
                                      itm_telemetry_snapshot_t *snapshot)
{
    return itm_telemetry_store_snapshot(context, now_ms, snapshot);
}

itm_telemetry_snapshot_port_t
itm_telemetry_store_snapshot_port(itm_telemetry_store_t *store)
{
    return (itm_telemetry_snapshot_port_t){store, capture_from_port};
}

size_t itm_telemetry_snapshot_serialized_size(void)
{
    return (size_t)ITM_SIGNAL_COUNT * ITM_SERIALIZED_SIGNAL_SIZE;
}

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)(value >> 8U);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFUL);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFUL);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFUL);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFUL);
}

static void write_i64_le(uint8_t *destination, int64_t value)
{
    const uint64_t encoded = (uint64_t)value;
    unsigned index;
    for (index = 0U; index < 8U; ++index) {
        destination[index] = (uint8_t)(encoded >> (index * 8U));
    }
}

itm_result_t itm_telemetry_snapshot_serialize(
    const itm_telemetry_snapshot_t *snapshot, uint8_t *data, size_t capacity,
    size_t *written)
{
    size_t index;
    size_t offset = 0U;
    const size_t required = itm_telemetry_snapshot_serialized_size();

    if ((snapshot == NULL) || (data == NULL) || (written == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < required) {
        return ITM_ERROR_NO_SPACE;
    }
    for (index = 0U; index < ITM_SIGNAL_COUNT; ++index) {
        const itm_signal_runtime_t *value = &snapshot->values[index];
        write_u16_le(&data[offset], (uint16_t)index);
        data[offset + 2U] = (uint8_t)itm_telemetry_catalog[index].type;
        data[offset + 3U] = (uint8_t)value->quality;
        write_u32_le(&data[offset + 4U], value->timestamp_ms);
        write_i64_le(&data[offset + 8U], value->value);
        offset += ITM_SERIALIZED_SIGNAL_SIZE;
    }
    *written = required;
    return ITM_OK;
}
