#include "itm/app/cb_voltage.h"

bool itm_cb_voltage_from_adc(uint16_t raw, uint16_t vref_mv,
                             uint16_t *monitor_mv)
{
    const uint32_t delta = raw >= 32768U ? (uint32_t)raw - 32768U : 0U;
    uint64_t input_uv;
    uint64_t voltage_uv;
    if (!monitor_mv || vref_mv == 0U || raw < 32768U) return false;
    /* 16-bit differential ADC: zero at midscale, span +/-VREF. */
    input_uv = ((uint64_t)delta * 2U * vref_mv * 1000U + 32767U) / 65535U;
    if (input_uv > 2000000U) return false;
    voltage_uv = (input_uv * 955U + 102U) / 205U;
    if (voltage_uv > 10000000U) return false;
    *monitor_mv = (uint16_t)((voltage_uv + 500U) / 1000U);
    return true;
}
