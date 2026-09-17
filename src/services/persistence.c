#include "itm/services/persistence.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ITM_PERSISTENCE_MAGIC 0x49544D50UL
#define ITM_PERSISTENCE_COMMITTED 0x434F4D4DUL

typedef struct {
    uint32_t magic;
    uint32_t schema_version;
    uint32_t generation;
    uint32_t payload_size;
    uint32_t payload_crc;
    uint32_t committed;
} record_header_t;

static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t index;
    unsigned bit;

    for (index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

static bool header_valid(const itm_persistence_t *persistence,
                         const record_header_t *header)
{
    return (header->magic == ITM_PERSISTENCE_MAGIC) &&
           (header->schema_version == persistence->schema_version) &&
           (header->committed == ITM_PERSISTENCE_COMMITTED) &&
           (header->payload_size <=
            persistence->flash.slot_size - sizeof(record_header_t));
}

static itm_result_t read_header(const itm_persistence_t *persistence,
                                uint8_t slot, record_header_t *header)
{
    return persistence->flash.read(persistence->flash.context, slot, 0U,
                                   header, sizeof(*header));
}

bool itm_persistence_init(itm_persistence_t *persistence,
                          itm_flash_port_t flash,
                          uint32_t schema_version)
{
    if ((persistence == NULL) || (flash.read == NULL) ||
        (flash.erase == NULL) || (flash.write == NULL) ||
        (flash.slot_size <= sizeof(record_header_t))) {
        return false;
    }
    persistence->flash = flash;
    persistence->schema_version = schema_version;
    return true;
}

itm_result_t itm_persistence_load(const itm_persistence_t *persistence,
                                  void *data, size_t capacity,
                                  size_t *loaded_size,
                                  uint32_t *generation)
{
    record_header_t headers[2];
    bool valid[2] = {false, false};
    uint8_t slots[2];
    uint8_t slot;
    uint8_t attempt;
    itm_result_t result;

    if ((persistence == NULL) || (data == NULL) || (loaded_size == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }

    for (slot = 0U; slot < 2U; ++slot) {
        result = read_header(persistence, slot, &headers[slot]);
        valid[slot] = (result == ITM_OK) &&
                      header_valid(persistence, &headers[slot]);
    }
    if (!valid[0] && !valid[1]) {
        return ITM_ERROR_NOT_FOUND;
    }
    slots[0] = (!valid[0] || (valid[1] &&
                (headers[1].generation > headers[0].generation))) ? 1U : 0U;
    slots[1] = (uint8_t)(1U - slots[0]);

    for (attempt = 0U; attempt < 2U; ++attempt) {
        slot = slots[attempt];
        if (!valid[slot]) {
            continue;
        }
        if (headers[slot].payload_size > capacity) {
            return ITM_ERROR_NO_SPACE;
        }
        result = persistence->flash.read(persistence->flash.context, slot,
                                         sizeof(record_header_t), data,
                                         headers[slot].payload_size);
        if ((result != ITM_OK) ||
            (crc32((const uint8_t *)data, headers[slot].payload_size) !=
             headers[slot].payload_crc)) {
            continue;
        }
        *loaded_size = headers[slot].payload_size;
        if (generation != NULL) {
            *generation = headers[slot].generation;
        }
        return ITM_OK;
    }
    return ITM_ERROR_CORRUPT;
}

itm_result_t itm_persistence_store(const itm_persistence_t *persistence,
                                   const void *data, size_t size,
                                   uint32_t *new_generation)
{
    record_header_t headers[2];
    bool valid[2];
    uint8_t target;
    uint32_t generation = 1U;
    record_header_t header;
    record_header_t verification;
    itm_result_t result;

    if ((persistence == NULL) || (data == NULL) || (size == 0U)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    if (size > persistence->flash.slot_size - sizeof(record_header_t)) {
        return ITM_ERROR_NO_SPACE;
    }

    for (target = 0U; target < 2U; ++target) {
        result = read_header(persistence, target, &headers[target]);
        valid[target] = (result == ITM_OK) &&
                        header_valid(persistence, &headers[target]);
    }
    if (valid[0] || valid[1]) {
        const uint8_t latest = (!valid[0] || (valid[1] &&
                               headers[1].generation > headers[0].generation))
                                   ? 1U : 0U;
        generation = headers[latest].generation + 1U;
        target = (uint8_t)(1U - latest);
    } else {
        target = 0U;
    }

    header = (record_header_t){ITM_PERSISTENCE_MAGIC,
                               persistence->schema_version,
                               generation, (uint32_t)size,
                               crc32((const uint8_t *)data, size), 0U};
    result = persistence->flash.erase(persistence->flash.context, target);
    if (result == ITM_OK) {
        result = persistence->flash.write(persistence->flash.context, target,
                                          0U, &header, sizeof(header));
    }
    if (result == ITM_OK) {
        result = persistence->flash.write(persistence->flash.context, target,
                                          sizeof(header), data, size);
    }
    if (result == ITM_OK) {
        header.committed = ITM_PERSISTENCE_COMMITTED;
        result = persistence->flash.write(persistence->flash.context, target,
                                          offsetof(record_header_t, committed),
                                          &header.committed,
                                          sizeof(header.committed));
    }
    if (result == ITM_OK) {
        result = read_header(persistence, target, &verification);
    }
    if ((result != ITM_OK) || !header_valid(persistence, &verification) ||
        (memcmp(&header, &verification, sizeof(header)) != 0)) {
        return (result == ITM_OK) ? ITM_ERROR_CORRUPT : result;
    }
    if (new_generation != NULL) {
        *new_generation = generation;
    }
    return ITM_OK;
}
