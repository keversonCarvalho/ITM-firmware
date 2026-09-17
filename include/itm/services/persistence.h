#ifndef ITM_SERVICES_PERSISTENCE_H
#define ITM_SERVICES_PERSISTENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/core/result.h"
#include "itm/ports/flash.h"

typedef struct {
    itm_flash_port_t flash;
    uint32_t schema_version;
} itm_persistence_t;

bool itm_persistence_init(itm_persistence_t *persistence,
                          itm_flash_port_t flash,
                          uint32_t schema_version);
itm_result_t itm_persistence_load(const itm_persistence_t *persistence,
                                  void *data, size_t capacity,
                                  size_t *loaded_size,
                                  uint32_t *generation);
itm_result_t itm_persistence_store(const itm_persistence_t *persistence,
                                   const void *data, size_t size,
                                   uint32_t *new_generation);

#endif
