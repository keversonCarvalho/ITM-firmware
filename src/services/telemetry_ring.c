#include "itm/services/telemetry_ring.h"

#include <string.h>

bool itm_telemetry_ring_init(itm_telemetry_ring_t *ring,
                             itm_ring_full_policy_t policy,
                             itm_critical_section_port_t critical)
{
    if ((ring == NULL) || (critical.enter == NULL) ||
        (critical.exit == NULL) || (policy > ITM_RING_SIGNAL_FAULT)) {
        return false;
    }
    (void)memset(ring, 0, sizeof(*ring));
    ring->policy = policy;
    ring->critical = critical;
    return true;
}

itm_ring_push_result_t itm_telemetry_ring_push(
    itm_telemetry_ring_t *ring, const itm_telemetry_event_t *event)
{
    itm_critical_state_t state;
    itm_ring_push_result_t result = ITM_RING_PUSHED;

    if ((ring == NULL) || (event == NULL)) {
        return ITM_RING_FAULT_FULL;
    }
    state = ring->critical.enter(ring->critical.context);
    if (ring->count == ITM_TELEMETRY_RING_CAPACITY) {
        ring->statistics.overflows++;
        if (ring->policy == ITM_RING_REJECT_NEWEST) {
            ring->statistics.rejected_newest++;
            ring->critical.exit(ring->critical.context, state);
            return ITM_RING_REJECTED_FULL;
        }
        if (ring->policy == ITM_RING_SIGNAL_FAULT) {
            ring->statistics.fault_signals++;
            ring->critical.exit(ring->critical.context, state);
            return ITM_RING_FAULT_FULL;
        }
        ring->tail = (ring->tail + 1U) % ITM_TELEMETRY_RING_CAPACITY;
        ring->count--;
        ring->statistics.dropped_oldest++;
        result = ITM_RING_PUSHED_DROPPED_OLDEST;
    }

    ring->entries[ring->head] = *event;
    ring->head = (ring->head + 1U) % ITM_TELEMETRY_RING_CAPACITY;
    ring->count++;
    ring->statistics.pushed++;
    ring->critical.exit(ring->critical.context, state);
    return result;
}

bool itm_telemetry_ring_pop(itm_telemetry_ring_t *ring,
                            itm_telemetry_event_t *event)
{
    itm_critical_state_t state;

    if ((ring == NULL) || (event == NULL)) {
        return false;
    }
    state = ring->critical.enter(ring->critical.context);
    if (ring->count == 0U) {
        ring->critical.exit(ring->critical.context, state);
        return false;
    }
    *event = ring->entries[ring->tail];
    ring->tail = (ring->tail + 1U) % ITM_TELEMETRY_RING_CAPACITY;
    ring->count--;
    ring->statistics.popped++;
    ring->critical.exit(ring->critical.context, state);
    return true;
}

size_t itm_telemetry_ring_count(itm_telemetry_ring_t *ring)
{
    size_t count = 0U;
    itm_critical_state_t state;

    if (ring == NULL) {
        return 0U;
    }
    state = ring->critical.enter(ring->critical.context);
    count = ring->count;
    ring->critical.exit(ring->critical.context, state);
    return count;
}

itm_telemetry_ring_statistics_t
itm_telemetry_ring_statistics(itm_telemetry_ring_t *ring)
{
    itm_telemetry_ring_statistics_t statistics = {0};
    itm_critical_state_t state;

    if (ring == NULL) {
        return statistics;
    }
    state = ring->critical.enter(ring->critical.context);
    statistics = ring->statistics;
    ring->critical.exit(ring->critical.context, state);
    return statistics;
}
