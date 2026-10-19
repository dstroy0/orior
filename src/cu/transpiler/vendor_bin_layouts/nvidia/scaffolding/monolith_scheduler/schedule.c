// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schedule.c: the schedule: what is waited on, the same operation, what stands behind later or earlier, an instruction followed, the container read, the stalls, the operations by count, and the record written
#include "scheduler_internal.h"

// 1 where an instruction of the section from `first` to `last` waits on barrier `barrier`
static int scheduler_waited(unsigned int barrier, unsigned int first, unsigned int last)
{
    for (unsigned int at = first; at <= last; at += 1u)
    {
        if ((s_section[at].wait >> barrier) & 1u)
        {
            return 1;
        }
    }
    return 0;
}

// 1 where operations `one` and `two` share their names before the first dot
static int scheduler_same_operation(unsigned int one, unsigned int two)
{
    const char *const left = s_operation[one].name;
    const char *const right = s_operation[two].name;
    const size_t length = strcspn(left, ".");
    return (length == strcspn(right, ".")) && (strncmp(left, right, length) == 0);
}

// 1 where an instruction after `writer` and before `reader`, of the writer's operation, sets a write barrier that an
// instruction after it and no later than the reader waits on
static int scheduler_behind_later(unsigned int writer, unsigned int reader)
{
    for (unsigned int at = writer + 1u; at < reader; at += 1u)
    {
        const SchedulerInstruction *const later = &s_section[at];
        if ((later->operation != SCHEDULER_OPERATIONS) && (later->write_barrier != SASS_BARRIER_NONE) &&
            scheduler_same_operation(s_section[writer].operation, later->operation) &&
            scheduler_waited(later->write_barrier, at + 1u, reader))
        {
            return 1;
        }
    }
    return 0;
}

// where an instruction before `writer`, of the writer's operation and no farther back than SCHEDULER_REACH, sets a
// write barrier that an instruction after the writer and no later than `reader` waits on: the cycles between the first
// such wait and the reader, or UINT_MAX where there is none
static unsigned int scheduler_behind_earlier(unsigned int writer, unsigned int reader)
{
    const unsigned int reach = (writer > SCHEDULER_REACH) ? (writer - SCHEDULER_REACH) : 0u;
    for (unsigned int back = writer; back > reach; back -= 1u)
    {
        const SchedulerInstruction *const earlier = &s_section[back - 1u];
        if ((earlier->operation == SCHEDULER_OPERATIONS) || (earlier->write_barrier == SASS_BARRIER_NONE) ||
            !scheduler_same_operation(s_section[writer].operation, earlier->operation))
        {
            continue;
        }
        for (unsigned int at = writer + 1u; at <= reader; at += 1u)
        {
            if ((s_section[at].wait >> earlier->write_barrier) & 1u)
            {
                unsigned int cycles = 0u;
                for (unsigned int step = at; step < reader; step += 1u)
                {
                    cycles += s_section[step].stall;
                }
                return cycles;
            }
        }
    }
    return UINT_MAX;
}

// each instruction of a section that writes a register, followed to the first instruction that reads it: where the
// writer sets a write barrier, whether a wait on it stands between them; where it sets none, whether it is held behind
// a later one's barrier, and otherwise the cycles between them. The look stops at an instruction that leaves the
// straight line, one that writes the register again, and past SCHEDULER_REACH instructions
static void scheduler_follow(unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const SchedulerInstruction *const writer = &s_section[at];
        if ((writer->writes == SCHEDULER_NO_REGISTER) || (writer->operation == SCHEDULER_OPERATIONS))
        {
            continue;
        }
        SchedulerOperation *const operation = &s_operation[writer->operation];
        unsigned int cycles = 0u;
        for (unsigned int next = at + 1u; (next < count) && (next <= at + SCHEDULER_REACH); next += 1u)
        {
            cycles += s_section[next - 1u].stall;
            const SchedulerInstruction *const reader = &s_section[next];
            if (scheduler_reads_register(reader, writer->writes))
            {
                if (writer->write_barrier != SASS_BARRIER_NONE)
                {
                    const int waited = scheduler_waited(writer->write_barrier, at + 1u, next);
                    operation->readers_waiting += waited ? 1ull : 0ull;
                    operation->readers_not_waiting += waited ? 0ull : 1ull;
                }
                else if (scheduler_behind_later(at, next))
                {
                    operation->readers_behind_later += 1ull;
                }
                else if (scheduler_behind_earlier(at, next) != UINT_MAX)
                {
                    const unsigned int after = scheduler_behind_earlier(at, next);
                    operation->readers_behind_earlier += 1ull;
                    operation->soonest_after_earlier =
                        (after < operation->soonest_after_earlier) ? after : operation->soonest_after_earlier;
                }
                else
                {
                    operation->timed_reads += 1ull;
                    operation->soonest_read = (cycles < operation->soonest_read) ? cycles : operation->soonest_read;
                }
                break;
            }
            if (reader->control || (reader->writes == writer->writes))
            {
                break;
            }
        }
    }
}

// every code section of the ELF in s_cubin, `size` bytes, read and followed. 1, or 0 where it is not an ELF this
// reads
int scheduler_cubin(unsigned long long size)
{
    if ((size < 0x40ull) || (memcmp(s_cubin, "\x7f" "ELF", 4u) != 0))
    {
        return 0;
    }
    const unsigned long long table = scheduler_word(&s_cubin[0x28], 8u);
    const unsigned int entry = (unsigned int)scheduler_word(&s_cubin[0x3a], 2u);
    const unsigned int count = (unsigned int)scheduler_word(&s_cubin[0x3c], 2u);
    const unsigned int names = (unsigned int)scheduler_word(&s_cubin[0x3e], 2u);
    if ((names >= count) || ((table + ((unsigned long long)entry * count)) > size))
    {
        return 0;
    }
    const unsigned long long strings = scheduler_word(&s_cubin[table + ((unsigned long long)entry * names) + 0x18], 8u);
    for (unsigned int section = 0u; section < count; section += 1u)
    {
        const unsigned char *const header = &s_cubin[table + ((unsigned long long)entry * section)];
        const unsigned long long label = strings + scheduler_word(header, 4u);
        const unsigned long long offset = scheduler_word(&header[0x18], 8u);
        const unsigned long long length = scheduler_word(&header[0x20], 8u);
        if ((label >= size) || (strncmp((const char *)&s_cubin[label], ".text.", 6u) != 0) ||
            ((offset + length) > size))
        {
            continue;
        }
        const unsigned int instructions = (unsigned int)(length / 16ull);
        const unsigned int kept =
            (instructions < SCHEDULER_SECTION_INSTRUCTIONS) ? instructions : SCHEDULER_SECTION_INSTRUCTIONS;
        for (unsigned int at = 0u; at < kept; at += 1u)
        {
            scheduler_instruction(&s_cubin[offset + (16ull * at)], 16ull * at, &s_section[at]);
        }
        scheduler_follow(kept);
    }
    return 1;
}

// the stalls an operation was given, as the least, the most and the one given most often
static void scheduler_stalls(const SchedulerOperation *operation, char *text, size_t room)
{
    unsigned int least = 16u;
    unsigned int most = 0u;
    unsigned int often = 0u;
    for (unsigned int stall = 0u; stall < 16u; stall += 1u)
    {
        if (operation->stalls[stall] == 0ull)
        {
            continue;
        }
        least = (stall < least) ? stall : least;
        most = stall;
        often = (operation->stalls[stall] > operation->stalls[often]) ? stall : often;
    }
    snprintf(text, room, "%u to %u, most often %u", least, most, often);
}

static int scheduler_by_count(const void *left, const void *right)
{
    const SchedulerOperation *const one = (const SchedulerOperation *)left;
    const SchedulerOperation *const two = (const SchedulerOperation *)right;
    return (one->count < two->count) ? 1 : ((one->count > two->count) ? -1 : strcmp(one->name, two->name));
}

// the record: every operation by how often NVIDIA wrote it, and the readings gathered from them
int scheduler_record(const char *path)
{
    FILE *const out = fopen(path, "wb");
    if (out == NULL)
    {
        return 0;
    }
    qsort(s_operation, s_operations, sizeof(s_operation[0]), scheduler_by_count);
    fprintf(out, "# The scheduler's bits NVIDIA's compiler writes\n\n");
    fprintf(out, "Written by `monolith_scheduler.sh` whole on every run. Every CUDA source of the tree is built by "
                 "NVIDIA's compiler for one part, each cubin's code sections are read from the ELF, each instruction "
                 "is named by our reader through the machine file and its bits 105 to 127 read through the fields the "
                 "machine file names. No listing is read and no disassembler is run.\n\n");
    fprintf(out, "%u cubins, %llu instructions, %llu no form reads, %u operations.\n\n", s_cubins, s_instructions,
            s_unread, s_operations);
    fprintf(out, "An operation's soonest read is the fewest cycles, its stalls summed, between it and the first "
                 "instruction that reads the register it writes, over every place it sets no write barrier and no "
                 "later one holds it. Readers waiting count the first readers of a register it writes with a write "
                 "barrier where a wait on that barrier stands between the two, at the reader or before it, against "
                 "those where none does. A barrier counts its producers, and a wait on it holds until all of them are "
                 "back. Behind a later one counts the first readers of a register it writes with no barrier where a "
                 "later instruction of the same operation sets one, waited on before the read. Behind an earlier one "
                 "counts those where no later one does and an earlier instruction of the same operation sets one, "
                 "waited on before the read, with the fewest cycles between that wait and the read. The machine file "
                 "columns count the places whose form records a write barrier, and a read barrier, against the places "
                 "a form was found.\n\n");
    fprintf(out, "| operation | written | stalls | write barrier | read barrier | waits | soonest read | readers "
                 "waiting | behind a later one | behind an earlier one | machine file write | machine file read |\n"
                 "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (unsigned int at = 0u; at < s_operations; at += 1u)
    {
        const SchedulerOperation *const operation = &s_operation[at];
        char stalls[64];
        char soonest[64];
        scheduler_stalls(operation, stalls, sizeof(stalls));
        if (operation->timed_reads == 0ull)
        {
            snprintf(soonest, sizeof(soonest), "-");
        }
        else
        {
            snprintf(soonest, sizeof(soonest), "%u over %llu", operation->soonest_read, operation->timed_reads);
        }
        char earlier[64];
        if (operation->readers_behind_earlier == 0ull)
        {
            snprintf(earlier, sizeof(earlier), "0");
        }
        else
        {
            snprintf(earlier, sizeof(earlier), "%llu, %u after the wait", operation->readers_behind_earlier,
                     operation->soonest_after_earlier);
        }
        fprintf(out,
                "| `%s` | %llu | %s | %llu | %llu | %llu | %s | %llu of %llu | %llu | %s | %llu of %llu | %llu of %llu |\n",
                operation->name, operation->count, stalls, operation->write_barriers, operation->read_barriers,
                operation->waits, soonest, operation->readers_waiting,
                operation->readers_waiting + operation->readers_not_waiting, operation->readers_behind_later, earlier,
                operation->machine_barriers, operation->machine_formed, operation->machine_read_barriers,
                operation->machine_formed);
    }
    fclose(out);
    return 1;
}
