#ifndef ITM_SERVICES_DIAGNOSTICS_H
#define ITM_SERVICES_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITM_DIAGNOSTIC_CAPACITY 16U

typedef enum {
    ITM_DIAG_INFO = 0,
    ITM_DIAG_WARNING,
    ITM_DIAG_FAULT
} itm_diag_severity_t;

typedef enum {
    ITM_DIAG_NONE = 0,
    ITM_DIAG_BC_LINK_LOST,
    ITM_DIAG_BC_LINK_RESTORED,
    ITM_DIAG_CB_FEEDBACK_INVALID,
    ITM_DIAG_CB_STATE_MISMATCH,
    ITM_DIAG_OUTPUT_WRITE_FAILED,
    ITM_DIAG_POWER_TRANSITION_TIMEOUT,
    ITM_DIAG_PERSISTENCE_INVALID
} itm_diag_code_t;

typedef struct {
    uint32_t timestamp_ms;
    itm_diag_code_t code;
    itm_diag_severity_t severity;
    uint32_t detail;
} itm_diag_event_t;

typedef struct {
    itm_diag_event_t events[ITM_DIAGNOSTIC_CAPACITY];
    size_t count;
    size_t next;
    uint32_t total_events;
    bool blocking_fault;
} itm_diagnostics_t;

void itm_diagnostics_init(itm_diagnostics_t *diagnostics);
void itm_diagnostics_report(itm_diagnostics_t *diagnostics,
                            itm_diag_event_t event);
bool itm_diagnostics_get(const itm_diagnostics_t *diagnostics,
                         size_t age, itm_diag_event_t *event);
void itm_diagnostics_clear_blocking_fault(itm_diagnostics_t *diagnostics);

#endif
