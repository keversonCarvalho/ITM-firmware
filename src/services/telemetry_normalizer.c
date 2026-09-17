#include "itm/services/telemetry_normalizer.h"

#include <limits.h>
#include <stddef.h>

itm_result_t itm_telemetry_normalize(int32_t raw_value,
                                     itm_normalization_rule_t rule,
                                     int64_t *normalized_value)
{
    int64_t scaled;

    if ((normalized_value == NULL) || (rule.denominator == 0U)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }

    scaled = (int64_t)raw_value * (int64_t)rule.numerator;
    scaled /= (int64_t)rule.denominator;
    if (((rule.offset > 0) && (scaled > (INT64_MAX - rule.offset))) ||
        ((rule.offset < 0) && (scaled < (INT64_MIN - rule.offset)))) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    *normalized_value = scaled + rule.offset;
    return ITM_OK;
}
