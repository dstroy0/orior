// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cases.c: the cases: a file read, an anchor that fits, the relations and the expected answers, the cases written, the
// kernel, a splice, a mark and a swap, a form that fits, the table and the sources, digits in order, an instruction and
// the container written
#include "../../../../../../platform/platform.h"
#include "../../../../../engine/rmc/precept_value.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "writings_internal.h"

#define LADDER_WORD_COUNT(name_, text_, words_, measured_) words_,

static const unsigned int s_anchor_words[] = {LADDER_ANCHORS(LADDER_WORD_COUNT)};

#undef LADDER_WORD_COUNT

#define PRECEPT_ARITY(name_, text_, arity_) arity_,

static const unsigned int s_precept_arity[] = {PRECEPTS(PRECEPT_ARITY)};

#undef PRECEPT_ARITY

SassMachine s_machine;
unsigned char s_pattern[WRITINGS_CUBIN_BYTES];
static unsigned char s_cubin[WRITINGS_CUBIN_BYTES];
unsigned char s_code[WRITINGS_CODE_BYTES];
char s_kernel_text[WRITINGS_TEXT_BYTES];
char s_query_text[WRITINGS_TEXT_BYTES];
static unsigned int s_exits[WRITINGS_EXITS];
char s_kernel[128];
unsigned long long s_pattern_size;

// the two words of each case a form is put with
unsigned int s_case[WRITINGS_CASES][2];

unsigned int s_cases;
WritingsRelation s_relation[WRITINGS_RELATIONS];
unsigned int s_relations;

// `path` read whole into `bytes`, sized for `room`: the bytes read, 0 where it was not read
unsigned long long writings_file_read(const char *path, unsigned char *bytes, unsigned long long room)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0ULL;
    }
    const unsigned long long read = (unsigned long long)platform_file_read_items(bytes, 1u, (size_t)room, file);
    platform_file_close(file);
    return read;
}

// 1 where `anchor` is a relation whose answer follows from two words. A measure's answer is the system's and no form
// gives it; a relation of other than two words does not fit the kernel's two sources
static int writings_anchor_fits(unsigned int anchor)
{
    return (s_ladder_measured[anchor] == 0) && (s_anchor_words[anchor] == 2u);
}

// The relations a form is read against: every ladder relation whose answer follows from two words, then every
// precept that carries a word from one or two, NOT and MOV reading the first word alone. NOP, ERR and the branches
// carry no word
void writings_relations(void)
{
    s_relations = 0u;
    for (unsigned int anchor = 0u; anchor < LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        if (writings_anchor_fits(anchor) && (s_relations < WRITINGS_RELATIONS))
        {
            s_relation[s_relations] = (WritingsRelation){s_anchor_text[anchor], 0, anchor};
            s_relations += 1u;
        }
    }
    for (unsigned int precept = 0u; precept < PRECEPT_COUNT; precept += 1u)
    {
        const int carries = (precept != PRECEPT_NOP) && (precept != PRECEPT_ERR) && (precept != PRECEPT_BRA) &&
                            (precept != PRECEPT_JCC) && (s_precept_arity[precept] != 0u);
        if (carries && (s_relations < WRITINGS_RELATIONS))
        {
            s_relation[s_relations] = (WritingsRelation){s_precept_text[precept], 1, precept};
            s_relations += 1u;
        }
    }
}

// the word `relation` gives the case `word`
unsigned int writings_expected(const WritingsRelation *relation, const unsigned int *word)
{
    if (relation->precept)
    {
        return precept_applied((unsigned char)relation->which, word[0], word[1]);
    }
    unsigned int answered = 0u;
    ladder_answer(relation->which, word, 2u, &answered);
    return answered;
}

// The cases a form is put with, every relation read on every one: the ladder's own two-word cases, the words a width
// turns on put against the counts a shift turns on, and words drawn as chain_build draws its sweep to fill a launch.
// The ladder's cases are small words, and a form that agrees with a relation on small words alone, as a dot product
// of bytes agrees with a product, is distinguished by the drawn ones
void writings_cases(void)
{
    static const unsigned int s_edge_words[] = {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
    static const unsigned int s_edge_counts[] = {0u, 1u, 31u, 32u, 33u, 0xffffffffu};
    s_cases = 0u;
    for (unsigned int at = 0u; (at < LADDER_CASE_COUNT) && (s_cases < WRITINGS_CASES); at += 1u)
    {
        const LadderQuestion *const question = &s_ladder_cases[at];
        if (writings_anchor_fits(question->anchor) && (question->words == 2u))
        {
            s_case[s_cases][0] = question->word[0];
            s_case[s_cases][1] = question->word[1];
            s_cases += 1u;
        }
    }
    for (unsigned int word = 0u; word < (sizeof(s_edge_words) / sizeof(s_edge_words[0])); word += 1u)
    {
        for (unsigned int count = 0u; count < (sizeof(s_edge_counts) / sizeof(s_edge_counts[0])); count += 1u)
        {
            if (s_cases < WRITINGS_CASES)
            {
                s_case[s_cases][0] = s_edge_words[word];
                s_case[s_cases][1] = s_edge_counts[count];
                s_cases += 1u;
            }
        }
    }
    unsigned int state = 0x9e3779b9u;
    while (s_cases < WRITINGS_CASES)
    {
        s_case[s_cases][0] = ladder_swept(&state);
        s_case[s_cases][1] = ladder_swept(&state);
        s_cases += 1u;
    }
}

// the kernel the kernel opens with, `.text.<kernel>:`, into `kernel`: 1, or 0 where it opens otherwise
int writings_kernel(const char *kernel_text, char *kernel, size_t room)
{
    if (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = kernel_text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = ".text.", .ci = EMBED_FALSE))
    {
        return 0;
    }
    const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = kernel_text + 6,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ":\r\n");
    VerbaLinea formed_line_17;
    return (kernel_text[6u + length] == ':') &&
           ((formed_line_17 = mmgr_line_over(kernel, room),
             mmgr_line_put_max(&formed_line_17, kernel_text + 6, (size_t)((int)length)),
             (void)mmgr_line_finish(&formed_line_17), (int)formed_line_17.at) < (int)room);
}

// the kernel with its line that begins `IADD3 ` replaced by `lines`, into `text`: 1, or 0 where the kernel contains no
// such line or `text` will not fit the whole
int writings_splice(const char *kernel_text, const char *lines, char *text, size_t room)
{
    const char *const line =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = kernel_text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "\nIADD3 ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if (line == NULL)
    {
        return 0;
    }
    const char *const after =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\n');
    const size_t before = (size_t)(line - kernel_text) + 1u;
    VerbaLinea formed_line_1 = mmgr_line_over(text, room);
    mmgr_line_put_max(&formed_line_1, kernel_text, (size_t)((int)before));
    mmgr_line_put(&formed_line_1, lines);
    mmgr_line_put(&formed_line_1, (after != NULL) ? after : "");
    (void)mmgr_line_finish(&formed_line_1);
    return (int)formed_line_1.at < (int)room;
}

// the sign an operand carries back as its text, since the parts reader cuts it off
static const char *writings_mark(unsigned int mark)
{
    if (mark == SASS_MARK_NEGATE)
    {
        return "-";
    }
    if (mark == SASS_MARK_INVERT)
    {
        return "~";
    }
    if (mark == SASS_MARK_NOT)
    {
        return "!";
    }
    return "";
}

// `operand`, which begins with a register token, written to `out` with that token replaced by `put`, a .hi dropped and
// any other suffix retained
static void writings_swap(const char *operand, const char *put, char *out, size_t room)
{
    size_t length = 0u;
    if (operand[0] == 'R')
    {
        length = (operand[1] == 'Z') ? 2u
                                     : (1u + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = operand + 1,
                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "0123456789"));
    }
    const char *const rest = operand + length;
    const size_t rest_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = rest, .cap = PLATFORM_TEXT_TO_TERMINATOR) -
        (sass_high_half(operand) ? 3u : 0u);
    {
        VerbaLinea buffer_line = mmgr_line_over(out, room);
        mmgr_line_put(&buffer_line, put);
        mmgr_line_put_max(&buffer_line, rest, (size_t)((int)rest_length));
        (void)mmgr_line_finish(&buffer_line);
    }
}

// 1 where `form` is put to the part: it writes its first operand, a register, it neither transfers control nor waits,
// and every operand is a register, a predicate or a number, which reach no memory and no state of the part's own
int writings_form_fits(const SassForm *form)
{
    if ((form->operands < 2u) || (form->type[0] != SASS_OPERAND_REGISTER) ||
        sass_operation_control_or_wait(form->operation) ||
        (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = form->operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = "NOP", .ci = EMBED_FALSE)))
    {
        return 0;
    }
    unsigned int sources = 0u;
    for (unsigned int at = 0u; at < form->operands; at += 1u)
    {
        const unsigned int type = form->type[at];
        if ((type != SASS_OPERAND_REGISTER) && (type != SASS_OPERAND_PREDICATE) && (type != SASS_OPERAND_IMMEDIATE))
        {
            return 0;
        }
        sources += ((at != 0u) && (type == SASS_OPERAND_REGISTER)) ? 1u : 0u;
    }
    return sources != 0u;
}

// the operand of `form` that is a number whose field is eight bits wide, a truth table of three inputs, or the operand
// count where none is
unsigned int writings_table(const SassForm *form)
{
    for (unsigned int operand = 1u; operand < form->operands; operand += 1u)
    {
        unsigned int width = 0u;
        for (unsigned int at = 0u; at < form->runs; at += 1u)
        {
            width += (form->run[at].operand == operand) ? (form->run[at].last - form->run[at].first + 1u) : 0u;
        }
        if ((form->type[operand] == SASS_OPERAND_IMMEDIATE) && (width == 8u))
        {
            return operand;
        }
    }
    return form->operands;
}

// how many register sources `form` reads: every register operand past its result
unsigned int writings_sources(const SassForm *form)
{
    unsigned int sources = 0u;
    for (unsigned int operand = 1u; operand < form->operands; operand += 1u)
    {
        sources += (form->type[operand] == SASS_OPERAND_REGISTER) ? 1u : 0u;
    }
    return sources;
}

// the register source `source` of an assignment, read as a number in base three with the first source its lowest
// digit: 0 is WRITINGS_LEFT, 1 WRITINGS_RIGHT and 2 RZ
unsigned int writings_digit(unsigned int assignment, unsigned int source)
{
    for (unsigned int at = 0u; at < source; at += 1u)
    {
        assignment /= 3u;
    }
    return assignment % 3u;
}

// the assignment that gives the first register source WRITINGS_LEFT, the second WRITINGS_RIGHT and every one past
// those RZ
unsigned int writings_in_order(unsigned int sources)
{
    unsigned int assignment = 0u;
    unsigned int place = 1u;
    for (unsigned int source = 0u; source < sources; source += 1u)
    {
        assignment += ((source == 0u) ? 0u : ((source == 1u) ? 1u : 2u)) * place;
        place *= 3u;
    }
    return assignment;
}

// `form`'s own instruction written to `out` with its result moved to WRITINGS_RESULT and each register source given
// WRITINGS_LEFT, WRITINGS_RIGHT or RZ by `assignment` (writings_digit). The operand `table` is written as `value`;
// every other predicate and number retains the value the form was seen with. 1, or 0 where `out` will not fit it
int writings_instruction(const SassForm *form, unsigned int table, unsigned int value, unsigned int assignment,
                         char *out, size_t room)
{
    static const char *const s_given[3] = {WRITINGS_LEFT, WRITINGS_RIGHT, "RZ"};
    SassInstructionParts parts;
    sass_instruction_read(form->text, &parts);
    if (parts.operands != form->operands)
    {
        return 0;
    }
    VerbaLinea formed_line_2 = mmgr_line_over(out, room);
    mmgr_line_put(&formed_line_2, parts.operation);
    (void)mmgr_line_finish(&formed_line_2);
    size_t at = (size_t)(int)formed_line_2.at;
    unsigned int source = 0u;
    for (unsigned int operand = 0u; operand < parts.operands; operand += 1u)
    {
        char text[SASS_MACHINE_TOKEN];
        if (parts.type[operand] == SASS_OPERAND_REGISTER)
        {
            const char *const put = (operand == 0u) ? WRITINGS_RESULT : s_given[writings_digit(assignment, source)];
            source += (operand != 0u) ? 1u : 0u;
            writings_swap(parts.operand[operand], put, text, sizeof(text));
        }
        else if (operand == table)
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, "0x");
            mmgr_line_hex(&buffer_line, value, 2u);
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, parts.operand[operand]);
            (void)mmgr_line_finish(&buffer_line);
        }
        VerbaLinea formed_line_3 = mmgr_line_over(&out[at], (at < room) ? (room - at) : 0u);
        mmgr_line_put(&formed_line_3, (operand == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_3, writings_mark(parts.mark[operand]));
        mmgr_line_put(&formed_line_3, text);
        (void)mmgr_line_finish(&formed_line_3);
        at += (size_t)(int)formed_line_3.at;
    }
    return at < room;
}

// `code`, `count` instructions, positioned into a cubin and written to `path`: 1, or 0 where it was not
int writings_cubin(unsigned int count, const char *path)
{
    const unsigned long long code_size = 16ULL * count;
    CubinWrite written;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &written, .bytes = sizeof(written), .val = 0);
    written.pattern = s_pattern;
    written.pattern_size = s_pattern_size;
    written.kernel = s_kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = WRITINGS_REGISTERS;
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(&s_machine), s_exits, WRITINGS_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    if (!cubin_write(&written, s_cubin, sizeof(s_cubin), &size) ||
        (cubin_safe_image(&s_machine, s_cubin, size, written.registers, &at) != CUBIN_SAFE))
    {
        return 0;
    }
    PlatformFile *const file = platform_file_open(path, "wb");
    const int put = (file != NULL) && (platform_file_write_items(s_cubin, 1u, (size_t)size, file) == size);
    const int closed = (file != NULL) && (platform_file_close(file) == 0);
    return put && closed;
}
