// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef KEY_SCHEDULE_H
#define KEY_SCHEDULE_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define KEY_SCHEDULE_ERROR (-1L)

    long key_schedule_layout(const EngineKey *key, EngineKeyLayout *layout, EngineError *error);

    void key_schedule_release(EngineKeyLayout *layout);

    typedef struct
    {
        const EngineRecordKey *key;
        const unsigned int *field_offset;
        unsigned int fields;
        const unsigned int *in_limbs;
        // Where `reuse` is not zero, the schedule frees a register after no later step reads it. Thus a long chain runs
        // within the ENGINE_RECORD_LIMBS_MAX limbs of the file. Else, every step uses its own register for the
        // whole run, the layout that the measures of the proven programs used.
        int reuse;
        EngineRecordLayout *layout;
        EngineError *error;
    } KeyScheduleRecordRequest;

    long key_schedule_record_layout(const KeyScheduleRecordRequest *request);

    void key_schedule_record_release(EngineRecordLayout *layout);

#ifdef __cplusplus
}
#endif

#endif
