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

int rank_device_room(void **buffer, size_t *room, size_t bytes)
{
    if (bytes <= *room)
    {
        return 1;
    }
    cudaFree(*buffer);
    *buffer = NULL;
    *room = 0u;
    if (cudaMalloc(buffer, bytes) != cudaSuccess)
    {
        *buffer = NULL;
        return 0;
    }
    *room = bytes;
    return 1;
}

int rank_sweep(SimResults *results, const char *name, const RankMachine *machine, const unsigned int *const *members,
               const unsigned long long *bodies, const unsigned int *index, unsigned long long lanes, unsigned int *out,
               unsigned long long *microseconds)
{
    if (lanes == 0ull)
    {
        return 1;
    }
    // a piece of lanes at a time, as many as hold RANK_SWEEP_BYTES on the device and at least one. Of each member, the
    // records from the least a piece's lanes read to the most go to the device, once for every member that names the
    // same records, and the piece's index counts from that least. With no index lane i reads record i: a piece is
    // the lanes one lane's bytes fill; through an index every record goes at once where every lane fits beside them,
    // and otherwise each lane's records are read to find where a piece ends
    const unsigned int count = machine->members;
    const unsigned int out_limbs = machine->layout.out_limbs;
    unsigned int same[ENGINE_RECORD_MEMBERS_MAX] = {0u, 1u, 2u};
    for (unsigned int member = 0u; member < count; member += 1u)
    {
        for (unsigned int earlier = 0u; (same[member] == member) && (earlier < member); earlier += 1u)
        {
            same[member] = ((members[earlier] == members[member]) && (bodies[earlier] == bodies[member]) &&
                            (machine->limbs[earlier] == machine->limbs[member]))
                               ? same[earlier]
                               : member;
        }
    }
    const unsigned long long lane_bytes =
        (unsigned long long)(((index != NULL) ? count : 0u) + out_limbs) * sizeof(unsigned int);
    unsigned long long whole_bytes = lanes * lane_bytes;
    unsigned long long one_bytes = 0ull;
    unsigned long long own_bytes = lane_bytes;
    for (unsigned int member = 0u; member < count; member += 1u)
    {
        const unsigned long long record_bytes =
            (same[member] == member) ? (machine->limbs[member] * sizeof(unsigned int)) : 0ull;
        whole_bytes += bodies[member] * record_bytes;
        one_bytes += (bodies[member] == 1ull) ? record_bytes : 0ull;
        own_bytes += (bodies[member] == 1ull) ? 0ull : record_bytes;
    }
    const int whole = (index != NULL) && (whole_bytes <= RANK_SWEEP_BYTES);
    const unsigned long long own_lanes =
        (one_bytes < RANK_SWEEP_BYTES) ? ((RANK_SWEEP_BYTES - one_bytes) / own_bytes) : 0ull;
    unsigned int *device_in[ENGINE_RECORD_MEMBERS_MAX] = {NULL, NULL, NULL};
    size_t in_room[ENGINE_RECORD_MEMBERS_MAX] = {0u, 0u, 0u};
    unsigned int *device_index = NULL;
    size_t index_room = 0u;
    unsigned int *device_out = NULL;
    size_t out_room = 0u;
    std::vector<unsigned int> piece_index;
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long started = engine_clock_microseconds();
    int ok = 1;
    unsigned long long start = 0ull;
    while (ok && (start < lanes))
    {
        unsigned long long least[ENGINE_RECORD_MEMBERS_MAX] = {~0ull, ~0ull, ~0ull};
        unsigned long long most[ENGINE_RECORD_MEMBERS_MAX] = {0ull, 0ull, 0ull};
        unsigned long long piece = 0ull;
        if ((index == NULL) || whole)
        {
            piece = (index == NULL) ? (lanes - start) : lanes;
            piece = ((index == NULL) && (own_lanes < piece)) ? ((own_lanes == 0ull) ? 1ull : own_lanes) : piece;
            for (unsigned int member = 0u; member < count; member += 1u)
            {
                const int own = (index == NULL) && (bodies[member] != 1ull);
                least[member] = own ? start : 0ull;
                most[member] = own ? (start + piece - 1ull) : (bodies[member] - 1ull);
            }
        }
        while ((index != NULL) && !whole && ((start + piece) < lanes))
        {
            unsigned long long low[ENGINE_RECORD_MEMBERS_MAX] = {least[0], least[1], least[2]};
            unsigned long long high[ENGINE_RECORD_MEMBERS_MAX] = {most[0], most[1], most[2]};
            for (unsigned int member = 0u; member < count; member += 1u)
            {
                const unsigned long long record = index[((start + piece) * count) + member];
                const unsigned int held = same[member];
                low[held] = (record < low[held]) ? record : low[held];
                high[held] = (record > high[held]) ? record : high[held];
            }
            unsigned long long bytes = (piece + 1ull) * lane_bytes;
            for (unsigned int member = 0u; member < count; member += 1u)
            {
                bytes += (same[member] == member)
                             ? ((high[member] - low[member] + 1ull) * machine->limbs[member] * sizeof(unsigned int))
                             : 0ull;
            }
            if ((piece != 0ull) && (bytes > RANK_SWEEP_BYTES))
            {
                break;
            }
            memcpy(least, low, sizeof(least));
            memcpy(most, high, sizeof(most));
            piece += 1ull;
        }
        const unsigned int *run_in[ENGINE_RECORD_MEMBERS_MAX] = {NULL, NULL, NULL};
        unsigned long long piece_bodies[ENGINE_RECORD_MEMBERS_MAX] = {0ull, 0ull, 0ull};
        for (unsigned int member = 0u; ok && (member < count); member += 1u)
        {
            const unsigned int held = same[member];
            piece_bodies[member] = most[held] - least[held] + 1ull;
            if (held == member)
            {
                const size_t bytes = (size_t)(piece_bodies[member] * machine->limbs[member]) * sizeof(unsigned int);
                ok = rank_device_room((void **)&device_in[member], &in_room[member], bytes + 4u) &&
                     (cudaMemcpy(device_in[member], members[member] + (least[member] * machine->limbs[member]), bytes,
                                 cudaMemcpyHostToDevice) == cudaSuccess);
            }
            run_in[member] = device_in[held];
        }
        if (ok && whole)
        {
            const size_t bytes = (size_t)(lanes * count) * sizeof(unsigned int);
            ok = rank_device_room((void **)&device_index, &index_room, bytes) &&
                 (cudaMemcpy(device_index, index, bytes, cudaMemcpyHostToDevice) == cudaSuccess);
        }
        if (ok && (index != NULL) && !whole)
        {
            piece_index.resize((size_t)(piece * count));
            for (unsigned long long lane = 0ull; lane < piece; lane += 1ull)
            {
                for (unsigned int member = 0u; member < count; member += 1u)
                {
                    piece_index[(lane * count) + member] =
                        (unsigned int)(index[((start + lane) * count) + member] - least[same[member]]);
                }
            }
            const size_t bytes = piece_index.size() * sizeof(unsigned int);
            ok = rank_device_room((void **)&device_index, &index_room, bytes) &&
                 (cudaMemcpy(device_index, piece_index.data(), bytes, cudaMemcpyHostToDevice) == cudaSuccess);
        }
        const size_t out_bytes = (size_t)(piece * out_limbs) * sizeof(unsigned int);
        ok = ok && rank_device_room((void **)&device_out, &out_room, out_bytes);
        if (ok)
        {
            const CycleRecordRunRequest run = {machine->record,
                                               {run_in[0], run_in[1], run_in[2]},
                                               {piece_bodies[0], piece_bodies[1], piece_bodies[2]},
                                               device_index,
                                               piece,
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
        ok = ok && (cudaMemcpy(out + (start * out_limbs), device_out, out_bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
        start += piece;
    }
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
