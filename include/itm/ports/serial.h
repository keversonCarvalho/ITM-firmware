#ifndef ITM_PORTS_SERIAL_H
#define ITM_PORTS_SERIAL_H

#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"

typedef struct {
    void *context;
    itm_result_t (*write)(void *context, const uint8_t *data, size_t size);
    itm_result_t (*read)(void *context, uint8_t *data, size_t capacity,
                         size_t *received);
} itm_serial_port_t;

#endif
