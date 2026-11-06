#include <uxr/client/util/time.h>
#include "stm32h7xx_hal.h"

int64_t uxr_millis(void) { return HAL_GetTick(); }
int64_t uxr_nanos(void) { return (int64_t)HAL_GetTick() * 1000000LL; }
