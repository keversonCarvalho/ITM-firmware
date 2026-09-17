#include "itm/services/canopen_ret.h"

#include <string.h>

#define ITM_CANOPEN_CONFIG_SCHEMA 1UL
#define ITM_CANOPEN_STORE_SIGNATURE 0x65766173UL
#define ITM_CANOPEN_CONFIG_SIZE 7U

static const char device_name[] = "ITM-100";
static const char software_version[] = "1.2.0";

static void encode_config(const itm_canopen_persistent_config_t *config,
                          uint8_t data[ITM_CANOPEN_CONFIG_SIZE])
{
    data[0] = config->node_id;
    data[1] = (uint8_t)(config->heartbeat_producer_ms & 0xFFU);
    data[2] = (uint8_t)(config->heartbeat_producer_ms >> 8U);
    data[3] = (uint8_t)(config->configuration_version & 0xFFUL);
    data[4] = (uint8_t)((config->configuration_version >> 8U) & 0xFFUL);
    data[5] = (uint8_t)((config->configuration_version >> 16U) & 0xFFUL);
    data[6] = (uint8_t)((config->configuration_version >> 24U) & 0xFFUL);
}

static void decode_config(const uint8_t data[ITM_CANOPEN_CONFIG_SIZE],
                          itm_canopen_persistent_config_t *config)
{
    config->node_id = data[0];
    config->heartbeat_producer_ms =
        (uint16_t)data[1] | ((uint16_t)data[2] << 8U);
    config->configuration_version =
        (uint32_t)data[3] | ((uint32_t)data[4] << 8U) |
        ((uint32_t)data[5] << 16U) | ((uint32_t)data[6] << 24U);
}

static itm_sdo_abort_t write_bytes(const void *source, size_t source_size,
                                   uint8_t *data, size_t capacity,
                                   size_t *size)
{
    if ((data == NULL) || (size == NULL) || (capacity < source_size)) {
        return ITM_SDO_ABORT_TYPE_MISMATCH;
    }
    (void)memcpy(data, source, source_size);
    *size = source_size;
    return ITM_SDO_ABORT_NONE;
}

static itm_sdo_abort_t read_identity(const itm_canopen_ret_t *service,
                                     uint8_t subindex, uint8_t *data,
                                     size_t capacity, size_t *size)
{
    const uint8_t entries = 4U;

    switch (subindex) {
    case 0U:
        return write_bytes(&entries, sizeof(entries), data, capacity, size);
    case 1U: {
        const uint32_t vendor_id = ITM_CANOPEN_VENDOR_ID;
        return write_bytes(&vendor_id, sizeof(vendor_id), data, capacity, size);
    }
    case 2U:
        return write_bytes(&service->identity.product_code,
                           sizeof(service->identity.product_code), data,
                           capacity, size);
    case 3U:
        return write_bytes(&service->identity.revision_number,
                           sizeof(service->identity.revision_number), data,
                           capacity, size);
    case 4U:
        return write_bytes(&service->identity.serial_number,
                           sizeof(service->identity.serial_number), data,
                           capacity, size);
    default:
        return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    }
}

bool itm_canopen_ret_init(itm_canopen_ret_t *service,
                          itm_canopen_identity_t identity,
                          itm_canopen_persistent_config_t defaults,
                          itm_persistence_t *persistence)
{
    uint8_t stored[ITM_CANOPEN_CONFIG_SIZE];
    itm_canopen_persistent_config_t decoded;
    size_t loaded_size = 0U;

    if ((service == NULL) || (defaults.node_id == 0U) ||
        (defaults.node_id > 127U)) {
        return false;
    }

    service->identity = identity;
    service->config = defaults;
    service->persistence = persistence;
    service->error_register = 0U;
    service->configuration_dirty = false;

    if ((persistence != NULL) &&
        (persistence->schema_version == ITM_CANOPEN_CONFIG_SCHEMA) &&
        (itm_persistence_load(persistence, stored, sizeof(stored),
                              &loaded_size, NULL) == ITM_OK) &&
        (loaded_size == sizeof(stored))) {
        decode_config(stored, &decoded);
        if ((decoded.node_id > 0U) && (decoded.node_id <= 127U)) {
            service->config = decoded;
        }
    }
    return true;
}

itm_sdo_abort_t itm_canopen_ret_sdo_read(const itm_canopen_ret_t *service,
                                         uint16_t index, uint8_t subindex,
                                         uint8_t *data, size_t capacity,
                                         size_t *size)
{
    const uint32_t save_supported = 1UL;
    const uint16_t telemetry_period = ITM_CANOPEN_TELEMETRY_PERIOD_MS;

    if ((service == NULL) || (data == NULL) || (size == NULL)) {
        return ITM_SDO_ABORT_HARDWARE;
    }

    switch (index) {
    case ITM_OD_DEVICE_TYPE: {
        const uint32_t device_type = 0UL;
        return (subindex == 0U)
                   ? write_bytes(&device_type, sizeof(device_type), data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    }
    case ITM_OD_ERROR_REGISTER:
        return (subindex == 0U)
                   ? write_bytes(&service->error_register,
                                 sizeof(service->error_register), data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_DEVICE_NAME:
        return (subindex == 0U)
                   ? write_bytes(device_name, sizeof(device_name) - 1U, data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_SOFTWARE_VERSION:
        return (subindex == 0U)
                   ? write_bytes(software_version,
                                 sizeof(software_version) - 1U, data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_STORE_PARAMETERS:
        return (subindex == 1U)
                   ? write_bytes(&save_supported, sizeof(save_supported), data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_HEARTBEAT_PRODUCER:
        return (subindex == 0U)
                   ? write_bytes(&service->config.heartbeat_producer_ms,
                                 sizeof(service->config.heartbeat_producer_ms),
                                 data, capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_IDENTITY:
        return read_identity(service, subindex, data, capacity, size);
    case ITM_OD_CONFIGURATION_VERSION:
        return (subindex == 0U)
                   ? write_bytes(&service->config.configuration_version,
                                 sizeof(service->config.configuration_version),
                                 data, capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_NODE_ID:
        return (subindex == 0U)
                   ? write_bytes(&service->config.node_id,
                                 sizeof(service->config.node_id), data,
                                 capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_TELEMETRY_PERIOD:
        return (subindex == 0U)
                   ? write_bytes(&telemetry_period, sizeof(telemetry_period),
                                 data, capacity, size)
                   : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    default:
        return ITM_SDO_ABORT_OBJECT_NOT_FOUND;
    }
}

itm_sdo_abort_t itm_canopen_ret_sdo_write(itm_canopen_ret_t *service,
                                          uint16_t index, uint8_t subindex,
                                          const uint8_t *data, size_t size)
{
    if ((service == NULL) || (data == NULL)) {
        return ITM_SDO_ABORT_HARDWARE;
    }

    if ((index == ITM_OD_NODE_ID) && (subindex == 0U)) {
        if (size != sizeof(uint8_t)) {
            return ITM_SDO_ABORT_TYPE_MISMATCH;
        }
        if ((data[0] == 0U) || (data[0] > 127U)) {
            return ITM_SDO_ABORT_VALUE_RANGE;
        }
        service->config.node_id = data[0];
        service->configuration_dirty = true;
        return ITM_SDO_ABORT_NONE;
    }

    if ((index == ITM_OD_HEARTBEAT_PRODUCER) && (subindex == 0U)) {
        if (size != sizeof(uint16_t)) {
            return ITM_SDO_ABORT_TYPE_MISMATCH;
        }
        (void)memcpy(&service->config.heartbeat_producer_ms, data,
                     sizeof(service->config.heartbeat_producer_ms));
        service->configuration_dirty = true;
        return ITM_SDO_ABORT_NONE;
    }

    if ((index == ITM_OD_STORE_PARAMETERS) && (subindex == 1U)) {
        uint32_t signature;
        itm_canopen_persistent_config_t candidate;
        uint8_t serialized[ITM_CANOPEN_CONFIG_SIZE];
        if (size != sizeof(signature)) {
            return ITM_SDO_ABORT_TYPE_MISMATCH;
        }
        (void)memcpy(&signature, data, sizeof(signature));
        if (signature != ITM_CANOPEN_STORE_SIGNATURE) {
            return ITM_SDO_ABORT_VALUE_RANGE;
        }
        if (service->persistence == NULL) {
            return ITM_SDO_ABORT_HARDWARE;
        }
        candidate = service->config;
        candidate.configuration_version++;
        encode_config(&candidate, serialized);
        if (itm_persistence_store(service->persistence, serialized,
                                  sizeof(serialized), NULL) != ITM_OK) {
            return ITM_SDO_ABORT_HARDWARE;
        }
        service->config = candidate;
        service->configuration_dirty = false;
        return ITM_SDO_ABORT_NONE;
    }

    switch (index) {
    case ITM_OD_DEVICE_TYPE:
    case ITM_OD_ERROR_REGISTER:
    case ITM_OD_DEVICE_NAME:
    case ITM_OD_SOFTWARE_VERSION:
    case ITM_OD_IDENTITY:
    case ITM_OD_CONFIGURATION_VERSION:
    case ITM_OD_TELEMETRY_PERIOD:
        return ITM_SDO_ABORT_READ_ONLY;
    default:
        return ITM_SDO_ABORT_OBJECT_NOT_FOUND;
    }
}

uint32_t itm_canopen_pack_revision(uint8_t major, uint8_t minor,
                                  uint8_t patch)
{
    return ((uint32_t)major << 16U) | ((uint32_t)minor << 8U) |
           (uint32_t)patch;
}
