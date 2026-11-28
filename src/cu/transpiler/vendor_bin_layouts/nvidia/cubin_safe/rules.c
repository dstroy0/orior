// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rules.c: the rules: a register read and blocked out of range, the key's form and the key straight, the exit and where
// it ends, the code gated safe, the image, the launch, and a fault's status read
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cubin_safe_internal.h"

// the register that the operand whose run starts at `run_first` names, read from its whole fixed eight-bit field. It
// is the number in the field that holds the bit. It is RZ where the run lies in none of them, and then has no bound
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
                value |= (unsigned int)((word >> (whole % 64u)) & 1ULL) << bit;
            }
            return value;
        }
    }
    return CUBIN_REGISTER_ZERO;
}

// 1 where an operand of `form` names a register that the kernel never allotted, RZ always excepted. Where the call
// knows `registers`, that is one past `R(registers - register_past)`, the answer of the part to how far the code of a
// container reaches. Where the part did not answer it, it is every one. It is also one past the ceiling of the part,
// register_last. The form tells which operands are registers and where they sit (sass_machine.h). The instruction
// `low` and `high` encodes their numbers
static int cubin_register_out_of_range(const SassMachine *machine, const SassForm *form, unsigned long long low,
                                       unsigned long long high, unsigned int registers)
{
    for (unsigned int run = 0u; run < form->runs; run += 1u)
    {
        const unsigned int operand = form->run[run].operand;
        if ((operand >= form->operands) || (form->type[operand] != SASS_OPERAND_REGISTER))
        {
            continue;
        }
        const unsigned int value = cubin_register_of(low, high, form->run[run].first);
        if (value == CUBIN_REGISTER_ZERO)
        {
            continue;
        }
        // a register past the answered reach faults the part with a register out of range. One can fault the part
        // where the part did not answer the reach. A container that declares d reaches no further than `R(d - 1)`,
        // whatever a machine holds
        const unsigned int past = (machine->register_past == 0u) ? 1u : machine->register_past;
        if ((registers != 0u) && ((machine->register_past == SASS_MACHINE_UNANSWERED) || ((value + past) > registers)))
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

// a form of `machine` under the operation key of `low`, for an instruction that no form matches whole. It is the first
// that the call finds, whose register fields are the own fields of the key, or NULL where the key has none
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
    unsigned long long value = 0ULL;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

// 1 where `machine` has a form under the operation key of `low` and none of its forms there transfers control or
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
    return (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = parts->operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "EXIT", .ci = EMBED_FALSE)) &&
           ((parts->operation[4] == '\0') || (parts->operation[4] == '.'));
}

// 1 where every thread that reaches the EXIT `parts` takes it. It has no guard, and every operand that it prints is PT
// with no mark, the predicate that is always true. No thread takes `EXIT !PT`, and a thread takes `EXIT P6` only where
// P6 is true
static int cubin_exit_ends(const SassInstructionParts *parts)
{
    if (parts->guard[0] != '\0')
    {
        return 0;
    }
    for (unsigned int operand = 0u; operand < parts->operands; operand += 1u)
    {
        if ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts->operand[operand], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = "PT", .ci = EMBED_FALSE)) ||
            (parts->mark[operand] != SASS_MARK_NONE))
        {
            return 0;
        }
    }
    return 1;
}

// the size of the section of the constant bank beside the code section under the gate, or 0 where the gate does not
// know it. The call cubin_safe_image sets it for each code section from the constant sections of the container, in
// their order
static unsigned long long s_bank_size;

unsigned int cubin_safe(const SassMachine *machine, const unsigned char *code, unsigned long long code_size,
                        unsigned int registers, unsigned long long *at)
{
    *at = 0ULL;
    if ((code_size == 0ULL) || ((code_size % 16ULL) != 0ULL))
    {
        return CUBIN_SAFE_SIZE;
    }
    unsigned int ungated = 0u;
    for (unsigned long long place = 0ULL; place < code_size; place += 16ULL)
    {
        *at = place;
        const unsigned long long low = cubin_word(&code[place]);
        const unsigned long long high = cubin_word(&code[place + 8u]);
        const unsigned long long stall = (high >> CUBIN_STALL_SHIFT) & 0xfULL;
        // the verdict of the vendor, where the gate has one (a launch, never a dry read). The gate blocks an encoding
        // that its reader called illegal from the part, whatever our own gate reads it as
        if (cubin_safe_verdict_ready() && cubin_safe_verdict_gated(low, high))
        {
            return CUBIN_SAFE_GATED;
        }
        if (((high >> CUBIN_WAIT_SHIFT) & 0x3fULL) != SASS_WAIT_EVERY)
        {
            return CUBIN_SAFE_WAIT;
        }
        // a read of the constant bank that the part would fault on. The gate reads it through the own instruction set
        // of the vendor, where the program pulled the tables of the part (rule 13)
        if (cubin_safe_tables_ready() && cubin_constant_out_of_bank(cubin_safe_tables(), low, high, s_bank_size))
        {
            return CUBIN_SAFE_CONSTANT;
        }
        // the gate blocks an operation that the vendor instruction set names as a control transfer or a wait. It does
        // so whatever name the form of the machine file gives its encoding (rules 2 and 3)
        if (cubin_safe_tables_ready() && cubin_class_control_or_wait(cubin_safe_tables(), low, high))
        {
            return CUBIN_SAFE_CONTROL;
        }
        // the gate blocks an encoding that the own conditions of its class name illegal before the part faults on it
        // (rule 14). Such an encoding has a register out of range, or one misaligned for its width
        if (cubin_safe_tables_ready() && cubin_class_illegal(cubin_safe_tables(), low, high))
        {
            return CUBIN_SAFE_ILLEGAL;
        }
        char text[256];
        if (!sass_encoding_read(machine, low, high, place, text, sizeof(text)))
        {
            // an instruction that no form matches names no operation with a measured soonest read, and stalls the
            // longest
            if (stall != SASS_STALL_MAX_LEN)
            {
                return CUBIN_SAFE_STALL;
            }
            // the gate puts an encoding that a conference admitted past the key alone, as the lesson gives it. It
            // counts as one that a form matches. The cross read it, and the own word of the airlock is for a form
            const int admitted = cubin_safe_admitted(low, high);
            if (!cubin_key_straight(machine, low) && !admitted)
            {
                return CUBIN_SAFE_KEY;
            }
            ungated += admitted ? 0u : 1u;
            if (ungated > 1u)
            {
                return CUBIN_SAFE_UNGATED;
            }
            // a form under its key defines the register fields the key's instructions share: its operands bound this
            // one's, which no form matches whole (rule 6)
            const SassForm *const key_form = cubin_key_form(machine, low);
            if ((key_form != NULL) && cubin_register_out_of_range(machine, key_form, low, high, registers))
            {
                return CUBIN_SAFE_REGISTER;
            }
            continue;
        }
        SassInstructionParts parts;
        sass_instruction_read(text, &parts);
        unsigned int soonest = SASS_STALL_MAX_LEN;
        const unsigned int schedule = sass_operation_schedule(machine, parts.operation, &soonest);
        // Nothing releases a barrier that an operation sets where its result is back in a fixed count of cycles. Then
        // the next wait on all six never ends. An operation releases one where its schedule is late or a store. It also
        // releases one where the encoding that the system wrote its form with sets one
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
    unsigned long long bank_offsets[CUBIN_SAFE_SECTIONS];
    unsigned long long bank_sizes[CUBIN_SAFE_SECTIONS];
    const unsigned int sections = cubin_code_sections(image, size, offsets, sizes, CUBIN_SAFE_SECTIONS);
    // the sections of the constant bank are in the order of the code sections, one for each kernel. A container with
    // fewer leaves the size of the bank unknown for the code sections past them
    const unsigned int banks = cubin_constant_sections(image, size, bank_offsets, bank_sizes, CUBIN_SAFE_SECTIONS);
    *at = 0ULL;
    if (sections == 0u)
    {
        return CUBIN_SAFE_SIZE;
    }
    for (unsigned int section = 0u; section < sections; section += 1u)
    {
        unsigned long long inside = 0ULL;
        s_bank_size = (section < banks) ? bank_sizes[section] : 0ULL;
        const unsigned int verdict = cubin_safe(machine, &image[offsets[section]], sizes[section], registers, &inside);
        s_bank_size = 0ULL;
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

// 1 where `status` is a driver fault that corrupts the context. The part faulted on the launch, and the error is
// sticky. Every launch after it in the process is invalid
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
