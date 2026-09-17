#ifndef ITM_PORTS_CAN_H
#define ITM_PORTS_CAN_H

#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"

typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t length;
} itm_can_frame_t;

typedef struct {
    void *context;
    itm_result_t (*send)(void *context, const itm_can_frame_t *frame);
    itm_result_t (*receive)(void *context, itm_can_frame_t *frame);
} itm_can_port_t;

#endif
