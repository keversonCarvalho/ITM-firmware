#ifndef ITM_PORTS_DIGITAL_OUTPUT_H
#define ITM_PORTS_DIGITAL_OUTPUT_H

#include <stdbool.h>
#include "itm/core/result.h"

typedef struct {
    void *context;
    itm_result_t (*write)(void *context, bool active);
} itm_digital_output_port_t;

#endif
