#include "itm/protocols/itm_dbcc.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "battery_dbc_generated.h"

static itm_result_t float_to_milli(float value, int64_t *converted)
{
    float scaled;

    if ((converted == NULL) || (value != value) ||
        (value > 2147483.0F) || (value < -2147483.0F)) {
        return ITM_ERROR_CORRUPT;
    }
    scaled = value * 1000.0F;
    *converted = (int64_t)(scaled + (scaled >= 0.0F ? 0.5F : -0.5F));
    return ITM_OK;
}

static uint8_t expected_length(uint32_t id)
{
    switch (id) {
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_TEMPS_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_I_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_AH_WH_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_CELL_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_BAL_FRAME_ID:
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_SOC_SOH_TEMP_STAT_FRAME_ID:
        return 8U;
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_HUM_FRAME_ID:
        return 6U;
    default:
        return 0U;
    }
}

static itm_result_t decode_temps(const itm_dbcc_frame_t *frame,
                                 itm_dbcc_decoded_t *decoded)
{
    struct battery_dbc_generated_can_packet_bms_temps_t value;
    if (battery_dbc_generated_can_packet_bms_temps_unpack(
            &value, frame->data, frame->length) != 0) {
        return ITM_ERROR_CORRUPT;
    }
    decoded->message = ITM_DBCC_MESSAGE_TEMPS;
    decoded->value_count = 4U;
    decoded->values[0] = value.no_of_cells;
    decoded->values[1] = value.aux_voltages_individual1;
    decoded->values[2] = value.aux_voltages_individual2;
    decoded->values[3] = value.aux_voltages_individual3;
    return ITM_OK;
}

static itm_result_t decode_float_pair(const itm_dbcc_frame_t *frame,
                                      itm_dbcc_decoded_t *decoded,
                                      itm_dbcc_message_t message)
{
    float first;
    float second;

    if (message == ITM_DBCC_MESSAGE_V_TOTAL) {
        struct battery_dbc_generated_can_packet_bms_v_tot_t value;
        if (battery_dbc_generated_can_packet_bms_v_tot_unpack(
                &value, frame->data, frame->length) != 0) {
            return ITM_ERROR_CORRUPT;
        }
        first = value.pack_voltage;
        decoded->values[1] = (int64_t)value.charger_voltage * 1000;
        if (float_to_milli(first, &decoded->values[0]) != ITM_OK) {
            return ITM_ERROR_CORRUPT;
        }
    } else if (message == ITM_DBCC_MESSAGE_CURRENT) {
        struct battery_dbc_generated_can_packet_bms_i_t value;
        if (battery_dbc_generated_can_packet_bms_i_unpack(
                &value, frame->data, frame->length) != 0) {
            return ITM_ERROR_CORRUPT;
        }
        first = value.pack_current1;
        second = value.pack_current2;
        if ((float_to_milli(first, &decoded->values[0]) != ITM_OK) ||
            (float_to_milli(second, &decoded->values[1]) != ITM_OK)) {
            return ITM_ERROR_CORRUPT;
        }
    } else {
        struct battery_dbc_generated_can_packet_bms_ah_wh_t value;
        if (battery_dbc_generated_can_packet_bms_ah_wh_unpack(
                &value, frame->data, frame->length) != 0) {
            return ITM_ERROR_CORRUPT;
        }
        first = value.ah_counter;
        second = value.wh_counter;
        if ((float_to_milli(first, &decoded->values[0]) != ITM_OK) ||
            (float_to_milli(second, &decoded->values[1]) != ITM_OK)) {
            return ITM_ERROR_CORRUPT;
        }
    }
    decoded->message = message;
    decoded->value_count = 2U;
    return ITM_OK;
}

static itm_result_t decode_integer_message(const itm_dbcc_frame_t *frame,
                                           itm_dbcc_decoded_t *decoded)
{
    if (frame->id == BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_CELL_FRAME_ID) {
        struct battery_dbc_generated_can_packet_bms_v_cell_t value;
        if (battery_dbc_generated_can_packet_bms_v_cell_unpack(
                &value, frame->data, frame->length) != 0) return ITM_ERROR_CORRUPT;
        decoded->message = ITM_DBCC_MESSAGE_CELL_VOLTAGE;
        decoded->value_count = 5U;
        decoded->values[0] = value.cell_point;
        decoded->values[1] = value.no_of_cells;
        decoded->values[2] = value.cell_voltage10;
        decoded->values[3] = value.cell_voltage11;
        decoded->values[4] = value.cell_voltage12;
    } else if (frame->id == BATTERY_DBC_GENERATED_CAN_PACKET_BMS_BAL_FRAME_ID) {
        struct battery_dbc_generated_can_packet_bms_bal_t value;
        if (battery_dbc_generated_can_packet_bms_bal_unpack(
                &value, frame->data, frame->length) != 0) return ITM_ERROR_CORRUPT;
        decoded->message = ITM_DBCC_MESSAGE_BALANCE;
        decoded->value_count = 2U;
        decoded->values[0] = value.no_of_cells;
        decoded->values[1] = (int64_t)value.bal_state;
    } else if (frame->id == BATTERY_DBC_GENERATED_CAN_PACKET_BMS_SOC_SOH_TEMP_STAT_FRAME_ID) {
        struct battery_dbc_generated_can_packet_bms_soc_soh_temp_stat_t value;
        if (battery_dbc_generated_can_packet_bms_soc_soh_temp_stat_unpack(
                &value, frame->data, frame->length) != 0) return ITM_ERROR_CORRUPT;
        decoded->message = ITM_DBCC_MESSAGE_STATUS;
        decoded->value_count = 6U;
        decoded->values[0] = value.cell_voltage_low;
        decoded->values[1] = value.cell_voltage_high;
        decoded->values[2] = ((int64_t)value.soc * 10000 + 127) / 255;
        decoded->values[3] = ((int64_t)value.soh * 3922 + 50) / 100;
        decoded->values[4] = value.t_batt_hi;
        decoded->values[5] = value.bit_f;
    } else {
        struct battery_dbc_generated_can_packet_bms_hum_t value;
        if (battery_dbc_generated_can_packet_bms_hum_unpack(
                &value, frame->data, frame->length) != 0) return ITM_ERROR_CORRUPT;
        decoded->message = ITM_DBCC_MESSAGE_HUMIDITY;
        decoded->value_count = 3U;
        decoded->values[0] = value.can_packet_bms_temp0;
        decoded->values[1] = value.can_packet_bms_hum_hum;
        decoded->values[2] = value.can_packet_bms_hum_temp1;
    }
    return ITM_OK;
}

bool itm_dbcc_init(itm_dbcc_t *dbcc, itm_telemetry_store_t *telemetry)
{
    if ((dbcc == NULL) || (telemetry == NULL)) return false;
    (void)memset(dbcc, 0, sizeof(*dbcc));
    dbcc->telemetry = telemetry;
    return true;
}

itm_result_t itm_dbcc_decode(const itm_dbcc_frame_t *frame,
                             itm_dbcc_decoded_t *decoded)
{
    uint8_t length;
    if ((frame == NULL) || (decoded == NULL)) return ITM_ERROR_INVALID_ARGUMENT;
    length = expected_length(frame->id);
    if (length == 0U) return ITM_ERROR_NOT_FOUND;
    if (!frame->is_extended || (frame->length != length)) return ITM_ERROR_CORRUPT;
    (void)memset(decoded, 0, sizeof(*decoded));
    switch (frame->id) {
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_TEMPS_FRAME_ID:
        return decode_temps(frame, decoded);
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID:
        return decode_float_pair(frame, decoded, ITM_DBCC_MESSAGE_V_TOTAL);
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_I_FRAME_ID:
        return decode_float_pair(frame, decoded, ITM_DBCC_MESSAGE_CURRENT);
    case BATTERY_DBC_GENERATED_CAN_PACKET_BMS_AH_WH_FRAME_ID:
        return decode_float_pair(frame, decoded, ITM_DBCC_MESSAGE_ENERGY);
    default:
        return decode_integer_message(frame, decoded);
    }
}

itm_result_t itm_dbcc_ingest(itm_dbcc_t *dbcc,
                             const itm_dbcc_frame_t *frame)
{
    itm_dbcc_decoded_t decoded;
    itm_result_t result;
    itm_signal_source_t source;
    itm_signal_id_t id;
    int64_t value;
    itm_telemetry_statistics_t before;
    itm_telemetry_statistics_t after;

    if ((dbcc == NULL) || (frame == NULL) || (dbcc->telemetry == NULL))
        return ITM_ERROR_INVALID_ARGUMENT;
    if ((frame->bus != ITM_DBCC_BUS_BATT_28V) &&
        (frame->bus != ITM_DBCC_BUS_BATT_150V))
        return ITM_ERROR_INVALID_ARGUMENT;
    dbcc->statistics.frames_received++;
    if (expected_length(frame->id) == 0U) {
        dbcc->statistics.unknown_id++;
        return ITM_ERROR_NOT_FOUND;
    }
    if (!frame->is_extended) {
        dbcc->statistics.invalid_frame_type++;
        return ITM_ERROR_CORRUPT;
    }
    if (frame->length != expected_length(frame->id)) {
        dbcc->statistics.invalid_dlc++;
        return ITM_ERROR_CORRUPT;
    }
    result = itm_dbcc_decode(frame, &decoded);
    if (result != ITM_OK) {
        dbcc->statistics.conversion_error++;
        return result;
    }
    dbcc->statistics.frames_decoded++;
    if ((decoded.message != ITM_DBCC_MESSAGE_V_TOTAL) &&
        (decoded.message != ITM_DBCC_MESSAGE_STATUS)) return ITM_OK;
    source = frame->bus == ITM_DBCC_BUS_BATT_28V ? ITM_SOURCE_AV_BATT
                                                  : ITM_SOURCE_ACT_BATT;
    id = decoded.message == ITM_DBCC_MESSAGE_V_TOTAL
             ? (source == ITM_SOURCE_AV_BATT ? ITM_SIGNAL_AV_BATT_VOLTAGE
                                             : ITM_SIGNAL_ACT_BATT_VOLTAGE)
             : (source == ITM_SOURCE_AV_BATT ? ITM_SIGNAL_AV_BATT_SOC
                                             : ITM_SIGNAL_ACT_BATT_SOC);
    value = decoded.message == ITM_DBCC_MESSAGE_V_TOTAL ? decoded.values[0]
                                                        : decoded.values[2];
    before = itm_telemetry_store_statistics(dbcc->telemetry);
    result = itm_telemetry_store_update(dbcc->telemetry, source, id,
                                        ITM_SIGNAL_TYPE_U32, value,
                                        frame->timestamp_ms);
    after = itm_telemetry_store_statistics(dbcc->telemetry);
    dbcc->statistics.invalid_value += after.rejected_range - before.rejected_range;
    dbcc->statistics.out_of_order_timestamp +=
        after.rejected_timestamp - before.rejected_timestamp;
    return result;
}

itm_dbcc_statistics_t itm_dbcc_statistics(const itm_dbcc_t *dbcc)
{
    itm_dbcc_statistics_t empty = {0};
    return dbcc == NULL ? empty : dbcc->statistics;
}
