// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// instructions.c: the instructions: a word and a field, an operation, predicates alone, control, what an instruction
// reads and whether a register, and an instruction read whole
#include "../../../../../../platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "scheduler_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int instructions_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

SassMachine s_machine;
unsigned char s_cubin[SCHEDULER_CUBIN_BYTES];
SchedulerInstruction s_section[SCHEDULER_SECTION_INSTRUCTIONS];
SchedulerOperation s_operation[SCHEDULER_OPERATIONS];
unsigned int s_operations;
unsigned long long s_instructions;
unsigned long long s_unread;
unsigned int s_cubins;

unsigned long long scheduler_word(const unsigned char *bytes, unsigned int width)
{
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < width; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

// the scheduler's field beginning at `first`, `bits` wide, of the high word `high`
static unsigned int scheduler_field(unsigned long long high, unsigned int first, unsigned int bits)
{
    return (unsigned int)((high >> (first - 64u)) & ((1ULL << bits) - 1ULL));
}

// the operation named `name`, stored in s_operation where it is not yet: its number, or SCHEDULER_OPERATIONS where the
// table is full
static unsigned int scheduler_operation(const char *name)
{
    for (unsigned int at = 0u; at < s_operations; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_operation[at].name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
        {
            return at;
        }
    }
    if (s_operations == SCHEDULER_OPERATIONS)
    {
        return SCHEDULER_OPERATIONS;
    }
    SchedulerOperation *const made = &s_operation[s_operations];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = made, .bytes = sizeof(*made), .val = 0);
    {
        VerbaLinea buffer_line = mmgr_line_over(made->name, sizeof(made->name));
        mmgr_line_put(&buffer_line, name);
        (void)mmgr_line_finish(&buffer_line);
    }
    made->soonest_read = UINT_MAX;
    made->soonest_after_earlier = UINT_MAX;
    s_operations += 1u;
    return s_operations - 1u;
}

// 1 where the operation writes predicates alone: a compare that sets them and the logic over them, whose register
// operands are each read
static int scheduler_predicates_only(const char *operation)
{
    static const char *const setters[] = {"ISETP", "FSETP", "DSETP", "HSETP2", "PSETP", "PLOP3", "UISETP", "UPLOP3"};
    for (unsigned int at = 0u; at < (sizeof(setters) / sizeof(setters[0])); at += 1u)
    {
        if (instructions_text_order(operation, setters[at],
                                    (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = setters[at],
                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// 1 where the operation leaves the straight line: a branch, a call, a return, an exit or a wait on other lanes, past
// which a reader is not looked for
static int scheduler_control(const char *operation)
{
    static const char *const leaves[] = {"BRA",   "BRX",   "JMP",      "JMX", "CALL", "RET", "EXIT",  "BSSY",
                                         "BSYNC", "BREAK", "WARPSYNC", "BAR", "KILL", "BPT", "YIELD", "NANOSLEEP"};
    for (unsigned int at = 0u; at < (sizeof(leaves) / sizeof(leaves[0])); at += 1u)
    {
        if (instructions_text_order(operation, leaves[at],
                                    (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = leaves[at],
                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// every register `text` names, R then digits, added to the instruction's reads; a register named with .64 adds the
// one after it too, the second of its pair
static void scheduler_reads(const char *text, SchedulerInstruction *instruction)
{
    // each R, reached from the one before it
    for (const char *walk = text + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "R");
         *walk != '\0'; walk += 1 + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk + 1,
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "R"))
    {
        // Explicit cast reads the byte as its code point
        const int starts = EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)walk[1]) &&
                           ((walk == text) || ((walk[-1] != 'U') && (walk[-1] != 'S')));
        if (!starts)
        {
            continue;
        }
        const char *after = NULL;
        const unsigned long number =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = walk + 1, .end = &after, .base = 10);
        const unsigned int pair = (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = after,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".64", .ci = EMBED_FALSE))
                                      ? 2u
                                      : 1u;
        for (unsigned int add = 0u; (add < pair) && (instruction->reads < SCHEDULER_READS); add += 1u)
        {
            instruction->read[instruction->reads] = (unsigned int)number + add;
            instruction->reads += 1u;
        }
    }
}

// 1 where `instruction` reads register `number`
int scheduler_reads_register(const SchedulerInstruction *instruction, unsigned int number)
{
    for (unsigned int at = 0u; at < instruction->reads; at += 1u)
    {
        if (instruction->read[at] == number)
        {
            return 1;
        }
    }
    return 0;
}

// the instruction at `bytes` read into `instruction` and counted against its operation
void scheduler_instruction(const unsigned char *bytes, unsigned long long address, SchedulerInstruction *instruction)
{
    const unsigned long long low = scheduler_word(bytes, 8u);
    const unsigned long long high = scheduler_word(&bytes[8], 8u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = instruction, .bytes = sizeof(*instruction), .val = 0);
    instruction->stall = scheduler_field(high, SASS_STALL_FIRST, 4u);
    instruction->write_barrier = scheduler_field(high, SASS_WRITE_BARRIER_FIRST, 3u);
    instruction->read_barrier = scheduler_field(high, SASS_READ_BARRIER_FIRST, 3u);
    instruction->wait = scheduler_field(high, SASS_WAIT_FIRST, 6u);
    instruction->writes = SCHEDULER_NO_REGISTER;
    instruction->control = 1u;
    char text[256];
    char name[SASS_MACHINE_TOKEN];
    SassInstructionParts parts;
    const int read = sass_encoding_read(&s_machine, low, high, address, text, sizeof(text));
    if (read)
    {
        sass_instruction_read(text, &parts);
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, parts.operation);
            (void)mmgr_line_finish(&buffer_line);
        }
    }
    else
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "no form, key 0x");
            mmgr_line_hex(&buffer_line, low & 0xfffULL, 3u);
            (void)mmgr_line_finish(&buffer_line);
        }
        s_unread += 1ULL;
    }
    s_instructions += 1ULL;
    instruction->operation = scheduler_operation(name);
    if (instruction->operation == SCHEDULER_OPERATIONS)
    {
        return;
    }
    SchedulerOperation *const operation = &s_operation[instruction->operation];
    operation->count += 1ULL;
    operation->stalls[instruction->stall] += 1ULL;
    operation->yields += (unsigned long long)scheduler_field(high, SASS_YIELD_FIRST, 1u);
    operation->write_barriers += (instruction->write_barrier != SASS_BARRIER_NONE) ? 1ULL : 0ULL;
    operation->read_barriers += (instruction->read_barrier != SASS_BARRIER_NONE) ? 1ULL : 0ULL;
    operation->waits += (instruction->wait != 0u) ? 1ULL : 0ULL;
    if (!read)
    {
        return;
    }
    const SassForm *const form = sass_machine_form(&s_machine, &parts);
    if (form != NULL)
    {
        operation->machine_formed += 1ULL;
        operation->machine_barriers += sass_barrier_set(form->high, SASS_WRITE_BARRIER_FIRST) ? 1ULL : 0ULL;
        operation->machine_read_barriers += sass_barrier_set(form->high, SASS_READ_BARRIER_FIRST) ? 1ULL : 0ULL;
    }
    instruction->control = (unsigned int)scheduler_control(parts.operation);
    // the outputs lead: any predicates first, then the register written, where the operation writes one. A compare or
    // a logic over predicates writes predicates alone, and an operation that leaves the straight line reads its
    // register
    unsigned int written = 0u;
    while ((written < parts.operands) && (parts.type[written] == SASS_OPERAND_PREDICATE))
    {
        written += 1u;
    }
    const int writes_register = (written < parts.operands) && (parts.type[written] == SASS_OPERAND_REGISTER) &&
                                !instruction->control && !scheduler_predicates_only(parts.operation);
    if (writes_register && (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[written],
                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "RZ", .ci = EMBED_FALSE)))
    {
        instruction->writes = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                       .src = parts.operand[written] + 1, .end = NULL, .base = 10);
    }
    for (unsigned int at = writes_register ? (written + 1u) : 0u; at < parts.operands; at += 1u)
    {
        scheduler_reads(parts.operand[at], instruction);
    }
}
