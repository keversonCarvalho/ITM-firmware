#ifndef ITM_PROTOCOLS_ITM_DBCC_H
#define ITM_PROTOCOLS_ITM_DBCC_H

#include <stdbool.h>
#include <stdint.h>

#include "itm/core/result.h"
#include "itm/services/telemetry_store.h"

typedef enum {
    ITM_DBCC_BUS_BATT_28V = 0,
    ITM_DBCC_BUS_BATT_150V
} itm_dbcc_bus_t;

typedef struct {
    itm_dbcc_bus_t bus;
    uint32_t id;
    bool is_extended;
    uint8_t length;
    uint8_t data[8];
    uint32_t timestamp_ms;
} itm_dbcc_frame_t;

typedef enum {
    ITM_DBCC_MESSAGE_TEMPS = 0,
    ITM_DBCC_MESSAGE_V_TOTAL,
    ITM_DBCC_MESSAGE_CURRENT,
    ITM_DBCC_MESSAGE_ENERGY,
    ITM_DBCC_MESSAGE_CELL_VOLTAGE,
    ITM_DBCC_MESSAGE_BALANCE,
    ITM_DBCC_MESSAGE_STATUS,
    ITM_DBCC_MESSAGE_HUMIDITY
} itm_dbcc_message_t;

typedef struct {
    itm_dbcc_message_t message;
    int64_t values[6];
    uint8_t value_count;
} itm_dbcc_decoded_t;

typedef struct {
    uint32_t frames_received;
    uint32_t frames_decoded;
    uint32_t unknown_id;
    uint32_t invalid_frame_type;
    uint32_t invalid_dlc;
    uint32_t conversion_error;
    uint32_t invalid_value;
    uint32_t out_of_order_timestamp;
} itm_dbcc_statistics_t;

typedef struct {
    itm_telemetry_store_t *telemetry;
    itm_dbcc_statistics_t statistics;
} itm_dbcc_t;

bool itm_dbcc_init(itm_dbcc_t *dbcc, itm_telemetry_store_t *telemetry);
itm_result_t itm_dbcc_decode(const itm_dbcc_frame_t *frame,
                             itm_dbcc_decoded_t *decoded);
itm_result_t itm_dbcc_ingest(itm_dbcc_t *dbcc,
                             const itm_dbcc_frame_t *frame);
itm_dbcc_statistics_t itm_dbcc_statistics(const itm_dbcc_t *dbcc);

#endif
