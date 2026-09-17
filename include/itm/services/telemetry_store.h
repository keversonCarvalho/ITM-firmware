#ifndef ITM_SERVICES_TELEMETRY_STORE_H
#define ITM_SERVICES_TELEMETRY_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/app/telemetry_catalog.h"
#include "itm/core/result.h"
#include "itm/ports/critical_section.h"

typedef enum {
    ITM_SIGNAL_QUALITY_INVALID = 0,
    ITM_SIGNAL_QUALITY_VALID,
    ITM_SIGNAL_QUALITY_STALE
} itm_signal_quality_t;

typedef struct {
    int64_t value;
    uint32_t timestamp_ms;
    uint32_t update_counter;
    itm_signal_quality_t quality;
    bool has_timestamp;
} itm_signal_runtime_t;

typedef struct {
    uint32_t accepted_updates;
    uint32_t rejected_identifier;
    uint32_t rejected_type;
    uint32_t rejected_range;
    uint32_t rejected_timestamp;
    uint32_t rejected_source;
    uint32_t stale_transitions;
    uint32_t invalidations;
} itm_telemetry_statistics_t;

typedef struct {
    itm_signal_runtime_t values[ITM_SIGNAL_COUNT];
    uint32_t sequence;
    uint32_t captured_at_ms;
} itm_telemetry_snapshot_t;

typedef struct {
    itm_signal_runtime_t values[ITM_SIGNAL_COUNT];
    itm_telemetry_statistics_t statistics;
    uint32_t sequence;
    itm_critical_section_port_t critical;
} itm_telemetry_store_t;

typedef struct {
    void *context;
    itm_result_t (*capture)(void *context, uint32_t now_ms,
                            itm_telemetry_snapshot_t *snapshot);
} itm_telemetry_snapshot_port_t;

bool itm_telemetry_store_init(itm_telemetry_store_t *store,
                              itm_critical_section_port_t critical);
itm_result_t itm_telemetry_store_update(itm_telemetry_store_t *store,
                                        itm_signal_source_t producer,
                                        itm_signal_id_t id,
                                        itm_signal_type_t type, int64_t value,
                                        uint32_t timestamp_ms);
itm_result_t itm_telemetry_store_invalidate(itm_telemetry_store_t *store,
                                            itm_signal_source_t producer,
                                            itm_signal_id_t id,
                                            uint32_t timestamp_ms);
itm_result_t itm_telemetry_store_read(itm_telemetry_store_t *store,
                                      itm_signal_id_t id, uint32_t now_ms,
                                      itm_signal_runtime_t *value);
itm_result_t itm_telemetry_store_snapshot(itm_telemetry_store_t *store,
                                          uint32_t now_ms,
                                          itm_telemetry_snapshot_t *snapshot);
itm_telemetry_statistics_t
itm_telemetry_store_statistics(itm_telemetry_store_t *store);
itm_telemetry_snapshot_port_t
itm_telemetry_store_snapshot_port(itm_telemetry_store_t *store);
size_t itm_telemetry_snapshot_serialized_size(void);
itm_result_t itm_telemetry_snapshot_serialize(
    const itm_telemetry_snapshot_t *snapshot, uint8_t *data, size_t capacity,
    size_t *written);

#endif
