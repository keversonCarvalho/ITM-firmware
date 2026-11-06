#include "test_framework.h"
#include "itm/protocols/itm_dds_power.h"
#include <string.h>

bool test_itm_dds_power_cdr_contract(void)
{
    const uint8_t command_wire[ITM_DDS_POWER_COMMAND_CDR_SIZE] = {
        0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,
        0x44,0x33,0x22,0x11,1,0};
    itm_dds_power_command_t command;
    itm_dds_power_status_t status = {0};
    uint8_t wire[ITM_DDS_POWER_STATUS_CDR_SIZE];
    size_t length = 0U;
    TEST_ASSERT(itm_dds_power_command_deserialize(command_wire,
        sizeof(command_wire), &command) == ITM_OK);
    TEST_ASSERT(command.session_id == UINT64_C(0x0102030405060708));
    TEST_ASSERT(command.command_id == 0x11223344U);
    TEST_ASSERT(command.ceb_on && !command.cb_on);
    status.session_id = command.session_id; status.command_id = command.command_id;
    status.result = 2U; status.power_state = 4U;
    status.ceb_requested = true; status.cb_requested = true;
    status.ceb_output_on = true; status.cb_condition = 5U;
    status.em1_valid = true; status.em1_mv = 176400U;
    status.em2_mv = UINT32_MAX;
    TEST_ASSERT(itm_dds_power_status_serialize(&status, wire, sizeof(wire),
                                               &length) == ITM_OK);
    TEST_ASSERT(length == ITM_DDS_POWER_STATUS_CDR_SIZE);
    TEST_ASSERT(memcmp(wire, command_wire, 12U) == 0);
    TEST_ASSERT(wire[12] == 2U && wire[13] == 4U && wire[14] == 1U);
    TEST_ASSERT(wire[21] == 0U && wire[22] == 0U && wire[23] == 0U);
    TEST_ASSERT(wire[24] == 0x10U && wire[25] == 0xb1U && wire[26] == 0x02U);
    return true;
}

bool test_itm_dds_power_rejects_malformed_command(void)
{
    uint8_t wire[ITM_DDS_POWER_COMMAND_CDR_SIZE] = {0};
    itm_dds_power_command_t command;
    wire[12] = 2U;
    TEST_ASSERT(itm_dds_power_command_deserialize(wire, sizeof(wire),
        &command) == ITM_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(itm_dds_power_command_deserialize(wire, sizeof(wire)-1U,
        &command) == ITM_ERROR_INVALID_ARGUMENT);
    return true;
}
