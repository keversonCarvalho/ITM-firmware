#ifndef ITM_APP_CB_MONITOR_H
#define ITM_APP_CB_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ITM_CB_SIGNAL_LOW = 0,
    ITM_CB_SIGNAL_HIGH,
    ITM_CB_SIGNAL_INDETERMINATE
} itm_cb_signal_t;

typedef enum {
    ITM_CB_MONITOR_PENDING = 0,
    ITM_CB_MONITOR_MATCH,
    ITM_CB_MONITOR_MISMATCH,
    ITM_CB_MONITOR_INVALID
} itm_cb_monitor_status_t;

typedef struct {
    uint16_t low_max_mv;
    uint16_t high_min_mv;
    uint8_t stable_samples_required;
    bool expected_em1_when_off;
    bool expected_em2_when_off;
    bool expected_em1_when_on;
    bool expected_em2_when_on;
} itm_cb_monitor_config_t;

typedef struct {
    itm_cb_monitor_config_t config;
    itm_cb_signal_t candidate_em1;
    itm_cb_signal_t candidate_em2;
    itm_cb_signal_t stable_em1;
    itm_cb_signal_t stable_em2;
    uint8_t stable_count;
    bool has_stable_sample;
} itm_cb_monitor_t;

bool itm_cb_monitor_init(itm_cb_monitor_t *monitor,
                         const itm_cb_monitor_config_t *config);
itm_cb_monitor_status_t itm_cb_monitor_sample(itm_cb_monitor_t *monitor,
                                              uint16_t em1_mv,
                                              uint16_t em2_mv,
                                              bool commanded_on);

#endif
