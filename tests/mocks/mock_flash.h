#ifndef ITM_TESTS_MOCK_FLASH_H
#define ITM_TESTS_MOCK_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "itm/ports/flash.h"

#define MOCK_FLASH_SLOT_SIZE 128U

typedef struct {
    uint8_t slots[2][MOCK_FLASH_SLOT_SIZE];
    unsigned write_calls;
    unsigned fail_on_write_call;
} mock_flash_t;

void mock_flash_init(mock_flash_t *mock);
itm_flash_port_t mock_flash_port(mock_flash_t *mock);

#endif
