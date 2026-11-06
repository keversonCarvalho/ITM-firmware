#include "itm_bc_network.h"
#include "itm_update_platform.h"
#include "itm/protocols/itm_dds_snapshot.h"
#include "itm/services/power_authority.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include <uxr/client/client.h>

#include <string.h>
#include <errno.h>

#define ITM_AGENT_PORT 2018U
#define ITM_AGENT_ADDRESS "10.44.0.1"
#define XRCE_MTU 768U

static struct netif ppp_netif;
static ppp_pcb *ppp;
static volatile bool tcpip_ready, ppp_ready;
static volatile bool xrce_ready;
static volatile uint32_t ppp_tx_errors, ppp_rx_errors;
static uxrCustomTransport xrce_transport;
static uxrSession xrce_session;
static uint8_t reliable_out_buffer[XRCE_MTU * 2U];
static uint8_t reliable_in_buffer[XRCE_MTU * 2U];
static uint8_t best_effort_buffer[XRCE_MTU];
static itm_telemetry_snapshot_t snapshot;
static itm_dds_snapshot_t topic;
static uint8_t payload[ITM_DDS_CDR_SIZE];
static uint8_t status_payload[ITM_DDS_POWER_STATUS_CDR_SIZE];
static int agent_fd = -1;
static QueueHandle_t event_queue, status_queue;
static uint16_t command_request_id;
static uxrObjectId command_reader;
uint8_t ucHeap[configTOTAL_HEAP_SIZE]
    __attribute__((section(".axi_bss"), aligned(8)));

bool itm_bc_network_ready(void) { return xrce_ready; }

bool itm_bc_network_take_event(itm_bc_event_t *event)
{
    return event && event_queue &&
           xQueueReceive(event_queue, event, 0U) == pdTRUE;
}

bool itm_bc_network_submit_status(const itm_dds_power_status_t *status)
{
    return status && status_queue && xQueueOverwrite(status_queue, status) == pdTRUE;
}

static void queue_event(const itm_bc_event_t *event)
{
    if (event_queue && event) (void)xQueueSend(event_queue, event, 0U);
}

uint32_t itm_lwip_random(void)
{
    static uint32_t state;
    if (state == 0U)
        state = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2() ^
                HAL_GetTick() ^ 0x49544d31UL;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

static void tcpip_started(void *arg)
{
    (void)arg;
    tcpip_ready = true;
}

static u32_t ppp_serial_output(ppp_pcb *pcb, const void *data, u32_t length,
                               void *context)
{
    const uint32_t started = HAL_GetTick();
    (void)pcb; (void)context;
    while (!itm_update_bc_uart_send(data, length)) {
        if ((uint32_t)(HAL_GetTick() - started) >= 100U) {
            ++ppp_tx_errors;
            return 0U;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return length;
}

static void ppp_status(ppp_pcb *pcb, int error, void *context)
{
    (void)pcb; (void)context;
    ppp_ready = error == PPPERR_NONE;
}

static void ppp_task(void *arg)
{
    ip4_addr_t local, remote;
    uint8_t incoming[256];
    uint32_t last_retry = 0U;
    (void)arg;
    tcpip_init(tcpip_started, NULL);
    while (!tcpip_ready) vTaskDelay(pdMS_TO_TICKS(1));
    ppp = pppapi_pppos_create(&ppp_netif, ppp_serial_output, ppp_status, NULL);
    if (!ppp || !ip4addr_aton("10.44.0.2", &local) ||
        !ip4addr_aton(ITM_AGENT_ADDRESS, &remote)) vTaskDelete(NULL);
    ppp_set_ipcp_ouraddr(ppp, &local);
    ppp_set_ipcp_hisaddr(ppp, &remote);
    (void)pppapi_set_default(ppp);
    (void)pppapi_connect(ppp, 0U);
    last_retry = HAL_GetTick();
    for (;;) {
        size_t count = 0U;
        while (count < sizeof(incoming) && itm_update_bc_uart_read(&incoming[count]))
            ++count;
        if (count > 0U && pppos_input_tcpip(ppp, incoming, (int)count) != ERR_OK)
            ++ppp_rx_errors;
        if (!ppp_ready && (uint32_t)(HAL_GetTick() - last_retry) >= 5000U) {
            last_retry = HAL_GetTick();
            (void)pppapi_connect(ppp, 0U);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static bool send_all(int fd, const uint8_t *data, size_t length)
{
    size_t done = 0U;
    while (done < length) {
        const int sent = lwip_send(fd, data + done, length - done, 0);
        if (sent <= 0) return false;
        done += (size_t)sent;
    }
    return true;
}

/* 1 complete, 0 idle timeout, -1 broken stream (including partial timeout). */
static int receive_all(int fd, uint8_t *data, size_t length, int timeout)
{
    size_t done = 0U;
    const uint32_t started = HAL_GetTick();
    while (done < length) {
        const int got = lwip_recv(fd, data + done, length - done, MSG_DONTWAIT);
        if (got > 0) { done += (size_t)got; continue; }
        if (got == 0 || !ppp_ready) return -1;
        if (errno != EWOULDBLOCK && errno != EAGAIN) return -1;
        if ((uint32_t)(HAL_GetTick() - started) >=
            (uint32_t)(done > 0U && timeout < 100 ? 100 : timeout))
            return done == 0U ? 0 : -1;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 1;
}

static bool agent_open(uxrCustomTransport *transport)
{
    struct sockaddr_in address = {0};
    const int send_timeout_ms = 250;
    (void)transport;
    agent_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (agent_fd < 0) return false;
    (void)lwip_setsockopt(agent_fd, SOL_SOCKET, SO_SNDTIMEO,
                          &send_timeout_ms, sizeof(send_timeout_ms));
    address.sin_family = AF_INET;
    address.sin_port = lwip_htons(ITM_AGENT_PORT);
    address.sin_addr.s_addr = ipaddr_addr(ITM_AGENT_ADDRESS);
    if (lwip_connect(agent_fd, (struct sockaddr *)&address,
                     sizeof(address)) == 0) return true;
    lwip_close(agent_fd);
    agent_fd = -1;
    return false;
}

static bool agent_close(uxrCustomTransport *transport)
{
    (void)transport;
    if (agent_fd >= 0) { lwip_close(agent_fd); agent_fd = -1; }
    return true;
}

static size_t agent_write(uxrCustomTransport *transport, const uint8_t *data,
                          size_t length, uint8_t *error)
{
    const uint8_t prefix[2] = {(uint8_t)length, (uint8_t)(length >> 8U)};
    (void)transport;
    *error = 0U;
    if (agent_fd < 0 || length > UINT16_MAX ||
        !send_all(agent_fd, prefix, sizeof(prefix)) ||
        !send_all(agent_fd, data, length)) {
        *error = 1U;
        return 0U;
    }
    return length;
}

static size_t agent_read(uxrCustomTransport *transport, uint8_t *data,
                         size_t capacity, int timeout, uint8_t *error)
{
    uint8_t prefix[2];
    size_t length;
    int received;
    (void)transport;
    *error = 0U;
    if (agent_fd < 0) return 0U;
    received = receive_all(agent_fd, prefix, 2U, timeout);
    if (received == 0) return 0U;
    if (received < 0) {
        *error = 1U;
        return 0U;
    }
    length = (size_t)prefix[0] | ((size_t)prefix[1] << 8U);
    if (length == 0U || length > capacity ||
        receive_all(agent_fd, data, length,
                    timeout < 100 ? 100 : timeout) != 1) {
        *error = 1U;
        return 0U;
    }
    return length;
}

static bool create_entity(uxrStreamId output, uxrObjectId object,
                          uxrObjectId parent, const char *xml, unsigned kind)
{
    uint16_t request;
    uint8_t status = 0xffU;
    if (kind == 0U)
        request = uxr_buffer_create_participant_xml(&xrce_session, output,
                                                     object, 0U, xml, UXR_REUSE);
    else if (kind == 1U)
        request = uxr_buffer_create_topic_xml(&xrce_session, output,
                                               object, parent, xml, UXR_REUSE);
    else if (kind == 2U)
        request = uxr_buffer_create_publisher_xml(&xrce_session, output,
                                                   object, parent, xml, UXR_REUSE);
    else if (kind == 3U)
        request = uxr_buffer_create_datawriter_xml(&xrce_session, output,
                                                    object, parent, xml, UXR_REUSE);
    else if (kind == 4U)
        request = uxr_buffer_create_subscriber_xml(&xrce_session, output,
                                                    object, parent, xml, UXR_REUSE);
    else
        request = uxr_buffer_create_datareader_xml(&xrce_session, output,
                                                    object, parent, xml, UXR_REUSE);
    return request != 0U &&
           uxr_run_session_until_all_status(&xrce_session, 2000,
                                            &request, &status, 1U) &&
           (status == UXR_STATUS_OK || status == UXR_STATUS_OK_MATCHED);
}

static bool new_session_id(uint64_t *id)
{
    static uint64_t previous;
    uint32_t high, low;
    unsigned tries;
    if (!id) return false;
    for (tries = 0U; tries < 4U; ++tries) {
        if (!itm_update_random_u32(&high) || !itm_update_random_u32(&low))
            return false;
        *id = ((uint64_t)high << 32U) | low;
        if (*id != 0U && *id != previous) { previous = *id; return true; }
    }
    return false;
}

static void command_received(uxrSession *session, uxrObjectId object_id,
                             uint16_t request_id, uxrStreamId stream_id,
                             ucdrBuffer *cdr, uint16_t length, void *args)
{
    uint8_t bytes[ITM_DDS_POWER_COMMAND_CDR_SIZE];
    itm_bc_event_t event = {.kind = ITM_BC_EVENT_MALFORMED_COMMAND};
    (void)session; (void)stream_id; (void)args;
    if (object_id.id == command_reader.id && request_id == command_request_id &&
        length == sizeof(bytes) &&
        ucdr_deserialize_array_uint8_t(cdr, bytes, sizeof(bytes)) &&
        itm_dds_power_command_deserialize(bytes, sizeof(bytes),
                                           &event.command) == ITM_OK)
        event.kind = ITM_BC_EVENT_COMMAND;
    queue_event(&event);
}

static void xrce_task(void *arg)
{
    static const char participant_xml[] =
        "<dds><participant><rtps><name>ITM</name></rtps></participant></dds>";
    static const char topic_xml[] =
        "<dds><topic><name>ITM_Snapshot_v2</name>"
        "<dataType>itm::telemetry::SnapshotV2</dataType></topic></dds>";
    static const char publisher_xml[] = "<dds><publisher/></dds>";
    static const char writer_xml[] =
        "<dds><data_writer><topic><kind>NO_KEY</kind>"
        "<name>ITM_Snapshot_v2</name>"
        "<dataType>itm::telemetry::SnapshotV2</dataType>"
        "</topic></data_writer></dds>";
    static const char status_topic_xml[] =
        "<dds><topic><name>ITM_PowerStatus_v1</name>"
        "<dataType>itm::power::Status</dataType></topic></dds>";
    static const char status_writer_xml[] =
        "<dds><data_writer><topic><kind>NO_KEY</kind>"
        "<name>ITM_PowerStatus_v1</name><dataType>itm::power::Status</dataType>"
        "</topic><qos><reliability><kind>RELIABLE</kind></reliability>"
        "<durability><kind>VOLATILE</kind></durability><history>"
        "<kind>KEEP_LAST</kind><depth>1</depth></history></qos>"
        "</data_writer></dds>";
    static const char command_topic_xml[] =
        "<dds><topic><name>ITM_PowerCommand_v1</name>"
        "<dataType>itm::power::Command</dataType></topic></dds>";
    static const char subscriber_xml[] = "<dds><subscriber/></dds>";
    static const char command_reader_xml[] =
        "<dds><data_reader><topic><kind>NO_KEY</kind>"
        "<name>ITM_PowerCommand_v1</name><dataType>itm::power::Command</dataType>"
        "</topic><qos><reliability><kind>RELIABLE</kind></reliability>"
        "<durability><kind>VOLATILE</kind></durability><history>"
        "<kind>KEEP_LAST</kind><depth>1</depth></history></qos>"
        "</data_reader></dds>";
    const uxrObjectId participant = uxr_object_id(1U, UXR_PARTICIPANT_ID);
    const uxrObjectId topic_id = uxr_object_id(1U, UXR_TOPIC_ID);
    const uxrObjectId publisher = uxr_object_id(1U, UXR_PUBLISHER_ID);
    const uxrObjectId writer = uxr_object_id(1U, UXR_DATAWRITER_ID);
    const uxrObjectId status_topic = uxr_object_id(2U, UXR_TOPIC_ID);
    const uxrObjectId status_writer = uxr_object_id(2U, UXR_DATAWRITER_ID);
    const uxrObjectId command_topic = uxr_object_id(3U, UXR_TOPIC_ID);
    const uxrObjectId subscriber = uxr_object_id(1U, UXR_SUBSCRIBER_ID);
    (void)arg;
    command_reader = uxr_object_id(1U, UXR_DATAREADER_ID);
    for (;;) {
        uxrStreamId reliable, reliable_input, best_effort;
        uint32_t last_publish, last_status;
        uint64_t session_id;
        bool announced = false, have_status = false, status_changed = false;
        itm_dds_power_status_t latest_status;
        itm_bc_event_t event;
        uxrDeliveryControl delivery = {UXR_MAX_SAMPLES_UNLIMITED,
            UXR_MAX_ELAPSED_TIME_UNLIMITED,
            UXR_MAX_BYTES_PER_SECOND_UNLIMITED, 0U};
        if (!ppp_ready) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        if (!new_session_id(&session_id)) {
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        uxr_set_custom_transport_callbacks(&xrce_transport, false, agent_open,
                                            agent_close, agent_write, agent_read);
        if (!uxr_init_custom_transport(&xrce_transport, NULL)) {
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        uxr_init_session(&xrce_session, &xrce_transport.comm,
                         (uint32_t)session_id ^ (uint32_t)(session_id >> 32U));
        if (!uxr_create_session(&xrce_session)) goto disconnected;
        reliable = uxr_create_output_reliable_stream(&xrce_session,
                      reliable_out_buffer, sizeof(reliable_out_buffer), 2U);
        reliable_input = uxr_create_input_reliable_stream(&xrce_session,
                      reliable_in_buffer, sizeof(reliable_in_buffer), 2U);
        best_effort = uxr_create_output_best_effort_stream(&xrce_session,
                      best_effort_buffer, sizeof(best_effort_buffer));
        if (!create_entity(reliable, participant, participant,
                           participant_xml, 0U) ||
            !create_entity(reliable, topic_id, participant, topic_xml, 1U) ||
            !create_entity(reliable, publisher, participant, publisher_xml, 2U) ||
            !create_entity(reliable, writer, publisher, writer_xml, 3U) ||
            !create_entity(reliable, status_topic, participant,
                           status_topic_xml, 1U) ||
            !create_entity(reliable, status_writer, publisher,
                           status_writer_xml, 3U) ||
            !create_entity(reliable, command_topic, participant,
                           command_topic_xml, 1U) ||
            !create_entity(reliable, subscriber, participant,
                           subscriber_xml, 4U) ||
            !create_entity(reliable, command_reader, subscriber,
                           command_reader_xml, 5U))
            goto disconnected;
        uxr_set_topic_callback(&xrce_session, command_received, NULL);
        command_request_id = uxr_buffer_request_data(&xrce_session, reliable,
            command_reader, reliable_input, &delivery);
        if (command_request_id == 0U || !uxr_run_session_time(&xrce_session, 10))
            goto disconnected;
        xrce_ready = true;
        event = (itm_bc_event_t){.kind = ITM_BC_EVENT_SESSION_STARTED,
                                 .session_id = session_id};
        queue_event(&event); announced = true;
        last_publish = last_status = HAL_GetTick();
        while (ppp_ready) {
            size_t length;
            ucdrBuffer cdr;
            const uint32_t now = HAL_GetTick();
            itm_dds_power_status_t pending;
            if (xQueueReceive(status_queue, &pending, 0U) == pdTRUE) {
                latest_status = pending; have_status = true; status_changed = true;
            }
            if ((uint32_t)(now - last_publish) >= ITM_DDS_PUBLISH_PERIOD_MS) {
                last_publish = now; length = 0U;
                if (itm_update_app_snapshot(&snapshot) == ITM_OK &&
                    itm_dds_snapshot_prepare(&snapshot, &topic) == ITM_OK) {
                    topic.cb_condition = itm_update_app_cb_condition();
                    if (itm_dds_snapshot_serialize(&topic, payload,
                            sizeof(payload), &length) == ITM_OK &&
                        uxr_prepare_output_stream(&xrce_session, best_effort,
                            writer, &cdr, (uint32_t)length) != 0U)
                        (void)ucdr_serialize_array_uint8_t(&cdr, payload, length);
                }
            }
            if (have_status && (status_changed ||
                                (uint32_t)(now - last_status) >= 250U)) {
                length = 0U; last_status = now;
                if (itm_dds_power_status_serialize(&latest_status,
                        status_payload, sizeof(status_payload), &length) == ITM_OK &&
                    uxr_prepare_output_stream(&xrce_session, reliable,
                        status_writer, &cdr, (uint32_t)length) != 0U)
                    (void)ucdr_serialize_array_uint8_t(&cdr, status_payload, length);
                status_changed = false;
            }
            if (!uxr_run_session_time(&xrce_session, 1)) break;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
disconnected:
        xrce_ready = false;
        if (announced) {
            event = (itm_bc_event_t){.kind = ITM_BC_EVENT_SESSION_LOST,
                                     .session_id = session_id};
            queue_event(&event);
        }
        (void)uxr_close_custom_transport(&xrce_transport);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool itm_bc_network_start(void)
{
    event_queue = xQueueCreate(8U, sizeof(itm_bc_event_t));
    status_queue = xQueueCreate(1U, sizeof(itm_dds_power_status_t));
    if (!event_queue || !status_queue) return false;
    return xTaskCreate(ppp_task, "ppp", 768U, NULL, 3U, NULL) == pdPASS &&
           xTaskCreate(xrce_task, "xrce", 1536U, NULL, 2U, NULL) == pdPASS;
}
