#ifndef ITM_PORTS_CLOCK_H
#define ITM_PORTS_CLOCK_H

#include <stdint.h>

typedef struct {
    void *context;
    uint32_t (*now_ms)(void *context);
} itm_clock_port_t;

#endif
