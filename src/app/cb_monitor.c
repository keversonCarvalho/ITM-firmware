#include "itm/app/cb_monitor.h"

#include <stddef.h>

static itm_cb_signal_t classify(const itm_cb_monitor_config_t *config,
                                uint16_t voltage_mv)
{
    if (voltage_mv < config->low_max_mv) {
        return ITM_CB_SIGNAL_LOW;
    }
    if (voltage_mv > config->high_min_mv) {
        return ITM_CB_SIGNAL_HIGH;
    }
    return ITM_CB_SIGNAL_INDETERMINATE;
}

bool itm_cb_monitor_init(itm_cb_monitor_t *monitor,
                         const itm_cb_monitor_config_t *config)
{
    if ((monitor == NULL) || (config == NULL) ||
        (config->low_max_mv >= config->high_min_mv) ||
        (config->stable_samples_required == 0U)) {
        return false;
    }

    monitor->config = *config;
    monitor->candidate_em1 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->candidate_em2 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->stable_em1 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->stable_em2 = ITM_CB_SIGNAL_INDETERMINATE;
    monitor->stable_count = 0U;
    monitor->has_stable_sample = false;
    return true;
}

itm_cb_monitor_status_t itm_cb_monitor_sample(itm_cb_monitor_t *monitor,
                                              uint16_t em1_mv,
                                              uint16_t em2_mv,
                                              bool commanded_on)
{
    itm_cb_signal_t em1;
    itm_cb_signal_t em2;
    bool expected_em1;
    bool expected_em2;

    if (monitor == NULL) {
        return ITM_CB_MONITOR_INVALID;
    }

    em1 = classify(&monitor->config, em1_mv);
    em2 = classify(&monitor->config, em2_mv);
    if ((em1 == ITM_CB_SIGNAL_INDETERMINATE) ||
        (em2 == ITM_CB_SIGNAL_INDETERMINATE)) {
        monitor->stable_count = 0U;
        monitor->has_stable_sample = false;
        return ITM_CB_MONITOR_INVALID;
    }

    if ((em1 != monitor->candidate_em1) ||
        (em2 != monitor->candidate_em2)) {
        monitor->candidate_em1 = em1;
        monitor->candidate_em2 = em2;
        monitor->stable_count = 1U;
    } else if (monitor->stable_count < monitor->config.stable_samples_required) {
        monitor->stable_count++;
    }

    if (monitor->stable_count < monitor->config.stable_samples_required) {
        return ITM_CB_MONITOR_PENDING;
    }

    monitor->stable_em1 = em1;
    monitor->stable_em2 = em2;
    monitor->has_stable_sample = true;
    expected_em1 = commanded_on ? monitor->config.expected_em1_when_on
                                : monitor->config.expected_em1_when_off;
    expected_em2 = commanded_on ? monitor->config.expected_em2_when_on
                                : monitor->config.expected_em2_when_off;

    return (((em1 == ITM_CB_SIGNAL_HIGH) == expected_em1) &&
            ((em2 == ITM_CB_SIGNAL_HIGH) == expected_em2))
               ? ITM_CB_MONITOR_MATCH
               : ITM_CB_MONITOR_MISMATCH;
}
