// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_forms.cpp: the forms a lane is written in, each submitted to NVIDIA's compiler in one program, and each form
// of sass.krs and ptx.krs read back off what the compiler wrote for it.
//
//     monolith_forms write <c.krs> <ptx.krs> <monolith.cu> <questions>
//     monolith_forms read <questions> <listing> <ptx> <sass.krs> <ptx.krs> <machine> <record> [apply]
//
// The first decides the lanes of the record programs that the host oracle runs (record_programs.h). It gathers every
// form that they decide, with the banks that its arguments come from. A form decided with the same banks is one
// question.
//
// Each question is a block of the monolith between two tags. Its text is the form's own text in c.krs, the sense that
// the form of every target shares. Each argument that the form reads loads from `in`, and each one that it writes
// stores to `out`, each through a volatile access. Nothing that a block does crosses a tag.
//
// The forms that carry the carry of a chain go in the runs that a lane decides them in. A tag separates each, and the
// carry stays between them. The fixed registers that the text of a form names load the same way.
//
// The second reads the listing of the compiler and its PTX a block at a time:
//   - a register loaded from `in` is the argument that it loads, and a register stored to `out` is the argument stored.
//   - a predicate set from a loaded word, or a word selected from a predicate for a store, is the predicate.
//   - what remains in the block is the form, written with each register named by its argument.
//   - a register or predicate that is no argument is scratch. The first word is R237 and the first predicate P6,
//     sm_86.kdm's own. The tool reports a block that needs more, and does not write it.
//   - a number argument has a name where the number occurs once.
//
// The tool compares each form that it reads with the ruleset's own form, and writes it whole to the record. With apply,
// the tool writes into the ruleset in place each form whose questions all read whole and alike (read_adopted defines
// what whole is).
#include "../../../../../../utils/test/src/cu/engine/analysis/cycle/record_programs.h"
#include "../../../../engine/engine_note.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the cubin writer is C and its headers carry no guard of their own: this file names the linkage here
extern "C"
{
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../sass_assemble.h"
#include "../sass_machine.h"
#include "interface_sass_probe.h"
}

#include "../../../../../platform/platform.h"
#include "../../../../scaffolding/c_target.h"
#include "machine_ir_types.h"
#include "target.h"

#include <stddef.h>

// ENGINE_IMAGE_BASE reads the ELF header that the linker marks with __ehdr_start (engine_config_platform.h). This tool
// links as a PE, where there is none. Nothing here requests the image base: this file names it only so that the link
// resolves
#if defined(__GNUC__) && !defined(__ELF__)
extern "C" const char __ehdr_start = 0;
#endif

// the tags a question's loads and stores are behind, past its own tag, which the form's region is behind
#define MONOLITH_FORMS_LOADS 0x4000u
#define MONOLITH_FORMS_STORES 0x8000u

// the tag a question put in ptx.krs's own text is behind is past this; below it a question put in c.krs's
#define MONOLITH_FORMS_PTX 0x1000u

#define FORM_NAME(name_, text_, parameters_, type_) text_,
static const char *const s_form_names[OPCODE_COUNT] = {OPCODES(FORM_NAME)};
#undef FORM_NAME
#define BANK_NAME(name_, text_) text_,
static const char *const s_bank_names[REGCLASS_COUNT] = {REGCLASSES(BANK_NAME)};
#undef BANK_NAME
#define FIXED_NAME(name_, text_) text_,
static const char *const s_fixed_names[PHYSREG_COUNT] = {PHYSREGS(FIXED_NAME)};
#undef FIXED_NAME

// The bytes of a name, a word or a number, and of a line, an operand or a key, each with its terminator. A text past
// its bound ends the run with the reason, and the tool never cuts a text short
#define FORMS_WORD_BYTES 64u
#define FORMS_LINE_BYTES 512u

// the bytes of a text that one call builds and gives back, and of a text that grows over the run, each at max
#define FORMS_TEXT_MAX (1ULL << 24u)
#define FORMS_KEPT_MAX (1ULL << 26u)

// the parameters of one form at max
#define FORMS_PARAMETERS_MAX 32u

// a name, a word or a number, inline
struct FormsWord
{
    char text[FORMS_WORD_BYTES];
};

// a line, an operand or a key, inline
struct FormsLine
{
    char text[FORMS_LINE_BYTES];
};

// `why`, then the `length` bytes at `piece`, written to the engine's note, and the run ended
static void forms_refused(const char *why, const char *piece, size_t length)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, "monolith_forms: ");
    mmgr_line_put(&engine_note.line, why);
    (void)mmgr_line_put_n(&engine_note.line, piece, length);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
    platform_process_ended(2);
}

// `pointer`, or the run ended where the pool gave no cell
static void *forms_allocated(void *pointer)
{
    if (pointer == NULL)
    {
        forms_refused("the pool gave no cell", "", 0u);
    }
    return pointer;
}

// the bytes of the terminated `text`
static size_t forms_length(const char *text)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
}

// The `length` bytes at `text` put into the `room` bytes at `into` after the `at` bytes it holds, terminated. A text
// past the room ends the run, with `what` and the text as the reason
static void forms_put_at(char *into, size_t room, size_t at, const char *text, size_t length, const char *what)
{
    if ((at + length) >= room)
    {
        forms_refused(what, text, length);
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &into[at], .src = text, .bytes = length);
    into[at + length] = '\0';
}

static void forms_set_n(char *into, size_t room, const char *text, size_t length)
{
    forms_put_at(into, room, 0u, text, length, "a text passes its room: ");
}

static void forms_add_n(char *into, size_t room, const char *text, size_t length)
{
    forms_put_at(into, room, forms_length(into), text, length, "a text passes its room: ");
}

static void forms_add(char *into, size_t room, const char *text)
{
    forms_add_n(into, room, text, forms_length(text));
}

static void forms_add_decimal(char *into, size_t room, unsigned long long value)
{
    char digits[24];
    VerbaLinea line = mmgr_line_over(digits, sizeof(digits));
    mmgr_line_decimal(&line, value, 1u);
    (void)mmgr_line_finish(&line);
    forms_add(into, room, digits);
}

static void forms_add_signed(char *into, size_t room, long long value)
{
    char digits[24];
    VerbaLinea line = mmgr_line_over(digits, sizeof(digits));
    mmgr_line_decimal_signed(&line, value);
    (void)mmgr_line_finish(&line);
    forms_add(into, room, digits);
}

#define FORMS_SET(to_, from_) forms_set_n((to_).text, sizeof((to_).text), (from_), forms_length(from_))
#define FORMS_SET_N(to_, from_, length_) forms_set_n((to_).text, sizeof((to_).text), (from_), (length_))
#define FORMS_ADD(to_, from_) forms_add((to_).text, sizeof((to_).text), (from_))
#define FORMS_ADD_N(to_, from_, length_) forms_add_n((to_).text, sizeof((to_).text), (from_), (length_))
#define FORMS_ADD_DECIMAL(to_, value_) forms_add_decimal((to_).text, sizeof((to_).text), (value_))
#define FORMS_ADD_SIGNED(to_, value_) forms_add_signed((to_).text, sizeof((to_).text), (value_))

// 1 where the terminated texts are the same
static int forms_same(const char *left, const char *right)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = right,
                      .ci = EMBED_FALSE);
}

// 1 where the `length` bytes at `text` are the terminated `other`
static int forms_same_n(const char *text, size_t length, const char *other)
{
    return (forms_length(other) == length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = text, .other = other, .bytes = length) == 0);
}

// 1 where the terminated `text` begins with the terminated `prefix`
static int forms_begins(const char *text, const char *prefix)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = prefix,
                      .ci = EMBED_FALSE);
}

// the order of the terminated texts, as the bytes read unsigned order them: below 0, 0 or above 0
static int forms_order(const char *left, const char *right)
{
    const size_t left_length = forms_length(left);
    const size_t right_length = forms_length(right);
    const size_t reach = ((left_length < right_length) ? left_length : right_length) + 1u;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// The place of the terminated `needle` in the `length` bytes at `text` from `from` on, or `length` where it is not
// there. An empty needle is at `from`
static size_t forms_find_n(const char *text, size_t length, const char *needle, size_t from)
{
    const size_t size = forms_length(needle);
    if ((from > length) || (size > (length - from)))
    {
        return length;
    }
    if (size == 0u)
    {
        return from;
    }
    const char *const found = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[from], .cap = length - from,
                                         .other = needle, .other_cap = size, .ci = EMBED_FALSE);
    // a found needle lies within the text, at or past `from`
    return (found != NULL) ? (size_t)(found - text) : length;
}

// the place of the terminated `needle` in the terminated `text` from `from` on, or the text's length
static size_t forms_find(const char *text, const char *needle, size_t from)
{
    return forms_find_n(text, forms_length(text), needle, from);
}

// 1 where the terminated `text` holds the terminated `needle`
static int forms_has(const char *text, const char *needle)
{
    const size_t length = forms_length(text);
    return forms_find_n(text, length, needle, 0u) < length;
}

// the place of the byte `byte` in the `length` bytes at `text` from `from` on, or `length`
static size_t forms_chr_n(const char *text, size_t length, char byte, size_t from)
{
    const char *const found = (from < length)
                                  ? (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = &text[from],
                                                             .bytes = length - from, .val = (unsigned char)byte)
                                  : NULL;
    // a found byte lies within the text, at or past `from`
    return (found != NULL) ? (size_t)(found - text) : length;
}

// the place of the last byte `byte` in the first `length` bytes at `text`, or `length` where none is there
static size_t forms_chr_last(const char *text, size_t length, char byte)
{
    const char *const found =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = text, .cap = length, .byte = (uint8_t)byte);
    // a found byte lies within the text
    return (found != NULL) ? (size_t)(found - text) : length;
}

// 1 where `character` is a byte of an identifier: a character, a digit or `_`
static int forms_identifier(char character)
{
    // a byte of a text read as the unsigned byte the class takes
    return (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)character) != 0) ||
           (character == '_');
}

// The end of the last run of bytes outside `set` in the `length` bytes at `text`, or 0 where every byte is in `set`.
// The walk jumps from run to run
static size_t forms_end_outside(const char *text, size_t length, const char *set)
{
    size_t end = 0u;
    size_t at = 0u;
    while (at < length)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = set);
        at += run;
        end = (run != 0u) ? at : end;
        const size_t inside =
            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = set);
        at += inside;
        if ((run == 0u) && (inside == 0u))
        {
            break;
        }
    }
    return end;
}

// the `length` bytes at `text` with the bytes of `set` at either end cut, as a piece: its first byte into `first`, and
// its bytes the answer
static size_t forms_trimmed(const char *text, size_t length, const char *set, size_t *first)
{
    *first = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = length, .other = set);
    return forms_end_outside(&text[*first], length - *first, set);
}

// the `length` bytes at `text` with the spaces and tabs at either end cut, put into `trimmed`
static FormsLine forms_trim_n(const char *text, size_t length)
{
    size_t first = 0u;
    const size_t size = forms_trimmed(text, length, " \t", &first);
    FormsLine trimmed;
    FORMS_SET_N(trimmed, &text[first], size);
    return trimmed;
}

static FormsLine forms_trim(const char *text)
{
    return forms_trim_n(text, forms_length(text));
}

// The count of the places of the terminated `part` in the terminated `text`. The search for each goes on past the one
// before it
static size_t forms_count(const char *text, const char *part)
{
    const size_t length = forms_length(text);
    const size_t size = forms_length(part);
    size_t count = 0u;
    for (size_t at = forms_find_n(text, length, part, 0u); (size != 0u) && (at < length);
         at = forms_find_n(text, length, part, at + size))
    {
        count += 1u;
    }
    return count;
}

// 1 where `name` is in `text` as a whole identifier
static int forms_names(const char *text, const char *name)
{
    const size_t length = forms_length(text);
    const size_t size = forms_length(name);
    for (size_t at = forms_find_n(text, length, name, 0u); at < length; at = forms_find_n(text, length, name, at + 1u))
    {
        const int before = (at == 0u) || !forms_identifier(text[at - 1u]);
        const int after = ((at + size) >= length) || !forms_identifier(text[at + size]);
        if (before && after)
        {
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// the texts a call builds

// a text of the temporary tier, begun empty, for a text that one call builds and gives back before it returns
static void forms_text_begun(PlatformText *text)
{
    platform_text_begun(text, FORMS_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

// a text of the movable tier, begun empty, for a text that grows large and lasts past the call that begins it
static void forms_kept_begun(PlatformText *text)
{
    platform_text_begun(text, FORMS_KEPT_MAX, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
}

// the bytes of `text`, terminated [BORROWS], and their count into `size`. A text past its max ends the run
static const char *forms_text_bytes(PlatformText *text, size_t *size)
{
    if (!mmgr_line_ok(&text->line))
    {
        forms_refused("a text passes its max", "", 0u);
    }
    const char *const bytes = (const char *)forms_allocated((void *)platform_text_bytes(text));
    // a text at its max is far below what a size_t counts
    *size = (size_t)platform_text_size(text);
    return bytes;
}

// the bytes of `from` put after the bytes of `to`
static void forms_text_put(PlatformText *to, PlatformText *from)
{
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(from, &size);
    (void)mmgr_line_put_n(&to->line, bytes, size);
}

// `text` with every `{name}` written `put`, in place
static void forms_text_fill(PlatformText *text, const char *name, const char *put)
{
    FormsWord brace;
    FORMS_SET(brace, "{");
    FORMS_ADD(brace, name);
    FORMS_ADD(brace, "}");
    PlatformText filled;
    forms_text_begun(&filled);
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(text, &size);
    const size_t brace_size = forms_length(brace.text);
    size_t at = 0u;
    for (size_t found = forms_find_n(bytes, size, brace.text, 0u); found < size;
         found = forms_find_n(bytes, size, brace.text, at))
    {
        (void)mmgr_line_put_n(&filled.line, &bytes[at], found - at);
        mmgr_line_put(&filled.line, put);
        at = found + brace_size;
    }
    (void)mmgr_line_put_n(&filled.line, &bytes[at], size - at);
    platform_text_reset(text);
    forms_text_put(text, &filled);
    platform_text_ended(&filled);
}

// `text` set to the terminated `from`
static void forms_text_set(PlatformText *text, const char *from)
{
    platform_text_reset(text);
    mmgr_line_put(&text->line, from);
}

// a copy of the bytes of `text` in a text of the temporary tier `copy`, begun here; the caller ends it
static const char *forms_text_copy(PlatformText *text, PlatformText *copy, size_t *size)
{
    forms_text_begun(copy);
    forms_text_put(copy, text);
    return forms_text_bytes(copy, size);
}

// ---------------------------------------------------------------------------------------------------------------
// the rulesets

// The types of a ruleset's line: a form with its parameters, a bank's text and a fixed register's text
enum KrsType
{
    KRS_FORM,
    KRS_BANK,
    KRS_FIXED,
    KRS_TYPES
};

// One line of a ruleset: its name, the place and bytes of its text in the ruleset's text cell, and its parameters from
// `parameter_first`, `parameter_count` of them
struct KrsEntry
{
    char name[FORMS_WORD_BYTES];
    size_t text;
    size_t length;
    unsigned int parameter_first;
    unsigned int parameter_count;
};

// A ruleset as this reads it: its texts, a form's with the escapes read, in one cell. Then the lines of each type in a
// cell of the type, in the order of their names, each once. Then the parameters of the forms in one cell. Each cell is
// of the persistent tier, taken once at its count
struct Krs
{
    char *text;
    KrsEntry *entries[KRS_TYPES];
    unsigned int counts[KRS_TYPES];
    FormsWord *parameters;
    unsigned int parameter_count;
};

// the text of `entry` of `krs`, terminated [BORROWS]
static const char *krs_text(const Krs *krs, const KrsEntry *entry)
{
    return &krs->text[entry->text];
}

// the line of `type` named `name`, or NULL where the ruleset has none
static const KrsEntry *krs_find(const Krs *krs, unsigned int type, const char *name)
{
    const KrsEntry *const entries = krs->entries[type];
    unsigned int low = 0u;
    unsigned int high = krs->counts[type];
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (forms_order(entries[middle].name, name) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < krs->counts[type]) && forms_same(entries[low].name, name)) ? &entries[low] : NULL;
}

// the text of the form, the bank or the fixed register named `name`, or "" where the ruleset has none [BORROWS]
static const char *krs_text_of(const Krs *krs, unsigned int type, const char *name)
{
    const KrsEntry *const entry = krs_find(krs, type, name);
    return (entry != NULL) ? krs_text(krs, entry) : "";
}

static int krs_entry_order(const void *left, const void *right)
{
    return forms_order(((const KrsEntry *)left)->name, ((const KrsEntry *)right)->name);
}

// the `length` bytes at `text` with \t, \n and \\ read, put into `into` from `*at`, terminated, and `*at` past them
static void krs_unescaped(const char *text, size_t length, char *into, size_t *at)
{
    size_t from = 0u;
    while (from < length)
    {
        const size_t slash = forms_chr_n(text, length, '\\', from);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &into[*at], .src = &text[from], .bytes = slash - from);
        *at += slash - from;
        from = slash;
        if (from >= length)
        {
            break;
        }
        if ((from + 1u) >= length)
        {
            into[*at] = '\\';
            *at += 1u;
            from += 1u;
            break;
        }
        const char next = text[from + 1u];
        into[*at] = (next == 't') ? '\t' : ((next == 'n') ? '\n' : next);
        *at += 1u;
        from += 2u;
    }
    into[*at] = '\0';
    *at += 1u;
}

// The lines of the ruleset in `bytes`, counted where `filling` is 0 and written where it is 1. A line is
// `form <name> <parameters> = <text>`, `bank <name> = <text>` or `fixed <name> = <text>`. The code leaves out every
// carriage return of a line. `line` is room for one line, and `bytes_counted` counts the bytes of the texts
static void krs_lines_made(const unsigned char *bytes, unsigned long long size, Krs *krs, int filling, char *line,
                           size_t line_room, size_t *bytes_counted)
{
    *bytes_counted = 0u;
    krs->parameter_count = 0u;
    for (unsigned int type = 0u; type < KRS_TYPES; type += 1u)
    {
        krs->counts[type] = 0u;
    }
    // a ruleset the run holds whole is at max what a size_t counts
    const char *const text = (const char *)bytes;
    const size_t whole = (size_t)size;
    size_t at = 0u;
    while (at < whole)
    {
        const size_t end = forms_chr_n(text, whole, '\n', at);
        // the line put together with its carriage returns left out
        size_t length = 0u;
        for (size_t from = at; from < end;)
        {
            const size_t carriage = forms_chr_n(text, end, '\r', from);
            if ((length + (carriage - from)) >= line_room)
            {
                forms_refused("a line of a ruleset passes its room: ", &text[at], end - at);
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &line[length], .src = &text[from], .bytes = carriage - from);
            length += carriage - from;
            from = carriage + 1u;
        }
        line[length] = '\0';
        at = end + 1u;
        const size_t equals = forms_find_n(line, length, " = ", 0u);
        if (equals == length)
        {
            continue;
        }
        // the words of the head, apart by spaces
        FormsWord head[FORMS_PARAMETERS_MAX + 2u];
        unsigned int words = 0u;
        for (size_t word = 0u; word < equals;)
        {
            word += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[word], .cap = equals - word, .other = " ");
            const size_t run =
                EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[word], .cap = equals - word, .other = " ");
            if (run == 0u)
            {
                break;
            }
            if (words == (FORMS_PARAMETERS_MAX + 2u))
            {
                forms_refused("a line of a ruleset names more parameters than a form holds: ", line, length);
            }
            FORMS_SET_N(head[words], &line[word], run);
            words += 1u;
            word += run;
        }
        const int form = (words >= 2u) && forms_same(head[0].text, "form");
        const int bank = (words == 2u) && forms_same(head[0].text, "bank");
        const int fixed = (words == 2u) && forms_same(head[0].text, "fixed");
        if (!form && !bank && !fixed)
        {
            continue;
        }
        const unsigned int type = form ? KRS_FORM : (bank ? KRS_BANK : KRS_FIXED);
        const char *const body = &line[equals + 3u];
        const size_t body_length = length - equals - 3u;
        if (filling)
        {
            KrsEntry *const entry = &krs->entries[type][krs->counts[type]];
            forms_set_n(entry->name, sizeof(entry->name), head[1].text, forms_length(head[1].text));
            entry->text = *bytes_counted;
            entry->parameter_first = krs->parameter_count;
            entry->parameter_count = form ? (words - 2u) : 0u;
            size_t written = *bytes_counted;
            if (form)
            {
                krs_unescaped(body, body_length, krs->text, &written);
            }
            else
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &krs->text[written], .src = body, .bytes = body_length);
                krs->text[written + body_length] = '\0';
                written += body_length + 1u;
            }
            entry->length = written - *bytes_counted - 1u;
            *bytes_counted = written;
            for (unsigned int parameter = 2u; form && (parameter < words); parameter += 1u)
            {
                krs->parameters[krs->parameter_count + parameter - 2u] = head[parameter];
            }
        }
        else
        {
            *bytes_counted += body_length + 1u;
        }
        krs->parameter_count += form ? (words - 2u) : 0u;
        krs->counts[type] += 1u;
    }
}

// the room of one line of a ruleset as the reader puts it together
#define KRS_LINE_BYTES 65536u

// The ruleset at `path` read into `krs`, its lines counted first, then each cell taken at its count, then the lines
// written. The lines of each type then go in the order of their names, and a name a ruleset gives twice keeps its last
// line. 1, or 0 where the ruleset did not read
static int krs_read(const char *path, Krs *krs)
{
    // the ruleset as the run stores it, or the disk where the run stores none (qry_member_file)
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return 0;
    }
    static char s_line[KRS_LINE_BYTES];
    size_t text_bytes = 0u;
    krs_lines_made(bytes, size, krs, 0, s_line, sizeof(s_line), &text_bytes);
    krs->text = (char *)forms_allocated(
        platform_cell_taken(text_bytes + 1u, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    for (unsigned int type = 0u; type < KRS_TYPES; type += 1u)
    {
        krs->entries[type] = (KrsEntry *)forms_allocated(
            platform_cell_cleared((krs->counts[type] != 0u) ? krs->counts[type] : 1u, sizeof(KrsEntry),
                                  platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    }
    krs->parameters = (FormsWord *)forms_allocated(
        platform_cell_cleared((krs->parameter_count != 0u) ? krs->parameter_count : 1u, sizeof(FormsWord),
                              platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    krs_lines_made(bytes, size, krs, 1, s_line, sizeof(s_line), &text_bytes);
    for (unsigned int type = 0u; type < KRS_TYPES; type += 1u)
    {
        KrsEntry *const entries = krs->entries[type];
        const unsigned int count = krs->counts[type];
        if (count >= 2u)
        {
            void *const spare = forms_allocated(platform_cell_taken(
                (unsigned long long)count * sizeof(KrsEntry), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
            (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = entries, .bytes = sizeof(KrsEntry), .scratch = spare,
                             .count = count, .order = krs_entry_order);
            platform_cell_released(spare, platform_place_g_program_owner);
        }
        unsigned int kept = 0u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (((at + 1u) == count) || !forms_same(entries[at].name, entries[at + 1u].name))
            {
                entries[kept] = entries[at];
                kept += 1u;
            }
        }
        krs->counts[type] = kept;
    }
    return 1;
}

// the parameter `at` of the form `entry` of `krs` [BORROWS]
static const char *krs_parameter(const Krs *krs, const KrsEntry *entry, unsigned int at)
{
    return krs->parameters[entry->parameter_first + at].text;
}

// ---------------------------------------------------------------------------------------------------------------
// the C types the lane's registers contain, read off c.krs

// a variable that a ruleset declares and the type of its declaration, and the order the reader met it in
struct FormsDeclared
{
    char name[FORMS_WORD_BYTES];
    char type[FORMS_WORD_BYTES];
};

// the declarations a ruleset makes, in one cell in the order of their names, each once: `count` of them
struct FormsTypes
{
    FormsDeclared *declared;
    unsigned int count;
};

static int forms_declared_order(const void *left, const void *right)
{
    return forms_order(((const FormsDeclared *)left)->name, ((const FormsDeclared *)right)->name);
}

// The `count` declarations at `declared` put in the order of their names, stable, and each name kept once. The first of
// a name stays where `first` is 1, and the last where it is 0
static unsigned int forms_declared_settled(FormsDeclared *declared, unsigned int count, int first)
{
    if (count >= 2u)
    {
        void *const spare =
            forms_allocated(platform_cell_taken((unsigned long long)count * sizeof(FormsDeclared),
                                                platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = declared, .bytes = sizeof(FormsDeclared), .scratch = spare,
                         .count = count, .order = forms_declared_order);
        platform_cell_released(spare, platform_place_g_program_owner);
    }
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int opens = (at == 0u) || !forms_same(declared[at - 1u].name, declared[at].name);
        const int closes = ((at + 1u) == count) || !forms_same(declared[at].name, declared[at + 1u].name);
        if (first ? opens : closes)
        {
            declared[kept] = declared[at];
            kept += 1u;
        }
    }
    return kept;
}

// the type `types` declares `name` of, or NULL where it declares none [BORROWS]
static const char *forms_type_of(const FormsTypes *types, const char *name)
{
    unsigned int low = 0u;
    unsigned int high = types->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (forms_order(types->declared[middle].name, name) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < types->count) && forms_same(types->declared[low].name, name)) ? types->declared[low].type : NULL;
}

// The declarations of c.krs, `type a, b, c;` each a line of its own, counted where `declared` is NULL and put at
// `declared` where it is not. The forms go in the order of their names. A line with a bracket, a parenthesis or a `=`
// is no such declaration
static unsigned int c_declared_made(const Krs *c, FormsDeclared *declared)
{
    unsigned int count = 0u;
    for (unsigned int form = 0u; form < c->counts[KRS_FORM]; form += 1u)
    {
        const KrsEntry *const entry = &c->entries[KRS_FORM][form];
        const char *const text = krs_text(c, entry);
        const size_t size = entry->length;
        for (size_t at = 0u; at < size;)
        {
            const size_t stop = forms_chr_n(text, size, '\n', at);
            const char *const line = &text[at];
            const size_t length = stop - at;
            at = stop + 1u;
            const size_t first = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line, .cap = length, .other = " ");
            if ((first == length) || (forms_chr_n(line, length, '[', 0u) < length) ||
                (forms_chr_n(line, length, '(', 0u) < length) || (forms_chr_n(line, length, '=', 0u) < length) ||
                (line[length - 1u] != ';'))
            {
                continue;
            }
            // the declaration, its spaces before and its `;` cut
            const char *const kept = &line[first];
            const size_t kept_length = length - first - 1u;
            const size_t space = forms_chr_n(kept, kept_length, ' ', 0u);
            if (space == kept_length)
            {
                continue;
            }
            const char *const names = &kept[space + 1u];
            const size_t names_length = kept_length - space - 1u;
            for (size_t from = 0u; from < names_length;)
            {
                const size_t until = forms_chr_n(names, names_length, ',', from);
                size_t name_first = 0u;
                const size_t name_length = forms_trimmed(&names[from], until - from, " ", &name_first);
                if ((name_length != 0u) && (declared != NULL))
                {
                    forms_set_n(declared[count].name, sizeof(declared[count].name), &names[from + name_first],
                                name_length);
                    forms_set_n(declared[count].type, sizeof(declared[count].type), kept, space);
                }
                count += (name_length != 0u) ? 1u : 0u;
                from = until + 1u;
            }
        }
    }
    return count;
}

// The C type each variable c.krs declares in a list of its own, `type a, b, c;`, by the variable's name. The first
// declaration of a name, in the order of the forms' names, holds
static FormsTypes c_declared(const Krs *c)
{
    FormsTypes types;
    const unsigned int count = c_declared_made(c, NULL);
    types.declared = (FormsDeclared *)forms_allocated(platform_cell_cleared(
        (count != 0u) ? count : 1u, sizeof(FormsDeclared), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    types.count = forms_declared_settled(types.declared, c_declared_made(c, types.declared), 1);
    return types;
}

// the C type that a bank's registers contain: the type that its text casts to, or the type that declares its name as an
// array
static FormsWord c_bank_type(const Krs *c, const char *bank)
{
    FormsWord type;
    type.text[0] = '\0';
    const KrsEntry *const found = krs_find(c, KRS_BANK, bank);
    if (found == NULL)
    {
        return type;
    }
    const char *const text = krs_text(c, found);
    const size_t length = found->length;
    if (forms_begins(text, "(("))
    {
        const size_t space = forms_chr_n(text, length, ' ', 2u);
        if (space < length)
        {
            FORMS_SET_N(type, &text[2], space - 2u);
        }
        return type;
    }
    const size_t bracket = forms_chr_n(text, length, '[', 0u);
    if (bracket == length)
    {
        return type;
    }
    FormsLine array;
    FORMS_SET(array, " ");
    FORMS_ADD_N(array, text, bracket);
    FORMS_ADD(array, "[");
    for (unsigned int form = 0u; form < c->counts[KRS_FORM]; form += 1u)
    {
        const KrsEntry *const entry = &c->entries[KRS_FORM][form];
        const char *const form_text = krs_text(c, entry);
        const size_t at = forms_find_n(form_text, entry->length, array.text, 0u);
        if (at == entry->length)
        {
            continue;
        }
        // the space before the type: the last at or before the place before `at`, or the last of the text where `at`
        // is 0
        const size_t searched = (at == 0u) ? entry->length : at;
        const size_t space = forms_chr_last(form_text, searched, ' ');
        const size_t begin = (space == searched) ? 0u : (space + 1u);
        FORMS_SET_N(type, &form_text[begin], (begin <= at) ? (at - begin) : (entry->length - begin));
        return type;
    }
    return type;
}

// ---------------------------------------------------------------------------------------------------------------
// the questions

// one argument of a decided form, as a question puts it: a register of a bank, a fixed register or a number
struct Argument
{
    unsigned int type;
    unsigned int which;
    unsigned int number;
};

// one form a lane decides: its place in the schema and its arguments
struct LaneForm
{
    unsigned int form;
    unsigned int count;
    Argument arguments[MACHINE_INSTR_OPERANDS];
};

// what the arguments of a submitted form come from, and two of them alike are one question
static FormsLine lane_form_key(const LaneForm *lane_form)
{
    FormsLine key;
    FORMS_SET(key, s_form_names[lane_form->form]);
    for (unsigned int at = 0u; at < lane_form->count; at += 1u)
    {
        const Argument *const argument = &lane_form->arguments[at];
        if (argument->type == OPERAND_REGISTER)
        {
            FORMS_ADD(key, " r");
            FORMS_ADD_DECIMAL(key, argument->which);
        }
        else if (argument->type == OPERAND_PHYSREG)
        {
            FORMS_ADD(key, " f");
            FORMS_ADD_DECIMAL(key, argument->which);
        }
        else
        {
            FORMS_ADD(key, " n");
        }
    }
    return key;
}

// 1 where the two forms have one key (lane_form_key): one form, and arguments from the same banks
static int lane_form_alike(const LaneForm *one, const LaneForm *other)
{
    if ((one->form != other->form) || (one->count != other->count))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < one->count; at += 1u)
    {
        const Argument *const left = &one->arguments[at];
        const Argument *const right = &other->arguments[at];
        const int left_register = (left->type == OPERAND_REGISTER) || (left->type == OPERAND_PHYSREG);
        const int right_register = (right->type == OPERAND_REGISTER) || (right->type == OPERAND_PHYSREG);
        if ((left_register != right_register) || (left_register && (left->type != right->type)) ||
            (left_register && (left->which != right->which)))
        {
            return 0;
        }
    }
    return 1;
}

// 1 where c.krs's text for the form writes the chain's carry, and through `reads` whether it reads it
static int form_carries(const Krs *c, const char *name, int *reads)
{
    const char *const text = krs_text_of(c, KRS_FORM, name);
    const size_t named = forms_count(text, "carry");
    const size_t written = forms_count(text, "&carry");
    *reads = (named > written) ? 1 : 0;
    return (written != 0u) ? 1 : 0;
}

// 1 where the form is a question. `c.krs` gives it a text that does something with its arguments, and not a
// declaration, a label, a note, a jump or the resident
static int form_is_question(const Krs *c, const char *name)
{
    const KrsEntry *const found = krs_find(c, KRS_FORM, name);
    if ((found == NULL) || (found->parameter_count == 0u))
    {
        return 0;
    }
    const char *const text = krs_text(c, found);
    const size_t length = found->length;
    const size_t first = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = length, .other = " \t\n");
    if ((first == length) || forms_begins(&text[first], "//") || forms_has(text, "goto") || forms_has(text, "return") ||
        forms_has(text, "__global__") || (text[length - 1u] != '\n') || forms_has(text, ":\n"))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < found->parameter_count; at += 1u)
    {
        FormsWord indexed;
        FORMS_SET(indexed, "[{");
        FORMS_ADD(indexed, krs_parameter(c, found, at));
        FORMS_ADD(indexed, "}]");
        if (forms_has(text, indexed.text))
        {
            return 0;
        }
    }
    return 1;
}

// the questions and their lane forms at max, and the forms of one run of forms that carry at max
#define FORMS_QUESTIONS_MAX 4096u
#define FORMS_LANE_FORMS_MAX 16384u
#define FORMS_RUN_MAX 1024u

// A question gathered from the lanes: each form submitted alone, or a run of forms that carry, `count` of them from
// `first` of s_lane_forms. The tables are static at a bound, in the order the lanes give them
struct Gathered
{
    unsigned int first;
    unsigned int count;
};

static LaneForm s_lane_forms[FORMS_LANE_FORMS_MAX];
static unsigned int s_lane_form_count = 0u;
static Gathered s_gathered[FORMS_QUESTIONS_MAX];
static unsigned int s_gathered_count = 0u;

// each number of the `count` forms at `merged` raised to the one `seen` puts in its place where the number is 0 or 1
static void gather_number(LaneForm *merged, const LaneForm *seen, unsigned int count)
{
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        for (unsigned int at = 0u; at < merged[one].count; at += 1u)
        {
            Argument *const argument = &merged[one].arguments[at];
            const unsigned int now = seen[one].arguments[at].number;
            const int number = (argument->type == OPERAND_NUMBER) || (argument->type == OPERAND_SIGNED) ||
                               ((argument->type == OPERAND_REGISTER) && (argument->which == REGCLASS_IMMEDIATE));
            // a number of 0 or 1 is the likeliest for the compiler to meet with a constant of its own
            if (number && (argument->number < 2u) && (now >= 2u))
            {
                argument->number = now;
            }
        }
    }
}

// The `count` forms at `run` gathered as one question, a form that repeats the one before it once. A question alike
// to one gathered before raises that one's numbers, and is no question of its own
static void gather_add(const LaneForm *run, unsigned int count)
{
    if (count == 0u)
    {
        return;
    }
    // the run put after the lane forms, a repeat of the form before it left out
    const unsigned int first = s_lane_form_count;
    unsigned int merged = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((merged != 0u) && lane_form_alike(&s_lane_forms[first + merged - 1u], &run[at]))
        {
            continue;
        }
        if ((first + merged) == FORMS_LANE_FORMS_MAX)
        {
            forms_refused("more lane forms than the table holds", "", 0u);
        }
        s_lane_forms[first + merged] = run[at];
        merged += 1u;
    }
    for (unsigned int question = 0u; question < s_gathered_count; question += 1u)
    {
        const Gathered *const before = &s_gathered[question];
        int alike = (before->count == merged);
        for (unsigned int at = 0u; alike && (at < merged); at += 1u)
        {
            alike = lane_form_alike(&s_lane_forms[before->first + at], &s_lane_forms[first + at]);
        }
        if (alike)
        {
            gather_number(&s_lane_forms[before->first], &s_lane_forms[first], merged);
            return;
        }
    }
    if (s_gathered_count == FORMS_QUESTIONS_MAX)
    {
        forms_refused("more questions than the table holds", "", 0u);
    }
    s_gathered[s_gathered_count].first = first;
    s_gathered[s_gathered_count].count = merged;
    s_gathered_count += 1u;
    s_lane_form_count += merged;
}

// a line on the output, `head`, `name` and `tail`
static void forms_said(const char *head, const char *name, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, head);
    mmgr_line_put(output_line, name);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

static void gather_lane(const Krs *c, const HostProgram *program, int reuse)
{
    HostLoaded loaded;
    if (host_load(program, reuse, &loaded) == 0)
    {
        forms_said("  ", program->name, ": not arranged");
        return;
    }
    PlatformArray decided;
    platform_array_begun(&decided, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    unsigned int places = 0u;
    if (c_target().decided(&loaded.layout, &places, &decided) == 0)
    {
        platform_array_ended(&decided);
        forms_said("  ", program->name, ": the core decides no lane for it");
        host_free(&loaded);
        return;
    }
    static LaneForm s_run[FORMS_RUN_MAX];
    unsigned int run = 0u;
    const MachineInstr *const items = (const MachineInstr *)decided.cell;
    for (unsigned long long one = 0ULL; one < decided.count; one += 1ULL)
    {
        const MachineInstr *const item = &items[one];
        if ((item->form >= OPCODE_COUNT) || !form_is_question(c, s_form_names[item->form]))
        {
            continue;
        }
        LaneForm lane_form;
        lane_form.form = item->form;
        lane_form.count = codegen_operand_count(item->form);
        for (unsigned int at = 0u; at < lane_form.count; at += 1u)
        {
            lane_form.arguments[at].type = item->arguments[at].type;
            lane_form.arguments[at].which = item->arguments[at].which;
            lane_form.arguments[at].number = item->arguments[at].number;
        }
        int reads = 0;
        const int writes = form_carries(c, s_form_names[item->form], &reads);
        if (!writes && !reads)
        {
            gather_add(&lane_form, 1u);
            continue;
        }
        // a form that writes the carry and does not read it opens a run, and the run before it ends
        if (writes && !reads)
        {
            gather_add(s_run, run);
            run = 0u;
        }
        if (run == FORMS_RUN_MAX)
        {
            forms_refused("a run of forms that carry passes the room of a run", "", 0u);
        }
        s_run[run] = lane_form;
        run += 1u;
    }
    gather_add(s_run, run);
    platform_array_ended(&decided);
    host_free(&loaded);
}

// the operands of an instruction at max
#define FORMS_OPERANDS_MAX 16u

// the bytes of an operand, with its terminator
#define FORMS_OPERAND_BYTES 256u

struct FormsOperand
{
    char text[FORMS_OPERAND_BYTES];
};

// an instruction of a listing: its guard, its operation and its operands
struct Instruction
{
    FormsWord guard;
    FormsWord operation;
    FormsOperand operands[FORMS_OPERANDS_MAX];
    unsigned int operand_count;
};

// The operands of the `length` bytes at `text` split at every comma outside brackets and braces, each with its spaces
// and tabs cut, into `instruction`. The walk keeps a running depth, and jumps from one bracket, brace or comma to the
// next. The last operand counts only where it is not empty
static void operands_split(const char *text, size_t length, Instruction *instruction)
{
    instruction->operand_count = 0u;
    int depth = 0;
    size_t from = 0u;
    size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = length, .other = ",[]{}");
    while (at < length)
    {
        const char character = text[at];
        depth += ((character == '[') || (character == '{')) ? 1 : 0;
        depth -= ((character == ']') || (character == '}')) ? 1 : 0;
        if ((character == ',') && (depth == 0))
        {
            if (instruction->operand_count == FORMS_OPERANDS_MAX)
            {
                forms_refused("an instruction names more operands than it holds: ", text, length);
            }
            const FormsLine one = forms_trim_n(&text[from], at - from);
            FORMS_SET(instruction->operands[instruction->operand_count], one.text);
            instruction->operand_count += 1u;
            from = at + 1u;
        }
        at += 1u;
        at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = ",[]{}");
    }
    const FormsLine last = forms_trim_n(&text[from], length - from);
    if (last.text[0] != '\0')
    {
        if (instruction->operand_count == FORMS_OPERANDS_MAX)
        {
            forms_refused("an instruction names more operands than it holds: ", text, length);
        }
        FORMS_SET(instruction->operands[instruction->operand_count], last.text);
        instruction->operand_count += 1u;
    }
}

// The instruction of the `length` bytes at `text`. That is a guard after an `@` up to its space, then the operation up
// to its space or tab, then the operands
static Instruction instruction_read_n(const char *text, size_t length)
{
    Instruction instruction;
    instruction.guard.text[0] = '\0';
    instruction.operation.text[0] = '\0';
    instruction.operand_count = 0u;
    size_t first = 0u;
    size_t size = forms_trimmed(text, length, " \t", &first);
    const char *rest = &text[first];
    if ((size != 0u) && (rest[0] == '@'))
    {
        const size_t space = forms_chr_n(rest, size, ' ', 0u);
        FORMS_SET_N(instruction.guard, &rest[1], (space > 1u) ? (space - 1u) : 0u);
        size_t after = 0u;
        const size_t kept = (space < size) ? forms_trimmed(&rest[space], size - space, " \t", &after) : 0u;
        rest = (space < size) ? &rest[space + after] : &rest[size];
        size = kept;
    }
    const size_t space = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = rest, .cap = size, .other = " \t");
    FORMS_SET_N(instruction.operation, rest, space);
    if (space < size)
    {
        operands_split(&rest[space], size - space, &instruction);
    }
    return instruction;
}

static Instruction instruction_read(const char *text)
{
    return instruction_read_n(text, forms_length(text));
}

// ---------------------------------------------------------------------------------------------------------------
// the monolith written

// The writer of the monolith: the rulesets and the types they declare, and the helpers of the lane's opening. Then the
// body and the rows of the questions, each a movable text. Then the tags and the words taken from `in` and `out`
struct Writer
{
    const Krs *c;
    const Krs *ptx;
    FormsTypes declared;
    FormsTypes ptx_types;
    const char *helpers;
    size_t helpers_length;
    PlatformText body;
    PlatformText rows;
    unsigned int questions;
    unsigned int tag;
    unsigned int ptx_tag;
    unsigned int in_words;
    unsigned int out_words;
};

static unsigned int writer_take(unsigned int *words, const char *type)
{
    if (forms_same(type, "u64"))
    {
        *words += (*words & 1u);
        const unsigned int at = *words;
        *words += 2u;
        return at;
    }
    const unsigned int at = *words;
    *words += 1u;
    return at;
}

static FormsLine writer_load(const char *type, unsigned int at)
{
    FormsLine text;
    if (forms_same(type, "u64"))
    {
        FORMS_SET(text, "*(volatile const u64 *)&in[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "]");
    }
    else if (forms_same(type, "int"))
    {
        FORMS_SET(text, "((*(volatile const u32 *)&in[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "] != 0u) ? 1 : 0)");
    }
    else if (forms_same(type, "s8"))
    {
        FORMS_SET(text, "(s8)*(volatile const u32 *)&in[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "]");
    }
    else
    {
        FORMS_SET(text, "*(volatile const u32 *)&in[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "]");
    }
    return text;
}

static FormsLine writer_store(const char *type, unsigned int at, const char *name)
{
    FormsLine text;
    if (forms_same(type, "u64"))
    {
        FORMS_SET(text, "        *(volatile u64 *)&out[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "] = ");
        FORMS_ADD(text, name);
        FORMS_ADD(text, ";\n");
    }
    else if (forms_same(type, "int"))
    {
        FORMS_SET(text, "        *(volatile u32 *)&out[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "] = (");
        FORMS_ADD(text, name);
        FORMS_ADD(text, " != 0) ? 1u : 0u;\n");
    }
    else if (forms_same(type, "s8"))
    {
        FORMS_SET(text, "        *(volatile u32 *)&out[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "] = (u32)(int)");
        FORMS_ADD(text, name);
        FORMS_ADD(text, ";\n");
    }
    else
    {
        FORMS_SET(text, "        *(volatile u32 *)&out[");
        FORMS_ADD_DECIMAL(text, at);
        FORMS_ADD(text, "] = ");
        FORMS_ADD(text, name);
        FORMS_ADD(text, ";\n");
    }
    return text;
}

// the C type and the c.krs variable an argument is: a bank's type, a fixed register's, or none for a number
static FormsWord writer_type(const Writer *writer, const Argument *argument, FormsWord *variable)
{
    FormsWord type;
    type.text[0] = '\0';
    variable->text[0] = '\0';
    if ((argument->type == OPERAND_REGISTER) && (argument->which != REGCLASS_IMMEDIATE))
    {
        return c_bank_type(writer->c, s_bank_names[argument->which]);
    }
    if (argument->type == OPERAND_PHYSREG)
    {
        FORMS_SET(*variable, krs_text_of(writer->c, KRS_FIXED, s_fixed_names[argument->which]));
        const char *const declared = forms_type_of(&writer->declared, variable->text);
        FORMS_SET(type, (declared != NULL) ? declared : "");
    }
    return type;
}

// The tool submits a question that contains a number again, with another number. The second time, a form that the
// compiler wrote around the number reads apart. Examples are a power of two as a shift, or a count folded into a
// constant.
//
// The other number leaves a multiple of four a multiple of four, as an offset does. It stays below the first where it
// can, as a count must stay below the width. Neither is 0 or 1, which the compiler meets in constants of its own
static unsigned int number_alternate(unsigned int number)
{
    if ((number % 4u) == 0u)
    {
        return (number >= 8u) ? (number - 4u) : (number + 4u);
    }
    return (number >= 3u) ? (number - 1u) : (number + 1u);
}

// 1 where a question puts a number to any of its `count` forms at `question`
static int question_numbered(const LaneForm *question, unsigned int count)
{
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        for (unsigned int at = 0u; at < question[one].count; at += 1u)
        {
            const Argument *const argument = &question[one].arguments[at];
            if ((argument->type == OPERAND_NUMBER) || (argument->type == OPERAND_SIGNED) ||
                ((argument->type == OPERAND_REGISTER) && (argument->which == REGCLASS_IMMEDIATE)))
            {
                return 1;
            }
        }
    }
    return 0;
}

// a tag line of the body: `head`, the tag, then `tail`
static void writer_tag(Writer *writer, const char *head, unsigned int tag, const char *tail)
{
    mmgr_line_put(&writer->body.line, head);
    mmgr_line_decimal(&writer->body.line, tag, 1u);
    mmgr_line_put(&writer->body.line, tail);
}

// the row of a question put after the rows, and the count of the rows past it
static void writer_row(Writer *writer, const FormsLine *row)
{
    mmgr_line_put(&writer->rows.line, row->text);
    mmgr_line_ch(&writer->rows.line, '\n');
    writer->questions += 1u;
}

// a role of a row: `prefix`, the role's name, `=`, the place and the word, `:` and the type
static void writer_role(FormsLine *row, const char *prefix, const char *name, const char *place, unsigned int word,
                        const char *type)
{
    FORMS_ADD(*row, prefix);
    FORMS_ADD(*row, name);
    FORMS_ADD(*row, "=");
    FORMS_ADD(*row, place);
    FORMS_ADD(*row, ":");
    FORMS_ADD_DECIMAL(*row, word);
    FORMS_ADD(*row, ":");
    FORMS_ADD(*row, type);
}

// a number written as a question puts it, signed where the argument is
static FormsWord writer_number(unsigned int value, int is_signed)
{
    FormsWord number;
    number.text[0] = '\0';
    if (is_signed)
    {
        // the bits of the word read as the signed word a signed argument holds
        FORMS_ADD_SIGNED(number, (long long)(int)value);
    }
    else
    {
        FORMS_ADD_DECIMAL(number, value);
    }
    return number;
}

// One question written: its run of the `count` forms at `question`, each in three regions between tags, its loads,
// the form and its stores. A volatile access does not cross a tag, and the region of the form contains none. The
// compiler cannot fold a load or a store into the form
static void writer_question(Writer *writer, const LaneForm *question, unsigned int count, int alternate)
{
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const LaneForm *const lane_form = &question[one];
        const char *const name = s_form_names[lane_form->form];
        const KrsEntry *const form = krs_find(writer->c, KRS_FORM, name);
        if (form == NULL)
        {
            forms_refused("c.krs gives no form of a question: ", name, forms_length(name));
        }
        writer->tag += 1u;
        writer_tag(writer, "\n    MONOLITH_TAG(", MONOLITH_FORMS_LOADS + writer->tag, ");\n    {\n");
        FormsLine row;
        row.text[0] = '\0';
        FORMS_ADD_DECIMAL(row, writer->tag);
        FORMS_ADD(row, "\t");
        FORMS_ADD(row, name);
        FORMS_ADD(row, "\t");
        FORMS_ADD(row, lane_form_key(lane_form).text);
        PlatformText text;
        forms_text_begun(&text);
        mmgr_line_put(&text.line, krs_text(writer->c, form));
        PlatformText stores;
        forms_text_begun(&stores);
        size_t size = 0u;
        const int conditional = forms_has(forms_text_bytes(&text, &size), "if (");
        for (unsigned int at = 0u; at < form->parameter_count; at += 1u)
        {
            const char *const parameter = krs_parameter(writer->c, form, at);
            const Argument *const argument = &lane_form->arguments[at];
            FormsWord variable;
            const FormsWord type = writer_type(writer, argument, &variable);
            if (type.text[0] == '\0')
            {
                const int immediate = (argument->type == OPERAND_REGISTER);
                const unsigned int value = alternate ? number_alternate(argument->number) : argument->number;
                FormsWord number = writer_number(value, argument->type == OPERAND_SIGNED);
                FormsWord put = number;
                FORMS_ADD(put, immediate ? "u" : "");
                forms_text_fill(&text, parameter, put.text);
                FORMS_ADD(row, "\t");
                FORMS_ADD(row, parameter);
                FORMS_ADD(row, "=number:");
                FORMS_ADD(row, number.text);
                continue;
            }
            FormsWord local;
            FORMS_SET(local, "a_");
            FORMS_ADD(local, parameter);
            FormsWord braced;
            FORMS_SET(braced, "{");
            FORMS_ADD(braced, parameter);
            FORMS_ADD(braced, "}");
            FormsWord assigned_text = braced;
            FORMS_ADD(assigned_text, " =");
            FormsWord compared_text = braced;
            FORMS_ADD(compared_text, " ==");
            const char *const bytes = forms_text_bytes(&text, &size);
            const size_t named = forms_count(bytes, braced.text);
            const size_t assigned = forms_count(bytes, assigned_text.text) - forms_count(bytes, compared_text.text);
            const int output = (assigned != 0u);
            const int input = !output || conditional || (named > assigned);
            if (input)
            {
                const unsigned int word = writer_take(&writer->in_words, type.text);
                mmgr_line_put(&writer->body.line, "        ");
                mmgr_line_put(&writer->body.line, type.text);
                mmgr_line_put(&writer->body.line, " ");
                mmgr_line_put(&writer->body.line, local.text);
                mmgr_line_put(&writer->body.line, " = ");
                mmgr_line_put(&writer->body.line, writer_load(type.text, word).text);
                mmgr_line_put(&writer->body.line, ";\n");
                writer_role(&row, "\t", parameter, "in", word, type.text);
            }
            else
            {
                mmgr_line_put(&writer->body.line, "        ");
                mmgr_line_put(&writer->body.line, type.text);
                mmgr_line_put(&writer->body.line, " ");
                mmgr_line_put(&writer->body.line, local.text);
                mmgr_line_put(&writer->body.line, ";\n");
            }
            if (output)
            {
                const unsigned int word = writer_take(&writer->out_words, type.text);
                mmgr_line_put(&stores.line, writer_store(type.text, word, local.text).text);
                writer_role(&row, "\t", parameter, "out", word, type.text);
            }
            forms_text_fill(&text, parameter, local.text);
        }
        // The fixed registers the text names, each loaded as an argument is. The carry is the chain's own, and the
        // question loads and stores it below as a form that carries reads and writes it
        int launch_loaded = 0;
        for (unsigned int fixed = 0u; fixed < writer->c->counts[KRS_FIXED]; fixed += 1u)
        {
            const KrsEntry *const entry = &writer->c->entries[KRS_FIXED][fixed];
            const char *const variable = krs_text(writer->c, entry);
            const char *const type = forms_type_of(&writer->declared, variable);
            if (forms_same(variable, "carry") || (type == NULL) ||
                !forms_names(forms_text_bytes(&text, &size), variable))
            {
                continue;
            }
            const unsigned int word = writer_take(&writer->in_words, type);
            mmgr_line_put(&writer->body.line, "        const ");
            mmgr_line_put(&writer->body.line, type);
            mmgr_line_put(&writer->body.line, " ");
            mmgr_line_put(&writer->body.line, variable);
            mmgr_line_put(&writer->body.line, " = ");
            mmgr_line_put(&writer->body.line, writer_load(type, word).text);
            mmgr_line_put(&writer->body.line, ";\n");
            writer_role(&row, "\t!", entry->name, "in", word, type);
            launch_loaded = launch_loaded || forms_same(variable, "launch");
        }
        // the launch, where c.krs names it as no fixed register and the loop above has not loaded it
        const char *const launch_type = forms_type_of(&writer->declared, "launch");
        if ((launch_type != NULL) && !launch_loaded && forms_names(forms_text_bytes(&text, &size), "launch"))
        {
            const unsigned int word = writer_take(&writer->in_words, launch_type);
            mmgr_line_put(&writer->body.line, "        const ");
            mmgr_line_put(&writer->body.line, launch_type);
            mmgr_line_put(&writer->body.line, " launch = ");
            mmgr_line_put(&writer->body.line, writer_load(launch_type, word).text);
            mmgr_line_put(&writer->body.line, ";\n");
            writer_role(&row, "\t!", "launch", "in", word, launch_type);
        }
        // the question loads the carry that a form reads, and stores the carry that it writes, as every other word that
        // it reads and writes
        int reads = 0;
        const int writes = form_carries(writer->c, name, &reads);
        if (reads)
        {
            const unsigned int word = writer_take(&writer->in_words, "u32");
            mmgr_line_put(&writer->body.line, "        carry = ");
            mmgr_line_put(&writer->body.line, writer_load("u32", word).text);
            mmgr_line_put(&writer->body.line, ";\n");
            writer_role(&row, "\t!", "carry", "in", word, "u32");
        }
        if (writes)
        {
            const unsigned int word = writer_take(&writer->out_words, "u32");
            mmgr_line_put(&stores.line, writer_store("u32", word, "carry").text);
            writer_role(&row, "\t!", "carry", "out", word, "u32");
        }
        writer_tag(writer, "        MONOLITH_TAG(", writer->tag, ");\n");
        forms_text_put(&writer->body, &text);
        writer_tag(writer, "        MONOLITH_TAG(", MONOLITH_FORMS_STORES + writer->tag, ");\n");
        forms_text_put(&writer->body, &stores);
        mmgr_line_put(&writer->body.line, "    }\n");
        writer_row(writer, &row);
        platform_text_ended(&stores);
        platform_text_ended(&text);
    }
}

// The PTX types the forms of ptx.krs declare, by the register's name, counted where `declared` is NULL and put at
// `declared` where it is not. Each comes off a `.reg .<type> <names>;`, each name up to its `<`
static unsigned int ptx_declared_made(const Krs *ptx, FormsDeclared *declared)
{
    unsigned int count = 0u;
    for (unsigned int form = 0u; form < ptx->counts[KRS_FORM]; form += 1u)
    {
        const KrsEntry *const entry = &ptx->entries[KRS_FORM][form];
        const char *const text = krs_text(ptx, entry);
        const size_t length = entry->length;
        for (size_t at = forms_find_n(text, length, ".reg .", 0u); at < length;)
        {
            const size_t type_from = at + 6u;
            const size_t type_end =
                type_from + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[type_from],
                                       .cap = (type_from < length) ? (length - type_from) : 0u, .other = " \t");
            const size_t end = forms_chr_n(text, length, ';', at);
            if ((type_end >= length) || (end == length))
            {
                break;
            }
            // the names between the type and the `;`, apart by commas, an empty one among them
            const char *const names = &text[type_end];
            const size_t names_length = (end > type_end) ? (end - type_end) : 0u;
            for (size_t from = 0u; from <= names_length;)
            {
                const size_t until = forms_chr_n(names, names_length, ',', from);
                size_t first = 0u;
                const size_t size = forms_trimmed(&names[from], until - from, " \t", &first);
                const size_t angle = forms_chr_n(&names[from + first], size, '<', 0u);
                if (declared != NULL)
                {
                    forms_set_n(declared[count].name, sizeof(declared[count].name), &names[from + first], angle);
                    forms_set_n(declared[count].type, sizeof(declared[count].type), &text[type_from],
                                type_end - type_from);
                }
                count += 1u;
                from = until + 1u;
            }
            at = forms_find_n(text, length, ".reg .", end);
        }
    }
    return count;
}

// The PTX type that each register declared in ptx.krs stores, b32, b64 or pred, by its name. Also the type of each
// bank, by the prefix of the names of its registers. Both come off every `.reg .<type> <names>;` that the forms of the
// ruleset write. The last declaration of a name, in the order of the forms' names, holds
static FormsTypes ptx_declared(const Krs *ptx)
{
    FormsTypes types;
    const unsigned int count = ptx_declared_made(ptx, NULL);
    types.declared = (FormsDeclared *)forms_allocated(platform_cell_cleared(
        (count != 0u) ? count : 1u, sizeof(FormsDeclared), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    types.count = forms_declared_settled(types.declared, ptx_declared_made(ptx, types.declared), 0);
    return types;
}

// the PTX type a register of `bank` stores, through the prefix ptx.krs names the bank's registers with
static FormsWord ptx_bank_type(const Writer *writer, const char *bank)
{
    FormsWord type;
    type.text[0] = '\0';
    const KrsEntry *const entry = krs_find(writer->ptx, KRS_BANK, bank);
    if (entry == NULL)
    {
        return type;
    }
    const char *const text = krs_text(writer->ptx, entry);
    FormsWord prefix;
    FORMS_SET_N(prefix, text, forms_chr_n(text, entry->length, '{', 0u));
    const char *const found = forms_type_of(&writer->ptx_types, prefix.text);
    FORMS_SET(type, (found != NULL) ? found : "");
    return type;
}

// the `size` bytes at `text` written as a C string's contents onto `line`. The walk jumps from one byte to escape to
// the next
static void c_string(VerbaLinea *line, const char *text, size_t size)
{
    size_t at = 0u;
    while (at < size)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = size - at, .other = "\n\t\"\\");
        (void)mmgr_line_put_n(line, &text[at], run);
        at += run;
        if (at >= size)
        {
            break;
        }
        const char character = text[at];
        mmgr_line_put(line, (character == '\n')   ? "\\n"
                            : (character == '\t') ? "\\t"
                            : (character == '"')  ? "\\\""
                                                  : "\\\\");
        at += 1u;
    }
}

// The place of each write of `name` in the text of a PTX form. 1 where it is the first operand of a line that writes
// one. 2 where that line is also under a guard, and the line then reads the register as well. 0 where the text only
// reads it
static int ptx_written(const char *text, size_t size, const char *name)
{
    int written = 0;
    for (size_t at = 0u; at < size;)
    {
        const size_t stop = forms_chr_n(text, size, '\n', at);
        const Instruction instruction = instruction_read_n(&text[at], stop - at);
        at = stop + 1u;
        const int stores =
            forms_begins(instruction.operation.text, "st.") || forms_begins(instruction.operation.text, "red.");
        if (!stores && (instruction.operand_count != 0u) && forms_has(instruction.operands[0].text, name))
        {
            written = (instruction.guard.text[0] == '\0') ? ((written == 2) ? 2 : 1) : 2;
        }
    }
    return written;
}

// the registers of a PTX question at max: its outputs and its inputs
#define FORMS_PTX_OPERANDS_MAX 64u

// The PTX question that the writer builds: its loads, its stores, and the lines before and after its form, each a text
// of the call's. Then its predicates, its outputs and its inputs, the count of its predicates, and its row
struct PtxQuestion
{
    Writer *writer;
    PlatformText loads;
    PlatformText stores;
    PlatformText before;
    PlatformText after;
    FormsLine predicates;
    FormsLine outputs[FORMS_PTX_OPERANDS_MAX];
    unsigned int output_count;
    FormsLine inputs[FORMS_PTX_OPERANDS_MAX];
    unsigned int input_count;
    unsigned int predicate_count;
    FormsLine row;
};

// One register of the form: its operand, loaded, stored or both, `role` its name in the questions' row. The answer is
// the text the form names it by: its operand, or the predicate that a predicate crosses through
static FormsWord ptx_operand(PtxQuestion *question, const char *role, const char *type, int fixed, int read, int write)
{
    Writer *const writer = question->writer;
    const char *const prefix = fixed ? "\t!" : "\t";
    FormsWord local;
    FORMS_SET(local, "p_");
    FORMS_ADD_DECIMAL(local, question->input_count + question->output_count + question->predicate_count);
    const int predicate = forms_same(type, "pred");
    const char *const c_type = forms_same(type, "b64") ? "u64" : "u32";
    const char *const row_type = predicate ? "int" : c_type;
    mmgr_line_put(&question->loads.line, "        ");
    mmgr_line_put(&question->loads.line, c_type);
    mmgr_line_put(&question->loads.line, " ");
    mmgr_line_put(&question->loads.line, local.text);
    if (read)
    {
        const unsigned int word = writer_take(&writer->in_words, c_type);
        mmgr_line_put(&question->loads.line, " = ");
        mmgr_line_put(&question->loads.line, writer_load(c_type, word).text);
        writer_role(&question->row, prefix, role, "in", word, row_type);
    }
    mmgr_line_put(&question->loads.line, ";\n");
    if (write)
    {
        const unsigned int word = writer_take(&writer->out_words, c_type);
        mmgr_line_put(&question->stores.line, writer_store(c_type, word, local.text).text);
        writer_role(&question->row, prefix, role, "out", word, row_type);
    }
    const char *const constraint = forms_same(c_type, "u64") ? "l" : "r";
    FormsWord placed;
    if ((write ? question->output_count : question->input_count) == FORMS_PTX_OPERANDS_MAX)
    {
        forms_refused("a PTX question names more registers than it holds: ", role, forms_length(role));
    }
    if (write)
    {
        FormsLine *const output = &question->outputs[question->output_count];
        FORMS_SET(*output, "\"");
        FORMS_ADD(*output, read ? "+" : "=");
        FORMS_ADD(*output, constraint);
        FORMS_ADD(*output, "\"(");
        FORMS_ADD(*output, local.text);
        FORMS_ADD(*output, ")");
        FORMS_SET(placed, "%");
        FORMS_ADD_DECIMAL(placed, question->output_count);
        question->output_count += 1u;
    }
    else
    {
        FormsLine *const input = &question->inputs[question->input_count];
        FORMS_SET(*input, "\"");
        FORMS_ADD(*input, constraint);
        FORMS_ADD(*input, "\"(");
        FORMS_ADD(*input, local.text);
        FORMS_ADD(*input, ")");
        FORMS_SET(placed, "%I");
        FORMS_ADD_DECIMAL(placed, question->input_count);
        question->input_count += 1u;
    }
    if (!predicate)
    {
        return placed;
    }
    FormsWord predicate_name;
    FORMS_SET(predicate_name, "mq");
    FORMS_ADD_DECIMAL(predicate_name, question->predicate_count);
    question->predicate_count += 1u;
    FORMS_ADD(question->predicates, (question->predicates.text[0] == '\0') ? "" : ", ");
    FORMS_ADD(question->predicates, predicate_name.text);
    if (read)
    {
        mmgr_line_put(&question->before.line, "\tsetp.ne.u32 ");
        mmgr_line_put(&question->before.line, predicate_name.text);
        mmgr_line_put(&question->before.line, ", ");
        mmgr_line_put(&question->before.line, placed.text);
        mmgr_line_put(&question->before.line, ", 0;\n");
    }
    if (write)
    {
        mmgr_line_put(&question->after.line, "\tselp.u32 ");
        mmgr_line_put(&question->after.line, placed.text);
        mmgr_line_put(&question->after.line, ", 1, 0, ");
        mmgr_line_put(&question->after.line, predicate_name.text);
        mmgr_line_put(&question->after.line, ";\n");
    }
    return predicate_name;
}

// the line of the `size` bytes at `text` that holds the place `at`, as a piece: its first byte into `first`, and its
// bytes the answer
static size_t forms_line_of(const char *text, size_t size, size_t at, size_t *first)
{
    const size_t newline = forms_chr_last(text, at, '\n');
    *first = (newline == at) ? 0u : (newline + 1u);
    return forms_chr_n(text, size, '\n', at) - *first;
}

// `text` with each place of `mark` written `put`, in place. The search for each goes on past the one put before it
static void forms_text_replaced(PlatformText *text, const char *mark, const char *put)
{
    PlatformText replaced;
    forms_text_begun(&replaced);
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(text, &size);
    const size_t mark_size = forms_length(mark);
    size_t at = 0u;
    for (size_t found = forms_find_n(bytes, size, mark, 0u); found < size; found = forms_find_n(bytes, size, mark, at))
    {
        (void)mmgr_line_put_n(&replaced.line, &bytes[at], found - at);
        mmgr_line_put(&replaced.line, put);
        at = found + mark_size;
    }
    (void)mmgr_line_put_n(&replaced.line, &bytes[at], size - at);
    platform_text_reset(text);
    forms_text_put(text, &replaced);
    platform_text_ended(&replaced);
}

// `text` with each place of `%<register>` that no byte of an identifier follows written `put`, in place
static void ptx_register_replaced(PlatformText *text, const char *register_name, const char *put)
{
    FormsWord mark;
    FORMS_SET(mark, "%");
    FORMS_ADD(mark, register_name);
    PlatformText replaced;
    forms_text_begun(&replaced);
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(text, &size);
    const size_t mark_size = forms_length(mark.text);
    size_t last = 0u;
    for (size_t at = forms_find_n(bytes, size, mark.text, 0u); at < size;)
    {
        const size_t end = at + mark_size;
        if ((end >= size) || !forms_identifier(bytes[end]))
        {
            (void)mmgr_line_put_n(&replaced.line, &bytes[last], at - last);
            mmgr_line_put(&replaced.line, put);
            last = end;
        }
        at = forms_find_n(bytes, size, mark.text, end);
    }
    (void)mmgr_line_put_n(&replaced.line, &bytes[last], size - last);
    platform_text_reset(text);
    forms_text_put(text, &replaced);
    platform_text_ended(&replaced);
}

// The registers of a PTX form, `first` to `count`, apart by ", " onto `line`
static void ptx_joined(VerbaLinea *line, const FormsLine *operands, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(line, (at == 0u) ? "" : ", ");
        mmgr_line_put(line, operands[at].text);
    }
}

// One form submitted in the own text of ptx.krs, for the SASS that NVIDIA's assembler writes from it. The text is an
// inline assembly statement between the tags of the form. Every register that it names is an operand of the statement,
// loaded from `in` or stored to `out`.
//
// A predicate crosses as a word. Where the form reads it, the question sets it from the word before the form. Where the
// form writes it, the question selects the word from it after the form. The carry that a form reads is the carry flag
// set from a word before it. The carry that it writes goes into a word after it.
//
// The text holds a number as written, or its alternate where `alternate` is 1. Where `alternate` is 2, each number
// outside an address loads from `in` as an argument does, 64 bits wide where the type of its line is. The count of a
// shift is the exception. The compiler cannot fold a value that it cannot see, and writes the instruction that takes
// any word, which the number then fills.
//
// No question reads a copy. In a straight run, the allocator gives the two words of the copy one register. Only a
// register fixed from outside, such as that of a call, makes it write one
static void writer_ptx_question(Writer *writer, const LaneForm *lane_form, int alternate)
{
    const char *const name = s_form_names[lane_form->form];
    const KrsEntry *const form = krs_find(writer->ptx, KRS_FORM, name);
    if (form == NULL)
    {
        return;
    }
    const char *const form_text = krs_text(writer->ptx, form);
    const size_t form_size = form->length;
    writer->ptx_tag += 1u;
    const unsigned int tag = MONOLITH_FORMS_PTX + writer->ptx_tag;
    static PtxQuestion s_question;
    PtxQuestion *const question = &s_question;
    question->writer = writer;
    question->predicates.text[0] = '\0';
    question->output_count = 0u;
    question->input_count = 0u;
    question->predicate_count = 0u;
    question->row.text[0] = '\0';
    FORMS_ADD_DECIMAL(question->row, tag);
    FORMS_ADD(question->row, "\t");
    FORMS_ADD(question->row, name);
    FORMS_ADD(question->row, "\t");
    FORMS_ADD(question->row, lane_form_key(lane_form).text);
    forms_text_begun(&question->loads);
    forms_text_begun(&question->stores);
    forms_text_begun(&question->before);
    forms_text_begun(&question->after);
    // every % of the text retained as one, and each register named in it then given its operand
    PlatformText text;
    forms_text_begun(&text);
    for (size_t at = 0u; at < form_size;)
    {
        const size_t percent = forms_chr_n(form_text, form_size, '%', at);
        (void)mmgr_line_put_n(&text.line, &form_text[at], percent - at);
        if (percent < form_size)
        {
            mmgr_line_put(&text.line, "%%");
        }
        at = percent + 1u;
    }
    unsigned int opaque = 0u;
    for (unsigned int at = 0u; at < form->parameter_count; at += 1u)
    {
        const char *const parameter = krs_parameter(writer->ptx, form, at);
        const Argument *const argument = &lane_form->arguments[at];
        FormsWord type;
        type.text[0] = '\0';
        if ((argument->type == OPERAND_REGISTER) && (argument->which != REGCLASS_IMMEDIATE))
        {
            type = ptx_bank_type(writer, s_bank_names[argument->which]);
        }
        else if (argument->type == OPERAND_PHYSREG)
        {
            const KrsEntry *const fixed = krs_find(writer->ptx, KRS_FIXED, s_fixed_names[argument->which]);
            const char *const declared =
                (fixed == NULL) ? NULL : forms_type_of(&writer->ptx_types, krs_text(writer->ptx, fixed));
            FORMS_SET(type, (declared != NULL) ? declared : "");
        }
        FormsWord braced;
        FORMS_SET(braced, "{");
        FORMS_ADD(braced, parameter);
        FORMS_ADD(braced, "}");
        if (type.text[0] == '\0')
        {
            const unsigned int value = (alternate == 1) ? number_alternate(argument->number) : argument->number;
            // the line of the form's text that names the parameter first
            FormsLine line_of;
            line_of.text[0] = '\0';
            const size_t named = forms_find_n(form_text, form_size, braced.text, 0u);
            if (named < form_size)
            {
                size_t first = 0u;
                const size_t length = forms_line_of(form_text, form_size, named, &first);
                FORMS_SET_N(line_of, &form_text[first], length);
            }
            const size_t line_length = forms_length(line_of.text);
            const size_t bracket = forms_chr_n(line_of.text, line_length, '[', 0u);
            const int addressed =
                (bracket < line_length) && (forms_find_n(line_of.text, line_length, braced.text, 0u) > bracket);
            // a shift's count is a 32-bit word whatever the width it shifts
            FormsWord operation;
            operation.text[0] = '\0';
            if (line_length != 0u)
            {
                operation = instruction_read_n(line_of.text, line_length).operation;
            }
            const int wide = !addressed && forms_has(operation.text, "64") && !forms_begins(operation.text, "sh");
            // the question submits a 64-bit number whole. Its high word is the alternate of its low, which is never 0
            // and never the low word itself. The compiler folds a high word of 0 to RZ, and the tool does not read the
            // form that it writes for any other
            const unsigned long long whole =
                wide ? (((unsigned long long)number_alternate(value) << 32u) | value) : value;
            FormsWord number;
            number.text[0] = '\0';
            if ((argument->type == OPERAND_SIGNED) && !wide)
            {
                // the bits of the word read as the signed word a signed argument holds
                FORMS_ADD_SIGNED(number, (long long)(int)value);
            }
            else
            {
                FORMS_ADD_DECIMAL(number, whole);
            }
            if ((alternate == 2) && (line_length != 0u) && !addressed)
            {
                const FormsWord placed = ptx_operand(question, parameter, wide ? "b64" : "b32", 0, 1, 0);
                forms_text_fill(&text, parameter, placed.text);
                FORMS_ADD(question->row, ":number:");
                FORMS_ADD(question->row, number.text);
                opaque += 1u;
                continue;
            }
            forms_text_fill(&text, parameter, number.text);
            FORMS_ADD(question->row, "\t");
            FORMS_ADD(question->row, parameter);
            FORMS_ADD(question->row, "=number:");
            FORMS_ADD(question->row, number.text);
            continue;
        }
        const int written = ptx_written(form_text, form_size, braced.text);
        const int read = (written != 1);
        const FormsWord placed = ptx_operand(question, parameter, type.text, 0, read, written != 0);
        forms_text_fill(&text, parameter, placed.text);
    }
    // a form whose every number sits in an address tests nothing the literal questions did not
    if ((alternate == 2) && (opaque == 0u))
    {
        writer->ptx_tag -= 1u;
        platform_text_ended(&text);
        platform_text_ended(&question->after);
        platform_text_ended(&question->before);
        platform_text_ended(&question->stores);
        platform_text_ended(&question->loads);
        return;
    }
    // the fixed registers the text names, each loaded as an argument is: the ruleset's name for it where it has one
    for (unsigned int declared = 0u; declared < writer->ptx_types.count; declared += 1u)
    {
        const char *const register_name = writer->ptx_types.declared[declared].name;
        FormsWord mark;
        FORMS_SET(mark, "%");
        FORMS_ADD(mark, register_name);
        size_t size = 0u;
        if ((register_name[0] != '%') || !forms_names(forms_text_bytes(&text, &size), mark.text))
        {
            continue;
        }
        FormsWord role;
        FORMS_SET(role, &register_name[1]);
        for (unsigned int fixed = 0u; fixed < writer->ptx->counts[KRS_FIXED]; fixed += 1u)
        {
            const KrsEntry *const entry = &writer->ptx->entries[KRS_FIXED][fixed];
            if (forms_same(krs_text(writer->ptx, entry), register_name))
            {
                FORMS_SET(role, entry->name);
            }
        }
        const int written = ptx_written(form_text, form_size, register_name);
        const FormsWord placed =
            ptx_operand(question, role.text, writer->ptx_types.declared[declared].type, 1, written != 1, written != 0);
        ptx_register_replaced(&text, register_name, placed.text);
    }
    // the carry flag: set from a word before a form that reads it, read into a word after a form that writes it
    int carry_in = 0;
    int carry_out = 0;
    for (size_t at = 0u; at < form_size;)
    {
        const size_t stop = forms_chr_n(form_text, form_size, '\n', at);
        const FormsWord operation = instruction_read_n(&form_text[at], stop - at).operation;
        at = stop + 1u;
        carry_in = carry_in || forms_begins(operation.text, "addc") || forms_begins(operation.text, "subc") ||
                   forms_begins(operation.text, "madc");
        carry_out = carry_out || forms_has(operation.text, ".cc");
    }
    if (carry_in)
    {
        const unsigned int word = writer_take(&writer->in_words, "u32");
        mmgr_line_put(&question->loads.line, "        u32 p_carry_in = ");
        mmgr_line_put(&question->loads.line, writer_load("u32", word).text);
        mmgr_line_put(&question->loads.line, ";\n");
        writer_role(&question->row, "\t!", "carry", "in", word, "int");
        if (question->input_count == FORMS_PTX_OPERANDS_MAX)
        {
            forms_refused("a PTX question names more registers than it holds: ", name, forms_length(name));
        }
        FORMS_SET(question->inputs[question->input_count], "\"r\"(p_carry_in)");
        question->input_count += 1u;
        PlatformText before;
        forms_text_begun(&before);
        mmgr_line_put(&before.line, "\tadd.cc.u32 mw, %I");
        mmgr_line_decimal(&before.line, question->input_count - 1u, 1u);
        mmgr_line_put(&before.line, ", 0xffffffff;\n");
        forms_text_put(&before, &question->before);
        platform_text_reset(&question->before);
        forms_text_put(&question->before, &before);
        platform_text_ended(&before);
    }
    if (carry_out)
    {
        const unsigned int word = writer_take(&writer->out_words, "u32");
        mmgr_line_put(&question->loads.line, "        u32 p_carry_out;\n");
        mmgr_line_put(&question->stores.line, writer_store("u32", word, "p_carry_out").text);
        writer_role(&question->row, "\t!", "carry", "out", word, "int");
        if (question->output_count == FORMS_PTX_OPERANDS_MAX)
        {
            forms_refused("a PTX question names more registers than it holds: ", name, forms_length(name));
        }
        FORMS_SET(question->outputs[question->output_count], "\"=r\"(p_carry_out)");
        question->output_count += 1u;
        mmgr_line_put(&question->after.line, "\taddc.u32 %");
        mmgr_line_decimal(&question->after.line, question->output_count - 1u, 1u);
        mmgr_line_put(&question->after.line, ", 0, 0;\n");
    }
    PlatformText statement;
    forms_text_begun(&statement);
    mmgr_line_put(&statement.line, "{\n\t.reg .b32 mw;\n");
    if (question->predicates.text[0] != '\0')
    {
        mmgr_line_put(&statement.line, "\t.reg .pred ");
        mmgr_line_put(&statement.line, question->predicates.text);
        mmgr_line_put(&statement.line, ";\n");
    }
    forms_text_put(&statement, &question->before);
    forms_text_put(&statement, &text);
    forms_text_put(&statement, &question->after);
    mmgr_line_put(&statement.line, "}\n");
    // the inputs numbered past the outputs, as an inline assembly statement numbers them
    for (unsigned int one = question->input_count; one > 0u; one -= 1u)
    {
        FormsWord mark;
        FORMS_SET(mark, "%I");
        FORMS_ADD_DECIMAL(mark, one - 1u);
        FormsWord put;
        FORMS_SET(put, "%");
        FORMS_ADD_DECIMAL(put, question->output_count + one - 1u);
        forms_text_replaced(&statement, mark.text, put.text);
    }
    writer_tag(writer, "\n    MONOLITH_TAG(", MONOLITH_FORMS_LOADS + tag, ");\n    {\n");
    forms_text_put(&writer->body, &question->loads);
    writer_tag(writer, "        MONOLITH_TAG(", tag, ");\n");
    mmgr_line_put(&writer->body.line, "        asm volatile(\"");
    size_t statement_size = 0u;
    const char *const statement_bytes = forms_text_bytes(&statement, &statement_size);
    c_string(&writer->body.line, statement_bytes, statement_size);
    mmgr_line_put(&writer->body.line, "\" : ");
    ptx_joined(&writer->body.line, question->outputs, question->output_count);
    mmgr_line_put(&writer->body.line, " : ");
    ptx_joined(&writer->body.line, question->inputs, question->input_count);
    mmgr_line_put(&writer->body.line, ");\n");
    writer_tag(writer, "        MONOLITH_TAG(", MONOLITH_FORMS_STORES + tag, ");\n");
    forms_text_put(&writer->body, &question->stores);
    mmgr_line_put(&writer->body.line, "    }\n");
    writer_row(writer, &question->row);
    platform_text_ended(&statement);
    platform_text_ended(&text);
    platform_text_ended(&question->after);
    platform_text_ended(&question->before);
    platform_text_ended(&question->stores);
    platform_text_ended(&question->loads);
}

// a note of the engine's that a file did not read or the tool did not write it: `head`, `path`, `middle`, `other` and
// `tail`
static void forms_noted(const char *head, const char *path, const char *middle, const char *other, const char *tail)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, head);
    mmgr_line_put(&engine_note.line, path);
    mmgr_line_put(&engine_note.line, middle);
    mmgr_line_put(&engine_note.line, other);
    mmgr_line_put(&engine_note.line, tail);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

static int forms_write(const char *c_path, const char *ptx_path, const char *monolith_path, const char *questions_path)
{
    static Krs s_c;
    static Krs s_ptx;
    if ((c_target().ruleset(1) == NULL) || !krs_read(c_path, &s_c) || !krs_read(ptx_path, &s_ptx))
    {
        forms_noted("the ruleset ", c_path, " or ", ptx_path, " did not read");
        return 2;
    }
    HostProgram program;
    HostProgram bare;
    for (int reuse = 0; reuse <= 1; reuse += 1)
    {
        host_arithmetic(&program);
        gather_lane(&s_c, &program, reuse);
        host_division(&program);
        gather_lane(&s_c, &program, reuse);
        host_bitwise(&program);
        gather_lane(&s_c, &program, reuse);
        host_members(&program);
        gather_lane(&s_c, &program, reuse);
        host_affine_limit(&program);
        gather_lane(&s_c, &program, reuse);
        host_bare_divisor(&bare);
        gather_lane(&s_c, &bare, reuse);
        host_inexact(&program, &bare);
        gather_lane(&s_c, &program, reuse);
        // the two tables the program reads through, sized as the host oracle sizes them
        unsigned int *const wide = (unsigned int *)platform_cell_taken(
            (unsigned long long)(512u * sizeof(unsigned int)), (unsigned int)platform_place_g_program_owner, 0);
        unsigned int *const narrow = (unsigned int *)platform_cell_taken(
            (unsigned long long)(4096u * sizeof(unsigned int)), (unsigned int)platform_place_g_program_owner, 0);
        host_tables(&program, wide, narrow);
        gather_lane(&s_c, &program, reuse);
        platform_cell_released(wide, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(narrow, (unsigned int)platform_place_g_program_owner);
    }
    static Writer s_writer;
    Writer *const writer = &s_writer;
    writer->c = &s_c;
    writer->ptx = &s_ptx;
    writer->declared = c_declared(&s_c);
    writer->ptx_types = ptx_declared(&s_ptx);
    writer->questions = 0u;
    writer->tag = 0u;
    writer->ptx_tag = 0u;
    writer->in_words = 0u;
    writer->out_words = 0u;
    forms_kept_begun(&writer->body);
    forms_kept_begun(&writer->rows);
    // the helpers the lane's opening defines for the carry chains, everything it contains before the lane itself
    const KrsEntry *const open = krs_find(&s_c, KRS_FORM, "lane_open");
    if (open == NULL)
    {
        forms_refused("c.krs gives no form lane_open", "", 0u);
    }
    writer->helpers = krs_text(&s_c, open);
    writer->helpers_length = forms_find_n(writer->helpers, open->length, "extern \"C\" __device__", 0u);
    for (unsigned int at = 0u; at < s_gathered_count; at += 1u)
    {
        const LaneForm *const question = &s_lane_forms[s_gathered[at].first];
        const unsigned int count = s_gathered[at].count;
        writer_question(writer, question, count, 0);
        if (question_numbered(question, count))
        {
            writer_question(writer, question, count, 1);
        }
    }
    // each form again in ptx.krs's own text, for the SASS read off it: alone, since the carry crosses as a word. A
    // form of one key goes once, the first that the questions give
    static const LaneForm *s_ptx_seen[FORMS_LANE_FORMS_MAX];
    unsigned int ptx_seen = 0u;
    for (unsigned int at = 0u; at < s_gathered_count; at += 1u)
    {
        for (unsigned int one = 0u; one < s_gathered[at].count; one += 1u)
        {
            const LaneForm *const lane_form = &s_lane_forms[s_gathered[at].first + one];
            int seen = 0;
            for (unsigned int before = 0u; !seen && (before < ptx_seen); before += 1u)
            {
                seen = lane_form_alike(s_ptx_seen[before], lane_form);
            }
            if (seen)
            {
                continue;
            }
            s_ptx_seen[ptx_seen] = lane_form;
            ptx_seen += 1u;
            writer_ptx_question(writer, lane_form, 0);
            if (question_numbered(lane_form, 1u))
            {
                writer_ptx_question(writer, lane_form, 1);
                writer_ptx_question(writer, lane_form, 2);
            }
        }
    }
    PlatformFile *const out = platform_file_open(monolith_path, "wb");
    PlatformFile *const rows = platform_file_open(questions_path, "wb");
    if ((out == NULL) || (rows == NULL))
    {
        forms_noted("the monolith ", monolith_path, " or the questions ", questions_path, " was not written");
        return 2;
    }
    VerbaLinea *const line = platform_file_line(out);
    mmgr_line_put(line, "// written by monolith_forms from c.krs and the lanes of the record programs; not edited by "
                        "hand\ntypedef unsigned int u32;\ntypedef unsigned long long u64;\ntypedef signed char "
                        "s8;\n\n#define MONOLITH_TAG(tag_) asm volatile(\"pmevent.mask ");
    mmgr_line_ch(line, '%');
    mmgr_line_put(line, "0;\" ::\"n\"(tag_) : \"memory\")\n");
    (void)mmgr_line_put_n(line, writer->helpers, writer->helpers_length);
    mmgr_line_put(line,
                  "\nextern \"C\" __global__ void monolith_forms(const u32 *in, u32 *out)\n{\n    u32 carry = 0u;\n");
    size_t body_size = 0u;
    const char *const body = forms_text_bytes(&writer->body, &body_size);
    (void)mmgr_line_put_n(line, body, body_size);
    mmgr_line_put(line, "\n    MONOLITH_TAG(");
    mmgr_line_decimal(line, writer->tag + 1u, 1u);
    mmgr_line_put(line, ");\n}\n");
    size_t rows_size = 0u;
    const char *const rows_bytes = forms_text_bytes(&writer->rows, &rows_size);
    (void)mmgr_line_put_n(platform_file_line(rows), rows_bytes, rows_size);
    platform_file_close(out);
    platform_file_close(rows);
    platform_text_ended(&writer->rows);
    platform_text_ended(&writer->body);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "monolith_forms: ");
    mmgr_line_decimal(output_line, s_gathered_count, 1u);
    mmgr_line_put(output_line, " questions, ");
    mmgr_line_decimal(output_line, writer->tag, 1u);
    mmgr_line_put(output_line, " blocks in c.krs's text and ");
    mmgr_line_decimal(output_line, writer->ptx_tag, 1u);
    mmgr_line_put(output_line, " in ptx.krs's, ");
    mmgr_line_decimal(output_line, writer->in_words, 1u);
    mmgr_line_put(output_line, " words in and ");
    mmgr_line_decimal(output_line, writer->out_words, 1u);
    mmgr_line_put(output_line, " out");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// the forms read back

// the roles of one question at max
#define FORMS_ROLES_MAX 32u

// one argument of a question as written. That is where the question loads it from or stores it to, and its type, or
// the number that the question put
struct Role
{
    FormsWord name;
    FormsWord place;
    unsigned int word;
    FormsWord type;
    FormsWord number;
    int fixed;
    // a number the question loaded where the lanes write it into the instruction
    int opaque;
};

struct Question
{
    unsigned int tag;
    FormsWord form;
    FormsLine key;
    Role roles[FORMS_ROLES_MAX];
    unsigned int role_count;
};

// The file at `path` read whole into a cell of the temporary tier, terminated, and its bytes into `size`. The caller
// gives the cell back before its work ends. NULL where the file does not read
static char *forms_file_read(const char *path, size_t *size)
{
    unsigned long long bytes = 0ULL;
    char *const text =
        (platform_file_size(path, &bytes) != 0)
            ? (char *)platform_cell_taken(bytes + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
            : NULL;
    unsigned long long read = 0ULL;
    if ((text == NULL) || !platform_file_read(path, text, bytes, &read))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        *size = 0u;
        return NULL;
    }
    text[read] = '\0';
    // a file read whole is at max what a size_t counts
    *size = (size_t)read;
    return text;
}

// the lines of the `size` bytes at `text`, the last counted where no newline ends it
static unsigned int forms_lines_counted(const char *text, size_t size)
{
    unsigned int lines = 0u;
    for (size_t at = 0u; at < size; at = forms_chr_n(text, size, '\n', at) + 1u)
    {
        lines += 1u;
    }
    return lines;
}

// The field `at` of the `length` bytes at `text` apart by `by`, as a piece. Its first byte goes into `first`, and its
// bytes are the answer, or 0 bytes where the text has fewer fields. The count of the fields goes into `count`
static size_t forms_field(const char *text, size_t length, char by, unsigned int at, size_t *first, unsigned int *count)
{
    *count = 0u;
    size_t found = 0u;
    size_t found_first = 0u;
    for (size_t from = 0u; from <= length;)
    {
        const size_t stop = forms_chr_n(text, length, by, from);
        if (*count == at)
        {
            found_first = from;
            found = stop - from;
        }
        *count += 1u;
        from = stop + 1u;
    }
    *first = found_first;
    return (at < *count) ? found : 0u;
}

// One line of the questions, the `length` bytes at `line`, read into `question`: 1, or 0 where it has fewer than three
// fields. Each field past the third is a role, `<name>=<place>:<word>:<type>`, a `!` before the name of a fixed one
static int question_read(const char *line, size_t length, Question *question)
{
    unsigned int fields = 0u;
    size_t first = 0u;
    (void)forms_field(line, length, '\t', 0u, &first, &fields);
    if (fields < 3u)
    {
        return 0;
    }
    size_t size = forms_field(line, length, '\t', 0u, &first, &fields);
    FormsWord tag;
    FORMS_SET_N(tag, &line[first], size);
    // a tag of a question is far below 2^32
    question->tag =
        (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = tag.text, .end = NULL, .base = 10);
    size = forms_field(line, length, '\t', 1u, &first, &fields);
    FORMS_SET_N(question->form, &line[first], size);
    size = forms_field(line, length, '\t', 2u, &first, &fields);
    FORMS_SET_N(question->key, &line[first], size);
    question->role_count = 0u;
    for (unsigned int at = 3u; at < fields; at += 1u)
    {
        size = forms_field(line, length, '\t', at, &first, &fields);
        const char *const part = &line[first];
        const size_t equals = forms_chr_n(part, size, '=', 0u);
        if (equals == size)
        {
            continue;
        }
        if (question->role_count == FORMS_ROLES_MAX)
        {
            forms_refused("a question names more roles than it holds: ", line, length);
        }
        Role *const role = &question->roles[question->role_count];
        question->role_count += 1u;
        role->fixed = (part[0] == '!');
        const size_t name_from = role->fixed ? 1u : 0u;
        FORMS_SET_N(role->name, &part[name_from], (equals > name_from) ? (equals - name_from) : 0u);
        const char *const rest = &part[equals + 1u];
        const size_t rest_length = size - equals - 1u;
        unsigned int parts = 0u;
        size_t piece_first = 0u;
        size_t piece = forms_field(rest, rest_length, ':', 0u, &piece_first, &parts);
        FORMS_SET_N(role->place, &rest[piece_first], piece);
        FormsWord second;
        second.text[0] = '\0';
        if (parts > 1u)
        {
            piece = forms_field(rest, rest_length, ':', 1u, &piece_first, &parts);
            FORMS_SET_N(second, &rest[piece_first], piece);
        }
        // a word of a question's in or out is far below 2^32
        role->word = (parts > 1u) ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = second.text,
                                                             .end = NULL, .base = 10)
                                  : 0u;
        role->type.text[0] = '\0';
        if (parts > 2u)
        {
            piece = forms_field(rest, rest_length, ':', 2u, &piece_first, &parts);
            FORMS_SET_N(role->type, &rest[piece_first], piece);
        }
        role->number.text[0] = '\0';
        if (forms_same(role->place.text, "number"))
        {
            role->number = second;
        }
        FormsWord fourth;
        fourth.text[0] = '\0';
        if (parts > 3u)
        {
            piece = forms_field(rest, rest_length, ':', 3u, &piece_first, &parts);
            FORMS_SET_N(fourth, &rest[piece_first], piece);
        }
        role->opaque = (parts > 4u) && forms_same(fourth.text, "number");
        if (role->opaque)
        {
            piece = forms_field(rest, rest_length, ':', 4u, &piece_first, &parts);
            FORMS_SET_N(role->number, &rest[piece_first], piece);
        }
    }
    return 1;
}

// The questions at `path`, a line each, in one cell taken at the count of the lines: their count into `count`. Each
// line ends at its newline, with the carriage returns and the newlines at its end cut
static Question *questions_read(const char *path, unsigned int *count)
{
    *count = 0u;
    size_t size = 0u;
    char *const file = forms_file_read(path, &size);
    if (file == NULL)
    {
        return NULL;
    }
    const unsigned int lines = forms_lines_counted(file, size);
    Question *const questions = (Question *)forms_allocated(platform_cell_cleared(
        (lines != 0u) ? lines : 1u, sizeof(Question), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    for (size_t at = 0u; at < size;)
    {
        const size_t end = forms_chr_n(file, size, '\n', at);
        const size_t length = forms_end_outside(&file[at], end - at, "\r\n");
        *count += question_read(&file[at], length, &questions[*count]) ? 1u : 0u;
        at = end + 1u;
    }
    platform_cell_released(file, platform_place_g_program_owner);
    return questions;
}

// one line of a block of the listing or of the PTX: the tag it is behind and its instruction
struct BlockLine
{
    unsigned int tag;
    FormsLine text;
};

// The lines of a listing or of the PTX, in one cell in the order of their tags. The lines of a tag stay in their own
// order
struct Blocks
{
    BlockLine *lines;
    unsigned int count;
};

static int block_line_order(const void *left, const void *right)
{
    const unsigned int one = ((const BlockLine *)left)->tag;
    const unsigned int other = ((const BlockLine *)right)->tag;
    return (one == other) ? 0 : ((one < other) ? -1 : 1);
}

// the lines at `blocks` put in the order of their tags, stable
static void blocks_sorted(Blocks *blocks)
{
    if (blocks->count < 2u)
    {
        return;
    }
    void *const spare = forms_allocated(platform_cell_taken((unsigned long long)blocks->count * sizeof(BlockLine),
                                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = blocks->lines, .bytes = sizeof(BlockLine), .scratch = spare,
                     .count = blocks->count, .order = block_line_order);
    platform_cell_released(spare, platform_place_g_program_owner);
}

// the first line of `blocks` behind `tag`, or the count where none is; the lines behind it into `count`
static unsigned int blocks_of(const Blocks *blocks, unsigned int tag, unsigned int *count)
{
    unsigned int low = 0u;
    unsigned int high = blocks->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (blocks->lines[middle].tag < tag)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    unsigned int end = low;
    high = blocks->count;
    while (end < high)
    {
        const unsigned int middle = end + ((high - end) / 2u);
        if (blocks->lines[middle].tag <= tag)
        {
            end = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    *count = end - low;
    return low;
}

// `text` with every `.reuse` of it left out, in place
static void sass_reuse_dropped(FormsLine *text)
{
    FormsLine kept;
    kept.text[0] = '\0';
    const size_t length = forms_length(text->text);
    size_t at = 0u;
    for (size_t found = forms_find_n(text->text, length, ".reuse", 0u); found < length;
         found = forms_find_n(text->text, length, ".reuse", at))
    {
        FORMS_ADD_N(kept, &text->text[at], found - at);
        at = found + 6u;
    }
    FORMS_ADD_N(kept, &text->text[at], length - at);
    *text = kept;
}

// The instruction of one line of the SASS listing, the `length` bytes at `line`, into `instruction`: 1, or 0 where the
// line is no instruction. An instruction's line opens with its address inside a C comment, in hex with no space. A
// line of encoding alone opens with a space and `0x` inside that comment. The instruction runs to its ` ;`. The code
// leaves out each `.reuse`, and cuts the spaces and tabs at either end
static int sass_line_read(const char *line, size_t length, FormsLine *instruction)
{
    const size_t open = forms_find_n(line, length, "/*", 0u);
    const size_t address = forms_find_n(line, length, "*/", 0u);
    if ((open == length) || (address == length) || (open > address) ||
        !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = (unsigned char)line[open + 2u]))
    {
        return 0;
    }
    const size_t end = forms_find_n(line, length, " ;", address);
    if (end == length)
    {
        return 0;
    }
    *instruction = forms_trim_n(&line[address + 2u], end - address - 2u);
    sass_reuse_dropped(instruction);
    return 1;
}

// The SASS listing's blocks, each tag's instructions, and the instructions before the first tag. Each line counts once
// to size the cell, and the lines then go in the order of their tags
static Blocks sass_blocks(const char *path)
{
    Blocks blocks = {NULL, 0u};
    size_t size = 0u;
    char *const file = forms_file_read(path, &size);
    if (file == NULL)
    {
        return blocks;
    }
    const unsigned int lines = forms_lines_counted(file, size);
    blocks.lines = (BlockLine *)forms_allocated(platform_cell_cleared(
        (lines != 0u) ? lines : 1u, sizeof(BlockLine), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    unsigned int tag = 0u;
    for (size_t at = 0u; at < size;)
    {
        const size_t end = forms_chr_n(file, size, '\n', at);
        FormsLine instruction;
        const int read = sass_line_read(&file[at], end - at, &instruction);
        at = end + 1u;
        if (!read)
        {
            continue;
        }
        if (forms_begins(instruction.text, "PMTRIG "))
        {
            // a tag is far below 2^32
            tag = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &instruction.text[7], .end = NULL,
                                           .base = 16);
            continue;
        }
        blocks.lines[blocks.count].tag = tag;
        blocks.lines[blocks.count].text = instruction;
        blocks.count += 1u;
    }
    platform_cell_released(file, platform_place_g_program_owner);
    blocks_sorted(&blocks);
    return blocks;
}

// The PTX's blocks, each tag's instructions. The lines count only past the entry of monolith_forms. A line is an
// instruction where it ends with `;` and is no directive and no comment. Each line counts once to size the cell, and
// the lines then go in the order of their tags
static Blocks ptx_blocks(const char *path)
{
    Blocks blocks = {NULL, 0u};
    size_t size = 0u;
    char *const file = forms_file_read(path, &size);
    if (file == NULL)
    {
        return blocks;
    }
    const unsigned int lines = forms_lines_counted(file, size);
    blocks.lines = (BlockLine *)forms_allocated(platform_cell_cleared(
        (lines != 0u) ? lines : 1u, sizeof(BlockLine), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    unsigned int tag = 0u;
    int inside = 0;
    for (size_t at = 0u; at < size;)
    {
        const size_t end = forms_chr_n(file, size, '\n', at);
        size_t first = 0u;
        size_t length = forms_trimmed(&file[at], end - at, " \t", &first);
        const char *line = &file[at + first];
        length = forms_end_outside(line, length, "\r\n");
        size_t again = 0u;
        length = forms_trimmed(line, length, " \t", &again);
        line = &line[again];
        at = end + 1u;
        inside = inside || (forms_find_n(line, length, ".entry monolith_forms", 0u) < length);
        if (!inside || (length == 0u) || (line[length - 1u] != ';') || (line[0] == '.') ||
            ((length >= 2u) && (line[0] == '/') && (line[1] == '/')))
        {
            continue;
        }
        length -= 1u;
        FormsLine text;
        FORMS_SET_N(text, line, length);
        if (forms_begins(text.text, "pmevent.mask "))
        {
            // a tag is far below 2^32
            tag = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &text.text[13], .end = NULL,
                                           .base = 10);
            continue;
        }
        blocks.lines[blocks.count].tag = tag;
        blocks.lines[blocks.count].text = forms_trim(text.text);
        blocks.count += 1u;
    }
    platform_cell_released(file, platform_place_g_program_owner);
    blocks_sorted(&blocks);
    return blocks;
}

// A register token of a SASS operand, the first at or past `from`: its start, and its length into `length`. The start
// is the operand's length where no token is there. A token is R or P and digits, with no byte of an identifier on
// either side. The walk jumps from one R or P to the next
static size_t sass_register(const char *operand, size_t from, size_t *length)
{
    const size_t size = forms_length(operand);
    for (size_t at = from + ((from < size) ? EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &operand[from],
                                                        .cap = size - from, .other = "RP")
                                           : 0u);
         at < size; at += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &operand[at + 1u],
                                          .cap = size - at - 1u, .other = "RP"))
    {
        const int start = (at == 0u) || !forms_identifier(operand[at - 1u]);
        if (!start)
        {
            continue;
        }
        const size_t digits = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &operand[at + 1u], .cap = size - at - 1u,
                                         .other = "0123456789");
        const int ends = ((at + 1u + digits) >= size) || !forms_identifier(operand[at + 1u + digits]);
        if ((digits != 0u) && ends)
        {
            *length = 1u + digits;
            return at;
        }
    }
    return size;
}

// the number of the register `token`, R7 or P3
static unsigned int token_number(const char *token)
{
    // a register's number is far below 2^32
    return (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &token[1], .end = NULL, .base = 10);
}

// The operands that an instruction writes: its first, and every predicate straight after it, where it writes any. None
// for a store, a reduction, a branch and the rest that write nothing of a register's
static unsigned int sass_written(const Instruction *instruction)
{
    static const char *const s_none[] = {"ST",       "RED", "BRA",  "BSSY", "BSYNC", "EXIT",
                                         "WARPSYNC", "BAR", "CALL", "RET",  "NOP",   "ATOM.E.ADD.STRONG.GPU.RZ"};
    for (const char *const none : s_none)
    {
        if (forms_begins(instruction->operation.text, none))
        {
            return 0u;
        }
    }
    unsigned int written = (instruction->operand_count == 0u) ? 0u : 1u;
    while ((written < instruction->operand_count) && (instruction->operands[written].text[0] == 'P'))
    {
        written += 1u;
    }
    return written;
}

static int sass_wide_write(const Instruction *instruction)
{
    return forms_has(instruction->operation.text, ".WIDE") || forms_has(instruction->operation.text, ".64");
}

// the bytes of a form's text that a block reads back, and of the reasons a reading gives, each at max
#define FORMS_READ_BYTES 16384u
#define FORMS_WHY_BYTES 4096u

// a block read back into a form's text: 1, or 0 with the reason through `why`
struct Read
{
    char text[FORMS_READ_BYTES];
    char why[FORMS_WHY_BYTES];
};

// the in and out bases the listing set up so far, a register each and its pair
struct Bases
{
    int in_low;
    int out_low;
};

// a number of a question, written in decimal, written in hexadecimal: -0x for a negative
static FormsWord hex_number(const char *decimal)
{
    FormsWord text;
    const TransfiguroCfg read = {decimal, NULL, 0u};
    // Explicit cast widens the signed word of the read to the long long of the value
    const long long value = (long long)mmgr_cellul_to_long(&read);
    VerbaLinea line = mmgr_line_over(text.text, sizeof(text.text));
    mmgr_line_put(&line, (value < 0) ? "-0x" : "0x");
    // the magnitude of the value, a negative read as its negation
    mmgr_line_hex(&line, (value < 0) ? (unsigned long long)(-value) : (unsigned long long)value, 1u);
    (void)mmgr_line_finish(&line);
    return text;
}

// the negative a word past the sign bit reads as when signed, or `number` itself, in decimal
static FormsWord number_negative(const char *number, unsigned long long word)
{
    FormsWord negative;
    if ((word >= 0x80000000ULL) && (word <= 0xffffffffULL))
    {
        negative.text[0] = '\0';
        // the word read as the negative 32-bit word it is when signed
        FORMS_ADD_SIGNED(negative, -(long long)(0x100000000ULL - word));
        return negative;
    }
    FORMS_SET(negative, number);
    return negative;
}

// the places of `literal` in `text` as a whole token, into `places`, at max `room` of them: their count
static unsigned int literal_places(const char *text, const char *literal, size_t *places, unsigned int room)
{
    const size_t length = forms_length(text);
    const size_t size = forms_length(literal);
    unsigned int count = 0u;
    for (size_t at = forms_find_n(text, length, literal, 0u); at < length;
         at = forms_find_n(text, length, literal, at + 1u))
    {
        const int before = (at == 0u) || !forms_identifier(text[at - 1u]);
        const int after = ((at + size) >= length) || !forms_identifier(text[at + size]);
        if (before && after)
        {
            if (count < room)
            {
                places[count] = at;
            }
            count += 1u;
        }
    }
    return count;
}

// `named` with the number `number` named `name` where it is a whole token exactly once. The number can be in hex or
// decimal. A word past the sign bit can also be the negative that it reads as when signed. 1 where the name applies
static int number_named(char *named, size_t room, const char *number, const char *name, char *why, size_t why_room)
{
    const unsigned long long word =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = number, .end = NULL, .base = 10);
    const FormsWord negative = number_negative(number, word);
    const FormsWord hex = hex_number(number);
    const FormsWord hex_negative = hex_number(negative.text);
    const char *const order[4] = {hex.text, number, hex_negative.text, negative.text};
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        size_t places[2];
        const unsigned int count = literal_places(named, order[one], places, 2u);
        if (count == 1u)
        {
            // the name put in the place of the one number
            static char s_rest[FORMS_READ_BYTES];
            const size_t length = forms_length(named);
            const size_t size = forms_length(order[one]);
            forms_set_n(s_rest, sizeof(s_rest), &named[places[0] + size], length - places[0] - size);
            named[places[0]] = '\0';
            forms_add(named, room, name);
            forms_add(named, room, s_rest);
            return 1;
        }
        if (count > 1u)
        {
            forms_add(why, why_room, "the number ");
            forms_add(why, why_room, number);
            forms_add(why, why_room, " is ");
            forms_add_decimal(why, why_room, count);
            forms_add(why, why_room, " times; ");
            return 0;
        }
    }
    return 0;
}

// the word a number of a role holds: 0 where it is negative
static unsigned long long role_word(const Role *role)
{
    return (role->number.text[0] == '-')
               ? 0ULL
               : EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = role->number.text, .end = NULL, .base = 10);
}

// `text` with each number argument named where its number is a whole token exactly once. A number past 32 bits that
// does not occur whole occurs as its two words. The low word gets the name of the number, and the high word its .hi
static void numbers_named(char *text, size_t room, const Question *question, char *why, size_t why_room)
{
    for (unsigned int at = 0u; at < question->role_count; at += 1u)
    {
        const Role *const role = &question->roles[at];
        if (!forms_same(role->place.text, "number"))
        {
            continue;
        }
        const unsigned long long word = role_word(role);
        FormsWord name;
        FORMS_SET(name, "{");
        FORMS_ADD(name, role->name.text);
        FORMS_ADD(name, "}");
        if (number_named(text, room, role->number.text, name.text, why, why_room) || (word <= 0xffffffffULL))
        {
            continue;
        }
        FormsWord low;
        low.text[0] = '\0';
        FORMS_ADD_DECIMAL(low, word & 0xffffffffULL);
        FormsWord high;
        high.text[0] = '\0';
        FORMS_ADD_DECIMAL(high, word >> 32u);
        FormsWord high_name = name;
        FORMS_ADD(high_name, ".hi");
        (void)number_named(text, room, low.text, name.text, why, why_room);
        (void)number_named(text, room, high.text, high_name.text, why, why_room);
    }
}

// 1 where `text`, a reading, carries the number of `role`: named where it occurs once, or located as its own token
// where it occurs more than once. 0 where it is nowhere in the answer, because the system folded it into a form of its
// own
static int number_carried(const char *text, const Role *role)
{
    // the bare name is the low word. The name followed by .hi is the high half alone, which does not carry the low word
    // a fold left behind
    FormsWord named;
    FORMS_SET(named, "{");
    FORMS_ADD(named, role->name.text);
    FORMS_ADD(named, "}");
    const size_t length = forms_length(text);
    const size_t size = forms_length(named.text);
    for (size_t low = forms_find_n(text, length, named.text, 0u); low < length;
         low = forms_find_n(text, length, named.text, low + 1u))
    {
        if (!forms_begins(&text[low + size], ".hi"))
        {
            return 1;
        }
    }
    const unsigned long long word = role_word(role);
    const FormsWord negative = number_negative(role->number.text, word);
    const FormsWord hex = hex_number(role->number.text);
    const FormsWord hex_negative = hex_number(negative.text);
    const char *const order[4] = {hex.text, role->number.text, hex_negative.text, negative.text};
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        size_t place = 0u;
        if (literal_places(text, order[one], &place, 1u) != 0u)
        {
            return 1;
        }
    }
    return 0;
}

// A token with every signed type marker written unsigned. An `s` that sits where the signedness of a type sits, after a
// dot and before a bit width, reads as `u`. Two operations that differ only in signedness write the same word through
// this. An operation that differs in any other way does not. The walk jumps from one `.s` to the next
static FormsLine as_unsigned(const char *token)
{
    FormsLine unsigned_token;
    FORMS_SET(unsigned_token, token);
    const size_t length = forms_length(unsigned_token.text);
    for (size_t at = forms_find_n(unsigned_token.text, length, ".s", 0u); at < length;
         at = forms_find_n(unsigned_token.text, length, ".s", at + 1u))
    {
        const size_t marker = at + 1u;
        // a byte of a text read as the unsigned byte the class takes
        if (((marker + 1u) < length) && (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM,
                                                    .byte = (unsigned char)unsigned_token.text[marker + 1u]) != 0))
        {
            unsigned_token.text[marker] = 'u';
        }
    }
    return unsigned_token;
}

// the writings that differ only in their signedness, a pair each, at max
#define FORMS_ALIKE_MAX 256u

struct FormsAlike
{
    FormsLine now;
    FormsLine read;
};

// Whether `read` is `now` with nothing changed but the signedness of one or more operations. The function walks both a
// word at a time, with whitespace between words:
//   - a word that matches passes.
//   - a word that matches only once the signedness goes to unsigned is an operation that the system writes either way.
//     The function adds it to `alike`.
//   - a word that differs in any other way, or a word with no partner, shows that the two are not one form written two
//     ways.
//
// The soundness is not here. The caller verifies that the system compiles both to the same machine code before it reads
// this. This function only names the writings that the caller may record alike
static int signedness_only(const char *now, const char *read, FormsAlike *alike, unsigned int *alike_count)
{
    static const char s_space_class[] = " \t\n\r\v\f";
    const size_t now_size = forms_length(now);
    const size_t read_size = forms_length(read);
    size_t here = 0u;
    size_t there = 0u;
    int any = 0;
    *alike_count = 0u;
    for (;;)
    {
        here +=
            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &now[here], .cap = now_size - here, .other = s_space_class);
        there += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &read[there], .cap = read_size - there,
                            .other = s_space_class);
        if ((here >= now_size) || (there >= read_size))
        {
            return any && (here >= now_size) && (there >= read_size);
        }
        const size_t now_end = here + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &now[here],
                                                 .cap = now_size - here, .other = " \t\n");
        const size_t read_end = there + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &read[there],
                                                   .cap = read_size - there, .other = " \t\n");
        FormsLine now_word;
        FORMS_SET_N(now_word, &now[here], now_end - here);
        FormsLine read_word;
        FORMS_SET_N(read_word, &read[there], read_end - there);
        here = now_end;
        there = read_end;
        if (forms_same(now_word.text, read_word.text))
        {
            continue;
        }
        if (!forms_same(as_unsigned(now_word.text).text, as_unsigned(read_word.text).text))
        {
            return 0;
        }
        if (*alike_count == FORMS_ALIKE_MAX)
        {
            forms_refused("more writings alike than the table holds: ", now_word.text, forms_length(now_word.text));
        }
        alike[*alike_count].now = now_word;
        alike[*alike_count].read = read_word;
        *alike_count += 1u;
        any = 1;
    }
}

// the lines of one question's three regions, and the values a block's registers take, each at max
#define FORMS_BLOCK_MAX 2048u
#define FORMS_VALUES_MAX 4096u

// what a block's register or predicate contains: an argument, its half, or scratch. The table drops a value it erases
struct RegisterContent
{
    FormsWord value;
    FormsWord argument;
    unsigned int half;
    int negated;
    int erased;
};

// a register of a block and the value that it contains now
struct FormsCurrent
{
    FormsWord token;
    FormsWord value;
};

// an instruction a block keeps, each operand with each register it reads written as its value's name between two \x02
struct Retained
{
    Instruction instruction;
    FormsOperand names[FORMS_OPERANDS_MAX];
    FormsOperand guard;
};

// The tables of one block's read: the value of each register now, what each value contains, the instructions kept and
// those dropped. Each is static at a bound, and a read clears its counts as it begins
static FormsCurrent s_current[FORMS_VALUES_MAX];
static unsigned int s_current_count = 0u;
static RegisterContent s_bound[FORMS_VALUES_MAX];
static unsigned int s_bound_count = 0u;
static Retained s_retained[FORMS_BLOCK_MAX];
static unsigned int s_retained_count = 0u;
static int s_dropped[FORMS_BLOCK_MAX];

// the value that the register `token` contains now [BORROWS], or NULL where it holds none of the block's
static const char *current_of(const char *token)
{
    for (unsigned int at = 0u; at < s_current_count; at += 1u)
    {
        if (forms_same(s_current[at].token.text, token))
        {
            return s_current[at].value.text;
        }
    }
    return NULL;
}

// the register `token` given the value `value`, in place of the one it had
static void current_put(const char *token, const char *value)
{
    for (unsigned int at = 0u; at < s_current_count; at += 1u)
    {
        if (forms_same(s_current[at].token.text, token))
        {
            FORMS_SET(s_current[at].value, value);
            return;
        }
    }
    if (s_current_count == FORMS_VALUES_MAX)
    {
        forms_refused("a block names more registers than the table holds: ", token, forms_length(token));
    }
    FORMS_SET(s_current[s_current_count].token, token);
    FORMS_SET(s_current[s_current_count].value, value);
    s_current_count += 1u;
}

// what the value `value` contains [BORROWS], or NULL where it is no argument's
static RegisterContent *bound_of(const char *value)
{
    for (unsigned int at = 0u; at < s_bound_count; at += 1u)
    {
        if (!s_bound[at].erased && forms_same(s_bound[at].value.text, value))
        {
            return &s_bound[at];
        }
    }
    return NULL;
}

// the value `value` bound to the half `half` of `argument`, negated where `negated` is 1, in place of what it had
static void bound_put(const char *value, const char *argument, unsigned int half, int negated)
{
    RegisterContent *content = bound_of(value);
    if (content == NULL)
    {
        if (s_bound_count == FORMS_VALUES_MAX)
        {
            forms_refused("a block holds more values than the table holds: ", value, forms_length(value));
        }
        content = &s_bound[s_bound_count];
        s_bound_count += 1u;
        FORMS_SET(content->value, value);
    }
    FORMS_SET(content->argument, argument);
    content->half = half;
    content->negated = negated;
    content->erased = 0;
}

// the role of `question` at `place` that holds the word `word`, and the half of it through `half`, or NULL
static const Role *role_at(const Question *question, const char *place, unsigned int word, unsigned int *half)
{
    for (unsigned int at = 0u; at < question->role_count; at += 1u)
    {
        const Role *const role = &question->roles[at];
        if (forms_same(role->place.text, place) &&
            ((role->word == word) || (forms_same(role->type.text, "u64") && ((role->word + 1u) == word))))
        {
            *half = word - role->word;
            return role;
        }
    }
    return NULL;
}

// the role of type int that the value `value` contains, or NULL
static const Role *int_role(const Question *question, const char *value)
{
    const RegisterContent *const found = bound_of(value);
    for (unsigned int at = 0u; (found != NULL) && (at < question->role_count); at += 1u)
    {
        const Role *const role = &question->roles[at];
        if (forms_same(role->name.text, found->argument.text) && forms_same(role->type.text, "int"))
        {
            return role;
        }
    }
    return NULL;
}

// the offset of an address `[R<n>+0x<offset>]`, or 0 where it names none
static unsigned int offset_of(const char *address)
{
    const size_t length = forms_length(address);
    const size_t plus = forms_chr_n(address, length, '+', 0u);
    // an offset into a question's words is far below 2^32
    return (plus == length) ? 0u
                            : (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &address[plus + 1u],
                                                       .end = NULL, .base = 16);
}

// `operand` with each register it reads written as its value's name between two \x02, into `renamed`
static void read_names(const char *operand, FormsOperand *renamed)
{
    renamed->text[0] = '\0';
    const size_t size = forms_length(operand);
    size_t from = 0u;
    size_t length = 0u;
    for (size_t found = sass_register(operand, 0u, &length); found < size;
         found = sass_register(operand, from, &length))
    {
        FormsWord token;
        FORMS_SET_N(token, &operand[found], length);
        const char *const value = current_of(token.text);
        FORMS_ADD_N(*renamed, &operand[from], found - from);
        if (value != NULL)
        {
            FORMS_ADD(*renamed, "\x02");
            FORMS_ADD(*renamed, value);
            FORMS_ADD(*renamed, "\x02");
        }
        else
        {
            FORMS_ADD(*renamed, token.text);
        }
        from = found + length;
    }
    FORMS_ADD_N(*renamed, &operand[from], size - from);
}

// the values of a block's register that `name` holds, at max this many
#define FORMS_NAME_VALUES_MAX 16u

// `name` with each value between two \x02 taken out into `values`, `*count` of them: the rest
static FormsOperand values_of(const char *name, FormsWord *values, unsigned int *count)
{
    FormsOperand rest;
    rest.text[0] = '\0';
    *count = 0u;
    const size_t size = forms_length(name);
    size_t from = 0u;
    for (size_t open = forms_chr_n(name, size, '\x02', 0u); open < size; open = forms_chr_n(name, size, '\x02', from))
    {
        const size_t close = forms_chr_n(name, size, '\x02', open + 1u);
        FORMS_ADD_N(rest, &name[from], open - from);
        if (*count == FORMS_NAME_VALUES_MAX)
        {
            forms_refused("an operand names more values than it holds: ", name, size);
        }
        FORMS_SET_N(values[*count], &name[open + 1u], (close > open) ? (close - open - 1u) : 0u);
        *count += 1u;
        from = (close < size) ? (close + 1u) : size;
    }
    FORMS_ADD_N(rest, &name[from], size - from);
    return rest;
}

// 1 where the rest of an operand is RZ, PT, a negation of one, a number or nothing
static int constant(const char *rest)
{
    const FormsLine bare = forms_trim(rest);
    return (bare.text[0] == '\0') || forms_same(bare.text, "RZ") || forms_same(bare.text, "PT") ||
           forms_same(bare.text, "!PT") || forms_same(bare.text, "!") || forms_begins(bare.text, "0x") ||
           forms_begins(bare.text, "-0x");
}

// 1 where a value that an instruction wrote is a predicate's
static int predicate_value(const char *value)
{
    const size_t size = forms_length(value);
    const size_t colon = forms_chr_n(value, size, ':', 0u);
    return (colon < size) && (value[colon + 1u] == 'P');
}

// the operand `at` of a kept instruction's names, or "" where it has fewer [BORROWS]
static const char *retained_name(const Retained *one, unsigned int at)
{
    return (at < one->instruction.operand_count) ? one->names[at].text : "";
}

// the scratch a block's reading names, a register of the compiler's each and the name the reading gives it, at max
#define FORMS_SCRATCH_MAX 512u

struct FormsScratch
{
    FormsWord physical;
    FormsWord name;
};

// the state of one resolution of a block's names: its scratch and the counts of scratch words and predicates
struct Resolution
{
    const Question *question;
    Read *read;
    FormsScratch scratch[FORMS_SCRATCH_MAX];
    unsigned int scratch_count;
    unsigned int words;
    unsigned int predicates;
};

// `with`, each value between two \x02 written as an argument's name, or as the scratch it is, onto `out`
static void resolved(Resolution *resolution, const char *with, FormsLine *out)
{
    const Question *const question = resolution->question;
    const size_t size = forms_length(with);
    size_t from = 0u;
    for (size_t open = forms_chr_n(with, size, '\x02', 0u); open < size; open = forms_chr_n(with, size, '\x02', from))
    {
        const size_t close = forms_chr_n(with, size, '\x02', open + 1u);
        FORMS_ADD_N(*out, &with[from], open - from);
        FormsWord value;
        FORMS_SET_N(value, &with[open + 1u], (close > open) ? (close - open - 1u) : 0u);
        const RegisterContent *const content = bound_of(value.text);
        if (content != NULL)
        {
            int fixed = 0;
            for (unsigned int at = 0u; at < question->role_count; at += 1u)
            {
                fixed = forms_same(question->roles[at].name.text, content->argument.text) ? question->roles[at].fixed
                                                                                          : fixed;
            }
            // the carry is P6, which sass.krs reserves for a chain's carry
            if (forms_same(content->argument.text, "carry"))
            {
                FORMS_ADD(*out, "P6");
            }
            else
            {
                FORMS_ADD(*out, fixed ? "<" : "{");
                FORMS_ADD(*out, content->argument.text);
                FORMS_ADD(*out, fixed ? ">" : "}");
                FORMS_ADD(*out, (content->half != 0u) ? ".hi" : "");
            }
        }
        else
        {
            // the compiler's own register, past the `:` of the value's name, or the whole name where it has none
            const size_t value_size = forms_length(value.text);
            const size_t colon = forms_chr_n(value.text, value_size, ':', 0u);
            const char *const physical = (colon < value_size) ? &value.text[colon + 1u] : value.text;
            unsigned int found = resolution->scratch_count;
            for (unsigned int at = 0u; at < resolution->scratch_count; at += 1u)
            {
                found = forms_same(resolution->scratch[at].physical.text, physical) ? at : found;
            }
            if (found == resolution->scratch_count)
            {
                if (found == FORMS_SCRATCH_MAX)
                {
                    forms_refused("a block names more scratch than the table holds: ", physical,
                                  forms_length(physical));
                }
                const int predicate = (physical[0] == 'P');
                unsigned int *const taken = predicate ? &resolution->predicates : &resolution->words;
                FORMS_SET(resolution->scratch[found].physical, physical);
                FORMS_SET(resolution->scratch[found].name, (*taken == 0u) ? (predicate ? "P6" : "R237") : physical);
                if (*taken == 1u)
                {
                    forms_add(resolution->read->why, sizeof(resolution->read->why), "more scratch ");
                    forms_add(resolution->read->why, sizeof(resolution->read->why), predicate ? "predicates" : "words");
                    forms_add(resolution->read->why, sizeof(resolution->read->why), " than one; ");
                }
                *taken += 1u;
                resolution->scratch_count += 1u;
            }
            FORMS_ADD(*out, resolution->scratch[found].name.text);
        }
        from = (close < size) ? (close + 1u) : size;
    }
    FORMS_ADD_N(*out, &with[from], size - from);
}

// `why` with the first place of `note` left out, in place
static void why_dropped(char *why, const char *note)
{
    const size_t length = forms_length(why);
    const size_t found = forms_find_n(why, length, note, 0u);
    if (found == length)
    {
        return;
    }
    const size_t size = forms_length(note);
    EMBED_CALL(memor.move_down, MemoriaCfg, .dst = &why[found], .src = &why[found + size],
               .bytes = length - found - size + 1u);
}

// a note of a read with a role's name between `head` and "; "
static void why_noted(Read *read, const char *head, const char *name)
{
    forms_add(read->why, sizeof(read->why), head);
    forms_add(read->why, sizeof(read->why), name);
    forms_add(read->why, sizeof(read->why), "; ");
}

// The SASS block of the `count` lines at `lines` read back into the form that `question` puts, into `read`. The bases
// that the opening of the entry sets up carry from one block to the next through `bases`
static void sass_read(const Question *question, const char *const *lines, unsigned int count, Bases *bases, Read *read)
{
    read->text[0] = '\0';
    read->why[0] = '\0';
    // the value that each register contains now, by a name of its own: L<n> loaded, W<n>:<register> written by kept
    // instruction n. A value bound to an argument takes the name of the argument
    s_current_count = 0u;
    s_bound_count = 0u;
    s_retained_count = 0u;
    unsigned int loads = 0u;
    for (unsigned int line = 0u; line < count; line += 1u)
    {
        const Instruction instruction = instruction_read(lines[line]);
        const char *const operation = instruction.operation.text;
        const FormsOperand *const operands = instruction.operands;
        const unsigned int operand_count = instruction.operand_count;
        // the bases: a register set from the entry's parameters, `in` at 0x160 and `out` at 0x168
        if ((operand_count >= 2u) && forms_begins(operands[operand_count - 1u].text, "c[0x0][0x1") &&
            (forms_same(operation, "MOV") || forms_same(operation, "IMAD.MOV.U32")))
        {
            // a place in the constant bank is far below 2^32
            const unsigned int place =
                (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                         .src = &operands[operand_count - 1u].text[7], .end = NULL, .base = 16);
            if ((place >= 0x160u) && (place < 0x170u))
            {
                // a register's number is far below 2^31
                bases->in_low = (place == 0x160u) ? (int)token_number(operands[0].text) : bases->in_low;
                bases->out_low = (place == 0x168u) ? (int)token_number(operands[0].text) : bases->out_low;
                continue;
            }
        }
        if (forms_begins(operation, "ULDC"))
        {
            continue;
        }
        // a load from `in`: the register it writes contains the argument loaded
        if (forms_begins(operation, "LDG") && (operand_count == 2u) && (operands[1].text[0] == '[') &&
            ((int)token_number(&operands[1].text[1]) == bases->in_low))
        {
            unsigned int half = 0u;
            const Role *const role = role_at(question, "in", offset_of(operands[1].text) / 4u, &half);
            if (role != NULL)
            {
                const unsigned int first = token_number(operands[0].text);
                const unsigned int words = forms_has(operation, ".64") ? 2u : 1u;
                for (unsigned int word = 0u; word < words; word += 1u)
                {
                    FormsWord value;
                    FORMS_SET(value, "L");
                    FORMS_ADD_DECIMAL(value, loads);
                    loads += 1u;
                    FormsWord token;
                    FORMS_SET(token, "R");
                    FORMS_ADD_DECIMAL(token, first + word);
                    current_put(token.text, value.text);
                    bound_put(value.text, role->name.text, half + word, 0);
                }
                continue;
            }
        }
        // a store to `out`: the value of the register it reads is the argument stored
        if (forms_begins(operation, "STG") && (operand_count == 2u) && (operands[0].text[0] == '[') &&
            ((int)token_number(&operands[0].text[1]) == bases->out_low))
        {
            unsigned int half = 0u;
            const Role *const role = role_at(question, "out", offset_of(operands[0].text) / 4u, &half);
            if (role != NULL)
            {
                const unsigned int words = forms_has(operation, ".64") ? 2u : 1u;
                for (unsigned int word = 0u; word < words; word += 1u)
                {
                    FormsWord token;
                    if (forms_same(operands[1].text, "RZ"))
                    {
                        FORMS_SET(token, "RZ");
                    }
                    else
                    {
                        FORMS_SET(token, "R");
                        FORMS_ADD_DECIMAL(token, token_number(operands[1].text) + word);
                    }
                    const char *const value = current_of(token.text);
                    if (value == NULL)
                    {
                        forms_add(read->why, sizeof(read->why), "the compiler stores ");
                        forms_add(read->why, sizeof(read->why), token.text);
                        why_noted(read, " for ", role->name.text);
                        continue;
                    }
                    if (value[0] == 'L')
                    {
                        forms_add(read->why, sizeof(read->why), "the compiler stores ");
                        forms_add(read->why, sizeof(read->why), role->name.text);
                        forms_add(read->why, sizeof(read->why), " straight from a load; ");
                    }
                    FormsWord held;
                    FORMS_SET(held, value);
                    bound_put(held.text, role->name.text, half + word, 0);
                }
                continue;
            }
        }
        if (s_retained_count == FORMS_BLOCK_MAX)
        {
            forms_refused("a block keeps more instructions than the table holds", "", 0u);
        }
        Retained *const one = &s_retained[s_retained_count];
        one->instruction = instruction;
        for (unsigned int at = 0u; at < operand_count; at += 1u)
        {
            one->names[at].text[0] = '\0';
        }
        const unsigned int written = sass_written(&instruction);
        for (unsigned int at = written; at < operand_count; at += 1u)
        {
            read_names(operands[at].text, &one->names[at]);
        }
        one->guard.text[0] = '\0';
        if (instruction.guard.text[0] != '\0')
        {
            read_names(instruction.guard.text, &one->guard);
        }
        FormsWord id;
        FORMS_SET(id, "W");
        FORMS_ADD_DECIMAL(id, s_retained_count);
        FORMS_ADD(id, ":");
        for (unsigned int at = 0u; at < written; at += 1u)
        {
            size_t length = 0u;
            const size_t size = forms_length(operands[at].text);
            const size_t found = sass_register(operands[at].text, 0u, &length);
            if (found == size)
            {
                one->names[at] = operands[at];
                continue;
            }
            FormsWord token;
            FORMS_SET_N(token, &operands[at].text[found], length);
            FormsWord value = id;
            FORMS_ADD(value, token.text);
            current_put(token.text, value.text);
            FORMS_SET_N(one->names[at], operands[at].text, found);
            FORMS_ADD(one->names[at], "\x02");
            FORMS_ADD(one->names[at], value.text);
            FORMS_ADD(one->names[at], "\x02");
            FORMS_ADD(one->names[at], &operands[at].text[found + length]);
            if ((at == 0u) && (token.text[0] == 'R') && sass_wide_write(&instruction))
            {
                FormsWord next;
                FORMS_SET(next, "R");
                FORMS_ADD_DECIMAL(next, token_number(token.text) + 1u);
                FormsWord next_value = id;
                FORMS_ADD(next_value, next.text);
                current_put(next.text, next_value.text);
            }
        }
        s_dropped[s_retained_count] = 0;
        s_retained_count += 1u;
    }
    // A predicate argument, and the carry, cross the edge of the block as a word.
    //   - read in: an instruction that meets three conditions sets the predicate from the word. Its one value read is
    //     the word loaded for it. The rest are RZ, PT and numbers. It writes only predicates and RZ.
    //   - stored: an instruction that meets three conditions writes the word from the predicate. It writes the word
    //     stored for it. Its one value read is a predicate. The rest are RZ, PT and numbers. A SEL of RZ and 1 is 1
    //     where the predicate is true. Any other form reads the predicate as written.
    //
    // The tool drops each such instruction from the form, and its predicate is the argument
    int set_negated = 0;
    for (unsigned int at = 0u; at < s_retained_count; at += 1u)
    {
        const Retained *const one = &s_retained[at];
        const Instruction *const instruction = &one->instruction;
        const unsigned int written = sass_written(instruction);
        FormsWord reads[FORMS_NAME_VALUES_MAX * FORMS_OPERANDS_MAX];
        unsigned int read_count = 0u;
        int others = 0;
        int inverted = 0;
        for (unsigned int operand = written; operand < instruction->operand_count; operand += 1u)
        {
            FormsWord values[FORMS_NAME_VALUES_MAX];
            unsigned int value_count = 0u;
            const FormsOperand rest = values_of(one->names[operand].text, values, &value_count);
            inverted = inverted || ((value_count != 0u) && forms_has(rest.text, "!"));
            others = others || !constant(rest.text);
            for (unsigned int value = 0u; value < value_count; value += 1u)
            {
                reads[read_count] = values[value];
                read_count += 1u;
            }
        }
        FormsWord writes[FORMS_NAME_VALUES_MAX * FORMS_OPERANDS_MAX];
        unsigned int write_count = 0u;
        int written_words = 0;
        for (unsigned int operand = 0u; operand < written; operand += 1u)
        {
            FormsWord values[FORMS_NAME_VALUES_MAX];
            unsigned int value_count = 0u;
            (void)values_of(one->names[operand].text, values, &value_count);
            for (unsigned int value = 0u; value < value_count; value += 1u)
            {
                written_words = written_words || !predicate_value(values[value].text);
                writes[write_count] = values[value];
                write_count += 1u;
            }
        }
        if (others || (read_count != 1u) || (instruction->guard.text[0] != '\0'))
        {
            continue;
        }
        const Role *const read_role = int_role(question, reads[0].text);
        if ((read_role != NULL) && (reads[0].text[0] == 'L') && !written_words && (write_count != 0u))
        {
            const int negated = forms_has(instruction->operation.text, ".EQ");
            for (unsigned int value = 0u; value < write_count; value += 1u)
            {
                bound_put(writes[value].text, read_role->name.text, 0u, negated);
            }
            s_dropped[at] = 1;
            if (negated)
            {
                why_noted(read, "the compiler reads the negation of ", read_role->name.text);
            }
            continue;
        }
        const Role *const write_role = (write_count == 1u) ? int_role(question, writes[0].text) : NULL;
        if ((write_role != NULL) && predicate_value(reads[0].text))
        {
            int negated = inverted;
            if (forms_same(instruction->operation.text, "SEL"))
            {
                const int straight =
                    forms_same(retained_name(one, 1u), "RZ") && forms_same(retained_name(one, 2u), "0x1");
                const int crossed =
                    forms_same(retained_name(one, 1u), "0x1") && forms_same(retained_name(one, 2u), "RZ");
                if (!straight && !crossed)
                {
                    continue;
                }
                negated = straight ? !inverted : inverted;
            }
            bound_put(reads[0].text, write_role->name.text, 0u, negated);
            RegisterContent *const erased = bound_of(writes[0].text);
            if (erased != NULL)
            {
                erased->erased = 1;
            }
            s_dropped[at] = 1;
            set_negated = set_negated || negated;
            if (negated)
            {
                why_noted(read, "the compiler sets the negation of ", write_role->name.text);
            }
        }
    }
    // A predicate stored as its negation, where every instruction left is an ISETP anded with PT: each comparison
    // turned over sets the predicate itself. A chain through .EX turns over whole, by De Morgan. LT.EX under LT becomes
    // GE.EX under GE, and EQ.EX under EQ becomes NE.EX under NE
    if (set_negated)
    {
        static const char *const turned[6][2] = {{".EQ.", ".NE."}, {".NE.", ".EQ."}, {".LT.", ".GE."},
                                                 {".GE.", ".LT."}, {".GT.", ".LE."}, {".LE.", ".GT."}};
        int turnable = 1;
        for (unsigned int at = 0u; at < s_retained_count; at += 1u)
        {
            turnable =
                turnable && (s_dropped[at] || (forms_begins(s_retained[at].instruction.operation.text, "ISETP.") &&
                                               (s_retained[at].instruction.operand_count >= 5u) &&
                                               forms_same(s_retained[at].names[4].text, "PT")));
        }
        for (unsigned int at = 0u; turnable && (at < s_retained_count); at += 1u)
        {
            char *const operation = s_retained[at].instruction.operation.text;
            for (unsigned int one = 0u; !s_dropped[at] && (one < 6u); one += 1u)
            {
                if (forms_find(operation, turned[one][0], 0u) == 5u)
                {
                    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &operation[5], .src = turned[one][1], .bytes = 4u);
                    break;
                }
            }
        }
        for (unsigned int at = 0u; turnable && (at < question->role_count); at += 1u)
        {
            FormsLine note;
            FORMS_SET(note, "the compiler sets the negation of ");
            FORMS_ADD(note, question->roles[at].name.text);
            FORMS_ADD(note, "; ");
            why_dropped(read->why, note.text);
        }
    }
    // the pair a 64-bit write gives: where one half is an argument's, the other is the argument's other half
    for (unsigned int at = 0u; at < s_retained_count; at += 1u)
    {
        const Instruction *const instruction = &s_retained[at].instruction;
        size_t length = 0u;
        const size_t size = (instruction->operand_count != 0u) ? forms_length(instruction->operands[0].text) : 0u;
        const size_t found =
            (instruction->operand_count != 0u) ? sass_register(instruction->operands[0].text, 0u, &length) : 0u;
        if ((instruction->operand_count == 0u) || (found == size) || (sass_written(instruction) == 0u) ||
            !sass_wide_write(instruction) || (instruction->operands[0].text[found] != 'R'))
        {
            continue;
        }
        FormsWord token;
        FORMS_SET_N(token, &instruction->operands[0].text[found], length);
        const unsigned int first = token_number(token.text);
        FormsWord low;
        FORMS_SET(low, "W");
        FORMS_ADD_DECIMAL(low, at);
        FORMS_ADD(low, ":R");
        FormsWord high = low;
        FORMS_ADD_DECIMAL(low, first);
        FORMS_ADD_DECIMAL(high, first + 1u);
        const RegisterContent *const low_bound = bound_of(low.text);
        const RegisterContent *const high_bound = bound_of(high.text);
        if ((low_bound != NULL) && (high_bound == NULL) && (low_bound->half == 0u))
        {
            const FormsWord argument = low_bound->argument;
            bound_put(high.text, argument.text, 1u, 0);
        }
        else if ((high_bound != NULL) && (low_bound == NULL) && (high_bound->half == 1u))
        {
            const FormsWord argument = high_bound->argument;
            bound_put(low.text, argument.text, 0u, 0);
        }
    }
    // every value left: an argument's, or scratch, named by the compiler's own register for it
    static Resolution s_resolution;
    Resolution *const resolution = &s_resolution;
    resolution->question = question;
    resolution->read = read;
    resolution->scratch_count = 0u;
    resolution->words = 0u;
    // a form that carries occupies P6 already, and any predicate scratch beside it is one more than sass.krs reserves
    resolution->predicates = 0u;
    for (unsigned int at = 0u; at < question->role_count; at += 1u)
    {
        resolution->predicates = forms_same(question->roles[at].name.text, "carry") ? 1u : resolution->predicates;
    }
    for (unsigned int at = 0u; at < s_retained_count; at += 1u)
    {
        if (s_dropped[at])
        {
            continue;
        }
        const Retained *const one = &s_retained[at];
        FormsLine line;
        FORMS_SET(line, "\t");
        if (one->guard.text[0] != '\0')
        {
            FORMS_ADD(line, "@");
            resolved(resolution, one->guard.text, &line);
            FORMS_ADD(line, " ");
        }
        FORMS_ADD(line, one->instruction.operation.text);
        for (unsigned int operand = 0u; operand < one->instruction.operand_count; operand += 1u)
        {
            FORMS_ADD(line, (operand == 0u) ? " \t" : ", ");
            resolved(resolution, one->names[operand].text, &line);
        }
        FORMS_ADD(line, ";\n");
        forms_add(read->text, sizeof(read->text), line.text);
    }
    numbers_named(read->text, sizeof(read->text), question, read->why, sizeof(read->why));
}

// ---------------------------------------------------------------------------------------------------------------
// PTX

// the parameters the PTX loads its bases from at max
#define FORMS_PARAMETER_LOADS_MAX 64u

// a register the PTX loads from a parameter, and the parameter
struct PtxParameter
{
    FormsLine register_name;
    FormsLine parameter;
};

// the PTX's bases: the register `in` is in and the register `out` is in, and the registers loaded from parameters
struct PtxBases
{
    FormsLine in;
    FormsLine out;
    PtxParameter parameters[FORMS_PARAMETER_LOADS_MAX];
    unsigned int parameter_count;
};

// the parameter that the PTX loaded the register `name` from, or "" [BORROWS]
static const char *ptx_parameter_of(const PtxBases *bases, const char *name)
{
    for (unsigned int at = 0u; at < bases->parameter_count; at += 1u)
    {
        if (forms_same(bases->parameters[at].register_name.text, name))
        {
            return bases->parameters[at].parameter.text;
        }
    }
    return "";
}

// the register `name` loaded from `parameter`, in place of what it had
static void ptx_parameter_put(PtxBases *bases, const char *name, const char *parameter)
{
    for (unsigned int at = 0u; at < bases->parameter_count; at += 1u)
    {
        if (forms_same(bases->parameters[at].register_name.text, name))
        {
            FORMS_SET(bases->parameters[at].parameter, parameter);
            return;
        }
    }
    if (bases->parameter_count == FORMS_PARAMETER_LOADS_MAX)
    {
        forms_refused("the PTX loads more parameters than the table holds: ", name, forms_length(name));
    }
    FORMS_SET(bases->parameters[bases->parameter_count].register_name, name);
    FORMS_SET(bases->parameters[bases->parameter_count].parameter, parameter);
    bases->parameter_count += 1u;
}

// the base of an address `[<base>+<offset>]` into `base`, and its offset in decimal, or 0 where it names none
static unsigned int ptx_address(const char *operand, FormsLine *base)
{
    const size_t size = forms_length(operand);
    const size_t plus = forms_chr_n(operand, size, '+', 0u);
    const size_t end = (plus == size) ? (size - 1u) : plus;
    FORMS_SET_N(*base, &operand[1], (end > 1u) ? (end - 1u) : 0u);
    // an offset into a question's words is far below 2^32
    return (plus == size) ? 0u
                          : (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &operand[plus + 1u],
                                                     .end = NULL, .base = 10);
}

// 1 where a PTX register starts at `at` of `text`: a `%` and a byte of an identifier
static int ptx_register_start(const char *text, size_t size, size_t at)
{
    return (text[at] == '%') && ((at + 1u) < size) && forms_identifier(text[at + 1u]);
}

// the PTX's own temporaries a reading names, at max
#define FORMS_TEMPORARIES_MAX 512u

// the role of `question` that a register contains, its argument's, or NULL
static const Role *role_of_argument(const Question *question, const char *argument)
{
    for (unsigned int at = 0u; at < question->role_count; at += 1u)
    {
        if (forms_same(question->roles[at].name.text, argument))
        {
            return &question->roles[at];
        }
    }
    return NULL;
}

// `operand` with each PTX register an argument contains written as the argument's name onto `out`, and each register no
// argument contains put among `temporaries`, `%tid` and `%ntid` aside. The walk jumps from one `%` to the next
static void ptx_renamed(const Question *question, const char *operand, FormsLine *out, FormsWord *temporaries,
                        unsigned int *temporary_count)
{
    const size_t size = forms_length(operand);
    size_t at = 0u;
    while (at < size)
    {
        const size_t percent = forms_chr_n(operand, size, '%', at);
        FORMS_ADD_N(*out, &operand[at], percent - at);
        at = percent;
        if (at >= size)
        {
            break;
        }
        if (!ptx_register_start(operand, size, at))
        {
            FORMS_ADD_N(*out, &operand[at], 1u);
            at += 1u;
            continue;
        }
        size_t end = at + 1u;
        end += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &operand[end], .cap = size - end,
                          .other = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
        FormsWord token;
        FORMS_SET_N(token, &operand[at], end - at);
        const RegisterContent *const content = bound_of(token.text);
        if (content != NULL)
        {
            const Role *const role = role_of_argument(question, content->argument.text);
            const int fixed = (role != NULL) ? role->fixed : 0;
            FORMS_ADD(*out, fixed ? "<" : "{");
            FORMS_ADD(*out, content->argument.text);
            FORMS_ADD(*out, fixed ? ">" : "}");
        }
        else
        {
            FORMS_ADD(*out, token.text);
            if (!forms_begins(token.text, "%tid") && !forms_begins(token.text, "%ntid"))
            {
                int seen = 0;
                for (unsigned int one = 0u; !seen && (one < *temporary_count); one += 1u)
                {
                    seen = forms_same(temporaries[one].text, token.text);
                }
                if (!seen)
                {
                    if (*temporary_count == FORMS_TEMPORARIES_MAX)
                    {
                        forms_refused("a block names more temporaries than the table holds: ", token.text,
                                      forms_length(token.text));
                    }
                    temporaries[*temporary_count] = token;
                    *temporary_count += 1u;
                }
            }
        }
        at = end;
    }
}

// The PTX block of the `count` lines at `lines` read back into the form that `question` puts, into `read`. The bases
// that the opening of the entry sets up carry from one block to the next through `bases`
static void ptx_read(const Question *question, const char *const *lines, unsigned int count, PtxBases *bases,
                     Read *read)
{
    read->text[0] = '\0';
    read->why[0] = '\0';
    // what each register contains, by the register: the table of values of the SASS read, a register's name the value
    s_bound_count = 0u;
    s_retained_count = 0u;
    for (unsigned int line = 0u; line < count; line += 1u)
    {
        const Instruction instruction = instruction_read(lines[line]);
        const char *const operation = instruction.operation.text;
        const FormsOperand *const operands = instruction.operands;
        if (forms_begins(operation, "ld.param.") && (instruction.operand_count == 2u))
        {
            ptx_parameter_put(bases, operands[0].text, operands[1].text);
            continue;
        }
        if (forms_begins(operation, "cvta.to.gl") && (instruction.operand_count == 2u))
        {
            const char *const parameter = ptx_parameter_of(bases, operands[1].text);
            if (forms_has(parameter, "param_0"))
            {
                FORMS_SET(bases->in, operands[0].text);
            }
            if (forms_has(parameter, "param_1"))
            {
                FORMS_SET(bases->out, operands[0].text);
            }
            continue;
        }
        if (forms_begins(operation, "ld.volatile.global.") && (instruction.operand_count == 2u))
        {
            FormsLine base;
            const unsigned int offset = ptx_address(operands[1].text, &base);
            const Role *role = NULL;
            for (unsigned int one = 0u; one < question->role_count; one += 1u)
            {
                const Role *const candidate = &question->roles[one];
                role = (forms_same(base.text, bases->in.text) && forms_same(candidate->place.text, "in") &&
                        ((candidate->word * 4u) == offset))
                           ? candidate
                           : role;
            }
            if (role != NULL)
            {
                bound_put(operands[0].text, role->name.text, 0u, 0);
                continue;
            }
        }
        if (forms_begins(operation, "st.volatile.global.") && (instruction.operand_count == 2u))
        {
            FormsLine base;
            const unsigned int offset = ptx_address(operands[0].text, &base);
            const Role *role = NULL;
            for (unsigned int one = 0u; one < question->role_count; one += 1u)
            {
                const Role *const candidate = &question->roles[one];
                role = (forms_same(base.text, bases->out.text) && forms_same(candidate->place.text, "out") &&
                        ((candidate->word * 4u) == offset))
                           ? candidate
                           : role;
            }
            if (role != NULL)
            {
                bound_put(operands[1].text, role->name.text, 0u, 0);
                continue;
            }
        }
        if (s_retained_count == FORMS_BLOCK_MAX)
        {
            forms_refused("a block keeps more instructions than the table holds", "", 0u);
        }
        s_retained[s_retained_count].instruction = instruction;
        s_dropped[s_retained_count] = 0;
        s_retained_count += 1u;
    }
    // a predicate argument read in, and one stored
    for (unsigned int at = 0u; at < s_retained_count; at += 1u)
    {
        const Instruction *const instruction = &s_retained[at].instruction;
        // the last role of type int that a register contains, or NULL
        const RegisterContent *content = NULL;
        const Role *role = NULL;
        if (forms_begins(instruction->operation.text, "setp.ne") && (instruction->operand_count == 3u) &&
            forms_same(instruction->operands[2].text, "0"))
        {
            content = bound_of(instruction->operands[1].text);
            for (unsigned int one = 0u; (content != NULL) && (one < question->role_count); one += 1u)
            {
                const Role *const candidate = &question->roles[one];
                role = (forms_same(candidate->name.text, content->argument.text) &&
                        forms_same(candidate->type.text, "int"))
                           ? candidate
                           : role;
            }
            if (role != NULL)
            {
                bound_put(instruction->operands[0].text, role->name.text, 0u, 0);
                s_dropped[at] = 1;
            }
        }
        role = NULL;
        if (forms_begins(instruction->operation.text, "selp.") && (instruction->operand_count == 4u))
        {
            content = bound_of(instruction->operands[0].text);
            for (unsigned int one = 0u; (content != NULL) && (one < question->role_count); one += 1u)
            {
                const Role *const candidate = &question->roles[one];
                role = (forms_same(candidate->name.text, content->argument.text) &&
                        forms_same(candidate->type.text, "int"))
                           ? candidate
                           : role;
            }
            if (role != NULL)
            {
                const int negated = forms_same(instruction->operands[1].text, "0") ? 1 : 0;
                const FormsWord name = role->name;
                bound_put(instruction->operands[3].text, name.text, 0u, negated);
                s_dropped[at] = 1;
                if (negated)
                {
                    why_noted(read, "the compiler sets the negation of ", name.text);
                }
            }
        }
    }
    // the PTX's own registers renamed; one no argument occupies is a temporary of the compiler's
    static FormsWord s_temporaries[FORMS_TEMPORARIES_MAX];
    unsigned int temporaries = 0u;
    for (unsigned int at = 0u; at < s_retained_count; at += 1u)
    {
        if (s_dropped[at])
        {
            continue;
        }
        const Instruction *const instruction = &s_retained[at].instruction;
        FormsLine line;
        FORMS_SET(line, "\t");
        if (instruction->guard.text[0] != '\0')
        {
            FORMS_ADD(line, "@");
            ptx_renamed(question, instruction->guard.text, &line, s_temporaries, &temporaries);
            FORMS_ADD(line, " ");
        }
        FORMS_ADD(line, instruction->operation.text);
        for (unsigned int one = 0u; one < instruction->operand_count; one += 1u)
        {
            FORMS_ADD(line, (one == 0u) ? " \t" : ", ");
            ptx_renamed(question, instruction->operands[one].text, &line, s_temporaries, &temporaries);
        }
        FORMS_ADD(line, ";\n");
        forms_add(read->text, sizeof(read->text), line.text);
    }
    if (temporaries != 0u)
    {
        forms_add_decimal(read->why, sizeof(read->why), temporaries);
        forms_add(read->why, sizeof(read->why), " temporaries of the compiler's; ");
    }
    numbers_named(read->text, sizeof(read->text), question, read->why, sizeof(read->why));
}

// A form's text as one line for a table, onto `flat`. The whitespace between words is a space, and each line end is a
// ` |`. The code cuts the spaces and bars at its end, and the walk jumps from run to run of whitespace
static void text_flat(const char *text, PlatformText *flat)
{
    static const char s_white[] = "\t \n";
    const size_t size = forms_length(text);
    platform_text_reset(flat);
    int space = 0;
    size_t written = 0u;
    char last = '\0';
    size_t at = 0u;
    while (at < size)
    {
        const size_t white =
            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = size - at, .other = s_white);
        if (white != 0u)
        {
            // a byte of the run read while the line is not empty marks a space; the first newline makes it not empty
            const size_t newline = forms_chr_n(&text[at], white, '\n', 0u);
            const int had = (written != 0u);
            space = space || had || ((newline < white) && ((newline + 1u) < white));
            for (size_t one = newline; one < white; one = forms_chr_n(&text[at], white, '\n', one + 1u))
            {
                mmgr_line_put(&flat->line, " |");
                written += 2u;
                last = '|';
            }
            at += white;
            continue;
        }
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = size - at, .other = s_white);
        if (space && (written != 0u) && (last != ' '))
        {
            mmgr_line_ch(&flat->line, ' ');
            written += 1u;
        }
        space = 0;
        (void)mmgr_line_put_n(&flat->line, &text[at], run);
        written += run;
        last = text[at + run - 1u];
        at += run;
    }
    size_t flat_size = 0u;
    const char *const bytes = forms_text_bytes(flat, &flat_size);
    platform_text_cut(flat, forms_end_outside(bytes, flat_size, "| "));
}

// `text` with every `|` written `\|`, onto `line`. The walk jumps from one `|` to the next
static void cell_put(VerbaLinea *line, const char *text, size_t size)
{
    size_t at = 0u;
    while (at < size)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = size - at, .other = "|");
        (void)mmgr_line_put_n(line, &text[at], run);
        at += run;
        if (at < size)
        {
            mmgr_line_put(line, "\\|");
            at += 1u;
        }
    }
}

// a form's text flat and with its bars escaped, onto `line`
static void flat_cell_put(VerbaLinea *line, const char *text, PlatformText *flat)
{
    text_flat(text, flat);
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(flat, &size);
    cell_put(line, bytes, size);
}

// a question's three regions joined, its loads, its form and its stores, into `lines`: their count
static unsigned int regions_joined(const Blocks *blocks, unsigned int tag, const char **lines, unsigned int room)
{
    const unsigned int regions[3] = {MONOLITH_FORMS_LOADS + tag, tag, MONOLITH_FORMS_STORES + tag};
    unsigned int joined = 0u;
    for (unsigned int region = 0u; region < 3u; region += 1u)
    {
        unsigned int count = 0u;
        const unsigned int first = blocks_of(blocks, regions[region], &count);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (joined == room)
            {
                forms_refused("a question's regions hold more lines than the table holds", "", 0u);
            }
            lines[joined] = blocks->lines[first + at].text.text;
            joined += 1u;
        }
    }
    return joined;
}

// the instructions a form's text contains, a line each
static size_t text_instructions(const char *text)
{
    return forms_count(text, "\n");
}

// the part's machine file. The tool assembles a SASS form that it reads against this file before it writes the form
static SassMachine s_machine;
static int s_machine_read;

// 1 where every line of `text`, a SASS form with named parameters, assembles against the machine file. Each parameter
// takes a value:
//   - each word parameter takes a register pair of its own from R10. The assembler reads its .hi as the second of the
//     pair.
//   - each predicate parameter takes a predicate from P0.
//   - each number parameter takes the number that its question put, loaded or not, as the lanes write a number into the
//     instruction.
static int sass_assembles(const char *text, const Question *question)
{
    if (!s_machine_read)
    {
        return 0;
    }
    PlatformText filled;
    forms_text_begun(&filled);
    mmgr_line_put(&filled.line, text);
    unsigned int pair = 10u;
    unsigned int predicate = 0u;
    for (unsigned int at = 0u; at < question->role_count; at += 1u)
    {
        const Role *const role = &question->roles[at];
        if (role->fixed)
        {
            continue;
        }
        // a number's .hi is its high word, and the number itself its low word, each the hexadecimal immediate that the
        // assembler reads. The assembler would take a bare decimal for a register number, which tests the wrong operand
        // type
        if (forms_same(role->place.text, "number") || role->opaque)
        {
            const unsigned long long word = role_word(role);
            FormsWord high;
            FORMS_SET(high, "{");
            FORMS_ADD(high, role->name.text);
            FORMS_ADD(high, "}.hi");
            FormsWord high_word;
            high_word.text[0] = '\0';
            FORMS_ADD_DECIMAL(high_word, word >> 32u);
            forms_text_replaced(&filled, high.text, hex_number(high_word.text).text);
            FormsWord low;
            low.text[0] = '\0';
            if ((role->number.text[0] != '-') && (word > 0xffffffffULL))
            {
                FORMS_ADD_DECIMAL(low, word & 0xffffffffULL);
            }
            else
            {
                FORMS_SET(low, role->number.text);
            }
            forms_text_fill(&filled, role->name.text, hex_number(low.text).text);
            continue;
        }
        FormsWord put;
        if (forms_same(role->type.text, "int"))
        {
            FORMS_SET(put, "P");
            FORMS_ADD_DECIMAL(put, predicate);
            forms_text_fill(&filled, role->name.text, put.text);
            predicate += 1u;
            continue;
        }
        FORMS_SET(put, "R");
        FORMS_ADD_DECIMAL(put, pair);
        forms_text_fill(&filled, role->name.text, put.text);
        pair += 2u;
    }
    static unsigned char code[256];
    size_t size = 0u;
    const char *const bytes = forms_text_bytes(&filled, &size);
    int assembles = 1;
    for (size_t from = 0u; assembles && (from < size);)
    {
        const size_t stop = forms_chr_n(bytes, size, '\n', from);
        FormsLine line;
        FORMS_SET_N(line, &bytes[from], stop - from);
        FORMS_ADD(line, "\n");
        from = stop + 1u;
        size_t first = 0u;
        if (forms_trimmed(line.text, forms_length(line.text), " \t", &first) <= 1u)
        {
            continue;
        }
        assembles = (sass_assemble_lines(&s_machine, line.text, SASS_CONTROL_SAFE, code, sizeof(code)) != 0u);
    }
    platform_text_ended(&filled);
    return assembles;
}

// 1 where `text` reaches memory through a space named: SASS's global load and store, PTX's .global
static int text_spaced(const char *text, int sass)
{
    return sass ? (forms_has(text, "LDG") || forms_has(text, "STG")) : forms_has(text, ".global");
}

// the reason that the tool adopts no reading, `head` then `middle`, put into `why` in place of what it had
static void why_set(char *why, size_t room, const char *head, const char *middle)
{
    why[0] = '\0';
    forms_add(why, room, head);
    forms_add(why, room, middle);
}

// The tool writes a form that it reads into a ruleset where it is the whole form:
//   - it names every parameter that the form of the ruleset takes.
//   - each fixed register that it names is one that the ruleset names.
//   - it contains no register of the compiler's own past the scratch that the ruleset reserves.
//   - the reading left nothing unsettled.
//   - it contains no branch, because the target of a branch is the monolith's own address.
//
// Where the form of the ruleset reaches memory through a named space and the reading does not, the question named none.
// `c.krs` writes an address as a plain pointer. Of two correct writings, the one with fewer instructions wins. The tool
// does not write a reading longer than the form of the ruleset. The text to write goes into `text`, or empty with the
// reason through `why`
static void read_adopted(const Read *read, const Krs *rules, const Question *question, int sass, char *text,
                         size_t room, char *why, size_t why_room)
{
    text[0] = '\0';
    why[0] = '\0';
    const KrsEntry *const found = krs_find(rules, KRS_FORM, question->form.text);
    const char *const now = (found == NULL) ? "" : krs_text(rules, found);
    if (forms_has(read->text, "BRA") || forms_has(read->text, "bra "))
    {
        why_set(why, why_room, "the reading contains a branch", "");
        return;
    }
    if (text_spaced(now, sass) && (read->text[0] != '\0') && !text_spaced(read->text, sass))
    {
        why_set(why, why_room, "the question names no address space", "");
        return;
    }
    if ((now[0] != '\0') && (read->text[0] != '\0') && (text_instructions(read->text) > text_instructions(now)))
    {
        why_set(why, why_room, "the compiler writes ", "");
        forms_add_decimal(why, why_room, text_instructions(read->text));
        forms_add(why, why_room, " instructions where the ruleset writes ");
        forms_add_decimal(why, why_room, text_instructions(now));
        return;
    }
    if (read->text[0] == '\0')
    {
        why_set(why, why_room, "the compiler writes no instruction for it", "");
        return;
    }
    if (read->why[0] != '\0')
    {
        why_set(why, why_room, read->why, "");
        return;
    }
    if (found == NULL)
    {
        why_set(why, why_room, "the ruleset contains no form of the name", "");
        return;
    }
    const size_t read_size = forms_length(read->text);
    for (unsigned int at = 0u; at < found->parameter_count; at += 1u)
    {
        const char *const parameter = krs_parameter(rules, found, at);
        // the parameter itself, and not only its .hi, which names the other half
        FormsWord low;
        FORMS_SET(low, "{");
        FORMS_ADD(low, parameter);
        FORMS_ADD(low, "}");
        const size_t low_size = forms_length(low.text);
        size_t place = forms_find_n(read->text, read_size, low.text, 0u);
        while ((place < read_size) && forms_begins(&read->text[place + low_size], ".hi"))
        {
            place = forms_find_n(read->text, read_size, low.text, place + 1u);
        }
        if (place == read_size)
        {
            why_set(why, why_room, "the reading does not name ", parameter);
            return;
        }
        // a half the ruleset names and the reading does not is a half the reading does not carry
        FormsWord high = low;
        FORMS_ADD(high, ".hi");
        if (forms_has(now, high.text) && !forms_has(read->text, high.text))
        {
            why_set(why, why_room, "the reading does not name ", high.text);
            return;
        }
    }
    forms_set_n(text, room, read->text, read_size);
    for (size_t open = forms_chr_n(text, forms_length(text), '<', 0u); open < forms_length(text);
         open = forms_chr_n(text, forms_length(text), '<', open))
    {
        const size_t length = forms_length(text);
        const size_t close = forms_chr_n(text, length, '>', open);
        FormsWord fixed;
        FORMS_SET_N(fixed, &text[open + 1u], (close > open) ? (close - open - 1u) : 0u);
        const KrsEntry *const named = krs_find(rules, KRS_FIXED, fixed.text);
        if (named == NULL)
        {
            text[0] = '\0';
            why_set(why, why_room, "the ruleset names no fixed register ", fixed.text);
            return;
        }
        static char s_rest[FORMS_READ_BYTES];
        forms_set_n(s_rest, sizeof(s_rest), &text[close + 1u], (close < length) ? (length - close - 1u) : 0u);
        text[open] = '\0';
        forms_add(text, room, krs_text(rules, named));
        forms_add(text, room, s_rest);
    }
    // a register of the compiler's own left in the text: one the reading did not name. The walk jumps from one R or P
    // to the next
    const size_t length = forms_length(text);
    for (size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = length, .other = "RP");
         sass && (at < length); at += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at + 1u],
                                                      .cap = length - at - 1u, .other = "RP"))
    {
        const int start = (at == 0u) || !forms_identifier(text[at - 1u]);
        const char next = ((at + 1u) < length) ? text[at + 1u] : '\0';
        if (!start || (next < '0') || (next > '9'))
        {
            continue;
        }
        if (text[at] == 'R')
        {
            const unsigned int number = token_number(&text[at]);
            if (number < 236u)
            {
                text[0] = '\0';
                why_set(why, why_room, "the reading leaves the compiler's register R", "");
                forms_add_decimal(why, why_room, number);
                return;
            }
        }
        if ((text[at] == 'P') && (next <= '5'))
        {
            FormsWord predicate;
            FORMS_SET_N(predicate, &text[at], 2u);
            text[0] = '\0';
            why_set(why, why_room, "the reading leaves the compiler's predicate ", predicate.text);
            return;
        }
    }
    if (sass && !sass_assembles(text, question))
    {
        text[0] = '\0';
        why_set(why, why_room, "the machine file contains no form for it", "");
    }
}

// a form's text written back as a ruleset line contains it, onto `line`: \t, \n and \\ escaped. The walk jumps from
// one byte to escape to the next
static void krs_escaped(VerbaLinea *line, const char *text)
{
    const size_t size = forms_length(text);
    size_t at = 0u;
    while (at < size)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = size - at, .other = "\t\n\\");
        (void)mmgr_line_put_n(line, &text[at], run);
        at += run;
        if (at < size)
        {
            mmgr_line_put(line, (text[at] == '\t') ? "\\t" : ((text[at] == '\n') ? "\\n" : "\\\\"));
            at += 1u;
        }
    }
}

// the forms of a read at max
#define FORMS_FORMS_MAX 512u

// each form's readings: the text every question gave where all gave one whole, else empty with the reason. `seen` is 1
// once a question or a fold settles it
struct Verdict
{
    char text[FORMS_READ_BYTES];
    char why[FORMS_WHY_BYTES];
    int seen;
};

// A form of the read: its verdicts off the SASS, off the PTX, and off the SASS with the numbers loaded. Then the fold
// that the system does of it, named once for its classification. Then the folds its questions tested off each, with
// the reason. Then whether each ruleset adopts its reading
struct FormState
{
    FormsWord form;
    Verdict sass;
    Verdict ptx;
    Verdict loaded;
    FormsLine fold_fact;
    char sass_fold[FORMS_WHY_BYTES];
    char ptx_fold[FORMS_WHY_BYTES];
    int has_sass_fold;
    int has_ptx_fold;
    int sass_adopted;
    int ptx_adopted;
};

static FormState s_forms[FORMS_FORMS_MAX];
static unsigned int s_form_count = 0u;

// the state of the form `form`, put after the forms where they hold none
static FormState *form_state(const char *form)
{
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        if (forms_same(s_forms[at].form.text, form))
        {
            return &s_forms[at];
        }
    }
    if (s_form_count == FORMS_FORMS_MAX)
    {
        forms_refused("a read reads more forms than the table holds: ", form, forms_length(form));
    }
    FormState *const state = &s_forms[s_form_count];
    s_form_count += 1u;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = state, .bytes = sizeof(*state), .val = 0);
    FORMS_SET(state->form, form);
    return state;
}

static int form_state_order(const void *left, const void *right)
{
    return forms_order(((const FormState *)left)->form.text, ((const FormState *)right)->form.text);
}

// the verdict `verdict` settled by one more question's `text` and `why`
static void settle(Verdict *verdict, const char *text, const char *why)
{
    if (verdict->seen == 0)
    {
        forms_set_n(verdict->text, sizeof(verdict->text), text, forms_length(text));
        forms_set_n(verdict->why, sizeof(verdict->why), why, forms_length(why));
        verdict->seen = 1;
        return;
    }
    if ((verdict->text[0] != '\0') && !forms_same(text, verdict->text))
    {
        const char *const reason = (text[0] == '\0') ? why : "the questions read apart";
        forms_set_n(verdict->why, sizeof(verdict->why), reason, forms_length(reason));
        verdict->text[0] = '\0';
    }
}

// A reading of a question off the SASS or the PTX: the form, the question, and the text it adopts, empty where it
// adopts none. The readings go in one cell at the count of the questions. A reading settles its form where it is a
// question of the SASS that loads no number and the system folds nothing of. A question of the SASS that loads no
// number constrains its form, folded or not
struct FormReading
{
    FormState *state;
    const Question *question;
    int settles;
    int constrains;
    char text[FORMS_READ_BYTES];
};

// 1 where `text` assembles for the banks of every question that constrains the form of `state`
static int assembles_everywhere(const FormReading *readings, unsigned int count, const FormState *state,
                                const char *text)
{
    int everywhere = 1;
    for (unsigned int at = 0u; everywhere && (at < count); at += 1u)
    {
        if ((readings[at].state == state) && readings[at].constrains)
        {
            everywhere = sass_assembles(text, readings[at].question);
        }
    }
    return everywhere;
}

// The ruleset at `path` with the text of each form that the forms adopt written in place. The text is off the SASS
// where `sass` is 1, and off the PTX where it is 0. The ruleset goes to the run's writer, which writes it in its place
// as the run ends.
// The run holds the ruleset as the tool read it, and the writer writes that copy back over a write to the disk. The
// answer is the count written, or -1 where the read of the file failed or the run did not take it
static int krs_apply(const char *path, int sass)
{
    // the ruleset as the run stores it, or the disk where the run stores none (qry_member_file)
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return -1;
    }
    // a ruleset read whole is at max what a size_t counts
    const char *const whole = (const char *)bytes;
    const size_t length = (size_t)size;
    int written = 0;
    PlatformText out;
    forms_text_begun(&out);
    PlatformText replaced;
    forms_text_begun(&replaced);
    for (size_t at = 0u; at < length;)
    {
        const size_t end = forms_chr_n(whole, length, '\n', at);
        const char *const line = &whole[at];
        const size_t line_length = end - at;
        const size_t equals = forms_find_n(line, line_length, " = ", 0u);
        const FormState *adopted = NULL;
        if ((line_length >= 5u) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line, .other = "form ", .bytes = 5u) == 0) &&
            (equals < line_length))
        {
            // the form's name, the second word of the head
            size_t word = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line, .cap = equals, .other = " ");
            word +=
                EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[word], .cap = equals - word, .other = " ");
            word += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[word], .cap = equals - word, .other = " ");
            const size_t name =
                EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[word], .cap = equals - word, .other = " ");
            for (unsigned int one = 0u; (name != 0u) && (one < s_form_count); one += 1u)
            {
                const FormState *const state = &s_forms[one];
                adopted = (forms_same_n(&line[word], name, state->form.text) &&
                           (sass ? state->sass_adopted : state->ptx_adopted))
                              ? state
                              : adopted;
            }
        }
        if (adopted != NULL)
        {
            platform_text_reset(&replaced);
            (void)mmgr_line_put_n(&replaced.line, line, equals + 3u);
            krs_escaped(&replaced.line, sass ? adopted->sass.text : adopted->ptx.text);
            size_t replaced_size = 0u;
            const char *const replaced_bytes = forms_text_bytes(&replaced, &replaced_size);
            written += ((replaced_size != line_length) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = replaced_bytes,
                                                                      .other = line, .bytes = line_length) != 0))
                           ? 1
                           : 0;
            (void)mmgr_line_put_n(&out.line, replaced_bytes, replaced_size);
        }
        else
        {
            (void)mmgr_line_put_n(&out.line, line, line_length);
        }
        mmgr_line_put(&out.line, (end < length) ? "\n" : "");
        at = end + 1u;
    }
    size_t out_size = 0u;
    const char *const out_bytes = forms_text_bytes(&out, &out_size);
    const int failed = (qry_member_filed(path, out_bytes, out_size) == 0ULL);
    platform_text_ended(&replaced);
    platform_text_ended(&out);
    return failed ? -1 : written;
}

// the state of a verdict for the table, `retained: <why>`, `same` or `read`, into `state`, and the count of the same
static void verdict_state(const Verdict *verdict, const char *now, unsigned int *same, FormsLine *state)
{
    if (verdict->text[0] == '\0')
    {
        FORMS_SET(*state, "retained: ");
        FORMS_ADD(*state, verdict->why);
        return;
    }
    if (forms_same(verdict->text, now))
    {
        *same += 1u;
        FORMS_SET(*state, "same");
        return;
    }
    FORMS_SET(*state, "read");
}

// The instructions of the SASS blocks read for the bases, a block at a time in the order of the tags. The read stops
// once it has both bases
static void sass_bases_read(const Blocks *blocks, Bases *bases)
{
    static const Question s_nothing = {};
    static const char *s_lines[FORMS_BLOCK_MAX];
    static Read s_read;
    for (unsigned int at = 0u; (at < blocks->count) && ((bases->in_low < 0) || (bases->out_low < 0));)
    {
        unsigned int count = 0u;
        const unsigned int first = blocks_of(blocks, blocks->lines[at].tag, &count);
        if (count > FORMS_BLOCK_MAX)
        {
            forms_refused("a block holds more lines than the table holds", "", 0u);
        }
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            s_lines[one] = blocks->lines[first + one].text.text;
        }
        sass_read(&s_nothing, s_lines, count, bases, &s_read);
        at = first + count;
    }
}

// a line of the record, `text` with no more
static void record_put(PlatformFile *out, const char *text)
{
    mmgr_line_put(platform_file_line(out), text);
}

static int forms_read(const char *questions_path, const char *listing, const char *ptx, const char *sass_krs,
                      const char *ptx_krs, const char *machine, const char *record, int apply)
{
    unsigned int question_count = 0u;
    const Question *const questions = questions_read(questions_path, &question_count);
    s_machine_read = sass_machine_read(&s_machine, machine);
    static Krs s_sass;
    static Krs s_ptx_rules;
    if ((question_count == 0u) || !krs_read(sass_krs, &s_sass) || !krs_read(ptx_krs, &s_ptx_rules))
    {
        forms_noted("the questions, sass.krs or ptx.krs did not read", "", "", "", "");
        return 2;
    }
    const Blocks sass_lines = sass_blocks(listing);
    const Blocks ptx_lines = ptx_blocks(ptx);
    PlatformFile *const out = platform_file_open(record, "wb");
    if (out == NULL)
    {
        forms_noted("the record ", record, " was not written", "", "");
        return 2;
    }
    Bases bases = {-1, -1};
    static PtxBases s_ptx_bases;
    static const Question s_nothing = {};
    static const char *s_lines[3u * FORMS_BLOCK_MAX];
    static Read s_read;
    // the code sets the bases in whichever block the compiler opens the entry into
    sass_bases_read(&sass_lines, &bases);
    unsigned int opening_count = 0u;
    const unsigned int opening = blocks_of(&ptx_lines, 0u, &opening_count);
    if (opening_count > (3u * FORMS_BLOCK_MAX))
    {
        forms_refused("the PTX's opening holds more lines than the table holds", "", 0u);
    }
    for (unsigned int at = 0u; at < opening_count; at += 1u)
    {
        s_lines[at] = ptx_lines.lines[opening + at].text.text;
    }
    ptx_read(&s_nothing, s_lines, opening_count, &s_ptx_bases, &s_read);
    record_put(out, "# The forms read off NVIDIA's compiler\n\n");
    record_put(out, "Written by `monolith_forms.sh` whole on every run. Every form the lanes of the record programs "
                    "decide is submitted to NVIDIA's compiler once for each set of banks its arguments come from, in "
                    "one program between tags (`monolith_forms.cpp`). A question in the form's text in `c.krs` is "
                    "read off the PTX and compared with the form `ptx.krs` gives; one in its text in `ptx.krs`, put "
                    "to `ptxas` as inline PTX, is read off the listing and compared with the form `sass.krs` gives. "
                    "Each block is read back into the form it is, its registers named by the arguments they contain. "
                    "A fixed register is named in angle brackets, and the ruleset names it. A form every question of "
                    "which reads whole and alike is the ruleset's form, written into it by `monolith_forms.sh "
                    "apply`; any other retains the ruleset's text, and the reason is beside it.\n\n");
    record_put(out, "| tag | form | banks | read off | read | note |\n|---|---|---|---|---|---|\n");
    PlatformText flat;
    forms_text_begun(&flat);
    // the readings off the SASS of every question, in one cell at the count of the questions
    FormReading *const readings = (FormReading *)forms_allocated(platform_cell_cleared(
        question_count, sizeof(FormReading), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
    unsigned int reading_count = 0u;
    // The tool reads a question submitted in the text of ptx.krs for sass.krs off the listing. It reads one submitted
    // in the text of c.krs for ptx.krs off the PTX. The questions that load their numbers settle a form of their own
    // with the questions that contain none. That form applies where the questions that write their numbers in do not
    // settle one
    static char s_text[FORMS_READ_BYTES];
    static char s_why[FORMS_WHY_BYTES];
    for (unsigned int at = 0u; at < question_count; at += 1u)
    {
        const Question *const question = &questions[at];
        const int from_ptx = (question->tag >= MONOLITH_FORMS_PTX);
        const unsigned int count =
            regions_joined(from_ptx ? &sass_lines : &ptx_lines, question->tag, s_lines, 3u * FORMS_BLOCK_MAX);
        if (from_ptx)
        {
            sass_read(question, s_lines, count, &bases, &s_read);
        }
        else
        {
            ptx_read(question, s_lines, count, &s_ptx_bases, &s_read);
        }
        read_adopted(&s_read, from_ptx ? &s_sass : &s_ptx_rules, question, from_ptx, s_text, sizeof(s_text), s_why,
                     sizeof(s_why));
        int loaded = 0;
        int numbered = 0;
        // A probe classifies an operator only where what it varies shows in the answer. The system folded a numbered
        // probe whose number the answer does not carry. It answered as it would for a value of its own, and gave the
        // classifier nothing of the form. The system folds the same way a register that the probe puts and the answer
        // names nowhere. It keeps the value without an instruction of its own. A gate holds either back, and a probe
        // that the system cannot fold decides instead.
        //
        // What a system folds is its own, written to its .ksc, and nothing here names one.
        FormsLine folded;
        folded.text[0] = '\0';
        for (unsigned int one = 0u; one < question->role_count; one += 1u)
        {
            const Role *const role = &question->roles[one];
            loaded = loaded || role->opaque;
            numbered = numbered || forms_same(role->place.text, "number");
            if (forms_same(role->place.text, "number") && !number_carried(s_read.text, role))
            {
                FORMS_SET(folded, "the system folds the number put for ");
                FORMS_ADD(folded, role->name.text);
            }
        }
        for (unsigned int one = 0u; one < question->role_count; one += 1u)
        {
            const Role *const role = &question->roles[one];
            const int put = !role->fixed && (forms_same(role->place.text, "in") || forms_same(role->place.text, "out"));
            FormsWord braced;
            FORMS_SET(braced, "{");
            FORMS_ADD(braced, role->name.text);
            FORMS_ADD(braced, "}");
            if ((folded.text[0] == '\0') && put && !forms_has(s_read.text, braced.text))
            {
                FORMS_SET(folded, "the system folds the register put for ");
                FORMS_ADD(folded, role->name.text);
            }
        }
        FormState *const state = form_state(question->form.text);
        if (!loaded && (folded.text[0] != '\0'))
        {
            FORMS_SET(state->fold_fact, folded.text);
            static char s_folded_why[FORMS_WHY_BYTES];
            forms_set_n(s_folded_why, sizeof(s_folded_why), folded.text, forms_length(folded.text));
            if (s_why[0] != '\0')
            {
                forms_add(s_folded_why, sizeof(s_folded_why), "; ");
                forms_add(s_folded_why, sizeof(s_folded_why), s_why);
            }
            forms_set_n(s_why, sizeof(s_why), s_folded_why, forms_length(s_folded_why));
            char *const fold = from_ptx ? state->sass_fold : state->ptx_fold;
            forms_set_n(fold, FORMS_WHY_BYTES, s_why, forms_length(s_why));
            *(from_ptx ? &state->has_sass_fold : &state->has_ptx_fold) = 1;
        }
        FormReading *const reading = &readings[reading_count];
        reading_count += 1u;
        reading->state = state;
        reading->question = question;
        reading->settles = 0;
        reading->constrains = 0;
        forms_set_n(reading->text, sizeof(reading->text), s_text, forms_length(s_text));
        if (!loaded && (folded.text[0] == '\0'))
        {
            settle(from_ptx ? &state->sass : &state->ptx, s_text, s_why);
            reading->settles = from_ptx;
        }
        reading->constrains = from_ptx && !loaded;
        if (from_ptx && (loaded || !numbered))
        {
            settle(&state->loaded, s_text, s_why);
        }
        const size_t key_size = forms_length(question->key.text);
        const size_t space = forms_chr_n(question->key.text, key_size, ' ', 0u);
        PlatformFile *const file = out;
        VerbaLinea *const line = platform_file_line(file);
        mmgr_line_put(line, "| ");
        mmgr_line_decimal(line, question->tag, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, question->form.text);
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, (space < key_size) ? &question->key.text[space + 1u] : "");
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, !from_ptx ? "PTX" : (loaded ? "SASS, numbers loaded" : "SASS"));
        mmgr_line_put(line, " | `");
        flat_cell_put(line, s_read.text, &flat);
        mmgr_line_put(line, "` | ");
        cell_put(line, s_why, forms_length(s_why));
        mmgr_line_put(line, " |\n");
    }
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (state->has_sass_fold && !state->sass.seen)
        {
            state->sass.text[0] = '\0';
            forms_set_n(state->sass.why, sizeof(state->sass.why), state->sass_fold, forms_length(state->sass_fold));
            state->sass.seen = 1;
        }
        if (state->has_ptx_fold && !state->ptx.seen)
        {
            state->ptx.text[0] = '\0';
            forms_set_n(state->ptx.why, sizeof(state->ptx.why), state->ptx_fold, forms_length(state->ptx_fold));
            state->ptx.seen = 1;
        }
    }
    // Where the written-in questions read apart, each whole and the two numbers of one set of banks alike, the banks
    // chose the instruction. The compiler adds a register as IMAD.IADD and a number as IADD3. A reading that assembles
    // for the banks of every question does what each requested, and is the form. It is the ruleset's own form where it
    // is one, else the shortest. Two numbers of one set of banks that read apart are a folded number, and no reading of
    // theirs is the form
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (!state->sass.seen)
        {
            continue;
        }
        int any = 0;
        int whole = 1;
        for (unsigned int one = 0u; one < reading_count; one += 1u)
        {
            if ((readings[one].state != state) || !readings[one].settles)
            {
                continue;
            }
            any = 1;
            whole = whole && (readings[one].text[0] != '\0');
            // the reading of the same banks before it, which this one must read alike
            for (unsigned int before = one; whole && (before > 0u); before -= 1u)
            {
                const FormReading *const earlier = &readings[before - 1u];
                if ((earlier->state == state) && earlier->settles &&
                    forms_same(earlier->question->key.text, readings[one].question->key.text))
                {
                    whole = forms_same(earlier->text, readings[one].text);
                    break;
                }
            }
        }
        whole = whole && any;
        if ((state->sass.text[0] != '\0') || !whole)
        {
            continue;
        }
        const char *const now = krs_text_of(&s_sass, KRS_FORM, state->form.text);
        const char *chosen = "";
        for (unsigned int one = 0u; one < reading_count; one += 1u)
        {
            if ((readings[one].state != state) || !readings[one].settles)
            {
                continue;
            }
            const char *const text = readings[one].text;
            if (assembles_everywhere(readings, reading_count, state, text) &&
                ((chosen[0] == '\0') || forms_same(text, now) ||
                 (!forms_same(chosen, now) && (text_instructions(text) < text_instructions(chosen)))))
            {
                chosen = text;
            }
        }
        if (chosen[0] != '\0')
        {
            forms_set_n(state->sass.text, sizeof(state->sass.text), chosen, forms_length(chosen));
            state->sass.why[0] = '\0';
        }
    }
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (!state->sass.seen || (state->sass.text[0] != '\0') || !state->loaded.seen ||
            (state->loaded.text[0] == '\0'))
        {
            continue;
        }
        // the loaded reading places the numbers of the form in registers. It is the form only where it also assembles
        // for every written-in number that the tests of the form used. Else it is a form only for operands that the
        // written-in questions never used
        if (assembles_everywhere(readings, reading_count, state, state->loaded.text))
        {
            state->sass = state->loaded;
        }
    }
    // A verdict holds only where the reading assembles for every operand that the tests of the form used, folded probes
    // included. A reading settled from register operands alone is no form of the operator in one case. A test put a
    // literal in a position that the reading cannot encode. That holds however every register question agreed on it
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (!state->sass.seen || (state->sass.text[0] == '\0'))
        {
            continue;
        }
        for (unsigned int one = 0u; one < reading_count; one += 1u)
        {
            if ((readings[one].state != state) || !readings[one].constrains ||
                sass_assembles(state->sass.text, readings[one].question))
            {
                continue;
            }
            const char *const why = state->has_sass_fold
                                        ? state->sass_fold
                                        : "the reading does not assemble for every operand the form was tested with";
            forms_set_n(state->sass.why, sizeof(state->sass.why), why, forms_length(why));
            state->sass.text[0] = '\0';
            break;
        }
    }
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (state->ptx.seen && !state->sass.seen)
        {
            state->sass.text[0] = '\0';
            forms_set_n(state->sass.why, sizeof(state->sass.why), "ptx.krs gives no form", 21u);
            state->sass.seen = 1;
        }
    }
    platform_cell_released(readings, platform_place_g_program_owner);
    // the forms in the order of their names, for the table and the folds, each once
    if (s_form_count >= 2u)
    {
        void *const spare =
            forms_allocated(platform_cell_taken((unsigned long long)s_form_count * sizeof(FormState),
                                                platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = s_forms, .bytes = sizeof(FormState), .scratch = spare,
                         .count = s_form_count, .order = form_state_order);
        platform_cell_released(spare, platform_place_g_program_owner);
    }
    record_put(out, "\n## By form\n\n| form | sass.krs | SASS read | | ptx.krs | PTX read | "
                    "|\n|---|---|---|---|---|---|---|\n");
    unsigned int sass_same = 0u;
    unsigned int ptx_same = 0u;
    unsigned int sass_forms = 0u;
    unsigned int sass_adopted = 0u;
    unsigned int ptx_adopted = 0u;
    static FormsAlike s_alike[FORMS_ALIKE_MAX];
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        FormState *const state = &s_forms[at];
        if (!state->sass.seen)
        {
            continue;
        }
        sass_forms += 1u;
        const char *const sass_now = krs_text_of(&s_sass, KRS_FORM, state->form.text);
        const char *const ptx_now = krs_text_of(&s_ptx_rules, KRS_FORM, state->form.text);
        FormsLine sass_state;
        verdict_state(&state->sass, sass_now, &sass_same, &sass_state);
        FormsLine ptx_state;
        verdict_state(&state->ptx, ptx_now, &ptx_same, &ptx_state);
        // A PTX reading that differs from the ruleset only in the signedness of an operation is the form of the
        // ruleset. That holds where the system compiles both to the same machine code. The SASS read whole and alike is
        // that proof. Record the writings that the system answers alike, and read the form as given.
        if (forms_same(ptx_state.text, "read") && forms_same(sass_state.text, "same"))
        {
            unsigned int alike = 0u;
            if (signedness_only(ptx_now, state->ptx.text, s_alike, &alike))
            {
                for (unsigned int one = 0u; one < alike; one += 1u)
                {
                    sass_equal_take(s_alike[one].now.text, s_alike[one].read.text);
                }
                ptx_same += 1u;
                FORMS_SET(ptx_state, "same");
            }
        }
        state->sass_adopted = forms_same(sass_state.text, "read");
        state->ptx_adopted = forms_same(ptx_state.text, "read");
        sass_adopted += state->sass_adopted ? 1u : 0u;
        ptx_adopted += state->ptx_adopted ? 1u : 0u;
        VerbaLinea *const line = platform_file_line(out);
        mmgr_line_put(line, "| ");
        mmgr_line_put(line, state->form.text);
        mmgr_line_put(line, " | `");
        flat_cell_put(line, sass_now, &flat);
        mmgr_line_put(line, "` | `");
        flat_cell_put(line, state->sass.text, &flat);
        mmgr_line_put(line, "` | ");
        cell_put(line, sass_state.text, forms_length(sass_state.text));
        mmgr_line_put(line, " | `");
        flat_cell_put(line, ptx_now, &flat);
        mmgr_line_put(line, "` | `");
        flat_cell_put(line, state->ptx.text, &flat);
        mmgr_line_put(line, "` | ");
        cell_put(line, ptx_state.text, forms_length(ptx_state.text));
        mmgr_line_put(line, " |\n");
    }
    platform_text_ended(&flat);
    VerbaLinea *const summary = platform_file_line(out);
    mmgr_line_put(summary, "\n");
    mmgr_line_decimal(summary, question_count, 1u);
    mmgr_line_put(summary, " questions over ");
    mmgr_line_decimal(summary, sass_forms, 1u);
    mmgr_line_put(summary, " forms. Of the forms, sass.krs already gives ");
    mmgr_line_decimal(summary, sass_same, 1u);
    mmgr_line_put(summary, " as read and ");
    mmgr_line_decimal(summary, sass_adopted, 1u);
    mmgr_line_put(summary, " are read otherwise; ptx.krs gives ");
    mmgr_line_decimal(summary, ptx_same, 1u);
    mmgr_line_put(summary, " as read and ");
    mmgr_line_decimal(summary, ptx_adopted, 1u);
    mmgr_line_put(summary, " are read otherwise.\n");
    platform_file_close(out);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "monolith_forms: ");
    mmgr_line_decimal(output_line, question_count, 1u);
    mmgr_line_put(output_line, " questions over ");
    mmgr_line_decimal(output_line, sass_forms, 1u);
    mmgr_line_put(output_line, " forms; sass.krs: ");
    mmgr_line_decimal(output_line, sass_same, 1u);
    mmgr_line_put(output_line, " same, ");
    mmgr_line_decimal(output_line, sass_adopted, 1u);
    mmgr_line_put(output_line, " read otherwise; ptx.krs: ");
    mmgr_line_decimal(output_line, ptx_same, 1u);
    mmgr_line_put(output_line, " same, ");
    mmgr_line_decimal(output_line, ptx_adopted, 1u);
    mmgr_line_put(output_line, " read otherwise; the record written to ");
    mmgr_line_put(output_line, record);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    // The folds that the system does are its own, and belong in the classification of what the generator emits. That is
    // the .ksc beside sass.krs that the code generator reads with it. Read it, and drop what it contains of each number
    // and register that this pass put for a role of a form that it tested. Add the compile-channel folds of this pass
    // in place, and write it back. Every other fold stays
    const size_t path_length = forms_length(sass_krs);
    const size_t slash = forms_chr_last(sass_krs, path_length, '/');
    FormsLine rulesets;
    FORMS_SET_N(rulesets, (slash == path_length) ? "." : sass_krs, (slash == path_length) ? 1u : slash);
    const char *const file = (slash == path_length) ? sass_krs : &sass_krs[slash + 1u];
    const size_t file_length = forms_length(file);
    FormsLine stem;
    FORMS_SET_N(stem, file, forms_chr_last(file, file_length, '.'));
    sass_class_read(rulesets.text, stem.text);
    for (unsigned int at = 0u; at < question_count; at += 1u)
    {
        const Question *const question = &questions[at];
        for (unsigned int one = 0u; one < question->role_count; one += 1u)
        {
            const Role *const role = &question->roles[one];
            const int put = !role->fixed && (forms_same(role->place.text, "in") || forms_same(role->place.text, "out"));
            FormsLine dropped;
            if (forms_same(role->place.text, "number"))
            {
                FORMS_SET(dropped, question->form.text);
                FORMS_ADD(dropped, ": the system folds the number put for ");
                FORMS_ADD(dropped, role->name.text);
                sass_class_drop(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, dropped.text);
            }
            if (put)
            {
                FORMS_SET(dropped, question->form.text);
                FORMS_ADD(dropped, ": the system folds the register put for ");
                FORMS_ADD(dropped, role->name.text);
                sass_class_drop(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, dropped.text);
            }
        }
    }
    for (unsigned int at = 0u; at < s_form_count; at += 1u)
    {
        const FormState *const state = &s_forms[at];
        if (state->fold_fact.text[0] == '\0')
        {
            continue;
        }
        FormsLine taken;
        FORMS_SET(taken, state->form.text);
        FORMS_ADD(taken, ": ");
        FORMS_ADD(taken, state->fold_fact.text);
        sass_class_take(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, taken.text, 0u);
    }
    sass_class_write(rulesets.text, stem.text);
    if (apply)
    {
        const int sass_written = krs_apply(sass_krs, 1);
        const int ptx_written = krs_apply(ptx_krs, 0);
        VerbaLinea *const applied = platform_line_out();
        mmgr_line_put(applied, "monolith_forms: ");
        mmgr_line_decimal_signed(applied, sass_written);
        mmgr_line_put(applied, " forms written into ");
        mmgr_line_put(applied, sass_krs);
        mmgr_line_put(applied, " and ");
        mmgr_line_decimal_signed(applied, ptx_written);
        mmgr_line_put(applied, " into ");
        mmgr_line_put(applied, ptx_krs);
        mmgr_line_ch(applied, '\n');
        platform_line_out_ended();
        if ((sass_written < 0) || (ptx_written < 0))
        {
            return 2;
        }
    }
    return 0;
}

int program_main(int count, char **words)
{
    if ((count == 6) && forms_same(words[1], "write"))
    {
        return forms_write(words[2], words[3], words[4], words[5]);
    }
    if (((count == 9) || (count == 10)) && forms_same(words[1], "read"))
    {
        return forms_read(words[2], words[3], words[4], words[5], words[6], words[7], words[8],
                          (count == 10) && forms_same(words[9], "apply"));
    }
    forms_noted("monolith_forms write <c.krs> <ptx.krs> <monolith.cu> <questions>\nmonolith_forms read "
                "<questions> <listing> <ptx> <sass.krs> <ptx.krs> <machine> <record> [apply]",
                "", "", "", "");
    return 2;
}
