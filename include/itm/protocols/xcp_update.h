#ifndef ITM_XCP_UPDATE_H
#define ITM_XCP_UPDATE_H
#include "itm/ports/can.h"
#include "itm/services/firmware_update.h"
/* Bench allocation only. Must be reserved in the RET ICD before flight. */
#define ITM_XCP_RX_ID 0x6F0U
#define ITM_XCP_TX_ID 0x6F1U
#define ITM_XCP_TIMEOUT_MS 10000U
typedef struct {
    itm_fw_update_t *fw;
    uint32_t mta, last_ms;
    bool connected, programming, complete, reset_requested;
} itm_xcp_update_t;
void itm_xcp_init(itm_xcp_update_t *xcp, itm_fw_update_t *fw);
/* Returns true only for an XCP request. response.length == 0 means no reply. */
bool itm_xcp_receive(itm_xcp_update_t *xcp, const itm_can_frame_t *request,
                     uint32_t now_ms, itm_can_frame_t *response);
void itm_xcp_tick(itm_xcp_update_t *xcp, uint32_t now_ms);
#endif
