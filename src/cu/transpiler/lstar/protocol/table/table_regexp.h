// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_regexp.h: the regular expressions of the table's tools over the regexp module (includes/regexp), in the
// ECMAScript syntax that both read.
//
// A pattern reads once into a work state that every read shares. The read then copies the nodes, items and parts
// it took into a cell of the pattern's own. The pattern keeps that cell to its end. A whole pattern reads as
// `^(?:<pattern>)$`, which matches a text only from its first character to its last.
//
// A text a pattern reads is its bytes, each a character. A search reads it from any place, and its answer is the
// first match at or past that place. A replacement writes each match's replacement in its place: `$&` the whole
// match, `$1` to `$9` a group's, and `$$` one `$`. It writes the text between the matches as it is. A match of no
// characters goes one character on
#ifndef TABLE_REGEXP_H
#define TABLE_REGEXP_H

#include "../../../../../../includes/regexp/regexp.h"
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include <stddef.h>

// the characters a text holds in the room of its own, past which it takes a cell of the temporary tier
#define TABLE_CHARACTERS_LOCAL 1024u

// a pattern read and ready to match: the regexp, the cell of its arrays, and 1 where it read
struct TablePattern
{
    Regexp regexp;
    void *cell;
    int read;
};

// The work state that every read shares. The calls of the table's tools share it, and a read copies out of it what it
// took
static RegexpWorkState s_table_work;

// the `source` read into `pattern` with its arrays in a cell of its own: 1, or 0 where it does not read
static inline int table_pattern_read(TablePattern *pattern, const char *source)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pattern, .bytes = sizeof(*pattern), .val = 0);
    const RegexpWorkStateCfg work = {&pattern->regexp, &s_table_work};
    regexp_on_capacity(&work);
    char why[256];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    const RegexpSourceCfg read = {&pattern->regexp, source, "", &error};
    if (!regexp_read(&read))
    {
        return 0;
    }
    Regexp *const regexp = &pattern->regexp;
    const size_t nodes = (size_t)regexp->nodes * sizeof(RegexpNode);
    const size_t items = (size_t)regexp->items * sizeof(RegexpItem);
    const size_t parts = (size_t)regexp->parts * sizeof(unsigned int);
    const size_t firsts = (size_t)regexp->parts * sizeof(RegexpFirst);
    // the four arrays one after another, each on a boundary of 16 bytes
    const size_t nodes_at = 0u;
    const size_t items_at = (nodes_at + nodes + 15u) & ~(size_t)15u;
    const size_t parts_at = (items_at + items + 15u) & ~(size_t)15u;
    const size_t firsts_at = (parts_at + parts + 15u) & ~(size_t)15u;
    const size_t bytes = firsts_at + firsts + 16u;
    char *const cell =
        (char *)platform_cell_cleared(bytes, 1ULL, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if (cell == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &cell[nodes_at], .src = regexp->node, .bytes = nodes);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &cell[items_at], .src = regexp->item, .bytes = items);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &cell[parts_at], .src = regexp->part, .bytes = parts);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &cell[firsts_at], .src = regexp->part_first, .bytes = firsts);
    // Explicit casts read each part of the cell as the array that it holds
    regexp->node = (RegexpNode *)(void *)&cell[nodes_at];
    regexp->item = (RegexpItem *)(void *)&cell[items_at];
    regexp->part = (unsigned int *)(void *)&cell[parts_at];
    regexp->part_first = (RegexpFirst *)(void *)&cell[firsts_at];
    pattern->cell = cell;
    pattern->read = 1;
    return 1;
}

// the `source` read as a whole pattern, `^(?:<source>)$`: 1, or 0 where it does not read
static inline int table_pattern_whole(TablePattern *pattern, const char *source)
{
    PlatformText whole;
    platform_text_begun(&whole, 1ULL << 20u, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    mmgr_line_put(&whole.line, "^(?:");
    mmgr_line_put(&whole.line, source);
    mmgr_line_put(&whole.line, ")$");
    const char *const bytes = platform_text_bytes(&whole);
    const int read = (bytes != NULL) && table_pattern_read(pattern, bytes);
    platform_text_ended(&whole);
    return read;
}

// the cell of the pattern given back
static inline void table_pattern_ended(TablePattern *pattern)
{
    platform_cell_released(pattern->cell, platform_place_g_program_owner);
    pattern->cell = NULL;
    pattern->read = 0;
}

// a text as a pattern reads it: its characters, in the room of its own or in a cell of the temporary tier
struct TableCharacters
{
    unsigned int local[TABLE_CHARACTERS_LOCAL];
    unsigned int *character;
    unsigned long long length;
    void *cell;
};

// the `length` bytes of `text` as characters, each a byte
static inline void table_characters_begun(TableCharacters *characters, const char *text, size_t length)
{
    characters->cell = NULL;
    characters->character = characters->local;
    if (length > TABLE_CHARACTERS_LOCAL)
    {
        characters->cell =
            platform_cell_taken(length * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        characters->character = (unsigned int *)characters->cell;
    }
    const size_t held = (characters->character != NULL) ? length : 0u;
    for (size_t at = 0u; at < held; at += 1u)
    {
        characters->character[at] = (unsigned char)text[at];
    }
    characters->length = held;
    if (characters->character == NULL)
    {
        characters->character = characters->local;
    }
}

static inline void table_characters_ended(TableCharacters *characters)
{
    platform_cell_released(characters->cell, platform_place_g_program_owner);
    characters->cell = NULL;
}

// a match: each group's span, the whole match first
struct TableMatch
{
    RegexpSpan group[REGEXP_GROUPS + 1u];
};

// The first match of `pattern` in `characters` at or past `from` into `match`. 1, or 0 where it matches nowhere there
static inline int table_found(const TablePattern *pattern, const TableCharacters *characters, unsigned long long from,
                              TableMatch *match)
{
    for (unsigned long long at = from; (pattern->read != 0) && (at <= characters->length); at += 1ULL)
    {
        const RegexpTextU32sCfg attempt = {&pattern->regexp, characters->character, characters->length, at,
                                           match->group};
        if (regexp_match_into(&attempt))
        {
            return 1;
        }
        // an anchored pattern matches at the text's first character alone
        if (pattern->regexp.anchored)
        {
            return 0;
        }
    }
    return 0;
}

// 1 where `pattern` matches somewhere in the `length` bytes of `text`, the match into `match`
static inline int table_search(const TablePattern *pattern, const char *text, size_t length, TableMatch *match)
{
    TableCharacters characters;
    table_characters_begun(&characters, text, length);
    const int found = table_found(pattern, &characters, 0ULL, match);
    table_characters_ended(&characters);
    return found;
}

// 1 where the whole pattern `pattern` (table_pattern_whole) matches the `length` bytes of `text`, the match into
// `match`
static inline int table_matched(const TablePattern *pattern, const char *text, size_t length, TableMatch *match)
{
    TableCharacters characters;
    table_characters_begun(&characters, text, length);
    const RegexpTextU32sCfg attempt = {&pattern->regexp, characters.character, characters.length, 0ULL, match->group};
    const int matched = (pattern->read != 0) && regexp_match_into(&attempt);
    table_characters_ended(&characters);
    return matched;
}

// the bytes of group `group` of `match` in `text`: its first byte and its count, an empty piece where it took no part
static inline const char *table_group(const TableMatch *match, unsigned int group, const char *text, size_t *length)
{
    const RegexpSpan *const span = &match->group[group];
    *length = span->set ? (size_t)(span->to - span->from) : 0u;
    return span->set ? &text[span->from] : text;
}

// A pattern kept by a caller and read on its first use: the pattern, and 1 once the first use tried a read. The
// caller's pattern is static, and every later use reads the one read
struct TablePatternOnce
{
    TablePattern pattern;
    int tried;
};

// the pattern of `once`, `source` read on the first use, a whole pattern where `whole` is 1 [BORROWS]
static inline const TablePattern *table_once(TablePatternOnce *once, const char *source, int whole)
{
    if (once->tried == 0)
    {
        once->tried = 1;
        (void)(whole ? table_pattern_whole(&once->pattern, source) : table_pattern_read(&once->pattern, source));
    }
    return &once->pattern;
}

// a walk over every match of a pattern in a text, as std::sregex_iterator walks. It holds the pattern, the text
// and its characters, the place the next search starts at, and the match the walk is at
struct TableWalk
{
    const TablePattern *pattern;
    const char *text;
    TableCharacters characters;
    unsigned long long at;
    TableMatch match;
};

static inline void table_walk_begun(TableWalk *walk, const TablePattern *pattern, const char *text, size_t length)
{
    walk->pattern = pattern;
    walk->text = text;
    table_characters_begun(&walk->characters, text, length);
    walk->at = 0ULL;
}

// The walk on to its next match: 1, or 0 where the text holds no match past it. A match of no characters takes
// the search one character on
static inline int table_walk_next(TableWalk *walk)
{
    if ((walk->at > walk->characters.length) || !table_found(walk->pattern, &walk->characters, walk->at, &walk->match))
    {
        walk->at = walk->characters.length + 1ULL;
        return 0;
    }
    const RegexpSpan whole = walk->match.group[0];
    walk->at = (whole.to > whole.from) ? whole.to : (whole.to + 1ULL);
    if (walk->pattern->regexp.anchored)
    {
        walk->at = walk->characters.length + 1ULL;
    }
    return 1;
}

static inline void table_walk_ended(TableWalk *walk)
{
    table_characters_ended(&walk->characters);
}

// the bytes of group `group` of the walk's match [BORROWS]
static inline const char *table_walk_group(const TableWalk *walk, unsigned int group, size_t *length)
{
    return table_group(&walk->match, group, walk->text, length);
}

// The `length` bytes of `text` with each match of `pattern` replaced by `replacement`, onto `out`
static inline void table_replaced(const TablePattern *pattern, const char *text, size_t length, const char *replacement,
                                  PlatformText *out)
{
    TableCharacters characters;
    table_characters_begun(&characters, text, length);
    unsigned long long at = 0ULL;
    TableMatch match;
    while ((at <= characters.length) && table_found(pattern, &characters, at, &match))
    {
        const RegexpSpan whole = match.group[0];
        (void)mmgr_line_put_n(&out->line, &text[at], (size_t)(whole.from - at));
        for (const char *each = replacement; *each != '\0'; each += 1)
        {
            if ((each[0] == '$') && (each[1] == '&'))
            {
                (void)mmgr_line_put_n(&out->line, &text[whole.from], (size_t)(whole.to - whole.from));
                each += 1;
            }
            else if ((each[0] == '$') && (each[1] >= '1') && (each[1] <= '9'))
            {
                size_t group_length = 0u;
                const char *const group = table_group(&match, (unsigned int)(each[1] - '0'), text, &group_length);
                (void)mmgr_line_put_n(&out->line, group, group_length);
                each += 1;
            }
            else if ((each[0] == '$') && (each[1] == '$'))
            {
                mmgr_line_ch(&out->line, '$');
                each += 1;
            }
            else
            {
                mmgr_line_ch(&out->line, each[0]);
            }
        }
        if (whole.to == whole.from)
        {
            // a match of no characters: the character after it goes as it is, and the search goes on past it
            if (whole.to < characters.length)
            {
                mmgr_line_ch(&out->line, text[whole.to]);
            }
            at = whole.to + 1ULL;
        }
        else
        {
            at = whole.to;
        }
        if (pattern->regexp.anchored)
        {
            break;
        }
    }
    if (at < characters.length)
    {
        (void)mmgr_line_put_n(&out->line, &text[at], (size_t)(characters.length - at));
    }
    table_characters_ended(&characters);
}

#endif
