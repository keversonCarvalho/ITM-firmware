#include "mocks/mock_flash.h"

#include <string.h>

static itm_result_t read_data(void *context, uint8_t slot, size_t offset,
                              void *data, size_t size)
{
    mock_flash_t *mock = context;
    if ((slot >= 2U) || ((offset + size) > MOCK_FLASH_SLOT_SIZE)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    (void)memcpy(data, &mock->slots[slot][offset], size);
    return ITM_OK;
}

static itm_result_t erase_slot(void *context, uint8_t slot)
{
    mock_flash_t *mock = context;
    if (slot >= 2U) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(mock->slots[slot], 0xFF, MOCK_FLASH_SLOT_SIZE);
    return ITM_OK;
}

static itm_result_t write_data(void *context, uint8_t slot, size_t offset,
                               const void *data, size_t size)
{
    mock_flash_t *mock = context;
    if ((slot >= 2U) || ((offset + size) > MOCK_FLASH_SLOT_SIZE)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    mock->write_calls++;
    if ((mock->fail_on_write_call != 0U) &&
        (mock->write_calls == mock->fail_on_write_call)) {
        return ITM_ERROR_IO;
    }
    (void)memcpy(&mock->slots[slot][offset], data, size);
    return ITM_OK;
}

void mock_flash_init(mock_flash_t *mock)
{
    (void)memset(mock, 0, sizeof(*mock));
    (void)memset(mock->slots, 0xFF, sizeof(mock->slots));
}

itm_flash_port_t mock_flash_port(mock_flash_t *mock)
{
    return (itm_flash_port_t){mock, MOCK_FLASH_SLOT_SIZE,
                              read_data, erase_slot, write_data};
}
