#ifndef ITM_PROTOCOLS_N7SPACE_H
#define ITM_PROTOCOLS_N7SPACE_H
#include "itm/ports/can.h"
#include "itm/services/canopen_ret.h"
/* One RET instance; all upstream types and fixed pool remain in the adapter.
 * Calls must be serialized from the main loop, never directly from an ISR. */
bool itm_n7space_init(itm_can_port_t can, itm_canopen_ret_t *service);
void itm_n7space_fini(void);
itm_result_t itm_n7space_receive(const itm_can_frame_t *frame);
itm_result_t itm_n7space_tick(uint32_t now_ms);
/* SDO 0x2103:0 accepts the literal UPDT (little endian) in maintenance builds.
 * The caller must perform safe shutdown and defer reset until TX completes. */
bool itm_n7space_take_update_request(void);
void itm_n7space_allow_update(bool allowed);
size_t itm_n7space_pool_used(void);
void itm_n7space_set_boot_info(uint8_t slot, uint32_t version, bool confirmed);
#endif
