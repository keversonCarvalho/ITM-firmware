#ifndef ITM_PROTOCOLS_INTERFACES_H
#define ITM_PROTOCOLS_INTERFACES_H

#include <stdbool.h>
#include <stdint.h>
#include "itm/core/result.h"

typedef enum {
    ITM_COMMAND_NONE = 0,
    ITM_COMMAND_ALL_OFF,
    ITM_COMMAND_CEB_ON,
    ITM_COMMAND_SYSTEM_ON,
    ITM_COMMAND_CLEAR_FAULT
} itm_command_t;

typedef struct {
    void *context;
    itm_result_t (*poll)(void *context, itm_command_t *command,
                         bool *valid_frame_received);
} itm_bc_protocol_t;

typedef struct {
    void *context;
    itm_result_t (*poll)(void *context);
} itm_ceb_protocol_t;

typedef struct {
    void *context;
    itm_result_t (*poll)(void *context);
} itm_bms_protocol_t;

typedef struct {
    void *context;
    itm_result_t (*process)(void *context);
    itm_result_t (*publish_telemetry)(void *context);
} itm_canopen_protocol_t;

#endif
