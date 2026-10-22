#include "test_framework.h"
#include "itm/protocols/xcp_update.h"
#include "itm/protocols/n7space.h"
#include <string.h>

static uint8_t flash[4][ITM_FW_SLOT_SIZE];
static int fail_after;
static int unreadable_region;
static bool auth_ok;
static bool read_flash(void *p, unsigned region, uint32_t off, void *dst, size_t n)
{
    (void)p;
    if (region > 3 || (int)region == unreadable_region ||
        off > ITM_FW_SLOT_SIZE || n > ITM_FW_SLOT_SIZE - off) return false;
    memcpy(dst, flash[region] + off, n); return true;
}
static bool mutation(void)
{
    if (fail_after == 0) return false;
    if (fail_after > 0) fail_after--;
    return true;
}
static bool erase_flash(void *p, unsigned region)
{
    (void)p;
    if (region > 3 || !mutation()) return false;
    memset(flash[region], 0xff, ITM_FW_SLOT_SIZE); return true;
}
static bool program_flash(void *p, unsigned region, uint32_t off, const void *src, size_t n)
{
    size_t i;
    (void)p;
    if (region > 3 || off > ITM_FW_SLOT_SIZE || n > ITM_FW_SLOT_SIZE - off ||
        (off % 32) || n != 32 || !mutation()) return false;
    for (i = 0; i < n; ++i) if (flash[region][off + i] != 0xff) return false;
    memcpy(flash[region] + off, src, n); return true;
}
static bool authorize(void *p, unsigned slot, const uint8_t *header)
{
    (void)p; (void)slot; (void)header; return auth_ok;
}
static itm_fw_port_t port(void)
{
    return (itm_fw_port_t){NULL, read_flash, erase_flash, program_flash, authorize};
}
static void put32(uint8_t *p, uint32_t v)
{
    unsigned i;
    for (i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8U * i));
}
static void package(uint8_t *image, unsigned slot)
{
    const uint32_t base = itm_fw_slot_address(slot) + ITM_FW_HEADER_SIZE;
    memset(image, 0, 576);
    memcpy(image, "ITMF", 4); put32(image + 4, 1);
    put32(image + 8, ITM_FW_TARGET); put32(image + 12, base);
    put32(image + 16, 64); put32(image + 20, 7);
    put32(image + 512, 0x20020000); put32(image + 516, base + 9);
    put32(image + 24, itm_fw_crc32(image + 512, 64));
    put32(image + 28, itm_fw_crc32(image, 28));
}
static bool stage(itm_fw_update_t *fw, unsigned slot)
{
    uint8_t image[576]; package(image, slot);
    return itm_fw_begin(fw, sizeof(image)) && itm_fw_write(fw, image, sizeof(image)) && itm_fw_finish(fw);
}
static bool save_canopen_metadata(void *context,
    const itm_canopen_persistent_config_t *configuration)
{
    itm_fw_update_t *fw = context;
    return itm_fw_save_configuration(fw, configuration->node_id,
        configuration->heartbeat_producer_ms,
        configuration->configuration_version);
}
static bool provision(itm_fw_update_t *fw)
{
    uint8_t slot;
    memset(flash, 0xff, sizeof(flash)); fail_after = -1;
    unreadable_region = -1; auth_ok = true;
    return itm_fw_init(fw, port()) && stage(fw, 0) &&
           itm_fw_select_boot(fw, &slot) && slot == 0 && itm_fw_confirm(fw, 0);
}
bool test_update_rollback(void)
{
    itm_fw_update_t fw;
    uint8_t slot;
    TEST_ASSERT(provision(&fw));
    TEST_ASSERT(stage(&fw, 1));
    TEST_ASSERT(itm_fw_select_boot(&fw, &slot)); TEST_ASSERT_EQ(1, slot);
    TEST_ASSERT(itm_fw_init(&fw, port())); /* Power loss before confirmation. */
    TEST_ASSERT(itm_fw_select_boot(&fw, &slot)); TEST_ASSERT_EQ(0, slot);
    TEST_ASSERT_EQ(ITM_FW_NONE, fw.candidate);
    TEST_ASSERT(stage(&fw, 1)); TEST_ASSERT(itm_fw_select_boot(&fw, &slot));
    TEST_ASSERT(itm_fw_confirm(&fw, 1));
    TEST_ASSERT(itm_fw_init(&fw, port()));
    TEST_ASSERT(itm_fw_select_boot(&fw, &slot)); TEST_ASSERT_EQ(1, slot);
    return true;
}
bool test_update_interruptions(void)
{
    int cut;
    for (cut = 0; cut < 30; ++cut) {
        itm_fw_update_t fw;
        uint8_t slot;
        TEST_ASSERT(provision(&fw));
        fail_after = cut;
        (void)stage(&fw, 1);
        fail_after = -1;
        TEST_ASSERT(itm_fw_init(&fw, port()));
        TEST_ASSERT(itm_fw_validate(&fw, 0));
        TEST_ASSERT(itm_fw_select_boot(&fw, &slot));
        TEST_ASSERT(slot == 0 || slot == 1);
        if (slot == 1) {
            TEST_ASSERT(itm_fw_init(&fw, port()));
            TEST_ASSERT(itm_fw_select_boot(&fw, &slot));
            TEST_ASSERT_EQ(0, slot);
        }
    }
    return true;
}
bool test_update_shared_metadata(void)
{
    int cut;
    for (cut = 0; cut < 3; ++cut) {
        itm_fw_update_t fw;
        uint8_t slot;
        TEST_ASSERT(provision(&fw));
        TEST_ASSERT(itm_fw_save_configuration(&fw, 42, 250, 1));
        TEST_ASSERT(stage(&fw, 1));
        TEST_ASSERT(itm_fw_select_boot(&fw, &slot));
        TEST_ASSERT_EQ(1, slot);
        TEST_ASSERT(itm_fw_confirm(&fw, 1));
        TEST_ASSERT(itm_fw_init(&fw, port()));
        TEST_ASSERT_EQ(42, fw.node_id);
        TEST_ASSERT_EQ(250, fw.heartbeat_producer_ms);
        TEST_ASSERT_EQ(1, fw.configuration_version);
        TEST_ASSERT_EQ(1, fw.confirmed);
        fail_after = cut;
        TEST_ASSERT(!itm_fw_save_configuration(&fw, 43, 300, 2));
        fail_after = -1;
        TEST_ASSERT(itm_fw_init(&fw, port()));
        TEST_ASSERT_EQ(42, fw.node_id);
        TEST_ASSERT_EQ(1, fw.configuration_version);
        TEST_ASSERT_EQ(1, fw.confirmed);
        TEST_ASSERT(itm_fw_select_boot(&fw, &slot));
        TEST_ASSERT_EQ(1, slot);
    }
    return true;
}
bool test_update_legacy_metadata_migration(void)
{
    itm_fw_update_t fw;
    uint8_t *record;
    TEST_ASSERT(provision(&fw));
    record = flash[fw.journal];
    record[15] = 0xff;
    record[16] = 0xff;
    record[17] = 0xff;
    record[18] = 0xff;
    memset(record + 20, 0xff, 4);
    put32(record + 28, itm_fw_crc32(record, 28));
    put32(record + 40, itm_fw_crc32(record, 28));
    TEST_ASSERT(itm_fw_init(&fw, port()));
    TEST_ASSERT(!fw.configuration_present);
    TEST_ASSERT_EQ(10, fw.node_id);
    TEST_ASSERT(itm_fw_migrate_metadata(&fw));
    TEST_ASSERT(itm_fw_init(&fw, port()));
    TEST_ASSERT(fw.configuration_present);
    TEST_ASSERT_EQ(10, fw.node_id);
    TEST_ASSERT_EQ(0, fw.configuration_version);
    return true;
}
bool test_update_metadata_read_fallback(void)
{
    itm_fw_update_t fw;
    TEST_ASSERT(provision(&fw));
    TEST_ASSERT(itm_fw_save_configuration(&fw, 42, 250, 1));
    unreadable_region = fw.journal;
    TEST_ASSERT(itm_fw_init(&fw, port()));
    TEST_ASSERT_EQ(10, fw.node_id);
    TEST_ASSERT_EQ(0, fw.configuration_version);
    TEST_ASSERT_EQ(0, fw.confirmed);
    unreadable_region = -1;
    return true;
}
bool test_update_canopen_configuration_journal(void)
{
    itm_fw_update_t fw;
    itm_canopen_ret_t service;
    uint8_t node = 42;
    uint32_t signature = 0x65766173UL;
    TEST_ASSERT(provision(&fw));
    TEST_ASSERT(itm_canopen_ret_init(&service, (itm_canopen_identity_t){0},
        (itm_canopen_persistent_config_t){fw.node_id,
            fw.heartbeat_producer_ms, fw.configuration_version}, NULL));
    TEST_ASSERT(itm_canopen_ret_bind_config_store(
        &service, save_canopen_metadata, &fw));
    TEST_ASSERT_EQ(ITM_SDO_ABORT_NONE, itm_canopen_ret_sdo_write(
        &service, ITM_OD_NODE_ID, 0, &node, sizeof(node)));
    TEST_ASSERT_EQ(ITM_SDO_ABORT_NONE, itm_canopen_ret_sdo_write(
        &service, ITM_OD_STORE_PARAMETERS, 1,
        (const uint8_t *)&signature, sizeof(signature)));
    TEST_ASSERT(itm_fw_init(&fw, port()));
    TEST_ASSERT_EQ(42, fw.node_id);
    TEST_ASSERT_EQ(1, fw.configuration_version);
    TEST_ASSERT_EQ(0, fw.confirmed);
    return true;
}
bool test_update_rejects_invalid(void)
{
    itm_fw_update_t fw;
    uint8_t image[576];
    TEST_ASSERT(provision(&fw));
    package(image, 0); /* Wrong link address for inactive slot B. */
    TEST_ASSERT(itm_fw_begin(&fw, sizeof(image)));
    TEST_ASSERT(itm_fw_write(&fw, image, sizeof(image)));
    TEST_ASSERT(!itm_fw_finish(&fw));
    package(image, 1); image[560] ^= 1;
    TEST_ASSERT(itm_fw_begin(&fw, sizeof(image)));
    TEST_ASSERT(itm_fw_write(&fw, image, sizeof(image)));
    TEST_ASSERT(!itm_fw_finish(&fw));
    auth_ok = false; TEST_ASSERT(!stage(&fw, 1)); auth_ok = true;
    TEST_ASSERT_EQ(ITM_FW_NONE, fw.candidate);
    TEST_ASSERT(!itm_fw_begin(&fw, ITM_FW_SLOT_SIZE + 1));
    TEST_ASSERT(itm_fw_begin(&fw, 576));
    TEST_ASSERT(!itm_fw_write(&fw, image, 577));
    TEST_ASSERT(!itm_fw_finish(&fw));
    return true;
}
bool test_xcp_update(void)
{
    itm_fw_update_t fw;
    itm_xcp_update_t x;
    itm_can_frame_t q = {ITM_XCP_RX_ID, {0xff, 0}, 2}, r;
    uint8_t image[576]; size_t i;
    TEST_ASSERT(provision(&fw)); package(image, 1); itm_xcp_init(&x, &fw);
    TEST_ASSERT(itm_xcp_receive(&x, &q, 0, &r)); TEST_ASSERT_EQ(0x10, r.data[1]);
    q.data[0] = 0xd1; q.length = 8;
    itm_xcp_receive(&x, &q, 1, &r); TEST_ASSERT_EQ(0xfe, r.data[0]);
    q.data[0] = 0xd2; q.length = 1;
    itm_xcp_receive(&x, &q, 2, &r); TEST_ASSERT_EQ(0xff, r.data[0]);
    q.data[0] = 0xf6; q.length = 8; memset(q.data + 1, 0, 7);
    put32(q.data + 4, ITM_FW_SLOT_A);
    itm_xcp_receive(&x, &q, 3, &r); TEST_ASSERT_EQ(0xfe, r.data[0]);
    put32(q.data + 4, ITM_FW_SLOT_B);
    itm_xcp_receive(&x, &q, 4, &r); TEST_ASSERT_EQ(0xff, r.data[0]);
    q.data[0] = 0xd1; put32(q.data + 4, sizeof(image));
    itm_xcp_receive(&x, &q, 5, &r); TEST_ASSERT_EQ(0xff, r.data[0]);
    for (i = 0; i < sizeof(image); i += 6) {
        q.data[0] = 0xd0; q.data[1] = 6; memcpy(q.data + 2, image + i, 6);
        itm_xcp_receive(&x, &q, (uint32_t)(i + 6), &r); TEST_ASSERT_EQ(0xff, r.data[0]);
    }
    q.data[0] = 0xd0; q.data[1] = 0; q.length = 2;
    itm_xcp_receive(&x, &q, 600, &r); TEST_ASSERT_EQ(0xff, r.data[0]);
    TEST_ASSERT_EQ(1, fw.candidate);
    q.data[0] = 0xcf; q.length = 1;
    itm_xcp_receive(&x, &q, 601, &r); TEST_ASSERT(x.reset_requested);
    itm_xcp_init(&x, &fw); q.data[0] = 0xff; q.data[1] = 0; q.length = 2;
    itm_xcp_receive(&x, &q, 0, &r);
    itm_xcp_tick(&x, ITM_XCP_TIMEOUT_MS + 1); TEST_ASSERT(!x.connected);
    return true;
}
static itm_can_frame_t last_tx;
static itm_result_t send_can(void *p, const itm_can_frame_t *f)
{
    (void)p; last_tx = *f; return ITM_OK;
}
bool test_n7space_wire(void)
{
    itm_canopen_ret_t service;
    const uint8_t uid[12] = {1, 2, 3};
    itm_can_frame_t q = {0x60a, {0x40, 0x18, 0x10, 1}, 8};
    TEST_ASSERT(itm_canopen_ret_init(&service, (itm_canopen_identity_t){0},
                 (itm_canopen_persistent_config_t){10, 100, 0}, NULL));
    TEST_ASSERT(itm_canopen_ret_set_management(&service, uid, 7, 0x12345678UL));
    TEST_ASSERT(itm_n7space_init((itm_can_port_t){NULL, send_can, NULL}, &service));
    TEST_ASSERT_EQ(ITM_OK, itm_n7space_receive(&q));
    TEST_ASSERT_EQ(0x58a, last_tx.id); TEST_ASSERT_EQ(0x43, last_tx.data[0]);
    TEST_ASSERT_EQ(0xa9, last_tx.data[4]); TEST_ASSERT_EQ(6, last_tx.data[5]);
    q.data[0] = 0x40; q.data[1] = 0x0a; q.data[2] = 0x21; q.data[3] = 0;
    TEST_ASSERT_EQ(ITM_OK, itm_n7space_receive(&q));
    TEST_ASSERT_EQ(0x43, last_tx.data[0]);
    TEST_ASSERT_EQ(0x78, last_tx.data[4]); TEST_ASSERT_EQ(0x12, last_tx.data[7]);
    q.data[0] = 0x23; q.data[1] = 3; q.data[2] = 0x21; q.data[3] = 0;
    memcpy(q.data + 4, "UPDT", 4);
    itm_n7space_receive(&q); TEST_ASSERT_EQ(0x80, last_tx.data[0]);
    itm_n7space_allow_update(true); itm_n7space_receive(&q);
    TEST_ASSERT_EQ(0x60, last_tx.data[0]); TEST_ASSERT(itm_n7space_take_update_request());
    TEST_ASSERT(!itm_n7space_take_update_request());
    TEST_ASSERT_EQ(ITM_OK, itm_n7space_tick(100));
    TEST_ASSERT_EQ(0x70a, last_tx.id);
    TEST_ASSERT(itm_n7space_pool_used() < 32768);
    itm_n7space_fini();
    return true;
}
