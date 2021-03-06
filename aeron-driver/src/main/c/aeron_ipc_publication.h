/*
 * Copyright 2014 - 2017 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AERON_AERON_IPC_PUBLICATION_H
#define AERON_AERON_IPC_PUBLICATION_H

#include "aeron_driver_common.h"
#include "util/aeron_bitutil.h"
#include "aeron_driver_context.h"
#include "util/aeron_fileutil.h"
#include "concurrent/aeron_counters_manager.h"
#include "aeron_system_counters.h"

typedef enum aeron_ipc_publication_status_enum
{
    AERON_IPC_PUBLICATION_STATUS_ACTIVE,
    AERON_IPC_PUBLICATION_STATUS_INACTIVE,
    AERON_IPC_PUBLICATION_STATUS_LINGER
}
aeron_ipc_publication_status_t;

typedef struct aeron_ipc_publication_stct
{
    struct aeron_ipc_publication_conductor_fields_stct
    {
        aeron_driver_managed_resource_t managed_resource;
        aeron_subscribeable_t subscribeable;
        int64_t cleaning_position;
        int64_t trip_limit;
        int64_t consumer_position;
        int64_t last_consumer_position;
        int64_t time_of_last_consumer_position_change;
        int32_t refcnt;
        bool has_reached_end_of_life;
        aeron_ipc_publication_status_t status;
    }
    conductor_fields;

    /* uint8_t conductor_fields_pad[(2 * AERON_CACHE_LINE_LENGTH) - sizeof(struct conductor_fields_stct)]; */

    aeron_mapped_raw_log_t mapped_raw_log;
    aeron_position_t pub_lmt_position;
    aeron_logbuffer_metadata_t *log_meta_data;
    aeron_clock_func_t nano_clock;

    char *log_file_name;
    int64_t term_window_length;
    int64_t trip_gain;
    int64_t linger_timeout_ns;
    int64_t unblock_timeout_ns;
    int32_t session_id;
    int32_t stream_id;
    int32_t initial_term_id;
    size_t log_file_name_length;
    size_t position_bits_to_shift;
    bool is_exclusive;
    aeron_map_raw_log_close_func_t map_raw_log_close_func;

    int64_t *unblocked_publications_counter;
}
aeron_ipc_publication_t;

int aeron_ipc_publication_create(
    aeron_ipc_publication_t **publication,
    aeron_driver_context_t *context,
    int32_t session_id,
    int32_t stream_id,
    int64_t registration_id,
    aeron_position_t *pub_lmt_position,
    int32_t initial_term_id,
    size_t term_buffer_length,
    size_t mtu_length,
    bool is_exclusive,
    aeron_system_counters_t *system_counters);

void aeron_ipc_publication_close(aeron_counters_manager_t *counters_manager, aeron_ipc_publication_t *publication);

int aeron_ipc_publication_update_pub_lmt(aeron_ipc_publication_t *publication);

void aeron_ipc_publication_clean_buffer(aeron_ipc_publication_t *publication, int64_t min_sub_pos);

void aeron_ipc_publication_on_time_event(aeron_ipc_publication_t *publication, int64_t now_ns, int64_t now_ms);

void aeron_ipc_publication_incref(void *clientd);
void aeron_ipc_publication_decref(void *clientd);

void aeron_ipc_publication_check_for_blocked_publisher(aeron_ipc_publication_t *publication, int64_t now_ns);

inline void aeron_ipc_publication_remove_subscriber_hook(void *clientd, int64_t *value_addr)
{
    aeron_ipc_publication_t *publication = (aeron_ipc_publication_t *)clientd;
    int64_t position = aeron_counter_get_volatile(value_addr);

    publication->conductor_fields.consumer_position =
        position > publication->conductor_fields.consumer_position ?
            position : publication->conductor_fields.consumer_position;
}

inline int64_t aeron_ipc_publication_producer_position(aeron_ipc_publication_t *publication)
{
    int64_t raw_tail;

    AERON_LOGBUFFER_RAWTAIL_VOLATILE(raw_tail, publication->log_meta_data);

    return aeron_logbuffer_compute_position(
        aeron_logbuffer_term_id(raw_tail),
        aeron_logbuffer_term_offset(raw_tail, (int32_t)publication->mapped_raw_log.term_length),
        publication->position_bits_to_shift,
        publication->initial_term_id);
}

inline int64_t aeron_ipc_publication_joining_position(aeron_ipc_publication_t *publication)
{
    return publication->conductor_fields.consumer_position;
}

inline bool aeron_ipc_publication_has_reached_end_of_life(aeron_ipc_publication_t *publication)
{
    return publication->conductor_fields.has_reached_end_of_life;
}

inline bool aeron_ipc_publication_is_drained(aeron_ipc_publication_t *publication)
{
    int64_t producer_position = aeron_ipc_publication_producer_position(publication);

    for (size_t i = 0, length = publication->conductor_fields.subscribeable.length; i < length; i++)
    {
        int64_t sub_pos = aeron_counter_get_volatile(publication->conductor_fields.subscribeable.array[i].value_addr);

        if (sub_pos < producer_position)
        {
            return false;
        }
    }

    return true;
}

inline size_t aeron_ipc_publication_num_subscribers(aeron_ipc_publication_t *publication)
{
    return publication->conductor_fields.subscribeable.length;
}

#endif //AERON_AERON_IPC_PUBLICATION_H
