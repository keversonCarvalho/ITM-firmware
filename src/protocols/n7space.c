#include "itm/protocols/n7space.h"
#include <lely/can/net.h>
#include <lely/co/detail/dev.h>
#include <lely/co/detail/obj.h>
#include <lely/co/nmt.h>
#include <lely/co/sdo.h>
#include <lely/util/mempool.h>
#include <string.h>

typedef struct {
    uint16_t index;
    uint8_t sub, type, code, access;
    uint32_t value;
} od_entry_t;
static const od_entry_t entries[] = {
#include "n7space_od.inc"
};
#define OD_COUNT (sizeof(entries) / sizeof(entries[0]))
static struct {
    union { max_align_t alignment; unsigned char bytes[32768]; } arena;
    struct mempool pool;
    alloc_t *allocator;
    struct co_dev dev;
    struct co_obj objects[OD_COUNT];
    struct co_sub subs[OD_COUNT];
    union co_val values[OD_COUNT];
    can_net_t *net;
    co_nmt_t *nmt;
    itm_can_port_t can;
    itm_canopen_ret_t *service;
    uint8_t upload[ITM_SIGNAL_COUNT * 16U];
    uint32_t previous_ms;
    uint64_t elapsed_ms;
    bool ready, allow_update, update_request;
    uint8_t boot_slot;
    uint32_t boot_version;
    bool boot_confirmed;
} adapter;

static int send_frame(const struct can_msg *msg, uint_least8_t bus, void *context)
{
    itm_can_frame_t frame = {0};
    (void)context;
    if (bus != 0 || msg->flags != 0 || msg->len > 8 || msg->id > 0x7ff)
        return -1;
    frame.id = msg->id;
    frame.length = msg->len;
    memcpy(frame.data, msg->data, msg->len);
    return adapter.can.send(adapter.can.context, &frame) == ITM_OK ? 0 : -1;
}

static co_unsigned32_t upload(const co_sub_t *sub, struct co_sdo_req *req,
                              co_unsigned32_t ac, void *context)
{
    size_t length = 0;
    const uint16_t index = co_obj_get_idx(co_sub_get_obj(sub));
    (void)context;
    if (ac) return ac;
    if (index >= 0x2103 && index <= 0x2106) {
        uint32_t value = index == 0x2104 ? adapter.boot_slot :
                         index == 0x2105 ? adapter.boot_version :
                         index == 0x2106 ? (uint32_t)adapter.boot_confirmed : 0;
        unsigned i;
        memset(adapter.upload, 0, 4);
        for (i = 0; i < 4; ++i) adapter.upload[i] = (uint8_t)(value >> (8U * i));
        length = 4;
    } else {
        ac = itm_canopen_ret_sdo_read(adapter.service, index,
            co_sub_get_subidx(sub), adapter.upload, sizeof(adapter.upload), &length);
    }
    if (!ac) co_sdo_req_up(req, adapter.upload, length);
    return ac;
}

static co_unsigned32_t download(co_sub_t *sub, struct co_sdo_req *req,
                                co_unsigned32_t ac, void *context)
{
    const void *data;
    size_t size;
    const uint16_t index = co_obj_get_idx(co_sub_get_obj(sub));
    (void)context;
    if (ac || co_sdo_req_dn(req, &data, &size, &ac)) return ac;
    if (index == 0x2103) {
        if (!adapter.allow_update) return ITM_SDO_ABORT_UNSUPPORTED_ACCESS;
        if (size != 4 || memcmp(data, "UPDT", 4)) return ITM_SDO_ABORT_VALUE_RANGE;
        adapter.update_request = true;
        return 0;
    }
    ac = itm_canopen_ret_sdo_write(adapter.service, index,
                                  co_sub_get_subidx(sub), data, size);
    if (!ac && index == ITM_OD_NODE_ID) {
        /* Applied at next reset, never change the active SDO connection. */
        co_sub_set_val_u8(sub, adapter.service->config.node_id);
    }
    return ac;
}

bool itm_n7space_init(itm_can_port_t can, itm_canopen_ret_t *service)
{
    size_t i;
    co_obj_t *obj = NULL;
    if (adapter.ready || !can.send || !service) return false;
    memset(&adapter, 0, sizeof(adapter));
    adapter.can = can;
    adapter.service = service;
    adapter.allocator = mempool_init(&adapter.pool, adapter.arena.bytes,
                                    sizeof(adapter.arena.bytes));
    if (!adapter.allocator || !co_dev_init(&adapter.dev, service->config.node_id))
        return false;
    for (i = 0; i < OD_COUNT; ++i) {
        const od_entry_t *entry = &entries[i];
        if (!obj || co_obj_get_idx(obj) != entry->index) {
            obj = co_obj_init(&adapter.objects[i], entry->index, NULL, 0);
            if (!obj || co_obj_set_code(obj, entry->code) ||
                co_dev_insert_obj(&adapter.dev, obj)) return false;
        }
        if (!co_sub_init(&adapter.subs[i], entry->sub, entry->type,
                         &adapter.values[i])) return false;
        if (entry->type == 5) adapter.values[i].u8 = (uint8_t)entry->value;
        if (entry->type == 6) adapter.values[i].u16 = (uint16_t)entry->value;
        if (entry->type == 7) adapter.values[i].u32 = entry->value;
        if (entry->index == 0x1017) adapter.values[i].u16 = service->config.heartbeat_producer_ms;
        if (co_obj_insert_sub(obj, &adapter.subs[i]) ||
            co_sub_set_access(&adapter.subs[i], entry->access)) return false;
        co_sub_set_up_ind(&adapter.subs[i], upload, NULL);
        co_sub_set_dn_ind(&adapter.subs[i], download, NULL);
    }
    adapter.net = can_net_create(adapter.allocator, 0);
    if (!adapter.net) return false;
    can_net_set_send_func(adapter.net, send_frame, NULL);
    adapter.nmt = co_nmt_create(adapter.net, &adapter.dev);
    if (!adapter.nmt) { itm_n7space_fini(); return false; }
    /* N7 owns standard communication-object handlers such as 0x1017. */
    if (co_nmt_cs_ind(adapter.nmt, CO_NMT_CS_RESET_NODE) != 0) {
        itm_n7space_fini(); return false;
    }
    adapter.ready = true;
    return true;
}

void itm_n7space_fini(void)
{
    if (adapter.nmt) co_nmt_destroy(adapter.nmt);
    if (adapter.net) can_net_destroy(adapter.net);
    adapter.nmt = NULL;
    adapter.net = NULL;
    adapter.ready = false;
}

itm_result_t itm_n7space_receive(const itm_can_frame_t *frame)
{
    struct can_msg msg = CAN_MSG_INIT;
    if (!adapter.ready) return ITM_ERROR_NOT_READY;
    if (!frame || frame->id > 0x7ff || frame->length > 8) return ITM_ERROR_INVALID_ARGUMENT;
    msg.id = frame->id;
    msg.len = frame->length;
    memcpy(msg.data, frame->data, msg.len);
    return can_net_recv(adapter.net, &msg, 0) < 0 ? ITM_ERROR_IO : ITM_OK;
}

itm_result_t itm_n7space_tick(uint32_t now_ms)
{
    struct timespec time;
    if (!adapter.ready) return ITM_ERROR_NOT_READY;
    adapter.elapsed_ms += (uint32_t)(now_ms - adapter.previous_ms);
    adapter.previous_ms = now_ms;
    time.tv_sec = (time_t)(adapter.elapsed_ms / 1000U);
    time.tv_nsec = (long)((adapter.elapsed_ms % 1000U) * 1000000U);
    adapter.service->config.heartbeat_producer_ms = co_dev_get_val_u16(&adapter.dev, 0x1017, 0);
    return can_net_set_time(adapter.net, &time) ? ITM_ERROR_IO : ITM_OK;
}
bool itm_n7space_take_update_request(void)
{
    const bool requested = adapter.update_request;
    adapter.update_request = false;
    return requested;
}
void itm_n7space_allow_update(bool allowed) { adapter.allow_update = allowed; }
size_t itm_n7space_pool_used(void) { return mem_size(adapter.allocator); }
void itm_n7space_set_boot_info(uint8_t slot, uint32_t version, bool confirmed)
{
    adapter.boot_slot = slot; adapter.boot_version = version;
    adapter.boot_confirmed = confirmed;
}
