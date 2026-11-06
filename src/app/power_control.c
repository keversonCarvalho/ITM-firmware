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
        (config->ceb_status_delay_ms == 0U) ||
        (config->cb_status_delay_ms == 0U) ||
        (config->off_status_delay_ms == 0U)) {
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
    bool ceb_on;
    bool cb_on;
    bool changed = false;
    uint32_t settle_ms;
    itm_power_state_t settled_state;

    if ((control == NULL) || (inputs == NULL)) {
        return;
    }

    if (control->state == ITM_POWER_FAULT) {
        return;
    }
    if ((!inputs->commands_allowed || inputs->blocking_fault) &&
        (control->ceb_commanded || control->cb_commanded)) {
        enter_fault(control, now_ms, ITM_DIAG_BC_LINK_LOST, diagnostics);
        return;
    }
    if (!inputs->commands_allowed || inputs->blocking_fault) {
        return;
    }
    ceb_on = control->request == ITM_POWER_REQUEST_CEB_ONLY ||
             control->request == ITM_POWER_REQUEST_SYSTEM_ON;
    cb_on = control->request == ITM_POWER_REQUEST_CB_ONLY ||
            control->request == ITM_POWER_REQUEST_SYSTEM_ON;
    if (control->cb_commanded != cb_on) {
        if (!set_output(&control->cb_output, cb_on)) {
            enter_fault(control, now_ms, ITM_DIAG_OUTPUT_WRITE_FAILED,
                        diagnostics);
            return;
        }
        control->cb_commanded = cb_on;
        changed = true;
    }
    if (control->ceb_commanded != ceb_on) {
        if (!set_output(&control->ceb_output, ceb_on)) {
            enter_fault(control, now_ms, ITM_DIAG_OUTPUT_WRITE_FAILED,
                        diagnostics);
            return;
        }
        control->ceb_commanded = ceb_on;
        changed = true;
    }
    if (changed) {
        enter_state(control, !ceb_on && !cb_on ? ITM_POWER_STOPPING :
                    cb_on ? ITM_POWER_CB_STARTING : ITM_POWER_CEB_STARTING,
                    now_ms);
    }
    settled_state = !ceb_on && !cb_on ? ITM_POWER_OFF :
                    ceb_on && cb_on ? ITM_POWER_OPERATIONAL :
                    ceb_on ? ITM_POWER_CEB_ON : ITM_POWER_CB_ON;
    settle_ms = !ceb_on && !cb_on ? control->config.off_status_delay_ms :
                ceb_on && cb_on ?
                    (control->config.ceb_status_delay_ms >
                     control->config.cb_status_delay_ms ?
                     control->config.ceb_status_delay_ms :
                     control->config.cb_status_delay_ms) :
                cb_on ? control->config.cb_status_delay_ms :
                        control->config.ceb_status_delay_ms;
    if (control->state != settled_state &&
        (uint32_t)(now_ms - control->state_entered_ms) >= settle_ms) {
        enter_state(control, settled_state, now_ms);
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
