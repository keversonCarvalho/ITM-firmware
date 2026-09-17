#ifndef ITM_TESTS_MOCK_PORTS_H
#define ITM_TESTS_MOCK_PORTS_H

#include <stddef.h>
#include <stdint.h>
#include "itm/ports/can.h"
#include "itm/ports/clock.h"
#include "itm/ports/serial.h"

#define MOCK_PORT_QUEUE_CAPACITY 8U
#define MOCK_SERIAL_BUFFER_SIZE 256U

typedef struct {
    uint32_t now_ms;
} mock_clock_t;

typedef struct {
    itm_can_frame_t received[MOCK_PORT_QUEUE_CAPACITY];
    itm_can_frame_t sent[MOCK_PORT_QUEUE_CAPACITY];
    size_t received_count;
    size_t received_next;
    size_t sent_count;
} mock_can_t;

typedef struct {
    uint8_t received[MOCK_SERIAL_BUFFER_SIZE];
    uint8_t sent[MOCK_SERIAL_BUFFER_SIZE];
    size_t received_size;
    size_t sent_size;
} mock_serial_t;

itm_clock_port_t mock_clock_port(mock_clock_t *mock);
itm_can_port_t mock_can_port(mock_can_t *mock);
itm_serial_port_t mock_serial_port(mock_serial_t *mock);

#endif
