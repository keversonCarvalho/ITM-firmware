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

typedef enum {
    ITM_CB_CONDITION_PENDING = 0,
    ITM_CB_CONDITION_NORMAL,
    ITM_CB_CONDITION_BATTERY_DISCONNECTED,
    ITM_CB_CONDITION_MONITOR_FAULT,
    ITM_CB_CONDITION_STUCK_ON,
    ITM_CB_CONDITION_STUCK_OFF,
    ITM_CB_CONDITION_INVALID
} itm_cb_condition_t;

typedef struct {
    uint16_t low_max_mv;
    uint16_t high_min_mv;
    uint16_t debounce_ms;
} itm_cb_monitor_config_t;

typedef struct {
    itm_cb_monitor_config_t config;
    itm_cb_signal_t candidate_em1;
    itm_cb_signal_t candidate_em2;
    itm_cb_signal_t stable_em1;
    itm_cb_signal_t stable_em2;
    uint32_t candidate_since_ms;
    bool has_stable_sample;
    itm_cb_condition_t condition;
} itm_cb_monitor_t;

bool itm_cb_monitor_init(itm_cb_monitor_t *monitor,
                         const itm_cb_monitor_config_t *config);
itm_cb_monitor_status_t itm_cb_monitor_sample(itm_cb_monitor_t *monitor,
                                              uint16_t em1_mv,
                                              uint16_t em2_mv,
                                              bool commanded_on,
                                              uint32_t now_ms);

#endif
