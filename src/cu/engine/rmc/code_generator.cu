// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "code_generator.h"
#include "codegen_core.h"
#include "embed_atomic.h"
#include "memoria_operor/memoria_operor.h"

#include <stddef.h>

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int code_generator_text_order(const char *left, const char *right, size_t cap)
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

static const RulesetName s_opcode_names[OPCODE_COUNT] = {OPCODES(OPCODE_WRITTEN)};

static const RulesetName s_regclass_names[REGCLASS_COUNT] = {REGCLASSES(REGCLASS_WRITTEN)};

static const RulesetName s_physreg_names[PHYSREG_COUNT] = {PHYSREGS(PHYSREG_WRITTEN)};

// The record program as a register lane, in any language whose ruleset writes the lane's forms: each step
// unrolled at its widths into straight-line text over registers the lane uses itself. The
// file is a bank by place and its signs another, as key_schedule positioned them out; the record's words are a bank,
// arranged by each put and each stored once the last put that positions it out has run; the atoms' words are a bank,
// each loaded at its first reader. What a compiler would loop over a register's limbs is written out here limb by limb
// from the program's own widths. The code generator writes no text of its own: every line is a form of the language's
// ruleset, and every register a written form of its banks and fixed registers. The code generator decides what a step
// does and the ruleset how the language writes it. What each step does, and the lane's opening, close and split into
// states, is decided in codegen_core.h, which the device compiles as well. Here the forms it decides are written in the
// ruleset and arranged into the text in its order. The steps that loop on their values are loops of the same forms,
// each body over registers of fixed widths so no limb is named by a value the lane computes: the gcd, the golden
// ladder, a division by more than one limb, and a product too wide to unroll. A loop is a label and a branch back to it
// on a predicate, which a language that clocks the lane splits into states that go back. The lane calls nothing and
// takes no shared memory. Every step is the interpreter's arithmetic limb for limb, branch-free where the interpreter
// branches on a sign or a borrow, and CYCLE_RECORD_CHECK=1 checks the two against each other. After the lane comes the
// program resident that runs its launch's lanes (program_unit), the kernel the record machine launches or a circuit's
// top. The text is the whole program, and nothing hand-written is linked with it.

// Where each bank begins in the register file, and how many registers one of its own takes. A language whose
// registers are virtual gives every bank a namespace of its own and leaves this zero; a language with one register
// file has every bank in that file, and a bank numbered from 0 like every other collides with them. The banks are
// positioned end to end from the counts the lane declares, and a bank whose registers are 64 bits takes two of the
// file's for each of its own.
struct RegisterFile
{
    unsigned int at[REGCLASS_COUNT];
    unsigned int takes[REGCLASS_COUNT];
};

// register `at` of a bank, as the ruleset writes it, positioned into the file by `file` where the language has one,
// onto `name`
static void code_generator_register(const Ruleset *rules, const RegisterFile *file, unsigned int bank, unsigned int at,
                                    VerbaLinea *name)
{
    const InstrTemplate *const form = &rules->banks[bank];
    const unsigned int number = file->at[bank] + (file->takes[bank] * at);
    mmgr_line_put(name, form->pieces[0]);
    for (unsigned int index = 0u; index < form->placeholder_count; index += 1u)
    {
        mmgr_line_decimal(name, number, 1u);
        mmgr_line_put(name, form->pieces[index + 1u]);
    }
}

// An argument the core decided, as the ruleset writes it, onto `text`. A register goes by its bank and a fixed
// register by its name. A number goes in decimal, a signed one with its minus
static void code_generator_argument(const Ruleset *rules, const RegisterFile *file, const MachineOperand &argument,
                                    VerbaLinea *text)
{
    if (argument.type == OPERAND_REGISTER)
    {
        code_generator_register(rules, file, argument.which, argument.number, text);
        return;
    }
    if (argument.type == OPERAND_PHYSREG)
    {
        mmgr_line_put(text, rules->fixed[argument.which]);
        return;
    }
    // a signed number is stored as its two's complement word
    if ((argument.type == OPERAND_SIGNED) && (argument.number >= 0x80000000u))
    {
        mmgr_line_ch(text, '-');
        mmgr_line_decimal(text, 0u - argument.number, 1u);
        return;
    }
    mmgr_line_decimal(text, argument.number, 1u);
}

// how many registers of the file one register of a bank takes: two for a bank of 64-bit registers, which the file
// is valid as a pair, and one for the rest. The bank of immediates is a word's value written into a form and none of
// the file: it is never moved
static unsigned int code_generator_takes(unsigned int bank)
{
    return ((bank == REGCLASS_WIDE) || (bank == REGCLASS_MEMBER)) ? 2u : 1u;
}

// The banks positioned into one register file, from the counts the lane declared: each bank begins where the one before
// it ended, in the order the declarations come. A language whose banks are namespaces of their own leaves every bank at
// 0, which writes each register by its own number. The predicates are a file of their own on every
// language that has them, and are not positioned with the rest. 0 where the banks run past what the file contains, the
// lane then written by nobody: every register from there on would be one the language has already pinned
static int code_generator_file(const CodeGenerator *generator, const MachineInstr *items, unsigned long long count,
                               RegisterFile *file)
{
    for (unsigned int bank = 0u; bank < REGCLASS_COUNT; bank += 1u)
    {
        file->at[bank] = 0u;
        file->takes[bank] = 1u;
    }
    const unsigned int file_size = generator->register_file_size();
    if (file_size == 0u)
    {
        return 1;
    }
    // each bank's count is the argument of the form that declares it, in the order the lane's declarations are written
    const unsigned int declares[] = {OPCODE_DECLARE_FILE,   OPCODE_DECLARE_SIGNS,       OPCODE_DECLARE_OUT,
                                     OPCODE_DECLARE_ATOMS,  OPCODE_DECLARE_TEMPORARIES, OPCODE_DECLARE_WIDES,
                                     OPCODE_DECLARE_MEMBERS};
    const unsigned int banks[] = {REGCLASS_FILE,      REGCLASS_SIGN, REGCLASS_OUT,   REGCLASS_ATOM,
                                  REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_MEMBER};
    unsigned int at = 0u;
    for (unsigned int which = 0u; which < (sizeof(banks) / sizeof(banks[0])); which += 1u)
    {
        unsigned int declared = 0u;
        for (unsigned long long item = 0ULL; item < count; item += 1ULL)
        {
            declared = (items[item].form == declares[which]) ? items[item].arguments[0].number : declared;
        }
        file->at[banks[which]] = at;
        file->takes[banks[which]] = code_generator_takes(banks[which]);
        at += declared * file->takes[banks[which]];
    }
    return (at <= file_size) ? 1 : 0;
}

// the most arguments a form the core decides takes, and the bytes of one written argument, its terminator among them
#define CODE_GENERATOR_ARGUMENTS PROGRAM_UNIT_PARAMETERS
#define CODE_GENERATOR_ARGUMENT_BYTES 64u

static_assert((CODE_GENERATOR_ARGUMENTS >= 6u) && (CODE_GENERATOR_ARGUMENTS >= MACHINE_INSTR_OPERANDS),
              "code_generator: a note takes six arguments, and an item its operands");

// the scratch of one form as the core left each bank when it decided the form, and where the lane's banks are
struct CodeGeneratorScratch
{
    const Ruleset *rules;
    const RegisterFile *file;
    unsigned int taken[3];
    unsigned int banks[3];
};

// the next scratch register of `bank` that the form takes, onto `name`; 0 where the form takes none of that bank
static int code_generator_scratch(void *context, unsigned int bank, VerbaLinea *name)
{
    CodeGeneratorScratch *const scratch = (CodeGeneratorScratch *)context;
    for (unsigned int bank_index = 0u; bank_index < 3u; bank_index += 1u)
    {
        if (bank == scratch->banks[bank_index])
        {
            scratch->taken[bank_index] += 1u;
            code_generator_register(scratch->rules, scratch->file, bank, scratch->taken[bank_index] - 1u, name);
            return 1;
        }
    }
    return 0;
}

// A form the core decided, written in the ruleset onto `text`, with its arguments. The program's note takes its
// arguments from its step count and the target, and the resident from the launch's layout. A construct takes its
// scratch from where the core left each bank as it decided the form. `broken` set where the ruleset cannot write it
static void code_generator_text(const Ruleset *rules, const RegisterFile *file, const TargetInfo *target,
                                const MachineInstr &item, VerbaLinea *text, int *broken)
{
    char written[CODE_GENERATOR_ARGUMENTS][CODE_GENERATOR_ARGUMENT_BYTES];
    const char *arguments[CODE_GENERATOR_ARGUMENTS];
    unsigned int count = 0u;
    VerbaLinea lines[CODE_GENERATOR_ARGUMENTS];
    for (unsigned int at = 0u; at < CODE_GENERATOR_ARGUMENTS; at += 1u)
    {
        lines[at] = mmgr_line_over(written[at], sizeof(written[at]));
        arguments[at] = written[at];
    }
    if (item.form == OPCODE_PROGRAM_NOTE)
    {
        // a device's compute capability and NVRTC's version are small counts, never negative
        mmgr_line_decimal(&lines[0], item.arguments[0].number, 1u);
        mmgr_line_decimal(&lines[1], (unsigned long long)target->major, 1u);
        mmgr_line_decimal(&lines[2], (unsigned long long)target->minor, 1u);
        mmgr_line_decimal(&lines[3], (unsigned long long)target->nvrtc_major, 1u);
        mmgr_line_decimal(&lines[4], (unsigned long long)target->nvrtc_minor, 1u);
        mmgr_line_hex(&lines[5], target->block_hash, 16u);
        count = 6u;
    }
    else if (item.form == OPCODE_PROGRAM_UNIT)
    {
        for (unsigned int at = 0u; at < PROGRAM_UNIT_PARAMETERS; at += 1u)
        {
            mmgr_line_decimal(&lines[at], codegen_unit(at), 1u);
        }
        count = PROGRAM_UNIT_PARAMETERS;
    }
    else
    {
        for (unsigned int at = 0u; (at < item.count) && (at < MACHINE_INSTR_OPERANDS); at += 1u)
        {
            code_generator_argument(rules, file, item.arguments[at], &lines[at]);
            count = at + 1u;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        (void)mmgr_line_finish(&lines[at]);
        *broken = *broken || (mmgr_line_ok(&lines[at]) == 0);
    }
    CodeGeneratorScratch taken = {rules,
                                  file,
                                  {item.scratch[0], item.scratch[1], item.scratch[2]},
                                  {REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_PREDICATE}};
    const ScratchRegisters scratch = {code_generator_scratch, &taken};
    ruleset_write_taking(rules, text, item.form, arguments, count, &scratch, broken);
}

// the elements of `from` put after the last of `to`: 1, or 0 where the pool cannot meet the room
static int code_generator_appended(PlatformArray *to, const PlatformArray *from)
{
    const unsigned long long held = to->count;
    if (platform_array_counted(to, held + from->count) == 0)
    {
        return 0;
    }
    if (from->count != 0ULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = platform_array_at(to, held), .src = from->cell,
                   .bytes = (size_t)(from->count * from->element));
    }
    return 1;
}

// The items `decide` decides put after those `items` holds: counted with no items first, then the list grown by that
// count once and decided into its end. 1, or 0 where the pool cannot meet the room
template <typename Decide>
static int code_generator_decided_after(MachineFunction *lane, PlatformArray *items, const Decide &decide)
{
    MachineFunction counting = *lane;
    counting.items = NULL;
    counting.capacity = 0ULL;
    counting.count = 0ULL;
    decide(&counting);
    const unsigned long long held = items->count;
    if (platform_array_counted(items, held + counting.count) == 0)
    {
        return 0;
    }
    lane->items = (MachineInstr *)platform_array_at(items, held);
    lane->capacity = counting.count;
    lane->count = 0ULL;
    decide(lane);
    lane->items = NULL;
    lane->capacity = 0ULL;
    return 1;
}

// Forms `decide` decides, into `items`, which the call empties first. `decide` counts them first on a copy of the lane.
// It then decides them into as many items as it counted, and the lane goes on from the second as from one decision. 1,
// or 0 where the pool cannot meet the items
template <typename Decide>
static int code_generator_decided(MachineFunction *lane, PlatformArray *items, const Decide &decide)
{
    MachineFunction counting = *lane;
    counting.items = NULL;
    counting.capacity = 0ULL;
    counting.count = 0ULL;
    decide(&counting);
    // the list keeps the cell and the tier its caller gave it, cut to empty and then taken at the count once
    if ((platform_array_counted(items, 0ULL) == 0) || (platform_array_counted(items, counting.count) == 0))
    {
        return 0;
    }
    lane->items = (MachineInstr *)items->cell;
    lane->capacity = counting.count;
    lane->count = 0ULL;
    decide(lane);
    lane->items = NULL;
    lane->capacity = 0ULL;
    return 1;
}

// the 32-bit words every lane uses throughout: %zero, %thread, %threads, %word_base, %word_stride and %sign_base, and
// the seven 64-bit %launch, %lane_number, %record, %index, %body, %bodies and %tables; each member's atom address adds
// two more
#define CODEGEN_RESERVED_WORDS 20u

// a cell of `count` words, each `value`, on the temporary tier; NULL where the pool cannot meet it
static unsigned int *code_generator_words(unsigned long long count, unsigned int value)
{
    unsigned int *const words = (unsigned int *)platform_cell_taken(
        ((count != 0ULL) ? count : 1ULL) * sizeof(unsigned int), platform_place_g_program_owner, 1);
    for (unsigned long long at = 0ULL; (words != NULL) && (at < count); at += 1ULL)
    {
        words[at] = value;
    }
    return words;
}

// the max 32-bit words the lane uses live at once, reckoned from the lifetimes its text gives ptxas: each value's
// limbs and sign from its step to its last reader, each record word from its first put to its last, each atom word from
// its first reader to its last, each step's own temporaries at that step (`step_words`), and the words every lane uses
// throughout. 0 where the pool cannot meet the reckoning
static unsigned int code_generator_live_max(const IrProgram *program, const unsigned int *step_words,
                                            const unsigned int *atom_last, unsigned int atoms)
{
    const unsigned int steps = program->step_count;
    unsigned int *const read_last = code_generator_words(steps, 0u);
    long long *const change = (long long *)platform_cell_cleared((unsigned long long)steps + 1ULL, sizeof(long long),
                                                                 platform_place_g_program_owner, 1);
    if ((read_last == NULL) || (change == NULL))
    {
        platform_cell_released(change, platform_place_g_program_owner);
        platform_cell_released(read_last, platform_place_g_program_owner);
        return 0u;
    }
    for (unsigned int at = 0u; at < steps; at += 1u)
    {
        read_last[at] = at;
    }
    // every operand is an earlier step, and the readers seen in order leave each value's last one
    for (unsigned int at = 0u; at < steps; at += 1u)
    {
        const DeviceRecordStep *const step = &program->steps[at];
        if (codegen_reads_left(step->operation))
        {
            read_last[step->left] = at;
        }
        if (codegen_reads_right(step->operation))
        {
            read_last[step->right] = at;
        }
    }
    // the change in live words at each step, their count its running sum
    for (unsigned int at = 0u; at < steps; at += 1u)
    {
        const long long value = (long long)program->steps[at].limbs + 1LL;
        change[at] += value + (long long)step_words[at];
        change[read_last[at] + 1u] -= value;
        change[at + 1u] -= (long long)step_words[at];
    }
    for (unsigned int word = 0u; word < program->out_limbs; word += 1u)
    {
        if (program->put_last[word] != 0u)
        {
            change[program->put_first[word]] += 1LL;
            change[program->put_last[word]] -= 1LL;
        }
    }
    for (unsigned int atom = 0u; atom < atoms; atom += 1u)
    {
        if (program->atom_reader[atom] != steps)
        {
            change[program->atom_reader[atom]] += 1LL;
            change[(size_t)atom_last[atom] + 1u] -= 1LL;
        }
    }
    long long live = 0LL;
    long long max = 0LL;
    for (unsigned int at = 0u; at < steps; at += 1u)
    {
        live += change[at];
        max = (live > max) ? live : max;
    }
    platform_cell_released(change, platform_place_g_program_owner);
    platform_cell_released(read_last, platform_place_g_program_owner);
    // at max the file's limbs and signs, the record's words, the atoms' words and one step's temporaries, far under
    // 2^32
    return (unsigned int)max + CODEGEN_RESERVED_WORDS + (2u * program->members);
}

// One ruleset's schema, which the process retains in a cell of its own with the name of its file after it. The schema
// names the lane's forms, banks and fixed registers, and the file. The toolchain and the header are the file's own.
// NULL where the pool cannot meet the cell
static const RulesetSchema *code_generator_schema(const char *file)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = file, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    unsigned char *const cell = (unsigned char *)platform_cell_cleared(1ULL, sizeof(RulesetSchema) + length + 1u,
                                                                       platform_place_g_program_owner, 0);
    if (cell == NULL)
    {
        return NULL;
    }
    char *const named = (char *)(cell + sizeof(RulesetSchema));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = named, .src = file, .bytes = length + 1u);
    RulesetSchema *const schema = (RulesetSchema *)cell;
    const RulesetSchema made = {
        named,           NULL,         NULL, s_opcode_names, OPCODE_COUNT, s_regclass_names, REGCLASS_COUNT,
        s_physreg_names, PHYSREG_COUNT};
    *schema = made;
    return schema;
}

// the bytes that the register claims of one lane hold at max
#define CODE_GENERATOR_CLAIM_BYTES (1ULL << 30u)

CodeGenerator::CodeGenerator(const char *file) : Target(code_generator_schema(file))
{
    platform_text_begun(&claim_lines, CODE_GENERATOR_CLAIM_BYTES, platform_place_g_program_owner,
                        PLATFORM_TIER_MOVABLE);
}

CodeGenerator::~CodeGenerator()
{
    platform_text_ended(&claim_lines);
}

void *CodeGenerator::operator new(size_t bytes, void *cell)
{
    (void)bytes;
    return cell;
}

void CodeGenerator::operator delete(void *cell, void *place)
{
    (void)place;
    platform_cell_released(cell, platform_place_g_program_owner);
}

void CodeGenerator::operator delete(void *cell)
{
    platform_cell_released(cell, platform_place_g_program_owner);
}

// the most code generators a process retains, one a ruleset file
#define CODE_GENERATORS_MAX 64u

// the code generators a process retains, each where its maker put it, and the lock the makers take in turn
static CodeGenerator *s_code_generators[CODE_GENERATORS_MAX];

static unsigned int s_code_generator_count;

static embed_atomic_u32 s_code_generators_lock = EMBED_ATOMIC_INIT(0u);

CodeGenerator &code_generator(const char *file)
{
    const EmbedAtomicU32ExchangeCfg take = {&s_code_generators_lock, 1u};
    while (embed_atomic_u32_exchange(&take) != 0u)
    {
    }
    CodeGenerator *generator = NULL;
    for (unsigned int at = 0u; (generator == NULL) && (at < s_code_generator_count); at += 1u)
    {
        const RulesetSchema *const schema = s_code_generators[at]->schema();
        generator =
            ((schema != NULL) && (code_generator_text_order(schema->file, file, PLATFORM_TEXT_TO_TERMINATOR) == 0))
                ? s_code_generators[at]
                : NULL;
    }
    if (generator == NULL)
    {
        void *const cell = platform_cell_cleared(1ULL, sizeof(CodeGenerator), platform_place_g_program_owner, 0);
        generator = (cell != NULL) ? new (cell) CodeGenerator(file) : NULL;
        if ((generator != NULL) && (s_code_generator_count < CODE_GENERATORS_MAX))
        {
            s_code_generators[s_code_generator_count] = generator;
            s_code_generator_count += 1u;
        }
    }
    const EmbedAtomicU32StoreCfg give = {&s_code_generators_lock, 0u};
    embed_atomic_u32_store(&give);
    if (generator == NULL)
    {
        // a code generator of no ruleset reads none and writes no lane, where the pool cannot meet a code generator
        static CodeGenerator s_refused("");
        return s_refused;
    }
    return *generator;
}

// the memory writes the ruleset's text makes in one state, its line write_ports, SCHEDULE_UNBOUNDED where it gives
// none or is not read
static unsigned int code_generator_write_ports(const Ruleset *rules)
{
    if ((rules == NULL) || (rules->write_ports[0] == '\0'))
    {
        return SCHEDULE_UNBOUNDED;
    }
    // Explicit cast takes a count of ports, which an unsigned int counts
    return (unsigned int)EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = rules->write_ports);
}

// the cells of one decision, each given back as the decision ends
struct CodeGeneratorDecision
{
    unsigned int *scratch;
    unsigned int *put_first;
    unsigned int *put_last;
    unsigned int *atom_reader;
    unsigned int *atom_last;
    unsigned int *errors;
    unsigned int *step_words;
    PlatformArray parts[9];
};

// the places of the parts of one decision among CodeGeneratorDecision::parts
enum CodeGeneratorPart
{
    CODE_GENERATOR_STEPPED = 0,
    CODE_GENERATOR_NOTED = 1,
    CODE_GENERATOR_OPENING_LANE = 2,
    CODE_GENERATOR_DECLARATIONS = 3,
    CODE_GENERATOR_OPENED = 4,
    CODE_GENERATOR_CLOSED = 5,
    CODE_GENERATOR_BODY_OPEN = 6,
    CODE_GENERATOR_BODY = 7,
    CODE_GENERATOR_ENDING = 8
};

static void code_generator_decision_ended(CodeGeneratorDecision *decision)
{
    platform_cell_released(decision->scratch, platform_place_g_program_owner);
    platform_cell_released(decision->put_first, platform_place_g_program_owner);
    platform_cell_released(decision->put_last, platform_place_g_program_owner);
    platform_cell_released(decision->atom_reader, platform_place_g_program_owner);
    platform_cell_released(decision->atom_last, platform_place_g_program_owner);
    platform_cell_released(decision->errors, platform_place_g_program_owner);
    platform_cell_released(decision->step_words, platform_place_g_program_owner);
    for (unsigned int part = 0u; part < 9u; part += 1u)
    {
        platform_array_ended(&decision->parts[part]);
    }
}

// The body split into states by `model`, into the body's part of `decision`, and its opening into the body-open
// part. `report` gets how the split went. 1, or 0 where the pool cannot meet a list
static int code_generator_scheduled_body(const CodeGenerator *generator, MachineFunction *lane, unsigned int steps,
                                         const ScheduleModel *model, ScheduleReport *report,
                                         CodeGeneratorDecision *decision)
{
    // each form's cost; an error's label for the opening and one for each step at max, and each loop the steps
    // wrote
    const ScheduleCosts costs = generator->schedule_costs(*model);
    MachineOperand *const dispatch_error = (MachineOperand *)platform_cell_cleared(
        (unsigned long long)steps + 1ULL, sizeof(MachineOperand), platform_place_g_program_owner, 1);
    unsigned int *const dispatch_state = code_generator_words((unsigned long long)steps + 1ULL, 0u);
    unsigned int *const loop_state = code_generator_words((unsigned long long)lane->loops + 1ULL, 0u);
    int ok = (dispatch_error != NULL) && (dispatch_state != NULL) && (loop_state != NULL);
    if (ok != 0)
    {
        Schedule schedule{};
        schedule.cost = costs.cost;
        schedule.budget = costs.budget;
        schedule.dispatch_error = dispatch_error;
        schedule.dispatch_state = dispatch_state;
        schedule.dispatch_max = steps + 1u;
        schedule.loop_state = loop_state;
        schedule.loop_count = lane->loops;
        const unsigned int writes = costs.writes;
        const unsigned int ports = costs.ports;
        const PlatformArray *const parts[3] = {&decision->parts[CODE_GENERATOR_OPENED],
                                               &decision->parts[CODE_GENERATOR_STEPPED],
                                               &decision->parts[CODE_GENERATOR_CLOSED]};
        ok = code_generator_decided(
            lane, &decision->parts[CODE_GENERATOR_BODY], [&schedule, writes, ports, &parts](MachineFunction *deciding) {
                codegen_schedule_open(deciding, &schedule, writes, ports);
                for (unsigned int part = 0u; part < 3u; part += 1u)
                {
                    for (unsigned long long at = 0ULL; at < parts[part]->count; at += 1ULL)
                    {
                        codegen_schedule_instr(deciding, &schedule,
                                               (const MachineInstr *)platform_array_at(parts[part], at));
                    }
                }
                codegen_schedule_close(deciding, &schedule);
            });
        const unsigned int states = schedule.state;
        ok = ok &&
             code_generator_decided(lane, &decision->parts[CODE_GENERATOR_BODY_OPEN],
                                    [states](MachineFunction *deciding) { codegen_body_open(deciding, 1, states); });
        report->states = schedule.state;
        report->max = schedule.max;
        report->over = schedule.over;
    }
    platform_cell_released(loop_state, platform_place_g_program_owner);
    platform_cell_released(dispatch_state, platform_place_g_program_owner);
    platform_cell_released(dispatch_error, platform_place_g_program_owner);
    return ok;
}

// A program's lane and its resident as the core decides them, into `items` in the text's order: the note, the header's
// place, occupied by an item of the form ASM_PRINTER_ALL_ONES, the lane's opening and declarations, the body's opening,
// the body and the end; the places a thread uses in shared memory (the file's where the language positions it out
// there, else 0), and the max words it uses live at once. The core decides the steps first, then the lane's own forms
// in the order the lane writes them. 0 where the ruleset is not read, a step is one the lane does not support, a
// form breaks the lane, or the pool cannot meet a list. With a schedule model, the body is split into states and
// `report` informed how
int CodeGenerator::decide(const EngineRecordLayout *layout, const ScheduleModel *model, ScheduleReport *report,
                          unsigned int *places, unsigned int *live, PlatformArray *items)
{
    const Ruleset *const rules = ready();
    if (rules == NULL)
    {
        return 0;
    }
    const unsigned int steps = layout->steps;
    const unsigned int scratch_banks[3] = {REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_PREDICATE};
    IrProgram program{};
    program.steps = layout->step_table;
    program.step_count = steps;
    program.members = layout->members;
    program.file_limbs = layout->file_limbs;
    program.out_limbs = layout->out_limbs;
    unsigned int atoms = 0u;
    for (unsigned int member = 0u; member < ENGINE_RECORD_MEMBERS_MAX; member += 1u)
    {
        program.in_limbs[member] = layout->in_limbs[member];
        program.atom_first[member] = atoms;
        atoms += (member < layout->members) ? layout->in_limbs[member] : 0u;
    }
    CodeGeneratorDecision decision;
    decision.scratch = ruleset_scratch(rules, scratch_banks);
    decision.put_first = code_generator_words(layout->out_limbs, 0u);
    decision.put_last = code_generator_words(layout->out_limbs, 0u);
    decision.atom_reader = code_generator_words(atoms, steps);
    decision.atom_last = code_generator_words(atoms, 0u);
    decision.errors = code_generator_words(steps, 0u);
    decision.step_words = code_generator_words(steps, 0u);
    for (unsigned int part = 0u; part < 9u; part += 1u)
    {
        // each part is ephemeral work of this call, given back before it returns
        platform_array_begun(&decision.parts[part], sizeof(MachineInstr), platform_place_g_program_owner,
                             PLATFORM_TIER_TEMPORARY);
    }
    int ok = (decision.scratch != NULL) && (decision.put_first != NULL) && (decision.put_last != NULL) &&
             (decision.atom_reader != NULL) && (decision.atom_last != NULL) && (decision.errors != NULL) &&
             (decision.step_words != NULL);
    // The program as every step reads it: each record word's first and last put, and each atom word's first and last
    // reader. The code finds them before it decides a step
    for (unsigned int at = 0u; (ok != 0) && (at < steps); at += 1u)
    {
        unsigned int low = 0u;
        unsigned int high = 0u;
        codegen_put_words(&program, &layout->step_table[at], &low, &high);
        for (unsigned int word = low; word < high; word += 1u)
        {
            decision.put_first[word] = (decision.put_last[word] == 0u) ? at : decision.put_first[word];
            decision.put_last[word] = at + 1u;
        }
    }
    for (unsigned int at = 0u; (ok != 0) && (at < steps); at += 1u)
    {
        unsigned int low = 0u;
        unsigned int high = 0u;
        codegen_atom_words(&program, at, &low, &high);
        for (unsigned int word = low; word < high; word += 1u)
        {
            const unsigned int atom = program.atom_first[layout->step_table[at].member] + word;
            decision.atom_reader[atom] = (decision.atom_reader[atom] == steps) ? at : decision.atom_reader[atom];
            decision.atom_last[atom] = at;
        }
    }
    program.put_first = decision.put_first;
    program.put_last = decision.put_last;
    program.atom_reader = decision.atom_reader;
    program.scratch = decision.scratch;
    MachineFunction lane{};
    lane.program = &program;
    // the lane's opening takes %t0 and %w0 before any step does
    lane.temps_max = 1u;
    lane.wides_max = 1u;
    // the steps, each from the loop number the steps before it left
    for (unsigned int at = 0u; (ok != 0) && (at < steps); at += 1u)
    {
        int stepped = 0;
        ok = code_generator_decided_after(
            &lane, &decision.parts[CODE_GENERATOR_STEPPED],
            [at, &stepped](MachineFunction *deciding) { stepped = codegen_step(deciding, at); });
        ok = ok && (stepped != 0);
        decision.errors[at] = lane.errors;
        // a 64-bit temporary takes two words
        decision.step_words[at] = lane.temps + (2u * lane.wides);
    }
    // the lane calls nothing; it uses places in shared memory only where its ruleset gives shared 1
    *places =
        (code_generator_text_order(rules->shared, "1", PLATFORM_TEXT_TO_TERMINATOR) == 0) ? layout->file_limbs : 0u;
    const unsigned int place_count = *places;
    const unsigned int *const error = decision.errors;
    const unsigned int tables = lane.tables;
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_NOTED],
                                      [](MachineFunction *deciding) { codegen_note(deciding); });
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_OPENING_LANE],
                                      [](MachineFunction *deciding) { codegen_instr0(deciding, OPCODE_LANE_OPEN); });
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_DECLARATIONS],
                                      [atoms](MachineFunction *deciding) { codegen_declare(deciding, atoms); });
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_OPENED],
                                      [error, tables, place_count](MachineFunction *deciding) {
                                          codegen_open(deciding, error, tables, place_count);
                                      });
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_CLOSED],
                                      [error](MachineFunction *deciding) { codegen_close(deciding, error); });
    // the body's forms in the text's order, between the forms that open it and those that end the lane
    if ((ok != 0) && (model == NULL))
    {
        ok = code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_BODY_OPEN],
                                    [](MachineFunction *deciding) { codegen_body_open(deciding, 0, 0u); });
        ok = ok &&
             code_generator_appended(&decision.parts[CODE_GENERATOR_BODY], &decision.parts[CODE_GENERATOR_OPENED]) &&
             code_generator_appended(&decision.parts[CODE_GENERATOR_BODY], &decision.parts[CODE_GENERATOR_STEPPED]) &&
             code_generator_appended(&decision.parts[CODE_GENERATOR_BODY], &decision.parts[CODE_GENERATOR_CLOSED]);
    }
    else if (ok != 0)
    {
        ok = code_generator_scheduled_body(this, &lane, steps, model, report, &decision);
    }
    ok = ok && code_generator_decided(&lane, &decision.parts[CODE_GENERATOR_ENDING],
                                      [](MachineFunction *deciding) { codegen_end(deciding); });
    ok = ok && (lane.broken == 0u);
    // the header's place, which no form of the ruleset writes
    MachineInstr header{};
    header.form = ASM_PRINTER_ALL_ONES;
    header.arguments[0] = codegen_zero();
    header.arguments[1] = codegen_zero();
    header.arguments[2] = codegen_zero();
    header.arguments[3] = codegen_zero();
    const unsigned int parts[5] = {CODE_GENERATOR_OPENING_LANE, CODE_GENERATOR_DECLARATIONS, CODE_GENERATOR_BODY_OPEN,
                                   CODE_GENERATOR_BODY, CODE_GENERATOR_ENDING};
    // the list taken once at its full size, the parts and the header, so that it does not grow above them
    unsigned long long total = decision.parts[CODE_GENERATOR_NOTED].count + 1ULL;
    for (unsigned int part = 0u; part < 5u; part += 1u)
    {
        total += decision.parts[parts[part]].count;
    }
    ok = ok && platform_array_counted(items, total) && platform_array_counted(items, 0ULL) &&
         code_generator_appended(items, &decision.parts[CODE_GENERATOR_NOTED]) &&
         platform_array_appended(items, &header);
    for (unsigned int part = 0u; (ok != 0) && (part < 5u); part += 1u)
    {
        ok = code_generator_appended(items, &decision.parts[parts[part]]);
    }
    *live = (ok != 0) ? code_generator_live_max(&program, decision.step_words, decision.atom_last, atoms) : 0u;
    code_generator_decision_ended(&decision);
    return ((ok != 0) && (*live != 0u)) ? 1 : 0;
}

// each register the lane uses onto the claim lines, with the items that claim and release it. 0 where a lane names
// another item's scratch, and nobody writes the lane, or the pool cannot meet the claims
static int code_generator_claims_written(const Ruleset *rules, const RegisterFile *file, const MachineInstr *items,
                                         unsigned long long count, PlatformText *claim_lines)
{
    const unsigned int scratch_banks[3] = {REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_PREDICATE};
    unsigned int *const scratch = ruleset_scratch(rules, scratch_banks);
    const unsigned long long claim_room = (16ULL * count) + 16ULL;
    CodegenClaim *const claims =
        (CodegenClaim *)platform_cell_cleared(claim_room, sizeof(CodegenClaim), platform_place_g_program_owner, 1);
    unsigned long long collided = CODEGEN_NO_ITEM;
    // the claims of a lane are far under 2^32
    const unsigned int claim_count =
        ((scratch != NULL) && (claims != NULL))
            ? codegen_claims(items, count, scratch, scratch_banks, claims, (unsigned int)claim_room, &collided)
            : 0u;
    const int ok = (scratch != NULL) && (claims != NULL) && (collided == CODEGEN_NO_ITEM);
    platform_text_reset(claim_lines);
    for (unsigned int at = 0u; (ok != 0) && (at < claim_count); at += 1u)
    {
        const CodegenClaim *const one = &claims[at];
        VerbaLinea *const line = &claim_lines->line;
        if (one->bank >= REGCLASS_COUNT)
        {
            mmgr_line_put(line, rules->fixed[one->bank - REGCLASS_COUNT]);
            mmgr_line_ch(line, ' ');
            mmgr_line_put(line, s_physreg_names[one->bank - REGCLASS_COUNT].text);
        }
        else
        {
            code_generator_register(rules, file, one->bank, one->number, line);
            mmgr_line_ch(line, ' ');
            if (one->scratch_of != CODEGEN_NO_ITEM)
            {
                mmgr_line_put(line, "scratch of ");
                mmgr_line_put(line, s_opcode_names[items[one->scratch_of].form].text);
            }
            else
            {
                mmgr_line_put(line, s_regclass_names[one->bank].text);
            }
        }
        mmgr_line_put(line, " claimed ");
        mmgr_line_decimal(line, one->claimed + 1ULL, 1u);
        mmgr_line_ch(line, ' ');
        mmgr_line_put(line, s_opcode_names[items[one->claimed].form].text);
        mmgr_line_put(line, " released ");
        mmgr_line_decimal(line, one->released + 1ULL, 1u);
        mmgr_line_ch(line, ' ');
        mmgr_line_put(line, s_opcode_names[items[one->released].form].text);
        mmgr_line_ch(line, '\n');
    }
    platform_cell_released(claims, platform_place_g_program_owner);
    platform_cell_released(scratch, platform_place_g_program_owner);
    return ok;
}

// The lane as decide() decides it, written in the ruleset under `header` for `target`, which the note names, onto
// `text`. 0, and `text` empty, where decide() does not decide it or its banks run past the language's register file.
// Also 0 where the code wrote a form with other arguments than it takes
int CodeGenerator::lane(const EngineRecordLayout *layout, const TargetInfo *target, const char *header,
                        PlatformText *text, unsigned int *places, unsigned int *live, const ScheduleModel *model,
                        ScheduleReport *report)
{
    platform_text_reset(text);
    // the lane's items are ephemeral work of this call, given back before it returns
    PlatformArray items;
    platform_array_begun(&items, sizeof(MachineInstr), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    const Ruleset *const rules = ready();
    RegisterFile file;
    int ok = decide(layout, model, report, places, live, &items) &&
             code_generator_file(this, (const MachineInstr *)items.cell, items.count, &file) &&
             code_generator_claims_written(rules, &file, (const MachineInstr *)items.cell, items.count, &claim_lines);
    int broken = 0;
    for (unsigned long long at = 0ULL; (ok != 0) && (at < items.count); at += 1ULL)
    {
        const MachineInstr *const item = (const MachineInstr *)platform_array_at(&items, at);
        if (item->form == ASM_PRINTER_ALL_ONES)
        {
            mmgr_line_put(&text->line, header);
        }
        else if ((item->form == OPCODE_PROGRAM_UNIT) && (program_unit_written() == 0))
        {
            // the language's resident reaches it already built, and the ruleset is never queried for one
        }
        else
        {
            code_generator_text(rules, &file, target, *item, &text->line, &broken);
        }
    }
    platform_array_ended(&items);
    ok = ok && (broken == 0) && (mmgr_line_ok(&text->line) != 0);
    if (ok == 0)
    {
        platform_text_reset(text);
    }
    return ok;
}

const char *CodeGenerator::register_claims(void)
{
    return platform_text_bytes(&claim_lines);
}

// each form's cost by its name in the model, and the model's cost for a form it does not name
ScheduleCosts CodeGenerator::schedule_costs(const ScheduleModel &model) const
{
    ScheduleCosts costs;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        costs.cost[form] = model.other;
        for (unsigned int at = 0u; at < model.cost_count; at += 1u)
        {
            costs.cost[form] = (code_generator_text_order(model.cost[at].form, s_opcode_names[form].text,
                                                          PLATFORM_TEXT_TO_TERMINATOR) == 0)
                                   ? model.cost[at].cost
                                   : costs.cost[form];
        }
    }
    costs.budget = model.budget;
    costs.writes = model.writes;
    costs.ports = code_generator_write_ports(ready());
    return costs;
}
