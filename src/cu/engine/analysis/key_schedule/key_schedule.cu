// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "key_schedule.h"
#include "key_schedule_core.h"

#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

#define KEY_SCHEDULE_CHECK(condition_, evacaddr_, error_, type_)                                                       \
    engine_error_check((condition_), (type_), ENGINE_MODULE_KEY_SCHEDULE, (unsigned int)__LINE__,                      \
                       (const void *)(evacaddr_), (error_))

// the owner that the cells of the key schedule count to
#define KEY_SCHEDULE_PLACE ((unsigned int)platform_place_g_engine_owner)

static const unsigned int KEY_SCHEDULE_SWEEP_AXES[ENGINE_AXES] = {0u, 2u, 1u};

static unsigned long long key_schedule_count_growth_bits(unsigned long long count)
{
    unsigned long long less = count - 1ULL;
    unsigned long long bits = 0ULL;
    while (less != 0ULL)
    {
        bits += 1ULL;
        less >>= 1u;
    }
    return bits;
}

extern "C" long key_schedule_layout(const EngineKey *key, EngineKeyLayout *layout, EngineError *error)
{
    if (error == NULL)
    {
        return KEY_SCHEDULE_ERROR;
    }
    if (!KEY_SCHEDULE_CHECK((key != NULL) && (layout != NULL), key, error, ENGINE_ERROR_REQUEST) ||
        !KEY_SCHEDULE_CHECK((key->terms != 0u) && (key->term != NULL) && (key->limbs != NULL), key, error,
                            ENGINE_ERROR_REQUEST))
    {
        return KEY_SCHEDULE_ERROR;
    }
    mmgr_bytes_set(layout, 0u, sizeof(*layout));
    const unsigned int count = key->terms;
    // the table of terms is a resource of the layout, and the weights gather in ephemeral work before they join it
    DeviceTerm *const terms =
        (DeviceTerm *)platform_cell_taken((unsigned long long)count * sizeof(DeviceTerm), KEY_SCHEDULE_PLACE, 0);
    if (!KEY_SCHEDULE_CHECK(terms != NULL, layout, error, ENGINE_ERROR_RESOURCE))
    {
        return KEY_SCHEDULE_ERROR;
    }
    mmgr_bytes_set(terms, 0u, (size_t)count * sizeof(DeviceTerm));
    layout->term_table = terms;
    PlatformArray weights;
    platform_array_begun(&weights, sizeof(unsigned int), KEY_SCHEDULE_PLACE, 1);
    unsigned long long widest = 0ULL;
    unsigned int columns = 0u;
    unsigned int planes = 0u;
    unsigned long long range[3] = {0ULL, 0ULL, 0ULL};
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const EngineKeyTerm *const term = &key->term[index];
        DeviceTerm *const device = &terms[index];
        device->negative = term->negative;
        unsigned long long bits = 16ULL;
        unsigned int in_limbs = 1u;
        unsigned int in_plane = ENGINE_FROM_ATOM;
        for (unsigned int sweep = 0u; sweep < ENGINE_AXES; sweep += 1u)
        {
            const unsigned int axis = KEY_SCHEDULE_SWEEP_AXES[sweep];
            const EngineKeyRow *const row = &term->row[axis];
            bits += row->growth_bits;
            if (!KEY_SCHEDULE_CHECK((row->taps * row->limbs) < (1ULL << 30u), row, error, ENGINE_ERROR_REQUEST))
            {
                platform_array_ended(&weights);
                key_schedule_release(layout);
                return KEY_SCHEDULE_ERROR;
            }
            DeviceSweep *const out = &device->sweep[sweep];
            out->axis = axis;
            out->taps = (unsigned int)row->taps;
            out->row_limbs = (unsigned int)row->limbs;
            out->in_limbs = in_limbs;
            out->out_limbs = (unsigned int)((bits + 31ULL) / 32ULL);
            out->in_plane = in_plane;
            out->out_plane = ENGINE_FROM_ATOM;
            out->weights = weights.count;
            const unsigned long long first = weights.count;
            const unsigned long long added = row->limbs * row->taps;
            if (!KEY_SCHEDULE_CHECK(platform_array_counted(&weights, first + added), layout, error,
                                    ENGINE_ERROR_RESOURCE))
            {
                platform_array_ended(&weights);
                key_schedule_release(layout);
                return KEY_SCHEDULE_ERROR;
            }
            mmgr_bytes_copy(platform_array_at(&weights, first), &key->limbs[row->first],
                            (size_t)(added * sizeof(unsigned int)));
            const unsigned int used = out->row_limbs + out->in_limbs + 2u;
            columns = (used > columns) ? used : columns;
            const unsigned long long half = (unsigned long long)(out->taps / 2u);
            range[out->axis] = (half > range[out->axis]) ? half : range[out->axis];
            if (sweep < (ENGINE_AXES - 1u))
            {
                out->out_plane = planes;
                planes += out->out_limbs;
            }
            in_limbs = out->out_limbs;
            in_plane = out->out_plane;
        }
        if (!KEY_SCHEDULE_CHECK(term->shift <= 0x7FFFFFFFULL, &term->shift, error, ENGINE_ERROR_REQUEST))
        {
            platform_array_ended(&weights);
            key_schedule_release(layout);
            return KEY_SCHEDULE_ERROR;
        }
        device->shift = (unsigned int)term->shift;
        widest = ((bits + term->shift) > widest) ? (bits + term->shift) : widest;
    }
    const unsigned long long lane_bits = widest + key_schedule_count_growth_bits((unsigned long long)count) + 1ULL;
    layout->weights =
        (unsigned int *)platform_cell_taken((weights.count + 1ULL) * sizeof(unsigned int), KEY_SCHEDULE_PLACE, 0);
    if (!KEY_SCHEDULE_CHECK(lane_bits <= 0x7FFFFFFFULL, key, error, ENGINE_ERROR_REQUEST) ||
        !KEY_SCHEDULE_CHECK(layout->weights != NULL, layout, error, ENGINE_ERROR_RESOURCE))
    {
        platform_array_ended(&weights);
        key_schedule_release(layout);
        return KEY_SCHEDULE_ERROR;
    }
    if (weights.count != 0ULL)
    {
        mmgr_bytes_copy(layout->weights, weights.cell, (size_t)(weights.count * sizeof(unsigned int)));
    }
    layout->weight_count = weights.count;
    platform_array_ended(&weights);
    layout->terms = count;
    layout->bits = (unsigned int)lane_bits;
    layout->columns = columns;
    layout->planes = planes;
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        layout->range[axis] = range[axis];
    }
    return (long)lane_bits;
}

extern "C" void key_schedule_release(EngineKeyLayout *layout)
{
    platform_cell_released(layout->term_table, KEY_SCHEDULE_PLACE);
    platform_cell_released(layout->weights, KEY_SCHEDULE_PLACE);
    mmgr_bytes_set(layout, 0u, sizeof(*layout));
}

// one ephemeral cell of `count` elements of `bytes` each, all zero, or NULL where the pool cannot meet it
static void *key_schedule_work(unsigned long long count, unsigned long long bytes)
{
    void *const work = platform_cell_taken(count * bytes, KEY_SCHEDULE_PLACE, 1);
    if (work != NULL)
    {
        mmgr_bytes_set(work, 0u, (size_t)(count * bytes));
    }
    return work;
}

// The record layout. Its core is key_schedule_core_record_layout of key_schedule_core.h, which the device runs as well.
// The core arranges each step for the device, places it, and places each output in the record. The layout then comes
// from them and from the tables of the key
extern "C" long key_schedule_record_layout(const KeyScheduleRecordRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return KEY_SCHEDULE_ERROR;
    }
    EngineError *const error = request->error;
    if (!KEY_SCHEDULE_CHECK((request->key != NULL) && (request->layout != NULL) && (request->in_limbs != NULL), request,
                            error, ENGINE_ERROR_REQUEST) ||
        !KEY_SCHEDULE_CHECK((request->key->steps != 0u) && (request->key->term != NULL) &&
                                (request->key->output != NULL) && (request->key->members != 0u) &&
                                (request->key->members <= ENGINE_RECORD_MEMBERS_MAX),
                            request->key, error, ENGINE_ERROR_REQUEST))
    {
        return KEY_SCHEDULE_ERROR;
    }
    for (unsigned int member = 0u; member < request->key->members; member += 1u)
    {
        if (!KEY_SCHEDULE_CHECK(request->in_limbs[member] != 0u, &request->in_limbs[member], error,
                                ENGINE_ERROR_REQUEST))
        {
            return KEY_SCHEDULE_ERROR;
        }
    }
    const EngineRecordKey *const key = request->key;
    EngineRecordLayout *const layout = request->layout;
    mmgr_bytes_set(layout, 0u, sizeof(*layout));
    const unsigned long long count = key->steps;
    // the table of steps is a resource of the layout, and the rest is the ephemeral work of the core
    DeviceRecordStep *const steps =
        (DeviceRecordStep *)platform_cell_taken(count * sizeof(DeviceRecordStep), KEY_SCHEDULE_PLACE, 0);
    unsigned int *const last_use = (unsigned int *)key_schedule_work(count, sizeof(unsigned int));
    unsigned int *const ending_first = (unsigned int *)key_schedule_work(count + 1ULL, sizeof(unsigned int));
    unsigned int *const ending = (unsigned int *)key_schedule_work(count, sizeof(unsigned int));
    KeyScheduleBlock *const freed = (KeyScheduleBlock *)key_schedule_work(count + 1ULL, sizeof(KeyScheduleBlock));
    unsigned long long *const table_offset =
        (unsigned long long *)key_schedule_work((unsigned long long)key->tables + 1ULL, sizeof(unsigned long long));
    int ok = KEY_SCHEDULE_CHECK((steps != NULL) && (last_use != NULL) && (ending_first != NULL) && (ending != NULL) &&
                                    (freed != NULL) && (table_offset != NULL),
                                layout, error, ENGINE_ERROR_RESOURCE);
    if (ok)
    {
        mmgr_bytes_set(steps, 0u, (size_t)(count * sizeof(DeviceRecordStep)));
    }
    KeyScheduleCoreLayout core_layout{};
    core_layout.terms = key->term;
    core_layout.step_count = key->steps;
    core_layout.outputs = key->output;
    core_layout.output_count = key->outputs;
    core_layout.tables = key->table;
    core_layout.table_count = key->tables;
    core_layout.field_offset = request->field_offset;
    core_layout.fields = request->fields;
    core_layout.in_limbs = request->in_limbs;
    core_layout.reuse = request->reuse;
    core_layout.steps = steps;
    core_layout.last_use = last_use;
    core_layout.ending_first = ending_first;
    core_layout.ending = ending;
    core_layout.freed = freed;
    core_layout.table_offset = table_offset;
    if (ok && (key_schedule_core_record_layout(&core_layout) == 0))
    {
        // the step, the key or the outputs the layout ended at
        const void *const evacaddr = (core_layout.end == KEY_SCHEDULE_CORE_STEP)
                                         ? (const void *)&key->term[core_layout.at]
                                     : (core_layout.end == KEY_SCHEDULE_CORE_FILE) ? (const void *)key
                                                                                   : (const void *)key->output;
        ok = KEY_SCHEDULE_CHECK(0, evacaddr, error, ENGINE_ERROR_REQUEST);
    }
    const unsigned long long table_words = (ok != 0) ? table_offset[key->tables] : 0ULL;
    platform_cell_released(table_offset, KEY_SCHEDULE_PLACE);
    platform_cell_released(freed, KEY_SCHEDULE_PLACE);
    platform_cell_released(ending, KEY_SCHEDULE_PLACE);
    platform_cell_released(ending_first, KEY_SCHEDULE_PLACE);
    platform_cell_released(last_use, KEY_SCHEDULE_PLACE);
    layout->step_table = steps;
    if (!ok)
    {
        key_schedule_record_release(layout);
        return KEY_SCHEDULE_ERROR;
    }
    if (table_words != 0ULL)
    {
        layout->table_values = (unsigned int *)platform_cell_taken(
            (key->table_word_count + 1ULL) * sizeof(unsigned int), KEY_SCHEDULE_PLACE, 0);
        if (!KEY_SCHEDULE_CHECK(layout->table_values != NULL, layout, error, ENGINE_ERROR_RESOURCE))
        {
            key_schedule_record_release(layout);
            return KEY_SCHEDULE_ERROR;
        }
        mmgr_bytes_copy(layout->table_values, key->table_values, (size_t)key->table_word_count * sizeof(unsigned int));
        layout->table_word_count = key->table_word_count;
    }
    layout->steps = key->steps;
    layout->members = key->members;
    // the file is at max ENGINE_RECORD_LIMBS_MAX limbs and the record's bits a 31-bit count, as the layout stores them
    layout->file_limbs = (unsigned int)core_layout.file_limbs;
    for (unsigned int member = 0u; member < key->members; member += 1u)
    {
        layout->in_limbs[member] = request->in_limbs[member];
    }
    layout->out_bits = (unsigned int)core_layout.out_bits;
    layout->out_limbs = (unsigned int)((core_layout.out_bits + 31ULL) / 32ULL);
    return (long)layout->file_limbs;
}

extern "C" void key_schedule_record_release(EngineRecordLayout *layout)
{
    platform_cell_released(layout->step_table, KEY_SCHEDULE_PLACE);
    platform_cell_released(layout->table_values, KEY_SCHEDULE_PLACE);
    mmgr_bytes_set(layout, 0u, sizeof(*layout));
}
