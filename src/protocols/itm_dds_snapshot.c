#include "itm/protocols/itm_dds_snapshot.h"

#include <limits.h>
#include <string.h>

_Static_assert(ITM_SIGNAL_COUNT == 71, "Update the DDS mapping with the catalog");
_Static_assert(ITM_SIGNAL_CEB_FRAME_COUNTER == ITM_DDS_SIGNAL_COUNT,
               "The excluded CEB counter must remain last");

static void put_le(uint8_t *target, uint64_t value, unsigned bytes)
{
    unsigned i;
    for (i = 0U; i < bytes; ++i)
        target[i] = (uint8_t)(value >> (8U * i));
}

static unsigned value_width(unsigned index)
{
    switch ((itm_signal_id_t)index) {
    case ITM_SIGNAL_AV_BATT_CHARGER_VOLTAGE:
    case ITM_SIGNAL_AV_BATT_BALANCE_STATE:
    case ITM_SIGNAL_ACT_BATT_CHARGER_VOLTAGE:
    case ITM_SIGNAL_ACT_BATT_BALANCE_STATE:
        return 8U;
    case ITM_SIGNAL_ACT_BATT_VOLTAGE:
    case ITM_SIGNAL_AV_BATT_CURRENT_1:
    case ITM_SIGNAL_AV_BATT_CURRENT_2:
    case ITM_SIGNAL_AV_BATT_AMPERE_HOURS:
    case ITM_SIGNAL_AV_BATT_WATT_HOURS:
    case ITM_SIGNAL_ACT_BATT_CURRENT_1:
    case ITM_SIGNAL_ACT_BATT_CURRENT_2:
    case ITM_SIGNAL_ACT_BATT_AMPERE_HOURS:
    case ITM_SIGNAL_ACT_BATT_WATT_HOURS:
        return 4U;
    default:
        return 2U;
    }
}

itm_result_t itm_dds_snapshot_prepare(const itm_telemetry_snapshot_t *source,
                                      itm_dds_snapshot_t *destination)
{
    unsigned i;
    if (!source || !destination) return ITM_ERROR_INVALID_ARGUMENT;
    memset(destination, 0, sizeof(*destination));
    destination->sequence = (uint16_t)source->sequence;
    for (i = 0U; i < ITM_DDS_SIGNAL_COUNT; ++i) {
        const itm_signal_runtime_t *sample = &source->values[i];
        if (sample->quality > ITM_SIGNAL_QUALITY_STALE)
            return ITM_ERROR_CORRUPT;
        if (!sample->has_timestamp) continue;
        destination->quality[i / 4U] |=
            (uint8_t)((uint8_t)sample->quality << (6U - 2U * (i % 4U)));
        if (sample->quality != ITM_SIGNAL_QUALITY_INVALID) {
            if (itm_telemetry_catalog[i].source == ITM_SOURCE_CEB) {
                if (!sample->has_ceb_raw) return ITM_ERROR_CORRUPT;
                destination->value[i] = sample->ceb_raw;
            } else {
                destination->value[i] = sample->value;
            }
        }
    }
    return ITM_OK;
}

itm_result_t itm_dds_snapshot_serialize(const itm_dds_snapshot_t *snapshot,
                                        uint8_t *payload, size_t capacity,
                                        size_t *written)
{
    unsigned i;
    size_t offset = 2U + ITM_DDS_QUALITY_BYTES;
    if (!snapshot || !payload || !written) return ITM_ERROR_INVALID_ARGUMENT;
    if (capacity < ITM_DDS_CDR_SIZE) return ITM_ERROR_NO_SPACE;
    memset(payload, 0, ITM_DDS_CDR_SIZE);
    put_le(payload, snapshot->sequence, 2U);
    memcpy(payload + 2U, snapshot->quality, ITM_DDS_QUALITY_BYTES);
    for (i = 0U; i < ITM_DDS_SIGNAL_COUNT; ++i) {
        const unsigned width = value_width(i);
        const uint8_t quality = (uint8_t)((snapshot->quality[i / 4U] >>
                                (6U - 2U * (i % 4U))) & 3U);
        const int64_t value = snapshot->value[i];
        const itm_signal_metadata_t *metadata = &itm_telemetry_catalog[i];
        if (quality > ITM_SIGNAL_QUALITY_STALE)
            return ITM_ERROR_CORRUPT;
        if (quality != ITM_SIGNAL_QUALITY_INVALID) {
            if (metadata->source == ITM_SOURCE_CEB) {
                if (value < 0 || value > UINT16_MAX) return ITM_ERROR_CORRUPT;
            } else if (width == 2U &&
                       ((metadata->type == ITM_SIGNAL_TYPE_I32 &&
                         (value < INT16_MIN || value > INT16_MAX)) ||
                        (metadata->type != ITM_SIGNAL_TYPE_I32 &&
                         (value < 0 || value > UINT16_MAX))))
                return ITM_ERROR_CORRUPT;
            else if (width == 4U &&
                     ((metadata->type == ITM_SIGNAL_TYPE_I32 &&
                       (value < INT32_MIN || value > INT32_MAX)) ||
                      (metadata->type != ITM_SIGNAL_TYPE_I32 &&
                       (value < 0 || (uint64_t)value > UINT32_MAX))))
                return ITM_ERROR_CORRUPT;
        }
        offset = (offset + width - 1U) & ~(size_t)(width - 1U);
        if (offset + width > ITM_DDS_CDR_SIZE) return ITM_ERROR_CORRUPT;
        put_le(payload + offset, (uint64_t)value, width);
        offset += width;
    }
    if (offset + 1U != ITM_DDS_CDR_SIZE ||
        snapshot->cb_condition > 6U) return ITM_ERROR_CORRUPT;
    payload[offset] = snapshot->cb_condition;
    *written = offset + 1U;
    return ITM_OK;
}
