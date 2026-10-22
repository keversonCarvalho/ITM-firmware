#include "itm/protocols/xcp_update.h"
#include <string.h>
static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void error(itm_can_frame_t *r, uint8_t code)
{
    r->data[0] = 0xfe; r->data[1] = code; r->length = 2;
}
void itm_xcp_init(itm_xcp_update_t *x, itm_fw_update_t *fw)
{
    memset(x, 0, sizeof(*x)); x->fw = fw;
}
void itm_xcp_tick(itm_xcp_update_t *x, uint32_t now)
{
    if (x && x->connected && (uint32_t)(now - x->last_ms) > ITM_XCP_TIMEOUT_MS) {
        itm_fw_abort(x->fw); itm_xcp_init(x, x->fw);
    }
}
bool itm_xcp_receive(itm_xcp_update_t *x, const itm_can_frame_t *q,
                     uint32_t now, itm_can_frame_t *r)
{
    uint32_t base;
    if (!x || !x->fw || !q || !r || q->id != ITM_XCP_RX_ID) return false;
    memset(r, 0, sizeof(*r)); r->id = ITM_XCP_TX_ID;
    if (!q->length || q->length > 8) { error(r, 0x21); return true; }
    itm_xcp_tick(x, now);
    x->last_ms = now;
    if (!x->fw->initialized) { error(r, 0x24); return true; }
    base = itm_fw_slot_address(x->fw->confirmed == 0 ? 1 : 0);
    r->data[0] = 0xff; r->length = 1;
    if (q->data[0] == 0xff) { /* CONNECT */
        if (q->length < 2 || q->data[1] != 0) { error(r, 0x22); return true; }
        itm_fw_abort(x->fw); itm_xcp_init(x, x->fw);
        x->connected = true; x->last_ms = now; x->mta = base;
        /* PGM only, byte addresses, little endian, no block transfer. */
        memcpy(r->data, (uint8_t[]){0xff, 0x10, 0, 8, 8, 0, 1, 1}, 8);
        r->length = 8; return true;
    }
    if (!x->connected) { r->length = 0; return true; }
    switch (q->data[0]) {
    case 0xfe: /* DISCONNECT */
        itm_fw_abort(x->fw); itm_xcp_init(x, x->fw); break;
    case 0xfd: /* GET_STATUS */
        r->length = 6; break;
    case 0xfc: /* SYNCH */
        error(r, 0); break;
    case 0xf1: /* USER_CMD 0: ITM update status, no memory access. */
        if (q->length < 2 || q->data[1] != 0) { error(r, 0x22); break; }
        r->data[1] = x->fw->confirmed == 0 ? 1 : 0;
        r->data[2] = x->fw->confirmed;
        r->data[3] = x->fw->candidate;
        r->data[4] = x->fw->tried;
        r->length = 5;
        break;
    case 0xf6: /* SET_MTA: arbitrary writes are deliberately unavailable. */
        if (q->length < 8 || q->data[3]) { error(r, 0x22); break; }
        if (read32(q->data + 4) != base + (x->fw->receiving ? x->fw->received : 0)) {
            error(r, 0x22); break;
        }
        x->mta = read32(q->data + 4); break;
    case 0xd2: /* PROGRAM_START */
        if (x->programming || x->complete) { error(r, 0x29); break; }
        x->programming = true;
        memcpy(r->data, (uint8_t[]){0xff, 0, 0, 8, 0, 0, 0}, 7);
        r->length = 7; break;
    case 0xd1: /* PROGRAM_CLEAR absolute address */
        if (!x->programming || x->complete || x->mta != base) { error(r, 0x29); break; }
        if (q->length < 8 || q->data[1]) { error(r, 0x22); break; }
        if (!itm_fw_begin(x->fw, read32(q->data + 4))) error(r, 0x31);
        break;
    case 0xd0: /* PROGRAM, standard mode, at most 6 bytes per request */
        if (!x->programming || x->complete || !x->fw->receiving) { error(r, 0x29); break; }
        if (q->length < 2 || q->data[1] > 6 || q->length < 2 + q->data[1]) {
            error(r, 0x21); break;
        }
        if (!q->data[1]) {
            x->complete = itm_fw_finish(x->fw);
            if (!x->complete) error(r, 0x32);
        } else if (!itm_fw_write(x->fw, q->data + 2, q->data[1])) {
            error(r, 0x31);
        } else x->mta += q->data[1];
        break;
    case 0xcf: /* PROGRAM_RESET: no positive response before reset. */
        if (!x->complete) { error(r, 0x29); break; }
        x->reset_requested = true; r->length = 0; break;
    default:
        error(r, 0x20); break;
    }
    return true;
}
