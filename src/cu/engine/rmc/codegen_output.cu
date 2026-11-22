// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// codegen_output.cu: the assembly text written on the device, the step layout, and the entry points
#include "codegen_device_internal.h"
#include "memoria_operor/memoria_linea.h"

#include "../../../platform/platform.h"

#if (defined(__CUDACC__))

// The lane written on the device, in `device_arena`, which the caller frees, from the step table at `device_steps`, in
// device memory, and the rest of `layout`'s sizes: its forms decided (codegen_device_decide), split by `costs` where it
// is given, arranged as the assembly printer's records, written a lane a byte and gathered. The records and the lanes
// come back to the host after they are arranged, and the text's length after its bytes are gathered
static int codegen_device_written(DeviceArena *device_arena, const EngineRecordLayout *layout,
                                  const DeviceRecordStep *device_steps, const AsmPrinterRuleset *text_rules,
                                  unsigned int places, const ScheduleCosts *costs, PlatformText *text,
                                  VerbaLinea *error_message)
{
    DeviceArena &memory = *device_arena;
    MachineInstr *items = NULL;
    unsigned long long item_count = 0ULL;
    ScheduleReport report{};
    if (codegen_device_decide(device_arena, layout, device_steps, text_rules->scratch.words, text_rules->scratch.count,
                              places, costs, &report, &items, &item_count, error_message) == 0)
    {
        return 0;
    }
    // a form given other arguments than it takes breaks the lane, counted here from none
    unsigned int summary[CODEGEN_SUMMARY] = {0u};
    unsigned int *const device_summary = codegen_device_copy(&memory, summary, CODEGEN_SUMMARY);
    // the ruleset's written forms with their lists in device memory, and the records each item takes
    AsmPrinterLists forms = asm_printer_lists(text_rules);
    forms.word_lengths = codegen_device_copy(&memory, text_rules->word_lengths.words, text_rules->word_lengths.count);
    forms.part_lanes = codegen_device_copy(&memory, text_rules->part_lanes.words, text_rules->part_lanes.count);
    forms.form_part_first =
        codegen_device_copy(&memory, text_rules->form_part_first.words, text_rules->form_part_first.count);
    forms.form_parts = codegen_device_copy(&memory, text_rules->form_parts.words, text_rules->form_parts.count);
    forms.form_placeholder_first = codegen_device_copy(&memory, text_rules->form_placeholder_first.words,
                                                       text_rules->form_placeholder_first.count);
    forms.placeholder_parameters = codegen_device_copy(&memory, text_rules->placeholder_parameters.words,
                                                       text_rules->placeholder_parameters.count);
    unsigned long long *const record_counts = codegen_device_take<unsigned long long>(&memory, item_count);
    unsigned long long *const record_first = codegen_device_take<unsigned long long>(&memory, item_count);
    if (memory.ok != 0)
    {
        codegen_device_record_counts<<<codegen_device_blocks(item_count), CODEGEN_THREADS>>>(
            forms, items, item_count, record_counts, device_summary);
        codegen_device_launched(&memory);
    }
    const unsigned long long record_count = codegen_device_scan(&memory, record_counts, record_first, item_count);
    codegen_device_read(&memory, summary, device_summary, CODEGEN_SUMMARY);
    if ((memory.ok != 0) && ((summary[CODEGEN_BROKEN] != 0u) || (record_count >= ASM_PRINTER_LANES_MAX)))
    {
        mmgr_line_put(error_message, (summary[CODEGEN_BROKEN] != 0u)
                                         ? "a form breaks the lane"
                                         : "the text contains more records than the assembly printer supports");
        return 0;
    }
    unsigned int *const records = codegen_device_take<unsigned int>(&memory, record_count * ASM_PRINTER_RECORD_LIMBS);
    unsigned long long *const record_lanes = codegen_device_take<unsigned long long>(&memory, record_count);
    unsigned long long *const lane_first = codegen_device_take<unsigned long long>(&memory, record_count);
    const size_t record_bytes = sizeof(unsigned int) * ASM_PRINTER_RECORD_LIMBS * (size_t)record_count;
    memory.ok = (memory.ok != 0) && (cudaMemset(records, 0, record_bytes) == cudaSuccess);
    if (memory.ok != 0)
    {
        codegen_device_records<<<codegen_device_blocks(item_count), CODEGEN_THREADS>>>(
            forms, items, item_count, record_first, records, record_lanes);
        codegen_device_launched(&memory);
    }
    const unsigned long long lanes = codegen_device_scan(&memory, record_lanes, lane_first, record_count);
    if ((memory.ok != 0) && ((lanes >= ASM_PRINTER_LANES_MAX) || (lanes == 0ULL)))
    {
        mmgr_line_put(error_message, "the text contains more lanes than the assembly printer supports");
        return 0;
    }
    unsigned int *const index = codegen_device_take<unsigned int>(&memory, lanes);
    if (memory.ok != 0)
    {
        codegen_device_index<<<codegen_device_blocks(record_count), CODEGEN_THREADS>>>(records, record_count,
                                                                                       lane_first, record_lanes, index);
        codegen_device_launched(&memory);
    }
    if (memory.ok == 0)
    {
        mmgr_line_put(error_message, "the device errored on a call");
        return 0;
    }
    // the assembly printer arranged by the device and checked against the host's word for word, then run by the record
    // machine, a lane a byte. Its output's offset read before the layout, which the record stores its own of, is given
    // back
    EngineRecordLayout text_layout{};
    if (asm_printer_program_placed(text_rules, &text_layout, error_message) == 0)
    {
        return 0;
    }
    const unsigned int output_offset = text_layout.step_table[text_rules->program.output].out_offset;
    CycleRecord *record = NULL;
    EngineError error{};
    const long loaded = cycle_record_load(&text_layout, &record, &error);
    key_schedule_record_release(&text_layout);
    if (loaded == CYCLE_ERROR)
    {
        mmgr_line_put(error_message, "the record machine did not load the assembly printer");
        return 0;
    }
    const unsigned int out_limbs = cycle_record_out_limbs(record);
    unsigned int *const out = codegen_device_take<unsigned int>(&memory, lanes * out_limbs);
    if (memory.ok != 0)
    {
        CycleRecordRunRequest request{};
        request.record = record;
        request.device_in[0] = records;
        request.bodies[0] = record_count;
        request.device_index = index;
        request.count = lanes;
        request.device_out = out;
        request.error = &error;
        memory.ok = cycle_record_run(&request) != CYCLE_ERROR;
    }
    cycle_record_release(record);
    // the bytes that are not 0 gathered in lane order
    unsigned char *const bytes = codegen_device_take<unsigned char>(&memory, lanes);
    unsigned char *const gathered = codegen_device_take<unsigned char>(&memory, lanes);
    int *const gathered_count = codegen_device_take<int>(&memory, 1ULL);
    if (memory.ok != 0)
    {
        codegen_device_bytes<<<codegen_device_blocks(lanes), CODEGEN_THREADS>>>(out, lanes, out_limbs, output_offset,
                                                                                bytes);
        codegen_device_launched(&memory);
    }
    size_t select_bytes = 0u;
    memory.ok = (memory.ok != 0) && (cub::DeviceSelect::If(NULL, select_bytes, bytes, gathered, gathered_count,
                                                           (int)lanes, NonzeroByte()) == cudaSuccess);
    void *const select_temporary = codegen_device_take<unsigned char>(&memory, select_bytes);
    memory.ok = (memory.ok != 0) && (cub::DeviceSelect::If(select_temporary, select_bytes, bytes, gathered,
                                                           gathered_count, (int)lanes, NonzeroByte()) == cudaSuccess);
    int length = 0;
    codegen_device_read(&memory, &length, gathered_count, 1ULL);
    // a text's length is never negative, and the selection counts it in an int
    const unsigned long long written = (length > 0) ? (unsigned long long)length : 0ULL;
    char *const host_bytes = (char *)platform_cell_taken(written + 1ULL, platform_place_g_program_owner, 1);
    memory.ok = (memory.ok != 0) && (host_bytes != NULL);
    codegen_device_read(&memory, host_bytes, (const char *)gathered, written);
    if (memory.ok == 0)
    {
        platform_cell_released(host_bytes, platform_place_g_program_owner);
        mmgr_line_put(error_message, "the device errored on a call, or the assembly printer did not run");
        return 0;
    }
    platform_text_reset(text);
    (void)mmgr_line_put_n(&text->line, host_bytes, (size_t)written);
    platform_cell_released(host_bytes, platform_place_g_program_owner);
    return 1;
}

// why keymath's encoding, where `keymath` is 1, or key_schedule's layout errored on the program, from where it ended,
// onto `error`
static void layout_error(int keymath, const LayoutEnd *ended, VerbaLinea *error)
{
    if ((keymath != 0) && (ended->end == KEYMATH_CORE_OUTPUT))
    {
        mmgr_line_put(error, "keymath errored output ");
        mmgr_line_decimal(error, ended->at, 1u);
        return;
    }
    if (keymath != 0)
    {
        mmgr_line_put(error, "keymath errored step ");
        mmgr_line_decimal(error, ended->at, 1u);
        mmgr_line_put(error, (ended->end == KEYMATH_CORE_TABLE) ? "'s table" : "");
        return;
    }
    if (ended->end == KEY_SCHEDULE_CORE_STEP)
    {
        mmgr_line_put(error, "key_schedule errored step ");
        mmgr_line_decimal(error, ended->at, 1u);
        return;
    }
    mmgr_line_put(error, (ended->end == KEY_SCHEDULE_CORE_FILE)
                             ? "the program's registers are more limbs than the file contains"
                             : "the program's outputs are more bits than a record counts");
}

// The program of `request` arranged on the device, in `memory`, which the caller frees: keymath's encoding in one
// thread, run again with an arena twice the size while it fills the one it has, then key_schedule's layout in one
// thread. The step table is left in device memory at `*device_steps`, the tables' values, one after another as keymath
// arranges the key's, at `*device_values`, `*value_count` words, and the rest of the program's sizes arranged in
// `layout`, whose step table and tables' values are not. The host checks the request as keymath_record_encode and
// key_schedule_record_layout do before their cores run, and reads back only how each core ended, the file's limbs and
// the record's bits
static int layout_steps(DeviceArena *memory, const LayoutRequest *request, EngineRecordLayout *layout,
                        DeviceRecordStep **device_steps, unsigned int **device_values, unsigned long long *value_count,
                        VerbaLinea *error)
{
    int valid = (request->steps != NULL) && (request->count != 0u) && (request->outputs != NULL) &&
                (request->output_count != 0u) && (request->output_count <= request->count) &&
                (request->members != 0u) && (request->members <= ENGINE_RECORD_MEMBERS_MAX) &&
                (request->in_limbs != NULL) && ((request->tables != NULL) || (request->table_count == 0u));
    for (unsigned int member = 0u; (valid != 0) && (member < request->members); member += 1u)
    {
        valid = request->in_limbs[member] != 0u;
    }
    if (valid == 0)
    {
        mmgr_line_put(error, "the request is not a program keymath and key_schedule take");
        return 0;
    }
    if (request->count >= CODEGEN_COUNT_MAX)
    {
        mmgr_line_put(error, "the program has more steps than a scan counts");
        return 0;
    }
    const unsigned int count = request->count;
    // each table's values in device memory, one after another, and its descriptor pointing at them there; a table
    // indexed by more bits than a table supports, or with no values, errors on whole, as keymath could not arrange the
    // key's values
    unsigned long long words = 0ULL;
    for (unsigned int table = 0u; table < request->table_count; table += 1u)
    {
        const EngineRecordTable *const given = &request->tables[table];
        if ((given->index_bits > ENGINE_RECORD_TABLE_INDEX_BITS_MAX) || (given->values == NULL))
        {
            mmgr_line_put(error, "table ");
            mmgr_line_decimal(error, table, 1u);
            mmgr_line_put(error, " is indexed by more bits than a table supports, or has no values");
            return 0;
        }
        // index_bits is at max 32, and the entry count at max 2^32
        words += (1ULL << given->index_bits) * (unsigned long long)((given->out_bits + 31u) / 32u);
    }
    unsigned int *const values = codegen_device_take<unsigned int>(memory, words);
    EngineRecordTable *const device_tables =
        (request->tables != NULL) ? codegen_device_take<EngineRecordTable>(memory, request->table_count) : NULL;
    unsigned long long table_first = 0ULL;
    for (unsigned int table = 0u; (memory->ok != 0) && (table < request->table_count); table += 1u)
    {
        // each table's descriptor goes to the device alone, and points at its values there
        EngineRecordTable placed = request->tables[table];
        const unsigned long long table_words =
            (1ULL << placed.index_bits) * (unsigned long long)((placed.out_bits + 31u) / 32u);
        placed.values = &values[table_first];
        memory->ok = (table_words == 0ULL) ||
                     (cudaMemcpy(&values[table_first], request->tables[table].values,
                                 (size_t)(table_words * sizeof(unsigned int)), cudaMemcpyHostToDevice) == cudaSuccess);
        memory->ok = (memory->ok != 0) && (cudaMemcpy(&device_tables[table], &placed, sizeof(placed),
                                                      cudaMemcpyHostToDevice) == cudaSuccess);
        table_first += table_words;
    }
    // the program keymath and key_schedule read, in device memory
    const EngineRecordStep *const steps = codegen_device_copy(memory, request->steps, count);
    const unsigned int *const field_bits =
        (request->field_bits != NULL) ? codegen_device_copy(memory, request->field_bits, request->fields) : NULL;
    const unsigned int *const field_offset =
        (request->field_offset != NULL) ? codegen_device_copy(memory, request->field_offset, request->fields) : NULL;
    const unsigned int *const in_limbs = codegen_device_copy(memory, request->in_limbs, request->members);
    const unsigned int *const outputs = codegen_device_copy(memory, request->outputs, request->output_count);
    LayoutEnd *const device_ended = codegen_device_take<LayoutEnd>(memory, 1ULL);
    LayoutEnd ended{};
    // keymath's encoding: each step's term, never-negative mark and form. An arena that filled stays taken until the
    // memory is freed, and the arenas taken are under twice the last
    EngineRecordTerm *const terms = codegen_device_take<EngineRecordTerm>(memory, count);
    unsigned char *const never_negative = codegen_device_take<unsigned char>(memory, count);
    memory->ok = (memory->ok != 0) && (cudaMemset(never_negative, 0, count) == cudaSuccess);
    KeymathCoreEncode encoding{};
    encoding.steps = steps;
    encoding.count = count;
    encoding.field_bits = field_bits;
    encoding.fields = request->fields;
    encoding.members = request->members;
    encoding.outputs = outputs;
    encoding.output_count = request->output_count;
    encoding.tables = device_tables;
    encoding.table_count = request->table_count;
    encoding.terms = terms;
    encoding.never_negative = never_negative;
    encoding.forms = codegen_device_take<KeymathCoreAffine>(memory, count);
    encoding.bound = codegen_device_take<unsigned int>(memory, KEYMATH_BOUND_LIMBS);
    unsigned long long capacity = (unsigned long long)count * KEYMATH_ARENA_PER_STEP;
    do
    {
        encoding.arena.terms = codegen_device_take<KeymathCoreTerm>(memory, capacity);
        encoding.arena.capacity = capacity;
        if (memory->ok != 0)
        {
            codegen_device_encode<<<1u, 1u>>>(encoding, device_ended);
            codegen_device_launched(memory);
        }
        codegen_device_read(memory, &ended, device_ended, 1ULL);
        capacity *= 2ULL;
    } while ((memory->ok != 0) && (ended.ok == 0) && (ended.end == KEYMATH_CORE_FULL));
    if ((memory->ok != 0) && (ended.ok == 0))
    {
        layout_error(1, &ended, error);
        return 0;
    }
    // key_schedule's layout: each step arranged for the device and placed, and each output placed in the record
    DeviceRecordStep *const record_steps = codegen_device_take<DeviceRecordStep>(memory, count);
    KeyScheduleCoreLayout core_layout{};
    core_layout.terms = terms;
    core_layout.step_count = count;
    core_layout.outputs = outputs;
    core_layout.output_count = request->output_count;
    core_layout.tables = device_tables;
    core_layout.table_count = request->table_count;
    core_layout.field_offset = field_offset;
    core_layout.fields = request->fields;
    core_layout.in_limbs = in_limbs;
    core_layout.reuse = request->reuse;
    core_layout.steps = record_steps;
    core_layout.last_use = codegen_device_take<unsigned int>(memory, count);
    core_layout.ending_first = codegen_device_take<unsigned int>(memory, (unsigned long long)count + 1ULL);
    core_layout.ending = codegen_device_take<unsigned int>(memory, count);
    core_layout.freed = codegen_device_take<KeyScheduleBlock>(memory, (unsigned long long)count + 1ULL);
    core_layout.table_offset =
        codegen_device_take<unsigned long long>(memory, (unsigned long long)request->table_count + 1ULL);
    ended = LayoutEnd{};
    if (memory->ok != 0)
    {
        codegen_layout<<<1u, 1u>>>(core_layout, device_ended);
        codegen_device_launched(memory);
    }
    codegen_device_read(memory, &ended, device_ended, 1ULL);
    if (memory->ok == 0)
    {
        mmgr_line_put(error, "the device errored on a call");
        return 0;
    }
    if (ended.ok == 0)
    {
        layout_error(0, &ended, error);
        return 0;
    }
    // the sizes as key_schedule_record_layout positions it out; the layout limited the file to ENGINE_RECORD_LIMBS_MAX
    // limbs and the record's bits to a 31-bit count
    mmgr_bytes_set(layout, 0, sizeof(*layout));
    layout->steps = count;
    layout->members = request->members;
    layout->file_limbs = (unsigned int)ended.file_limbs;
    for (unsigned int member = 0u; member < request->members; member += 1u)
    {
        layout->in_limbs[member] = request->in_limbs[member];
    }
    layout->out_bits = (unsigned int)ended.out_bits;
    layout->out_limbs = (unsigned int)((ended.out_bits + 31ULL) / 32ULL);
    *device_steps = record_steps;
    *device_values = values;
    *value_count = words;
    return 1;
}

int codegen_device(const EngineRecordLayout *layout, const AsmPrinterRuleset *text_rules, unsigned int places,
                   const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error)
{
    DeviceArena memory{};
    memory.ok = 1;
    const DeviceRecordStep *const device_steps = codegen_device_copy(&memory, layout->step_table, layout->steps);
    const int written = codegen_device_written(&memory, layout, device_steps, text_rules, places, costs, text, error);
    codegen_device_release(&memory);
    return written;
}

int layout_device(const LayoutRequest *request, EngineRecordLayout *layout, VerbaLinea *error)
{
    DeviceArena memory{};
    memory.ok = 1;
    DeviceRecordStep *device_steps = NULL;
    unsigned int *device_values = NULL;
    unsigned long long value_count = 0ULL;
    EngineRecordLayout device_layout{};
    int ok = layout_steps(&memory, request, &device_layout, &device_steps, &device_values, &value_count, error);
    // the step table and the tables' values read back, arranged as key_schedule_record_layout positions them out
    if (ok != 0)
    {
        // the tables are resources of the layout, which key_schedule_record_release gives back to the pool
        device_layout.step_table =
            (DeviceRecordStep *)platform_cell_taken((unsigned long long)device_layout.steps * sizeof(DeviceRecordStep),
                                                    (unsigned int)platform_place_g_engine_owner, 0);
        device_layout.table_values =
            (value_count != 0ULL) ? (unsigned int *)platform_cell_taken((value_count + 1ULL) * sizeof(unsigned int),
                                                                        (unsigned int)platform_place_g_engine_owner, 0)
                                  : NULL;
        device_layout.table_word_count = value_count;
        ok = (device_layout.step_table != NULL) && ((value_count == 0ULL) || (device_layout.table_values != NULL));
        if (ok != 0)
        {
            codegen_device_read(&memory, device_layout.step_table, device_steps, device_layout.steps);
            codegen_device_read(&memory, device_layout.table_values, device_values, value_count);
            ok = memory.ok != 0;
        }
        if (ok == 0)
        {
            mmgr_line_put(error, (memory.ok != 0) ? "the host could not store the layout read back"
                                                  : "the device errored on a call");
            key_schedule_record_release(&device_layout);
        }
    }
    codegen_device_release(&memory);
    *layout = device_layout;
    return ok;
}

int codegen_device_steps(const LayoutRequest *request, const AsmPrinterRuleset *text_rules, unsigned int places,
                         const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error)
{
    DeviceArena memory{};
    memory.ok = 1;
    DeviceRecordStep *device_steps = NULL;
    unsigned int *device_values = NULL;
    unsigned long long value_count = 0ULL;
    EngineRecordLayout device_layout{};
    const int written =
        (layout_steps(&memory, request, &device_layout, &device_steps, &device_values, &value_count, error) != 0) &&
        (codegen_device_written(&memory, &device_layout, device_steps, text_rules, places, costs, text, error) != 0);
    codegen_device_release(&memory);
    return written;
}

int codegen_device_instrs(const EngineRecordLayout *layout, const unsigned int *scratch,
                          unsigned long long scratch_count, unsigned int places, const ScheduleCosts *costs,
                          ScheduleReport *report, PlatformArray *items, VerbaLinea *error)
{
    DeviceArena memory{};
    memory.ok = 1;
    const DeviceRecordStep *const device_steps = codegen_device_copy(&memory, layout->step_table, layout->steps);
    MachineInstr *decided = NULL;
    unsigned long long item_count = 0ULL;
    int ok = codegen_device_decide(&memory, layout, device_steps, scratch, scratch_count, places, costs, report,
                                   &decided, &item_count, error);
    if (ok != 0)
    {
        memory.ok = platform_array_counted(items, 0ULL) && platform_array_counted(items, item_count);
        codegen_device_read(&memory, (MachineInstr *)items->cell, decided, item_count);
        ok = memory.ok != 0;
        if (ok == 0)
        {
            mmgr_line_put(error, "the device errored on a call");
        }
    }
    codegen_device_release(&memory);
    return ok;
}
#endif
#if !(defined(__CUDACC__))
int codegen_device(const EngineRecordLayout *layout, const AsmPrinterRuleset *text_rules, unsigned int places,
                   const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error)
{
    (void)layout;
    (void)text_rules;
    (void)places;
    (void)costs;
    (void)text;
    mmgr_line_put(error, "the build has no device");
    return 0;
}

int layout_device(const LayoutRequest *request, EngineRecordLayout *layout, VerbaLinea *error)
{
    (void)request;
    (void)layout;
    mmgr_line_put(error, "the build has no device");
    return 0;
}

int codegen_device_steps(const LayoutRequest *request, const AsmPrinterRuleset *text_rules, unsigned int places,
                         const ScheduleCosts *costs, PlatformText *text, VerbaLinea *error)
{
    (void)request;
    (void)text_rules;
    (void)places;
    (void)costs;
    (void)text;
    mmgr_line_put(error, "the build has no device");
    return 0;
}

int codegen_device_instrs(const EngineRecordLayout *layout, const unsigned int *scratch,
                          unsigned long long scratch_count, unsigned int places, const ScheduleCosts *costs,
                          ScheduleReport *report, PlatformArray *items, VerbaLinea *error)
{
    (void)layout;
    (void)scratch;
    (void)scratch_count;
    (void)places;
    (void)costs;
    (void)report;
    (void)items;
    mmgr_line_put(error, "the build has no device");
    return 0;
}
#endif
