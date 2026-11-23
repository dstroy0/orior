// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// identity_internal.h: what the modules of orior_klq_identity share: its includes, every define and struct, the const
// tables, and the state and functions one module reads from another
#ifndef IDENTITY_INTERNAL_H
#define IDENTITY_INTERNAL_H

#include "../../../../../../../includes/file_defs/kdm/kdm.h"
#include "../../../../../../../includes/file_defs/kernel_blob/forms_blob.h"
#include "../../../../../../../includes/file_defs/kernel_blob/kernel_blob.h"
#include "../../../../../../../includes/file_defs/klq/query_trace.h"
#include "../../../../../../../includes/file_defs/kqr/kqr_record.h"
#include "../../../../../../../includes/file_defs/krf/krf.h"
#include "../../../../../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../../../../../../includes/file_defs/ksc/ksc.h"
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../interface/interface.h"
#include "../../overseer/overseer.h"
#include "../../teacher/run_channel.h"
#include "../carrier_flow.h"
#include "../concept_product.h"
#include "../table_draw.h"
#include "../table_regexp.h"

#include "code_generator.h"
#include "embed_atomic.h"
#include "embed_turnstile.h"
#include "memoria_operor/memoria_operor.h"
#include "precepts.h"
#include "target_internal.h"
#include "word_web.h"

#include <limits.h>
#include <stddef.h>

#include "../../../../../../../includes/parsers/who.h"
#include "identity_settings.h"

// the values each operand goes through: zero, the small numbers, and the edges of every width
static const unsigned long long s_values[] = {0x0ULL,
                                              0x1ULL,
                                              0x2ULL,
                                              0x3ULL,
                                              0x7fULL,
                                              0x80ULL,
                                              0xffULL,
                                              0x100ULL,
                                              0x7fffULL,
                                              0x8000ULL,
                                              0xffffULL,
                                              0x7fffffffULL,
                                              0x80000000ULL,
                                              0xffffffffULL,
                                              0x100000000ULL,
                                              0x7fffffffffffffffULL,
                                              0x8000000000000000ULL,
                                              0xffffffffffffffffULL};

#define IDENTITY_VALUES (sizeof(s_values) / sizeof(s_values[0]))

// the third operand, a test's other predicate where a question reads one
#define IDENTITY_THIRDS 2u

#define IDENTITY_CASES (IDENTITY_VALUES * IDENTITY_VALUES * IDENTITY_THIRDS)

// the blob of the run that holds the host's answers, host_questions' output, one chain a line
#define IDENTITY_HOST_ANSWERS "host_answers.txt"

// the bytes a text of the temporary tier grows to at max, and a text of the movable tier
#define IDENTITY_TEXT_MAX (1ULL << 26u)
#define IDENTITY_KEPT_MAX (1ULL << 30u)

// ---- names ----

// The bytes of a short name, its terminator among them: a question's number or category, a link's position, a
// register, a form's name. A record holds its names inline, as EmitName holds one
#define IDENTITY_NAME_BYTES 64u

// the bytes of an instruction's text, its terminator among them. The stick's longest instruction is 63 bytes
#define IDENTITY_LINE_BYTES 128u

// the bytes of a question's C, its terminator among them. The stick's longest is 227 bytes
#define IDENTITY_C_BYTES 256u

// a short name, terminated
struct IdentityName
{
    char text[IDENTITY_NAME_BYTES];
};

// an instruction's text, terminated
struct IdentityLine
{
    char text[IDENTITY_LINE_BYTES];
};

// The `length` bytes of `from`, cut at a NUL where one comes earlier, as the whole of `to` of `room` bytes,
// terminated. 1, or 0 where they do not fit, and `to` is then empty
static inline int identity_name_set(char *to, size_t room, const char *from, size_t length)
{
    const size_t bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = from, .cap = length);
    if (bytes >= room)
    {
        to[0] = '\0';
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = to, .src = from, .bytes = bytes);
    to[bytes] = '\0';
    return 1;
}

#define IDENTITY_SET(to_, from_, length_) identity_name_set((to_).text, sizeof((to_).text), (from_), (length_))

// the terminated `from` as the whole of `to`, as identity_name_set sets it
#define IDENTITY_SET_TEXT(to_, from_)                                                                                  \
    identity_name_set((to_).text, sizeof((to_).text), (from_), PLATFORM_TEXT_TO_TERMINATOR)

// The order of two terminated texts as their bytes order: below 0 where `left` comes first, 0 where they are the same,
// above 0 where `right` does. A text that the other begins with comes first
static inline int identity_order_text(const char *left, const char *right)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t right_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const int bytes = EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right,
                                 .bytes = (left_length < right_length) ? left_length : right_length);
    return (bytes != 0) ? bytes : ((left_length < right_length) ? -1 : ((left_length > right_length) ? 1 : 0));
}

// 1 where two terminated texts hold the same bytes
static inline int identity_same(const char *left, const char *right)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = right,
                      .ci = EMBED_FALSE);
}

// ---- order ----

// The `count` elements at `elements` sorted by `before`, which is 1 where its first element goes before its second.
// Elements that neither goes before keep their order, as std::stable_sort keeps them. The merge takes one cell of the
// temporary tier as large as the elements, and gives it back. 1, or 0 where the pool cannot meet that cell, and the
// elements are then as they were
template <typename T, typename Before>
static inline int identity_sorted(T *elements, unsigned long long count, const Before &before)
{
    if (count < 2ULL)
    {
        return 1;
    }
    T *const spare =
        (T *)platform_cell_taken(count * sizeof(T), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (spare == NULL)
    {
        return 0;
    }
    T *from = elements;
    T *to = spare;
    for (unsigned long long width = 1ULL; width < count; width *= 2ULL)
    {
        for (unsigned long long left = 0ULL; left < count; left += 2ULL * width)
        {
            const unsigned long long middle = ((left + width) < count) ? (left + width) : count;
            const unsigned long long right = ((left + (2ULL * width)) < count) ? (left + (2ULL * width)) : count;
            unsigned long long one = left;
            unsigned long long other = middle;
            for (unsigned long long put = left; put < right; put += 1ULL)
            {
                const int take_other = (other < right) && ((one >= middle) || before(from[other], from[one]));
                if (take_other)
                {
                    to[put] = from[other];
                    other += 1ULL;
                }
                else
                {
                    to[put] = from[one];
                    one += 1ULL;
                }
            }
        }
        T *const merged = to;
        to = from;
        from = merged;
    }
    if (from != elements)
    {
        // the elements are at max the pool's bytes, which a size_t counts
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = elements, .src = from, .bytes = (size_t)(count * sizeof(T)));
    }
    platform_cell_released(spare, platform_place_g_program_owner);
    return 1;
}

// The element at `root` moved down the heap of the first `end` elements at `elements` until neither child goes
// after it by `before`
template <typename T, typename Before>
static inline void identity_sifted(T *elements, unsigned long long root, unsigned long long end, const Before &before)
{
    for (;;)
    {
        const unsigned long long left = (2ULL * root) + 1ULL;
        if (left >= end)
        {
            return;
        }
        const unsigned long long right = left + 1ULL;
        const unsigned long long child = ((right < end) && before(elements[left], elements[right])) ? right : left;
        if (!before(elements[root], elements[child]))
        {
            return;
        }
        const T moved = elements[root];
        elements[root] = elements[child];
        elements[child] = moved;
        root = child;
    }
}

// The `count` elements at `elements` sorted in place by `before`, as a heap sorts them, with no cell taken. `before`
// must order every two elements that are not the same, since the sort keeps no order of its own
template <typename T, typename Before>
static inline void identity_sorted_in_place(T *elements, unsigned long long count, const Before &before)
{
    for (unsigned long long parent = count / 2ULL; parent > 0ULL; parent -= 1ULL)
    {
        identity_sifted(elements, parent - 1ULL, count, before);
    }
    for (unsigned long long end = count; end > 1ULL; end -= 1ULL)
    {
        const T top = elements[0];
        elements[0] = elements[end - 1ULL];
        elements[end - 1ULL] = top;
        identity_sifted(elements, 0ULL, end - 1ULL, before);
    }
}

// The `count` names at `names` sorted in place by their bytes, each once, as a std::set holds them. The answer is the
// count kept
static inline unsigned long long identity_names_settled(IdentityName *names, unsigned long long count)
{
    identity_sorted_in_place(names, count, [](const IdentityName &one, const IdentityName &other) {
        return identity_order_text(one.text, other.text) < 0;
    });
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if ((kept == 0ULL) || !identity_same(names[kept - 1ULL].text, names[at].text))
        {
            names[kept] = names[at];
            kept += 1ULL;
        }
    }
    return kept;
}

// the `count` numbers at `numbers` sorted in place, each once, as a std::set holds them. The answer is the count kept
static inline unsigned long long identity_numbers_settled(unsigned long long *numbers, unsigned long long count)
{
    identity_sorted_in_place(numbers, count,
                             [](unsigned long long one, unsigned long long other) { return one < other; });
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if ((kept == 0ULL) || (numbers[kept - 1ULL] != numbers[at]))
        {
            numbers[kept] = numbers[at];
            kept += 1ULL;
        }
    }
    return kept;
}

// The first place among the `count` sorted elements at `elements` where `below` is 0. `below` is 1 where an element
// goes before the key the search looks for. The answer is `count` where every element goes before it
template <typename T, typename Below>
static inline unsigned long long identity_lower(const T *elements, unsigned long long count, const Below &below)
{
    unsigned long long low = 0ULL;
    unsigned long long high = count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        if (below(elements[middle]))
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// ---- texts ----

// a text of the temporary tier, begun empty, for a text a call builds and gives back before it returns
static inline void identity_text_begun(PlatformText *text)
{
    platform_text_begun(text, IDENTITY_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

// a text of the movable tier, begun empty, for a text that grows large and lasts past the call that begins it
static inline void identity_kept_begun(PlatformText *text)
{
    platform_text_begun(text, IDENTITY_KEPT_MAX, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
}

// the bytes of `from` put after the bytes of `to`
static inline void identity_text_put(PlatformText *to, PlatformText *from)
{
    const unsigned long long size = platform_text_size(from);
    const char *const bytes = platform_text_bytes(from);
    if (bytes != NULL)
    {
        (void)mmgr_line_put_n(&to->line, bytes, (size_t)size);
    }
}

// The bytes of `text` as the whole of `to` of `room` bytes, terminated, and the text given back. 1, or 0 where they
// do not fit or the pool cannot meet the read, and `to` is then empty
static inline int identity_text_kept(PlatformText *text, char *to, size_t room)
{
    const unsigned long long size = platform_text_size(text);
    const char *const bytes = platform_text_bytes(text);
    // a text that fits a record's name is far below what a size_t counts
    const int kept = (bytes != NULL) && identity_name_set(to, room, bytes, (size_t)size);
    to[0] = kept ? to[0] : '\0';
    platform_text_ended(text);
    return kept;
}

#define IDENTITY_KEPT(text_, to_) identity_text_kept((text_), (to_).text, sizeof((to_).text))

// `text` with each match of `pattern` replaced by `replacement`, in place
static inline void identity_replaced(PlatformText *text, const TablePattern *pattern, const char *replacement)
{
    PlatformText replaced;
    identity_text_begun(&replaced);
    const unsigned long long size = platform_text_size(text);
    const char *const bytes = platform_text_bytes(text);
    table_replaced(pattern, (bytes != NULL) ? bytes : "", (size_t)size, replacement, &replaced);
    platform_text_reset(text);
    identity_text_put(text, &replaced);
    platform_text_ended(&replaced);
}

// 1 where the bytes of `piece` are the terminated `text`
static inline int identity_piece_is(CarrierPiece piece, const char *text)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length == length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = text, .bytes = length) == 0);
}

// the order of two pieces as their bytes order, as identity_order_text orders two texts
static inline int identity_piece_order(CarrierPiece left, CarrierPiece right)
{
    const size_t shorter = (left.length < right.length) ? left.length : right.length;
    const int bytes = EMBED_CALL(memor.cmp, MemoriaCfg, .src = left.text, .other = right.text, .bytes = shorter);
    return (bytes != 0) ? bytes : ((left.length < right.length) ? -1 : ((left.length > right.length) ? 1 : 0));
}

// 1 where two pieces hold the same bytes
static inline int identity_piece_same(CarrierPiece left, CarrierPiece right)
{
    return (left.length == right.length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left.text, .other = right.text, .bytes = left.length) == 0);
}

// 1 where the terminated `text` holds `needle`
static inline int identity_has(const char *text, const char *needle)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = needle,
                      .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL;
}

// the file at `path` read whole into a cell of the temporary tier, terminated, its bytes into `size`. NULL where it
// does not read. The caller gives the cell back [RETURNS OWNERSHIP]
static inline char *identity_file_read(const char *path, size_t *size)
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

// The next line of the text of a file from `*at` into `line`, and `*at` past its end: 0 where the text holds
// no line past `*at`. A line
// reads as std::getline reads it from a stream of text mode. On Windows that stream reads a carriage return before a
// line feed as part of the line end
static inline int identity_line_next(CarrierPiece text, size_t *at, CarrierPiece *line)
{
    if (!carrier_piece_next(text, '\n', at, line))
    {
        return 0;
    }
#if defined(_WIN32)
    // the line ended at a line feed, which lies at *at less one
    const int fed = (*at <= text.length);
    if (fed && (line->length != 0u) && (line->text[line->length - 1u] == '\r'))
    {
        line->length -= 1u;
    }
#endif
    return 1;
}

// The bytes of a file read whole as a stream of text mode reads them, in place, with the count into `size`. On
// Windows that stream reads a carriage return before a line feed as the line feed alone
static inline void identity_text_mode(char *text, size_t *size)
{
#if defined(_WIN32)
    size_t kept = 0u;
    for (size_t at = 0u; at < *size; at += 1u)
    {
        const int dropped = (text[at] == '\r') && ((at + 1u) < *size) && (text[at + 1u] == '\n');
        if (!dropped)
        {
            text[kept] = text[at];
            kept += 1u;
        }
    }
    text[kept] = '\0';
    *size = kept;
#else
    (void)text;
    (void)size;
#endif
}

// 1 where `character` is a space as a stream's extraction skips it
static inline int identity_space(char character)
{
    return (character == ' ') || (character == '\t') || (character == '\n') || (character == '\r') ||
           (character == '\v') || (character == '\f');
}

// The next word of `text` from `*at` into `word`, and `*at` past it, as a stream's extraction reads a word: 0
// where the text holds no word past `*at`
static inline int identity_word_next(CarrierPiece text, size_t *at, CarrierPiece *word)
{
    while ((*at < text.length) && identity_space(text.text[*at]))
    {
        *at += 1u;
    }
    if (*at >= text.length)
    {
        return 0;
    }
    const size_t first = *at;
    while ((*at < text.length) && !identity_space(text.text[*at]))
    {
        *at += 1u;
    }
    word->text = &text.text[first];
    word->length = *at - first;
    return 1;
}

// a path of a file, terminated
struct IdentityPath
{
    char text[PLATFORM_PATH_BYTES];
};

// the terminated `folder` with the terminated `tail` after it, cut where the path's room ends
static inline IdentityPath identity_path_joined(const char *folder, const char *tail)
{
    IdentityPath path;
    VerbaLinea path_line = mmgr_line_over(path.text, sizeof(path.text));
    mmgr_line_put(&path_line, folder);
    mmgr_line_put(&path_line, tail);
    (void)mmgr_line_finish(&path_line);
    return path;
}

// a line on the output stream: two spaces, `path`, then `tail`
static inline void identity_path_said(const char *path, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, path);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// What a mode writes, which memory holds until the run's writer takes it whole. The writer is the only thing
// that writes a file. It holds where the text goes and its bytes so far. `filed` is 1 where it is a current file
// of the tree, which the writer writes in its place (qry_member_filed). It is 0 where it is a table of the run's
// own, a blob of the run named by the path's own name
struct RunWritten
{
    IdentityPath path;
    int filed;
    PlatformText text;
};

// the written begun at the terminated `path`, empty, `filed` as RunWritten holds it
static inline void run_written_begun(RunWritten *written, const char *path, int filed)
{
    (void)IDENTITY_SET_TEXT(written->path, path);
    written->filed = filed;
    identity_kept_begun(&written->text);
}

static inline void run_written_ended(RunWritten *written)
{
    platform_text_ended(&written->text);
}

struct Ruleset;

// The ruleset the identity writes forms in: the latest IDENTITY_RULESET of the run, as its code generator reads
// it. NULL where the run names none or the code generator reads none
static inline const Ruleset *identity_ruleset(void)
{
    static char s_path[IDENTITY_PATH_MAX_LEN];
    const unsigned char *named = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_blob(IDENTITY_RULESET, &named, &size, NULL) || (size == 0ULL) || (size >= sizeof(s_path)))
    {
        return NULL;
    }
    // a path below IDENTITY_PATH_MAX_LEN bytes, which a size_t counts
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_path, .src = named, .bytes = (size_t)size);
    s_path[size] = '\0';
    return code_generator(s_path).ruleset(1);
}

// the seed that the order of a magnitude's links comes from: the latest IDENTITY_SEED of the run, and 1 where it
// names none
static inline unsigned long identity_seed(void)
{
    const unsigned char *seed_bytes = NULL;
    unsigned long long size = 0ULL;
    unsigned long long seed = 1ULL;
    if (qry_member_blob(IDENTITY_SEED, &seed_bytes, &size, NULL) && (size == sizeof(seed)))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &seed, .src = seed_bytes, .bytes = sizeof(seed));
    }
    // a seed the crate gives fits the generator's word
    return (unsigned long)seed;
}

// ---- the links and the questions ----

// The parts of a link at max: the registers it names, and its literals, targets and distances each. An instruction
// of the stick names at max 6 registers, and writes 2 literals
#define LINK_PARTS_MAX 8u
#define LINK_LITERALS_MAX 4u

// The bytes of a register's name (UR63, R255, P6) and of a placeholder (g, or an operand's count), each terminator
// among them. The bytes of a literal, a target or a distance, each terminator among them. The stick's longest word is
// 33 bytes
#define LINK_REGISTER_BYTES 8u
#define LINK_PLACEHOLDER_BYTES 4u
#define LINK_WORD_BYTES 40u

// a register a link names, with the placeholder it names it in: g for the guard, and each operand by its count from 0
struct LinkPlaceholder
{
    char placeholder[LINK_PLACEHOLDER_BYTES];
    char name[LINK_REGISTER_BYTES];
};

// a register's name a link names, terminated
struct LinkRegister
{
    char text[LINK_REGISTER_BYTES];
};

// a literal, a target or a distance a link writes, terminated
struct LinkWord
{
    char text[LINK_WORD_BYTES];
};

// A link of a listing. It holds the link as nvcc wrote it, its registers as written, and the link with its
// registers read as their type. Then it holds its position in the chain, the address nvcc wrote beside it, and
// each register it names with its placeholder. Then it holds the link with each register, literal and target a
// mark. Last come the registers, the literals, the targets and the targets' distances in the order written. Every
// part lies inside the link, with its count beside it
struct Link
{
    IdentityLine exact;
    IdentityLine registers;
    IdentityLine type;
    IdentityName position;
    LinkPlaceholder placeholders[LINK_PARTS_MAX];
    unsigned int placeholder_count;
    IdentityLine marked;
    LinkRegister named[LINK_PARTS_MAX];
    unsigned int named_count;
    LinkWord literals[LINK_LITERALS_MAX];
    unsigned int literal_count;
    LinkWord targets[LINK_LITERALS_MAX];
    unsigned int target_count;
    LinkWord distances[LINK_LITERALS_MAX];
    unsigned int distance_count;
};

// One question of the stick: its number, its category and C, and its chain, every link in the order nvcc wrote it.
// The chain lies in the cell of the listing's links [BORROWS]. `by_exact` holds the places of the chain's links
// ordered by their exact text, and a run of one text by place. A run's length is the count of the text, and its
// first place the text's first occurrence. It lies in the listing's cell of places [BORROWS]
struct Question
{
    IdentityName number;
    IdentityName category;
    char text[IDENTITY_C_BYTES];
    Link *chain;
    unsigned long long links;
    unsigned long long *by_exact;
};

// The questions of one listing, in the order of their numbers. Each is a resource of the run, in three cells that
// the reader takes once at their counts. The questions, every link of the listing, and the places of every chain
// ordered by exact text. questions_ended gives the three back
struct Questions
{
    Question *question;
    unsigned long long count;
    Link *links;
    unsigned long long *places;
};

// the questions' three cells given back, and the questions empty
static inline void questions_ended(Questions *questions)
{
    platform_cell_released(questions->question, platform_place_g_program_owner);
    platform_cell_released(questions->links, platform_place_g_program_owner);
    platform_cell_released(questions->places, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = questions, .bytes = sizeof(*questions), .val = 0);
}

// The run of `exact` in the places of `question` ordered by exact text: its first entry into `first`, and its length.
// 0 where the chain holds no link of that text
static inline unsigned long long question_exact_run(const Question *question, const char *exact,
                                                    unsigned long long *first)
{
    const Link *const chain = question->chain;
    const unsigned long long at =
        identity_lower(question->by_exact, question->links, [chain, exact](unsigned long long place) {
            return identity_order_text(chain[place].exact.text, exact) < 0;
        });
    unsigned long long end = at;
    while ((end < question->links) && identity_same(chain[question->by_exact[end]].exact.text, exact))
    {
        end += 1ULL;
    }
    *first = at;
    return end - at;
}

// the first occurrence of a chain identity, the question and the link it starts at, and how often it occurs
struct Occurrence
{
    unsigned int question;
    unsigned int link;
    unsigned int count;
};

// one window of a chain: its identity, and the question and the link it starts at
struct Window
{
    unsigned long long identity;
    unsigned int question;
    unsigned int link;
};

// Every window of every chain of a listing, in one cell of the temporary tier that windows_counted takes once at
// their count. Identity orders them, then question, then link. A run of one identity counts it, and its first entry
// is its first occurrence. windows_ended gives the cell back
struct Windows
{
    Window *window;
    unsigned long long count;
};

static inline void windows_ended(Windows *windows)
{
    platform_cell_released(windows->window, platform_place_g_program_owner);
    windows->window = NULL;
    windows->count = 0ULL;
}

// The code of a question or a chain, sixteen bytes an instruction: its bytes, their count the code's size. A code
// that ends in the call that makes it is on the temporary tier
typedef PlatformArray IdentityCode;

// a code begun empty, on the temporary tier where `ephemeral` is 1
static inline void identity_code_begun(IdentityCode *code, int ephemeral)
{
    platform_array_begun(code, 1ULL, platform_place_g_program_owner, ephemeral);
}

// `to` made a copy of `from`, on the tier of `to`: 1, or 0 where the pool cannot meet the room
static inline int identity_code_copied(IdentityCode *to, const IdentityCode *from)
{
    const unsigned long long count = from->count;
    if (!platform_array_counted(to, count))
    {
        return 0;
    }
    if (count != 0ULL)
    {
        // a code is at max the pool's bytes, which a size_t counts
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = to->cell, .src = from->cell, .bytes = (size_t)count);
    }
    return 1;
}

// The host's answers to the cases of one chain, one a case in the order of the cases. Each answer is a terminated word
// of the blob that HostAnswers holds, or a text the caller holds [BORROWS]. A case past `count` has no answer
struct HostCases
{
    IdentityName number;
    const char *answer[IDENTITY_CASES];
    unsigned int count;
};

// the answer of case `at`, and - where the chain has none
static inline const char *host_case(const HostCases *cases, unsigned long long at)
{
    return (at < cases->count) ? cases->answer[at] : "-";
}

// Every chain's answers, ordered by number. The blob they lie in and the chains are a resource of the run. Each one
// cell of the persistent tier taken once at its count. host_answers_ended gives the two back
struct HostAnswers
{
    char *text;
    HostCases *chain;
    unsigned long long count;
};

static inline void host_answers_ended(HostAnswers *host)
{
    platform_cell_released(host->chain, platform_place_g_program_owner);
    platform_cell_released(host->text, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = host, .bytes = sizeof(*host), .val = 0);
}

// the answers of the chain whose number is the `length` bytes of `number`, or NULL where the host gives none
static inline const HostCases *host_cases_of_bytes(const HostAnswers *host, const char *number, size_t length)
{
    const unsigned long long at = identity_lower(host->chain, host->count, [number, length](const HostCases &one) {
        const size_t own =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = one.number.text, .cap = sizeof(one.number.text));
        const size_t shorter = (own < length) ? own : length;
        const int bytes = EMBED_CALL(memor.cmp, MemoriaCfg, .src = one.number.text, .other = number, .bytes = shorter);
        return (bytes < 0) || ((bytes == 0) && (own < length));
    });
    const int found =
        (at < host->count) &&
        (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = host->chain[at].number.text,
                    .cap = sizeof(host->chain[at].number.text)) == length) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = host->chain[at].number.text, .other = number, .bytes = length) == 0);
    return found ? &host->chain[at] : NULL;
}

// the answers of the chain whose number is the terminated `number`, or NULL where the host gives none
static inline const HostCases *host_cases_of(const HostAnswers *host, const char *number)
{
    return host_cases_of_bytes(
        host, number, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = number, .cap = PLATFORM_TEXT_TO_TERMINATOR));
}

// a field of an instruction as the system's .ksc gives it: its first bit and how many bits it takes
struct StallField
{
    unsigned int first;
    unsigned int bits;
};

// The record of the run channel's queries, the part's .ksc rows
//
//     run <answer> <word> query <code> <registers> <threads> <blocks> <cases> <host> [<refusal>]
//
// Each query goes by the hashes of its code, of its cases and of the host's answers to them. The registers and the
// shape it goes with also name it, and together these fix what it comes to. The word is the host's place of the
// first case apart, or ffffffff where every case is alike. It is 0 where the part refused the query or left
// nothing.
//
// The part does not get a query the record contains again. A timed query goes to the part every time, each time a
// sample of its own. A query the gate blocks is the host's, and no record holds it. A mode reads the record when it
// opens the run channel, and writes it back when it ends
#define RECORD_KEY_BYTES 160u
#define RECORD_ANSWER_BYTES 16u
#define RECORD_REFUSAL_BYTES 128u

struct QueryRecorded
{
    char key[RECORD_KEY_BYTES];
    char answer[RECORD_ANSWER_BYTES];
    unsigned int word;
    char refusal[RECORD_REFUSAL_BYTES];
};

// The record: every query by its key, a PlatformArray of QueryRecorded on the persistent tier. The first `sorted` are
// the rows the .ksc gave, ordered by key, and the rest the queries the run stored after, in the order stored
struct QueryRecords
{
    PlatformArray records;
    unsigned long long sorted;
};

// The query whose key is the `length` bytes of `key` in the record, or NULL where it holds none [BORROWS]. A later
// store can move it
static inline QueryRecorded *query_recorded(QueryRecords *record, const char *key, size_t length)
{
    QueryRecorded *const records = (QueryRecorded *)record->records.cell;
    const unsigned long long at = identity_lower(records, record->sorted, [key, length](const QueryRecorded &one) {
        const size_t own = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = one.key, .cap = sizeof(one.key));
        const size_t shorter = (own < length) ? own : length;
        const int bytes = EMBED_CALL(memor.cmp, MemoriaCfg, .src = one.key, .other = key, .bytes = shorter);
        return (bytes < 0) || ((bytes == 0) && (own < length));
    });
    if ((at < record->sorted) &&
        (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = records[at].key, .cap = sizeof(records[at].key)) == length) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = records[at].key, .other = key, .bytes = length) == 0))
    {
        return &records[at];
    }
    for (unsigned long long each = record->records.count; each > record->sorted; each -= 1ULL)
    {
        QueryRecorded *const one = &records[each - 1ULL];
        if ((EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = one->key, .cap = sizeof(one->key)) == length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->key, .other = key, .bytes = length) == 0))
        {
            return one;
        }
    }
    return NULL;
}

// ksc_answers_written, the who the code that calls it
#define ksc_answers_write(ksc_, queried_, rows_, count_)                                                               \
    ksc_answers_written((ksc_), (queried_), (rows_), (count_), WHO_HERE)

// ---- the queries ----

// A log the query writes. Memory holds it until the run takes it whole as a blob of its .qry (qry_member.h). It
// holds the name of its blobs, the bytes not yet passed to the run, and 1 once its text begins
struct QueryLog
{
    const char *name;
    PlatformText bytes;
    int begun;
};

// the line of `log`, its text begun on the first write
static inline VerbaLinea *query_log_line(QueryLog *log)
{
    if (log->begun == 0)
    {
        identity_kept_begun(&log->bytes);
        log->begun = 1;
    }
    return &log->bytes.line;
}

// One query a pair's thread passes to the round, 1 where it is dangerous. Its stand-in reads or writes memory
// through an address, or sends control elsewhere. A process of its own carries it
struct CarriedQuery
{
    RunQuestion *question;
    int dangerous;
};

// the queries a pair's thread passes to the round, a PlatformArray of CarriedQuery
typedef PlatformArray CarriedQueries;

// the order of two identities as numbers: below 0 where `one` comes first, 0 where they are the same
static inline int identity_order(const KqrIdentity &one, const KqrIdentity &other)
{
    const unsigned long long left[7] = {one.code,     one.registers, one.threads,   one.blocks,
                                        one.launches, one.cases,     one.cases_hash};
    const unsigned long long right[7] = {other.code,     other.registers, other.threads,   other.blocks,
                                         other.launches, other.cases,     other.cases_hash};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if (left[at] != right[at])
        {
            return (left[at] < right[at]) ? -1 : 1;
        }
    }
    return 0;
}

// ---- the turn ----

// The round. Each pair runs in a thread of its own while it has the turn, one at a time in the order of the
// bridge. It runs until it queries or its put ends. `s_turn` holds the pair that runs plus one, and 0 for the
// thread that carries the queries. A thread that gives the turn wakes every thread that waits on it
extern embed_atomic_word s_turn;

// the turn: the pair that runs, or -1 for the thread that carries the queries
static inline int identity_turn(void)
{
    const EmbedAtomicWordLoadCfg load = {&s_turn};
    // the turn holds a pair's index plus one, far below 2^31
    return (int)embed_atomic_word_load(&load) - 1;
}

// the turn given to `turn`, and every thread that waits woken
static inline void identity_turn_given(int turn)
{
    // a pair's index, or -1, plus one is at 0 or above
    const EmbedAtomicWordStoreCfg store = {&s_turn, (embed_atomic_word_value)(turn + 1)};
    embed_atomic_word_store(&store);
    const EmbedWordWakeCfg wake = {&s_turn};
    embed_word_wake(&wake);
}

// the wait until the turn is `turn`
static inline void identity_turn_awaited(int turn)
{
    // a pair's index, or -1, plus one is at 0 or above
    const embed_atomic_word_value wanted = (embed_atomic_word_value)(turn + 1);
    for (;;)
    {
        const EmbedAtomicWordLoadCfg load = {&s_turn};
        const embed_atomic_word_value seen = embed_atomic_word_load(&load);
        if (seen == wanted)
        {
            return;
        }
        const EmbedWordWaitCfg wait = {&s_turn, seen, EMBED_WAIT_INFINITE};
        (void)embed_word_wait(&wait);
    }
}

// ---- the state one module reads from another ----

extern QueryRecords s_identity_record;
extern char s_record_ksc[PLATFORM_PATH_BYTES];
extern KqrRecord s_query_record;
extern QueryTrace s_query_trace;
extern thread_local QueryLog *s_trace_log;
extern thread_local QueryLog *s_query_log;
extern QueryLog s_trace_whole;
extern QueryLog s_log_whole;
extern thread_local int t_pair;
extern thread_local int t_dangerous;
extern QryShared *s_query_run;
extern thread_local unsigned int s_apart_at;
// each pair's queries of the round, a PlatformArray of CarriedQueries, one a pair
extern PlatformArray s_round_queries;
extern unsigned long long s_round_passed;
extern unsigned long long s_round_recalled;
extern unsigned long long s_round_processes;
extern thread_local int t_turned;
extern thread_local int t_other_way;

// ---- questions.cu ----

// The latest blob of `name` in the run (qry_member.h), copied into a cell of the temporary tier, terminated, with its
// bytes into `size`. NULL where no run's writer runs or the run contains none. The caller gives the cell back
// [RETURNS OWNERSHIP]
char *run_blob_read(const char *name, size_t *size);
// The file at `path` as the run stores it (qry_member_file): its latest blob, or the file where the run stores none,
// copied as run_blob_read copies it. NULL where neither is there [RETURNS OWNERSHIP]
char *run_file_read(const char *path, size_t *size);
// `written` passed to the run's writer whole: as the current file at its path, or as a blob named by the path's
// own name. 1, or 0 where no run's writer runs or the pass failed
int run_closed(RunWritten *written);
// 1 where the link, as written or read as its type, transfers control or waits, which cubin_safe.h's rule 2 blocks from
// the part, EXIT alone excepted
int rule_broken(const char *link);
// the piece without the spaces at its ends and the semicolons at its end
CarrierPiece trimmed(CarrierPiece text);
// The line of a listing as a link. 1, or 0 where it is no instruction, or -1 where it is the branch to itself past
// the last exit, after which the chain reaches nothing
int link_read(CarrierPiece line, Link *link);
// The questions of the stick's listing at `listing`, each named by the manifest at `manifest`, into `questions`, which
// questions_ended gives back. 1, or 0 where a file does not read or the pool cannot meet a cell, said
int questions_read(const char *listing, const char *manifest, Questions *questions);
// The places of the question's chain ordered by exact text, into its `by_exact`, a run of one text by place. 1, or 0
// where the pool cannot meet the sort
int question_ordered(Question *question);
// The context of the link at `position` in the question's chain, onto `out`. For each register it names, the nearest
// link before it and the nearest after it that name the same register. Each goes as which side, the placeholder the
// link names it in, the placeholder the other names it in, and the other's link. The context reads nothing but the
// names the links share
void member_context(const Question *question, const char *position, PlatformText *out);
// The links of `one` past those of `other`, counted: a place in `one`'s chain for each, onto `past`, a PlatformArray of
// unsigned long long that the caller begins
void links_past(const Question *one, const Question *other, PlatformArray *past);
// 1 where the question's operands and result are integers, which the host computes as every system does
int question_integer(const Question *question);

// ---- slice.cu ----

// the window of `length` links from `first` of the question's chain, rendered link by link onto `out`
void window_render(const Question *question, unsigned long long first, unsigned long long length, PlatformText *out);
// the window of `length` links from `first` of the question's chain, rendered link by link onto `out`
void window_render(const Question *question, unsigned long long first, unsigned long long length, PlatformText *out);
// The identity of each window from `first` of the question's chain, one for each length, into `identities`, which
// holds the chain's links less `first` of them. Each is the hash of the window rendered, and one walk reads the window
// and its every extension. The answer is the count of identities
unsigned long long window_identities(const Question *question, unsigned long long first,
                                     unsigned long long *identities);
// 1 where the windows of `length` links from `first` of `one` and from `other_first` of `other` render the same
int windows_same(const Question *one, unsigned long long first, const Question *other, unsigned long long other_first,
                 unsigned long long length);
// The identity of each window from `first` of the question's chain, the hash of the window rendered. Each goes into
// `identities` where it is not NULL, at most `most` of them, and the walk stops after `most` windows. The answer is
// the last identity
unsigned long long windows_walked(const Question *question, unsigned long long first, unsigned long long most,
                                  unsigned long long *identities);
// the address of link `link` of question `question`, its number and its position, onto `line`
void address_text(const Questions *questions, unsigned int question, unsigned int link, VerbaLinea *line);
// Every window of every question's chain, into `windows`, which windows_ended gives back. 1, or 0 where the pool
// cannot meet the cell, said
int windows_counted(const Questions *questions, Windows *windows);
// The first occurrence of `identity` among the windows and its count, into `occurrence`. 1, or 0 where no window has
// the identity, and `occurrence` is then all zero
int window_occurrence(const Windows *windows, unsigned long long identity, Occurrence *occurrence);
// 1 where questions `one` and `other` are a slice: their listings the same but for one link a side. The two links
// name the same registers and read as different types, and go out through `left` and `right`
int slice_found(const Question *one, const Question *other, const Link **left, const Link **right);
int identity_slice(const char *listing, const char *manifest, const char *stick_path, const char *folder,
                   const char *ours);

// ---- record.cu ----

// the operands of a link at max, and the registers its operands name at max, a pair's two each counted
#define STALL_OPERANDS_MAX 12u
#define STALL_REGISTERS_MAX 32u

// a link's operation without its guard, and its operands in the order written
struct StallOperands
{
    IdentityName operation;
    IdentityLine operand[STALL_OPERANDS_MAX];
    unsigned int count;
};

// registers in the order of their names, each once, as a std::set holds them
struct StallRegisters
{
    LinkRegister name[STALL_REGISTERS_MAX];
    unsigned int count;
};

// 1 where `registers` holds the terminated `name`
static inline int stall_register_held(const StallRegisters *registers, const char *name)
{
    for (unsigned int at = 0u; at < registers->count; at += 1u)
    {
        if (identity_same(registers->name[at].text, name))
        {
            return 1;
        }
    }
    return 0;
}

// a field of the system's .ksc and its name
struct StallFieldNamed
{
    IdentityName name;
    StallField field;
};

// the fields of the system's .ksc ordered by name, in one cell of the persistent tier that stall_fields_ended gives
// back
struct StallFields
{
    StallFieldNamed *field;
    unsigned long long count;
};

static inline void stall_fields_ended(StallFields *fields)
{
    platform_cell_released(fields->field, platform_place_g_program_owner);
    fields->field = NULL;
    fields->count = 0ULL;
}

// the instructions' bits `field` takes at instruction `index` of `code`
unsigned long long stall_bits(const IdentityCode *code, unsigned long long index, StallField field);
// `value` written into the bits `field` takes at instruction `index` of `code`
void stall_bits_write(IdentityCode *code, unsigned long long index, StallField field, unsigned long long value);
// a link's operation without its guard, and its operands split, into `operands`
void stall_operation(const Link *link, StallOperands *operands);
// What the link writes, into `written`: the registers its first operand names, a pair where the operation's result is
// wide. An address in that place writes nothing, and neither does a link that writes no register
void stall_written(const Link *link, StallRegisters *written);
// What the link reads, into `read`: its guard, and every operand but a first it writes. A register a wide operation
// reads as data goes as its pair
void stall_read(const Link *link, StallRegisters *read);
// the fields the system's .ksc gives, by name, into `fields`, which stall_fields_ended gives back. A name the .ksc
// gives twice keeps its last field
void stall_fields(const char *ksc, StallFields *fields);
// The cases a pair's product goes over where the cases a question contains leave it short of whole. Each of the
// three operands goes through the values of every case and the `count` values at `qualifiers`, the values the
// pair's forms write of their own. The cases are every combination of them, cut into runs the run channel carries.
// The run `run` of them goes into `question`, and the answer is the count of runs. The host computes none of them,
// and a product requires nothing of the host
unsigned int whole_cases(RunQuestion *question, const unsigned long long *qualifiers, unsigned long long count,
                         unsigned int run);
// The cases a question goes over: every case the host computes, spread evenly through them only where the run
// channel carries fewer. The words of each go into `question`, and the host's place of each into `places`, which holds
// RUN_CASES_MAX
void stall_cases(RunQuestion *question, unsigned int *places);
// a 64-bit FNV-1a hash of `size` bytes at `bytes`, carried on from `hash`
unsigned long long record_hash(const void *bytes, size_t size, unsigned long long hash);
// the record written back to the .ksc that record_open read it from, every query a row in the order of its keys
void record_close(void);
// The record read from `ksc`, and written back to it when the program ends. Then the query record beside it, a
// cycle of mode `mode` pending in its order. The channel learns how far a container's code reaches, as `ksc`
// stores it
void record_open(const char *ksc, const char *mode);

// ---- alike.cu ----

// 1 where the part answers `code` alike with the host on every case it computes. The code goes as `question`
// specifies of its registers, shape and launches. 0 where the part answers apart, refuses it or the gate blocks
// it. `queries` counts each query
int question_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                   unsigned long long *queries);
// `code` put to the part as `question` specifies, to read what it answers, not to check it against the host. R stores a
// verdict alone and cannot answer a reading: the record is neither read nor written. 1 where the part answers it, each
// case's value in `question->answered`, 0 where it refuses it or the gate blocks it, each counted in `queries`
int question_read(const IdentityCode *code, RunQuestion *question, unsigned long long *queries);
// 1 where the part answers `code` alike with the host on every case it computes, in a container that declares
// every register its file uses (sm_86.kdm). 0 where it answers apart, refuses it or the gate blocks it. `queries`
// counts each query
int stall_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                unsigned long long *queries);
// Each chain's answers the host computes, one a case, by the chain's number, into `host`, which host_answers_ended
// gives back. They come from the latest blob of `answers` in the run, a chain a line, its number first. Where `whole`
// is 1, a line of other than IDENTITY_CASES answers gives nothing, and a later line of a number goes in place of an
// earlier. Where it is 0, every line gives its answers, and a later line of a number goes on after an earlier's. 1, or
// 0 where the pool cannot meet a cell. A run with no such blob gives no chain
int host_answers_read(const char *answers, int whole, HostAnswers *host);
// Chain `number` of ours, read from `engine`: its code, sixteen bytes an instruction, into `code`, and the links its
// listing gives, into `links`, a PlatformArray of Link. Each replaces what it holds, and the listing's lines size
// `links` once. 0 where either file does not read, or the listing gives more links than the code contains
// instructions
int chain_code_read(const char *engine, const char *number, IdentityCode *code, PlatformArray *links);
// The registers chain `number` of ours names: its high water mark as `engine` gives it, or 0 where it gives none.
// The part refuses a container that declares the mark alone where the code names a register of the part's own.
// That register lies past the last the part gives code. The walk then takes the count a container declares up
// from the mark until the part answers
unsigned int chain_registers(const char *engine, const char *number);
// The .ksc rewritten by `who`, with `rows` in place of every row of the run channel it recorded for a question.
// Each such question begins with the words `queried`, whatever came back. The rows go after its last row of the
// run channel, with its counts of the run channel taken again (ksc_rows_replaced_traced_by). Every row let go
// that no row put gives again goes in the part's .krf beside it (krf.h). 1, or 0 where the write of the .ksc
// failed
int ksc_answers_written(const char *ksc, const char *queried, const char *const *rows, unsigned int count, Who who);
// The stick's integer questions, the `count` numbers at `numbers` in the order of their bytes and each once. Written
// as a program the host compiles to compute them, at `path`, from the stick's text `stick`. 1, or 0 where the write
// failed
int host_questions_write(const char *path, CarrierPiece stick, const IdentityName *numbers, unsigned long long count);

// ---- assembled.cu ----

// The `count` terminated texts at `texts` put into code by the vendor's writer against the machine file at `machine`.
// Each text's code goes after the codes of `codes`, a PlatformArray of IdentityCode that the caller begins. Each code
// is empty where a line of it did not assemble or the writer did not run. Each code is on the tier of `codes`
void vendor_assembled(const char *machine, const char *const *texts, unsigned long long count, PlatformArray *codes);
// the terminated `text` put into code as above, into `code`, which it replaces
void vendor_assembled(const char *machine, const char *text, IdentityCode *code);
// each code of `codes`, a PlatformArray of IdentityCode, given back, and the array
void identity_codes_ended(PlatformArray *codes);

// the .ksc rewritten by `who` as ksc_answers_written rewrites it, with the one row `row`
static inline int ksc_answer_written(const char *ksc, const char *queried, const char *row, Who who)
{
    const char *const rows[1] = {row};
    return ksc_answers_written(ksc, queried, rows, 1u, who);
}

// ksc_answer_written, the who the code that calls it
#define ksc_answer_write(ksc_, queried_, row_) ksc_answer_written((ksc_), (queried_), (row_), WHO_HERE)

// `cases` filled with IDENTITY_CASES answers of the terminated `answer`, which the caller holds while `cases` lasts
static inline void host_cases_filled(HostCases *cases, const char *answer)
{
    cases->number.text[0] = '\0';
    for (unsigned int at = 0u; at < IDENTITY_CASES; at += 1u)
    {
        cases->answer[at] = answer;
    }
    cases->count = IDENTITY_CASES;
}

// ---- frame.cu ----

// The kernel the system accepted, as the vendor's writer appends it to the run in its kernel mode (kernel_blob.h). It
// is the latest blob of KERNEL_BLOB_NAME, read where the run stores it, every place of it whatever place the writer
// names. Its code goes into `code`, `places` of them, and the registers a thread of it uses into `registers`. The
// frame's queries and the fixed registers' start from the whole kernel. 1, or 0 where the run contains no kernel this
// reads
int frame_kernel_read(const unsigned char **code, unsigned int *places, unsigned int *registers);
// The frame queried (Open 6.3). The kernel the system accepted goes whole over every case, with no host value to
// check it against. Its body is the ladder's relation at its probe place. An answer on every case means the part
// loads and returns from the container the vendor's writer makes: `run answers 00000001 container returned`. Its
// answers are then the reference.
//
// The kernel with its first k places dropped goes to the part. A walk halves k to the greatest that still answers
// every case as the whole kernel does. That k is the least opening a kernel runs with: `run answers <k> opening
// least`. Each variant and its verdict is a line of the run's blob frame.txt. 0 where both rows reach the .ksc or
// the run is dry, 1 otherwise with the reason printed
int identity_frame(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const char *const *carrier);

// ---- fixed.cu ----

// The fixed registers queried (Open 6.2) over the kernel the system accepted: the probe place, zero, and what
// each register contains at the probe place. The rows go to `ksc`. 0 where the rows reach `ksc` or the run is
// dry
int identity_fixed(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const char *const *carrier);

// ---- arrangements.cu ----

// the bytes of one register's written form, its terminator among them
#define IDENTITY_REGISTER_BYTES 256u

// the times the cost writes an arrangement over itself in one timed block
#define COST_REPEATS 16u

// a tree over the precepts, its nodes children first and the root last, at max PRECEPT_NODES of them
struct PreceptTree
{
    PreceptNode node[PRECEPT_NODES];
    unsigned int count;
};

// The bytes of one line of a .kdm, its terminator among them. sm_86.kdm's longest is 96 bytes. The fields of a row a
// line holds, each an IdentityLine
#define COST_LINE_BYTES 256u
#define COST_FIELDS 5u

// the bytes of what came of an arrangement or a form, its terminator among them: a word, or `refused` and the
// refusal of the run channel
#define COST_VERDICT_BYTES 192u

struct CostVerdict
{
    char text[COST_VERDICT_BYTES];
};

struct CostLine
{
    char text[COST_LINE_BYTES];
};

// one arrangement of a .kdm: its line, its fields, its tree with the root last, what came of it, and the least its
// clock read
struct CostRow
{
    unsigned long long line;
    IdentityLine field[COST_FIELDS];
    PreceptTree tree;
    CostVerdict verdict;
    unsigned long long least;
};

// The lines of a .kdm and its rows, each a cell of the persistent tier taken once at the .kdm's count of lines. Then
// the places of the rows ordered by their operator, the first field, a run of one operator in the order of its rows
struct CostRows
{
    CostLine *line;
    unsigned long long line_count;
    CostRow *row;
    unsigned long long row_count;
    unsigned long long *by_operator;
};

static inline void cost_rows_ended(CostRows *rows)
{
    platform_cell_released(rows->by_operator, platform_place_g_program_owner);
    platform_cell_released(rows->row, platform_place_g_program_owner);
    platform_cell_released(rows->line, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = rows, .bytes = sizeof(*rows), .val = 0);
}

// register `number` of the bank `bank` as a name, empty where the ruleset has no bank of that name
IdentityName ruleset_register_name(const Ruleset *rules, const char *bank, unsigned int number);
// the register every lane uses throughout named `name`, empty where the ruleset holds none of that name
IdentityName ruleset_fixed_name(const Ruleset *rules, const char *name);
// Form `name` put onto `out`, the `count` terminated arguments at `arguments` in the order of its parameters, each
// scratch register of a construct from `scratch`. 0, and nothing put, where the ruleset writes no form of that name or
// the form takes another count of arguments. Also 0 where `scratch` gives no scratch register a construct names
int ruleset_form_put(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                     const RulesetNamedScratch *scratch, PlatformText *out);
// The word of the web of two reads whose tree answers as `tree` does: the word an operator's arrangements
// produce. The two trees agree on every pair of the low words of the values a question goes with. NULL where no
// word is such
const Word *word_produced(const PreceptTree *tree);
// The arrangement `tree` written through `sass` node by node onto `out`, each node in the form of the web's word that
// is its precept alone (word_web.h). A leaf is the operand at its place of the `count` at `operands`, the fixed zero
// for a word of zeroes and `ones` for a word of ones. Every node but the root goes to a register of its own numbered
// from `next` on, and the root to `to`. 1, or 0 and nothing put where a precept has no such word or the ruleset writes
// none of it
int arrangement_written(const Ruleset *sass, const PreceptTree *tree, const char *const *operands, unsigned int count,
                        const char *ones, const char *to, unsigned int next, PlatformText *out);
// The lines of the .kdm at `kdm` and each arrangement's row, into `rows`, which cost_rows_ended gives back. A row
// whose chain is no tree over the precepts is `unread`. 1, or 0 where the pool cannot meet a cell or a line passes
// COST_LINE_BYTES, said
int kdm_rows_read(const char *kdm, CostRows *rows);

// ---- read.cu ----

// A form of a ruleset: its name and the links its text writes, each a pattern a link's type matches whole. The
// patterns lie in one cell of the persistent tier, taken once at their count
struct IdentityForm
{
    IdentityName name;
    TablePattern *links;
    unsigned int link_count;
};

// the forms of the rulesets read, a PlatformArray of IdentityForm on the persistent tier that forms_ended gives back
struct IdentityForms
{
    PlatformArray forms;
};

// the form at `at` [BORROWS]
static inline const IdentityForm *identity_form(const IdentityForms *forms, unsigned long long at)
{
    return (const IdentityForm *)platform_array_at(&forms->forms, at);
}

static inline void forms_ended(IdentityForms *forms)
{
    for (unsigned long long at = 0ULL; at < forms->forms.count; at += 1ULL)
    {
        const IdentityForm *const form = identity_form(forms, at);
        for (unsigned int link = 0u; link < form->link_count; link += 1u)
        {
            table_pattern_ended(&form->links[link]);
        }
        platform_cell_released(form->links, platform_place_g_program_owner);
    }
    platform_array_ended(&forms->forms);
}

// a language's files read and checked against each other and its schema
struct LanguageSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// a member of a slice. Its question and the position of its link in that question's chain, each a piece of the
// slices' text [BORROWS]
struct Member
{
    CarrierPiece question;
    CarrierPiece position;
};

// a slice that carries an identity. The member that writes its first link and the member that writes its second,
// each with its context, a piece of the slices' text [BORROWS]
struct Carrier
{
    Member first;
    Member second;
    CarrierPiece first_context;
    CarrierPiece second_context;
};

// The forms of each ruleset given, each name with the links its text writes, into `forms`, which forms_ended gives
// back. 0 where a ruleset does not read, said
int forms_read(int count, char **files, IdentityForms *forms);
// the names of the forms whose texts write the terminated `type`, joined by spaces, or - where none does, onto `out`
void forms_writing(const IdentityForms *forms, const char *type, PlatformText *out);
// The identities of the slices, read against the host's answers and the forms, into the .klq `klq`. They go
// after its lines before the first identity: each identity, its contexts, their carriers and verdicts, then the
// chains
int identity_read(const char *folder, const char *klq, int count, char **files);
// The files the `count` paths at `given` name, onto `paths`, a PlatformArray of IdentityPath that the caller begins. A
// folder gives every .dis file in it in the order of their names. A folder of the engine's writing of the stick
// contains more files than one command line can name
void candidate_paths(const IdentityPath *given, unsigned long long count, PlatformArray *paths);
// The chains of a candidate file, into `candidates`, which questions_ended gives back: one for each function its
// listing names, in the order written. Where a link comes before any function, it begins a chain named for the file's
// stem. 1, or 0 where the pool cannot meet a cell, said
int candidates_read(const char *path, Questions *candidates);

// ---- known.cu ----

// each candidate chain of the `count` files at `paths` sifted through the identities nvcc's chains contain, into the
// run's blob known.txt
int identity_known(const char *listing, const char *manifest, const char *folder, const IdentityPath *paths,
                   unsigned long long count);
// the parts of nvcc's chains put together by their categories, the questions their permutations raise written to the
// run's blob permutations.txt
int identity_permute(const char *listing, const char *manifest, const char *folder);

// ---- broken.cu ----

// ours checked against the answer question by question, each place they part traced to the forms that wrote it, into
// the run's blob broken.txt. 0, or 1 where a file does not read or the write fails
int identity_broken(const char *listing, const char *manifest, const char *folder, const IdentityPath *ours_paths,
                    unsigned long long ours_count, int count, char **files);

// ---- stall.cu ----

// the max questions one process carries, for the memory each uses while the part runs it
#define STALL_CARRIED_MAX 1024u

// the bytes of a pair's key, its terminator among them: two operations and a tab between
#define STALL_PAIR_BYTES (2u * IDENTITY_NAME_BYTES)

// the key a pair goes by, its writer's operation and its reader's apart by a tab, which orders as the pair does
struct StallPair
{
    char text[STALL_PAIR_BYTES];
};

// one question of the stall walk: the pair it queries, the chain whose host answers check it, and the stall
struct StallQuery
{
    StallPair pair;
    IdentityName number;
    unsigned long long step;
};

// the key of the pair of the terminated operations `writer` and `reader`
StallPair stall_pair_key(const char *writer, const char *reader);
// The soonest a reader reads each operation's result, queried of the part link by link. Each pair's soonest goes
// to `ksc`, and every pair with its probes to the run's blob stall.txt. 0, or 1 with the reason printed
int identity_stall(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const char *machine_path, const char *const *carrier);

// ---- forms.cu ----

// The way a query enters the part is its vector. That is the registers it declares, the lanes it goes over, a
// case a lane, and the operands of a case its code loads. A query goes at the least magnitude, the registers its
// chain names as `mark`, the engine's high water mark.
//
// The part can refuse it there and answer it where it declares every register its file uses. A walk then finds
// the fewest it answers at between the two, and the query goes at the fewest. Where the part refuses it at both,
// the refusal is not the registers', and the part refuses the query. 1 where the part answers it alike with the
// host on every case it computes, 0 otherwise, its registers in `question`
int vector_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                 unsigned long long *queries, unsigned int mark);
// The carrier `text`, its links `links`, cut at the form located at link `link_at` over `form_lines` links, onto
// `out`. The cut stores the value of `read` in place of the carrier's answer. It holds the carrier's links
// before the form, then `located` in the form's place where the call gives it. Then comes the register the store
// reads, written with the value, a flag's as 1 where it is true. Then come the carrier's own links after the form
// that no operand of a case reaches, and its store and what follows. With no form located, `read` is a value the
// form reads; with one, a value it writes.
//
// 0, and nothing put, where a link of the carrier's own after the form names the stored register, or `read` is a
// uniform register
int carrier_cut_at_read(const CarrierLinks *links, long cases_defined, const char *text, long link_at, long form_lines,
                        const char *read, const char *located, int wide, PlatformText *out);
// The shape of what the form `form_text` reads at link `at` of a carrier, its links `links` and its text
// `chain_text`. It is the form and the link that writes each register it reads, their registers struck out. Two
// links of one shape read the cases alike, and contain the same rows of a product
void read_shape(const CarrierLinks *links, CarrierPiece chain_text, long cases_defined, CarrierPiece form_text, long at,
                PlatformText *out);
// pieces of a text, a PlatformArray of CarrierPiece, begun empty on the temporary tier
static inline void identity_pieces_begun(PlatformArray *pieces)
{
    platform_array_begun(pieces, sizeof(CarrierPiece), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

// the piece at `at` of `pieces`
static inline CarrierPiece identity_piece_at(const PlatformArray *pieces, unsigned long long at)
{
    return *(const CarrierPiece *)platform_array_at(pieces, at);
}

// A form's text, each parameter a marker, cut into its tokens, each a piece of `text`, onto `tokens`. `tokens` is a
// PlatformArray of CarrierPiece the caller begins, which it replaces. A marker is one token with the part of a pair
// it names, `.hi`.
// A run of characters, digits, `_` and `.` is one, and every other mark one, the space between passed over
void form_tokens(CarrierPiece text, PlatformArray *tokens);
// the markers a form's text names at max, each its byte, and so each a place of its own
#define FORM_MARKERS 256u

// The binding of each parameter of a form read against another, by its marker's byte. The piece of the other form's
// text it goes to, which lies in that text [BORROWS], and 1 in `held` where it goes to one. `count` counts them
struct FormBinding
{
    CarrierPiece value[FORM_MARKERS];
    unsigned char held[FORM_MARKERS];
    unsigned int count;
};

// the binding emptied
static inline void form_binding_cleared(FormBinding *binding)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = binding->held, .bytes = sizeof(binding->held), .val = 0);
    binding->count = 0u;
}
// The form `to_text` read against the form `from_text`, token by token, each parameter a marker. Each parameter of `to`
// goes in `bound`, by its letter, to what `from` writes in its place, a marker of one of `from`'s parameters or a
// literal. Each parameter of `from` that `to` writes a literal in place of goes in `fixed` to that literal. A
// marker that names a part of a pair goes against the same part of a marker. It also goes against the fixed zero,
// whose every part is zero.
// 1 where every other token is the same: `to` is `from` with the parameters `fixed` names bound, one structure
int form_unified(const Ruleset *sass, const char *to_text, const char *from_text, FormBinding *bound,
                 FormBinding *fixed);
// 1 where the form `text` reads what the launch gives. A thread's or a block's number from a special register, or a
// block's or a grid's extent from the constant bank below the parameters
int form_launched(const char *text);
// the conditions a form's comparisons test at max, each once
#define FORM_CONDITIONS_MAX 16u

// conditions in the order of their bytes, each once, as a std::set holds them
struct FormConditions
{
    IdentityName condition[FORM_CONDITIONS_MAX];
    unsigned int count;
};

// the conditions the comparisons of the form `text` test, each the word after a `SETP`'s first `.`, put into
// `conditions`
void form_conditions(const char *text, FormConditions *conditions);
// 1 where the form `text` decides which form runs next: an exit, a return, a branch, a call or a convergence
int form_controls(const char *text);
// 1 where the form `text` reads or writes memory through an address. A bank of constants, `c[...]`, holds its
// words at a place no case moves, and is no address
int form_addresses(const char *text);
// 1 where the form `text` reads or writes memory through an address, or sends control elsewhere. A process of
// its own carries a stand-in of it
int form_dangerous(const char *text);
// 1 where the form `text` computes a value from its operands
int form_operates(const char *text);
// 1 where every instruction of the form `text` gives one value with its first two sources taken either way round
int form_commutes(const char *text);
// 1 where the form `text` does a thing that computes no value, tests nothing and sends control nowhere. That is
// a load, a store, a move, a copy or a read of a special register
int form_verbs(const char *text);
// 1 where one of the forms `first` and `second` is the other loaded const: the same operands, its opcode the other's
// with `.CONSTANT` after it
int form_const_loaded(const char *first, const char *second);
// 1 where the form `text` contains no instruction, each of which ends at a `;`: a directive or a label alone.
// Such a form places the forms a case reads
int form_switches(const char *text);
// 1 where the form `text` names a parameter at its `.hi`: the form works a wide value in its pieces, the low word
// and the high
int form_ranged(const char *text);

// ---- carriers.cu ----

// One carrier of ours the host computes: its text and its links, read for what each writes, reads and loads. Then
// the link that writes the register its cases load through. Then its vector at its least: the registers its chain
// names, then the operands of a case it loads
struct CarrierChain
{
    IdentityName number;
    char *text;
    size_t length;
    CarrierLinks links;
    long cases_defined;
    unsigned int registers;
    unsigned int operands;
};

// Every carrier of ours the host computes, ordered by its question. In one cell of the persistent tier taken once at
// the host's count of chains. Each chain's text is a cell of its own, and its links another. carriers_ended gives
// every cell back
struct Carriers
{
    CarrierChain *chain;
    unsigned long long count;
};

static inline void carriers_ended(Carriers *carriers)
{
    for (unsigned long long at = 0ULL; at < carriers->count; at += 1ULL)
    {
        platform_cell_released(carriers->chain[at].text, platform_place_g_program_owner);
        platform_array_ended(&carriers->chain[at].links);
    }
    platform_cell_released(carriers->chain, platform_place_g_program_owner);
    carriers->chain = NULL;
    carriers->count = 0ULL;
}

// the carrier of the question `number`, or NULL where ours holds none the host computes
static inline const CarrierChain *carrier_chain_of(const Carriers *carriers, const char *number)
{
    const unsigned long long at = identity_lower(carriers->chain, carriers->count, [number](const CarrierChain &one) {
        return identity_order_text(one.number.text, number) < 0;
    });
    return ((at < carriers->count) && identity_same(carriers->chain[at].number.text, number)) ? &carriers->chain[at]
                                                                                              : NULL;
}

// the groups of a match a found link keeps, the whole match first
#define FOUND_GROUPS 16u

// A link a form is at in a carrier, with its vector. The vector is the carrier's delta before the form, then the
// carrier's magnitude: its registers and the operands of a case it loads. `link_at` is the link's place in the
// carrier's lines, and `group` the match's spans in the carrier's text
struct FoundLink
{
    IdentityName chain;
    long link_at;
    RegexpSpan group[FOUND_GROUPS];
    long delta;
    unsigned int registers;
    unsigned int operands;
};

// the bytes of group `group` of `link` in the carrier's text `text` [BORROWS]
static inline CarrierPiece found_group(const FoundLink *link, unsigned int group, const char *text)
{
    const RegexpSpan span = link->group[group];
    const CarrierPiece piece = {&text[span.from], (span.set != 0) ? (size_t)(span.to - span.from) : 0u};
    return piece;
}

// the scratch a form written into a chain takes: the ruleset, the registers the chain names, and the count taken
struct ScratchPast
{
    const Ruleset *sass;
    unsigned int reserved;
    unsigned int *taken;
};

// the carriers of the chains in `engine` the host computes, by `host`, into `carriers`, which carriers_ended gives
// back. 1, or 0 where the pool cannot meet a cell
int carriers_read(const char *engine, const HostAnswers *host, Carriers *carriers);
// the parameters a form names at max
#define FORM_NAMES_MAX 32u

// the names of a form's parameters in the order its line gives them
struct FormNames
{
    IdentityName name[FORM_NAMES_MAX];
    unsigned int count;
};

// the group of each marker in the pattern that finds a form's lines, by its byte, 0 where it holds none, and the
// count of groups
struct FormGroups
{
    unsigned int group[FORM_MARKERS];
    unsigned int count;
};
// The scratch a form written into a chain takes, each register as the ruleset writes its bank. It takes
// registers past the `reserved` the chain names, an even one first so that a wide's pair lies aligned. It takes
// the next each time the writing requires. Predicate 5 is the scratch predicate, which no chain of ours names
RulesetNamedScratch scratch_past(ScratchPast *past);
// the names of form `form`'s parameters in the order the ruleset's own line for it gives them, into `names`, which it
// replaces. None where it gives no line for the form
void ruleset_parameter_names(const Ruleset *rules, const char *form, FormNames *names);
// Form `form` as `sass` writes it onto `out`, each parameter a marker, and each scratch register the writing takes a
// marker of its own past them. Its parameters' names go into `names`. 1, or 0 and nothing put where the ruleset gives
// no form of the name
int marked(const Ruleset *sass, const char *form, FormNames *names, PlatformText *out);
// The links the form `text`, each parameter a marker, is at in every carrier of `carriers`, put after those of
// `found_links`, a PlatformArray of FoundLink. The least vector goes first, and those of one vector go in an order
// drawn from `seed` and `drawn_for`. Each marker's group in the pattern that finds its lines goes into `group`
void links_of(const Carriers *carriers, unsigned long seed, const char *text, FormGroups *group, const char *drawn_for,
              PlatformArray *found_links);
// the bytes of a pair's categories, its terminator among them
#define PAIR_CATEGORIES_BYTES 256u

struct PairCategories
{
    char text[PAIR_CATEGORIES_BYTES];
};

// What the texts of a pair's two forms, as `sass` writes them, show of it before any query, each word after a
// space. First the structure. One form the other with a parameter bound, both forms that read what the launch
// gives, or two operations that are one thing written twice. Then the categories the texts read the pair into.
// Both forms are comparisons, every condition tested an equality or some an order. Either form is an operation at
// any width.
//
// Every instruction of one gives one value with its first two sources either way round. Or either form is a word
// that does a thing and computes no value. Last, either form reads where its lane sits,
// works a wide value in pieces, decides which form runs next, or contains no instruction
PairCategories pair_categories(const Ruleset *sass, const char *first, const char *second);
// The lines the bridge contains beneath the pair of `first` and `second`, either way round, onto `beneath`, a
// PlatformArray of CarrierPiece. The bridge is its `count` lines at `bridge`, each a piece of its text. They are its
// verdict, the metas and sets orior_klq_decoder read it into, its concept and its intent. None where the bridge
// contains no such pair
void pair_beneath(const CarrierPiece *bridge, unsigned long long count, const char *first, const char *second,
                  PlatformArray *beneath);
// the bytes of a pair's verdict, its terminator among them
#define PAIR_VERDICT_BYTES 1024u

struct PairVerdict
{
    char text[PAIR_VERDICT_BYTES];
};

// the verdicts of a pair put both ways round, `one_way` then `other_way`: the first that closes it, and otherwise open
// with the cases alike over both
PairVerdict pair_verdicts_joined(const char *one_way, const char *other_way);
// A pair the cross read, a row of the run's table of pairs (pairs.oracle.tsv). The row holds its two forms'
// texts, closed or open, and its count of cases alike where it is open. Then the text of the case that closed it
// where it closed, the hash of what its put reads where `read` is 1, and its verdict's text
void pair_row_passed(const char *first, const char *second, int read, unsigned long long reads, const char *verdict);

// The cuts `codes`, `count` of them, put together over the cases `question` contains. Each is dangerous where
// `dangerous` specifies, and each has its place from `places`, RUN_PLACE_NONE where it contains none; NULL gives 0 and
// RUN_PLACE_NONE to every cut. Each container declares `mark` registers, and every register its file uses where the
// part refuses that, those put together in the round after. Each cut's question goes into `questions`, in the
// order of `codes`, its outcome, answers and refusal its own. A cut with no code goes unqueried.
//
// The call replaces what `questions` holds. `queries` counts each query. The answers are values the host does not
// compute, stored in the log alone and never in R
void cuts_queried(const IdentityCode *codes, unsigned long long count, const int *dangerous, const unsigned int *places,
                  const RunQuestion *question, unsigned long long *queries, unsigned int mark,
                  PlatformArray *questions);

// ---- pair.cu ----

// On a pair's thread: 1 where a conference turned its put the other way round. Then 1 while its put goes the
// other way round already, or goes as a witness (carriers.cu)
extern thread_local int t_turned;
extern thread_local int t_other_way;
// The bridge's pairs put to the part, `pair <form> <form>` in Lstar.klq. Each verdict goes beneath its pair in the
// bridge in place of the one it recorded. 0, or 1 with the reason printed
int identity_pair(const char *engine, const char *answers, const char *ksc, const char *klq, const char *machine_path,
                  const char *const *carrier);

// ---- cost.cu ----

// The arrangements of a part's .kdm costed on the part (P6, the rank), each cost in its row of `kdm`. The machine
// file's forms of the blob `forms` get a cost the same way, written to `ksc`. 0, or 1 with the reason printed
int identity_cost(const char *engine, const char *answers, const char *ksc, const char *folder, const char *kdm,
                  const char *machine_path, const char *forms, const char *const *carrier);

// Each pair the .kdm's arrangements at `kdm` contain and none of the `excluded_count` pairs at `excluded` names, the
// excluded in the order of their keys. Probed in that arrangement at its operator's link as the cost puts it. Every
// stall of the pair's writer goes on at max `max` probes. Each question goes after those of `queries`, a PlatformArray
// of StallQuery, and its code after those of `codes`, a PlatformArray of IdentityCode
void arrangement_probes(const char *engine, const HostAnswers *host, const char *kdm, const Ruleset *sass,
                        const char *machine_path, StallField stall, unsigned long long longest,
                        const StallPair *excluded, unsigned long long excluded_count, unsigned long long max,
                        PlatformArray *queries, PlatformArray *codes);

// ---- the walks ----

// A walk in steps: from `from` by `step` up to `end`, the first number `predicate` is 1 at. One step past `end` where
// it is 1 at none. `predicate` is a call of one long long that answers an int
template <typename Predicate>
static inline long long walk_step(const Predicate &predicate, long long from, long long step, long long end)
{
    for (long long number = from; number != (end + step); number += step)
    {
        if (predicate(number) == 1)
        {
            return number;
        }
    }
    return end + step;
}

// A walk that halves: `truthy` a number the relation is true at and `falsy` one it is not, on either side of it.
// Each moves to
// the middle the relation gives the same answer as, until the two are adjacent. The answer is the truthy end, the last
// number the relation is true at before the bound
template <typename Predicate>
static inline long long walk_halved(const Predicate &predicate, long long truthy, long long falsy)
{
    while (((truthy - falsy) > 1) || ((falsy - truthy) > 1))
    {
        const long long middle = truthy + ((falsy - truthy) / 2);
        if (predicate(middle) == 1)
        {
            truthy = middle;
        }
        else
        {
            falsy = middle;
        }
    }
    return truthy;
}

// the field `name` of `fields`, or NULL where the .ksc gives none
static inline const StallField *field_of(const StallFields *fields, const char *name)
{
    const unsigned long long at = identity_lower(fields->field, fields->count, [name](const StallFieldNamed &one) {
        return identity_order_text(one.name.text, name) < 0;
    });
    return ((at < fields->count) && identity_same(fields->field[at].name.text, name)) ? &fields->field[at].field : NULL;
}

// The threads every point of a curve launches in all, and the launches its time comes from. Then the queries in a
// row that the band of the fewest registers' times holds, with no change of width, before it counts as
// sustained. Then the max queries one band or one point's bounces gets
#define CURVE_THREADS (1u << 20u)

#define CURVE_LAUNCHES 100u

#define CURVE_SUSTAIN 5u

#define CURVE_QUERIES_MAX 64u

// the times a launch of one point takes over every query of it: the least and the max
struct CurveBand
{
    unsigned long long low;
    unsigned long long high;
};

// ---- walk.cu ----

// the last register a question's code can name, walked down and written to `ksc` as `register last`
int identity_register(const char *engine, const char *answers, const char *ksc, const char *folder,
                      const char *const *carrier);
// the last predicate a question's code can name, walked down and written to `ksc` as `predicate last`
int identity_predicate(const char *engine, const char *answers, const char *ksc, const char *folder,
                       const char *const *carrier);
// how far a container's code reaches past the highest register it names, written to `ksc` as `register past`
int identity_past(const char *engine, const char *answers, const char *ksc, const char *folder,
                  const char *const *carrier);

// ---- curve.cu ----

// The cost of registers to each task of `tasks`, queried of the part as the time of each launch. Each knee and
// each task's soonest count of threads goes to `ksc`, and every point is a line of curve.txt. 0, or 1 with the
// reason printed
int identity_curve(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const IdentityName *tasks, unsigned long long task_count, const char *const *carrier);

// ---- text.cu ----

// a case of the host, its operands and its third, as an identity's verdict writes it, onto `line`
void case_text(unsigned int at, VerbaLinea *line);
// The queue over the stick's questions (P13, the scheduler): each entry read against R, and put to the part where
// it is gray. Each entry is a line of the run's blob queue.txt. 0 where no entry reads 0
int identity_queue(const char *engine, const char *answers, const char *ksc, const char *folder, const char *manifest,
                   const char *const *carrier);
// the bridge's identities between texts checked on the part (P13, the identities), each verdict written beneath its
// identity in the bridge
int identity_text(const char *engine, const char *answers, const char *ksc, const char *klq, const char *manifest,
                  const char *const *carrier);

// ---- queries.cu ----

void query_log_passed(QueryLog *log);
void query_logs_opened(void);
void query_log_flushed(QueryLog *log);
// a question's identity written as the query record's text form writes it, for a person to read, into `text`
void question_identity(const RunQuestion *question, char *text, size_t capacity);
void queries_answered(const CarriedQueries *carried);
void queries_carried(RunQuestion *const *questions, const int *dangerous, unsigned int count);
int bracket_contains(const char *host, unsigned long long answered);

#endif
