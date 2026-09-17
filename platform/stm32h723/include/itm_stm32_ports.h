#ifndef ITM_STM32H723_PORTS_H
#define ITM_STM32H723_PORTS_H

#include "itm/ports/adc.h"
#include "itm/ports/can.h"
#include "itm/ports/clock.h"
#include "itm/ports/critical_section.h"
#include "itm/ports/digital_input.h"
#include "itm/ports/digital_output.h"
#include "itm/ports/flash.h"
#include "itm/ports/serial.h"

itm_clock_port_t itm_stm32_clock_port(void);
itm_critical_section_port_t itm_stm32_critical_section_port(void);
itm_digital_output_port_t itm_stm32_mfet1_port(void);
itm_digital_output_port_t itm_stm32_mfet2_port(void);
/* These ports expose raw electrical levels until FAULT polarity is approved. */
itm_digital_input_port_t itm_stm32_fault1_level_port(void);
itm_digital_input_port_t itm_stm32_fault2_level_port(void);

itm_adc_port_t itm_stm32_adc_stub(void);
itm_can_port_t itm_stm32_fdcan_stub(void);
itm_serial_port_t itm_stm32_serial_stub(void);
itm_flash_port_t itm_stm32_flash_stub(void);

#endif
