#ifndef ITM_FIRMWARE_UPDATE_H
#define ITM_FIRMWARE_UPDATE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITM_FW_SLOT_SIZE (256UL * 1024UL)
#define ITM_FW_HEADER_SIZE 512U
#define ITM_FW_SLOT_A 0x08060000UL
#define ITM_FW_SLOT_B 0x080A0000UL
#define ITM_FW_TARGET 0x07230001UL
#define ITM_FW_NONE 255U
/* Regions 0/1 are image slots; 2/3 are independent shared boot/configuration
 * metadata erase sectors. Each committed record contains both kinds of state.
 * program must obey physical flash alignment (caller supplies 32-byte words).
 * authorize must verify image authenticity for flight; bench may use an
 * explicitly selected CRC-only policy. It must never silently default true. */
typedef struct {
    void *context;
    bool (*read)(void *, unsigned, uint32_t, void *, size_t);
    bool (*erase)(void *, unsigned);
    bool (*program)(void *, unsigned, uint32_t, const void *, size_t);
    bool (*authorize)(void *, unsigned, const uint8_t header[ITM_FW_HEADER_SIZE]);
} itm_fw_port_t;
typedef struct {
    itm_fw_port_t port;
    uint32_t generation, expected, received, configuration_version;
    uint8_t confirmed, candidate, tried, journal, destination;
    uint8_t node_id;
    uint16_t heartbeat_producer_ms;
    uint8_t word[32];
    size_t buffered;
    bool receiving, initialized, configuration_present;
} itm_fw_update_t;
uint32_t itm_fw_crc32(const void *data, size_t length);
uint32_t itm_fw_slot_address(unsigned slot);
bool itm_fw_init(itm_fw_update_t *fw, itm_fw_port_t port);
bool itm_fw_begin(itm_fw_update_t *fw, uint32_t length);
bool itm_fw_write(itm_fw_update_t *fw, const void *data, size_t length);
bool itm_fw_finish(itm_fw_update_t *fw);
void itm_fw_abort(itm_fw_update_t *fw);
bool itm_fw_validate(itm_fw_update_t *fw, unsigned slot);
/* Persist trial BEFORE jumping. A reset before confirm rolls back. */
bool itm_fw_select_boot(itm_fw_update_t *fw, uint8_t *slot);
bool itm_fw_confirm(itm_fw_update_t *fw, uint8_t running_slot);
/* Convert a legacy boot-only journal to the shared boot/configuration format. */
bool itm_fw_migrate_metadata(itm_fw_update_t *fw);
/* Commit boot state and CANopen configuration atomically in the opposite sector. */
bool itm_fw_save_configuration(itm_fw_update_t *fw, uint8_t node_id,
                               uint16_t heartbeat_producer_ms,
                               uint32_t configuration_version);
#endif
