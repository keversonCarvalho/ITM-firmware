#include "mocks/mock_ports.h"

#include <string.h>

static uint32_t clock_now(void *context)
{
    return ((mock_clock_t *)context)->now_ms;
}

static itm_result_t can_send(void *context, const itm_can_frame_t *frame)
{
    mock_can_t *mock = context;
    if ((frame == NULL) || (frame->length > 8U) ||
        (mock->sent_count >= MOCK_PORT_QUEUE_CAPACITY)) {
        return ITM_ERROR_NO_SPACE;
    }
    mock->sent[mock->sent_count++] = *frame;
    return ITM_OK;
}

static itm_result_t can_receive(void *context, itm_can_frame_t *frame)
{
    mock_can_t *mock = context;
    if ((frame == NULL) || (mock->received_next >= mock->received_count)) {
        return ITM_ERROR_NOT_FOUND;
    }
    *frame = mock->received[mock->received_next++];
    return ITM_OK;
}

static itm_result_t serial_write(void *context, const uint8_t *data,
                                 size_t size)
{
    mock_serial_t *mock = context;
    if ((data == NULL) || ((mock->sent_size + size) > MOCK_SERIAL_BUFFER_SIZE)) {
        return ITM_ERROR_NO_SPACE;
    }
    (void)memcpy(&mock->sent[mock->sent_size], data, size);
    mock->sent_size += size;
    return ITM_OK;
}

static itm_result_t serial_read(void *context, uint8_t *data, size_t capacity,
                                size_t *received)
{
    mock_serial_t *mock = context;
    size_t size;
    if ((data == NULL) || (received == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    size = (mock->received_size < capacity) ? mock->received_size : capacity;
    (void)memcpy(data, mock->received, size);
    if (size < mock->received_size) {
        (void)memmove(mock->received, &mock->received[size],
                      mock->received_size - size);
    }
    mock->received_size -= size;
    *received = size;
    return (size == 0U) ? ITM_ERROR_NOT_FOUND : ITM_OK;
}

itm_clock_port_t mock_clock_port(mock_clock_t *mock)
{
    return (itm_clock_port_t){mock, clock_now};
}

itm_can_port_t mock_can_port(mock_can_t *mock)
{
    return (itm_can_port_t){mock, can_send, can_receive};
}

itm_serial_port_t mock_serial_port(mock_serial_t *mock)
{
    return (itm_serial_port_t){mock, serial_write, serial_read};
}
