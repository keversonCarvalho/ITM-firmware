#ifndef ITM_PORTS_FLASH_H
#define ITM_PORTS_FLASH_H

#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"

typedef struct {
    void *context;
    size_t slot_size;
    itm_result_t (*read)(void *context, uint8_t slot, size_t offset,
                         void *data, size_t size);
    itm_result_t (*erase)(void *context, uint8_t slot);
    itm_result_t (*write)(void *context, uint8_t slot, size_t offset,
                          const void *data, size_t size);
} itm_flash_port_t;

#endif
