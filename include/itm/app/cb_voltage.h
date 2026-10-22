#ifndef ITM_APP_CB_VOLTAGE_H
#define ITM_APP_CB_VOLTAGE_H
#include <stdbool.h>
#include <stdint.h>

/* Nominal Rtop=75k, Rbottom=20.5k, AMC3311-Q1 differential gain=1.
 * The specified linear AMC input stops at 2.0 V; raw values above it are
 * rejected rather than extrapolated into the requested 0-10 V monitor span. */
bool itm_cb_voltage_from_adc(uint16_t raw, uint16_t vref_mv,
                             uint16_t *monitor_mv);
#endif
