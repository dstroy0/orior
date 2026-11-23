// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair.h: what the modules of the pair share: the work of one pair pass, and a link of a carrier a put logs its
// product at
#ifndef IDENTITY_PAIR_H
#define IDENTITY_PAIR_H

#include "identity_internal.h"

// each pair's question and places, stored with the pair and reached from its own thread, since a pair waits on its
// round with both
extern thread_local RunQuestion *t_question;
extern thread_local unsigned int *t_places;

// the names a pair's put reads at max, and names in the order of their bytes, each once, as a std::set holds them
#define PAIR_NAMES_MAX (FORM_NAMES_MAX * 2u)

struct PairNames
{
    IdentityName name[PAIR_NAMES_MAX];
    unsigned int count;
};

// 1 where `names` holds the terminated `name`
static inline int pair_name_held(const PairNames *names, const char *name)
{
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        if (identity_same(names->name[at].text, name))
        {
            return 1;
        }
    }
    return 0;
}

// the terminated `name` put into `names` in its order, where they do not hold it yet and hold fewer than PAIR_NAMES_MAX
static inline void pair_name_added(PairNames *names, const char *name)
{
    if (pair_name_held(names, name) || (names->count >= PAIR_NAMES_MAX))
    {
        return;
    }
    unsigned int at = names->count;
    while ((at > 0u) && (identity_order_text(names->name[at - 1u].text, name) > 0))
    {
        names->name[at] = names->name[at - 1u];
        at -= 1u;
    }
    (void)IDENTITY_SET_TEXT(names->name[at], name);
    names->count += 1u;
}

// what a link gives each of a form's parameters, by the parameter's name
struct PairGiven
{
    IdentityName name[FORM_NAMES_MAX];
    IdentityName value[FORM_NAMES_MAX];
    unsigned int count;
};

// the value `given` holds of the terminated `name`, or NULL where it holds none
static inline const IdentityName *pair_given_of(const PairGiven *given, const char *name)
{
    for (unsigned int at = 0u; at < given->count; at += 1u)
    {
        if (identity_same(given->name[at].text, name))
        {
            return &given->value[at];
        }
    }
    return NULL;
}

// the `length` bytes of `value` given to the terminated `name`, in place of what it gave the name before
static inline void pair_given_set(PairGiven *given, const char *name, const char *value, size_t length)
{
    unsigned int at = 0u;
    while ((at < given->count) && !identity_same(given->name[at].text, name))
    {
        at += 1u;
    }
    if (at >= FORM_NAMES_MAX)
    {
        return;
    }
    if (at == given->count)
    {
        (void)IDENTITY_SET_TEXT(given->name[at], name);
        given->count += 1u;
    }
    (void)IDENTITY_SET(given->value[at], value, length);
}

struct PairWork;

// A link of a carrier a form goes in at, as the put reads it. It holds the pair's two forms, the first's
// parameters and text, and the parameters the second reads. Then it holds whether the two unify, and the
// parameters bound to literals. Then the values the forms write of their own, and the cases the pair goes over.
// Then the question and places the pair's thread uses, and the product so far. Then the carrier's chain and the
// link found in it.
//
// Then it holds the operands the link gives by name, the carrier's flow and the link's position, and the stand-in
// written there. Last come the question's address and the carrier's delta before the form. Then whether a lesson
// blocks the stand-in, and whether the form reads in a shape no link read before
struct PutLink
{
    const char *from;
    const char *to;
    const FormNames *from_names;
    const char *from_text;
    const PairNames *read_by_to;
    int unified;
    const FormBinding *fixed;
    const CarrierNumbers *qualifiers;
    const ConceptCased *pair_cased;
    RunQuestion *question;
    unsigned int *places;
    ConceptProduct *product;
    const char *chain;
    const FoundLink *found;
    const PairGiven *given;
    const CarrierChain *flow;
    long link_at;
    const char *link_address;
    const char *located;
    const char *address;
    long delta;
    int are_in_blocked;
    int shape_new;
    // the put beneath its query in the log and the product queried at the link (pair_product.cu). 1 where a put added
    // a row that came back one way
    int logged(PairWork *work) const;
};

// The work of one pair pass. The host's answers to the carriers' cases, the ruleset, the path of the part's machine
// file, and the carriers. Then the seed the order of the links of one magnitude draws from. Then the bridge's text,
// its lines and its keys, the schema's forms in its order. Last, the values of the cases, and the counts of
// conferences convened, lessons taught and queries put
struct PairWork
{
    HostAnswers host;
    const Ruleset *sass;
    const char *machine;
    Carriers carriers;
    unsigned long seed;
    char *bridge_text;
    CarrierPiece *bridge;
    unsigned long long bridge_count;
    CarrierPiece *keys;
    unsigned long long key_count;
    ConceptCased cased;
    unsigned int conferences;
    unsigned int lessons_taught;
    unsigned long long queries;
    // every choice a put makes at a link, written to the log as `choice <flags> <link>` (pair.cu)
    void chosen(const char *const *names, unsigned int count, const char *link) const;
    void chose(const char *name, const char *link) const;
    // the conference a refused put calls (pair.cu): the overseer's guidance, OVERSEER_HANDS_OFF where no party called
    // a conference of `statement`
    unsigned int conferred(const char *from, const char *to, const char *statement, const char *located,
                           const char *carrier_name, unsigned int scratch, const RunQuestion *question,
                           const char *closed_here, const char *link_address);
    // `from` was in for by `to` at each of its links in each carrier (pair_put.cu): 1 with its verdict, 0 where the
    // part answered none
    int put(const char *from, const char *to, PairVerdict *verdict);
    // the lower witness of a pair whose puts queried nothing (pair_witness.cu). 1 with the verdict where the part
    // answered both forms at a link of a third, 0 otherwise
    int witness_lower(const char *first, const char *second, PairVerdict *verdict);
};

// the address of a link: its chain and sixteen times its line in hex, four digits at the fewest, into `address`
static inline void link_address_written(const char *chain, long link_at, char *address, size_t capacity)
{
    VerbaLinea line = mmgr_line_over(address, capacity);
    mmgr_line_put(&line, chain);
    mmgr_line_put(&line, ":");
    // a carrier's lines are far below 2^28
    mmgr_line_hex(&line, 16u * (unsigned int)link_at, 4u);
    (void)mmgr_line_finish(&line);
}

// 1 where `after` names the register `written` again, a register's name not followed by a digit
static inline int register_named_after(CarrierPiece after, CarrierPiece written)
{
    PlatformText source;
    identity_text_begun(&source);
    mmgr_line_put(&source.line, "(^|[^A-Za-z0-9_])");
    (void)mmgr_line_put_n(&source.line, written.text, written.length);
    mmgr_line_put(&source.line, "([^0-9]|$)");
    TablePattern pattern;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pattern, .bytes = sizeof(pattern), .val = 0);
    const int read = table_pattern_read(&pattern, platform_text_bytes(&source));
    platform_text_ended(&source);
    TableMatch match;
    const int found = read && table_search(&pattern, after.text, after.length, &match);
    if (read)
    {
        table_pattern_ended(&pattern);
    }
    return found;
}

// 1 where the shapes a put read so far, each terminated in `shapes`, hold the `length` bytes of `shape`
static inline int shape_held(PlatformText *shapes, const char *shape, size_t length)
{
    const unsigned long long size = platform_text_size(shapes);
    const char *const bytes = platform_text_bytes(shapes);
    for (unsigned long long at = 0ULL; (bytes != NULL) && (at < size);)
    {
        const size_t own = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = &bytes[at], .cap = (size_t)(size - at));
        if ((own == length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &bytes[at], .other = shape, .bytes = length) == 0))
        {
            return 1;
        }
        at += own + 1ULL;
    }
    return 0;
}

// the shape at `shape` put after the shapes `shapes` holds, terminated, where they do not hold it yet: 1 where it was
// new
static inline int shape_added(PlatformText *shapes, PlatformText *shape)
{
    const unsigned long long size = platform_text_size(shape);
    const char *const bytes = platform_text_bytes(shape);
    if (shape_held(shapes, (bytes != NULL) ? bytes : "", (size_t)size))
    {
        return 0;
    }
    identity_text_put(shapes, shape);
    mmgr_line_ch(&shapes->line, '\0');
    return 1;
}

// the register a found text writes: its part before the first `.`
static inline CarrierPiece register_of(CarrierPiece written)
{
    const char *const dot =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = written.text, .cap = written.length, .byte = '.');
    // the dot lies within the text
    const CarrierPiece base = {written.text, (dot != NULL) ? (size_t)(dot - written.text) : written.length};
    return base;
}

#endif
