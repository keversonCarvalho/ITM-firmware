#include "itm_update_platform.h"
#include "itm_board.h"
#include "itm_stm32_ports.h"
#include "itm/protocols/n7space.h"
#include "itm/protocols/ceb_tlm.h"
#include "itm/protocols/itm_rs422_tlm.h"
#include "itm/app/power_control.h"
#include "itm/app/cb_monitor.h"
#include "itm/app/cb_voltage.h"
#include "itm/services/diagnostics.h"
#include <string.h>

static itm_fw_update_t fw;
static itm_canopen_ret_t service;
static itm_telemetry_store_t telemetry;
static itm_dbcc_t battery_decoder;
static itm_ceb_tlm_t ceb_decoder;
static itm_telemetry_snapshot_t bc_snapshot;
static uint8_t bc_frame[ITM_RS422_TLM_FRAME_SIZE];
static uint16_t bc_frame_counter;
static uint32_t bc_last_frame_ms;
static itm_power_control_t power;
static itm_cb_monitor_t cb_monitor;
static itm_diagnostics_t power_diagnostics;
static uint8_t bench_request;
static uint32_t bench_last_command_ms;
static uint32_t cb_last_sample_ms;
static uint32_t em1_mv = UINT32_MAX, em2_mv = UINT32_MAX;
static uint16_t cb_actuation_limit_ms = 50U;
static bool cb_confirmed_on;
#define ITM_BENCH_COMMAND_LEASE_MS 1000U
#define ITM_BENCH_VREF_MV 3300U

static bool power_bench_read(void *context, uint16_t index, uint32_t *value)
{
    (void)context;
    if (!value) return false;
    switch (index) {
    case ITM_OD_BENCH_POWER_COMMAND: *value = bench_request; break;
    case ITM_OD_BENCH_POWER_STATE: *value = (uint32_t)power.state; break;
    case ITM_OD_BENCH_DEBOUNCE_MS: *value = cb_monitor.config.debounce_ms; break;
    case ITM_OD_BENCH_CEB_DELAY_MS: *value = power.config.ceb_start_timeout_ms; break;
    case ITM_OD_BENCH_CB_TIMEOUT_MS: *value = cb_actuation_limit_ms; break;
    case ITM_OD_BENCH_EM1_MV: *value = em1_mv; break;
    case ITM_OD_BENCH_EM2_MV: *value = em2_mv; break;
    case ITM_OD_BENCH_CB_CONDITION: *value = (uint32_t)cb_monitor.condition; break;
    default: return false;
    }
    return true;
}

static itm_sdo_abort_t power_bench_write(void *context, uint16_t index,
                                          uint32_t value)
{
    const uint32_t now = HAL_GetTick();
    (void)context;
    if (index == ITM_OD_BENCH_POWER_COMMAND) {
        if (value > ITM_POWER_REQUEST_SYSTEM_ON) return ITM_SDO_ABORT_VALUE_RANGE;
        if (value != 0U && power.state == ITM_POWER_FAULT)
            return ITM_SDO_ABORT_HARDWARE;
        bench_request = (uint8_t)value;
        bench_last_command_ms = now;
        if (value == 0U && power.state == ITM_POWER_FAULT) {
            const itm_power_inputs_t safe = {.commands_allowed = true};
            itm_diagnostics_clear_blocking_fault(&power_diagnostics);
            /* A fresh explicit command is required after clearing the fault. */
            (void)itm_power_control_clear_fault(&power, now, &safe);
        }
        itm_power_control_request(&power, (itm_power_request_t)value);
        return ITM_SDO_ABORT_NONE;
    }
    if (value < 1U || value > 5000U) return ITM_SDO_ABORT_VALUE_RANGE;
    if (bench_request != 0U || power.state != ITM_POWER_OFF)
        return ITM_SDO_ABORT_HARDWARE;
    switch (index) {
    case ITM_OD_BENCH_DEBOUNCE_MS:
        cb_monitor.config.debounce_ms = (uint16_t)value;
        power.config.cb_start_timeout_ms = value + cb_actuation_limit_ms;
        break;
    case ITM_OD_BENCH_CEB_DELAY_MS:
        power.config.ceb_start_timeout_ms = value; break;
    case ITM_OD_BENCH_CB_TIMEOUT_MS:
        cb_actuation_limit_ms = (uint16_t)value;
        power.config.cb_start_timeout_ms = value + cb_monitor.config.debounce_ms;
        break;
    default: return ITM_SDO_ABORT_OBJECT_NOT_FOUND;
    }
    return ITM_SDO_ABORT_NONE;
}

static void power_bench_poll(uint32_t now)
{
    itm_power_inputs_t inputs = {0};
    uint16_t raw1, raw2, voltage1, voltage2;
    if (bench_request != 0U &&
        (uint32_t)(now - bench_last_command_ms) >= ITM_BENCH_COMMAND_LEASE_MS) {
        bench_request = 0U;
        itm_power_control_request(&power, ITM_POWER_REQUEST_OFF);
    }
    if ((uint32_t)(now - cb_last_sample_ms) >= 5U) {
        cb_last_sample_ms = now;
        if (itm_update_cb_adc_read(&raw1, &raw2) &&
            itm_cb_voltage_from_adc(raw1, ITM_BENCH_VREF_MV, &voltage1) &&
            itm_cb_voltage_from_adc(raw2, ITM_BENCH_VREF_MV, &voltage2)) {
            em1_mv = voltage1;
            em2_mv = voltage2;
            (void)itm_cb_monitor_sample(&cb_monitor, voltage1, voltage2,
                                        power.cb_commanded, now);
            if (cb_monitor.has_stable_sample)
                cb_confirmed_on = power.cb_commanded &&
                    cb_monitor.condition == ITM_CB_CONDITION_NORMAL;
        } else {
            em1_mv = UINT32_MAX;
            em2_mv = UINT32_MAX;
            cb_monitor.condition = ITM_CB_CONDITION_INVALID;
            cb_monitor.has_stable_sample = false;
            cb_confirmed_on = false;
        }
    }
    inputs.commands_allowed = true; /* Bench SDO is the only command source. */
    inputs.blocking_fault = power_diagnostics.blocking_fault;
    /* No independent CEB power-good input is connected in this bench build.
     * This is only a startup delay, never a claim of voltage confirmation. */
    inputs.ceb_power_good = power.state == ITM_POWER_CEB_STARTING &&
        (uint32_t)(now - power.state_entered_ms) >=
        power.config.ceb_start_timeout_ms;
    inputs.cb_feedback_valid = cb_monitor.has_stable_sample ||
        (power.state == ITM_POWER_OPERATIONAL &&
         cb_monitor.condition == ITM_CB_CONDITION_PENDING);
    inputs.cb_is_on = cb_confirmed_on;
    itm_power_control_tick(&power, now, &inputs, &power_diagnostics);
}
static uint32_t telemetry_now(void *context)
{
    (void)context;
    return HAL_GetTick();
}
static bool read_ceb_diagnostic(void *context, uint8_t subindex,
                                uint32_t *value)
{
    itm_ceb_tlm_t *decoder = context;
    itm_signal_runtime_t supply;
    if (!decoder || !value) return false;
    switch (subindex) {
    case 1: *value = decoder->valid_frames; break;
    case 2: *value = decoder->crc_errors; break;
    case 3: *value = decoder->framing_errors; break;
    case 4: *value = decoder->counter_gaps; break;
    case 5: *value = itm_update_ceb_uart_errors(); break;
    case 6: *value = decoder->has_counter ? decoder->last_counter : UINT32_MAX; break;
    case 7:
        if (itm_telemetry_store_read(&telemetry, ITM_SIGNAL_CEB_SUPPLY_28V_VOLTAGE,
                                     HAL_GetTick(), &supply) != ITM_OK) return false;
        *value = supply.has_timestamp ? (uint32_t)supply.value : UINT32_MAX;
        break;
    default: return false;
    }
    return true;
}
static bool save_canopen_config(void *context,
                                const itm_canopen_persistent_config_t *config)
{
    itm_fw_update_t *state = context;
    return config != NULL && state->confirmed == ITM_APP_SLOT &&
        state->candidate == ITM_FW_NONE && itm_fw_save_configuration(
        state, config->node_id, config->heartbeat_producer_ms,
        config->configuration_version);
}
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
static uint32_t battery_decoded[2];
static void write_u32(uint8_t *data, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) data[i] = (uint8_t)(value >> (8U * i));
}
static void write_i64(uint8_t *data, int64_t value)
{
    unsigned i;
    for (i = 0; i < 8; ++i) data[i] = (uint8_t)((uint64_t)value >> (8U * i));
}
static void battery_bench_query(const itm_dbcc_frame_t *frame)
{
    uint8_t reply[8];
    itm_signal_runtime_t voltage, soc;
    const bool av = frame->bus == ITM_DBCC_BUS_BATT_28V;
    const itm_signal_id_t voltage_id = av ? ITM_SIGNAL_AV_BATT_VOLTAGE
                                       : ITM_SIGNAL_ACT_BATT_VOLTAGE;
    const itm_signal_id_t soc_id = av ? ITM_SIGNAL_AV_BATT_SOC
                                   : ITM_SIGNAL_ACT_BATT_SOC;
    if (memcmp(frame->data, "ITMS", 4) == 0) {
        const itm_signal_id_t id = (itm_signal_id_t)(frame->data[4] |
                                                     ((uint16_t)frame->data[5] << 8));
        itm_signal_runtime_t selected = {0};
        const bool permitted = itm_telemetry_key_is_valid(id) &&
            itm_telemetry_catalog[id].source == (av ? ITM_SOURCE_AV_BATT
                                                 : ITM_SOURCE_ACT_BATT);
        if (permitted)
            (void)itm_telemetry_store_read(&telemetry, id, frame->timestamp_ms,
                                            &selected);
        reply[0] = frame->data[4]; reply[1] = frame->data[5];
        reply[2] = frame->data[6]; reply[3] = frame->data[7];
        reply[4] = (uint8_t)selected.quality;
        reply[5] = permitted ? (uint8_t)itm_telemetry_catalog[id].decimal_exponent : 0;
        reply[6] = permitted ? (uint8_t)itm_telemetry_catalog[id].type : 0xff;
        reply[7] = permitted ? (uint8_t)itm_telemetry_catalog[id].source : 0xff;
        (void)itm_update_battery_can_send(frame->bus, 0x7a3, reply, 8);
        write_i64(reply, selected.value);
        (void)itm_update_battery_can_send(frame->bus, 0x7a4, reply, 8);
        return;
    }
    const bool voltage_valid = itm_telemetry_store_read(&telemetry, voltage_id,
                                    frame->timestamp_ms, &voltage) == ITM_OK &&
                               voltage.quality == ITM_SIGNAL_QUALITY_VALID;
    const bool soc_valid = itm_telemetry_store_read(&telemetry, soc_id,
                                    frame->timestamp_ms, &soc) == ITM_OK &&
                           soc.quality == ITM_SIGNAL_QUALITY_VALID;
    memcpy(reply, frame->data + 4, 4);
    write_u32(reply + 4, battery_decoded[frame->bus]);
    (void)itm_update_battery_can_send(frame->bus, 0x7a1, reply, 8);
    write_u32(reply, voltage_valid ? (uint32_t)voltage.value : 0xffffffffU);
    reply[4] = soc_valid ? (uint8_t)soc.value : 0xff;
    reply[5] = soc_valid ? (uint8_t)((uint32_t)soc.value >> 8) : 0xff;
    reply[6] = (uint8_t)(voltage_valid | (soc_valid << 1));
    reply[7] = (uint8_t)frame->bus;
    (void)itm_update_battery_can_send(frame->bus, 0x7a2, reply, 8);
}
#endif
static bool reset_pending, boot_confirmed;
static uint32_t reset_since;
static uint32_t image_version;

void itm_update_app_init(void)
{
    itm_canopen_identity_t identity = {0};
    itm_canopen_persistent_config_t config;
    const uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    if (!itm_update_can_init() || !itm_update_battery_can_init() ||
        !itm_update_cb_adc_init() ||
        !itm_telemetry_store_init(&telemetry, itm_stm32_critical_section_port()) ||
        !itm_ceb_tlm_init(&ceb_decoder, &telemetry) ||
        !itm_update_ceb_uart_init() ||
        !itm_update_bc_uart_init() ||
        !itm_dbcc_init(&battery_decoder, &telemetry) ||
        !itm_fw_init(&fw, itm_update_flash_port())) for (;;) {}
    itm_diagnostics_init(&power_diagnostics);
    if (!itm_cb_monitor_init(&cb_monitor,
            &(const itm_cb_monitor_config_t){5600U, 6700U, 50U}) ||
        !itm_power_control_init(&power,
            &(const itm_power_config_t){50U, 100U, 50U},
            itm_stm32_mfet2_port(), itm_stm32_mfet1_port(), HAL_GetTick()))
        for (;;) {}
    config = (itm_canopen_persistent_config_t){
        fw.node_id, fw.heartbeat_producer_ms, fw.configuration_version};
    image_version = *(const uint32_t *)(uintptr_t)(itm_fw_slot_address(ITM_APP_SLOT) + 20);
    identity.serial_number = itm_fw_crc32(uid, sizeof(uid));
    identity.revision_number = itm_canopen_pack_revision(1, 2, 0);
    if (!itm_canopen_ret_init(&service, identity, config, NULL) ||
        !itm_canopen_ret_set_management(&service, (const uint8_t *)uid,
            image_version, *(const uint32_t *)(uintptr_t)
                (itm_fw_slot_address(ITM_APP_SLOT) + 24)) ||
        !itm_canopen_ret_bind_config_store(&service, save_canopen_config, &fw) ||
        !itm_canopen_ret_bind_telemetry(&service,
            itm_telemetry_store_snapshot_port(&telemetry)) ||
        !itm_canopen_ret_bind_telemetry_clock(&service, telemetry_now, NULL) ||
        !itm_canopen_ret_bind_ceb_diagnostics(&service, read_ceb_diagnostic,
                                                &ceb_decoder) ||
        !itm_canopen_ret_bind_power_bench(&service, power_bench_read,
                                           power_bench_write, NULL) ||
        !itm_n7space_init(itm_update_can_port(), &service)) for (;;) {}
    itm_n7space_allow_update(true);
    bc_last_frame_ms = HAL_GetTick();
    boot_confirmed = fw.confirmed == ITM_APP_SLOT;
    itm_n7space_set_boot_info(ITM_APP_SLOT, image_version, boot_confirmed);
}
void itm_update_app_poll(void)
{
    itm_can_frame_t frame;
    itm_can_port_t can = itm_update_can_port();
    uint32_t now = HAL_GetTick();
    unsigned budget;
    itm_dbcc_bus_t bus;
    for (budget = 0; budget < 16 && can.receive(can.context, &frame) == ITM_OK; ++budget)
        (void)itm_n7space_receive(&frame);
    power_bench_poll(now);
    for (bus = ITM_DBCC_BUS_BATT_28V; bus <= ITM_DBCC_BUS_BATT_150V; ++bus) {
        itm_dbcc_frame_t battery;
        for (budget = 0; budget < 16 &&
             itm_update_battery_can_receive(bus, &battery) == ITM_OK; ++budget) {
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
            if (!battery.is_extended && battery.id == 0x7a0 &&
                battery.length == 8 &&
                (memcmp(battery.data, "ITM?", 4) == 0 ||
                 memcmp(battery.data, "ITMS", 4) == 0)) {
                battery_bench_query(&battery);
                continue;
            }
#endif
            if (itm_dbcc_ingest(&battery_decoder, &battery) == ITM_OK) {
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
                ++battery_decoded[bus];
#endif
            }
        }
    }
    {
        uint8_t byte;
        for (budget = 0; budget < 256 && itm_update_ceb_uart_read(&byte); ++budget)
            itm_ceb_tlm_ingest_byte(&ceb_decoder, byte, now);
    }
    if ((uint32_t)(now - bc_last_frame_ms) >= ITM_RS422_TLM_PERIOD_MS) {
        bc_last_frame_ms = now;
        if (itm_update_bc_uart_idle() &&
            itm_telemetry_store_snapshot(&telemetry, now, &bc_snapshot) == ITM_OK &&
            itm_rs422_tlm_encode(&bc_snapshot, bc_frame_counter,
                                 bc_frame, sizeof(bc_frame)) == ITM_OK &&
            itm_update_bc_uart_send(bc_frame, sizeof(bc_frame)))
            ++bc_frame_counter;
    }
    if (itm_n7space_tick(now) != ITM_OK) for (;;) {} /* Watchdog recovery. */
    if (itm_n7space_take_update_request()) {
        itm_board_force_safe_outputs(); reset_pending = true; reset_since = now;
    }
    if (reset_pending && (itm_update_can_tx_idle() || now - reset_since > 1000)) NVIC_SystemReset();
    /* Bench acceptance: initialized CANopen and sustained event loop for 2 s.
     * Replace with mission health criteria before flight. */
    if (!boot_confirmed && now >= 2000) {
        if (!itm_fw_confirm(&fw, ITM_APP_SLOT)) for (;;) {}
        boot_confirmed = true;
        itm_n7space_set_boot_info(ITM_APP_SLOT, image_version, true);
    }
    itm_update_watchdog_feed();
}
