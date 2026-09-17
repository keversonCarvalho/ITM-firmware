#ifndef ITM_PORTS_ADC_H
#define ITM_PORTS_ADC_H

#include <stdint.h>
#include "itm/core/result.h"

typedef struct {
    void *context;
    itm_result_t (*read_raw)(void *context, uint8_t channel, int32_t *value);
} itm_adc_port_t;

#endif
