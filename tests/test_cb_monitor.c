#include "test_framework.h"
#include "itm/app/cb_monitor.h"

static itm_cb_monitor_config_t test_config(void)
{
    return (itm_cb_monitor_config_t){5600U, 6700U, 3U,
                                     false, true, true, false};
}

bool test_cb_monitor_debounces_and_matches_expected_state(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, true));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, true));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, true));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MISMATCH,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, false));
    return true;
}

bool test_cb_monitor_reports_indeterminate_voltage(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_INVALID,
                   itm_cb_monitor_sample(&monitor, 6000U, 1000U, false));
    TEST_ASSERT(!monitor.has_stable_sample);
    return true;
}
