#ifndef ITM_APP_POWER_CONTROL_H
#define ITM_APP_POWER_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "itm/core/result.h"
#include "itm/ports/digital_output.h"
#include "itm/services/diagnostics.h"

typedef enum {
    ITM_POWER_OFF = 0,
    ITM_POWER_CEB_STARTING,
    ITM_POWER_CEB_ON,
    ITM_POWER_CB_STARTING,
    ITM_POWER_OPERATIONAL,
    ITM_POWER_STOPPING,
    ITM_POWER_FAULT
} itm_power_state_t;

typedef enum {
    ITM_POWER_REQUEST_OFF = 0,
    ITM_POWER_REQUEST_CEB_ONLY,
    ITM_POWER_REQUEST_SYSTEM_ON
} itm_power_request_t;

typedef struct {
    uint32_t ceb_start_timeout_ms;
    uint32_t cb_start_timeout_ms;
    uint32_t shutdown_settle_ms;
} itm_power_config_t;

typedef struct {
    bool ceb_power_good;
    bool cb_feedback_valid;
    bool cb_is_on;
    bool commands_allowed;
    bool blocking_fault;
} itm_power_inputs_t;

typedef struct {
    itm_power_config_t config;
    itm_digital_output_port_t ceb_output;
    itm_digital_output_port_t cb_output;
    itm_power_state_t state;
    itm_power_request_t request;
    uint32_t state_entered_ms;
    bool ceb_commanded;
    bool cb_commanded;
} itm_power_control_t;

bool itm_power_control_init(itm_power_control_t *control,
                            const itm_power_config_t *config,
                            itm_digital_output_port_t ceb_output,
                            itm_digital_output_port_t cb_output,
                            uint32_t now_ms);
void itm_power_control_request(itm_power_control_t *control,
                               itm_power_request_t request);
void itm_power_control_tick(itm_power_control_t *control, uint32_t now_ms,
                            const itm_power_inputs_t *inputs,
                            itm_diagnostics_t *diagnostics);
bool itm_power_control_clear_fault(itm_power_control_t *control,
                                   uint32_t now_ms,
                                   const itm_power_inputs_t *inputs);

#endif
