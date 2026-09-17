#ifndef ITM_TESTS_MOCK_CRITICAL_H
#define ITM_TESTS_MOCK_CRITICAL_H

#include <stdint.h>
#include "itm/ports/critical_section.h"

typedef struct {
    uint32_t enter_count;
    uint32_t exit_count;
    uint32_t active_count;
    uint32_t maximum_active_count;
} mock_critical_t;

void mock_critical_init(mock_critical_t *mock);
itm_critical_section_port_t mock_critical_port(mock_critical_t *mock);

#endif
