#ifndef ITM_SERVICES_TELEMETRY_RING_H
#define ITM_SERVICES_TELEMETRY_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/app/telemetry_catalog.h"
#include "itm/ports/critical_section.h"
#include "itm/services/telemetry_store.h"

#define ITM_TELEMETRY_RING_CAPACITY 16U

typedef enum {
    ITM_RING_DROP_OLDEST = 0,
    ITM_RING_REJECT_NEWEST,
    ITM_RING_SIGNAL_FAULT
} itm_ring_full_policy_t;

typedef enum {
    ITM_RING_PUSHED = 0,
    ITM_RING_PUSHED_DROPPED_OLDEST,
    ITM_RING_REJECTED_FULL,
    ITM_RING_FAULT_FULL
} itm_ring_push_result_t;

typedef struct {
    itm_signal_id_t id;
    itm_signal_source_t source;
    itm_signal_type_t type;
    itm_signal_quality_t quality;
    int64_t value;
    uint32_t timestamp_ms;
} itm_telemetry_event_t;

typedef struct {
    uint32_t pushed;
    uint32_t popped;
    uint32_t overflows;
    uint32_t dropped_oldest;
    uint32_t rejected_newest;
    uint32_t fault_signals;
} itm_telemetry_ring_statistics_t;

typedef struct {
    itm_telemetry_event_t entries[ITM_TELEMETRY_RING_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    itm_ring_full_policy_t policy;
    itm_telemetry_ring_statistics_t statistics;
    itm_critical_section_port_t critical;
} itm_telemetry_ring_t;

bool itm_telemetry_ring_init(itm_telemetry_ring_t *ring,
                             itm_ring_full_policy_t policy,
                             itm_critical_section_port_t critical);
itm_ring_push_result_t itm_telemetry_ring_push(
    itm_telemetry_ring_t *ring, const itm_telemetry_event_t *event);
bool itm_telemetry_ring_pop(itm_telemetry_ring_t *ring,
                            itm_telemetry_event_t *event);
size_t itm_telemetry_ring_count(itm_telemetry_ring_t *ring);
itm_telemetry_ring_statistics_t
itm_telemetry_ring_statistics(itm_telemetry_ring_t *ring);

#endif
