#include "mocks/mock_critical.h"

#include <string.h>

static itm_critical_state_t enter_critical(void *context)
{
    mock_critical_t *mock = context;
    const itm_critical_state_t previous = mock->active_count;
    mock->enter_count++;
    mock->active_count++;
    if (mock->active_count > mock->maximum_active_count) {
        mock->maximum_active_count = mock->active_count;
    }
    return previous;
}

static void exit_critical(void *context,
                          itm_critical_state_t previous_state)
{
    mock_critical_t *mock = context;
    mock->exit_count++;
    mock->active_count = previous_state;
}

void mock_critical_init(mock_critical_t *mock)
{
    (void)memset(mock, 0, sizeof(*mock));
}

itm_critical_section_port_t mock_critical_port(mock_critical_t *mock)
{
    return (itm_critical_section_port_t){mock, enter_critical, exit_critical};
}
