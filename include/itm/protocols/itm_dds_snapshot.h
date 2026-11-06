#ifndef ITM_PROTOCOLS_ITM_DDS_SNAPSHOT_H
#define ITM_PROTOCOLS_ITM_DDS_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

#include "itm/core/result.h"
#include "itm/services/telemetry_store.h"

/* Provisional BC topic ITM_Snapshot_v2. Indexes 0..69 follow the catalog;
 * CEB_FRAME_COUNTER (index 70) remains diagnostic-only. */
#define ITM_DDS_SIGNAL_COUNT 70U
#define ITM_DDS_PUBLISH_PERIOD_MS 40U
#define ITM_DDS_QUALITY_BYTES 18U
#define ITM_DDS_CDR_SIZE 213U

typedef struct {
    uint16_t sequence;
    uint8_t quality[ITM_DDS_QUALITY_BYTES];
    int64_t value[ITM_DDS_SIGNAL_COUNT];
    uint8_t cb_condition;
} itm_dds_snapshot_t;

itm_result_t itm_dds_snapshot_prepare(const itm_telemetry_snapshot_t *source,
                                      itm_dds_snapshot_t *destination);
/* CDR1 little-endian payload for the IDL type, without XRCE headers. */
itm_result_t itm_dds_snapshot_serialize(const itm_dds_snapshot_t *snapshot,
                                        uint8_t *payload, size_t capacity,
                                        size_t *written);

#endif
