#ifndef ITM_SERVICES_CANOPEN_RET_H
#define ITM_SERVICES_CANOPEN_RET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"
#include "itm/services/persistence.h"

#define ITM_CANOPEN_VENDOR_ID 0x000006A9UL
#define ITM_CANOPEN_BIT_RATE 500000UL
#define ITM_CANOPEN_TELEMETRY_PERIOD_MS 10U

#define ITM_OD_DEVICE_TYPE 0x1000U
#define ITM_OD_ERROR_REGISTER 0x1001U
#define ITM_OD_DEVICE_NAME 0x1008U
#define ITM_OD_SOFTWARE_VERSION 0x100AU
#define ITM_OD_STORE_PARAMETERS 0x1010U
#define ITM_OD_HEARTBEAT_PRODUCER 0x1017U
#define ITM_OD_IDENTITY 0x1018U

/* Manufacturer-specific indices are provisional until the RET ICD is issued. */
#define ITM_OD_CONFIGURATION_VERSION 0x2000U
#define ITM_OD_NODE_ID 0x2001U
#define ITM_OD_TELEMETRY_PERIOD 0x2002U

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

typedef struct {
    itm_canopen_identity_t identity;
    itm_canopen_persistent_config_t config;
    itm_persistence_t *persistence;
    uint8_t error_register;
    bool configuration_dirty;
} itm_canopen_ret_t;

bool itm_canopen_ret_init(itm_canopen_ret_t *service,
                          itm_canopen_identity_t identity,
                          itm_canopen_persistent_config_t defaults,
                          itm_persistence_t *persistence);
itm_sdo_abort_t itm_canopen_ret_sdo_read(const itm_canopen_ret_t *service,
                                         uint16_t index, uint8_t subindex,
                                         uint8_t *data, size_t capacity,
                                         size_t *size);
itm_sdo_abort_t itm_canopen_ret_sdo_write(itm_canopen_ret_t *service,
                                          uint16_t index, uint8_t subindex,
                                          const uint8_t *data, size_t size);
uint32_t itm_canopen_pack_revision(uint8_t major, uint8_t minor,
                                  uint8_t patch);

#endif
