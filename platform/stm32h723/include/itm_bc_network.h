#ifndef ITM_BC_NETWORK_H
#define ITM_BC_NETWORK_H
#include "itm/protocols/itm_dds_power.h"
#include <stdbool.h>

typedef enum {
    ITM_BC_EVENT_SESSION_STARTED = 0,
    ITM_BC_EVENT_SESSION_LOST,
    ITM_BC_EVENT_COMMAND,
    ITM_BC_EVENT_MALFORMED_COMMAND
} itm_bc_event_kind_t;

typedef struct {
    itm_bc_event_kind_t kind;
    uint64_t session_id;
    itm_dds_power_command_t command;
} itm_bc_event_t;

bool itm_bc_network_start(void);
bool itm_bc_network_ready(void);
bool itm_bc_network_take_event(itm_bc_event_t *event);
bool itm_bc_network_submit_status(const itm_dds_power_status_t *status);
#endif
