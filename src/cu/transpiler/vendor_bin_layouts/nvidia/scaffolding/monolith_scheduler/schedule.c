// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schedule.c: the schedule: what is waited on, the same operation, what is behind later or earlier, an instruction
// followed, the container read, the stalls, the operations by count, and the record written
#include "scheduler_internal.h"
#include "../../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

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
    const size_t length = mmgr_text_span_not(left, ".", PLATFORM_TEXT_TO_TERMINATOR);
    return (length == mmgr_text_span_not(right, ".", PLATFORM_TEXT_TO_TERMINATOR)) &&
           (mmgr_text_order(left, right, (size_t)(length)) == 0);
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
// writer sets a write barrier, whether a wait on it is between them; where it sets none, whether it is blocked
// behind a later one's barrier, and otherwise the cycles between them. The look stops at an instruction that leaves the
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
                    operation->readers_waiting += waited ? 1ULL : 0ULL;
                    operation->readers_not_waiting += waited ? 0ULL : 1ULL;
                }
                else if (scheduler_behind_later(at, next))
                {
                    operation->readers_behind_later += 1ULL;
                }
                else if (scheduler_behind_earlier(at, next) != UINT_MAX)
                {
                    const unsigned int after = scheduler_behind_earlier(at, next);
                    operation->readers_behind_earlier += 1ULL;
                    operation->soonest_after_earlier =
                        (after < operation->soonest_after_earlier) ? after : operation->soonest_after_earlier;
                }
                else
                {
                    operation->timed_reads += 1ULL;
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
    if ((size < 0x40ULL) || (mmgr_bytes_compare(s_cubin,
                                                "\x7f"
                                                "ELF",
                                                4u) != 0))
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
        if ((label >= size) ||
            (!mmgr_text_starts((const char *)&s_cubin[label], ".text.", PLATFORM_TEXT_TO_TERMINATOR)) ||
            ((offset + length) > size))
        {
            continue;
        }
        const unsigned int instructions = (unsigned int)(length / 16ULL);
        const unsigned int retained =
            (instructions < SCHEDULER_SECTION_INSTRUCTIONS) ? instructions : SCHEDULER_SECTION_INSTRUCTIONS;
        for (unsigned int at = 0u; at < retained; at += 1u)
        {
            scheduler_instruction(&s_cubin[offset + (16ULL * at)], 16ULL * at, &s_section[at]);
        }
        scheduler_follow(retained);
    }
    return 1;
}

// the stalls an operation was given, as the least, the max and the one given usually
static void scheduler_stalls(const SchedulerOperation *operation, char *text, size_t room)
{
    unsigned int least = 16u;
    unsigned int max = 0u;
    unsigned int often = 0u;
    for (unsigned int stall = 0u; stall < 16u; stall += 1u)
    {
        if (operation->stalls[stall] == 0ULL)
        {
            continue;
        }
        least = (stall < least) ? stall : least;
        max = stall;
        often = (operation->stalls[stall] > operation->stalls[often]) ? stall : often;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(text, room);
        mmgr_line_decimal(&buffer_line, least, 1u);
        mmgr_line_put(&buffer_line, " to ");
        mmgr_line_decimal(&buffer_line, max, 1u);
        mmgr_line_put(&buffer_line, ", usually ");
        mmgr_line_decimal(&buffer_line, often, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
}

static int scheduler_by_count(const void *left, const void *right)
{
    const SchedulerOperation *const one = (const SchedulerOperation *)left;
    const SchedulerOperation *const two = (const SchedulerOperation *)right;
    return (one->count < two->count)
               ? 1
               : ((one->count > two->count) ? -1 : mmgr_text_order(one->name, two->name, PLATFORM_TEXT_TO_TERMINATOR));
}

// the record: every operation by how often NVIDIA wrote it, and the readings gathered from them
int scheduler_record(const char *path)
{
    PlatformFile *const out = platform_file_open(path, "wb");
    if (out == NULL)
    {
        return 0;
    }
    {
        // the room of the sort, on the temporary tier, which goes back as the sort ends
        void *const sort_room =
            platform_cell_taken((unsigned long long)(s_operations) * (unsigned long long)(sizeof(s_operation[0])),
                                platform_place_g_program_owner, 1);
        (void)mmgr_bytes_sort(s_operation, s_operations, sizeof(s_operation[0]), scheduler_by_count, sort_room);
        platform_cell_released(sort_room, platform_place_g_program_owner);
    }
    {
        mmgr_line_put(platform_file_line(out), "# The scheduler's bits NVIDIA's compiler writes\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out),
                      "Written by `monolith_scheduler.sh` whole on every run. Every CUDA source of the tree is built "
                      "by NVIDIA's compiler for one part, each cubin's code sections are read from the ELF, each "
                      "instruction is named by our reader through the machine file and its bits 105 to 127 read "
                      "through the fields the machine file names. No listing is read and no disassembler is run.\n\n");
    }
    {
        mmgr_line_decimal(platform_file_line(out), s_cubins, 1u);
        mmgr_line_put(platform_file_line(out), " cubins, ");
        mmgr_line_decimal(platform_file_line(out), s_instructions, 1u);
        mmgr_line_put(platform_file_line(out), " instructions, ");
        mmgr_line_decimal(platform_file_line(out), s_unread, 1u);
        mmgr_line_put(platform_file_line(out), " no form reads, ");
        mmgr_line_decimal(platform_file_line(out), s_operations, 1u);
        mmgr_line_put(platform_file_line(out), " operations.\n\n");
    }
    {
        mmgr_line_put(
            platform_file_line(out),
            "An operation's soonest read is the fewest cycles, its stalls summed, between it and the first instruction "
            "that reads the register it writes, over every place it sets no write barrier and no later one blocks it. "
            "Readers waiting count the first readers of a register it writes with a write barrier where a wait on that "
            "barrier is between the two, at the reader or before it, against those where none does. A barrier counts "
            "its producers, and a wait on it blocks until all of them are back. Behind a later one counts the first "
            "readers of a register it writes with no barrier where a later instruction of the same operation sets one, "
            "waited on before the read. Behind an earlier one counts those where no later one does and an earlier "
            "instruction of the same operation sets one, waited on before the read, with the fewest cycles between "
            "that wait and the read. The machine file columns count the places whose form records a write barrier, and "
            "a read barrier, against the places a form was found.\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out),
                      "| operation | written | stalls | write barrier | read barrier | waits | soonest read | readers "
                      "waiting | behind a later one | behind an earlier one | machine file write | machine file read "
                      "|\n|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    }
    for (unsigned int at = 0u; at < s_operations; at += 1u)
    {
        const SchedulerOperation *const operation = &s_operation[at];
        char stalls[64];
        char soonest[64];
        scheduler_stalls(operation, stalls, sizeof(stalls));
        if (operation->timed_reads == 0ULL)
        {
            VerbaLinea buffer_line = mmgr_line_over(soonest, sizeof(soonest));
            mmgr_line_put(&buffer_line, "-");
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(soonest, sizeof(soonest));
            mmgr_line_decimal(&buffer_line, operation->soonest_read, 1u);
            mmgr_line_put(&buffer_line, " over ");
            mmgr_line_decimal(&buffer_line, operation->timed_reads, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        char earlier[64];
        if (operation->readers_behind_earlier == 0ULL)
        {
            VerbaLinea buffer_line = mmgr_line_over(earlier, sizeof(earlier));
            mmgr_line_put(&buffer_line, "0");
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(earlier, sizeof(earlier));
            mmgr_line_decimal(&buffer_line, operation->readers_behind_earlier, 1u);
            mmgr_line_put(&buffer_line, ", ");
            mmgr_line_decimal(&buffer_line, operation->soonest_after_earlier, 1u);
            mmgr_line_put(&buffer_line, " after the wait");
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            mmgr_line_put(platform_file_line(out), "| `");
            mmgr_line_put(platform_file_line(out), operation->name);
            mmgr_line_put(platform_file_line(out), "` | ");
            mmgr_line_decimal(platform_file_line(out), operation->count, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_put(platform_file_line(out), stalls);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->write_barriers, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->read_barriers, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->waits, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_put(platform_file_line(out), soonest);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->readers_waiting, 1u);
            mmgr_line_put(platform_file_line(out), " of ");
            mmgr_line_decimal(platform_file_line(out), operation->readers_waiting + operation->readers_not_waiting, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->readers_behind_later, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_put(platform_file_line(out), earlier);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->machine_barriers, 1u);
            mmgr_line_put(platform_file_line(out), " of ");
            mmgr_line_decimal(platform_file_line(out), operation->machine_formed, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), operation->machine_read_barriers, 1u);
            mmgr_line_put(platform_file_line(out), " of ");
            mmgr_line_decimal(platform_file_line(out), operation->machine_formed, 1u);
            mmgr_line_put(platform_file_line(out), " |\n");
        }
    }
    platform_file_close(out);
    return 1;
}
