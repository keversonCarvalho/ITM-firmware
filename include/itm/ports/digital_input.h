#ifndef ITM_PORTS_DIGITAL_INPUT_H
#define ITM_PORTS_DIGITAL_INPUT_H

#include <stdbool.h>
#include "itm/core/result.h"

typedef struct {
    void *context;
    itm_result_t (*read)(void *context, bool *active);
} itm_digital_input_port_t;

#endif
