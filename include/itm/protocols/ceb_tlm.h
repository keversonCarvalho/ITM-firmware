#ifndef ITM_PROTOCOLS_CEB_TLM_H
#define ITM_PROTOCOLS_CEB_TLM_H

#include <stdbool.h>
#include <stdint.h>
#include "itm/services/telemetry_store.h"

#define ITM_CEB_TLM_FRAME_SIZE 44U

typedef struct {
    uint8_t frame[ITM_CEB_TLM_FRAME_SIZE];
    uint8_t length;
    uint32_t last_byte_ms;
    uint16_t last_counter;
    bool has_counter;
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t framing_errors;
    uint32_t counter_gaps;
    itm_telemetry_store_t *store;
} itm_ceb_tlm_t;

bool itm_ceb_tlm_init(itm_ceb_tlm_t *decoder, itm_telemetry_store_t *store);
void itm_ceb_tlm_ingest_byte(itm_ceb_tlm_t *decoder, uint8_t byte,
                             uint32_t timestamp_ms);

#endif
