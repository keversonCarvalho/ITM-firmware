#include "itm/services/power_authority.h"
#include <string.h>

static itm_power_request_t pair(bool ceb, bool cb)
{
    if (ceb && cb) return ITM_POWER_REQUEST_SYSTEM_ON;
    if (ceb) return ITM_POWER_REQUEST_CEB_ONLY;
    if (cb) return ITM_POWER_REQUEST_CB_ONLY;
    return ITM_POWER_REQUEST_OFF;
}

void itm_power_authority_init(itm_power_authority_t *s, bool bench)
{
    if (!s) return;
    memset(s, 0, sizeof(*s)); s->bench_enabled = bench; s->status_dirty = true;
}

void itm_power_authority_session_started(itm_power_authority_t *s,
                                         uint64_t id, uint32_t now)
{
    if (!s) return;
    s->session_id = id; s->command_id = 0U; s->last_command_ms = now;
    s->request = ITM_POWER_REQUEST_OFF; s->authority = ITM_POWER_AUTHORITY_BC;
    s->result = s->link_loss_pending ? ITM_POWER_RESULT_LINK_LOSS :
                                       ITM_POWER_RESULT_NONE;
    s->link_loss_pending = false; s->bc_connected = true;
    s->have_bc_command = false; s->status_dirty = true;
}

void itm_power_authority_session_lost(itm_power_authority_t *s, uint32_t now)
{
    if (!s) return;
    s->last_command_ms = now; s->request = ITM_POWER_REQUEST_OFF;
    s->authority = ITM_POWER_AUTHORITY_NONE;
    s->result = ITM_POWER_RESULT_LINK_LOSS; s->bc_connected = false;
    s->link_loss_pending = true;
    s->status_dirty = true;
}

void itm_power_authority_malformed(itm_power_authority_t *s)
{
    if (s) { s->result = ITM_POWER_RESULT_MALFORMED; s->status_dirty = true; }
}

void itm_power_authority_command(itm_power_authority_t *s,
                                 const itm_dds_power_command_t *c,
                                 uint32_t now, bool fault)
{
    if (!s || !c) return;
    s->status_dirty = true;
    if (!s->bc_connected || c->session_id != s->session_id) {
        s->result = ITM_POWER_RESULT_WRONG_SESSION; return;
    }
    if (s->have_bc_command && c->command_id == s->command_id) {
        if (c->ceb_on != s->last_ceb_on || c->cb_on != s->last_cb_on)
            s->result = ITM_POWER_RESULT_OLD_COMMAND;
        return; /* An idempotent retry never renews the lease. */
    }
    if (s->have_bc_command && c->command_id < s->command_id) {
        s->result = ITM_POWER_RESULT_OLD_COMMAND; return;
    }
    s->command_id = c->command_id; s->last_ceb_on = c->ceb_on;
    s->last_cb_on = c->cb_on; s->have_bc_command = true;
    if (fault && (c->ceb_on || c->cb_on)) {
        s->result = ITM_POWER_RESULT_BLOCKING_FAULT; return;
    }
    s->request = pair(c->ceb_on, c->cb_on); s->last_command_ms = now;
    s->authority = ITM_POWER_AUTHORITY_BC;
    s->result = ITM_POWER_RESULT_ACCEPTED;
}

itm_bench_accept_t itm_power_authority_bench(itm_power_authority_t *s,
                                              itm_power_request_t r,
                                              uint32_t now, bool fault)
{
    if (!s || r > ITM_POWER_REQUEST_CB_ONLY) return ITM_BENCH_FAULT;
    if (!s->bench_enabled) return ITM_BENCH_DISABLED;
    if (s->bc_connected) return ITM_BENCH_BC_OWNS;
    if (fault && r != ITM_POWER_REQUEST_OFF) return ITM_BENCH_FAULT;
    s->request = r; s->last_command_ms = now;
    s->authority = r == ITM_POWER_REQUEST_OFF ? ITM_POWER_AUTHORITY_NONE :
                                                ITM_POWER_AUTHORITY_BENCH;
    s->status_dirty = true;
    return ITM_BENCH_ACCEPTED;
}

void itm_power_authority_tick(itm_power_authority_t *s, uint32_t now)
{
    if (!s || s->request == ITM_POWER_REQUEST_OFF ||
        (uint32_t)(now - s->last_command_ms) < ITM_POWER_COMMAND_LEASE_MS) return;
    s->request = ITM_POWER_REQUEST_OFF;
    if (s->authority == ITM_POWER_AUTHORITY_BC)
        s->result = ITM_POWER_RESULT_EXPIRED;
    s->authority = s->bc_connected ? ITM_POWER_AUTHORITY_BC :
                                     ITM_POWER_AUTHORITY_NONE;
    s->status_dirty = true;
}

void itm_power_authority_observe(itm_power_authority_t *s,
                                 const itm_power_control_t *p)
{
    bool reached = false;
    if (!s || !p || s->authority != ITM_POWER_AUTHORITY_BC ||
        s->result != ITM_POWER_RESULT_ACCEPTED) return;
    if (s->request == ITM_POWER_REQUEST_OFF)
        reached = !p->ceb_commanded && !p->cb_commanded && p->state == ITM_POWER_OFF;
    else if (s->request == ITM_POWER_REQUEST_CEB_ONLY)
        reached = p->ceb_commanded && !p->cb_commanded && p->state == ITM_POWER_CEB_ON;
    else if (s->request == ITM_POWER_REQUEST_SYSTEM_ON)
        reached = p->ceb_commanded && p->cb_commanded && p->state == ITM_POWER_OPERATIONAL;
    else if (s->request == ITM_POWER_REQUEST_CB_ONLY)
        reached = !p->ceb_commanded && p->cb_commanded && p->state == ITM_POWER_CB_ON;
    if (p->state == ITM_POWER_FAULT) {
        s->result = ITM_POWER_RESULT_OUTPUT_FAILURE; s->status_dirty = true;
    } else if (reached) {
        s->result = ITM_POWER_RESULT_REACHED; s->status_dirty = true;
    }
}

void itm_power_authority_status(const itm_power_authority_t *s,
                                const itm_power_control_t *p, uint8_t cb,
                                uint32_t em1, uint32_t em2,
                                itm_dds_power_status_t *out)
{
    if (!s || !p || !out) return;
    memset(out, 0, sizeof(*out));
    out->session_id = s->session_id; out->command_id = s->command_id;
    out->result = (uint8_t)s->result; out->power_state = (uint8_t)p->state;
    out->ceb_requested = s->request == ITM_POWER_REQUEST_CEB_ONLY ||
                         s->request == ITM_POWER_REQUEST_SYSTEM_ON;
    out->cb_requested = s->request == ITM_POWER_REQUEST_CB_ONLY ||
                        s->request == ITM_POWER_REQUEST_SYSTEM_ON;
    out->ceb_output_on = p->ceb_commanded; out->cb_output_on = p->cb_commanded;
    out->cb_condition = cb; out->em1_valid = em1 != UINT32_MAX;
    out->em2_valid = em2 != UINT32_MAX;
    out->em1_mv = out->em1_valid ? em1 : UINT32_MAX;
    out->em2_mv = out->em2_valid ? em2 : UINT32_MAX;
}
