#include "test_framework.h"
#include "mocks/mock_flash.h"
#include "itm/services/persistence.h"

#include <string.h>

typedef struct {
    uint32_t node_id;
    uint32_t publish_period_ms;
} test_parameters_t;

bool test_persistence_uses_latest_valid_slot(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    test_parameters_t output = {0U, 0U};
    const test_parameters_t first = {7U, 10U};
    const test_parameters_t second = {9U, 20U};
    size_t loaded = 0U;
    uint32_t generation = 0U;

    mock_flash_init(&flash);
    TEST_ASSERT(itm_persistence_init(&persistence, mock_flash_port(&flash), 1U));
    TEST_ASSERT_EQ(ITM_OK, itm_persistence_store(&persistence, &first,
                                                sizeof(first), NULL));
    TEST_ASSERT_EQ(ITM_OK, itm_persistence_store(&persistence, &second,
                                                sizeof(second), NULL));
    TEST_ASSERT_EQ(ITM_OK, itm_persistence_load(&persistence, &output,
                                               sizeof(output), &loaded,
                                               &generation));
    TEST_ASSERT_EQ(sizeof(output), loaded);
    TEST_ASSERT_EQ(2U, generation);
    TEST_ASSERT(memcmp(&output, &second, sizeof(output)) == 0);
    return true;
}

bool test_persistence_survives_interrupted_write(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    test_parameters_t output = {0U, 0U};
    const test_parameters_t valid = {7U, 10U};
    const test_parameters_t interrupted = {9U, 20U};
    size_t loaded = 0U;

    mock_flash_init(&flash);
    TEST_ASSERT(itm_persistence_init(&persistence, mock_flash_port(&flash), 1U));
    TEST_ASSERT_EQ(ITM_OK, itm_persistence_store(&persistence, &valid,
                                                sizeof(valid), NULL));
    flash.fail_on_write_call = flash.write_calls + 2U;
    TEST_ASSERT_EQ(ITM_ERROR_IO,
                   itm_persistence_store(&persistence, &interrupted,
                                         sizeof(interrupted), NULL));
    TEST_ASSERT_EQ(ITM_OK, itm_persistence_load(&persistence, &output,
                                               sizeof(output), &loaded, NULL));
    TEST_ASSERT(memcmp(&output, &valid, sizeof(output)) == 0);
    return true;
}
