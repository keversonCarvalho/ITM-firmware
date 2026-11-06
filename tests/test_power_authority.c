#include "test_framework.h"
#include "itm/services/power_authority.h"

bool test_power_authority_prioritizes_bc_and_expires(void)
{
    itm_power_authority_t authority;
    itm_dds_power_command_t command = {UINT64_C(0x1234), 1U, true, false};
    itm_power_authority_init(&authority, true);
    TEST_ASSERT(itm_power_authority_bench(&authority,
        ITM_POWER_REQUEST_CB_ONLY, 0U, false) == ITM_BENCH_ACCEPTED);
    itm_power_authority_session_started(&authority, command.session_id, 10U);
    TEST_ASSERT(authority.request == ITM_POWER_REQUEST_OFF);
    TEST_ASSERT(itm_power_authority_bench(&authority,
        ITM_POWER_REQUEST_CEB_ONLY, 11U, false) == ITM_BENCH_BC_OWNS);
    itm_power_authority_command(&authority, &command, 20U, false);
    TEST_ASSERT(authority.request == ITM_POWER_REQUEST_CEB_ONLY);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_ACCEPTED);
    itm_power_authority_tick(&authority, 1019U);
    TEST_ASSERT(authority.request == ITM_POWER_REQUEST_CEB_ONLY);
    itm_power_authority_tick(&authority, 1020U);
    TEST_ASSERT(authority.request == ITM_POWER_REQUEST_OFF);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_EXPIRED);
    return true;
}

bool test_power_authority_rejects_stale_and_can_disable_bench(void)
{
    itm_power_authority_t authority;
    itm_dds_power_command_t command = {77U, 8U, false, true};
    itm_power_authority_init(&authority, false);
    TEST_ASSERT(itm_power_authority_bench(&authority,
        ITM_POWER_REQUEST_CEB_ONLY, 0U, false) == ITM_BENCH_DISABLED);
    itm_power_authority_session_started(&authority, 77U, 0U);
    itm_power_authority_command(&authority, &command, 1U, false);
    command.command_id = 7U;
    itm_power_authority_command(&authority, &command, 2U, false);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_OLD_COMMAND);
    command.command_id = 9U; command.session_id = 78U;
    itm_power_authority_command(&authority, &command, 3U, false);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_WRONG_SESSION);
    itm_power_authority_session_lost(&authority, 4U);
    TEST_ASSERT(authority.request == ITM_POWER_REQUEST_OFF);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_LINK_LOSS);
    itm_power_authority_session_started(&authority, 79U, 5U);
    TEST_ASSERT(authority.result == ITM_POWER_RESULT_LINK_LOSS);
    return true;
}
