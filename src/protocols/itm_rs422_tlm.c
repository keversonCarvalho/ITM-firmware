#include "itm/protocols/itm_rs422_tlm.h"

#include <limits.h>
#include <string.h>

_Static_assert(ITM_SIGNAL_COUNT == 71, "Update the RS-422 layout with the catalog");
_Static_assert(ITM_SIGNAL_CEB_FRAME_COUNTER == 70,
               "The excluded counter must remain the last catalog entry");

static void put_be(uint8_t *target, uint64_t value, unsigned width)
{
    unsigned byte;
    for (byte = 0; byte < width; ++byte)
        target[byte] = (uint8_t)(value >> (8U * (width - byte - 1U)));
}

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffffU;
    size_t index;
    unsigned bit;
    for (index = 0; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8U;
        for (bit = 0; bit < 8U; ++bit) {
            if (crc & 0x8000U) crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            else crc = (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static unsigned wire_width(unsigned index)
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

itm_result_t itm_rs422_tlm_encode(const itm_telemetry_snapshot_t *snapshot,
                                  uint16_t counter,
                                  uint8_t *frame, size_t capacity)
{
    unsigned index;
    size_t offset = 22U;
    uint16_t crc;
    if (snapshot == NULL || frame == NULL) return ITM_ERROR_INVALID_ARGUMENT;
    if (capacity < ITM_RS422_TLM_FRAME_SIZE) return ITM_ERROR_NO_SPACE;
    memset(frame, 0, ITM_RS422_TLM_FRAME_SIZE);
    frame[0] = 0x5aU; frame[1] = 0x5aU;
    put_be(&frame[2], counter, 2U);
    for (index = 0U; index < ITM_RS422_TLM_SIGNAL_COUNT; ++index) {
        const itm_signal_metadata_t *metadata = &itm_telemetry_catalog[index];
        const itm_signal_runtime_t *sample = &snapshot->values[index];
        const unsigned width = wire_width(index);
        uint64_t value = 0U;
        uint8_t quality = (uint8_t)sample->quality;
        if (quality > (uint8_t)ITM_SIGNAL_QUALITY_STALE)
            return ITM_ERROR_CORRUPT;
        if (!sample->has_timestamp) quality = ITM_SIGNAL_QUALITY_INVALID;
        if (quality != ITM_SIGNAL_QUALITY_INVALID) {
            if ((metadata->type == ITM_SIGNAL_TYPE_I32 &&
                 (sample->value < INT32_MIN || sample->value > INT32_MAX)) ||
                (metadata->type == ITM_SIGNAL_TYPE_U32 &&
                 (sample->value < 0 || (uint64_t)sample->value > UINT32_MAX)) ||
                (metadata->type == ITM_SIGNAL_TYPE_BOOL &&
                 sample->value != 0 && sample->value != 1))
                return ITM_ERROR_CORRUPT;
            if (metadata->source == ITM_SOURCE_CEB) {
                if (!sample->has_ceb_raw) return ITM_ERROR_CORRUPT;
                value = sample->ceb_raw;
            } else {
                if (width == 2U &&
                    ((metadata->type == ITM_SIGNAL_TYPE_I32 &&
                      (sample->value < INT16_MIN || sample->value > INT16_MAX)) ||
                     (metadata->type == ITM_SIGNAL_TYPE_U32 &&
                      (sample->value < 0 || sample->value > UINT16_MAX))))
                    return ITM_ERROR_CORRUPT;
                value = (uint64_t)sample->value;
            }
        }
        frame[4U + index / 4U] |=
            (uint8_t)(quality << (6U - 2U * (index % 4U)));
        put_be(&frame[offset], value, width);
        offset += width;
    }
    if (offset != ITM_RS422_TLM_FRAME_SIZE - 4U)
        return ITM_ERROR_CORRUPT;
    crc = crc16_ccitt_false(&frame[2], offset - 2U);
    put_be(&frame[offset], crc, 2U);
    frame[offset + 2U] = 0x5aU; frame[offset + 3U] = 0x5aU;
    return ITM_OK;
}
