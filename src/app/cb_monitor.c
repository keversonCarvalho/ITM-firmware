#include "itm/app/cb_monitor.h"

#include <stddef.h>

static itm_cb_signal_t classify(const itm_cb_monitor_config_t *config,
                                uint16_t voltage_mv)
{
    if (voltage_mv <= config->low_max_mv) {
        return ITM_CB_SIGNAL_LOW;
    }
    if (voltage_mv >= config->high_min_mv) {
        return ITM_CB_SIGNAL_HIGH;
    }
    return ITM_CB_SIGNAL_INDETERMINATE;
}

bool itm_cb_monitor_init(itm_cb_monitor_t *monitor,
                         const itm_cb_monitor_config_t *config)
{
    if ((monitor == NULL) || (config == NULL) ||
        (config->low_max_mv >= config->high_min_mv) ||
        (config->debounce_ms == 0U)) {
        return false;
    }

    monitor->config = *config;
    monitor->candidate_em1 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->candidate_em2 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->stable_em1 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->stable_em2 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->candidate_commanded_on = false;
    monitor->candidate_since_ms = 0U;
    monitor->has_stable_sample = false;
    monitor->condition = ITM_CB_CONDITION_PENDING;
    return true;
}

void itm_cb_monitor_invalidate(itm_cb_monitor_t *monitor)
{
    if (monitor == NULL) return;
    monitor->candidate_em1 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->candidate_em2 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->has_stable_sample = false;
    monitor->condition = ITM_CB_CONDITION_INVALID;
}

itm_cb_monitor_status_t itm_cb_monitor_sample(itm_cb_monitor_t *monitor,
                                              uint16_t em1_mv,
                                              uint16_t em2_mv,
                                              bool commanded_on,
                                              uint32_t now_ms)
{
    itm_cb_signal_t em1;
    itm_cb_signal_t em2;

    if (monitor == NULL) {
        return ITM_CB_MONITOR_INVALID;
    }

    em1 = classify(&monitor->config, em1_mv);
    em2 = classify(&monitor->config, em2_mv);
    if ((em1 == ITM_CB_SIGNAL_INDETERMINATE) ||
        (em2 == ITM_CB_SIGNAL_INDETERMINATE)) {
        itm_cb_monitor_invalidate(monitor);
        return ITM_CB_MONITOR_INVALID;
    }

    if ((commanded_on != monitor->candidate_commanded_on) ||
        (em1 != monitor->candidate_em1) ||
        (em2 != monitor->candidate_em2)) {
        monitor->candidate_commanded_on = commanded_on;
        monitor->candidate_em1 = em1;
        monitor->candidate_em2 = em2;
        monitor->candidate_since_ms = now_ms;
        monitor->has_stable_sample = false;
        monitor->condition = ITM_CB_CONDITION_PENDING;
    }

    if ((uint32_t)(now_ms - monitor->candidate_since_ms) <
        monitor->config.debounce_ms) {
        return ITM_CB_MONITOR_PENDING;
    }

    monitor->stable_em1 = em1;
    monitor->stable_em2 = em2;
    monitor->has_stable_sample = true;
    if (em1 == ITM_CB_SIGNAL_LOW)
        monitor->condition = em2 == ITM_CB_SIGNAL_LOW
            ? ITM_CB_CONDITION_BATTERY_DISCONNECTED
            : ITM_CB_CONDITION_MONITOR_FAULT;
    else if (commanded_on)
        monitor->condition = em2 == ITM_CB_SIGNAL_HIGH
            ? ITM_CB_CONDITION_NORMAL : ITM_CB_CONDITION_STUCK_OFF;
    else
        monitor->condition = em2 == ITM_CB_SIGNAL_LOW
            ? ITM_CB_CONDITION_NORMAL : ITM_CB_CONDITION_STUCK_ON;
    return monitor->condition == ITM_CB_CONDITION_NORMAL
        ? ITM_CB_MONITOR_MATCH : ITM_CB_MONITOR_MISMATCH;
}
