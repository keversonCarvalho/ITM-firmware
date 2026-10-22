#include "test_framework.h"
#include "mocks/mock_critical.h"
#include "itm/protocols/ceb_tlm.h"
#include "itm/protocols/itm_rs422_tlm.h"
#include <string.h>

static void be16_write(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static uint16_t reference_crc(const uint8_t *data, unsigned count)
{
    uint16_t crc = 0xffffU;
    unsigned i, bit;
    for (i = 0; i < count; ++i) {
        crc ^= (uint16_t)data[i] << 8U;
        for (bit = 0; bit < 8; ++bit) {
            const bool top = (crc & 0x8000U) != 0;
            crc = (uint16_t)(crc << 1U);
            if (top) crc ^= 0x1021U;
        }
    }
    return crc;
}

static void make_frame(uint8_t frame[ITM_CEB_TLM_FRAME_SIZE], uint16_t counter)
{
    memset(frame, 0, ITM_CEB_TLM_FRAME_SIZE);
    frame[0] = frame[1] = frame[42] = frame[43] = 0x5a;
    be16_write(frame + 2, counter);
    be16_write(frame + 4, 16384);   /* Pitch LVDT: 5 V. */
    be16_write(frame + 6, 0xc000);  /* Mirror LVDT: -5 V. */
    be16_write(frame + 12, 2048);  /* Pitch current: 0 A. */
    be16_write(frame + 14, 3685);  /* Yaw current: 150 A. */
    be16_write(frame + 16, 8192);  /* Pitch actuator: 0 C. */
    be16_write(frame + 18, 0x2000);
    be16_write(frame + 20, 0x2000);
    be16_write(frame + 22, 0x2000);
    be16_write(frame + 24, 2048);  /* CEB 150 V reading: 150 V. */
    be16_write(frame + 26, 2730);  /* CEB 28 V reading: 28 V. */
    be16_write(frame + 28, 16384); /* Pitch command: 15 mm. */
    be16_write(frame + 30, 0xc000);
    be16_write(frame + 32, 0x4000);
    be16_write(frame + 34, 0x4000);
    be16_write(frame + 36, 0x0101);
    be16_write(frame + 38, 0x00a5);
    be16_write(frame + 40, reference_crc(frame + 2, 38));
}

static void feed(itm_ceb_tlm_t *decoder, const uint8_t *frame, size_t count,
                 uint32_t timestamp_ms)
{
    size_t i;
    for (i = 0; i < count; ++i)
        itm_ceb_tlm_ingest_byte(decoder, frame[i], timestamp_ms);
}

bool test_ceb_tlm_decodes_full_snapshot(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store;
    itm_telemetry_snapshot_t snapshot;
    itm_ceb_tlm_t decoder;
    uint8_t frame[ITM_CEB_TLM_FRAME_SIZE];
    unsigned id;
    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_store_init(&store, mock_critical_port(&critical)));
    TEST_ASSERT(itm_ceb_tlm_init(&decoder, &store));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_put(&store, ITM_SOURCE_AV_BATT,
        ITM_SIGNAL_AV_BATT_VOLTAGE, ITM_SIGNAL_TYPE_U32, 28500, 9));
    make_frame(frame, 7);
    feed(&decoder, frame, sizeof(frame), 10);
    TEST_ASSERT_EQ(1U, decoder.valid_frames);
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_snapshot(&store, 10, &snapshot));
    for (id = ITM_SIGNAL_CEB_PITCH_MAIN_LVDT; id <= ITM_SIGNAL_CEB_FRAME_COUNTER; ++id) {
        TEST_ASSERT_EQ(ITM_SOURCE_CEB, itm_telemetry_catalog[id].source);
        TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_VALID, snapshot.values[id].quality);
    }
    TEST_ASSERT_EQ(5000, snapshot.values[ITM_SIGNAL_CEB_PITCH_MAIN_LVDT].value);
    TEST_ASSERT(snapshot.values[ITM_SIGNAL_CEB_PITCH_MAIN_LVDT].has_ceb_raw);
    TEST_ASSERT_EQ(0x4000, snapshot.values[ITM_SIGNAL_CEB_PITCH_MAIN_LVDT].ceb_raw);
    TEST_ASSERT_EQ(-5000, snapshot.values[ITM_SIGNAL_CEB_PITCH_MIRROR_LVDT].value);
    TEST_ASSERT_EQ(0, snapshot.values[ITM_SIGNAL_CEB_PITCH_CURRENT].value);
    TEST_ASSERT_EQ(150000, snapshot.values[ITM_SIGNAL_CEB_YAW_CURRENT].value);
    TEST_ASSERT_EQ(0, snapshot.values[ITM_SIGNAL_CEB_PITCH_ACTUATOR_TEMPERATURE].value);
    TEST_ASSERT_EQ(150036, snapshot.values[ITM_SIGNAL_CEB_BATTERY_150V_VOLTAGE].value);
    TEST_ASSERT_EQ(28000, snapshot.values[ITM_SIGNAL_CEB_SUPPLY_28V_VOLTAGE].value);
    TEST_ASSERT_EQ(15000, snapshot.values[ITM_SIGNAL_CEB_PITCH_COMMAND].value);
    TEST_ASSERT_EQ(0x0101, snapshot.values[ITM_SIGNAL_CEB_HEALTH].value);
    TEST_ASSERT_EQ(0x00a5, snapshot.values[ITM_SIGNAL_CEB_CAN_HEALTH].value);
    {
        uint8_t outbound[ITM_RS422_TLM_FRAME_SIZE];
        TEST_ASSERT_EQ(ITM_OK, itm_rs422_tlm_encode(&snapshot, 1U,
                                                     outbound, sizeof(outbound)));
        TEST_ASSERT(outbound[168] == 0x40 && outbound[169] == 0x00);
        TEST_ASSERT(outbound[170] == 0xc0 && outbound[171] == 0x00);
        TEST_ASSERT(outbound[202] == 0x00 && outbound[203] == 0xa5);
    }
    TEST_ASSERT_EQ(7, snapshot.values[ITM_SIGNAL_CEB_FRAME_COUNTER].value);
    TEST_ASSERT_EQ(28500, snapshot.values[ITM_SIGNAL_AV_BATT_VOLTAGE].value);
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_snapshot(&store, 111, &snapshot));
    TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_STALE,
                   snapshot.values[ITM_SIGNAL_CEB_FRAME_COUNTER].quality);
    return true;
}

bool test_ceb_tlm_rejects_corruption_and_resynchronizes(void)
{
    mock_critical_t critical;
    itm_telemetry_store_t store;
    itm_ceb_tlm_t decoder;
    uint8_t frame[ITM_CEB_TLM_FRAME_SIZE];
    itm_signal_runtime_t value;
    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_store_init(&store, mock_critical_port(&critical)));
    TEST_ASSERT(itm_ceb_tlm_init(&decoder, &store));
    make_frame(frame, 65535);
    frame[12] ^= 1;
    feed(&decoder, frame, sizeof(frame), 1);
    TEST_ASSERT_EQ(1U, decoder.crc_errors);
    TEST_ASSERT_EQ(0U, decoder.valid_frames);
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(&store,
        ITM_SIGNAL_CEB_FRAME_COUNTER, 1, &value));
    TEST_ASSERT_EQ(ITM_SIGNAL_QUALITY_INVALID, value.quality);
    make_frame(frame, 65535);
    feed(&decoder, frame, sizeof(frame), 2);
    make_frame(frame, 0);
    feed(&decoder, frame, sizeof(frame), 12);
    TEST_ASSERT_EQ(2U, decoder.valid_frames);
    TEST_ASSERT_EQ(0U, decoder.counter_gaps);
    make_frame(frame, 2);
    feed(&decoder, frame, sizeof(frame), 22);
    TEST_ASSERT_EQ(1U, decoder.counter_gaps);
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(&store,
        ITM_SIGNAL_CEB_FRAME_COUNTER, 22, &value));
    TEST_ASSERT_EQ(2, value.value);
    return true;
}
