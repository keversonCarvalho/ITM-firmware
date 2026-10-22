#include "itm/protocols/ceb_tlm.h"
#include <string.h>

static uint16_t be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static int16_t signed_be16(const uint8_t *data)
{
    const uint16_t raw = be16(data);
    return raw <= INT16_MAX ? (int16_t)raw : (int16_t)((int32_t)raw - 65536);
}

/* ICD polynomial/init/xorout; non-reflected until a CEB golden frame confirms it. */
static uint16_t crc16(const uint8_t *data, unsigned length)
{
    uint16_t crc = 0xffffU;
    unsigned i, bit;
    for (i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8U;
        for (bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1U) ^ 0x1021U
                                              : ((uint32_t)crc << 1U));
    }
    return crc;
}

static void put(itm_ceb_tlm_t *decoder, itm_signal_id_t id, int32_t value,
                uint16_t raw, uint32_t timestamp_ms)
{
    (void)itm_telemetry_store_put_ceb_raw(decoder->store, id, value,
                                          raw, timestamp_ms);
}

static void publish(itm_ceb_tlm_t *decoder, uint32_t timestamp_ms)
{
    const uint8_t *payload = &decoder->frame[4];
    const uint16_t counter = be16(&decoder->frame[2]);
    unsigned i;
    static const itm_signal_id_t lvdt_ids[4] = {
        ITM_SIGNAL_CEB_PITCH_MAIN_LVDT, ITM_SIGNAL_CEB_PITCH_MIRROR_LVDT,
        ITM_SIGNAL_CEB_YAW_MAIN_LVDT, ITM_SIGNAL_CEB_YAW_MIRROR_LVDT};
    static const itm_signal_id_t temp_ids[4] = {
        ITM_SIGNAL_CEB_PITCH_ACTUATOR_TEMPERATURE,
        ITM_SIGNAL_CEB_YAW_ACTUATOR_TEMPERATURE,
        ITM_SIGNAL_CEB_PITCH_DRIVER_TEMPERATURE,
        ITM_SIGNAL_CEB_YAW_DRIVER_TEMPERATURE};
    static const itm_signal_id_t position_ids[4] = {
        ITM_SIGNAL_CEB_PITCH_COMMAND, ITM_SIGNAL_CEB_YAW_COMMAND,
        ITM_SIGNAL_CEB_PITCH_POSITION, ITM_SIGNAL_CEB_YAW_POSITION};
    for (i = 0; i < 4; ++i)
        put(decoder, lvdt_ids[i], (int32_t)signed_be16(payload + i * 2U) * 10000 / 32768,
            be16(payload + i * 2U), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_PITCH_CURRENT,
        (int32_t)(((int64_t)be16(payload + 8) - 2048) * 150000 / 1637),
        be16(payload + 8), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_YAW_CURRENT,
        (int32_t)(((int64_t)be16(payload + 10) - 2048) * 150000 / 1637),
        be16(payload + 10), timestamp_ms);
    for (i = 0; i < 4; ++i)
        put(decoder, temp_ids[i], (int32_t)signed_be16(payload + 12U + i * 2U) * 100 / 32 - 25600,
            be16(payload + 12U + i * 2U), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_BATTERY_150V_VOLTAGE,
        (int32_t)((int64_t)be16(payload + 20) * 200000 / 2730),
        be16(payload + 20), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_SUPPLY_28V_VOLTAGE,
        (int32_t)((int64_t)be16(payload + 22) * 28000 / 2730),
        be16(payload + 22), timestamp_ms);
    for (i = 0; i < 4; ++i)
        put(decoder, position_ids[i], (int32_t)signed_be16(payload + 24U + i * 2U) * 30000 / 32768,
            be16(payload + 24U + i * 2U), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_HEALTH, be16(payload + 32),
        be16(payload + 32), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_CAN_HEALTH, be16(payload + 34),
        be16(payload + 34), timestamp_ms);
    put(decoder, ITM_SIGNAL_CEB_FRAME_COUNTER, counter, counter, timestamp_ms);
    if (decoder->has_counter && (uint16_t)(counter - decoder->last_counter) != 1U)
        ++decoder->counter_gaps;
    decoder->last_counter = counter;
    decoder->has_counter = true;
    ++decoder->valid_frames;
}

bool itm_ceb_tlm_init(itm_ceb_tlm_t *decoder, itm_telemetry_store_t *store)
{
    if (!decoder || !store) return false;
    memset(decoder, 0, sizeof(*decoder));
    decoder->store = store;
    return true;
}

static void resync(itm_ceb_tlm_t *decoder)
{
    unsigned offset;
    for (offset = 1; offset + 1 < decoder->length; ++offset)
        if (decoder->frame[offset] == 0x5a && decoder->frame[offset + 1] == 0x5a)
            break;
    if (offset + 1 < decoder->length) {
        decoder->length -= (uint8_t)offset;
        memmove(decoder->frame, decoder->frame + offset, decoder->length);
    } else if (decoder->frame[decoder->length - 1] == 0x5a) {
        decoder->frame[0] = 0x5a;
        decoder->length = 1;
    } else {
        decoder->length = 0;
    }
}

void itm_ceb_tlm_ingest_byte(itm_ceb_tlm_t *decoder, uint8_t byte,
                             uint32_t timestamp_ms)
{
    if (!decoder || !decoder->store) return;
    if (decoder->length && timestamp_ms - decoder->last_byte_ms > 30U)
        decoder->length = 0;
    decoder->last_byte_ms = timestamp_ms;
    if (decoder->length == 0 && byte != 0x5a) return;
    if (decoder->length == 1 && byte != 0x5a) {
        decoder->length = byte == 0x5a ? 1 : 0;
        return;
    }
    decoder->frame[decoder->length++] = byte;
    if (decoder->length != ITM_CEB_TLM_FRAME_SIZE) return;
    if (decoder->frame[42] == 0x5a && decoder->frame[43] == 0x5a &&
        be16(&decoder->frame[40]) == crc16(&decoder->frame[2], 38)) {
        publish(decoder, timestamp_ms);
        decoder->length = 0;
    } else {
        if (decoder->frame[42] != 0x5a || decoder->frame[43] != 0x5a)
            ++decoder->framing_errors;
        else
            ++decoder->crc_errors;
        resync(decoder);
    }
}
