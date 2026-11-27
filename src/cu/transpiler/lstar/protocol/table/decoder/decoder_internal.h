// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// decoder_internal.h: what the modules of orior_klq_decoder share: its includes, one put of the log, the metas, sets
// and categories a pair is read into, the functions over puts, and the decoding of one log
#ifndef KLQ_DECODER_INTERNAL_H
#define KLQ_DECODER_INTERNAL_H

#include "../../../../../../../includes/file_defs/klq/query_trace.h"
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "carrier_flow.h"
#include "concept_product.h"

static const char *const s_metas[] = {"structural_coherence",   "syntactic_coherence", "pragmatic_coherence",
                                      "data_quality_coherence", "noise_coherence",     "folding_coherence"};

// the sets, each written beneath a pair's verdict in the order the tree orders them, `unknown_coherence` last. The
// texts read a pair into a category, and only an answer reads it out of `unknown_coherence`
static const char *const s_sets[] = {
    "comparison_coherence",  "equality_coherence", "order_coherence",     "operation_coherence",
    "commutative_coherence", "verb_coherence",     "statement_coherence", "qualifier_coherence",
    "modifier_coherence",    "negation_coherence", "witness_coherence",   "range_coherence",
    "vector_coherence",      "control_coherence",  "switch_coherence",    "unknown_coherence"};

// the categories the texts read a pair into, before any query
static const char *const s_categories[] = {
    "comparison_coherence", "equality_coherence", "order_coherence",  "operation_coherence", "commutative_coherence",
    "verb_coherence",       "range_coherence",    "vector_coherence", "control_coherence",   "switch_coherence"};

#define DECODER_COUNT(names_) (sizeof(names_) / sizeof((names_)[0]))

// The names the decoder reads a pair into, each a bit of a mask: the metas first, then the sets. A name the log or the
// bridge gives is one of them, and every category is a set
#define DECODER_METAS ((unsigned int)DECODER_COUNT(s_metas))
#define DECODER_NAMES (DECODER_METAS + (unsigned int)DECODER_COUNT(s_sets))

// the bit of the meta at `at` of s_metas, and of the set at `at` of s_sets
#define DECODER_META_BIT(at_) (1u << (at_))
#define DECODER_SET_BIT(at_) (1u << (DECODER_METAS + (at_)))

// the name of the bit at `at`
static inline const char *decoder_name_text(unsigned int at)
{
    return (at < DECODER_METAS) ? s_metas[at] : s_sets[at - DECODER_METAS];
}

// 1 where `piece` is the terminated `text`
static inline int decoder_is(CarrierPiece piece, const char *text)
{
    return (piece.length == EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR)) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = text, .bytes = piece.length) == 0);
}

// 1 where `piece` begins with the terminated `prefix`
static inline int decoder_begins(CarrierPiece piece, const char *prefix)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = prefix, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length >= length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = prefix, .bytes = length) == 0);
}

// the place of the name `piece` among the decoder's names, or DECODER_NAMES where it is none of them
static inline unsigned int decoder_name_at(CarrierPiece piece)
{
    unsigned int at = 0u;
    while ((at < DECODER_NAMES) && !decoder_is(piece, decoder_name_text(at)))
    {
        at += 1u;
    }
    return at;
}

// the bit of the name `piece`, 0 where it is none of the decoder's names
static inline unsigned int decoder_name_bit(CarrierPiece piece)
{
    const unsigned int at = decoder_name_at(piece);
    return (at < DECODER_NAMES) ? (1u << at) : 0u;
}

// the bit of the terminated set name `name`, which s_sets holds
static inline unsigned int decoder_set_bit(const char *name)
{
    return decoder_name_bit(carrier_piece_of(name));
}

// the bits of the categories
static inline unsigned int decoder_category_bits(void)
{
    unsigned int bits = 0u;
    for (size_t at = 0u; at < DECODER_COUNT(s_categories); at += 1u)
    {
        bits |= decoder_set_bit(s_categories[at]);
    }
    return bits;
}

// the byte order of two pieces, as strcmp orders two texts: below 0, 0 or above 0
static inline int decoder_order(CarrierPiece left, CarrierPiece right)
{
    const size_t shorter = (left.length < right.length) ? left.length : right.length;
    for (size_t at = 0u; at < shorter; at += 1u)
    {
        const unsigned char one = (unsigned char)left.text[at];
        const unsigned char other = (unsigned char)right.text[at];
        if (one != other)
        {
            return (one < other) ? -1 : 1;
        }
    }
    return (left.length == right.length) ? 0 : ((left.length < right.length) ? -1 : 1);
}

// the bytes of a pair's key at max, its two forms and the space between them, and its terminator
#define DECODER_KEY_BYTES 160u

// a pair's key: its two forms, the lesser first, a space between them
struct DecoderKey
{
    char text[DECODER_KEY_BYTES];
};

// the key of the pair of `first` and `second`, cut at the room
static inline DecoderKey decoder_key(CarrierPiece first, CarrierPiece second)
{
    const int first_less = decoder_order(first, second) < 0;
    const CarrierPiece lesser = first_less ? first : second;
    const CarrierPiece greater = first_less ? second : first;
    DecoderKey key;
    VerbaLinea line = mmgr_line_over(key.text, sizeof(key.text));
    (void)mmgr_line_put_n(&line, lesser.text, lesser.length);
    mmgr_line_ch(&line, ' ');
    (void)mmgr_line_put_n(&line, greater.text, greater.length);
    (void)mmgr_line_finish(&line);
    return key;
}

// What the decoding reads of one pair. That is its key, and the metas and the categories its texts read it into.
// Then the sets its answers read it into, and 1 where they read it into any. Then 1 where the log read its texts and
// where the lower witness answered it.
//
// Then how many of its links carried each flag of the trace, a bit for each flag counted. Then the values its forms
// write of their own, its product and its concept where the product is whole. Last comes its concept as the bridge
// folds it
struct DecoderPair
{
    DecoderKey key;
    unsigned int metas;
    unsigned int categories;
    unsigned int decoded;
    int decoded_held;
    int faceted;
    int witnessed;
    unsigned int choice[QUERY_TRACE_FLAGS_MAX];
    unsigned long long choice_held;
    CarrierNumbers qualifiers;
    int qualifiers_held;
    ConceptProduct product;
    int product_held;
    char identity[CONCEPT_IDENTITY_BYTES];
    int identity_held;
    CarrierPiece concept_text;
    int concept_held;
};

// One put of a form in another's place, as the log records it. It holds its pair, its two forms and its address. Then
// what came back of every case: `=` alike, `x` apart and `-` unbracketed, or `recorded` or `refused`. Then the case
// set the log put it over, and the lines of the log beneath it, which carry what it read and wrote. Each piece lies in
// the log [BORROWS]
struct LoggedPut
{
    unsigned long long pair;
    CarrierPiece from;
    CarrierPiece to;
    CarrierPiece address;
    CarrierPiece came_back;
    unsigned long long cases_at;
    CarrierPiece block;
};

// the operands a put names that only the form it was gives, at max
#define DECODER_ONLY_MAX 64u

// What the lines beneath a put give. That is the form's text and its stand-in's, and the operands only the form gives.
// Then each register the form reads that a case reaches, as the form writes it. With it, its value on every case as the
// part stores it at the form, empty where the part refused the read. Then the values the form and its stand-in write
// on every case, each read just after the form, in cells of the temporary tier. Last come the values the pair's two
// forms write of their own
struct PutView
{
    CarrierPiece from_text;
    CarrierPiece to_text;
    CarrierPiece only[DECODER_ONLY_MAX];
    unsigned int only_count;
    ConceptReads reads;
    unsigned long long *writes[2];
    unsigned long long write_count[2];
    CarrierNumbers qualifiers;
};

// the view's cells given back
static inline void put_view_ended(PutView *view)
{
    concept_reads_ended(&view->reads);
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        platform_cell_released(view->writes[side], platform_place_g_program_owner);
        view->writes[side] = NULL;
        view->write_count[side] = 0ULL;
    }
}

// the lines beneath `put` read into `view`, which put_view_ended gives back (decoder/puts.cu)
void put_viewed(const LoggedPut *put, PutView *view);
// the first word of a link's text up to its first `.`, its guard passed over [BORROWS]
CarrierPiece operation_stem(CarrierPiece text);
// the bit of the set its cases apart read a put into, the case set `cases` (decoder/puts.cu), or 0
unsigned int put_decoded(const LoggedPut *put, const PutView *view, CarrierPiece cases, const char *folder);

// the sets the bridge reads a pair into, beside the decoder's own, at max
#define DECODER_FOREIGN_MAX 32u

// The sets the bridge recorded beneath a pair: those among the decoder's names as bits, and the rest by their pieces
struct FoldSets
{
    unsigned int bits;
    CarrierPiece foreign[DECODER_FOREIGN_MAX];
    unsigned int foreign_count;
};

// The decoding of one log. It holds the log's case sets, each the piece after `cases `, its puts, and its pairs,
// ordered by key. Then the trace and the choices of every pair's puts: how many links carried each flag, a bit for each
// flag counted. Then the bridge's lines as the run stores it. Then the bridge written with what the decoding reads
// each pair into, and 1 where its text and the concepts' text begin.
//
// Then the pair the lines are beneath as the fold writes it, and what the bridge recorded beneath it. Last come the
// names written counted, each set's pairs, signal and noise, and the concepts written, each a line of their own
struct Decoding
{
    CarrierPiece *case_sets;
    unsigned long long case_set_count;
    LoggedPut *puts;
    unsigned long long put_count;
    DecoderPair *pairs;
    unsigned long long pair_count;
    QueryTrace trace;
    unsigned int chosen[QUERY_TRACE_FLAGS_MAX];
    unsigned long long chosen_held;
    CarrierPiece *bridge;
    unsigned long long bridge_count;
    PlatformText written;
    int texts_begun;
    DecoderPair *pair_beneath;
    FoldSets sets_beneath;
    unsigned int metas_beneath;
    CarrierPiece identity_beneath;
    CarrierPiece intent_beneath;
    unsigned int counted[DECODER_NAMES];
    unsigned int named_pairs[DECODER_NAMES];
    unsigned long long set_signal[DECODER_NAMES];
    unsigned long long set_noise[DECODER_NAMES];
    PlatformText concepts;
    // The log of the run at `qry` and the bridge at `klq` read into the pairs, puts, metas, categories and choices
    // (decoder/log.cu). 0, or 1 where the log, the trace or the bridge does not read, or the pool cannot meet a cell
    int log_read(const char *qry, const char *klq);
    // the pair of the key `key`, or NULL where the decoding holds none
    DecoderPair *pair_of(const DecoderKey &key) const;
    // each pair's set, product and identity decoded off its puts, the carriers at `engine`
    void products_decoded(const char *engine);
    // each pair's concept folded: a pair whose concept another pair shares (decoder/bridge.cu)
    void concepts_folded();
    // the pair the bridge's lines are beneath closed: what the decoding reads it into written beneath it and counted
    void pair_closed();
    // the bridge folded line by line, each pair closed in its turn
    void bridge_folded();
    // the bridge written passed to the run's writer for `klq`: 0, or 1 where the writer did not take it
    int bridge_passed(const char *klq);
    // each pair's lights, the choices, the concepts shared and each set's rows printed
    void printed(const char *klq);
    // every cell of the decoding given back
    void ended();
};

// the words of a line split at runs of space, as a stream reads them: their pieces and their count
#define DECODER_WORDS_MAX 64u

struct DecoderWords
{
    CarrierPiece word[DECODER_WORDS_MAX];
    unsigned int count;
};

static inline void decoder_words(CarrierPiece line, DecoderWords *words)
{
    words->count = 0u;
    size_t at = 0u;
    while ((at < line.length) && (words->count < DECODER_WORDS_MAX))
    {
        at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line.text[at], .cap = line.length - at,
                         .other = CARRIER_SPACES);
        const size_t start = at;
        at += (at < line.length) ? EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line.text[at],
                                              .cap = line.length - at, .other = CARRIER_SPACES)
                                 : 0u;
        if (at > start)
        {
            words->word[words->count].text = &line.text[start];
            words->word[words->count].length = at - start;
            words->count += 1u;
        }
    }
}

// a piece read as a number in hex, as strtoull reads one in base 16
static inline unsigned long long decoder_hex(CarrierPiece piece)
{
    char text[32];
    const size_t length = (piece.length < (sizeof(text) - 1u)) ? piece.length : (sizeof(text) - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = text, .src = piece.text, .bytes = length);
    text[length] = '\0';
    const int prefixed = (text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X'));
    return EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = prefixed ? &text[2] : text, .end = NULL, .base = 16);
}

// a cell of the temporary tier of `count` elements of `size` bytes, at least one [RETURNS OWNERSHIP]; NULL where the
// pool cannot meet it
static inline void *decoder_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_cleared((count != 0ULL) ? count : 1ULL, size, platform_place_g_program_owner,
                                 PLATFORM_TIER_TEMPORARY);
}

// the `count` elements of `size` bytes at `elements` put in the order of `order`, stable, through a spare cell of the
// temporary tier. 1, or 0 where the pool cannot meet the spare
static inline int decoder_sorted(void *elements, unsigned long long count, unsigned long long size,
                                 int (*order)(const void *left, const void *right))
{
    if (count < 2ULL)
    {
        return 1;
    }
    void *const spare = decoder_cell(count, size);
    if (spare == NULL)
    {
        return 0;
    }
    // the elements of one cell are far below what a size_t counts
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = elements, .bytes = (size_t)size, .scratch = spare,
                     .count = (size_t)count, .order = order);
    platform_cell_released(spare, platform_place_g_program_owner);
    return 1;
}

#endif
