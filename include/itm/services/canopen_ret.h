#ifndef ITM_SERVICES_CANOPEN_RET_H
#define ITM_SERVICES_CANOPEN_RET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"
#include "itm/services/persistence.h"
#include "itm/services/telemetry_store.h"

#define ITM_CANOPEN_VENDOR_ID 0x000006A9UL
#define ITM_CANOPEN_BIT_RATE 500000UL
#define ITM_CANOPEN_DEFAULT_NODE_ID 10U
#define ITM_CANOPEN_TELEMETRY_PERIOD_MS 10U

#define ITM_OD_DEVICE_TYPE 0x1000U
#define ITM_OD_ERROR_REGISTER 0x1001U
#define ITM_OD_DEVICE_NAME 0x1008U
#define ITM_OD_SOFTWARE_VERSION 0x100AU
#define ITM_OD_STORE_PARAMETERS 0x1010U
#define ITM_OD_HEARTBEAT_PRODUCER 0x1017U
#define ITM_OD_IDENTITY 0x1018U

/* Manufacturer-specific indices are provisional until the RET ICD is issued. */
#define ITM_OD_CONFIGURATION_VERSION 0x2100U
#define ITM_OD_NODE_ID 0x2101U
#define ITM_OD_TELEMETRY_PERIOD 0x2102U
#define ITM_OD_HARDWARE_PART_NUMBER 0x2107U
#define ITM_OD_FIRMWARE_PART_NUMBER 0x2108U
#define ITM_OD_HARDWARE_UID 0x2109U
#define ITM_OD_FIRMWARE_CRC 0x210AU
#define ITM_OD_CEB_DIAGNOSTICS 0x2110U
#define ITM_OD_CEB_SNAPSHOT 0x2111U
#define ITM_OD_FULL_SNAPSHOT 0x2112U
#define ITM_OD_BENCH_POWER_COMMAND 0x2120U
#define ITM_OD_BENCH_POWER_STATE 0x2121U
#define ITM_OD_BENCH_DEBOUNCE_MS 0x2122U
#define ITM_OD_BENCH_CEB_DELAY_MS 0x2123U
#define ITM_OD_BENCH_CB_TIMEOUT_MS 0x2124U
#define ITM_OD_BENCH_EM1_MV 0x2125U
#define ITM_OD_BENCH_EM2_MV 0x2126U
#define ITM_OD_BENCH_CB_CONDITION 0x2127U
#define ITM_CEB_DIAGNOSTIC_COUNT 7U

typedef enum {
    ITM_SDO_ABORT_NONE = 0x00000000UL,
    ITM_SDO_ABORT_TOGGLE = 0x05030000UL,
    ITM_SDO_ABORT_UNSUPPORTED_ACCESS = 0x06010000UL,
    ITM_SDO_ABORT_READ_ONLY = 0x06010002UL,
    ITM_SDO_ABORT_OBJECT_NOT_FOUND = 0x06020000UL,
    ITM_SDO_ABORT_TYPE_MISMATCH = 0x06070010UL,
    ITM_SDO_ABORT_VALUE_RANGE = 0x06090030UL,
    ITM_SDO_ABORT_SUBINDEX_NOT_FOUND = 0x06090011UL,
    ITM_SDO_ABORT_HARDWARE = 0x06060000UL
} itm_sdo_abort_t;

typedef struct {
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
} itm_canopen_identity_t;

typedef struct {
    uint8_t node_id;
    uint16_t heartbeat_producer_ms;
    uint32_t configuration_version;
} itm_canopen_persistent_config_t;

typedef bool (*itm_canopen_save_config_t)(void *context,
    const itm_canopen_persistent_config_t *config);
typedef bool (*itm_canopen_read_ceb_diagnostic_t)(void *context,
    uint8_t subindex, uint32_t *value);
typedef uint32_t (*itm_canopen_telemetry_clock_t)(void *context);
typedef bool (*itm_canopen_power_read_t)(void *context, uint16_t index,
                                         uint32_t *value);
typedef itm_sdo_abort_t (*itm_canopen_power_write_t)(void *context,
    uint16_t index, uint32_t value);

typedef struct {
    itm_canopen_identity_t identity;
    itm_canopen_persistent_config_t config;
    itm_persistence_t *persistence;
    itm_canopen_save_config_t save_config;
    void *save_config_context;
    uint8_t hardware_uid[12];
    uint32_t firmware_version;
    uint32_t firmware_crc;
    itm_telemetry_snapshot_port_t telemetry_snapshot;
    itm_canopen_telemetry_clock_t telemetry_clock;
    void *telemetry_clock_context;
    itm_canopen_read_ceb_diagnostic_t read_ceb_diagnostic;
    void *ceb_diagnostic_context;
    itm_canopen_power_read_t power_read;
    itm_canopen_power_write_t power_write;
    void *power_context;
    uint8_t error_register;
    bool configuration_dirty;
} itm_canopen_ret_t;

bool itm_canopen_ret_init(itm_canopen_ret_t *service,
                          itm_canopen_identity_t identity,
                          itm_canopen_persistent_config_t defaults,
                          itm_persistence_t *persistence);
bool itm_canopen_ret_bind_config_store(itm_canopen_ret_t *service,
                                       itm_canopen_save_config_t save,
                                       void *context);
bool itm_canopen_ret_set_management(itm_canopen_ret_t *service,
                                    const uint8_t uid[12],
                                    uint32_t firmware_version,
                                    uint32_t firmware_crc);
itm_sdo_abort_t itm_canopen_ret_sdo_read(const itm_canopen_ret_t *service,
                                         uint16_t index, uint8_t subindex,
                                         uint8_t *data, size_t capacity,
                                         size_t *size);
itm_sdo_abort_t itm_canopen_ret_sdo_write(itm_canopen_ret_t *service,
                                          uint16_t index, uint8_t subindex,
                                          const uint8_t *data, size_t size);
uint32_t itm_canopen_pack_revision(uint8_t major, uint8_t minor,
                                  uint8_t patch);
bool itm_canopen_ret_bind_telemetry(
    itm_canopen_ret_t *service,
    itm_telemetry_snapshot_port_t telemetry_snapshot);
itm_result_t itm_canopen_ret_capture_telemetry(
    itm_canopen_ret_t *service, uint32_t now_ms,
    itm_telemetry_snapshot_t *snapshot);
bool itm_canopen_ret_bind_telemetry_clock(
    itm_canopen_ret_t *service, itm_canopen_telemetry_clock_t clock,
    void *context);
bool itm_canopen_ret_bind_ceb_diagnostics(
    itm_canopen_ret_t *service, itm_canopen_read_ceb_diagnostic_t read,
    void *context);
bool itm_canopen_ret_bind_power_bench(itm_canopen_ret_t *service,
    itm_canopen_power_read_t read, itm_canopen_power_write_t write,
    void *context);

#endif
