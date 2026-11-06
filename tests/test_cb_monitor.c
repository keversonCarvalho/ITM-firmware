#include "test_framework.h"
#include "itm/app/cb_monitor.h"
#include "itm/app/cb_voltage.h"
#include <stddef.h>

static itm_cb_monitor_config_t test_config(void)
{
    return (itm_cb_monitor_config_t){7304U, 7305U, 40U};
}

bool test_cb_monitor_debounces_and_matches_expected_state(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 9130U, 9130U, true, 100U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 9130U, 9130U, true, 139U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
                   itm_cb_monitor_sample(&monitor, 9130U, 9130U, true, 140U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 9130U, 9130U, false, 141U));
    TEST_ASSERT_EQ(ITM_CB_CONDITION_PENDING, monitor.condition);
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MISMATCH,
                   itm_cb_monitor_sample(&monitor, 9130U, 9130U, false, 181U));
    TEST_ASSERT_EQ(ITM_CB_CONDITION_STUCK_ON, monitor.condition);
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
                   itm_cb_monitor_sample(&monitor, 9130U, 0U, false, 182U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
                   itm_cb_monitor_sample(&monitor, 9130U, 0U, false, 222U));
    return true;
}

bool test_cb_monitor_reports_indeterminate_voltage(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = {300U, 700U, 40U};

    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_INVALID,
                   itm_cb_monitor_sample(&monitor, 500U, 1000U, false, 0U));
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
        {7304U, 0U, false, ITM_CB_CONDITION_BATTERY_DISCONNECTED},
        {1000U, 0U, true, ITM_CB_CONDITION_BATTERY_DISCONNECTED},
        {0U, 7305U, false, ITM_CB_CONDITION_MONITOR_FAULT},
        {0U, 7305U, true, ITM_CB_CONDITION_MONITOR_FAULT},
        {7305U, 0U, false, ITM_CB_CONDITION_NORMAL},
        {7305U, 0U, true, ITM_CB_CONDITION_STUCK_OFF},
        {9130U, 9130U, false, ITM_CB_CONDITION_STUCK_ON},
        {9130U, 9130U, true, ITM_CB_CONDITION_NORMAL},
    };
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        itm_cb_monitor_t monitor;
        const itm_cb_monitor_config_t config = test_config();
        TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
        TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
            itm_cb_monitor_sample(&monitor, cases[i].em1_mv,
                cases[i].em2_mv, cases[i].commanded_on, 0U));
        (void)itm_cb_monitor_sample(&monitor, cases[i].em1_mv,
            cases[i].em2_mv, cases[i].commanded_on, 40U);
        TEST_ASSERT_EQ(cases[i].expected, monitor.condition);
    }
    return true;
}

bool test_cb_monitor_invalid_sample_resets_debounce(void)
{
    itm_cb_monitor_t monitor;
    const itm_cb_monitor_config_t config = test_config();
    TEST_ASSERT(itm_cb_monitor_init(&monitor, &config));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
        itm_cb_monitor_sample(&monitor, 7305U, 0U, false, 0U));
    itm_cb_monitor_invalidate(&monitor);
    TEST_ASSERT_EQ(ITM_CB_CONDITION_INVALID, monitor.condition);
    TEST_ASSERT_EQ(ITM_CB_MONITOR_PENDING,
        itm_cb_monitor_sample(&monitor, 7305U, 0U, false, 100U));
    TEST_ASSERT_EQ(ITM_CB_MONITOR_MATCH,
        itm_cb_monitor_sample(&monitor, 7305U, 0U, false, 140U));
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
    TEST_ASSERT(itm_cb_voltage_from_adc(32762U, 3300U, &mv));
    TEST_ASSERT_EQ(0U, mv);
    TEST_ASSERT(!itm_cb_voltage_from_adc(32127U, 3300U, &mv));
    return true;
}
