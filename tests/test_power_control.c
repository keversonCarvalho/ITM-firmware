#include "test_framework.h"
#include "itm/app/power_control.h"

typedef struct {
    bool active;
    bool fail;
    unsigned writes;
} mock_output_t;

static itm_result_t write_output(void *context, bool active)
{
    mock_output_t *output = context;
    output->writes++;
    if (output->fail) {
        return ITM_ERROR_IO;
    }
    output->active = active;
    return ITM_OK;
}

static itm_power_control_t init_control(mock_output_t *ceb,
                                        mock_output_t *cb)
{
    itm_power_control_t control;
    const itm_power_config_t config = {100U, 100U, 10U};
    const itm_digital_output_port_t ceb_port = {ceb, write_output};
    const itm_digital_output_port_t cb_port = {cb, write_output};
    (void)itm_power_control_init(&control, &config, ceb_port, cb_port, 0U);
    return control;
}

bool test_power_control_sequences_outputs(void)
{
    mock_output_t ceb = {false, false, 0U};
    mock_output_t cb = {false, false, 0U};
    itm_power_control_t control = init_control(&ceb, &cb);
    itm_diagnostics_t diagnostics;
    itm_power_inputs_t inputs = {false, false, false, true, false};

    itm_diagnostics_init(&diagnostics);
    itm_power_control_request(&control, ITM_POWER_REQUEST_SYSTEM_ON);
    itm_power_control_tick(&control, 1U, &inputs, &diagnostics);
    TEST_ASSERT(ceb.active);
    TEST_ASSERT(!cb.active);
    TEST_ASSERT_EQ(ITM_POWER_CEB_STARTING, control.state);
    inputs.ceb_power_good = true;
    itm_power_control_tick(&control, 2U, &inputs, &diagnostics);
    TEST_ASSERT_EQ(ITM_POWER_CEB_ON, control.state);
    itm_power_control_tick(&control, 3U, &inputs, &diagnostics);
    TEST_ASSERT(cb.active);
    TEST_ASSERT_EQ(ITM_POWER_CB_STARTING, control.state);
    inputs.cb_feedback_valid = true;
    inputs.cb_is_on = true;
    itm_power_control_tick(&control, 4U, &inputs, &diagnostics);
    TEST_ASSERT_EQ(ITM_POWER_OPERATIONAL, control.state);
    return true;
}

bool test_power_control_fails_safe(void)
{
    mock_output_t ceb = {false, false, 0U};
    mock_output_t cb = {false, false, 0U};
    itm_power_control_t control = init_control(&ceb, &cb);
    itm_diagnostics_t diagnostics;
    itm_power_inputs_t inputs = {true, true, true, true, false};

    itm_diagnostics_init(&diagnostics);
    itm_power_control_request(&control, ITM_POWER_REQUEST_SYSTEM_ON);
    itm_power_control_tick(&control, 1U, &inputs, &diagnostics);
    itm_power_control_tick(&control, 2U, &inputs, &diagnostics);
    itm_power_control_tick(&control, 3U, &inputs, &diagnostics);
    itm_power_control_tick(&control, 4U, &inputs, &diagnostics);
    TEST_ASSERT(ceb.active && cb.active);
    inputs.commands_allowed = false;
    itm_power_control_tick(&control, 5U, &inputs, &diagnostics);
    TEST_ASSERT_EQ(ITM_POWER_FAULT, control.state);
    TEST_ASSERT(!ceb.active && !cb.active);
    TEST_ASSERT(diagnostics.blocking_fault);
    return true;
}
