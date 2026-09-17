#include "test_framework.h"
#include "mocks/mock_critical.h"
#include "itm/services/telemetry_ring.h"

static itm_telemetry_event_t event_with_value(int64_t value)
{
    return (itm_telemetry_event_t){ITM_SIGNAL_CB_STATE, ITM_SOURCE_CB,
                                   ITM_SIGNAL_TYPE_BOOL,
                                   ITM_SIGNAL_QUALITY_VALID, value,
                                   (uint32_t)value};
}

static void fill_ring(itm_telemetry_ring_t *ring)
{
    size_t index;
    for (index = 0U; index < ITM_TELEMETRY_RING_CAPACITY; ++index) {
        const itm_telemetry_event_t event = event_with_value((int64_t)index);
        (void)itm_telemetry_ring_push(ring, &event);
    }
}

bool test_telemetry_ring_drops_oldest(void)
{
    mock_critical_t critical;
    itm_telemetry_ring_t ring;
    itm_telemetry_event_t output;
    const itm_telemetry_event_t newest = event_with_value(99);

    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_ring_init(&ring, ITM_RING_DROP_OLDEST,
                                        mock_critical_port(&critical)));
    fill_ring(&ring);
    TEST_ASSERT_EQ(ITM_RING_PUSHED_DROPPED_OLDEST,
                   itm_telemetry_ring_push(&ring, &newest));
    TEST_ASSERT(itm_telemetry_ring_pop(&ring, &output));
    TEST_ASSERT_EQ(1, output.value);
    TEST_ASSERT_EQ(1U,
                   itm_telemetry_ring_statistics(&ring).dropped_oldest);
    return true;
}

bool test_telemetry_ring_rejects_newest(void)
{
    mock_critical_t critical;
    itm_telemetry_ring_t ring;
    itm_telemetry_event_t output;
    const itm_telemetry_event_t newest = event_with_value(99);

    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_ring_init(&ring, ITM_RING_REJECT_NEWEST,
                                        mock_critical_port(&critical)));
    fill_ring(&ring);
    TEST_ASSERT_EQ(ITM_RING_REJECTED_FULL,
                   itm_telemetry_ring_push(&ring, &newest));
    TEST_ASSERT(itm_telemetry_ring_pop(&ring, &output));
    TEST_ASSERT_EQ(0, output.value);
    TEST_ASSERT_EQ(1U,
                   itm_telemetry_ring_statistics(&ring).rejected_newest);
    return true;
}

bool test_telemetry_ring_signals_fault(void)
{
    mock_critical_t critical;
    itm_telemetry_ring_t ring;
    const itm_telemetry_event_t newest = event_with_value(99);

    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_ring_init(&ring, ITM_RING_SIGNAL_FAULT,
                                        mock_critical_port(&critical)));
    fill_ring(&ring);
    TEST_ASSERT_EQ(ITM_RING_FAULT_FULL,
                   itm_telemetry_ring_push(&ring, &newest));
    TEST_ASSERT_EQ(ITM_TELEMETRY_RING_CAPACITY,
                   itm_telemetry_ring_count(&ring));
    TEST_ASSERT_EQ(1U,
                   itm_telemetry_ring_statistics(&ring).fault_signals);
    return true;
}
