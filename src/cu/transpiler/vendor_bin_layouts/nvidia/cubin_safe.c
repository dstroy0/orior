// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_safe.c: each instruction a kernel reaches read with our own reader and held to the rules in cubin_safe.h
#include "cubin_safe.h"

#include "cubin_write.h"
#include "sass_assemble.h"

#include <stdio.h>
#include <string.h>

// the four bits of the stall and the six of the wait, counted from the high word's first bit
#define CUBIN_STALL_SHIFT (SASS_STALL_FIRST - 64u)
#define CUBIN_WAIT_SHIFT (SASS_WAIT_FIRST - 64u)
// the most code sections one image is read for: a kernel and the functions it calls
#define CUBIN_SAFE_SECTIONS 64u
// where a register sits in an instruction, read whole however much of it a run covered: the first bit of each of the
// four fixed eight-bit register fields (sass_assemble.h), the width of one, and RZ, the zero register every kernel
// holds whatever count it allots
#define CUBIN_REGISTER_FIELDS 4u
#define CUBIN_REGISTER_WIDTH 8u
#define CUBIN_REGISTER_ZERO 255u

// the register the operand whose run begins at `run_first` names, its whole fixed eight-bit field read: the number in
// the field that holds the bit, or RZ where the run lies in none of them, which is then left unbounded
static unsigned int cubin_register_of(unsigned long long low, unsigned long long high, unsigned int run_first)
{
    static const unsigned int field[CUBIN_REGISTER_FIELDS] = {16u, 24u, 32u, 64u};
    for (unsigned int at = 0u; at < CUBIN_REGISTER_FIELDS; at += 1u)
    {
        if ((run_first >= field[at]) && (run_first < (field[at] + CUBIN_REGISTER_WIDTH)))
        {
            unsigned int value = 0u;
            for (unsigned int bit = 0u; bit < CUBIN_REGISTER_WIDTH; bit += 1u)
            {
                const unsigned int whole = field[at] + bit;
                const unsigned long long word = (whole < 64u) ? low : high;
                value |= (unsigned int)((word >> (whole % 64u)) & 1ull) << bit;
            }
            return value;
        }
    }
    return CUBIN_REGISTER_ZERO;
}

// 1 where an operand of `form` names a register the kernel never allotted: one at `registers` or above, where that
// count is known, or past the part's ceiling register_last, RZ always excepted. The form says which operands are
// registers and where they sit (sass_machine.h); the instruction `low` and `high` holds their numbers
static int cubin_register_out_of_range(const SassMachine *machine, const SassForm *form, unsigned long long low,
                                       unsigned long long high, unsigned int registers)
{
    for (unsigned int run = 0u; run < form->runs; run += 1u)
    {
        const unsigned int operand = form->run[run].operand;
        if ((operand >= form->operands) || (form->kind[operand] != SASS_OPERAND_REGISTER))
        {
            continue;
        }
        const unsigned int value = cubin_register_of(low, high, form->run[run].first);
        if (value == CUBIN_REGISTER_ZERO)
        {
            continue;
        }
        if ((registers != 0u) && (value >= registers))
        {
            return 1;
        }
        if ((machine->register_last != SASS_MACHINE_UNANSWERED) && (value > machine->register_last))
        {
            return 1;
        }
    }
    return 0;
}

// a form of `machine` under the operation key of `low`, for an instruction no form holds whole: the first found, whose
// register fields are the key's own, NULL where the key holds none
static const SassForm *cubin_key_form(const SassMachine *machine, unsigned long long low)
{
    const unsigned long long key = low & SASS_OPERATION_MASK;
    for (unsigned int at = 0u; at < machine->forms; at += 1u)
    {
        if ((machine->form[at].low & SASS_OPERATION_MASK) == key)
        {
            return &machine->form[at];
        }
    }
    return NULL;
}

static unsigned long long cubin_word(const unsigned char *bytes)
{
    unsigned long long value = 0ull;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

// 1 where `machine` holds a form under the operation key of `low` and none of its forms there transfers control or
// waits
static int cubin_key_straight(const SassMachine *machine, unsigned long long low)
{
    const unsigned long long key = low & SASS_OPERATION_MASK;
    int known = 0;
    for (unsigned int at = 0u; at < machine->forms; at += 1u)
    {
        if ((machine->form[at].low & SASS_OPERATION_MASK) != key)
        {
            continue;
        }
        if (sass_operation_control_or_wait(machine->form[at].operation))
        {
            return 0;
        }
        known = 1;
    }
    return known;
}

// 1 where `parts` is an EXIT, its modifiers whatever they are
static int cubin_exit(const SassInstructionParts *parts)
{
    return (strncmp(parts->operation, "EXIT", 4u) == 0) && ((parts->operation[4] == '\0') || (parts->operation[4] == '.'));
}

// 1 where the EXIT `parts` is taken by every thread that reaches it: no guard, and every operand it prints PT with
// no mark, the predicate always true. `EXIT !PT` is never taken and `EXIT P6` only where P6 holds
static int cubin_exit_ends(const SassInstructionParts *parts)
{
    if (parts->guard[0] != '\0')
    {
        return 0;
    }
    for (unsigned int operand = 0u; operand < parts->operands; operand += 1u)
    {
        if ((strcmp(parts->operand[operand], "PT") != 0) || (parts->mark[operand] != SASS_MARK_NONE))
        {
            return 0;
        }
    }
    return 1;
}

unsigned int cubin_safe(const SassMachine *machine, const unsigned char *code, unsigned long long code_size,
                        unsigned int registers, unsigned long long *at)
{
    *at = 0ull;
    if ((code_size == 0ull) || ((code_size % 16ull) != 0ull))
    {
        return CUBIN_SAFE_SIZE;
    }
    unsigned int unheld = 0u;
    for (unsigned long long place = 0ull; place < code_size; place += 16ull)
    {
        *at = place;
        const unsigned long long low = cubin_word(&code[place]);
        const unsigned long long high = cubin_word(&code[place + 8u]);
        const unsigned long long stall = (high >> CUBIN_STALL_SHIFT) & 0xfull;
        // the vendor's verdict, where one is loaded (a launch, never a dry read): an encoding its reader called illegal
        // is held off the part, whatever our own gate reads it as
        if (cubin_safe_verdict_ready() && cubin_safe_verdict_holds(low, high))
        {
            return CUBIN_SAFE_HELD;
        }
        if (((high >> CUBIN_WAIT_SHIFT) & 0x3full) != SASS_WAIT_EVERY)
        {
            return CUBIN_SAFE_WAIT;
        }
        char text[256];
        if (!sass_encoding_read(machine, low, high, place, text, sizeof(text)))
        {
            // an instruction no form holds names no operation whose soonest read is measured, and stalls the longest
            if (stall != SASS_STALL_LONGEST)
            {
                return CUBIN_SAFE_STALL;
            }
            if (!cubin_key_straight(machine, low))
            {
                return CUBIN_SAFE_KEY;
            }
            unheld += 1u;
            if (unheld > 1u)
            {
                return CUBIN_SAFE_UNHELD;
            }
            // a form under its key holds the register fields the key's instructions share: its operands bound this
            // one's, which no form holds whole (rule 6)
            const SassForm *const key_form = cubin_key_form(machine, low);
            if ((key_form != NULL) && cubin_register_out_of_range(machine, key_form, low, high, registers))
            {
                return CUBIN_SAFE_REGISTER;
            }
            continue;
        }
        SassInstructionParts parts;
        sass_instruction_read(text, &parts);
        unsigned int soonest = SASS_STALL_LONGEST;
        const unsigned int schedule = sass_operation_schedule(machine, parts.operation, &soonest);
        // A barrier set by an operation whose result is back in a fixed count of cycles is never released, and the
        // next wait on all six never ends. An operation releases one where its schedule is late or a store, or where
        // the encoding the system wrote its form with sets one
        const SassForm *const form = sass_machine_form(machine, &parts);
        const int releases = (schedule != SASS_SCHEDULE_FIXED) ||
                             ((form != NULL) && (sass_barrier_set(form->high, SASS_WRITE_BARRIER_FIRST) ||
                                                 sass_barrier_set(form->high, SASS_READ_BARRIER_FIRST)));
        if (!releases &&
            (sass_barrier_set(high, SASS_WRITE_BARRIER_FIRST) || sass_barrier_set(high, SASS_READ_BARRIER_FIRST)))
        {
            return CUBIN_SAFE_BARRIER;
        }
        // every register this instruction names is one the kernel allotted, RZ excepted (rule 6)
        if ((form != NULL) && cubin_register_out_of_range(machine, form, low, high, registers))
        {
            return CUBIN_SAFE_REGISTER;
        }
        if (cubin_exit(&parts))
        {
            // an EXIT with no guard and no predicate of its own but PT ends every thread that reaches it, and nothing
            // past it runs
            if (cubin_exit_ends(&parts))
            {
                return CUBIN_SAFE;
            }
            continue;
        }
        if (sass_operation_control_or_wait(parts.operation))
        {
            return CUBIN_SAFE_CONTROL;
        }
    }
    return CUBIN_SAFE_EXIT;
}

unsigned int cubin_safe_image(const SassMachine *machine, const unsigned char *image, unsigned long long size,
                              unsigned int registers, unsigned long long *at)
{
    unsigned long long offsets[CUBIN_SAFE_SECTIONS];
    unsigned long long sizes[CUBIN_SAFE_SECTIONS];
    const unsigned int sections = cubin_code_sections(image, size, offsets, sizes, CUBIN_SAFE_SECTIONS);
    *at = 0ull;
    if (sections == 0u)
    {
        return CUBIN_SAFE_SIZE;
    }
    for (unsigned int section = 0u; section < sections; section += 1u)
    {
        unsigned long long inside = 0ull;
        const unsigned int verdict = cubin_safe(machine, &image[offsets[section]], sizes[section], registers, &inside);
        if (verdict != CUBIN_SAFE)
        {
            *at = offsets[section] + inside;
            return verdict;
        }
    }
    return CUBIN_SAFE;
}

unsigned int cubin_safe_launch(int status, unsigned long long waited)
{
    if (status == CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT)
    {
        return CUBIN_SAFE_LAUNCH_TIMEOUT;
    }
    return ((status == CUBIN_SAFE_STATUS_NOT_READY) && (waited > CUBIN_SAFE_LAUNCH_BOUND)) ? CUBIN_SAFE_LAUNCH_TIMEOUT
                                                                                            : CUBIN_SAFE;
}

// 1 where `status` is a driver fault that corrupts the context: the part faulted on the launch, the error is sticky,
// and every launch after it in the process is invalid
static int cubin_safe_status_fault(int status)
{
    switch (status)
    {
    case CUBIN_SAFE_STATUS_ILLEGAL_ADDRESS:
    case CUBIN_SAFE_STATUS_HARDWARE_STACK:
    case CUBIN_SAFE_STATUS_ILLEGAL_INSTRUCTION:
    case CUBIN_SAFE_STATUS_MISALIGNED_ADDRESS:
    case CUBIN_SAFE_STATUS_INVALID_ADDRESS_SPACE:
    case CUBIN_SAFE_STATUS_INVALID_PC:
    case CUBIN_SAFE_STATUS_LAUNCH_FAILED:
        return 1;
    default:
        return 0;
    }
}

unsigned int cubin_safe_fault(int part_faulted, int status)
{
    return (part_faulted || cubin_safe_status_fault(status)) ? CUBIN_SAFE_LAUNCH_FAULT : CUBIN_SAFE;
}

// the vendor's verdict, the held list the cross writes: the most encodings it holds and the longest line read, and the
// bits kept of the high word, the scheduler's own left out since whatever reaches the part sets them
#define CUBIN_SAFE_HELD_MOST 4096u
#define CUBIN_SAFE_HELD_LINE 1024u
#define CUBIN_SAFE_SCHED_KEPT_HIGH 0x000001ffffffffffull
static unsigned long long s_held_low[CUBIN_SAFE_HELD_MOST];
static unsigned long long s_held_high[CUBIN_SAFE_HELD_MOST];
static unsigned int s_held_count = 0u;
static int s_verdict_ready = 0;

int cubin_safe_verdict_load(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    s_held_count = 0u;
    char line[CUBIN_SAFE_HELD_LINE];
    while ((s_held_count < CUBIN_SAFE_HELD_MOST) && (fgets(line, sizeof(line), file) != NULL))
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if (sscanf(line, "%llx %llx", &low, &high) == 2)
        {
            s_held_low[s_held_count] = low;
            s_held_high[s_held_count] = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
            s_held_count += 1u;
        }
    }
    fclose(file);
    s_verdict_ready = 1;
    return 1;
}

int cubin_safe_verdict_ready(void)
{
    return s_verdict_ready;
}

int cubin_safe_verdict_holds(unsigned long long low, unsigned long long high)
{
    const unsigned long long kept = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
    for (unsigned int at = 0u; at < s_held_count; at += 1u)
    {
        if ((s_held_low[at] == low) && (s_held_high[at] == kept))
        {
            return 1;
        }
    }
    return 0;
}

const char *cubin_safe_name(unsigned int verdict)
{
    static const char *const s_names[] = {"safe",
                                          "not a whole count of instructions",
                                          "an instruction no form holds, stalled short of the longest",
                                          "a wait short of all six barriers",
                                          "a control transfer or a wait",
                                          "an operation key unknown or holding a control transfer or a wait",
                                          "a second instruction no form holds",
                                          "no EXIT without a guard",
                                          "a barrier set by an operation whose result is back in a measured count",
                                          "a launch the driver timed out, or not back inside the bound",
                                          "a launch that faulted the part, its context left corrupt",
                                          "a register the kernel did not allot",
                                          "an encoding the vendor held off the part"};
    return (verdict < (sizeof(s_names) / sizeof(s_names[0]))) ? s_names[verdict] : "unknown";
}
