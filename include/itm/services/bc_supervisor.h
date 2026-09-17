#ifndef ITM_SERVICES_BC_SUPERVISOR_H
#define ITM_SERVICES_BC_SUPERVISOR_H

#include <stdbool.h>
#include <stdint.h>
#include "itm/services/diagnostics.h"

typedef enum {
    ITM_BC_WAITING = 0,
    ITM_BC_CONNECTED,
    ITM_BC_DEGRADED,
    ITM_BC_LOST
} itm_bc_link_state_t;

typedef struct {
    uint32_t supervision_period_ms;
    uint8_t missed_periods_before_lost;
} itm_bc_supervisor_config_t;

typedef struct {
    itm_bc_supervisor_config_t config;
    itm_bc_link_state_t state;
    uint32_t last_valid_frame_ms;
    uint8_t missed_periods;
    bool has_received_frame;
} itm_bc_supervisor_t;

bool itm_bc_supervisor_init(itm_bc_supervisor_t *supervisor,
                            const itm_bc_supervisor_config_t *config);
void itm_bc_supervisor_on_valid_frame(itm_bc_supervisor_t *supervisor,
                                      uint32_t now_ms,
                                      itm_diagnostics_t *diagnostics);
void itm_bc_supervisor_tick(itm_bc_supervisor_t *supervisor, uint32_t now_ms,
                            itm_diagnostics_t *diagnostics);
bool itm_bc_supervisor_commands_allowed(const itm_bc_supervisor_t *supervisor);

#endif
