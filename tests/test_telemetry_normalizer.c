#include "test_framework.h"
#include "itm/services/telemetry_normalizer.h"

bool test_telemetry_normalizer_uses_integer_affine_rule(void)
{
    int64_t value = 0;
    const itm_normalization_rule_t voltage_rule = {10, 1U, 0};
    const itm_normalization_rule_t temperature_rule = {25, 10U, -5000};

    TEST_ASSERT_EQ(ITM_OK,
                   itm_telemetry_normalize(2800, voltage_rule, &value));
    TEST_ASSERT_EQ(28000, value);
    TEST_ASSERT_EQ(ITM_OK,
                   itm_telemetry_normalize(4000, temperature_rule, &value));
    TEST_ASSERT_EQ(5000, value);
    TEST_ASSERT_EQ(ITM_ERROR_INVALID_ARGUMENT,
                   itm_telemetry_normalize(
                       1, (itm_normalization_rule_t){1, 0U, 0}, &value));
    return true;
}
