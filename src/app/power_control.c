#include "itm/app/power_control.h"

#include <stddef.h>

static bool set_output(itm_digital_output_port_t *port, bool active)
{
    return (port->write != NULL) &&
           (port->write(port->context, active) == ITM_OK);
}

static void enter_state(itm_power_control_t *control,
                        itm_power_state_t state, uint32_t now_ms)
{
    control->state = state;
    control->state_entered_ms = now_ms;
}

static void enter_fault(itm_power_control_t *control, uint32_t now_ms,
                        itm_diag_code_t code, itm_diagnostics_t *diagnostics)
{
    (void)set_output(&control->cb_output, false);
    (void)set_output(&control->ceb_output, false);
    control->cb_commanded = false;
    control->ceb_commanded = false;
    control->request = ITM_POWER_REQUEST_OFF;
    enter_state(control, ITM_POWER_FAULT, now_ms);
    itm_diagnostics_report(diagnostics, (itm_diag_event_t){
        now_ms, code, ITM_DIAG_FAULT, 0U});
}

bool itm_power_control_init(itm_power_control_t *control,
                            const itm_power_config_t *config,
                            itm_digital_output_port_t ceb_output,
                            itm_digital_output_port_t cb_output,
                            uint32_t now_ms)
{
    bool cb_safe;
    bool ceb_safe;

    if ((control == NULL) || (config == NULL) ||
        (ceb_output.write == NULL) || (cb_output.write == NULL) ||
        (config->ceb_start_timeout_ms == 0U) ||
        (config->cb_start_timeout_ms == 0U)) {
        return false;
    }

    control->config = *config;
    control->ceb_output = ceb_output;
    control->cb_output = cb_output;
    control->state = ITM_POWER_OFF;
    control->request = ITM_POWER_REQUEST_OFF;
    control->state_entered_ms = now_ms;
    control->ceb_commanded = false;
    control->cb_commanded = false;

    cb_safe = set_output(&control->cb_output, false);
    ceb_safe = set_output(&control->ceb_output, false);
    return cb_safe && ceb_safe;
}

void itm_power_control_request(itm_power_control_t *control,
                               itm_power_request_t request)
{
    if (control != NULL) {
        control->request = request;
    }
}

void itm_power_control_tick(itm_power_control_t *control, uint32_t now_ms,
                            const itm_power_inputs_t *inputs,
                            itm_diagnostics_t *diagnostics)
{
    const uint32_t elapsed = (control != NULL)
                                 ? now_ms - control->state_entered_ms
                                 : 0U;

    if ((control == NULL) || (inputs == NULL)) {
        return;
    }

    if ((!inputs->commands_allowed || inputs->blocking_fault) &&
        (control->state != ITM_POWER_OFF) &&
        (control->state != ITM_POWER_FAULT)) {
        enter_fault(control, now_ms, ITM_DIAG_BC_LINK_LOST, diagnostics);
        return;
    }

    switch (control->state) {
    case ITM_POWER_OFF:
        if ((control->request != ITM_POWER_REQUEST_OFF) &&
            inputs->commands_allowed && !inputs->blocking_fault) {
            if (!set_output(&control->ceb_output, true)) {
                enter_fault(control, now_ms, ITM_DIAG_OUTPUT_WRITE_FAILED,
                            diagnostics);
                break;
            }
            control->ceb_commanded = true;
            enter_state(control, ITM_POWER_CEB_STARTING, now_ms);
        }
        break;

    case ITM_POWER_CEB_STARTING:
        if (control->request == ITM_POWER_REQUEST_OFF) {
            enter_state(control, ITM_POWER_STOPPING, now_ms);
        } else if (inputs->ceb_power_good) {
            enter_state(control, ITM_POWER_CEB_ON, now_ms);
        } else if (elapsed >= control->config.ceb_start_timeout_ms) {
            enter_fault(control, now_ms, ITM_DIAG_POWER_TRANSITION_TIMEOUT,
                        diagnostics);
        }
        break;

    case ITM_POWER_CEB_ON:
        if (control->request == ITM_POWER_REQUEST_OFF) {
            enter_state(control, ITM_POWER_STOPPING, now_ms);
        } else if (control->request == ITM_POWER_REQUEST_SYSTEM_ON) {
            if (!set_output(&control->cb_output, true)) {
                enter_fault(control, now_ms, ITM_DIAG_OUTPUT_WRITE_FAILED,
                            diagnostics);
                break;
            }
            control->cb_commanded = true;
            enter_state(control, ITM_POWER_CB_STARTING, now_ms);
        }
        break;

    case ITM_POWER_CB_STARTING:
        if (control->request != ITM_POWER_REQUEST_SYSTEM_ON) {
            (void)set_output(&control->cb_output, false);
            control->cb_commanded = false;
            enter_state(control, ITM_POWER_CEB_ON, now_ms);
        } else if (inputs->cb_feedback_valid && inputs->cb_is_on) {
            enter_state(control, ITM_POWER_OPERATIONAL, now_ms);
        } else if (elapsed >= control->config.cb_start_timeout_ms) {
            enter_fault(control, now_ms, ITM_DIAG_POWER_TRANSITION_TIMEOUT,
                        diagnostics);
        }
        break;

    case ITM_POWER_OPERATIONAL:
        if (control->request != ITM_POWER_REQUEST_SYSTEM_ON) {
            (void)set_output(&control->cb_output, false);
            control->cb_commanded = false;
            enter_state(control, (control->request == ITM_POWER_REQUEST_OFF)
                                     ? ITM_POWER_STOPPING
                                     : ITM_POWER_CEB_ON,
                        now_ms);
        } else if (!inputs->cb_feedback_valid || !inputs->cb_is_on) {
            enter_fault(control, now_ms, ITM_DIAG_CB_STATE_MISMATCH,
                        diagnostics);
        }
        break;

    case ITM_POWER_STOPPING:
        if (control->cb_commanded) {
            (void)set_output(&control->cb_output, false);
            control->cb_commanded = false;
            control->state_entered_ms = now_ms;
        } else if (elapsed >= control->config.shutdown_settle_ms) {
            if (!set_output(&control->ceb_output, false)) {
                enter_fault(control, now_ms, ITM_DIAG_OUTPUT_WRITE_FAILED,
                            diagnostics);
                break;
            }
            control->ceb_commanded = false;
            enter_state(control, ITM_POWER_OFF, now_ms);
        }
        break;

    case ITM_POWER_FAULT:
    default:
        break;
    }
}

bool itm_power_control_clear_fault(itm_power_control_t *control,
                                   uint32_t now_ms,
                                   const itm_power_inputs_t *inputs)
{
    if ((control == NULL) || (inputs == NULL) ||
        (control->state != ITM_POWER_FAULT) ||
        !inputs->commands_allowed || inputs->blocking_fault) {
        return false;
    }

    control->request = ITM_POWER_REQUEST_OFF;
    enter_state(control, ITM_POWER_OFF, now_ms);
    return true;
}
