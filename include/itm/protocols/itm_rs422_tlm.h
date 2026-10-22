#ifndef ITM_PROTOCOLS_ITM_RS422_TLM_H
#define ITM_PROTOCOLS_ITM_RS422_TLM_H

#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"
#include "itm/services/telemetry_store.h"

#define ITM_RS422_TLM_SIGNAL_COUNT 70U
#define ITM_RS422_TLM_FRAME_SIZE 208U
#define ITM_RS422_TLM_PERIOD_MS 10U

/* Fixed layout: counter, quality, 16/32/64-bit values and CRC. */
itm_result_t itm_rs422_tlm_encode(const itm_telemetry_snapshot_t *snapshot,
                                  uint16_t counter,
                                  uint8_t *frame, size_t capacity);

#endif
