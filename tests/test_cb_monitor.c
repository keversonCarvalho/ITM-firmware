#include "test_framework.h"
#include "itm/app/cb_monitor.h"
#include "itm/app/cb_voltage.h"
#include <stddef.h>

static itm_cb_monitor_config_t test_config(void)
{
    return (itm_cb_monitor_config_t){5600U, 6700U, 50U};
}

bool test_cb_monitor_debounces_and_matches_expected_state(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 7000U, 7000U, true, 100U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 7000U, 7000U, true, 149U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
                   itm_cb_monitor_sample(&monitor, 7000U, 7000U, true, 150U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MISMATCH,
                   itm_cb_monitor_sample(&monitor, 7000U, 7000U, false, 151U));
    TEST_ASSERT_EQ(ITM_CB_CONDITION_STUCK_ON, monitor.condition);
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, false, 152U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
                   itm_cb_monitor_sample(&monitor, 7000U, 1000U, false, 202U));
    return true;
}

bool test_cb_monitor_reports_indeterminate_voltage(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_INVALID,
                   itm_cb_monitor_sample(&monitor, 6000U, 1000U, false, 0U));
    TEST_ASSERT(!monitor.has_stable_sample);
    TEST_ASSERT_EQ(ITM_CB_CONDITION_INVALID, monitor.condition);
    return true;
}

bool test_cb_monitor_truth_table(void)
{
    static const struct {
        uint16_t em1_mv, em2_mv;
        bool commanded_on;
        itm_cb_condition_t expected;
    } cases[] = {
        {1000U, 1000U, false, ITM_CB_CONDITION_BATTERY_DISCONNECTED},
        {1000U, 1000U, true, ITM_CB_CONDITION_BATTERY_DISCONNECTED},
        {1000U, 7000U, false, ITM_CB_CONDITION_MONITOR_FAULT},
        {1000U, 7000U, true, ITM_CB_CONDITION_MONITOR_FAULT},
        {7000U, 1000U, false, ITM_CB_CONDITION_NORMAL},
        {7000U, 1000U, true, ITM_CB_CONDITION_STUCK_OFF},
        {7000U, 7000U, false, ITM_CB_CONDITION_STUCK_ON},
        {7000U, 7000U, true, ITM_CB_CONDITION_NORMAL},
    };
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        itm_cb_monitor_t monitor;
        const itm_cb_monitor_config_t config = test_config();
        TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
        TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
            itm_cb_monitor_sample(&monitor, cases[i].em1_mv,
                cases[i].em2_mv, cases[i].commanded_on, 0U));
        (void)itm_cb_monitor_sample(&monitor, cases[i].em1_mv,
            cases[i].em2_mv, cases[i].commanded_on, 50U);
        TEST_ASSERT_EQ(cases[i].expected, monitor.condition);
    }
    return true;
}

bool test_cb_voltage_nominal_scale_and_linear_limit(void)
{
    uint16_t mv = 0U;
    TEST_ASSERT(itm_cb_voltage_from_adc(44704U, 3300U, &mv));
    TEST_ASSERT(mv >= 5599U && mv <= 5601U);
    TEST_ASSERT(itm_cb_voltage_from_adc(47049U, 3300U, &mv));
    TEST_ASSERT(mv >= 6699U && mv <= 6701U);
    TEST_ASSERT(!itm_cb_voltage_from_adc(54083U, 3300U, &mv));
    TEST_ASSERT(!itm_cb_voltage_from_adc(1000U, 3300U, &mv));
    return true;
}
