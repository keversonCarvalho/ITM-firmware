#include "test_framework.h"
#include "mocks/mock_flash.h"
#include "mocks/mock_critical.h"
#include "itm/services/canopen_ret.h"

#include <string.h>

static itm_canopen_ret_t create_service(mock_flash_t *flash,
                                        itm_persistence_t *persistence)
{
    itm_canopen_ret_t service;
    const itm_canopen_identity_t identity = {
        0x12345678UL, itm_canopen_pack_revision(1U, 2U, 0U), 0xABCDEF01UL};
    const itm_canopen_persistent_config_t defaults = {7U, 1000U, 0U};

    mock_flash_init(flash);
    (void)itm_persistence_init(persistence, mock_flash_port(flash), 1U);
    (void)itm_canopen_ret_init(&service, identity, defaults, persistence);
    return service;
}

bool test_canopen_identity_uses_registered_vendor_id(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    itm_canopen_ret_t service = create_service(&flash, &persistence);
    uint32_t value = 0U;
    size_t size = 0U;

    TEST_ASSERT_EQ(ITM_SDO_ABORT_NONE,
                   itm_canopen_ret_sdo_read(&service, ITM_OD_IDENTITY, 1U,
                                            (uint8_t *)&value, sizeof(value),
                                            &size));
    TEST_ASSERT_EQ(sizeof(value), size);
    TEST_ASSERT_EQ(ITM_CANOPEN_VENDOR_ID, value);
    return true;
}

bool test_canopen_rejects_identity_write(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    itm_canopen_ret_t service = create_service(&flash, &persistence);
    const uint32_t value = 1U;

    TEST_ASSERT_EQ(ITM_SDO_ABORT_READ_ONLY,
                   itm_canopen_ret_sdo_write(&service, ITM_OD_IDENTITY, 1U,
                                             (const uint8_t *)&value,
                                             sizeof(value)));
    return true;
}

bool test_canopen_persists_node_id_on_save_command(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    itm_canopen_ret_t service = create_service(&flash, &persistence);
    itm_canopen_ret_t restored;
    const itm_canopen_identity_t identity = {0U, 0U, 0U};
    const itm_canopen_persistent_config_t defaults = {1U, 0U, 0U};
    const uint8_t node_id = 42U;
    const uint32_t save = 0x65766173UL;

    TEST_ASSERT_EQ(ITM_SDO_ABORT_NONE,
                   itm_canopen_ret_sdo_write(&service, ITM_OD_NODE_ID, 0U,
                                             &node_id, sizeof(node_id)));
    TEST_ASSERT(service.configuration_dirty);
    TEST_ASSERT_EQ(ITM_SDO_ABORT_NONE,
                   itm_canopen_ret_sdo_write(&service,
                                             ITM_OD_STORE_PARAMETERS, 1U,
                                             (const uint8_t *)&save,
                                             sizeof(save)));
    TEST_ASSERT_EQ(1U, service.config.configuration_version);
    TEST_ASSERT(!service.configuration_dirty);
    TEST_ASSERT(itm_canopen_ret_init(&restored, identity, defaults,
                                     &persistence));
    TEST_ASSERT_EQ(node_id, restored.config.node_id);
    TEST_ASSERT_EQ(1U, restored.config.configuration_version);
    return true;
}

bool test_canopen_validates_node_id_range(void)
{
    mock_flash_t flash;
    itm_persistence_t persistence;
    itm_canopen_ret_t service = create_service(&flash, &persistence);
    const uint8_t invalid_node_id = 0U;

    TEST_ASSERT_EQ(ITM_SDO_ABORT_VALUE_RANGE,
                   itm_canopen_ret_sdo_write(&service, ITM_OD_NODE_ID, 0U,
                                             &invalid_node_id,
                                             sizeof(invalid_node_id)));
    return true;
}

bool test_canopen_captures_telemetry_through_snapshot_port(void)
{
    mock_flash_t flash;
    mock_critical_t critical;
    itm_persistence_t persistence;
    itm_canopen_ret_t service = create_service(&flash, &persistence);
    itm_telemetry_store_t telemetry;
    itm_telemetry_snapshot_t snapshot;

    mock_critical_init(&critical);
    TEST_ASSERT(itm_telemetry_store_init(&telemetry,
                                         mock_critical_port(&critical)));
    TEST_ASSERT_EQ(ITM_ERROR_NOT_READY,
                   itm_canopen_ret_capture_telemetry(&service, 10U,
                                                     &snapshot));
    TEST_ASSERT(itm_canopen_ret_bind_telemetry(
        &service, itm_telemetry_store_snapshot_port(&telemetry)));
    TEST_ASSERT_EQ(ITM_OK, itm_telemetry_store_update(
                               &telemetry, ITM_SOURCE_CEB,
                               ITM_SIGNAL_CEB_TEMPERATURE,
                               ITM_SIGNAL_TYPE_I32, 2500, 10U));
    TEST_ASSERT_EQ(ITM_OK,
                   itm_canopen_ret_capture_telemetry(&service, 10U,
                                                     &snapshot));
    TEST_ASSERT_EQ(2500,
                   snapshot.values[ITM_SIGNAL_CEB_TEMPERATURE].value);
    return true;
}
