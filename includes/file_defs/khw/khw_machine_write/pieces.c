// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pieces.c: the pieces: a word and a run read, the pieces of the vendor's listing read, what a read contains, a piece's
// reading and name, a form's name and text composed, and a form stored
#include "../../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_machine_write_internal.h"
#include "memoria_operor/memoria_operor.h"

// one piece of a name as the vendor's table gives it
typedef struct
{
    char type[KHW_PIECE];
    char piece[KHW_PIECE];
    char reads[KHW_READS];
} KhwPiece;

static KhwPiece s_piece[KHW_PIECES];
static unsigned int s_pieces;

// the container that the system accepted, and the part where it enters. Then how many registers a thread of it uses,
// the code of the kernel, and how many places it holds
unsigned char s_pattern[KHW_PATTERN_BYTES];

unsigned long long s_pattern_size;
char s_kernel[128];
unsigned int s_registers;
unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
unsigned int s_kernel_places;

// the machine the file contains
SassMachine s_machine;

// one word of an instruction
unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte)
{
    unsigned long long value = 0ULL;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)instruction[byte + at] << (8u * at);
    }
    return value;
}

// the bits `first` to `last` of the instruction `low` and `high` make
static unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first,
                                       unsigned int last)
{
    unsigned long long value = 0ULL;
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        const unsigned long long word = (bit < 64u) ? low : high;
        value |= ((word >> (bit % 64u)) & 1ULL) << (bit - first);
    }
    return value;
}

// the pieces of the vendor's table at `path`, into s_piece. 1, or 0 with the reason printed
int khw_pieces_read(const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    char line[1024];
    while (platform_file_text_read(line, sizeof(line), file) != NULL)
    {
        line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\r\n")] = '\0';
        if ((line[0] == '#') || (line[0] == '\0') || (s_pieces == KHW_PIECES))
        {
            continue;
        }
        // each tab reached through the line itself, which the loop writes
        const char *const piece_found =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
        const char *const reads_found = (piece_found != NULL)
                                            ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = piece_found + 1,
                                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t')
                                            : NULL;
        if (reads_found == NULL)
        {
            continue;
        }
        char *const piece = &line[piece_found - line];
        char *const reads = &line[reads_found - line];
        *piece = '\0';
        *reads = '\0';
        const char *const gloss_found =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = reads + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
        char *const gloss = (gloss_found != NULL) ? &line[gloss_found - line] : NULL;
        if (gloss != NULL)
        {
            *gloss = '\0';
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(s_piece[s_pieces].type, sizeof(s_piece[s_pieces].type));
            mmgr_line_put_max(&buffer_line, line, (size_t)((int)(KHW_PIECE - 1u)));
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(s_piece[s_pieces].piece, sizeof(s_piece[s_pieces].piece));
            mmgr_line_put_max(&buffer_line, piece + 1, (size_t)((int)(KHW_PIECE - 1u)));
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(s_piece[s_pieces].reads, sizeof(s_piece[s_pieces].reads));
            mmgr_line_put_max(&buffer_line, reads + 1, (size_t)((int)(KHW_READS - 1u)));
            (void)mmgr_line_finish(&buffer_line);
        }
        s_pieces += 1u;
    }
    platform_file_close(file);
    if (s_pieces == 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " contains no piece of a name");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// 1 where the reads of `piece` contain `word` whole, and not as part of a longer one
static int khw_reads_contain(const KhwPiece *piece, const char *word)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char *at = piece->reads;
    while ((at = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = word,
                            .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE)) != NULL)
    {
        const int before = (at == piece->reads) || (*(at - 1) == ' ');
        const int after = (at[length] == '\0') || (at[length] == ' ');
        if (before && after)
        {
            return 1;
        }
        at += length;
    }
    return 0;
}

// the piece of type `type` whose reads contain `word`, or NULL where the table gives none
const char *khw_piece_reading(const char *type, const char *word)
{
    for (unsigned int at = 0u; at < s_pieces; at += 1u)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_piece[at].type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = type, .ci = EMBED_FALSE)) &&
            khw_reads_contain(&s_piece[at], word))
        {
            return s_piece[at].piece;
        }
    }
    return NULL;
}

// the piece of type `type` whose own text is `text`, or NULL where the table gives none
static const char *khw_piece_named(const char *type, const char *text)
{
    for (unsigned int at = 0u; at < s_pieces; at += 1u)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_piece[at].type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = type, .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_piece[at].piece, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = text, .ci = EMBED_FALSE)))
        {
            return s_piece[at].piece;
        }
    }
    return NULL;
}

// The name of a form that the part answered `form` of, made from the pieces of the table of the vendor, into
// `written`, which has room for `room`. In order:
//   - the type of value in the cases: the piece whose reads hold the relation, none where no type row reads it.
//   - the answered relation.
//   - how many sources the fields found.
//   - the qualifier that the answers read a word into, where a case of the relation reads two ways.
static void khw_name_composed(const KhwForm *form, char *written, size_t room)
{
    const char *const type = khw_piece_reading("type", form->relation);
    const char *const operation = khw_piece_reading("operation", form->relation);
    written[0] = '\0';
    if (operation == NULL)
    {
        return;
    }
    size_t at = 0u;
    VerbaLinea formed_line_1 = mmgr_line_over(&written[at], room - at);
    mmgr_line_put(&formed_line_1, (type != NULL) ? type : "");
    mmgr_line_put(&formed_line_1, operation);
    (void)mmgr_line_finish(&formed_line_1);
    at += (size_t)(int)formed_line_1.at;
    // the count of sources the part answered fields for, written where the table contains that count for this relation
    char sources[KHW_PIECE];
    {
        VerbaLinea buffer_line = mmgr_line_over(sources, sizeof(sources));
        mmgr_line_decimal(&buffer_line, (form->runs > 0u) ? (form->runs - 1u) : 0u, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    for (unsigned int piece = 0u; piece < s_pieces; piece += 1u)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_piece[piece].type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "count", .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_piece[piece].piece, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = sources, .ci = EMBED_FALSE)) &&
            khw_reads_contain(&s_piece[piece], form->relation))
        {
            VerbaLinea formed_line_2 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_2, sources);
            (void)mmgr_line_finish(&formed_line_2);
            at += (size_t)(int)formed_line_2.at;
            break;
        }
    }
    // a test form carries the test that the relation reads. Then comes only the qualifier of the unsigned read, because
    // the vendor writes a signed comparison with no qualifier. Then comes the join of its flag to another
    const char *const test = khw_piece_reading("test", form->relation);
    if (test != NULL)
    {
        VerbaLinea formed_line_3 = mmgr_line_over(&written[at], room - at);
        mmgr_line_put(&formed_line_3, ".");
        mmgr_line_put(&formed_line_3, test);
        (void)mmgr_line_finish(&formed_line_3);
        at += (size_t)(int)formed_line_3.at;
    }
    if ((form->signedness_queried != 0) && ((test == NULL) || (form->signed_read == 0)))
    {
        const char *const read_as =
            khw_piece_named("qualifier", (form->signed_read != 0) ? KHW_PIECE_SIGNED : KHW_PIECE_UNSIGNED);
        if (read_as != NULL)
        {
            VerbaLinea formed_line_4 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_4, ".");
            mmgr_line_put(&formed_line_4, read_as);
            (void)mmgr_line_finish(&formed_line_4);
            at += (size_t)(int)formed_line_4.at;
        }
    }
    const char *const join = khw_piece_reading("join", form->relation);
    if (join != NULL)
    {
        VerbaLinea formed_line_5 = mmgr_line_over(&written[at], room - at);
        mmgr_line_put(&formed_line_5, ".");
        mmgr_line_put(&formed_line_5, join);
        (void)mmgr_line_finish(&formed_line_5);
        at += (size_t)(int)formed_line_5.at;
    }
    (void)at;
}

// 1 where `run` of `form` is a flag: one bit with no place, which the form's encoding contains and no operand prints
static int khw_flag(const KhwForm *form, unsigned int run)
{
    return (form->place[run] == SASS_RUN_NO_PLACE) && (form->first[run] == form->last[run]);
}

// 1 where `run` of `form` prints no operand of its own. That is a flag, or a run where a run before it is at the same
// place of the tuple of the relation. Two runs of one place are one operand, which the first prints once.
// A wider run with no place prints as its type reads its bits, as a register where the table names nothing for it. Two
// forms that differ in such a run then read apart
static int khw_place_printed(const KhwForm *form, unsigned int run)
{
    if (khw_flag(form, run))
    {
        return 1;
    }
    for (unsigned int before = 0u; before < run; before += 1u)
    {
        if ((form->place[run] != SASS_RUN_NO_PLACE) && (form->place[before] == form->place[run]))
        {
            return 1;
        }
    }
    return 0;
}

// 1 where `run` of `form` is a constant run after the first of the form. The first constant run prints the operand of
// the constant bank once, as the offset. The bank comes from the second run, where the form has one
static int khw_constant_later(const KhwForm *form, unsigned int run)
{
    if (form->type[run] != KHW_TYPE_CONSTANT)
    {
        return 0;
    }
    for (unsigned int before = 0u; before < run; before += 1u)
    {
        if (form->type[before] == KHW_TYPE_CONSTANT)
        {
            return 1;
        }
    }
    return 0;
}

// the bank that the operand of the constant bank of `form` reads: the value of the second constant run. 0 where the
// form has only one constant run, the bank in its encoding
static unsigned long long khw_constant_bank(const KhwForm *form, unsigned int first_run)
{
    for (unsigned int run = first_run + 1u; run < form->runs; run += 1u)
    {
        if (form->type[run] == KHW_TYPE_CONSTANT)
        {
            return khw_run_read(form->low, form->high, form->first[run], form->last[run]);
        }
    }
    return 0ULL;
}

// the field of the instruction word that the .ksc of the part names the guard, as khw_guard_field_set gives it. A
// predicate run inside it is the guard, which prints before the operation. A predicate run outside it is an operand in
// its order
static unsigned int s_guard_first;
static unsigned int s_guard_bits;

void khw_guard_field_set(unsigned int first, unsigned int bits)
{
    s_guard_first = first;
    s_guard_bits = bits;
}

// 1 where `run` of `form` is the guard: a predicate run inside the guard field, or any predicate run where the code
// knows no guard field
static int khw_guard(const KhwForm *form, unsigned int run)
{
    return (form->type[run] == KHW_TYPE_PREDICATE) &&
           ((s_guard_bits == 0u) ||
            ((form->first[run] >= s_guard_first) && (form->first[run] < (s_guard_first + s_guard_bits))));
}

// The runs of `form` that print an operand, in their print order, into `order`, which has room for SASS_MACHINE_RUNS.
// The result is the count.
//
// The guard, a flag, a run of a place that an earlier run prints, and the bank run of a constant print none. The rest
// are in ascending order of their first bits.
//
// Where the table gives the relation an `order` piece, that piece has a character an operand: P a predicate and R any
// other, in that order. Each character takes the next run of its type by its first bit.
static unsigned int khw_printed_runs(const KhwForm *form, unsigned int *order)
{
    unsigned int count = 0u;
    for (unsigned int run = 0u; (run < form->runs) && (count < SASS_MACHINE_RUNS); run += 1u)
    {
        if (!khw_guard(form, run) && !khw_place_printed(form, run) && !khw_constant_later(form, run))
        {
            order[count] = run;
            count += 1u;
        }
    }
    const char *const pattern = khw_piece_reading("order", form->relation);
    if ((pattern == NULL) ||
        (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = pattern, .cap = PLATFORM_TEXT_TO_TERMINATOR) != count))
    {
        return count;
    }
    unsigned int ordered[SASS_MACHINE_RUNS];
    unsigned int taken[SASS_MACHINE_RUNS] = {0u};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        const int predicate = (pattern[each] == 'P');
        unsigned int found = count;
        for (unsigned int at = 0u; (at < count) && (found == count); at += 1u)
        {
            const int type_matches = (form->type[order[at]] == KHW_TYPE_PREDICATE) == predicate;
            found = (!taken[at] && type_matches) ? at : found;
        }
        if (found == count)
        {
            // the pattern requests a type the form has no more runs of: the bit order is
            return count;
        }
        taken[found] = 1u;
        ordered[each] = order[found];
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = order, .src = ordered, .bytes = count * sizeof(order[0]));
    return count;
}

// The text of a form that the part answered `form` of. Each run reads from the encoding through its own run, by its
// type. In order:
//   - a predicate run inside the guard field, as the vendor writes a guard: `@P<n>` before the operation. It is
//     `@!P<n>` where the bit past the three of the predicate is 1, and nothing where it is every predicate.
//   - its name.
//   - each other run in ascending order of its first bit, the print order. The runs of one place are one
//     operand, which the first prints.
//
// The operand types:
//   - a register `R<n>`, or the name under which the tree writes a register that the part answers nothing at.
//   - a predicate `P<n>`, or `PT` where it is every predicate.
//   - an address `[R<n>.64]`.
//   - a number `0x<n>`.
//   - the operand of the constant bank, `c[0x<bank>][0x<offset>]`, which its offset run prints once.
void khw_text_composed(const KhwForm *form, char *written, size_t room)
{
    char name[KHW_NAME];
    khw_name_composed(form, name, sizeof(name));
    written[0] = '\0';
    if (name[0] == '\0')
    {
        return;
    }
    size_t at = 0u;
    for (unsigned int run = 0u; (run < form->runs) && (at < room); run += 1u)
    {
        if (!khw_guard(form, run) || khw_place_printed(form, run))
        {
            continue;
        }
        const unsigned long long guard = khw_run_read(form->low, form->high, form->first[run], form->last[run]);
        const unsigned long long predicate = guard & 7ULL;
        if (predicate != 7ULL)
        {
            VerbaLinea formed_line_6 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_6, "@");
            mmgr_line_put(&formed_line_6, ((guard & 8ULL) != 0ULL) ? "!" : "");
            mmgr_line_put(&formed_line_6, "P");
            mmgr_line_decimal(&formed_line_6, predicate, 1u);
            mmgr_line_put(&formed_line_6, " ");
            (void)mmgr_line_finish(&formed_line_6);
            at += (size_t)(int)formed_line_6.at;
        }
    }
    VerbaLinea formed_line_7 = mmgr_line_over(&written[at], room - at);
    mmgr_line_put(&formed_line_7, name);
    (void)mmgr_line_finish(&formed_line_7);
    at += (size_t)(int)formed_line_7.at;
    // the mark the vendor writes before the operand at the relation's second word, where the table gives one
    const char *const marked = khw_piece_reading("mark", form->relation);
    unsigned int printed = 0u;
    unsigned int order[SASS_MACHINE_RUNS];
    const unsigned int operands = khw_printed_runs(form, order);
    for (unsigned int each = 0u; (each < operands) && (at < room); each += 1u)
    {
        const unsigned int run = order[each];
        const unsigned long long number = khw_run_read(form->low, form->high, form->first[run], form->last[run]);
        const char *const mark = ((marked != NULL) && (form->place[run] == 1u)) ? marked : "";
        char between[8];
        {
            VerbaLinea buffer_line = mmgr_line_over(between, sizeof(between));
            mmgr_line_put(&buffer_line, (printed == 0u) ? " " : ", ");
            mmgr_line_put(&buffer_line, mark);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (form->type[run] == KHW_TYPE_PREDICATE)
        {
            VerbaLinea formed_line_70;
            VerbaLinea formed_line_71;
            ((number & 7ULL) == 7ULL)
                ? (at += (size_t)(formed_line_70 = mmgr_line_over(&written[at], room - at),
                                  mmgr_line_put(&formed_line_70, between), mmgr_line_put(&formed_line_70, "PT"),
                                  (void)mmgr_line_finish(&formed_line_70), (int)formed_line_70.at))
                : (at += (size_t)(formed_line_71 = mmgr_line_over(&written[at], room - at),
                                  mmgr_line_put(&formed_line_71, between), mmgr_line_put(&formed_line_71, "P"),
                                  mmgr_line_decimal(&formed_line_71, number & 7ULL, 1u),
                                  (void)mmgr_line_finish(&formed_line_71), (int)formed_line_71.at));
        }
        else if (form->type[run] == KHW_TYPE_CONSTANT)
        {
            VerbaLinea formed_line_8 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_8, between);
            mmgr_line_put(&formed_line_8, "c[0x");
            mmgr_line_hex(&formed_line_8, khw_constant_bank(form, run), 1u);
            mmgr_line_put(&formed_line_8, "][0x");
            mmgr_line_hex(&formed_line_8, number, 1u);
            mmgr_line_put(&formed_line_8, "]");
            (void)mmgr_line_finish(&formed_line_8);
            at += (size_t)(int)formed_line_8.at;
        }
        else if (form->type[run] == KHW_TYPE_IMMEDIATE)
        {
            VerbaLinea formed_line_9 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_9, between);
            mmgr_line_put(&formed_line_9, "0x");
            mmgr_line_hex(&formed_line_9, number, 1u);
            (void)mmgr_line_finish(&formed_line_9);
            at += (size_t)(int)formed_line_9.at;
        }
        else if (form->type[run] == KHW_TYPE_ADDRESS)
        {
            VerbaLinea formed_line_10 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_10, between);
            mmgr_line_put(&formed_line_10, "[R");
            mmgr_line_decimal(&formed_line_10, number, 1u);
            mmgr_line_put(&formed_line_10, ".64]");
            (void)mmgr_line_finish(&formed_line_10);
            at += (size_t)(int)formed_line_10.at;
        }
        else if (number == KHW_REGISTER_NONE)
        {
            VerbaLinea formed_line_11 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_11, between);
            mmgr_line_put(&formed_line_11, KHW_REGISTER_NONE_TEXT);
            (void)mmgr_line_finish(&formed_line_11);
            at += (size_t)(int)formed_line_11.at;
        }
        else
        {
            VerbaLinea formed_line_12 = mmgr_line_over(&written[at], room - at);
            mmgr_line_put(&formed_line_12, between);
            mmgr_line_put(&formed_line_12, "R");
            mmgr_line_decimal(&formed_line_12, number, 1u);
            (void)mmgr_line_finish(&formed_line_12);
            at += (size_t)(int)formed_line_12.at;
        }
        printed += 1u;
    }
}

// `form` stored in `machine` as a form under `text`, with the runs the part answered for its operands. 1, or 0 where
// the machine is full
int khw_form_store(SassMachine *machine, const KhwForm *form, const char *text)
{
    SassForm *stored = NULL;
    if (!sass_machine_take_related(machine, text, form->low, form->high, form->relation, &stored) || (stored == NULL))
    {
        return 0;
    }
    // the name of the relation fits the room of the form, and the code cuts a longer name to it
    {
        VerbaLinea buffer_line = mmgr_line_over(stored->relation, sizeof(stored->relation));
        mmgr_line_put_max(&buffer_line, form->relation, (size_t)((int)(sizeof(stored->relation) - 1u)));
        (void)mmgr_line_finish(&buffer_line);
    }
    stored->signed_read = form->signed_read;
    stored->runs = 0u;
    // the operand of each run is its order among the printed runs. The runs of one place take the operand of the first.
    // A predicate run inside the guard field is the guard, which prints before the operation and is no operand. A flag
    // prints none. The code stores each of these two under the operand count as its mark
    unsigned int order[SASS_MACHINE_RUNS];
    const unsigned int operands = khw_printed_runs(form, order);
    unsigned int operand_of[SASS_MACHINE_RUNS];
    for (unsigned int run = 0u; (run < form->runs) && (stored->runs < SASS_MACHINE_RUNS); run += 1u)
    {
        // the operand this run prints, where it prints one, as the text orders them
        unsigned int operand = SASS_MACHINE_OPERANDS;
        for (unsigned int each = 0u; each < operands; each += 1u)
        {
            operand = (order[each] == run) ? each : operand;
        }
        // a run of a place an earlier run prints shares that run's operand
        for (unsigned int before = 0u; before < run; before += 1u)
        {
            operand = ((form->place[run] != SASS_RUN_NO_PLACE) && (form->place[before] == form->place[run]))
                          ? operand_of[before]
                          : operand;
        }
        operand_of[run] = operand;
        stored->run[stored->runs].operand = operand;
        stored->run[stored->runs].first = form->first[run];
        stored->run[stored->runs].last = form->last[run];
        stored->run[stored->runs].place = form->place[run];
        stored->runs += 1u;
    }
    return 1;
}
