#include "itm/services/firmware_update.h"
#include <string.h>

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put32(uint8_t *p, uint32_t v)
{
    unsigned i;
    for (i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8U * i));
}
static uint32_t crc_step(uint32_t crc, const uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned bit;
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320UL & (uint32_t)-(int32_t)(crc & 1));
    }
    return crc;
}
uint32_t itm_fw_crc32(const void *p, size_t n) { return ~crc_step(0xffffffffUL, p, n); }
uint32_t itm_fw_slot_address(unsigned slot) { return slot == 0 ? ITM_FW_SLOT_A : ITM_FW_SLOT_B; }
#define ITM_CONFIG_TAG 0xa5U
static bool record_valid(const uint8_t *r)
{
    return !memcmp(r, "ITMJ", 4) && get32(r + 4) == 1 &&
        (r[12] < 2 || r[12] == ITM_FW_NONE) &&
        (r[13] < 2 || r[13] == ITM_FW_NONE) && r[14] <= 1 &&
        (r[13] == ITM_FW_NONE || r[12] != r[13]) &&
        (r[13] != ITM_FW_NONE || r[14] == 0) &&
        get32(r + 28) == itm_fw_crc32(r, 28) &&
        !memcmp(r + 32, "DONE", 4) && get32(r + 36) == get32(r + 8) &&
        get32(r + 40) == get32(r + 28) &&
        (r[18] != ITM_CONFIG_TAG || (r[15] >= 1 && r[15] <= 127));
}
static bool save_record(itm_fw_update_t *fw, uint8_t confirmed,
                        uint8_t candidate, uint8_t tried, uint8_t node_id,
                        uint16_t heartbeat, uint32_t config_version)
{
    uint8_t record[64], check[64];
    const unsigned region = fw->journal == 2 ? 3U : 2U;
    if (fw->generation == UINT32_MAX || node_id < 1 || node_id > 127) return false;
    memset(record, 0xff, sizeof(record));
    memcpy(record, "ITMJ", 4);
    put32(record + 4, 1);
    put32(record + 8, fw->generation + 1);
    record[12] = confirmed; record[13] = candidate; record[14] = tried;
    record[15] = node_id;
    record[16] = (uint8_t)heartbeat;
    record[17] = (uint8_t)(heartbeat >> 8);
    record[18] = ITM_CONFIG_TAG;
    put32(record + 20, config_version);
    put32(record + 28, itm_fw_crc32(record, 28));
    memcpy(record + 32, "DONE", 4);
    put32(record + 36, fw->generation + 1);
    put32(record + 40, get32(record + 28));
    if (!fw->port.erase(fw->port.context, region) ||
        !fw->port.program(fw->port.context, region, 0, record, 32) ||
        !fw->port.program(fw->port.context, region, 32, record + 32, 32) ||
        !fw->port.read(fw->port.context, region, 0, check, 64) ||
        memcmp(check, record, 64)) {
        /* A commit can have succeeded even if its acknowledgement was lost.
         * Force reload before any further mutation. */
        fw->initialized = false;
        fw->receiving = false;
        return false;
    }
    fw->generation++;
    fw->journal = (uint8_t)region;
    fw->confirmed = confirmed; fw->candidate = candidate; fw->tried = tried;
    fw->node_id = node_id;
    fw->heartbeat_producer_ms = heartbeat;
    fw->configuration_version = config_version;
    fw->configuration_present = true;
    return true;
}
static bool save(itm_fw_update_t *fw, uint8_t confirmed, uint8_t candidate, uint8_t tried)
{
    return save_record(fw, confirmed, candidate, tried, fw->node_id,
                       fw->heartbeat_producer_ms, fw->configuration_version);
}
bool itm_fw_init(itm_fw_update_t *fw, itm_fw_port_t port)
{
    uint8_t records[2][64];
    bool valid[2];
    bool readable[2];
    unsigned i;
    if (!fw || !port.read || !port.erase || !port.program || !port.authorize) return false;
    memset(fw, 0, sizeof(*fw));
    fw->port = port;
    fw->confirmed = fw->candidate = ITM_FW_NONE;
    fw->node_id = 10;
    fw->heartbeat_producer_ms = 1000;
    for (i = 0; i < 2; ++i) {
        readable[i] = port.read(port.context, i + 2, 0, records[i], 64);
        valid[i] = readable[i] && record_valid(records[i]);
    }
    if (valid[0] || valid[1]) {
        i = valid[1] && (!valid[0] || get32(records[1] + 8) > get32(records[0] + 8)) ? 1U : 0U;
        fw->generation = get32(records[i] + 8);
        fw->confirmed = records[i][12]; fw->candidate = records[i][13]; fw->tried = records[i][14];
        fw->journal = (uint8_t)(i + 2);
        if (records[i][18] == ITM_CONFIG_TAG) {
            fw->node_id = records[i][15];
            fw->heartbeat_producer_ms =
                (uint16_t)records[i][16] | ((uint16_t)records[i][17] << 8);
            fw->configuration_version = get32(records[i] + 20);
            fw->configuration_present = true;
        }
    } else {
        /* Only a factory-erased pair can be provisioned automatically.
         * Corrupt journals must never cause erasure of an unknown active slot. */
        if (!readable[0] || !readable[1]) return false;
        for (i = 0; i < sizeof(records); ++i)
            if (((uint8_t *)records)[i] != 0xff) return false;
    }
    fw->destination = fw->confirmed == 0 ? 1 : 0;
    fw->initialized = true;
    return true;
}
bool itm_fw_migrate_metadata(itm_fw_update_t *fw)
{
    if (!fw || !fw->initialized || fw->receiving) return false;
    if (fw->configuration_present) return true;
    return save(fw, fw->confirmed, fw->candidate, fw->tried);
}
bool itm_fw_save_configuration(itm_fw_update_t *fw, uint8_t node_id,
                               uint16_t heartbeat_producer_ms,
                               uint32_t configuration_version)
{
    if (!fw || !fw->initialized || fw->receiving || !fw->configuration_present ||
        configuration_version != fw->configuration_version + 1U ||
        fw->configuration_version == UINT32_MAX) return false;
    return save_record(fw, fw->confirmed, fw->candidate, fw->tried,
                       node_id, heartbeat_producer_ms, configuration_version);
}
bool itm_fw_begin(itm_fw_update_t *fw, uint32_t length)
{
    if (!fw || !fw->initialized || fw->receiving || fw->tried ||
        length < ITM_FW_HEADER_SIZE + 8 || length > ITM_FW_SLOT_SIZE) return false;
    /* Anchor a factory-empty journal before any image mutation. */
    if (!save(fw, fw->confirmed, ITM_FW_NONE, 0)) return false;
    fw->destination = fw->confirmed == 0 ? 1 : 0;
    if (!fw->port.erase(fw->port.context, fw->destination)) return false;
    fw->expected = length; fw->received = 0; fw->buffered = 0;
    fw->receiving = true;
    return true;
}
void itm_fw_abort(itm_fw_update_t *fw)
{
    if (fw) { fw->receiving = false; fw->buffered = 0; }
}
bool itm_fw_write(itm_fw_update_t *fw, const void *data, size_t length)
{
    const uint8_t *p = data;
    size_t i;
    if (!fw || !fw->initialized || !fw->receiving || !data || !length ||
        length > fw->expected - fw->received) return false;
    for (i = 0; i < length; ++i) {
        fw->word[fw->buffered++] = p[i]; fw->received++;
        if (fw->buffered == 32) {
            if (!fw->port.program(fw->port.context, fw->destination,
                                  fw->received - 32, fw->word, 32)) {
                itm_fw_abort(fw); return false;
            }
            fw->buffered = 0;
        }
    }
    return true;
}
bool itm_fw_validate(itm_fw_update_t *fw, unsigned slot)
{
    uint8_t header[ITM_FW_HEADER_SIZE], block[128], vector[8];
    uint32_t size, offset, crc = 0xffffffffUL, reset, stack;
    const uint32_t base = itm_fw_slot_address(slot) + ITM_FW_HEADER_SIZE;
    if (!fw || !fw->initialized || slot > 1 ||
        !fw->port.read(fw->port.context, slot, 0, header, sizeof(header))) return false;
    size = get32(header + 16);
    if (memcmp(header, "ITMF", 4) || get32(header + 4) != 1 ||
        get32(header + 8) != ITM_FW_TARGET || get32(header + 12) != base ||
        size < 8 || size > ITM_FW_SLOT_SIZE - ITM_FW_HEADER_SIZE ||
        get32(header + 28) != itm_fw_crc32(header, 28)) return false;
    if (!fw->port.read(fw->port.context, slot, ITM_FW_HEADER_SIZE, vector, 8)) return false;
    stack = get32(vector); reset = get32(vector + 4);
    /* Current application linker uses only DTCM. Accept top-of-stack inclusive. */
    if ((stack & 7) || stack <= 0x20000000UL || stack > 0x20020000UL ||
        !(reset & 1) || (reset & ~1UL) < base || (reset & ~1UL) >= base + size) return false;
    for (offset = 0; offset < size;) {
        const size_t n = size - offset < sizeof(block) ? size - offset : sizeof(block);
        if (!fw->port.read(fw->port.context, slot, ITM_FW_HEADER_SIZE + offset, block, n)) return false;
        crc = crc_step(crc, block, n); offset += (uint32_t)n;
    }
    return ~crc == get32(header + 24) && fw->port.authorize(fw->port.context, slot, header);
}
bool itm_fw_finish(itm_fw_update_t *fw)
{
    uint8_t header[32];
    if (!fw || !fw->initialized || !fw->receiving || fw->received != fw->expected) return false;
    if (fw->buffered) {
        memset(fw->word + fw->buffered, 0xff, 32 - fw->buffered);
        if (!fw->port.program(fw->port.context, fw->destination,
            fw->received - (uint32_t)fw->buffered, fw->word, 32)) { itm_fw_abort(fw); return false; }
    }
    itm_fw_abort(fw);
    if (!fw->port.read(fw->port.context, fw->destination, 0, header, 32) ||
        get32(header + 16) != fw->expected - ITM_FW_HEADER_SIZE ||
        !itm_fw_validate(fw, fw->destination)) return false;
    return save(fw, fw->confirmed, fw->destination, 0);
}
bool itm_fw_select_boot(itm_fw_update_t *fw, uint8_t *slot)
{
    if (!fw || !fw->initialized || !slot || fw->receiving) return false;
    if (fw->candidate < 2) {
        if (!fw->tried && itm_fw_validate(fw, fw->candidate)) {
            const uint8_t candidate = fw->candidate;
            if (!save(fw, fw->confirmed, candidate, 1)) return false;
            *slot = candidate; return true;
        }
        if (!save(fw, fw->confirmed, ITM_FW_NONE, 0)) return false;
    }
    if (fw->confirmed < 2 && itm_fw_validate(fw, fw->confirmed)) {
        *slot = fw->confirmed; return true;
    }
    return false;
}
bool itm_fw_confirm(itm_fw_update_t *fw, uint8_t running)
{
    if (!fw || !fw->initialized || fw->receiving || !fw->tried ||
        running != fw->candidate || !itm_fw_validate(fw, running)) return false;
    return save(fw, running, ITM_FW_NONE, 0);
}
