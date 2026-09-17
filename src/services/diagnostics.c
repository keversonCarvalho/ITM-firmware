#include "itm/services/diagnostics.h"

#include <string.h>

void itm_diagnostics_init(itm_diagnostics_t *diagnostics)
{
    if (diagnostics != NULL) {
        (void)memset(diagnostics, 0, sizeof(*diagnostics));
    }
}

void itm_diagnostics_report(itm_diagnostics_t *diagnostics,
                            itm_diag_event_t event)
{
    if (diagnostics == NULL) {
        return;
    }

    diagnostics->events[diagnostics->next] = event;
    diagnostics->next = (diagnostics->next + 1U) % ITM_DIAGNOSTIC_CAPACITY;
    if (diagnostics->count < ITM_DIAGNOSTIC_CAPACITY) {
        diagnostics->count++;
    }
    diagnostics->total_events++;
    if (event.severity == ITM_DIAG_FAULT) {
        diagnostics->blocking_fault = true;
    }
}

bool itm_diagnostics_get(const itm_diagnostics_t *diagnostics,
                         size_t age, itm_diag_event_t *event)
{
    size_t index;

    if ((diagnostics == NULL) || (event == NULL) ||
        (age >= diagnostics->count)) {
        return false;
    }

    index = (diagnostics->next + ITM_DIAGNOSTIC_CAPACITY - 1U - age) %
            ITM_DIAGNOSTIC_CAPACITY;
    *event = diagnostics->events[index];
    return true;
}

void itm_diagnostics_clear_blocking_fault(itm_diagnostics_t *diagnostics)
{
    if (diagnostics != NULL) {
        diagnostics->blocking_fault = false;
    }
}
