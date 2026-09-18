#ifndef ITM_DK_PORTS_H
#define ITM_DK_PORTS_H

#include "itm/ports/clock.h"
#include "itm/ports/critical_section.h"
#include "itm/ports/serial.h"

itm_clock_port_t itm_dk_clock_port(void);
itm_critical_section_port_t itm_dk_critical_section_port(void);
itm_serial_port_t itm_dk_vcp_port(void);

#endif
