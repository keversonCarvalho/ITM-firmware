#ifndef ITM_APP_TELEMETRY_CATALOG_H
#define ITM_APP_TELEMETRY_CATALOG_H

#include <stdint.h>

typedef enum {
    ITM_SIGNAL_AV_BATT_VOLTAGE = 0,
    ITM_SIGNAL_AV_BATT_SOC,
    ITM_SIGNAL_ACT_BATT_VOLTAGE,
    ITM_SIGNAL_ACT_BATT_SOC,
    ITM_SIGNAL_CEB_TEMPERATURE,
    ITM_SIGNAL_CEB_STATUS,
    ITM_SIGNAL_CB_STATE,
    ITM_SIGNAL_CB_FAULT,
    ITM_SIGNAL_COUNT
} itm_signal_id_t;

typedef enum {
    ITM_SIGNAL_TYPE_I32 = 0,
    ITM_SIGNAL_TYPE_U32,
    ITM_SIGNAL_TYPE_BOOL
} itm_signal_type_t;

typedef enum {
    ITM_UNIT_NONE = 0,
    ITM_UNIT_VOLT,
    ITM_UNIT_PERCENT,
    ITM_UNIT_CELSIUS,
    ITM_UNIT_STATE
} itm_signal_unit_t;

typedef enum {
    ITM_SOURCE_AV_BATT = 0,
    ITM_SOURCE_ACT_BATT,
    ITM_SOURCE_CEB,
    ITM_SOURCE_CB
} itm_signal_source_t;

typedef struct {
    itm_signal_id_t id;
    itm_signal_type_t type;
    itm_signal_unit_t unit;
    itm_signal_source_t source;
    int8_t decimal_exponent;
    int64_t minimum;
    int64_t maximum;
    uint32_t stale_timeout_ms;
} itm_signal_metadata_t;

extern const itm_signal_metadata_t
    itm_telemetry_catalog[ITM_SIGNAL_COUNT];

#endif
