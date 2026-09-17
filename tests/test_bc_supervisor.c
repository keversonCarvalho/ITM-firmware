#include "test_framework.h"
#include "itm/services/bc_supervisor.h"

bool test_bc_supervisor_trips_after_configured_attempts(void)
{
    itm_bc_supervisor_t supervisor;
    itm_diagnostics_t diagnostics;
    const itm_bc_supervisor_config_t config = {100U, 3U};

    itm_diagnostics_init(&diagnostics);
    TEST_ASSERT(itm_bc_supervisor_init(&supervisor, &config));
    itm_bc_supervisor_on_valid_frame(&supervisor, 10U, &diagnostics);
    itm_bc_supervisor_tick(&supervisor, 209U, &diagnostics);
    TEST_ASSERT_EQ(ITM_BC_DEGRADED, supervisor.state);
    itm_bc_supervisor_tick(&supervisor, 310U, &diagnostics);
    TEST_ASSERT_EQ(ITM_BC_LOST, supervisor.state);
    TEST_ASSERT(!itm_bc_supervisor_commands_allowed(&supervisor));
    TEST_ASSERT(diagnostics.blocking_fault);
    return true;
}

bool test_bc_supervisor_recovers_on_valid_frame(void)
{
    itm_bc_supervisor_t supervisor;
    itm_diagnostics_t diagnostics;
    itm_diag_event_t event;
    const itm_bc_supervisor_config_t config = {10U, 3U};

    itm_diagnostics_init(&diagnostics);
    TEST_ASSERT(itm_bc_supervisor_init(&supervisor, &config));
    itm_bc_supervisor_on_valid_frame(&supervisor, 0U, &diagnostics);
    itm_bc_supervisor_tick(&supervisor, 30U, &diagnostics);
    itm_bc_supervisor_on_valid_frame(&supervisor, 31U, &diagnostics);
    TEST_ASSERT_EQ(ITM_BC_CONNECTED, supervisor.state);
    TEST_ASSERT(itm_diagnostics_get(&diagnostics, 0U, &event));
    TEST_ASSERT_EQ(ITM_DIAG_BC_LINK_RESTORED, event.code);
    return true;
}
