#ifndef ITM_UPDATE_PLATFORM_H
#define ITM_UPDATE_PLATFORM_H
#include "itm/ports/can.h"
#include "itm/protocols/itm_dbcc.h"
#include "itm/services/firmware_update.h"
bool itm_update_can_init(void);
itm_can_port_t itm_update_can_port(void);
bool itm_update_can_tx_idle(void);
bool itm_update_battery_can_init(void);
itm_result_t itm_update_battery_can_receive(itm_dbcc_bus_t bus,
                                             itm_dbcc_frame_t *frame);
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
itm_result_t itm_update_battery_can_send(itm_dbcc_bus_t bus, uint32_t id,
                                          const uint8_t *data, uint8_t length);
#endif
itm_fw_port_t itm_update_flash_port(void);
bool itm_update_boot_protected(void);
void itm_update_watchdog_start(void);
void itm_update_watchdog_feed(void);
void itm_update_jump(uint32_t vector) __attribute__((noreturn));
void itm_system_clock_config(void);
void itm_update_app_init(void);
void itm_update_app_poll(void);
bool itm_update_ceb_uart_init(void);
bool itm_update_ceb_uart_read(uint8_t *byte);
uint32_t itm_update_ceb_uart_errors(void);
bool itm_update_bc_uart_init(void);
bool itm_update_bc_uart_send(const uint8_t *frame, size_t length);
bool itm_update_bc_uart_idle(void);
bool itm_update_cb_adc_init(void);
bool itm_update_cb_adc_read(uint16_t *em1_raw, uint16_t *em2_raw);
#endif
