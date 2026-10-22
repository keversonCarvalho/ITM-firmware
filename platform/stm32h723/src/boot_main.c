#include "itm_update_platform.h"
#include "itm_board.h"
#include "itm/protocols/xcp_update.h"

void Error_Handler(void)
{
    itm_board_force_safe_outputs();
    for (;;) {} /* Watchdog resets if already started. */
}

int main(void)
{
    itm_fw_update_t fw;
    itm_xcp_update_t xcp;
    itm_can_port_t can;
    itm_can_frame_t request, reply, pending = {0};
    uint8_t slot;
    bool attempted = false;
    HAL_Init(); itm_board_force_safe_outputs(); itm_system_clock_config(); itm_board_init();
    itm_update_watchdog_start();
    if (!itm_update_can_init()) for (;;) {}
    can = itm_update_can_port();
    /* Failure leaves recovery answering ACCESS_DENIED; never guesses a slot. */
    if (itm_fw_init(&fw, itm_update_flash_port()))
        (void)itm_fw_migrate_metadata(&fw);
    itm_xcp_init(&xcp, &fw);
    for (;;) {
        uint32_t now = HAL_GetTick();
        itm_update_watchdog_feed();
        if (pending.length) {
            if (can.send(can.context, &pending) == ITM_OK) pending.length = 0;
        } else if (can.receive(can.context, &request) == ITM_OK) {
            if (itm_xcp_receive(&xcp, &request, now, &reply) && reply.length) pending = reply;
        }
        itm_xcp_tick(&xcp, now);
        if (xcp.reset_requested && !pending.length && itm_update_can_tx_idle()) NVIC_SystemReset();
        /* Five-second discovery window on every reset, including corrupt app.
         * A timed-out session returns to the confirmed image. */
        if (!attempted && now > 5000 && !xcp.connected && !pending.length) {
            attempted = true;
            if (itm_fw_select_boot(&fw, &slot))
                itm_update_jump(itm_fw_slot_address(slot) + ITM_FW_HEADER_SIZE);
        }
    }
}
