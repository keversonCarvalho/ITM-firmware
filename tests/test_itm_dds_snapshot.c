#include "test_framework.h"
#include "itm/protocols/itm_dds_snapshot.h"

#include <string.h>

bool test_itm_dds_snapshot_contract(void)
{
    itm_telemetry_snapshot_t source = {0};
    itm_dds_snapshot_t topic;
    uint8_t wire[ITM_DDS_CDR_SIZE];
    size_t written = 0U;
    source.sequence = 42U;
    source.captured_at_ms = 1234U;
    source.values[0].has_timestamp = true;
    source.values[0].quality = ITM_SIGNAL_QUALITY_VALID;
    source.values[0].value = 28500;
    source.values[1].has_timestamp = true;
    source.values[1].quality = ITM_SIGNAL_QUALITY_STALE;
    source.values[1].value = 5490;
    source.values[ITM_SIGNAL_CEB_CAN_HEALTH].has_timestamp = true;
    source.values[ITM_SIGNAL_CEB_CAN_HEALTH].quality = ITM_SIGNAL_QUALITY_VALID;
    source.values[ITM_SIGNAL_CEB_CAN_HEALTH].has_ceb_raw = true;
    source.values[ITM_SIGNAL_CEB_CAN_HEALTH].ceb_raw = 10U;
    source.values[ITM_SIGNAL_CEB_CAN_HEALTH].value = 99;
    source.values[ITM_SIGNAL_CEB_FRAME_COUNTER].has_timestamp = true;
    source.values[ITM_SIGNAL_CEB_FRAME_COUNTER].value = 777;
    TEST_ASSERT(itm_dds_snapshot_prepare(&source, &topic) == ITM_OK);
    TEST_ASSERT(topic.sequence == 42U);
    TEST_ASSERT(topic.quality[0] == 0x60U);
    TEST_ASSERT(topic.value[0] == 28500 && topic.value[1] == 5490);
    TEST_ASSERT(topic.value[ITM_SIGNAL_CEB_CAN_HEALTH] == 10);
    topic.cb_condition = 5U;
    TEST_ASSERT(itm_dds_snapshot_serialize(&topic, wire, sizeof(wire),
                                           &written) == ITM_OK);
    TEST_ASSERT(written == 213U);
    TEST_ASSERT(memcmp(wire, "\x2a\0", 2U) == 0);
    TEST_ASSERT(wire[2] == 0x60U);
    TEST_ASSERT(wire[3] == 0U);
    TEST_ASSERT(wire[20] == 0x54U && wire[21] == 0x6fU);
    TEST_ASSERT(wire[210] == 10U);
    TEST_ASSERT(wire[212] == 5U);
    TEST_ASSERT(itm_dds_snapshot_serialize(&topic, wire, sizeof(wire)-1U,
                                           &written) == ITM_ERROR_NO_SPACE);
    return true;
}
