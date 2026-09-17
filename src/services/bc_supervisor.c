#include "itm/services/bc_supervisor.h"

#include <stddef.h>

bool itm_bc_supervisor_init(itm_bc_supervisor_t *supervisor,
                            const itm_bc_supervisor_config_t *config)
{
    if ((supervisor == NULL) || (config == NULL) ||
        (config->supervision_period_ms == 0U) ||
        (config->missed_periods_before_lost == 0U)) {
        return false;
    }

    supervisor->config = *config;
    supervisor->state = ITM_BC_WAITING;
    supervisor->last_valid_frame_ms = 0U;
    supervisor->missed_periods = 0U;
    supervisor->has_received_frame = false;
    return true;
}

void itm_bc_supervisor_on_valid_frame(itm_bc_supervisor_t *supervisor,
                                      uint32_t now_ms,
                                      itm_diagnostics_t *diagnostics)
{
    const bool was_lost = (supervisor != NULL) &&
                          (supervisor->state == ITM_BC_LOST);

    if (supervisor == NULL) {
        return;
    }

    supervisor->last_valid_frame_ms = now_ms;
    supervisor->missed_periods = 0U;
    supervisor->has_received_frame = true;
    supervisor->state = ITM_BC_CONNECTED;

    if (was_lost) {
        itm_diagnostics_report(diagnostics, (itm_diag_event_t){
            now_ms, ITM_DIAG_BC_LINK_RESTORED, ITM_DIAG_INFO, 0U});
    }
}

void itm_bc_supervisor_tick(itm_bc_supervisor_t *supervisor, uint32_t now_ms,
                            itm_diagnostics_t *diagnostics)
{
    uint32_t elapsed;
    uint32_t missed;

    if ((supervisor == NULL) || !supervisor->has_received_frame ||
        (supervisor->state == ITM_BC_LOST)) {
        return;
    }

    elapsed = now_ms - supervisor->last_valid_frame_ms;
    missed = elapsed / supervisor->config.supervision_period_ms;
    if (missed > UINT8_MAX) {
        missed = UINT8_MAX;
    }
    supervisor->missed_periods = (uint8_t)missed;

    if (supervisor->missed_periods >=
        supervisor->config.missed_periods_before_lost) {
        supervisor->state = ITM_BC_LOST;
        itm_diagnostics_report(diagnostics, (itm_diag_event_t){
            now_ms, ITM_DIAG_BC_LINK_LOST, ITM_DIAG_FAULT,
            supervisor->missed_periods});
    } else if (supervisor->missed_periods > 0U) {
        supervisor->state = ITM_BC_DEGRADED;
    }
}

bool itm_bc_supervisor_commands_allowed(const itm_bc_supervisor_t *supervisor)
{
    return (supervisor != NULL) &&
           (supervisor->state == ITM_BC_CONNECTED);
}
