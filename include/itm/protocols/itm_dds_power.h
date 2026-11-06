#ifndef ITM_PROTOCOLS_ITM_DDS_POWER_H
#define ITM_PROTOCOLS_ITM_DDS_POWER_H

#include "itm/core/result.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITM_DDS_POWER_COMMAND_CDR_SIZE 14U
#define ITM_DDS_POWER_STATUS_CDR_SIZE 32U

typedef struct {
    uint64_t session_id;
    uint32_t command_id;
    bool ceb_on;
    bool cb_on;
} itm_dds_power_command_t;

typedef struct {
    uint64_t session_id;
    uint32_t command_id;
    uint8_t result;
    uint8_t power_state;
    bool ceb_requested;
    bool cb_requested;
    bool ceb_output_on;
    bool cb_output_on;
    uint8_t cb_condition;
    bool em1_valid;
    bool em2_valid;
    uint32_t em1_mv;
    uint32_t em2_mv;
} itm_dds_power_status_t;

itm_result_t itm_dds_power_command_deserialize(const uint8_t *data,
                                                size_t length,
                                                itm_dds_power_command_t *out);
itm_result_t itm_dds_power_status_serialize(const itm_dds_power_status_t *status,
                                             uint8_t *data, size_t capacity,
                                             size_t *length);

#endif
