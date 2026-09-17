#include "test_framework.h"
#include "itm/services/diagnostics.h"

bool test_diagnostics_keeps_recent_events(void)
{
    itm_diagnostics_t diagnostics;
    itm_diag_event_t event;
    uint32_t index;

    itm_diagnostics_init(&diagnostics);
    for (index = 0U; index < ITM_DIAGNOSTIC_CAPACITY + 2U; ++index) {
        itm_diagnostics_report(&diagnostics, (itm_diag_event_t){
            index, ITM_DIAG_PERSISTENCE_INVALID, ITM_DIAG_WARNING, index});
    }
    TEST_ASSERT_EQ(ITM_DIAGNOSTIC_CAPACITY, diagnostics.count);
    TEST_ASSERT_EQ(ITM_DIAGNOSTIC_CAPACITY + 2U,
                   diagnostics.total_events);
    TEST_ASSERT(itm_diagnostics_get(&diagnostics, 0U, &event));
    TEST_ASSERT_EQ(ITM_DIAGNOSTIC_CAPACITY + 1U, event.detail);
    TEST_ASSERT(itm_diagnostics_get(&diagnostics,
                                    ITM_DIAGNOSTIC_CAPACITY - 1U, &event));
    TEST_ASSERT_EQ(2U, event.detail);
    return true;
}
