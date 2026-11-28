// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Every form sass.krs writes, assembled against the part's machine file, with no device and no toolchain.
//
//     sass_krs_assemble [ruleset] [machine file] [writings]
//
// With a third argument, the program writes the text of every form that assembled to that file, one form after
// another.
//
// A read of the ruleset against its schema tells nothing about one thing. Nor does a check of the text of each form
// against its listing. They do not tell whether the assembler can turn that text into the sixteen bytes that the part
// runs. The lane cannot go through a form that this refuses, whatever the ruleset tells.
//
// The program does not guess the type of a parameter from its name, because one name is two things in two forms.
// `left` is a register in word_bitand and a predicate in predicate_bitxor. `value` is a register in test_word_nonzero
// and a number in word_set. The program writes each form with every assignment of types to its parameters, register
// first. The form passes where any one of them assembles. A form that assembles under no assignment is the gap, and
// the assignment that worked is what the parameters of the form are.
//
// The program reads the forms and their parameters from the own lines of the ruleset. The schema records only how many
// parameters a form takes. It counts apart a form that the ruleset leaves empty, and a form whose text is a label or a
// note and carries no instruction. Neither is a failure here.

// the cubin writer is C, and its headers carry no guard of their own. This file names the linkage here
extern "C"
{
#include "../sass_assemble.h"
#include "../sass_machine.h"
}

#include "../../../../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sass_target.h"
#include "target.h"

#include <stddef.h>

// the max parameters for which the program writes a form in every way. Past this, it writes the form with registers
// alone. The assignments tried are three to the power of the parameters, and the form of the resident takes
// twenty-four
#define KRS_PARAMETERS_SEARCHED 6u

// the parameters that a form takes at max, and the bytes of the name of a form and of an argument, each terminator
// among them. Then the bytes of a writing at max
#define KRS_PARAMETERS_MAX 32u
#define KRS_NAME_BYTES 128u
#define KRS_ARGUMENT_BYTES 16u
#define KRS_TEXT_BYTES (1ULL << 24u)

// what one parameter gets, in the order that the program tries them
enum KrsType
{
    KRS_REGISTER = 0,
    KRS_PREDICATE = 1,
    KRS_NUMBER = 2,
    KRS_TYPES = 3
};

// a scratch register of each bank onto `name`, numbered where no argument written here reaches. 0 for a bank it does
// not know
static int krs_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    static unsigned int temporaries;
    static unsigned int wides;
    static unsigned int predicates;
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "temporary",
                   .ci = EMBED_FALSE))
    {
        temporaries += 1u;
        mmgr_line_ch(name, 'R');
        mmgr_line_decimal(name, 100u + (temporaries % 8u), 1u);
        return 1;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "wide",
                   .ci = EMBED_FALSE))
    {
        wides += 1u;
        mmgr_line_ch(name, 'R');
        mmgr_line_decimal(name, 120u + (2u * (wides % 4u)), 1u);
        return 1;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "predicate",
                   .ci = EMBED_FALSE))
    {
        predicates += 1u;
        mmgr_line_ch(name, 'P');
        mmgr_line_decimal(name, predicates % 3u, 1u);
        return 1;
    }
    return 0;
}

// The argument at `place`, of the type `type`, into `text`. A 64-bit form takes registers of an even number. The file
// sass.krs writes the second of the pair as .hi, and a pair starts on an even register
static const char *krs_argument(unsigned int type, unsigned int place, int wide, char *text)
{
    VerbaLinea line = mmgr_line_over(text, KRS_ARGUMENT_BYTES);
    if (type == KRS_PREDICATE)
    {
        mmgr_line_ch(&line, 'P');
        mmgr_line_decimal(&line, place % 3u, 1u);
    }
    else if (type == KRS_NUMBER)
    {
        mmgr_line_decimal(&line, 1u + place, 1u);
    }
    else
    {
        mmgr_line_ch(&line, 'R');
        mmgr_line_decimal(&line, wide ? (2u * (1u + place)) : (1u + place), 1u);
    }
    (void)mmgr_line_finish(&line);
    return text;
}

// one form of the ruleset: its name, terminated, the parameters it takes, and whether the ruleset writes anything for
// it
struct KrsForm
{
    char name[KRS_NAME_BYTES];
    unsigned int parameters;
    int empty;
};

// the next word of the `length` bytes at `line` from `*at`, with its start into `word` and `*at` after it. The return
// is its length, or 0 where no word remains
static size_t krs_word(const char *line, size_t length, size_t *at, const char **word)
{
    while ((*at < length) && ((line[*at] == ' ') || (line[*at] == '\t') || (line[*at] == '\r')))
    {
        *at += 1u;
    }
    const size_t start = *at;
    while ((*at < length) && (line[*at] != ' ') && (line[*at] != '\t') && (line[*at] != '\r'))
    {
        *at += 1u;
    }
    *word = &line[start];
    return *at - start;
}

// 1 where the `length` bytes at `word` are the terminated `text`
static int krs_word_is(const char *word, size_t length, const char *text)
{
    return (length == EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR)) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = word, .other = text, .bytes = length) == 0);
}

// One line of the ruleset read as a form into `form`: `form <name> <parameter>... = <text>` and the head of a
// construct, `construct <name> <parameter>...`. 0 where the line is neither
static int krs_form_read(const char *line, size_t length, KrsForm *form)
{
    size_t at = 0u;
    const char *word = NULL;
    size_t size = krs_word(line, length, &at, &word);
    if (!krs_word_is(word, size, "form") && !krs_word_is(word, size, "construct"))
    {
        return 0;
    }
    size = krs_word(line, length, &at, &word);
    VerbaLinea name = mmgr_line_over(form->name, sizeof(form->name));
    (void)mmgr_line_put_n(&name, word, size);
    (void)mmgr_line_finish(&name);
    form->parameters = 0u;
    form->empty = 0;
    for (size = krs_word(line, length, &at, &word); size != 0u; size = krs_word(line, length, &at, &word))
    {
        if (krs_word_is(word, size, "="))
        {
            // a form whose text is empty writes nothing: its line ends at its equals
            form->empty = krs_word(line, length, &at, &word) == 0u;
            break;
        }
        form->parameters += 1u;
    }
    return 1;
}

// The ruleset's forms read out of its own lines and its part's into a cell of the run, `*count` of them, which the
// caller gives back. NULL where the ruleset did not read or the pool cannot meet the cell
static KrsForm *krs_forms(const Ruleset *rules, const char *path, unsigned int *count)
{
    *count = 0u;
    PlatformText text;
    ruleset_text_begun(&text);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    (void)ruleset_text(rules->schema, path, &text, &error);
    const size_t size = (size_t)platform_text_size(&text);
    const char *const bytes = platform_text_bytes(&text);
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; (bytes != NULL) && (at < size); at += 1u)
    {
        lines += (bytes[at] == '\n') ? 1ULL : 0ULL;
    }
    KrsForm *const forms =
        (bytes != NULL) ? (KrsForm *)platform_cell_cleared(lines, sizeof(KrsForm), platform_place_g_program_owner,
                                                           PLATFORM_TIER_PERSISTENT)
                        : NULL;
    for (size_t at = 0u; (forms != NULL) && (at < size);)
    {
        // the end of the line, or of the file where no line feed follows. Explicit cast reads the found byte as a byte
        // of the file
        const void *const ended = EMBED_CALL(memor.chr, MemoriaCfg, .src = &bytes[at], .bytes = size - at, .val = '\n');
        const size_t end = (ended != NULL) ? (size_t)((const char *)ended - bytes) : size;
        *count += krs_form_read(&bytes[at], end - at, &forms[*count]) ? 1u : 0u;
        at = end + 1u;
    }
    platform_text_ended(&text);
    return forms;
}

// 1 where the `length` bytes of `text` contain a line the part runs: a label, a note and a blank line are none
static int krs_has_instruction(const char *text, size_t length)
{
    size_t at = 0u;
    while (at < length)
    {
        // the end of the line, or of the text where no line feed follows, and the first byte past its blanks.
        // Explicit cast reads the found byte as a byte of the text
        const void *const ended =
            EMBED_CALL(memor.chr, MemoriaCfg, .src = &text[at], .bytes = length - at, .val = '\n');
        const size_t stop = (ended != NULL) ? (size_t)((const char *)ended - text) : length;
        const size_t first =
            at + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = stop - at, .other = " \t");
        if ((first < stop) && (text[first] != '.') && (text[first] != '/') && (text[first] != '#') &&
            (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[first], .cap = stop - first, .byte = ':') == NULL))
        {
            return 1;
        }
        at = stop + 1u;
    }
    return 0;
}

// form `name` written with its `count` arguments onto `text`, emptied first: 1 where the ruleset wrote it
static int krs_written(const Ruleset *rules, const char *name, const char *const *given, unsigned int count,
                       PlatformText *text)
{
    const RulesetNamedScratch scratch = {krs_scratch, NULL};
    platform_text_reset(text);
    return ruleset_form_written(rules, name, given, count, &scratch, &text->line);
}

// Every label that the forms of the ruleset define, as one preamble onto `preamble`. A branch names a label that
// another form writes (error_open_unless branches to the one that label_error_open writes). The assembler resolves a
// label from the lines of the text that it gets, and a form written alone has none. The call writes each form that
// carries no instruction with every number that a branch here gets. Thus whatever a branch names is in front of it
static void krs_labels(const Ruleset *rules, const KrsForm *forms, unsigned int count, PlatformText *preamble,
                       PlatformText *text)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const KrsForm *const form = &forms[at];
        for (unsigned int number = 1u; (form->empty == 0) && (number <= 8u); number += 1u)
        {
            char value[KRS_ARGUMENT_BYTES];
            VerbaLinea value_line = mmgr_line_over(value, sizeof(value));
            mmgr_line_decimal(&value_line, number, 1u);
            (void)mmgr_line_finish(&value_line);
            const char *given[KRS_PARAMETERS_MAX];
            const unsigned int parameters =
                (form->parameters < KRS_PARAMETERS_MAX) ? form->parameters : KRS_PARAMETERS_MAX;
            for (unsigned int place = 0u; place < parameters; place += 1u)
            {
                given[place] = value;
            }
            if (krs_written(rules, form->name, given, parameters, text) != 0)
            {
                const size_t length = (size_t)platform_text_size(text);
                const char *const bytes = platform_text_bytes(text);
                if (!krs_has_instruction(bytes, length) &&
                    (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = bytes, .cap = length, .byte = ':') != NULL))
                {
                    (void)mmgr_line_put_n(&preamble->line, bytes, length);
                }
            }
        }
    }
}

// how one form came out
enum KrsOutcome
{
    KRS_ASSEMBLED = 0,
    KRS_REFUSED = 1,
    KRS_NO_INSTRUCTION = 2,
    KRS_NOT_WRITTEN = 3
};

// Form `form`, written in every way that its parameters can go, and assembled behind `labels`. `wrote` gets the text
// of the assignment that passed, or of the last one tried. `whole` takes the labels and the writing for the assembler
static unsigned int krs_form_assembles(const Ruleset *rules, const SassMachine *machine, const KrsForm *form,
                                       PlatformText *labels, PlatformText *wrote, PlatformText *whole)
{
    const int wide =
        (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = form->name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = "wide", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL) ||
        (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = form->name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = "member", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL);
    const unsigned int parameters = (form->parameters < KRS_PARAMETERS_MAX) ? form->parameters : KRS_PARAMETERS_MAX;
    unsigned int assignments = 1u;
    for (unsigned int at = 0u; (at < parameters) && (at < KRS_PARAMETERS_SEARCHED); at += 1u)
    {
        assignments *= KRS_TYPES;
    }
    unsigned int outcome = KRS_NOT_WRITTEN;
    for (unsigned int assignment = 0u; assignment < assignments; assignment += 1u)
    {
        char values[KRS_PARAMETERS_MAX][KRS_ARGUMENT_BYTES];
        const char *given[KRS_PARAMETERS_MAX];
        unsigned int walk = assignment;
        for (unsigned int place = 0u; place < parameters; place += 1u)
        {
            const unsigned int type = (place < KRS_PARAMETERS_SEARCHED) ? (walk % KRS_TYPES) : KRS_REGISTER;
            walk = (place < KRS_PARAMETERS_SEARCHED) ? (walk / KRS_TYPES) : walk;
            given[place] = krs_argument(type, place, wide, values[place]);
        }
        if (krs_written(rules, form->name, given, parameters, wrote) == 0)
        {
            return KRS_NOT_WRITTEN;
        }
        const size_t length = (size_t)platform_text_size(wrote);
        const char *const bytes = platform_text_bytes(wrote);
        if (!krs_has_instruction(bytes, length))
        {
            return KRS_NO_INSTRUCTION;
        }
        outcome = KRS_REFUSED;
        static unsigned char code[4096];
        // the labels are in front, each at the address the instructions begin at: a branch to one resolves
        platform_text_reset(whole);
        (void)mmgr_line_put_n(&whole->line, platform_text_bytes(labels), (size_t)platform_text_size(labels));
        (void)mmgr_line_put_n(&whole->line, bytes, length);
        if (sass_assemble_lines(machine, platform_text_bytes(whole), SASS_CONTROL_SAFE, code, sizeof(code)) != 0u)
        {
            return KRS_ASSEMBLED;
        }
    }
    return outcome;
}

// a text of the run begun, empty
static void krs_text_begun(PlatformText *text)
{
    platform_text_begun(text, KRS_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
}

int program_main(int count, char **arguments)
{
    const char *const ruleset = (count > 1) ? arguments[1] : "src/cu/transpiler/lstar/protocol/table/sass.krs";
    const char *const path = (count > 2) ? arguments[2] : "src/cu/transpiler/lstar/protocol/table/sm_86.khw";
    // the writing of every form that assembled, stored where a third argument names a file for it
    PlatformFile *const output_file = (count > 3) ? platform_file_open(arguments[3], "w") : NULL;
    static SassMachine machine;
    if (!sass_machine_read(&machine, path))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "the machine file ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " did not read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    const Ruleset *const rules = sass_target().ruleset(1);
    if (rules == NULL)
    {
        return 1;
    }
    unsigned int form_count = 0u;
    KrsForm *const forms = krs_forms(rules, ruleset, &form_count);
    PlatformText labels;
    PlatformText wrote;
    PlatformText whole;
    krs_text_begun(&labels);
    krs_text_begun(&wrote);
    krs_text_begun(&whole);
    krs_labels(rules, forms, form_count, &labels, &wrote);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "machine ");
        mmgr_line_put(output_line, machine.part);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, machine.forms, 1u);
        mmgr_line_put(output_line, " forms; ");
        mmgr_line_put(output_line, ruleset);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, form_count, 1u);
        mmgr_line_put(output_line, " forms");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int assembled = 0u;
    unsigned int empty = 0u;
    unsigned int refused = 0u;
    unsigned int quiet = 0u;
    for (unsigned int at = 0u; at < form_count; at += 1u)
    {
        const KrsForm *const form = &forms[at];
        if (form->empty != 0)
        {
            empty += 1u;
            continue;
        }
        platform_text_reset(&wrote);
        const unsigned int outcome = krs_form_assembles(rules, &machine, form, &labels, &wrote, &whole);
        const size_t length = (size_t)platform_text_size(&wrote);
        const char *const bytes = platform_text_bytes(&wrote);
        if (outcome == KRS_ASSEMBLED)
        {
            assembled += 1u;
            if (output_file != NULL)
            {
                (void)mmgr_line_put_n(platform_file_line(output_file), bytes, length);
                mmgr_line_put(platform_file_line(output_file),
                              ((length != 0u) && (bytes[length - 1u] == '\n')) ? "" : "\n");
            }
        }
        else if (outcome == KRS_NO_INSTRUCTION)
        {
            quiet += 1u;
        }
        else
        {
            refused += 1u;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, form->name);
                mmgr_line_put(output_line, " refused, its last writing ");
                mmgr_line_put(output_line, (outcome == KRS_NOT_WRITTEN) ? "not written" : "");
                mmgr_line_put(output_line, ":\n");
                (void)mmgr_line_put_n(output_line, bytes, length);
                platform_line_out_ended();
            }
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "sass.krs: ");
        mmgr_line_decimal(output_line, assembled, 1u);
        mmgr_line_put(output_line, " forms assembled, ");
        mmgr_line_decimal(output_line, refused, 1u);
        mmgr_line_put(output_line, " refused, ");
        mmgr_line_decimal(output_line, quiet, 1u);
        mmgr_line_put(output_line, " carry no instruction, ");
        mmgr_line_decimal(output_line, empty, 1u);
        mmgr_line_put(output_line, " left empty");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (output_file != NULL)
    {
        platform_file_close(output_file);
    }
    platform_text_ended(&whole);
    platform_text_ended(&wrote);
    platform_text_ended(&labels);
    platform_cell_released(forms, platform_place_g_program_owner);
    return (refused == 0u) ? 0 : 1;
}
