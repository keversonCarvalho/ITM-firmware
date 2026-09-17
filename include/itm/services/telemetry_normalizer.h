#ifndef ITM_SERVICES_TELEMETRY_NORMALIZER_H
#define ITM_SERVICES_TELEMETRY_NORMALIZER_H

#include <stdint.h>
#include "itm/core/result.h"

typedef struct {
    int32_t numerator;
    uint32_t denominator;
    int64_t offset;
} itm_normalization_rule_t;

itm_result_t itm_telemetry_normalize(int32_t raw_value,
                                     itm_normalization_rule_t rule,
                                     int64_t *normalized_value);

#endif
