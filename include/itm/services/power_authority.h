#ifndef ITM_SERVICES_POWER_AUTHORITY_H
#define ITM_SERVICES_POWER_AUTHORITY_H

#include "itm/app/power_control.h"
#include "itm/protocols/itm_dds_power.h"

#define ITM_POWER_COMMAND_LEASE_MS 1000U

typedef enum {
    ITM_POWER_RESULT_NONE = 0, ITM_POWER_RESULT_ACCEPTED = 1,
    ITM_POWER_RESULT_REACHED = 2, ITM_POWER_RESULT_MALFORMED = 3,
    ITM_POWER_RESULT_WRONG_SESSION = 4, ITM_POWER_RESULT_OLD_COMMAND = 5,
    ITM_POWER_RESULT_BLOCKING_FAULT = 6,
    ITM_POWER_RESULT_OUTPUT_FAILURE = 7, ITM_POWER_RESULT_EXPIRED = 8,
    ITM_POWER_RESULT_LINK_LOSS = 9
} itm_power_result_t;

typedef enum {
    ITM_POWER_AUTHORITY_NONE = 0, ITM_POWER_AUTHORITY_BENCH,
    ITM_POWER_AUTHORITY_BC
} itm_power_authority_kind_t;

typedef enum {
    ITM_BENCH_ACCEPTED = 0, ITM_BENCH_DISABLED, ITM_BENCH_BC_OWNS,
    ITM_BENCH_FAULT
} itm_bench_accept_t;

typedef struct {
    uint64_t session_id;
    uint32_t command_id;
    uint32_t last_command_ms;
    itm_power_request_t request;
    itm_power_authority_kind_t authority;
    itm_power_result_t result;
    bool bc_connected;
    bool bench_enabled;
    bool have_bc_command;
    bool last_ceb_on;
    bool last_cb_on;
    bool link_loss_pending;
    bool status_dirty;
} itm_power_authority_t;

void itm_power_authority_init(itm_power_authority_t *, bool);
void itm_power_authority_session_started(itm_power_authority_t *, uint64_t,
                                         uint32_t);
void itm_power_authority_session_lost(itm_power_authority_t *, uint32_t);
void itm_power_authority_malformed(itm_power_authority_t *);
void itm_power_authority_command(itm_power_authority_t *,
                                 const itm_dds_power_command_t *, uint32_t,
                                 bool);
itm_bench_accept_t itm_power_authority_bench(itm_power_authority_t *,
                                              itm_power_request_t, uint32_t,
                                              bool);
void itm_power_authority_tick(itm_power_authority_t *, uint32_t);
void itm_power_authority_observe(itm_power_authority_t *,
                                 const itm_power_control_t *);
void itm_power_authority_status(const itm_power_authority_t *,
                                const itm_power_control_t *, uint8_t,
                                uint32_t, uint32_t,
                                itm_dds_power_status_t *);
#endif
