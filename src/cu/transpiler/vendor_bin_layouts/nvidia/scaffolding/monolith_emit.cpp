// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_emit.cpp: the monolith (monolith.cu), a block at a time, checked against what our compiler writes.
//
// NVIDIA's listing of the tagged build is the answer key: the instructions between tag n and tag n + 1 are its writing
// of precept n - 1, each with the 128 bits it encoded. For every one, our reader is queried for its text from those
// bits, and our assembler for its bits from that text. The scheduler's bits, 105 to 127, are read apart, since no
// listing prints them. Then the word our compiler writes the block's precept with (word_web.h) is written through
// sass.krs with NVIDIA's own registers, assembled, and set beside NVIDIA's instruction. A precept no word implements is
// reported with the tree the alphabet web writes it as (precepts.h). sass.krs, the machine file and the word web are
// read as they are.
//
//     monolith_emit <listing> <block> [machine file]
//     monolith_emit <listing> all <record> [machine file]
//
// The second form runs every block and writes the record, a table a row an instruction and a row our compiler's
// writing, to <record>. It is written whole on every run: it contains what the blocks read now.
//
// The listing is cuobjdump -sass -fun monolith of the build with MONOLITH_TAGGED 1.
extern "C"
{
#include "../../../../engine/engine_note.h"
#include "../sass_assemble.h"
#include "../sass_machine.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
}

#include "../../../../../platform/platform.h"
#include "../../../../engine/rmc/word_web.h"
#include "sass_target.h"
#include "target.h"

#include <stddef.h>

// ENGINE_IMAGE_BASE reads the ELF header the linker marks with __ehdr_start (engine_config_platform.h), and this
// program is linked as a PE, where there is none. Nothing here requests for the image base: it is named so the link
// resolves
#if defined(__GNUC__) && !defined(__ELF__)
extern "C" const char __ehdr_start = 0;
#endif

// the operation's own bits of an encoding's high word: below bit 105 of the 128, where the scheduler's begin
#define EMIT_OPERATION_HIGH ((1ULL << 41u) - 1ULL)

#define EMIT_NAMED(name_, text_, arity_) text_,

static const char *const s_names[PRECEPT_COUNT] = {PRECEPTS(EMIT_NAMED)};

// the bytes of one instruction's text, of a register's or a field's name, and of a text the emitter builds, each with
// its terminator; and the bytes of the record's texts at max
#define EMIT_LINE_BYTES 256u
#define EMIT_NAME_BYTES 64u
#define EMIT_TEXT_BYTES 1024u
#define EMIT_RECORD_BYTES (1ULL << 28u)

// the instructions of one block, the operands of one instruction, the fields the part does not read, and the
// questions of its answers, each at max
#define EMIT_PASSAGE_MAX 1024u
#define EMIT_OPERANDS_MAX 16u
#define EMIT_UNREAD_MAX 4096u
#define EMIT_QUESTIONS_MAX 1024u

// a text the emitter builds, terminated
struct EmitText
{
    char text[EMIT_TEXT_BYTES];
};

// a register's name, a modifier or an address, terminated
struct EmitName
{
    char text[EMIT_NAME_BYTES];
};

// one instruction of NVIDIA's listing: where it lies, its text, and its two words
struct EmitLine
{
    unsigned long long address;
    char text[EMIT_LINE_BYTES];
    unsigned long long low;
    unsigned long long high;
};

// a line over the terminated text in `text` of `room` bytes, which goes on after what the text holds
static VerbaLinea emit_after(char *text, size_t room)
{
    VerbaLinea line = mmgr_line_over(text, room);
    line.at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = room);
    return line;
}

// `from` put after what `to` holds, cut where the room ends
static void emit_add(char *to, size_t room, const char *from)
{
    VerbaLinea line = emit_after(to, room);
    mmgr_line_put(&line, from);
    (void)mmgr_line_finish(&line);
}

// `length` bytes of `from` put after what `to` holds, cut where the room ends
static void emit_add_n(char *to, size_t room, const char *from, size_t length)
{
    VerbaLinea line = emit_after(to, room);
    (void)mmgr_line_put_n(&line, from, length);
    (void)mmgr_line_finish(&line);
}

// `from` as the whole of `to`
static void emit_set(char *to, size_t room, const char *from)
{
    to[0] = '\0';
    emit_add(to, room, from);
}

#define EMIT_ADD(to_, from_) emit_add((to_).text, sizeof((to_).text), from_)
#define EMIT_SET(to_, from_) emit_set((to_).text, sizeof((to_).text), from_)

// 1 where the terminated `text` contains `needle`
static int emit_has(const char *text, const char *needle)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = needle,
                      .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL;
}

// the place of `needle` in the terminated `text` from `from`, or `none` where it is not there
static size_t emit_place(const char *text, const char *needle, size_t from, size_t none)
{
    const char *const found =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[from], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = needle, .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    // a found needle lies at or past its search's start
    return (found != NULL) ? (size_t)(found - text) : none;
}

// 1 where the terminated `text` begins with `prefix`
static int emit_begins(const char *text, const char *prefix)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = prefix, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return EMBED_CALL(memor.cmp, MemoriaCfg, .src = text, .other = prefix, .bytes = length) == 0;
}

static int emit_same(const char *left, const char *right)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = right,
                      .ci = EMBED_FALSE);
}

// The `length` bytes of `text` into `plain` with their whitespace run together, and their closing semicolon and the
// spaces around it gone
static void emit_plain(const char *text, size_t length, char *plain, size_t room)
{
    VerbaLinea line = mmgr_line_over(plain, room);
    int space = 0;
    size_t written = 0u;
    for (size_t at = 0u; (at < length) && (text[at] != '\0'); at += 1u)
    {
        const char character = text[at];
        if ((character == ' ') || (character == '\t') || (character == '\n') || (character == '\r'))
        {
            space = written != 0u;
            continue;
        }
        if (space != 0)
        {
            mmgr_line_ch(&line, ' ');
            written += 1u;
            space = 0;
        }
        mmgr_line_ch(&line, character);
        written += 1u;
    }
    size_t end = mmgr_line_finish(&line);
    while ((end != 0u) && ((plain[end - 1u] == ';') || (plain[end - 1u] == ' ')))
    {
        end -= 1u;
        plain[end] = '\0';
    }
}

// the hexadecimal word in the `length` bytes of `text` after `from`, written /* 0x... */; 0 where there is none
static int emit_word(const char *text, size_t length, size_t from, unsigned long long *word)
{
    for (size_t at = from; (at + 5u) <= length; at += 1u)
    {
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &text[at], .other = "/* 0x", .bytes = 5u) == 0)
        {
            *word = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &text[at + 5u], .end = NULL, .base = 16);
            return 1;
        }
    }
    return 0;
}

// the place of `needle` in the `length` bytes of `text` from `from`, or `length` where it is not there
static size_t emit_find_n(const char *text, size_t length, const char *needle, size_t from)
{
    const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = needle, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = from; (at + size) <= length; at += 1u)
    {
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &text[at], .other = needle, .bytes = size) == 0)
        {
            return at;
        }
    }
    return length;
}

// The file at `path` read whole into a cell of the temporary tier, terminated, and its bytes into `size`. The caller
// gives the cell back before its work ends. NULL where the file does not read
static char *emit_file_read(const char *path, size_t *size)
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

// the end of the line at `at` in the `size` bytes of `text`: its newline, or `size`
static size_t emit_line_end(const char *text, size_t size, size_t at)
{
    while ((at < size) && (text[at] != '\n'))
    {
        at += 1u;
    }
    return at;
}

// NVIDIA's listing read into its instructions, a cell of the run that the caller gives back, `*count` of them. Each is
// an address and text with its low word, and the next line carries its high word
static EmitLine *emit_listing(const char *path, unsigned long long *count)
{
    *count = 0ULL;
    size_t size = 0u;
    char *const file = emit_file_read(path, &size);
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; (file != NULL) && (at < size); at += 1u)
    {
        lines += (file[at] == '\n') ? 1ULL : 0ULL;
    }
    EmitLine *const read =
        (file != NULL) ? (EmitLine *)platform_cell_cleared(lines, sizeof(EmitLine), platform_place_g_program_owner,
                                                           PLATFORM_TIER_PERSISTENT)
                       : NULL;
    for (size_t at = 0u; (read != NULL) && (at < size);)
    {
        const size_t end = emit_line_end(file, size, at);
        const char *const line = &file[at];
        const size_t length = end - at;
        at = end + 1u;
        const size_t open = emit_find_n(line, length, "/*", 0u);
        const size_t close = (open == length) ? length : emit_find_n(line, length, "*/", open);
        if ((close == length) || (line[open + 2u] == ' '))
        {
            continue;
        }
        EmitLine *const one = &read[*count];
        one->address = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &line[open + 2u], .end = NULL, .base = 16);
        const size_t text_end = emit_find_n(line, length, ";", close);
        emit_plain(&line[close + 2u], text_end - close - 2u, one->text, sizeof(one->text));
        if (!emit_word(line, length, close, &one->low))
        {
            continue;
        }
        // the next line carries the high word, and the read goes on past it
        const size_t next_end = (at < size) ? emit_line_end(file, size, at) : size;
        const int high = (at < size) && emit_word(&file[at], next_end - at, 0u, &one->high);
        at = (at < size) ? (next_end + 1u) : at;
        *count += (high != 0) ? 1ULL : 0ULL;
    }
    platform_cell_released(file, platform_place_g_program_owner);
    return read;
}

// the tag a line is, PMTRIG's immediate, or 0 where it is no tag
static unsigned int emit_tag(const EmitLine *line)
{
    return emit_begins(line->text, "PMTRIG ")
               ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = line->text + 7u, .end = NULL,
                                          .base = 16)
               : 0u;
}

// a line's operands split at their commas into `operands`, its operation and guard dropped; their count
static unsigned int emit_operands(const char *text, EmitName *operands)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    unsigned int count = 0u;
    size_t at = (text[0] == '@') ? (emit_place(text, " ", 0u, length) + 1u) : 0u;
    at = (at <= length) ? emit_place(text, " ", at, length) : length;
    while ((at < length) && (count < EMIT_OPERANDS_MAX))
    {
        const size_t comma = emit_place(text, ",", at + 1u, length);
        emit_plain(&text[at + 1u], comma - at - 1u, operands[count].text, sizeof(operands[count].text));
        count += 1u;
        at = comma;
    }
    return count;
}

// 1 where `operand` names a register a word can be written with: R and a number, not RZ
static int emit_register(const char *operand)
{
    return (operand[0] == 'R') && (operand[1] >= '0') && (operand[1] <= '9');
}

// the alphabet web's tree for `precept` written into `tree`: 1, or 0 where it has none
static int emit_alphabet(unsigned int precept, EmitText *tree)
{
    tree->text[0] = '\0';
    for (unsigned int at = 0u; at < PRECEPT_WEB_COUNT; at += 1u)
    {
        if (s_precept_web[at].precept != precept)
        {
            continue;
        }
        for (unsigned int node = 0u; node < s_precept_web[at].nodes; node += 1u)
        {
            const PreceptNode &one = s_precept_web[at].node[node];
            EMIT_ADD(*tree, (node == 0u) ? "" : ", ");
            EMIT_ADD(*tree, s_names[one.precept]);
            EMIT_ADD(*tree, "(");
            const unsigned char child[2] = {one.left, one.right};
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned char leaf = child[side];
                EMIT_ADD(*tree, (side == 0u) ? "" : " ");
                if (leaf < PRECEPT_ARG && leaf != PRECEPT_NONE && leaf != PRECEPT_ZERO && leaf != PRECEPT_ONES)
                {
                    VerbaLinea line = emit_after(tree->text, sizeof(tree->text));
                    mmgr_line_ch(&line, 'n');
                    mmgr_line_decimal(&line, leaf, 1u);
                    (void)mmgr_line_finish(&line);
                }
                else
                {
                    EMIT_ADD(*tree, (leaf == PRECEPT_NONE)   ? "-"
                                    : (leaf == PRECEPT_ZERO) ? "0"
                                    : (leaf == PRECEPT_ONES) ? "ones"
                                    : (leaf == PRECEPT_LEFT) ? "left"
                                                             : "right");
                }
            }
            EMIT_ADD(*tree, ")");
        }
        return 1;
    }
    return 0;
}

// a line on stdout
static void emit_said(const char *text)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, text);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// Whether NVIDIA's block contains the passage: the register its load writes is the register its store reads, and
// nothing past the parameters' moves is between them. The two registers through `loaded` and `stored`, the reading
// printed
static int emit_passage(const EmitLine *passage, unsigned int count, EmitName *loaded, EmitName *stored)
{
    unsigned int between = 0u;
    loaded->text[0] = '\0';
    stored->text[0] = '\0';
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EmitName operands[EMIT_OPERANDS_MAX];
        const unsigned int operand_count = emit_operands(passage[at].text, operands);
        if (emit_has(passage[at].text, "LDG") && (operand_count != 0u))
        {
            *loaded = operands[0];
        }
        else if (emit_has(passage[at].text, "STG") && (operand_count != 0u))
        {
            *stored = operands[operand_count - 1u];
        }
        else if (!emit_has(passage[at].text, "c[0x0]"))
        {
            between += 1u;
        }
    }
    const int carried = (loaded->text[0] != '\0') && emit_same(loaded->text, stored->text) && (between == 0u);
    VerbaLinea *const output_line = platform_line_out();
    if (carried)
    {
        mmgr_line_put(output_line, "    NVIDIA's block: the word loaded into ");
        mmgr_line_put(output_line, loaded->text);
        mmgr_line_put(output_line, " is stored from ");
        mmgr_line_put(output_line, stored->text);
        mmgr_line_put(output_line, " with nothing between, the passage");
    }
    else
    {
        mmgr_line_put(output_line, "    NVIDIA's block: not the passage, loaded into ");
        mmgr_line_put(output_line, (loaded->text[0] == '\0') ? "nothing" : loaded->text);
        mmgr_line_put(output_line, ", stored from ");
        mmgr_line_put(output_line, (stored->text[0] == '\0') ? "nothing" : stored->text);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, between, 1u);
        mmgr_line_put(output_line, " instructions between");
    }
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return carried;
}

// a scratch register for a bank sass.krs requests one of while writing a form; nothing here writes a form that requests
static int emit_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    (void)bank;
    mmgr_line_put(name, "R237");
    return 1;
}

// form `name` written with its `count` arguments after what `written` holds: 1 where sass.krs writes it
static int emit_form(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                     EmitText *written)
{
    const RulesetNamedScratch scratch = {emit_scratch, NULL};
    VerbaLinea line = emit_after(written->text, sizeof(written->text));
    const int wrote = ruleset_form_written(rules, name, arguments, count, &scratch, &line);
    (void)mmgr_line_finish(&line);
    return wrote;
}

// `low` and `high` as 128 bits put after what `bits` holds, bit 127 first, a space between bytes
static void emit_binary(unsigned long long low, unsigned long long high, EmitText *bits)
{
    VerbaLinea line = emit_after(bits->text, sizeof(bits->text));
    for (int bit = 127; bit >= 0; bit -= 1)
    {
        const unsigned long long word = (bit >= 64) ? high : low;
        mmgr_line_ch(&line, ((word >> (unsigned int)(bit % 64)) & 1ULL) ? '1' : '0');
        if (((bit % 8) == 0) && (bit != 0))
        {
            mmgr_line_ch(&line, ' ');
        }
    }
    (void)mmgr_line_finish(&line);
}

// NVIDIA's encoding and ours bit by bit under each byte's top bit number into `bits`, and a line marking each bit
// apart: ^ an operation bit, . a scheduler bit
static void emit_bits_apart(unsigned long long nvidia_low, unsigned long long nvidia_high, unsigned long long low,
                            unsigned long long high, const char *label, EmitText *bits)
{
    EmitText marks;
    EMIT_SET(marks, "         ");
    EMIT_SET(*bits, "         ");
    VerbaLinea numbers = emit_after(bits->text, sizeof(bits->text));
    VerbaLinea marked = emit_after(marks.text, sizeof(marks.text));
    for (int bit = 127; bit >= 0; bit -= 1)
    {
        if ((bit % 8) == 7)
        {
            const size_t before = numbers.at;
            // Explicit cast widens a bit number of 0 to 127 to the unsigned word of the writer
            mmgr_line_decimal(&numbers, (uint64_t)bit, 1u);
            while ((numbers.at - before) < 9u)
            {
                mmgr_line_ch(&numbers, ' ');
            }
        }
        const unsigned long long theirs = (((bit >= 64) ? nvidia_high : nvidia_low) >> (unsigned int)(bit % 64)) & 1ULL;
        const unsigned long long mine = (((bit >= 64) ? high : low) >> (unsigned int)(bit % 64)) & 1ULL;
        mmgr_line_ch(&marked, (theirs == mine) ? ' ' : ((bit >= 105) ? '.' : '^'));
        if (((bit % 8) == 0) && (bit != 0))
        {
            mmgr_line_ch(&marked, ' ');
        }
    }
    (void)mmgr_line_finish(&numbers);
    (void)mmgr_line_finish(&marked);
    EMIT_ADD(*bits, "\nNVIDIA   ");
    emit_binary(nvidia_low, nvidia_high, bits);
    EMIT_ADD(*bits, "\n");
    EMIT_ADD(*bits, label);
    emit_binary(low, high, bits);
    EMIT_ADD(*bits, "\n");
    EMIT_ADD(*bits, marks.text);
    EMIT_ADD(*bits, "\n");
}

// `value` in hex, at least `width` digits, into `name`
static const char *emit_hex(unsigned long long value, unsigned int width, EmitName *name)
{
    VerbaLinea line = mmgr_line_over(name->text, sizeof(name->text));
    mmgr_line_hex(&line, value, width);
    (void)mmgr_line_finish(&line);
    return name->text;
}

// the dotted modifiers of a line's operation into `modifiers`, its guard and the operation's own name dropped; their
// count
static unsigned int emit_modifiers(const char *text, EmitName *modifiers)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t begin = (text[0] == '@') ? (emit_place(text, " ", 0u, length) + 1u) : 0u;
    const size_t end = (begin <= length) ? emit_place(text, " ", begin, length) : length;
    unsigned int count = 0u;
    size_t dot = emit_find_n(text, end, ".", begin);
    while ((dot < end) && (count < EMIT_OPERANDS_MAX))
    {
        const size_t next = emit_find_n(text, end, ".", dot + 1u);
        modifiers[count].text[0] = '\0';
        emit_add_n(modifiers[count].text, sizeof(modifiers[count].text), &text[dot + 1u], next - dot - 1u);
        count += 1u;
        dot = next;
    }
    return count;
}

// the modifiers of `one` that `other` does not carry, joined into `lacking`
static void emit_lacking(const char *one, const char *other, EmitText *lacking)
{
    EmitName theirs[EMIT_OPERANDS_MAX];
    EmitName ours[EMIT_OPERANDS_MAX];
    const unsigned int their_count = emit_modifiers(other, theirs);
    const unsigned int our_count = emit_modifiers(one, ours);
    lacking->text[0] = '\0';
    for (unsigned int at = 0u; at < our_count; at += 1u)
    {
        int found = 0;
        for (unsigned int their = 0u; their < their_count; their += 1u)
        {
            found = emit_same(theirs[their].text, ours[at].text) ? 1 : found;
        }
        if (found == 0)
        {
            EMIT_ADD(*lacking, (lacking->text[0] == '\0') ? "." : " .");
            EMIT_ADD(*lacking, ours[at].text);
        }
    }
}

// the uniform register the kernel loads its memory descriptor into, ULDC.64 URn, c[0x0][0x118]; empty where none
static EmitName s_descriptor;

// the folder of the probe records the emitter reads, from the tree's top
#define EMIT_SCAFFOLDING "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/"

// the part's answers on the bits the disassembler does not print, written by interface_sass_unprinted.sh
#define EMIT_UNREAD_PATH EMIT_SCAFFOLDING "interface_sass_unprinted.md"

// A field of a form the part does not read: every value of it written into a question's instruction answered as that
// instruction's text specifies, with no other bit fixed beside it. A bit apart there is apart in the encoding and not
// in what the part does. The answer is valid for the question it was tested over, and the record names where it was
// read
struct EmitUnread
{
    const SassForm *form;
    unsigned int first;
    unsigned int last;
};

// The fields the part does not read, `s_unread_count` of them. The calls of this module share them, and the run reads
// them once
static EmitUnread s_unread[EMIT_UNREAD_MAX];
static unsigned int s_unread_count;

// a field the part does not read kept, where the room holds it
static void emit_unread_kept(const SassForm *form, unsigned int first, unsigned int last)
{
    if (s_unread_count < EMIT_UNREAD_MAX)
    {
        s_unread[s_unread_count] = {form, first, last};
        s_unread_count += 1u;
    }
}

// a question of the answers: its instruction, its field's first and last bits, and 1 where every value answered as
// printed
struct EmitQuestion
{
    char instruction[EMIT_LINE_BYTES];
    unsigned int first;
    unsigned int last;
    int printed;
};

// the questions of the answers, read once. The calls of this module share them
static EmitQuestion s_questions[EMIT_QUESTIONS_MAX];

// the cells of a row of a table, `cell` each the start of one and `length` its bytes
struct EmitCells
{
    const char *cell[8];
    size_t length[8];
    unsigned int count;
};

// The fields the part does not read, from the answers at `path`, each keyed by the form its question's instruction is
// matched by. A row is `| question | instruction | bits | the form contains | value | answer |`. A question whose bits
// name one fixed beside its field, or one of whose values answered otherwise, gives none
static void emit_unread_read(const char *path, const SassMachine *machine)
{
    size_t size = 0u;
    char *const file = emit_file_read(path, &size);
    unsigned int questions = 0u;
    for (size_t at = 0u; (file != NULL) && (at < size);)
    {
        const size_t end = emit_line_end(file, size, at);
        const char *const line = &file[at];
        const size_t length = end - at;
        at = end + 1u;
        if ((length < 3u) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line, .other = "| ", .bytes = 2u) != 0) ||
            (line[2] < '0') || (line[2] > '9'))
        {
            continue;
        }
        EmitCells cells = {};
        for (size_t from = 2u; (from < length) && (cells.count < 8u);)
        {
            const size_t bar = emit_find_n(line, length, " |", from);
            if (bar == length)
            {
                break;
            }
            cells.cell[cells.count] = &line[from];
            cells.length[cells.count] = bar - from;
            cells.count += 1u;
            from = bar + 3u;
        }
        if (cells.count != 6u)
        {
            continue;
        }
        const unsigned int number =
            (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = cells.cell[0], .end = NULL, .base = 10);
        if ((number == 0u) || (number > EMIT_QUESTIONS_MAX))
        {
            continue;
        }
        for (; questions < number; questions += 1u)
        {
            s_questions[questions] = {{0}, 0u, 0u, 1};
        }
        EmitQuestion *const question = &s_questions[number - 1u];
        const char *const bits = cells.cell[2];
        const size_t bits_length = cells.length[2];
        const size_t dash = emit_find_n(bits, bits_length, "-", 0u);
        question->instruction[0] = '\0';
        if (cells.length[1] >= 2u)
        {
            emit_add_n(question->instruction, sizeof(question->instruction), cells.cell[1] + 1u, cells.length[1] - 2u);
        }
        question->first =
            (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = bits, .end = NULL, .base = 10);
        question->last = (dash == bits_length)
                             ? 0u
                             : (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = bits + dash + 1u,
                                                        .end = NULL, .base = 10);
        static const char s_printed[] = ", as printed";
        const size_t printed_length = sizeof(s_printed) - 1u;
        const int as_printed =
            (cells.length[5] > printed_length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = cells.cell[5] + cells.length[5] - printed_length,
                        .other = s_printed, .bytes = printed_length) == 0);
        question->printed = question->printed && as_printed &&
                            (emit_find_n(bits, bits_length, ",", 0u) == bits_length) && (dash != bits_length);
    }
    platform_cell_released(file, platform_place_g_program_owner);
    for (unsigned int at = 0u; at < questions; at += 1u)
    {
        const EmitQuestion *const question = &s_questions[at];
        SassInstructionParts parts;
        sass_instruction_read(question->instruction, &parts);
        const SassForm *const form = (question->instruction[0] == '\0') ? nullptr : sass_machine_form(machine, &parts);
        if (question->printed && (form != nullptr))
        {
            emit_unread_kept(form, question->first, question->last);
        }
    }
}

// the part's answers on every form's unprinted operands, written by interface_sass_unprinted.sh --forms
#define EMIT_UNREAD_FORMS_PATH EMIT_SCAFFOLDING "interface_sass_unprinted_forms.md"
// the widest run that record probes at every value
#define EMIT_UNREAD_FORMS_WIDEST 6u

// The fields the part does not read, from the answers at `path` on every form's unprinted operands. A row is
// `| form | probed as | bits | the form contains | its own bits answer | values alike | values otherwise |`. A run of
// no more than EMIT_UNREAD_FORMS_WIDEST bits whose every value answered alike is one, keyed by the form the row names
static void emit_unread_forms_read(const char *path, const SassMachine *machine)
{
    size_t size = 0u;
    char *const file = emit_file_read(path, &size);
    for (size_t at = 0u; (file != NULL) && (at < size);)
    {
        const size_t end = emit_line_end(file, size, at);
        const char *const line = &file[at];
        const size_t length = end - at;
        at = end + 1u;
        if ((length < 3u) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line, .other = "| `", .bytes = 3u) != 0))
        {
            continue;
        }
        EmitCells cells = {};
        size_t from = 1u;
        for (size_t bar = emit_find_n(line, length, "|", from); bar < length;
             bar = emit_find_n(line, length, "|", from))
        {
            size_t first = from;
            size_t last = bar;
            while ((first < last) && (line[first] == ' '))
            {
                first += 1u;
            }
            while ((last > first) && (line[last - 1u] == ' '))
            {
                last -= 1u;
            }
            if (cells.count < 8u)
            {
                cells.cell[cells.count] = &line[first];
                cells.length[cells.count] = last - first;
            }
            cells.count += 1u;
            from = bar + 1u;
        }
        if ((cells.count != 7u) || (cells.length[0] < 2u))
        {
            continue;
        }
        const size_t dash = emit_find_n(cells.cell[2], cells.length[2], "-", 0u);
        const size_t of = emit_find_n(cells.cell[5], cells.length[5], " of ", 0u);
        if ((dash == cells.length[2]) || (of == cells.length[5]))
        {
            continue;
        }
        const unsigned int first =
            (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = cells.cell[2], .end = NULL, .base = 10);
        const unsigned int last = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                           .src = cells.cell[2] + dash + 1u, .end = NULL, .base = 10);
        const unsigned long alike =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = cells.cell[5], .end = NULL, .base = 10);
        const unsigned long questioned =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = cells.cell[5] + of + 4u, .end = NULL, .base = 10);
        if ((last < first) || (((last - first) + 1u) > EMIT_UNREAD_FORMS_WIDEST) || (alike != questioned) ||
            (questioned == 0UL))
        {
            continue;
        }
        char text[EMIT_LINE_BYTES] = {0};
        emit_add_n(text, sizeof(text), cells.cell[0] + 1u, cells.length[0] - 2u);
        SassInstructionParts parts;
        sass_instruction_read(text, &parts);
        const SassForm *const form = sass_machine_form(machine, &parts);
        if (form != nullptr)
        {
            emit_unread_kept(form, first, last);
        }
    }
    platform_cell_released(file, platform_place_g_program_owner);
}

// 1 where `bit` lies in a field of `form` the part does not read
static int emit_unread(const SassForm *form, unsigned int bit)
{
    for (unsigned int at = 0u; at < s_unread_count; at += 1u)
    {
        if ((s_unread[at].form == form) && (bit >= s_unread[at].first) && (bit <= s_unread[at].last))
        {
            return 1;
        }
    }
    return 0;
}

// NVIDIA's text in `text` written again in ours. The listing prints no descriptor on a memory operand whose bit 101 is
// clear, and the field contains the kernel's descriptor register all the same. Each such operand of a form that reads
// one is written term[URn], the register the kernel loaded
static void emit_our_text(char *text, size_t room, const SassMachine *machine)
{
    if (s_descriptor.text[0] == '\0')
    {
        return;
    }
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    const SassForm *const form = sass_machine_form(machine, &parts);
    for (unsigned int place = 0u; (form != nullptr) && (place < parts.operands); place += 1u)
    {
        int displayed = 0;
        for (unsigned int number = 0u; number < form->runs; number += 1u)
        {
            displayed = displayed || ((form->run[number].operand == place) && (form->run[number].first == 101u) &&
                                      (form->run[number].last == 101u));
        }
        const char *const operand = parts.operand[place];
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = room);
        const size_t at = emit_place(text, operand, 0u, length);
        if ((parts.type[place] == SASS_OPERAND_ADDRESS) && displayed && (operand[0] == '[') && (at != length))
        {
            char ours[EMIT_LINE_BYTES] = {0};
            emit_add_n(ours, sizeof(ours), text, at);
            emit_add(ours, sizeof(ours), "term[");
            emit_add(ours, sizeof(ours), s_descriptor.text);
            emit_add(ours, sizeof(ours), "]");
            emit_add(ours, sizeof(ours), &text[at]);
            emit_set(text, room, ours);
        }
    }
}

// The record as the blocks give it: its table's rows and its bits apart, each a movable text, and the counts of its
// summary. The calls of this module share them, and program_main begins the texts and writes them out
static PlatformText s_table;
static PlatformText s_bits;
static unsigned int s_rows;
static unsigned int s_instructions;
static unsigned int s_formless;
static unsigned int s_unknown;
static unsigned int s_unplaced;
static unsigned int s_operation_apart;
static unsigned int s_unread_apart;
static unsigned int s_scheduler_apart;

// `text` onto `line` as a code span, doubled where it contains a backtick of its own
static void emit_code(VerbaLinea *line, const char *text)
{
    if (text[0] == '\0')
    {
        return;
    }
    const int ticked = emit_has(text, "`");
    mmgr_line_put(line, ticked ? "`` " : "`");
    mmgr_line_put(line, text);
    mmgr_line_put(line, ticked ? " ``" : "`");
}

// One row of the record: the block, NVIDIA's instruction and where it lies, and what our reader, our assembler and our
// compiler made of it. The difference read off them comes last. A row for our compiler's writing names no instruction.
// The row goes onto the table, its bits onto the bits apart where it has any, and its counts into the summary's
static void emit_row(unsigned int block, unsigned int precept, const char *address, const char *nvidia,
                     const char *reader, const char *operation, const char *scheduler, const char *compiler,
                     const char *difference, const char *bits = "")
{
    s_rows += 1u;
    s_instructions += ((address[0] == '\0') || (nvidia[0] == '\0') || (reader[0] == '\0')) ? 0u : 1u;
    s_formless += emit_same(difference, "no form in the machine file") ? 1u : 0u;
    s_unknown += emit_same(difference, "a form matches its text, not these bits") ? 1u : 0u;
    s_unplaced += emit_same(difference, "a form matches its text and places none of an operand") ? 1u : 0u;
    s_operation_apart += emit_same(difference, "operation bits apart") ? 1u : 0u;
    s_unread_apart += emit_has(operation, "the part does not read") ? 1u : 0u;
    s_scheduler_apart +=
        ((scheduler[0] != '\0') && !emit_same(scheduler, "same") && !emit_same(scheduler, "-")) ? 1u : 0u;
    VerbaLinea *const line = &s_table.line;
    mmgr_line_put(line, "| ");
    mmgr_line_decimal(line, block, 1u);
    mmgr_line_put(line, " | ");
    mmgr_line_put(line, s_names[precept]);
    mmgr_line_put(line, " | ");
    mmgr_line_put(line, address);
    mmgr_line_put(line, " | ");
    emit_code(line, nvidia);
    mmgr_line_put(line, " | ");
    if (emit_same(reader, "same") || emit_same(reader, "no form matches its text") ||
        emit_same(reader, "a form matches its text and not its bits") || (reader[0] == '\0'))
    {
        mmgr_line_put(line, reader);
    }
    else
    {
        emit_code(line, reader);
    }
    mmgr_line_put(line, " | ");
    mmgr_line_put(line, operation);
    mmgr_line_put(line, " | ");
    mmgr_line_put(line, scheduler);
    mmgr_line_put(line, " | ");
    emit_code(line, compiler);
    mmgr_line_put(line, " | ");
    mmgr_line_put(line, difference);
    mmgr_line_put(line, " |\n");
    if (bits[0] != '\0')
    {
        VerbaLinea *const apart = &s_bits.line;
        mmgr_line_put(apart, "\nBlock ");
        mmgr_line_decimal(apart, block, 1u);
        mmgr_line_put(apart, ", ");
        mmgr_line_put(apart, s_names[precept]);
        mmgr_line_put(apart, ", `");
        mmgr_line_put(apart, nvidia);
        mmgr_line_put(apart, "`");
        mmgr_line_put(apart, (compiler[0] == '\0') ? ", our assembler" : ", ");
        mmgr_line_put(apart, compiler);
        mmgr_line_put(apart, "\n\n```\n");
        mmgr_line_put(apart, bits);
        mmgr_line_put(apart, "```\n");
    }
}

// a check of our reader and our assembler against one of NVIDIA's instructions, its lines printed and its row
// recorded. The counts of the block's instructions that agree go up where they agree
static void emit_instruction(unsigned int block, const EmitLine *line, const SassMachine *machine,
                             unsigned int *read_same, unsigned int *assembled_same, unsigned int *control_same)
{
    const unsigned int precept = block - 1u;
    EmitName address;
    (void)emit_hex(line->address, 5u, &address);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, address.text);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, line->text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    char text[256];
    const int read = sass_encoding_read(machine, line->low, line->high, line->address, text, sizeof(text));
    EmitText ours = {{0}};
    if (read)
    {
        emit_plain(text, sizeof(text), ours.text, sizeof(ours.text));
    }
    // whether the machine file contains a form for the instruction's text, its operation and the types of its
    // operands, apart from whether that form matches these bits
    SassInstructionParts parts;
    sass_instruction_read(line->text, &parts);
    const int text_known = (sass_machine_form(machine, &parts) != nullptr);
    const char *const unread = text_known ? "a form matches its text and not its bits" : "no form matches its text";
    const int same_text = read && emit_same(ours.text, line->text);
    const char *const reader = !read ? unread : (same_text ? "same" : ours.text);
    *read_same += same_text ? 1u : 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "         our reader: ");
        mmgr_line_put(output_line, !read ? unread : (same_text ? "the same text" : ours.text));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // A branch's target is the address its text names. The listing prints it bare and our assembler reads a bare
    // number as the immediate itself. The text goes to the assembler as a label, `(0x...), which it counts from the
    // instruction after the branch, and the address is given as the label's
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line->text, .cap = sizeof(line->text));
    const size_t branch = emit_place(line->text, "BRA 0x", 0u, length);
    const unsigned long long target =
        (branch == length)
            ? 0ULL
            : EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = line->text + branch + 6u, .end = NULL, .base = 16);
    EmitText put = {{0}};
    if (branch == length)
    {
        EMIT_SET(put, line->text);
    }
    else
    {
        emit_add_n(put.text, sizeof(put.text), line->text, branch + 4u);
        EMIT_ADD(put, "`(");
        EMIT_ADD(put, line->text + branch + 4u);
        EMIT_ADD(put, ")");
    }
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    if (!sass_assemble(machine, put.text, line->address, target, SASS_CONTROL_BASE, &low, &high))
    {
        emit_said("         our assembler: refused it");
        // where a form matches the text, its own encoding beside NVIDIA's shows where the operand it cannot place
        // lies against the fields it contains
        const SassForm *const form = sass_machine_form(machine, &parts);
        EmitText sample = {{0}};
        if (form != nullptr)
        {
            emit_bits_apart(line->low, line->high, form->low, form->high, "the form ", &sample);
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "           NVIDIA's against the form's own encoding:\n");
            mmgr_line_put(output_line, sample.text);
            platform_line_out_ended();
        }
        emit_row(block, precept, address.text, line->text, reader, "refused", "-", "",
                 text_known ? "a form matches its text and places none of an operand" : "no form in the machine file",
                 sample.text);
        return;
    }
    const int operation = (low == line->low) && ((high & EMIT_OPERATION_HIGH) == (line->high & EMIT_OPERATION_HIGH));
    const int control = ((high & ~EMIT_OPERATION_HIGH) == (line->high & ~EMIT_OPERATION_HIGH));
    // the bits the operation is apart at, the high word's counted from 64, and whether every one lies in a field of the
    // form matching NVIDIA's text that the part does not read
    const SassForm *const form = sass_machine_form(machine, &parts);
    EmitText apart = {{0}};
    int unread_only = (operation == 0) && (form != nullptr);
    for (unsigned int bit = 0u; (operation == 0) && (bit < 105u); bit += 1u)
    {
        const unsigned long long theirs =
            (bit < 64u) ? ((line->low >> bit) & 1ULL) : ((line->high >> (bit - 64u)) & 1ULL);
        const unsigned long long mine = (bit < 64u) ? ((low >> bit) & 1ULL) : ((high >> (bit - 64u)) & 1ULL);
        if (theirs != mine)
        {
            VerbaLinea apart_line = emit_after(apart.text, sizeof(apart.text));
            mmgr_line_put(&apart_line, (apart.text[0] == '\0') ? "" : " ");
            mmgr_line_decimal(&apart_line, bit, 1u);
            (void)mmgr_line_finish(&apart_line);
        }
        unread_only = unread_only && ((theirs == mine) || emit_unread(form, bit));
    }
    *assembled_same += (operation || unread_only) ? 1u : 0u;
    *control_same += control ? 1u : 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "         our assembler: operation bits ");
        mmgr_line_put(output_line,
                      operation ? "the same" : (unread_only ? "apart only at bits the part does not read" : "apart"));
        mmgr_line_put(output_line, ", scheduler bits ");
        mmgr_line_put(output_line, control ? "the same" : "apart");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EmitText bits = {{0}};
    if (!operation && !unread_only)
    {
        emit_bits_apart(line->low, line->high, low, high, "ours     ", &bits);
    }
    if (!operation)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "           apart at bit ");
        mmgr_line_put(output_line, apart.text);
        mmgr_line_put(output_line, "\n");
        mmgr_line_put(output_line, bits.text);
        platform_line_out_ended();
    }
    EmitName theirs_scheduler;
    EmitName ours_scheduler;
    (void)emit_hex(line->high >> 41u, 6u, &theirs_scheduler);
    (void)emit_hex(high >> 41u, 6u, &ours_scheduler);
    if (!control)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "           scheduler NVIDIA ");
        mmgr_line_put(output_line, theirs_scheduler.text);
        mmgr_line_put(output_line, ", ours ");
        mmgr_line_put(output_line, ours_scheduler.text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EmitText scheduler = {{0}};
    if (control)
    {
        EMIT_SET(scheduler, "same");
    }
    else
    {
        EMIT_SET(scheduler, "NVIDIA ");
        EMIT_ADD(scheduler, theirs_scheduler.text);
        EMIT_ADD(scheduler, ", ours ");
        EMIT_ADD(scheduler, ours_scheduler.text);
    }
    const char *const difference = (!read && !operation && text_known) ? "a form matches its text, not these bits"
                                   : (!read && !operation)             ? "no form in the machine file"
                                   : (!operation && !unread_only)      ? "operation bits apart"
                                   : !control                          ? "scheduler bits"
                                                                       : "none";
    EmitText assembled = {{0}};
    if (operation)
    {
        EMIT_SET(assembled, "same");
    }
    else
    {
        EMIT_SET(assembled, unread_only ? "same where the part reads; apart at bit " : "apart at bit ");
        EMIT_ADD(assembled, apart.text);
        EMIT_ADD(assembled, unread_only ? ", which the part does not read" : "");
    }
    emit_row(block, precept, address.text, line->text, reader, assembled.text, scheduler.text, "", difference,
             bits.text);
}

// the passage of the block, its instructions between tag n and tag n + 1, written again in our text. The calls of
// this module share it
static EmitLine s_passage[EMIT_PASSAGE_MAX];

// `count` words of `code` read back one after another into `reads`, each by our reader, "; " between them
static void emit_reads(const SassMachine *machine, const unsigned char *code, unsigned int count,
                       unsigned long long address, EmitText *reads)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long word_low = 0ULL;
        unsigned long long word_high = 0ULL;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &word_low, .src = &code[16u * at], .bytes = 8u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &word_high, .src = &code[(16u * at) + 8u], .bytes = 8u);
        char text[256];
        if (sass_encoding_read(machine, word_low, word_high, address + (16ULL * at), text, sizeof(text)))
        {
            EmitText plain;
            emit_plain(text, sizeof(text), plain.text, sizeof(plain.text));
            EMIT_ADD(*reads, plain.text);
        }
        else
        {
            EMIT_ADD(*reads, "(unread)");
        }
        EMIT_ADD(*reads, ((at + 1u) < count) ? "; " : "");
    }
}

// a line on stdout: `head`, then `text`, then `tail`
static void emit_said3(const char *head, const char *text, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, head);
    mmgr_line_put(output_line, text);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// MOV has two candidates, and the part's clock chooses between them. One is the passage, the copy written as no
// instruction, where the register the word is read into is the register it is written from. The other is the copy word
// (word_copy, word_web.h), written through sass.krs as an instruction
static void emit_move(unsigned int block, unsigned int count, const SassMachine *machine, const Ruleset *rules)
{
    const unsigned int precept = block - 1u;
    emit_said("  our compiler, candidate 1, the passage: no instruction, the registers the copy joins made one");
    EmitName loaded;
    EmitName stored;
    const int carried = emit_passage(s_passage, count, &loaded, &stored);
    emit_row(block, precept, "", "", "", "", "", "candidate 1, the passage, no instruction",
             carried ? "none: NVIDIA's block is the passage" : "NVIDIA's block is not the passage");
    const char *const given[2] = {(stored.text[0] == '\0') ? "R9" : stored.text,
                                  (loaded.text[0] == '\0') ? "R9" : loaded.text};
    EmitText written = {{0}};
    if (emit_form(rules, "word_copy", given, 2u, &written) == 0)
    {
        emit_said("  our compiler, candidate 2, the copy word: sass.krs writes no word_copy form");
        emit_row(block, precept, "", "", "", "", "", "candidate 2, word_copy", "sass.krs writes no word_copy form");
        return;
    }
    EmitText plain;
    emit_plain(written.text, sizeof(written.text), plain.text, sizeof(plain.text));
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    const int assembles = sass_assemble(machine, plain.text, 0ULL, 0ULL, SASS_CONTROL_BASE, &low, &high);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  our compiler, candidate 2, the copy word: ");
        mmgr_line_put(output_line, plain.text);
        mmgr_line_put(output_line, ", ");
        mmgr_line_put(output_line, assembles ? "which assembles, one instruction where NVIDIA's block contains none"
                                             : "which does not assemble");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EmitText compiler;
    EMIT_SET(compiler, "candidate 2, word_copy: ");
    EMIT_ADD(compiler, plain.text);
    emit_row(block, precept, "", "", "", "", "", compiler.text,
             assembles ? "one instruction where NVIDIA's block contains none" : "does not assemble");
}

// ERR has two candidates, and the part's clock chooses between them, and this code does not. One is a trap where it
// is, the way NVIDIA writes it, a branch past the trap on the flag. The other is a branch to a handler, the error word
// (word_web.h), written through sass.krs on NVIDIA's predicate and landing on the label label_error writes
static void emit_error(unsigned int block, unsigned int count, const SassMachine *machine, const Ruleset *rules)
{
    const unsigned int precept = block - 1u;
    const EmitLine *trap = nullptr;
    const EmitLine *guard = nullptr;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        trap = emit_has(s_passage[at].text, "TRAP") ? &s_passage[at] : trap;
        guard = emit_has(s_passage[at].text, "BRA") ? &s_passage[at] : guard;
    }
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    const int trap_written =
        (trap != nullptr) && sass_assemble(machine, trap->text, trap->address, 0ULL, SASS_CONTROL_BASE, &low, &high);
    SassInstructionParts trap_parts;
    if (trap != nullptr)
    {
        sass_instruction_read(trap->text, &trap_parts);
    }
    const int trap_known = (trap != nullptr) && (sass_machine_form(machine, &trap_parts) != nullptr);
    const char *const trap_reading =
        (trap == nullptr) ? "NVIDIA's block contains no trap"
        : trap_written    ? "the trap assembles"
        : trap_known      ? "a form matches the trap and places none of its operand, and it cannot be written yet"
                          : "the machine file contains no form for the trap, and it cannot be written yet";
    emit_said3("  our compiler, candidate 1, a trap in place: ", trap_reading, "");
    emit_row(block, precept, "", "", "", "", "", "candidate 1, a trap in place", trap_reading);
    // the predicate the branch is taken on: NVIDIA's guard with its negation dropped, since the handler is gone to
    // where the flag is set and NVIDIA's branch passes the trap where it is clear
    EmitName where;
    EMIT_SET(where, "P0");
    if ((guard != nullptr) && (guard->text[0] == '@'))
    {
        const size_t skip = (guard->text[1] == '!') ? 2u : 1u;
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = guard->text, .cap = sizeof(guard->text));
        const size_t space = emit_place(guard->text, " ", 0u, length);
        where.text[0] = '\0';
        emit_add_n(where.text, sizeof(where.text), guard->text + skip, (space > skip) ? (space - skip) : 0u);
    }
    EmitText written = {{0}};
    const char *const on[2] = {where.text, "1"};
    const char *const one[1] = {"1"};
    const int branch = emit_form(rules, "error_if", on, 2u, &written);
    const int label = branch && emit_form(rules, "label_error", one, 1u, &written);
    if (!label)
    {
        emit_said("  our compiler, candidate 2, a branch to a handler: sass.krs writes no error or label_error form");
        emit_row(block, precept, "", "", "", "", "", "candidate 2, a branch to a handler",
                 "sass.krs writes no error or label_error form");
        return;
    }
    static unsigned char code[256];
    const unsigned int assembled = sass_assemble_lines(machine, written.text, SASS_CONTROL_BASE, code, sizeof(code));
    EmitText reads = {{0}};
    emit_reads(machine, code, assembled, 0ULL, &reads);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  our compiler, candidate 2, a branch to a handler, error on ");
        mmgr_line_put(output_line, where.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, (assembled == 0u) ? "does not assemble" : reads.text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    emit_said3("    against NVIDIA's ", (guard == nullptr) ? "block, which contains no branch" : guard->text,
               ": the branch is taken where the flag is set and lands on the handler, where NVIDIA's passes the trap "
               "where it is clear");
    EmitText compiler;
    EMIT_SET(compiler, "candidate 2, error on ");
    EMIT_ADD(compiler, where.text);
    EMIT_ADD(compiler, ": ");
    EMIT_ADD(compiler, reads.text);
    emit_row(block, precept, "", "", "", "", "", compiler.text,
             (assembled == 0u) ? "does not assemble"
                               : "taken where the flag is set, to a handler; NVIDIA's passes a trap where it is clear");
}

// JCC is the loop_back_if word (word_web.h), a jump taken on a predicate to a label. It is written through sass.krs on
// the predicate NVIDIA's branch is guarded by and lands on the label label_loop writes, and its guard keeps its sense
static void emit_jump(unsigned int block, unsigned int count, const SassMachine *machine, const Ruleset *rules)
{
    const unsigned int precept = block - 1u;
    const EmitLine *guard = nullptr;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        guard = ((s_passage[at].text[0] == '@') && emit_has(s_passage[at].text, "BRA")) ? &s_passage[at] : guard;
    }
    if (guard == nullptr)
    {
        emit_said("  our compiler: loop_back_if, and NVIDIA's block contains no guarded branch");
        emit_row(block, precept, "", "", "", "", "", "loop_back_if", "NVIDIA's block contains no guarded branch");
        return;
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = guard->text, .cap = sizeof(guard->text));
    const size_t space = emit_place(guard->text, " ", 0u, length);
    EmitName where = {{0}};
    emit_add_n(where.text, sizeof(where.text), guard->text + 1u, (space > 1u) ? (space - 1u) : 0u);
    EmitText written = {{0}};
    // loop_back_if's parameters are the loop's number, then the predicate
    const char *const back[2] = {"1", where.text};
    const char *const one[1] = {"1"};
    const int branch = emit_form(rules, "loop_back_if", back, 2u, &written);
    const int label = branch && emit_form(rules, "label_loop", one, 1u, &written);
    static unsigned char code[256];
    const unsigned int assembled =
        label ? sass_assemble_lines(machine, written.text, SASS_CONTROL_BASE, code, sizeof(code)) : 0u;
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &low, .src = &code[0], .bytes = 8u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &high, .src = &code[8], .bytes = 8u);
    char text[256];
    EmitText reads = {{0}};
    if (assembled == 0u)
    {
        EMIT_SET(reads, "does not assemble");
    }
    else if (sass_encoding_read(machine, low, high, guard->address, text, sizeof(text)))
    {
        emit_plain(text, sizeof(text), reads.text, sizeof(reads.text));
    }
    else
    {
        EMIT_SET(reads, "(unread)");
    }
    // the jump's guard and operation checked against NVIDIA's, its target apart: the label here lands where label_loop
    // is and NVIDIA's lands past its block
    const unsigned long long guard_bits = 0xf000ULL;
    const unsigned long long operation_bits = 0xfffULL;
    const int same_guard = (assembled != 0u) && ((low & guard_bits) == (guard->low & guard_bits));
    const int same_operation = (assembled != 0u) && ((low & operation_bits) == (guard->low & operation_bits));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  our compiler, loop_back_if on ");
        mmgr_line_put(output_line, where.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, reads.text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    against NVIDIA's ");
        mmgr_line_put(output_line, guard->text);
        mmgr_line_put(output_line, ": the guard ");
        mmgr_line_put(output_line, same_guard ? "the same" : "apart");
        mmgr_line_put(output_line, ", the operation ");
        mmgr_line_put(output_line, same_operation ? "the same" : "apart");
        mmgr_line_put(output_line, ", the target its own label's");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    EmitName address;
    EmitText compiler;
    EMIT_SET(compiler, "loop_back_if on ");
    EMIT_ADD(compiler, where.text);
    EMIT_ADD(compiler, ": ");
    EMIT_ADD(compiler, reads.text);
    EmitText difference;
    EMIT_SET(difference, "guard ");
    EMIT_ADD(difference, same_guard ? "the same" : "apart");
    EMIT_ADD(difference, ", operation ");
    EMIT_ADD(difference, same_operation ? "the same" : "apart");
    EMIT_ADD(difference, ", target its own label's");
    emit_row(block, precept, emit_hex(guard->address, 5u, &address), guard->text, "", "", "", compiler.text,
             (assembled == 0u) ? "does not assemble" : difference.text);
}

// The precept as our compiler writes it: the word whose tree is that precept alone, through sass.krs. It is written
// with NVIDIA's own registers and set beside NVIDIA's instruction, with the leaves swapped as well for a word of two
static void emit_word_written(unsigned int block, unsigned int count, const SassMachine *machine, const Ruleset *rules)
{
    const unsigned int precept = block - 1u;
    const Word *const word = word_web_word_for(precept);
    if (word == nullptr)
    {
        EmitText tree;
        EmitText reading;
        if (emit_alphabet(precept, &tree))
        {
            EMIT_SET(reading, "the alphabet web writes it ");
            EMIT_ADD(reading, tree.text);
        }
        else
        {
            EMIT_SET(reading, "the alphabet web contains no tree for it");
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  our compiler: no word is ");
            mmgr_line_put(output_line, s_names[precept]);
            mmgr_line_put(output_line, " alone; ");
            mmgr_line_put(output_line, reading.text);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        EmitText compiler;
        EMIT_SET(compiler, "no word; ");
        EMIT_ADD(compiler, reading.text);
        emit_row(block, precept, "", "", "", "", "", compiler.text, "no word");
        return;
    }
    // NVIDIA's instruction for the precept: the block's one line that writes a register from registers and is not a
    // load, a store or a move of a parameter
    const EmitLine *core = nullptr;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int moves = emit_has(s_passage[at].text, "LDG") || emit_has(s_passage[at].text, "STG") ||
                          emit_has(s_passage[at].text, "c[0x0]");
        core = moves ? core : &s_passage[at];
    }
    if (core == nullptr)
    {
        emit_said3("  our compiler: ", word->name,
                   ", and NVIDIA's block contains no instruction past its loads and stores");
        emit_row(block, precept, "", "", "", "", "", word->name, "NVIDIA's block contains no instruction for it");
        return;
    }
    // the registers the instruction names, a negation or a complement in front of one dropped: the word is written
    // with the register, and its form puts any negation itself
    EmitName operands[EMIT_OPERANDS_MAX];
    EmitName registers[EMIT_OPERANDS_MAX];
    unsigned int register_count = 0u;
    const unsigned int operand_count = emit_operands(core->text, operands);
    for (unsigned int at = 0u; at < operand_count; at += 1u)
    {
        const char *const operand = operands[at].text;
        const char *const bare = ((operand[0] == '-') || (operand[0] == '~')) ? (operand + 1u) : operand;
        if (emit_register(bare))
        {
            EMIT_SET(registers[register_count], bare);
            register_count += 1u;
        }
    }
    if (register_count < (1u + word->reads))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  our compiler: ");
        mmgr_line_put(output_line, word->name);
        mmgr_line_put(output_line, ", and NVIDIA's ");
        mmgr_line_put(output_line, core->text);
        mmgr_line_put(output_line, " names too few registers to write it with");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        emit_row(block, precept, "", "", "", "", "", word->name, "NVIDIA's instruction names too few registers");
        return;
    }
    // The destination first, then the leaves in NVIDIA's order, and for two leaves the other order as well. The record
    // keeps the order whose operation bits are apart from NVIDIA's at the fewest places
    EmitText recorded_compiler = {{0}};
    EmitText recorded_difference = {{0}};
    EmitText recorded_bits = {{0}};
    unsigned int recorded_apart = ~0u;
    for (unsigned int order = 0u; order < ((word->reads == 2u) ? 2u : 1u); order += 1u)
    {
        const char *given[EMIT_OPERANDS_MAX];
        given[0] = registers[0].text;
        for (unsigned int leaf = 0u; leaf < word->reads; leaf += 1u)
        {
            const unsigned int place = (order == 0u) ? leaf : (word->reads - 1u - leaf);
            given[1u + leaf] = registers[1u + place].text;
        }
        EmitText written = {{0}};
        if (emit_form(rules, word->name, given, 1u + word->reads, &written) == 0)
        {
            emit_said3("  our compiler: ", word->name, " is not written by sass.krs");
            emit_row(block, precept, "", "", "", "", "", word->name, "sass.krs writes no form for it");
            return;
        }
        EmitText plain;
        emit_plain(written.text, sizeof(written.text), plain.text, sizeof(plain.text));
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        const int assembles = sass_assemble(machine, plain.text, core->address, 0ULL, SASS_CONTROL_BASE, &low, &high);
        const int same =
            assembles && (low == core->low) && ((high & EMIT_OPERATION_HIGH) == (core->high & EMIT_OPERATION_HIGH));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  our compiler, ");
            mmgr_line_put(output_line, word->name);
            mmgr_line_put(output_line, " with its leaves ");
            mmgr_line_put(output_line, (order == 0u) ? "in NVIDIA's order" : "swapped");
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, plain.text);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    against NVIDIA's ");
            mmgr_line_put(output_line, core->text);
            mmgr_line_put(output_line, " at ");
            mmgr_line_hex(output_line, core->address, 5u);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, !assembles ? "ours does not assemble"
                                                  : (same ? "the same operation bits" : "operation bits apart"));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        EmitText lacking;
        EmitText extra;
        emit_lacking(core->text, plain.text, &lacking);
        emit_lacking(plain.text, core->text, &extra);
        EmitText difference;
        if (!assembles)
        {
            EMIT_SET(difference, "ours does not assemble");
        }
        else if (same)
        {
            EMIT_SET(difference, "none, leaves ");
            EMIT_ADD(difference, (order == 0u) ? "in NVIDIA's order" : "swapped");
        }
        else
        {
            EMIT_SET(difference, "operation bits apart");
        }
        if (lacking.text[0] != '\0')
        {
            EMIT_ADD(difference, "; NVIDIA's carries ");
            EMIT_ADD(difference, lacking.text);
            EMIT_ADD(difference, " and ours does not");
        }
        if (extra.text[0] != '\0')
        {
            EMIT_ADD(difference, "; ours carries ");
            EMIT_ADD(difference, extra.text);
            EMIT_ADD(difference, " and NVIDIA's does not");
        }
        // the places the operation bits are apart at; a writing that does not assemble is apart everywhere
        unsigned int places = assembles ? 0u : 105u;
        for (unsigned int bit = 0u; assembles && (bit < 105u); bit += 1u)
        {
            const unsigned long long theirs =
                (bit < 64u) ? ((core->low >> bit) & 1ULL) : ((core->high >> (bit - 64u)) & 1ULL);
            const unsigned long long mine = (bit < 64u) ? ((low >> bit) & 1ULL) : ((high >> (bit - 64u)) & 1ULL);
            places += (theirs != mine) ? 1u : 0u;
        }
        if (places < recorded_apart)
        {
            recorded_apart = places;
            EMIT_SET(recorded_compiler, word->name);
            EMIT_ADD(recorded_compiler, ": ");
            EMIT_ADD(recorded_compiler, plain.text);
            recorded_difference = difference;
            if (!same)
            {
                VerbaLinea apart_line = emit_after(recorded_difference.text, sizeof(recorded_difference.text));
                mmgr_line_put(&apart_line, "; apart at ");
                mmgr_line_decimal(&apart_line, places, 1u);
                mmgr_line_put(&apart_line, " bits");
                (void)mmgr_line_finish(&apart_line);
            }
            recorded_bits.text[0] = '\0';
            if (!same && assembles)
            {
                emit_bits_apart(core->low, core->high, low, high, "ours     ", &recorded_bits);
            }
        }
        if (same)
        {
            break;
        }
    }
    if (recorded_bits.text[0] != '\0')
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    the closest writing against NVIDIA's, bit by bit:\n");
        mmgr_line_put(output_line, recorded_bits.text);
        platform_line_out_ended();
    }
    EmitName address;
    emit_row(block, precept, emit_hex(core->address, 5u, &address), core->text, "", "", "", recorded_compiler.text,
             recorded_difference.text, recorded_bits.text);
}

// block `block` of the listing checked against our reader, our assembler and our compiler, printed and recorded
static void emit_block(const EmitLine *lines, unsigned long long line_count, unsigned int block,
                       const SassMachine *machine, const Ruleset *rules)
{
    const unsigned int precept = block - 1u;
    unsigned int count = 0u;
    int inside = 0;
    for (unsigned long long at = 0ULL; at < line_count; at += 1ULL)
    {
        const unsigned int tag = emit_tag(&lines[at]);
        inside = (tag == block) ? 1 : ((tag != 0u) ? 0 : inside);
        if ((inside != 0) && (tag == 0u) && (count < EMIT_PASSAGE_MAX))
        {
            s_passage[count] = lines[at];
            emit_our_text(s_passage[count].text, sizeof(s_passage[count].text), machine);
            count += 1u;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "block ");
        mmgr_line_decimal(output_line, block, 1u);
        mmgr_line_put(output_line, ": precept ");
        mmgr_line_put(output_line, s_names[precept]);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, " instructions between tag ");
        mmgr_line_decimal(output_line, block, 1u);
        mmgr_line_put(output_line, " and tag ");
        mmgr_line_decimal(output_line, block + 1u, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int read_same = 0u;
    unsigned int assembled_same = 0u;
    unsigned int control_same = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        emit_instruction(block, &s_passage[at], machine, &read_same, &assembled_same, &control_same);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  of ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, ": our reader gave NVIDIA's text for ");
        mmgr_line_decimal(output_line, read_same, 1u);
        mmgr_line_put(output_line, ", our assembler NVIDIA's operation bits for ");
        mmgr_line_decimal(output_line, assembled_same, 1u);
        mmgr_line_put(output_line, " and its scheduler bits for ");
        mmgr_line_decimal(output_line, control_same, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // NOP is gnascor's passage: gray between fuzz and fizz, the word carried in and carried out as it came, written as
    // no instruction
    if (precept == PRECEPT_NOP)
    {
        emit_said("  our compiler: NOP is the passage, gray between fuzz and fizz (gnascor): no instruction, the word "
                  "carried out as it came in");
        EmitName loaded;
        EmitName stored;
        const int carried = emit_passage(s_passage, count, &loaded, &stored);
        emit_row(block, precept, "", "", "", "", "", "the passage, no instruction",
                 carried ? "none: NVIDIA's block is the passage" : "NVIDIA's block is not the passage");
    }
    else if (precept == PRECEPT_MOV)
    {
        emit_move(block, count, machine, rules);
    }
    else if (precept == PRECEPT_ERR)
    {
        emit_error(block, count, machine, rules);
    }
    else if (precept == PRECEPT_JCC)
    {
        emit_jump(block, count, machine, rules);
    }
    else
    {
        emit_word_written(block, count, machine, rules);
    }
}

// the record written as a table to `path`
static int emit_record(const char *path, const char *listing)
{
    PlatformFile *const file = platform_file_open(path, "w");
    if (file == nullptr)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "the record ");
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_put(&engine_note.line, " did not open");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
        return 0;
    }
    VerbaLinea *const line = platform_file_line(file);
    mmgr_line_put(line, "# The monolith checked against our compiler\n\n");
    mmgr_line_put(line, "Written by `monolith_emit ");
    mmgr_line_put(line, listing);
    mmgr_line_put(line, " all <this file>` and written again on every run. ");
    mmgr_line_put(line, "Each block of the monolith's tagged build is NVIDIA's writing of one precept. ");
    mmgr_line_put(
        line, "A row with an address is one of NVIDIA's instructions checked against our reader and our assembler; ");
    mmgr_line_put(line, "a row with a compiler column is our compiler's writing of the block's precept. ");
    mmgr_line_put(line, "Scheduler bits are 105 to 127, apart from the operation's. ");
    mmgr_line_put(line, "NVIDIA's text is written in ours: a memory operand whose descriptor "
                        "the listing does not print is written term[");
    mmgr_line_put(line, (s_descriptor.text[0] == '\0') ? "URn" : s_descriptor.text);
    mmgr_line_put(line, "], the uniform register the kernel loads c[0x0][0x118] into. ");
    mmgr_line_put(line, "Operation bits apart only in a field the part does not read, every value of it answering as "
                        "printed in `" EMIT_UNREAD_PATH "` or alike in `" EMIT_UNREAD_FORMS_PATH
                        "`, are read as the same.\n\n");
    mmgr_line_decimal(line, s_instructions, 1u);
    mmgr_line_put(line, " of NVIDIA's instructions read: ");
    mmgr_line_decimal(line, s_formless, 1u);
    mmgr_line_put(line, " with no form in the machine file, ");
    mmgr_line_decimal(line, s_unknown, 1u);
    mmgr_line_put(line, " whose text a form matches and whose bits it does not, ");
    mmgr_line_decimal(line, s_unplaced, 1u);
    mmgr_line_put(line, " whose text a form matches with an operand it places none of, ");
    mmgr_line_decimal(line, s_operation_apart, 1u);
    mmgr_line_put(line, " with operation bits apart, ");
    mmgr_line_decimal(line, s_unread_apart, 1u);
    mmgr_line_put(line, " apart only at bits the part does not read, ");
    mmgr_line_decimal(line, s_scheduler_apart, 1u);
    mmgr_line_put(line, " with scheduler bits apart.\n\n");
    mmgr_line_put(line, "| block | precept | address | NVIDIA | our reader | our assembler | "
                        "scheduler | our compiler | difference |\n");
    mmgr_line_put(line, "|---|---|---|---|---|---|---|---|---|\n");
    const unsigned long long table_size = platform_text_size(&s_table);
    const char *const table = platform_text_bytes(&s_table);
    (void)mmgr_line_put_n(line, (table != NULL) ? table : "", (size_t)((table != NULL) ? table_size : 0ULL));
    // every row whose operation bits are apart, NVIDIA's encoding and ours bit by bit, bit 127 first
    mmgr_line_put(line, "\n## Bits apart\n\n");
    mmgr_line_put(line, "Each instruction whose operation bits are apart from NVIDIA's, both encodings bit by bit with "
                        "bit 127 first under each byte's top bit; `^` marks an operation bit apart and `.` a scheduler "
                        "bit apart.\n");
    const unsigned long long bits_size = platform_text_size(&s_bits);
    const char *const bits = platform_text_bytes(&s_bits);
    (void)mmgr_line_put_n(line, (bits != NULL) ? bits : "", (size_t)((bits != NULL) ? bits_size : 0ULL));
    platform_file_close(file);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "the record of ");
        mmgr_line_decimal(output_line, s_rows, 1u);
        mmgr_line_put(output_line, " rows written to ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// a note on stderr: `head`, then `text`, then `tail`
static void emit_noted(const char *head, const char *text, const char *tail)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, head);
    mmgr_line_put(&engine_note.line, text);
    mmgr_line_put(&engine_note.line, tail);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

int program_main(int count, char **arguments)
{
    const int all = (count > 2) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[2],
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "all", .ci = EMBED_FALSE));
    if ((count < 3) || (count > 5) || (all && (count < 4)))
    {
        emit_noted(
            "monolith_emit <listing> <block> [machine file]\nmonolith_emit <listing> all <record> [machine file]", "",
            "");
        return 2;
    }
    const int machine_at = all ? 4 : 3;
    const char *const path =
        (count > machine_at) ? arguments[machine_at] : "src/cu/transpiler/lstar/protocol/table/sm_86.khw";
    static SassMachine machine;
    if (!sass_machine_read(&machine, path))
    {
        emit_noted("the machine file ", path, " did not read");
        return 1;
    }
    const Ruleset *const rules = sass_target().ruleset(1);
    if (rules == nullptr)
    {
        return 1;
    }
    emit_unread_read(EMIT_UNREAD_PATH, &machine);
    emit_unread_forms_read(EMIT_UNREAD_FORMS_PATH, &machine);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, s_unread_count, 1u);
        mmgr_line_put(output_line, " fields the part does not read");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long line_count = 0ULL;
    EmitLine *const lines = emit_listing(arguments[1], &line_count);
    // the kernel's descriptor register, from the line that loads c[0x0][0x118] into a uniform register pair
    for (unsigned long long at = 0ULL; at < line_count; at += 1ULL)
    {
        const char *const text = lines[at].text;
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = sizeof(lines[at].text));
        const size_t uniform = emit_place(text, "ULDC.64 UR", 0u, length);
        if ((uniform != length) && emit_has(text, "c[0x0][0x118]"))
        {
            const size_t name = uniform + 8u;
            const size_t comma = emit_place(text, ",", name, length);
            s_descriptor.text[0] = '\0';
            emit_add_n(s_descriptor.text, sizeof(s_descriptor.text), &text[name], comma - name);
        }
    }
    platform_text_begun(&s_table, EMIT_RECORD_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    platform_text_begun(&s_bits, EMIT_RECORD_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    int status = 0;
    if (all)
    {
        for (unsigned int block = 1u; block <= PRECEPT_COUNT; block += 1u)
        {
            emit_block(lines, line_count, block, &machine, rules);
        }
        status = emit_record(arguments[3], arguments[1]) ? 0 : 1;
    }
    else
    {
        const unsigned int block = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = arguments[2],
                                                            .end = nullptr, .base = 10);
        if ((block == 0u) || (block > PRECEPT_COUNT))
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "a block is 1 to ");
            mmgr_line_decimal(&engine_note.line, (unsigned int)PRECEPT_COUNT, 1u);
            mmgr_line_put(&engine_note.line, ", the tag before precept block - 1");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
            status = 2;
        }
        else
        {
            emit_block(lines, line_count, block, &machine, rules);
        }
    }
    platform_text_ended(&s_bits);
    platform_text_ended(&s_table);
    platform_cell_released(lines, platform_place_g_program_owner);
    return status;
}
