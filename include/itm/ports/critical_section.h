#ifndef ITM_PORTS_CRITICAL_SECTION_H
#define ITM_PORTS_CRITICAL_SECTION_H

#include <stdint.h>

typedef uint32_t itm_critical_state_t;

typedef struct {
    void *context;
    itm_critical_state_t (*enter)(void *context);
    void (*exit)(void *context, itm_critical_state_t previous_state);
} itm_critical_section_port_t;

#endif
