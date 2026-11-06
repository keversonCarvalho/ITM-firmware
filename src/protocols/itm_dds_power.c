#include "itm/protocols/itm_dds_power.h"

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static uint64_t read_u64(const uint8_t *p)
{
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32U);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U); p[3] = (uint8_t)(value >> 24U);
}

static void write_u64(uint8_t *p, uint64_t value)
{
    write_u32(p, (uint32_t)value);
    write_u32(p + 4, (uint32_t)(value >> 32U));
}

itm_result_t itm_dds_power_command_deserialize(const uint8_t *data,
                                                size_t length,
                                                itm_dds_power_command_t *out)
{
    if (!data || !out || length != ITM_DDS_POWER_COMMAND_CDR_SIZE)
        return ITM_ERROR_INVALID_ARGUMENT;
    if (data[12] > 1U || data[13] > 1U)
        return ITM_ERROR_INVALID_ARGUMENT;
    out->session_id = read_u64(data);
    out->command_id = read_u32(data + 8);
    out->ceb_on = data[12] != 0U;
    out->cb_on = data[13] != 0U;
    return ITM_OK;
}

itm_result_t itm_dds_power_status_serialize(const itm_dds_power_status_t *status,
                                             uint8_t *data, size_t capacity,
                                             size_t *length)
{
    if (!status || !data || !length ||
        capacity < ITM_DDS_POWER_STATUS_CDR_SIZE)
        return ITM_ERROR_INVALID_ARGUMENT;
    write_u64(data, status->session_id);
    write_u32(data + 8, status->command_id);
    data[12] = status->result; data[13] = status->power_state;
    data[14] = status->ceb_requested ? 1U : 0U;
    data[15] = status->cb_requested ? 1U : 0U;
    data[16] = status->ceb_output_on ? 1U : 0U;
    data[17] = status->cb_output_on ? 1U : 0U;
    data[18] = status->cb_condition;
    data[19] = status->em1_valid ? 1U : 0U;
    data[20] = status->em2_valid ? 1U : 0U;
    data[21] = 0U; data[22] = 0U; data[23] = 0U;
    write_u32(data + 24, status->em1_mv);
    write_u32(data + 28, status->em2_mv);
    *length = ITM_DDS_POWER_STATUS_CDR_SIZE;
    return ITM_OK;
}
