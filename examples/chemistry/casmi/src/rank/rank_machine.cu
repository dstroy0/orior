// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_machine.cu: the record machine as the ranker reaches it: a built program loaded, a sweep of it run on the
// device over records the host holds, and a record's fields read and written as exact integers
#include "rank_internal.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

int rank_machine_load(SimResults *results, const char *name, ExactRecordProgram *program, const unsigned int *outputs,
                      unsigned int output_count, unsigned int members, const unsigned int *limbs, RankMachine *machine)
{
    machine->loaded = 0;
    machine->record = NULL;
    machine->members = members;
    machine->outputs.assign(outputs, outputs + output_count);
    for (unsigned int member = 0u; member < ENGINE_RECORD_MEMBERS_MAX; member += 1u)
    {
        machine->limbs[member] = (member < members) ? limbs[member] : 0u;
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    const KeymathRecordRequest encode = {program->steps,  program->count, program->field_bits,   program->fields,
                                         members,         outputs,        output_count,          program->tables,
                                         program->table_count, &machine->key, &error};
    if ((program->failed != 0) || (keymath_record_encode(&encode) == KEYMATH_ERROR))
    {
        scriptura_text(&results->line, "  program ");
        scriptura_text(&results->line, name);
        rank_error_line(results, "did not imprint", &error);
        return 0;
    }
    const KeyScheduleRecordRequest layout = {&machine->key, program->field_offset, program->fields, machine->limbs, 1,
                                             &machine->layout, &error};
    if (key_schedule_record_layout(&layout) == KEY_SCHEDULE_ERROR)
    {
        keymath_record_release(&machine->key);
        scriptura_text(&results->line, "  program ");
        scriptura_text(&results->line, name);
        rank_error_line(results, "did not lay out", &error);
        return 0;
    }
    if (cycle_record_load(&machine->layout, &machine->record, &error) == CYCLE_ERROR)
    {
        key_schedule_record_release(&machine->layout);
        keymath_record_release(&machine->key);
        scriptura_text(&results->line, "  program ");
        scriptura_text(&results->line, name);
        rank_error_line(results, "did not load", &error);
        return 0;
    }
    machine->loaded = 1;
    scriptura_text(&results->line, "  program ");
    scriptura_text(&results->line, name);
    rank_line_decimal(results, ": ", program->count);
    rank_line_decimal(results, " steps, record ", (unsigned long long)machine->layout.out_bits);
    rank_line_decimal(results, " bits, ", (unsigned long long)machine->layout.out_limbs);
    scriptura_text(&results->line, " limbs");
    rank_line_end(results);
    return 1;
}

void rank_machine_release(RankMachine *machine)
{
    if (machine->loaded)
    {
        cycle_record_release(machine->record);
        key_schedule_record_release(&machine->layout);
        keymath_record_release(&machine->key);
    }
    machine->loaded = 0;
}

RankField rank_output(const RankMachine *machine, unsigned int output)
{
    const DeviceRecordStep place = machine->layout.step_table[machine->outputs[output]];
    const RankField field = {place.out_offset, place.out_bits};
    return field;
}

int rank_sweep(SimResults *results, const char *name, const RankMachine *machine, const unsigned int *const *members,
               const unsigned long long *bodies, const unsigned int *index, unsigned long long lanes, unsigned int *out,
               unsigned long long *microseconds)
{
    if (lanes == 0ull)
    {
        return 1;
    }
    unsigned int *device_in[ENGINE_RECORD_MEMBERS_MAX] = {NULL, NULL, NULL};
    unsigned int *device_index = NULL;
    unsigned int *device_out = NULL;
    const size_t out_bytes = (size_t)lanes * machine->layout.out_limbs * sizeof(unsigned int);
    int ok = cudaMalloc((void **)&device_out, out_bytes) == cudaSuccess;
    for (unsigned int member = 0u; ok && (member < machine->members); member += 1u)
    {
        const size_t bytes = (size_t)bodies[member] * machine->limbs[member] * sizeof(unsigned int);
        ok = (cudaMalloc((void **)&device_in[member], bytes + 4u) == cudaSuccess) &&
             (cudaMemcpy(device_in[member], members[member], bytes, cudaMemcpyHostToDevice) == cudaSuccess);
    }
    if (ok && (index != NULL))
    {
        const size_t bytes = (size_t)(lanes * machine->members) * sizeof(unsigned int);
        ok = (cudaMalloc((void **)&device_index, bytes) == cudaSuccess) &&
             (cudaMemcpy(device_index, index, bytes, cudaMemcpyHostToDevice) == cudaSuccess);
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long started = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {machine->record,
                                           {device_in[0], device_in[1], device_in[2]},
                                           {bodies[0], (machine->members > 1u) ? bodies[1] : 0ull,
                                            (machine->members > 2u) ? bodies[2] : 0ull},
                                           device_index,
                                           lanes,
                                           device_out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
        if (!ok)
        {
            scriptura_text(&results->line, "  sweep ");
            scriptura_text(&results->line, name);
            rank_error_line(results, "errored", &error);
        }
    }
    ok = ok && (cudaMemcpy(out, device_out, out_bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
    *microseconds += engine_clock_microseconds() - started;
    for (unsigned int member = 0u; member < ENGINE_RECORD_MEMBERS_MAX; member += 1u)
    {
        cudaFree(device_in[member]);
    }
    cudaFree(device_index);
    cudaFree(device_out);
    return ok;
}

void rank_field_read(const unsigned int *record, RankField field, AnchorExactInteger *value)
{
    // the field's bits as a magnitude, then two's complement where its top bit is set
    anchor_exact_zero(value);
    AnchorExactInteger raw;
    anchor_exact_zero(&raw);
    for (unsigned int bit = 0u; bit < field.bits; bit += 1u)
    {
        const unsigned int at = field.offset + bit;
        if ((record[at / 32u] >> (at % 32u)) & 1u)
        {
            raw.limb[bit / 32u] |= 1u << (bit % 32u);
        }
    }
    const int negative = (field.bits != 0u) && ((raw.limb[(field.bits - 1u) / 32u] >> ((field.bits - 1u) % 32u)) & 1u);
    raw.sign = 0;
    for (unsigned int limb = 0u; limb < ANCHOR_EXACT_LIMBS; limb += 1u)
    {
        raw.sign = (raw.limb[limb] != 0u) ? 1 : raw.sign;
    }
    if (!negative)
    {
        *value = raw;
        return;
    }
    // the magnitude 2^bits - raw, its sign below zero
    AnchorExactInteger power;
    anchor_exact_zero(&power);
    power.limb[field.bits / 32u] = 1u << (field.bits % 32u);
    power.sign = 1;
    (void)anchor_exact_subtract(&power, &raw, value);
    value->sign = -value->sign;
}

int rank_field_sign(const unsigned int *record, RankField field)
{
    // the top bit is the sign; any bit set otherwise makes the value above zero
    if (field.bits == 0u)
    {
        return 0;
    }
    const unsigned int top = field.offset + field.bits - 1u;
    if ((record[top / 32u] >> (top % 32u)) & 1u)
    {
        return -1;
    }
    // the bits below the top, a limb at a time, each limb masked to the part of it the field holds
    for (unsigned int at = field.offset; at < top;)
    {
        const unsigned int low = at % 32u;
        const unsigned int span = ((top - at) < (32u - low)) ? (top - at) : (32u - low);
        const unsigned int mask = (span == 32u) ? 0xFFFFFFFFu : (((1u << span) - 1u) << low);
        if ((record[at / 32u] & mask) != 0u)
        {
            return 1;
        }
        at += span;
    }
    return 0;
}

void rank_bits_copy(unsigned int *record, unsigned int offset, const unsigned int *limbs, unsigned int bits)
{
    // a limb's bits at a time: each source limb shifted into the one or two record limbs it lands across
    for (unsigned int done = 0u; done < bits; done += 32u)
    {
        const unsigned int span = ((bits - done) < 32u) ? (bits - done) : 32u;
        const unsigned int word = (span == 32u) ? limbs[done / 32u] : (limbs[done / 32u] & ((1u << span) - 1u));
        const unsigned int at = offset + done;
        const unsigned int shift = at % 32u;
        record[at / 32u] |= word << shift;
        if ((shift != 0u) && ((shift + span) > 32u))
        {
            record[(at / 32u) + 1u] |= word >> (32u - shift);
        }
    }
}

void rank_field_put(unsigned int *record, unsigned int offset, unsigned int bits, const AnchorExactInteger *value)
{
    // two's complement: the magnitude, or 2^bits less it below zero
    AnchorExactInteger written = *value;
    if (value->sign < 0)
    {
        AnchorExactInteger power;
        anchor_exact_zero(&power);
        power.limb[bits / 32u] = 1u << (bits % 32u);
        power.sign = 1;
        AnchorExactInteger magnitude = *value;
        magnitude.sign = 1;
        (void)anchor_exact_subtract(&power, &magnitude, &written);
    }
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = offset + bit;
        const unsigned int set = (written.limb[bit / 32u] >> (bit % 32u)) & 1u;
        record[at / 32u] = (record[at / 32u] & ~(1u << (at % 32u))) | (set << (at % 32u));
    }
}

void rank_put_unsigned(unsigned int *record, unsigned int offset, unsigned int bits, unsigned long long value)
{
    // into a record whose field is 0, a value below 2^bits
    const unsigned int limbs[2] = {(unsigned int)(value & 0xFFFFFFFFull), (unsigned int)(value >> 32u)};
    rank_bits_copy(record, offset, limbs, (bits < 64u) ? bits : 64u);
}

unsigned int rank_bits(const AnchorExactInteger *value)
{
    for (unsigned int limb = ANCHOR_EXACT_LIMBS; limb > 0u; limb -= 1u)
    {
        if (value->limb[limb - 1u] != 0u)
        {
            return (32u * (limb - 1u)) + exact_record_bits_of(value->limb[limb - 1u]);
        }
    }
    return 0u;
}

void rank_exact_of(unsigned long long value, AnchorExactInteger *out)
{
    anchor_exact_zero(out);
    out->limb[0] = (unsigned int)(value & 0xFFFFFFFFull);
    out->limb[1] = (unsigned int)(value >> 32u);
    out->sign = (value != 0ull) ? 1 : 0;
}
