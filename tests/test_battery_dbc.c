#include "test_framework.h"
#include "mocks/mock_critical.h"

#include <string.h>

#include "battery_dbc_generated.h"
#include "itm/protocols/itm_dbcc.h"

static itm_dbcc_frame_t make_frame(itm_dbcc_bus_t bus, uint32_t id,
                                   uint8_t length)
{
    itm_dbcc_frame_t frame;
    (void)memset(&frame, 0, sizeof(frame));
    frame.bus = bus;
    frame.id = id;
    frame.is_extended = true;
    frame.length = length;
    return frame;
}

bool test_dbcc_decodes_independent_vectors(void)
{
    itm_dbcc_frame_t voltage = make_frame(
        ITM_DBCC_BUS_BATT_28V,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID, 8U);
    itm_dbcc_frame_t status = make_frame(
        ITM_DBCC_BUS_BATT_28V,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_SOC_SOH_TEMP_STAT_FRAME_ID, 8U);
    itm_dbcc_decoded_t decoded;
    const uint8_t voltage_data[8] =
        {0x41U, 0xE4U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1EU};

    (void)memcpy(voltage.data, voltage_data, sizeof(voltage_data));
    TEST_ASSERT_EQ(ITM_OK, itm_dbcc_decode(&voltage, &decoded));
    TEST_ASSERT_EQ(ITM_DBCC_MESSAGE_V_TOTAL, decoded.message);
    TEST_ASSERT_EQ(28500, decoded.values[0]);
    TEST_ASSERT_EQ(30000, decoded.values[1]);

    status.data[0] = 0x0CU;
    status.data[1] = 0xE4U;
    status.data[2] = 0x10U;
    status.data[3] = 0x04U;
    status.data[4] = 128U;
    status.data[5] = 255U;
    status.data[6] = 42U;
    status.data[7] = 0xA5U;
    TEST_ASSERT_EQ(ITM_OK, itm_dbcc_decode(&status, &decoded));
    TEST_ASSERT_EQ(3300, decoded.values[0]);
    TEST_ASSERT_EQ(4100, decoded.values[1]);
    TEST_ASSERT_EQ(5020, decoded.values[2]);
    TEST_ASSERT_EQ(10001, decoded.values[3]);
    return true;
}

bool test_dbcc_covers_all_dbc_messages(void)
{
    static const uint32_t ids[] = {
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_TEMPS_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_I_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_AH_WH_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_CELL_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_BAL_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_SOC_SOH_TEMP_STAT_FRAME_ID,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_HUM_FRAME_ID};
    size_t index;

    for (index = 0U; index < sizeof(ids) / sizeof(ids[0]); ++index) {
        const uint8_t length = ids[index] ==
            BATTERY_DBC_GENERATED_CAN_PACKET_BMS_HUM_FRAME_ID ? 6U : 8U;
        itm_dbcc_frame_t frame = make_frame(ITM_DBCC_BUS_BATT_28V,
                                             ids[index], length);
        itm_dbcc_decoded_t decoded;
        TEST_ASSERT_EQ(ITM_OK, itm_dbcc_decode(&frame, &decoded));
    }
    return true;
}

bool test_dbcc_generated_pack_unpack_roundtrip(void)
{
    struct battery_dbc_generated_can_packet_bms_v_cell_t source = {
        3U, 48U, 3300U, 3310U, 3320U};
    struct battery_dbc_generated_can_packet_bms_v_cell_t destination;
    uint8_t data[8];

    TEST_ASSERT_EQ(8, battery_dbc_generated_can_packet_bms_v_cell_pack(
                          data, &source, sizeof(data)));
    TEST_ASSERT_EQ(0, battery_dbc_generated_can_packet_bms_v_cell_unpack(
                          &destination, data, sizeof(data)));
    TEST_ASSERT_EQ(source.cell_point, destination.cell_point);
    TEST_ASSERT_EQ(source.cell_voltage12, destination.cell_voltage12);
    return true;
}

bool test_dbcc_rejects_bad_frames_and_float_values(void)
{
    itm_dbcc_frame_t frame = make_frame(
        ITM_DBCC_BUS_BATT_28V,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID, 7U);
    itm_dbcc_decoded_t decoded;

    TEST_ASSERT_EQ(ITM_ERROR_CORRUPT, itm_dbcc_decode(&frame, &decoded));
    frame.length = 8U;
    frame.is_extended = false;
    TEST_ASSERT_EQ(ITM_ERROR_CORRUPT, itm_dbcc_decode(&frame, &decoded));
    frame.is_extended = true;
    frame.id = 0x123U;
    TEST_ASSERT_EQ(ITM_ERROR_NOT_FOUND, itm_dbcc_decode(&frame, &decoded));
    frame.id = BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID;
    frame.data[0] = 0x7FU;
    frame.data[1] = 0xC0U;
    TEST_ASSERT_EQ(ITM_ERROR_CORRUPT, itm_dbcc_decode(&frame, &decoded));
    return true;
}

bool test_dbcc_ingest_isolates_buses_and_timestamps(void)
{
    itm_telemetry_store_t store;
    itm_dbcc_t dbcc;
    mock_critical_t critical = {0};
    itm_signal_runtime_t av;
    itm_signal_runtime_t act;
    itm_dbcc_frame_t frame = make_frame(
        ITM_DBCC_BUS_BATT_28V,
        BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID, 8U);
    const uint8_t data[8] =
        {0x41U, 0xE4U, 0x00U, 0x00U, 0U, 0U, 0U, 0U};

    TEST_ASSERT(itm_telemetry_store_init(&store,
                                         mock_critical_port(&critical)));
    TEST_ASSERT(itm_dbcc_init(&dbcc, &store));
    (void)memcpy(frame.data, data, sizeof(data));
    frame.timestamp_ms = 100U;
    TEST_ASSERT_EQ(ITM_OK, itm_dbcc_ingest(&dbcc, &frame));
    frame.bus = ITM_DBCC_BUS_BATT_150V;
    frame.timestamp_ms = 200U;
    TEST_ASSERT_EQ(ITM_OK, itm_dbcc_ingest(&dbcc, &frame));
    frame.timestamp_ms = 199U;
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_dbcc_ingest(&dbcc, &frame));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_AV_BATT_VOLTAGE, 200U, &av));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_read(
                               &store, ITM_SIGNAL_ACT_BATT_VOLTAGE, 200U, &act));
    TEST_ASSERT_EQ(100U, av.timestamp_ms);
    TEST_ASSERT_EQ(200U, act.timestamp_ms);
    TEST_ASSERT_EQ(1U, itm_dbcc_statistics(&dbcc).out_of_order_timestamp);
    return true;
}

bool test_dbcc_statistics_and_counter_wrap(void)
{
    itm_telemetry_store_t store;
    itm_dbcc_t dbcc;
    mock_critical_t critical = {0};
    itm_dbcc_frame_t frame = make_frame(ITM_DBCC_BUS_BATT_28V, 0x123U, 8U);

    TEST_ASSERT(itm_telemetry_store_init(&store,
                                         mock_critical_port(&critical)));
    TEST_ASSERT(itm_dbcc_init(&dbcc, &store));
    dbcc.statistics.frames_received = UINT32_MAX;
    TEST_ASSERT_EQ(ITM_ERROR_NOT_FOUND, itm_dbcc_ingest(&dbcc, &frame));
    TEST_ASSERT_EQ(0U, itm_dbcc_statistics(&dbcc).frames_received);
    TEST_ASSERT_EQ(1U, itm_dbcc_statistics(&dbcc).unknown_id);
    frame.id = BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID;
    frame.is_extended = false;
    TEST_ASSERT_EQ(ITM_ERROR_CORRUPT, itm_dbcc_ingest(&dbcc, &frame));
    TEST_ASSERT_EQ(1U, itm_dbcc_statistics(&dbcc).invalid_frame_type);
    frame.is_extended = true;
    frame.length = 7U;
    TEST_ASSERT_EQ(ITM_ERROR_CORRUPT, itm_dbcc_ingest(&dbcc, &frame));
    TEST_ASSERT_EQ(1U, itm_dbcc_statistics(&dbcc).invalid_dlc);
    return true;
}
