#include "test_framework.h"
#include "itm/protocols/itm_rs422_tlm.h"

#include <string.h>

static uint16_t independent_crc(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xffffU;
    size_t i;
    for (i = 0; i < size; ++i) {
        unsigned bit;
        crc ^= (uint16_t)data[i] << 8U;
        for (bit = 0; bit < 8U; ++bit)
            crc = (uint16_t)((crc << 1U) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}

bool test_itm_rs422_tlm_complete_frame(void)
{
    itm_telemetry_snapshot_t snapshot = {0};
    uint8_t frame[ITM_RS422_TLM_FRAME_SIZE];
    uint16_t crc;
    snapshot.captured_at_ms = 0x12345678U;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].quality = ITM_SIGNAL_QUALITY_VALID;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].value = 28500;
    snapshot.values[ITM_SIGNAL_AV_BATT_SOC].quality = ITM_SIGNAL_QUALITY_STALE;
    snapshot.values[ITM_SIGNAL_AV_BATT_SOC].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_AV_BATT_SOC].value = 5490;
    snapshot.values[ITM_SIGNAL_AV_BATT_CHARGER_VOLTAGE].quality = ITM_SIGNAL_QUALITY_VALID;
    snapshot.values[ITM_SIGNAL_AV_BATT_CHARGER_VOLTAGE].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_AV_BATT_CHARGER_VOLTAGE].value = 0x0102030405060708LL;
    snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].quality = ITM_SIGNAL_QUALITY_VALID;
    snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].value = 10;
    snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].has_ceb_raw = true;
    snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].ceb_raw = 10;
    snapshot.values[ITM_SIGNAL_CEB_FRAME_COUNTER].quality = ITM_SIGNAL_QUALITY_VALID;
    snapshot.values[ITM_SIGNAL_CEB_FRAME_COUNTER].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_CEB_FRAME_COUNTER].value = 0xdead;
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0xabcd, frame, sizeof(frame)) == ITM_OK);
    TEST_ASSERT(frame[0] == 0x5a && frame[1] == 0x5a);
    TEST_ASSERT(frame[2] == 0xab && frame[3] == 0xcd);
    TEST_ASSERT(frame[4] == 0x60); /* valid, stale, invalid, invalid */
    TEST_ASSERT(frame[6] == 0x40); /* signal 8 valid */
    TEST_ASSERT(frame[21] == 0x10); /* signal 69 valid, padding zero */
    TEST_ASSERT(frame[22] == 0x6f && frame[23] == 0x54);
    TEST_ASSERT(frame[24] == 0x15 && frame[25] == 0x72);
    TEST_ASSERT(memcmp(&frame[40], "\x01\x02\x03\x04\x05\x06\x07\x08", 8) == 0);
    TEST_ASSERT(frame[202] == 0 && frame[203] == 10);
    crc = independent_crc(&frame[2], 202);
    TEST_ASSERT(frame[204] == (uint8_t)(crc >> 8) &&
                frame[205] == (uint8_t)crc);
    TEST_ASSERT(frame[206] == 0x5a && frame[207] == 0x5a);
    snapshot.captured_at_ms = 0x87654321U;
    {
        uint8_t without_time[ITM_RS422_TLM_FRAME_SIZE];
        TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0xabcd, without_time,
                                         sizeof(without_time)) == ITM_OK);
        TEST_ASSERT(memcmp(frame, without_time, sizeof(frame)) == 0);
    }
    return true;
}

bool test_itm_rs422_tlm_rejects_bad_values(void)
{
    itm_telemetry_snapshot_t snapshot = {0};
    uint8_t frame[ITM_RS422_TLM_FRAME_SIZE];
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0, frame, sizeof(frame) - 1U)
                == ITM_ERROR_NO_SPACE);
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].quality = ITM_SIGNAL_QUALITY_VALID;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].value = -1;
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0, frame, sizeof(frame))
                == ITM_ERROR_CORRUPT);
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].has_timestamp = false;
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0, frame, sizeof(frame)) == ITM_OK);
    TEST_ASSERT((frame[4] & 0xc0U) == 0 && frame[22] == 0 && frame[23] == 0);
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].value = 65536;
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0, frame, sizeof(frame))
                == ITM_ERROR_CORRUPT);
    snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].value = 0;
    snapshot.values[ITM_SIGNAL_CEB_PITCH_MAIN_LVDT].has_timestamp = true;
    snapshot.values[ITM_SIGNAL_CEB_PITCH_MAIN_LVDT].quality = ITM_SIGNAL_QUALITY_VALID;
    TEST_ASSERT(itm_rs422_tlm_encode(&snapshot, 0, frame, sizeof(frame))
                == ITM_ERROR_CORRUPT);
    return true;
}

#ifdef ITM_RS422_TLM_STANDALONE_TEST
int main(void)
{
    return test_itm_rs422_tlm_complete_frame() &&
           test_itm_rs422_tlm_rejects_bad_values() ? 0 : 1;
}
#endif
