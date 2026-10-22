#include "itm/services/canopen_ret.h"

#include <string.h>

#define ITM_CANOPEN_CONFIG_SCHEMA 1UL
#define ITM_CANOPEN_STORE_SIGNATURE 0x65766173UL
#define ITM_CANOPEN_CONFIG_SIZE 7U

static const char device_name[] = "ITM-100";
static const char hardware_part_number[] = "ITM-100-REV00-PROVISIONAL";
static const char firmware_part_number[] = "ITM-100-FW-PROVISIONAL";
static const char hex_digits[] = "0123456789ABCDEF";

static size_t decimal_u32(uint32_t value, char output[10])
{
    char reversed[10];
    size_t length = 0, index;
    do {
        reversed[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    for (index = 0; index < length; ++index)
        output[index] = reversed[length - index - 1U];
    return length;
}

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

/* Versioned, atomic CEB slice of the same telemetry store used by the BMS.
 * Each entry is decimal value:quality, in catalog ID order. */
static itm_sdo_abort_t read_ceb_snapshot(const itm_canopen_ret_t *service,
                                         uint8_t *data, size_t capacity,
                                         size_t *size)
{
    itm_telemetry_snapshot_t snapshot;
    size_t used = 0;
    itm_signal_id_t id;
    if (!service->telemetry_snapshot.capture || !service->telemetry_clock ||
        capacity < 3U)
        return ITM_SDO_ABORT_HARDWARE;
    if (service->telemetry_snapshot.capture(service->telemetry_snapshot.context,
            service->telemetry_clock(service->telemetry_clock_context),
            &snapshot) != ITM_OK) return ITM_SDO_ABORT_HARDWARE;
    data[used++] = '1'; data[used++] = ';';
    for (id = ITM_SIGNAL_CEB_PITCH_MAIN_LVDT;
         id <= ITM_SIGNAL_CEB_FRAME_COUNTER; ++id) {
        const int64_t value = snapshot.values[id].value;
        const bool negative = value < 0;
        char digits[10];
        const size_t length = decimal_u32((uint32_t)(negative ? -value : value), digits);
        if (used + length + (negative ? 1U : 0U) + 2U > capacity)
            return ITM_SDO_ABORT_TYPE_MISMATCH;
        if (negative) data[used++] = '-';
        memcpy(data + used, digits, length); used += length;
        data[used++] = ':';
        data[used++] = (uint8_t)('0' + snapshot.values[id].quality);
        if (id != ITM_SIGNAL_CEB_FRAME_COUNTER) {
            if (used == capacity) return ITM_SDO_ABORT_TYPE_MISMATCH;
            data[used++] = ',';
        }
    }
    *size = used;
    return ITM_SDO_ABORT_NONE;
}

/* A single capture is serialized before SDO segmentation begins.  Each of the
 * ITM_SIGNAL_COUNT records is 16 bytes: id:u16, type:u8, quality:u8,
 * timestamp_ms:u32, value:i64; all multibyte fields are little-endian. */
static itm_sdo_abort_t read_full_snapshot(const itm_canopen_ret_t *service,
                                          uint8_t *data, size_t capacity,
                                          size_t *size)
{
    itm_telemetry_snapshot_t snapshot;
    if (!service->telemetry_snapshot.capture || !service->telemetry_clock)
        return ITM_SDO_ABORT_HARDWARE;
    if (capacity < itm_telemetry_snapshot_serialized_size())
        return ITM_SDO_ABORT_TYPE_MISMATCH;
    if (service->telemetry_snapshot.capture(service->telemetry_snapshot.context,
            service->telemetry_clock(service->telemetry_clock_context),
            &snapshot) != ITM_OK) return ITM_SDO_ABORT_HARDWARE;
    return itm_telemetry_snapshot_serialize(&snapshot, data, capacity, size)
               == ITM_OK ? ITM_SDO_ABORT_NONE : ITM_SDO_ABORT_HARDWARE;
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
    service->save_config = NULL;
    service->save_config_context = NULL;
    (void)memset(service->hardware_uid, 0, sizeof(service->hardware_uid));
    service->firmware_version = 0U;
    service->firmware_crc = 0U;
    service->telemetry_snapshot = (itm_telemetry_snapshot_port_t){0};
    service->telemetry_clock = NULL;
    service->telemetry_clock_context = NULL;
    service->read_ceb_diagnostic = NULL;
    service->ceb_diagnostic_context = NULL;
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
bool itm_canopen_ret_bind_config_store(itm_canopen_ret_t *service,
                                       itm_canopen_save_config_t save,
                                       void *context)
{
    if (!service || !save) return false;
    service->save_config = save;
    service->save_config_context = context;
    return true;
}
bool itm_canopen_ret_set_management(itm_canopen_ret_t *service,
                                    const uint8_t uid[12],
                                    uint32_t firmware_version,
                                    uint32_t firmware_crc)
{
    if (!service || !uid) return false;
    (void)memcpy(service->hardware_uid, uid, sizeof(service->hardware_uid));
    service->firmware_version = firmware_version;
    service->firmware_crc = firmware_crc;
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
        if (subindex == 0U) {
            char version[10];
            const size_t length = decimal_u32(service->firmware_version, version);
            return write_bytes(version, length, data, capacity, size);
        }
        return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
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
    case ITM_OD_HARDWARE_PART_NUMBER:
        return subindex == 0U ? write_bytes(hardware_part_number,
            sizeof(hardware_part_number) - 1U, data, capacity, size)
            : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_FIRMWARE_PART_NUMBER:
        return subindex == 0U ? write_bytes(firmware_part_number,
            sizeof(firmware_part_number) - 1U, data, capacity, size)
            : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_HARDWARE_UID:
        if (subindex == 0U) {
            char text[24];
            size_t index;
            for (index = 0; index < sizeof(service->hardware_uid); ++index) {
                text[2U * index] = hex_digits[service->hardware_uid[index] >> 4U];
                text[2U * index + 1U] = hex_digits[service->hardware_uid[index] & 15U];
            }
            return write_bytes(text, sizeof(text), data, capacity, size);
        }
        return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_FIRMWARE_CRC:
        return subindex == 0U ? write_bytes(&service->firmware_crc,
            sizeof(service->firmware_crc), data, capacity, size)
            : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_CEB_DIAGNOSTICS:
        if (subindex == 0U) {
            const uint8_t count = ITM_CEB_DIAGNOSTIC_COUNT;
            return write_bytes(&count, sizeof(count), data, capacity, size);
        }
        if (subindex <= ITM_CEB_DIAGNOSTIC_COUNT) {
            uint32_t value;
            if (!service->read_ceb_diagnostic ||
                !service->read_ceb_diagnostic(service->ceb_diagnostic_context,
                                              subindex, &value))
                return ITM_SDO_ABORT_HARDWARE;
            return write_bytes(&value, sizeof(value), data, capacity, size);
        }
        return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_CEB_SNAPSHOT:
        return subindex == 0U ? read_ceb_snapshot(service, data, capacity, size)
                              : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_FULL_SNAPSHOT:
        return subindex == 0U ? read_full_snapshot(service, data, capacity, size)
                              : ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
    case ITM_OD_BENCH_POWER_COMMAND:
    case ITM_OD_BENCH_POWER_STATE:
    case ITM_OD_BENCH_DEBOUNCE_MS:
    case ITM_OD_BENCH_CEB_DELAY_MS:
    case ITM_OD_BENCH_CB_TIMEOUT_MS:
    case ITM_OD_BENCH_EM1_MV:
    case ITM_OD_BENCH_EM2_MV:
    case ITM_OD_BENCH_CB_CONDITION: {
        uint32_t value;
        size_t width = sizeof(value);
        if (subindex != 0U) return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
        if (!service->power_read ||
            !service->power_read(service->power_context, index, &value))
            return ITM_SDO_ABORT_HARDWARE;
        if (index == ITM_OD_BENCH_POWER_COMMAND ||
            index == ITM_OD_BENCH_CB_CONDITION) width = 1U;
        else if (index >= ITM_OD_BENCH_DEBOUNCE_MS &&
                 index <= ITM_OD_BENCH_CB_TIMEOUT_MS) width = 2U;
        return write_bytes(&value, width, data, capacity, size);
    }
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
        if (service->persistence == NULL && service->save_config == NULL) {
            return ITM_SDO_ABORT_HARDWARE;
        }
        candidate = service->config;
        candidate.configuration_version++;
        encode_config(&candidate, serialized);
        if (service->save_config != NULL) {
            if (!service->save_config(service->save_config_context, &candidate))
                return ITM_SDO_ABORT_HARDWARE;
        } else {
            if (itm_persistence_store(service->persistence, serialized,
                                      sizeof(serialized), NULL) != ITM_OK)
                return ITM_SDO_ABORT_HARDWARE;
        }
        service->config = candidate;
        service->configuration_dirty = false;
        return ITM_SDO_ABORT_NONE;
    }

    if (index == ITM_OD_BENCH_POWER_COMMAND ||
        (index >= ITM_OD_BENCH_DEBOUNCE_MS &&
         index <= ITM_OD_BENCH_CB_TIMEOUT_MS)) {
        uint32_t value = 0U;
        const size_t width = index == ITM_OD_BENCH_POWER_COMMAND ? 1U : 2U;
        if (subindex != 0U) return ITM_SDO_ABORT_SUBINDEX_NOT_FOUND;
        if (size != width) return ITM_SDO_ABORT_TYPE_MISMATCH;
        if (!service->power_write) return ITM_SDO_ABORT_HARDWARE;
        (void)memcpy(&value, data, width);
        return service->power_write(service->power_context, index, value);
    }

    switch (index) {
    case ITM_OD_DEVICE_TYPE:
    case ITM_OD_ERROR_REGISTER:
    case ITM_OD_DEVICE_NAME:
    case ITM_OD_SOFTWARE_VERSION:
    case ITM_OD_IDENTITY:
    case ITM_OD_CONFIGURATION_VERSION:
    case ITM_OD_TELEMETRY_PERIOD:
    case ITM_OD_HARDWARE_PART_NUMBER:
    case ITM_OD_FIRMWARE_PART_NUMBER:
    case ITM_OD_HARDWARE_UID:
    case ITM_OD_FIRMWARE_CRC:
    case ITM_OD_CEB_DIAGNOSTICS:
    case ITM_OD_CEB_SNAPSHOT:
    case ITM_OD_FULL_SNAPSHOT:
    case ITM_OD_BENCH_POWER_STATE:
    case ITM_OD_BENCH_EM1_MV:
    case ITM_OD_BENCH_EM2_MV:
    case ITM_OD_BENCH_CB_CONDITION:
        return ITM_SDO_ABORT_READ_ONLY;
    default:
        return ITM_SDO_ABORT_OBJECT_NOT_FOUND;
    }
}

bool itm_canopen_ret_bind_power_bench(itm_canopen_ret_t *service,
    itm_canopen_power_read_t read, itm_canopen_power_write_t write,
    void *context)
{
    if (!service || !read || !write) return false;
    service->power_read = read;
    service->power_write = write;
    service->power_context = context;
    return true;
}

bool itm_canopen_ret_bind_ceb_diagnostics(
    itm_canopen_ret_t *service, itm_canopen_read_ceb_diagnostic_t read,
    void *context)
{
    if (!service || !read) return false;
    service->read_ceb_diagnostic = read;
    service->ceb_diagnostic_context = context;
    return true;
}

uint32_t itm_canopen_pack_revision(uint8_t major, uint8_t minor,
                                  uint8_t patch)
{
    return ((uint32_t)major << 16U) | ((uint32_t)minor << 8U) |
           (uint32_t)patch;
}

bool itm_canopen_ret_bind_telemetry(
    itm_canopen_ret_t *service,
    itm_telemetry_snapshot_port_t telemetry_snapshot)
{
    if ((service == NULL) || (telemetry_snapshot.capture == NULL)) {
        return false;
    }
    service->telemetry_snapshot = telemetry_snapshot;
    return true;
}

bool itm_canopen_ret_bind_telemetry_clock(
    itm_canopen_ret_t *service, itm_canopen_telemetry_clock_t clock,
    void *context)
{
    if (!service || !clock) return false;
    service->telemetry_clock = clock;
    service->telemetry_clock_context = context;
    return true;
}

itm_result_t itm_canopen_ret_capture_telemetry(
    itm_canopen_ret_t *service, uint32_t now_ms,
    itm_telemetry_snapshot_t *snapshot)
{
    if ((service == NULL) || (snapshot == NULL) ||
        (service->telemetry_snapshot.capture == NULL)) {
        return ITM_ERROR_NOT_READY;
    }
    return service->telemetry_snapshot.capture(
        service->telemetry_snapshot.context, now_ms, snapshot);
}
