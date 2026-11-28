// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// The part's instructions as the cell's probes read them back, and one instruction's text in its parts
#ifndef SASS_MACHINE_H
#define SASS_MACHINE_H

#include <stddef.h>

// A form is an operation with its modifiers. For each printed operand, it also has the type of thing that the operand
// is and its mark (- or ~ on a register, ! on a predicate). Two instructions of one form differ only in the values that
// their operand fields hold. The assembler builds every instruction of a form from the encoding that the probe first
// saw it with (sass_assemble.h).
//
// The program does not guess which bits carry which operand. The probe of the cell turns each of the 128 bits of the
// form over one at a time, and disassembles all 129 encodings. It records the runs of bits that changed a printed
// operand, and those runs are the fields of the form. A match of an operand to a field by the value of the base there
// is not enough. A field that the operation does not use holds 0, and a register or predicate numbered 0 matches it.
//
// The own file of a part is <part>.khw beside its rulesets (includes/file_defs/khw). Its first line is `forms 2`, then
// `part <name>`, then one line for each form:
//
//     form <operation> <type><mark>,... <low> <high> <operand>:<first>-<last>:<place>;... <relation> <signed>
//          <the instruction as the probe saw it>
//
// all on one line. `<low>` and `<high>` are the two 64-bit words of the instruction in hex, low first, as a listing
// prints them. The runs are the bits that change each operand, by the place of the operand in the printed text.
//
// The `<place>` of a run is where its operand is in the tuple of the relation. The tuple has the words in their order,
// then the answer, as 1,1 -> 2 satisfies them (ladder.h). It is - where the part answered it none. `<relation>` is the
// relation that every case of the form answered, and `<signed>` is 1 where the answers read a word as signed. Both are
// - where the program queried no relation for the form. A file at `forms 1` holds no place, relation or sign, and reads
// as - in each.

// the longest instruction text stored, the longest operation or operand, and the max operands an instruction carries
#define SASS_MACHINE_TEXT 192u
#define SASS_MACHINE_TOKEN 64u
#define SASS_MACHINE_OPERANDS 8u
// the max runs of bits that the operands of one form take between them, and the max forms that a machine file holds.
// The sweep puts every operation key to the disassembler from each of several carriers. It found 1001 forms on sm_86
// when the ceiling was 1024. That is close enough that another carrier or another part could reach the ceiling. A
// machine that fills up keeps the forms that it has, and counts the rest in `refused`.
//
// The widen step repeats over what it finds for a bounded count of rounds. The count of forms then follows the
// operations and operand types that lie that far from what a compiler wrote. It does not follow what the compiler
// wrote. A run to
// its own end does not close: the count climbs past what a machine holds. A form is 736 bytes, which puts this ceiling
// at 12 MB of a machine. A run that reaches it reports that in `refused`, and does not drop forms with no report
#define SASS_MACHINE_RUNS 32u
#define SASS_MACHINE_FORMS 16384u
#define SASS_MACHINE_PART 16u

// what one printed operand is
enum SassOperandType
{
    // an operand that the parts reader did not know. The assembler can build no instruction of that form
    SASS_OPERAND_UNKNOWN = 0,
    SASS_OPERAND_REGISTER = 1,
    SASS_OPERAND_PREDICATE = 2,
    SASS_OPERAND_IMMEDIATE = 3,
    SASS_OPERAND_CONSTANT = 4,
    SASS_OPERAND_ADDRESS = 5,
    SASS_OPERAND_LABEL = 6,
    SASS_OPERAND_UNIFORM = 7,
    SASS_OPERAND_SYSTEM = 8
};

// the mark that an operand carries. The form carries it, because the bit that puts it there belongs to the operation
// and not to the operand. The encoding that the probe saw the form with already carries the mark of every instruction
// of that form
enum SassOperandMark
{
    SASS_MARK_NONE = 0,
    SASS_MARK_NEGATE = 1,
    SASS_MARK_INVERT = 2,
    SASS_MARK_NOT = 3
};

// the text of one instruction in its parts: its guard predicate ("@P0", "@!P0" or empty), and its operation with its
// modifiers. Each operand has its mark cut off and .reuse dropped, because reuse lies in the control bits
typedef struct
{
    char guard[SASS_MACHINE_TOKEN];
    char operation[SASS_MACHINE_TOKEN];
    unsigned int operands;
    char operand[SASS_MACHINE_OPERANDS][SASS_MACHINE_TOKEN];
    unsigned int type[SASS_MACHINE_OPERANDS];
    unsigned int mark[SASS_MACHINE_OPERANDS];
} SassInstructionParts;

// the place of a run whose operand is nowhere in the tuple of the relation, and the text that the file writes for it
#define SASS_RUN_NO_PLACE 0xffffffffu
#define SASS_RUN_NO_PLACE_TEXT "-"

// one run of bits that changes one printed operand, as the probe found it. Then the place of its operand in the tuple
// of the relation, SASS_RUN_NO_PLACE where the part answered it none
typedef struct
{
    unsigned int operand;
    unsigned int first;
    unsigned int last;
    unsigned int place;
} SassRun;

// one form, the encoding that the probe first saw it with, and the bits that its operands sit in. Then the relation
// that every case of it answered, and whether the answers read a word as signed. `relation` is empty where the program
// queried none
typedef struct
{
    char operation[SASS_MACHINE_TOKEN];
    unsigned int operands;
    unsigned int type[SASS_MACHINE_OPERANDS];
    unsigned int mark[SASS_MACHINE_OPERANDS];
    unsigned long long low;
    unsigned long long high;
    unsigned int runs;
    SassRun run[SASS_MACHINE_RUNS];
    char relation[SASS_MACHINE_TOKEN];
    int signed_read;
    char text[SASS_MACHINE_TEXT];
} SassForm;

// the max operations the part answered a soonest read for
#define SASS_MACHINE_SOONEST 256u

// the soonest read of one operation, as the part answered it. It is the fewest cycles between it and an instruction
// that reads its result, the max that any reader needed
typedef struct
{
    char operation[SASS_MACHINE_TOKEN];
    unsigned int stall;
} SassSoonest;

// the last register a machine uses where the part has not answered which it is: every number is the code's
#define SASS_MACHINE_UNANSWERED 0xffffffffu

// `register_last` is the last register that, as the part answers, the code of a question can name. It comes from the
// .ksc beside the machine file (`run answers <last> register last`). SASS_MACHINE_UNANSWERED stands for it where that
// holds no answer.
//
// `register_past` is how many registers past the highest that its code names a container declares, as the part
// answers. A container that declares d names R0 through R(d - past) and no other. One past that faults the part with a
// register out of range (`run answers <past> register past`). SASS_MACHINE_UNANSWERED stands for it where the part did
// not answer it
typedef struct
{
    char part[SASS_MACHINE_PART];
    unsigned int forms;
    unsigned int refused;
    SassForm form[SASS_MACHINE_FORMS];
    SassSoonest soonest[SASS_MACHINE_SOONEST];
    unsigned int soonests;
    unsigned int register_last;
    unsigned int register_past;
    // where the operands of each form sit and the bits that it leaves open. The call sass_encoding_places_built
    // (sass_assemble.h) stores them once for every later read of an encoding. NULL where each read finds them again
    void *places_built;
} SassMachine;

// `text` read into its parts, whatever it holds. The call stores an operand that the reader does not know with the type
// SASS_OPERAND_UNKNOWN. The guard is empty where the instruction carries none.
// 1 where `text` ends in the .hi a ruleset writes for the second register of a 64-bit pair (sass.krs)
int sass_high_half(const char *text);

void sass_instruction_read(const char *text, SassInstructionParts *parts);

// The text of `form` written into `written`, which has room for `room`. Each operand that a run with a place reads goes
// as {<place>}, with its mark kept. Every other operand goes as the probe saw it with the form. This is the form as a
// ruleset writes it before it names its parameters. The return is 1, or 0 where it does not fit
int sass_text_placed(const SassForm *form, char *written, size_t room);

// the encoding of EXIT as `machine` contains it, or 0 where it contains none. A cubin names the offset of every exit in
// its own section, and whatever writes one finds them by this
unsigned long long sass_exit_encoding(const SassMachine *machine);

// the bits of the scheduler above the operation, which no listing prints. First the stall, the yield, and a write
// barrier where the result comes back late. Then a read barrier where the part reads an operand late, the wait over six
// barriers and the reuse flags. A barrier field of 7 sets none
#define SASS_STALL_FIRST 105u
#define SASS_YIELD_FIRST 109u
#define SASS_WRITE_BARRIER_FIRST 110u
#define SASS_READ_BARRIER_FIRST 113u
#define SASS_WAIT_FIRST 116u
#define SASS_REUSE_FIRST 122u
#define SASS_BARRIER_NONE 7u
// the longest stall the four bits of the stall field represent, and the wait field set to every one of the six barriers
#define SASS_STALL_MAX_LEN 15u
#define SASS_WAIT_EVERY 0x3fu
// the bits of the low word that name an operation and its operands' types, its operation key
#define SASS_OPERATION_MASK 0xfffULL

// 1 where `operation` transfers control or waits. Such an operation can loop or stall the part: a branch, a call, a
// return, a barrier, a sleep and a trap. A straight instruction falls through to the next and cannot loop by itself
int sass_operation_control_or_wait(const char *operation);

// how the scheduler tracks the result of an operation, a property of the operation and not of one encoding of it. The
// result is ready after a fixed count of cycles, or back late behind a write barrier that its readers wait on. Else it
// is a store whose operands the part reads late behind a read barrier, and the next writer of those registers waits on
// it.
//
// NVIDIA's compiler gives every late operation a write barrier and every store a read barrier over the CUDA sources of
// the tree (monolith_scheduler.md). A barrier counts its producers, and a wait on it waits until all of them are back.
// A late result with no barrier goes into a queue behind another result of the same operation from the same unit. The
// results come back in their order
enum SassSchedule
{
    SASS_SCHEDULE_FIXED = 0,
    SASS_SCHEDULE_LATE = 1,
    SASS_SCHEDULE_STORE = 2
};

// 1 where the encoding whose high word is `high` sets the barrier whose field starts at `first`,
// SASS_WRITE_BARRIER_FIRST or SASS_READ_BARRIER_FIRST. Then the result or an operand comes back late, and the waits on
// it wait on that barrier
int sass_barrier_set(unsigned long long high, unsigned int first);

// the schedule of `operation`, with its modifiers, read from its name before the first dot. Through `soonest` goes the
// fewest cycles between a fixed result and an instruction that reads it. That is the answer of the part for that
// operation on the run channel, as `machine` holds it. An operation that the part did not answer for, a late result,
// and a store leave SASS_STALL_MAX_LEN there
unsigned int sass_operation_schedule(const SassMachine *machine, const char *operation, unsigned int *soonest);

// an instruction stored in `machine` as a form, where the machine has none of that form yet. `low` and `high` are its
// encoding, and `text` is the instruction as the probe saw it. The form that it went in as, or the one already there,
// goes through `taken`, and the caller fills its runs. The return is 1, or 0 where the machine is full, counted in
// machine->refused
int sass_machine_take(SassMachine *machine, const char *text, unsigned long long low, unsigned long long high,
                      SassForm **taken);

// an instruction that the part answered `relation` for, stored in `machine` as a form. It goes in the form of its shape
// that answers no relation yet or answers `relation`. Where every form of its shape answers another relation, it goes
// in a form of its own. Two relations of one shape are two forms of the protocol. The form goes through `taken`, and
// the caller fills its runs. The return is 1, or 0 where the machine is full, counted in machine->refused
int sass_machine_take_related(SassMachine *machine, const char *text, unsigned long long low, unsigned long long high,
                              const char *relation, SassForm **taken);

// the form of `parts`, or NULL where the machine contains none
const SassForm *sass_machine_form(const SassMachine *machine, const SassInstructionParts *parts);

// The machine written to `path`, and read back from it: 1, or 0 with the reason printed. A read also takes what the
// part answered on the run channel, from the .ksc beside it: `path` with .ksc in place of .khw. Each line
// `run answers <stall> stall <writer> <reader>` is the soonest that `<reader>` reads the result of `<writer>`. The
// soonest read of an operation is the largest of its lines. A machine with no .ksc beside it holds none
int sass_machine_write(const SassMachine *machine, const char *path);

int sass_machine_read(SassMachine *machine, const char *path);

#endif
