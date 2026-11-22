// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read.c: a pattern read into nodes: alternation, sequence, quantifiers, escapes, classes and atoms, as
// JavaScript's syntax writes them; and each alternation's first characters and each repeat's one way filled in once
// the pattern is read. Each read takes a reader of its own from the cells, as patterns are read on many threads
#include "regexp_match_internal.h"
#include "regexp_read_internal.h"

unsigned long long regexp_characters(const RegexpTextCfg *args)
{
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < args->size) && (count < args->capacity_u64))
    {
        const unsigned char byte = (unsigned char)args->text[at];
        unsigned int length;
        if (byte < 0x80u)
        {
            length = 1u;
        }
        else if (byte < 0xe0u)
        {
            length = 2u;
        }
        else if (byte < 0xf0u)
        {
            length = 3u;
        }
        else
        {
            length = 4u;
        }
        unsigned int character;
        if (length == 1u)
        {
            character = byte;
        }
        else if (length == 2u)
        {
            character = byte & 0x1fu;
        }
        else if (length == 3u)
        {
            character = byte & 0x0fu;
        }
        else
        {
            character = byte & 0x07u;
        }
        for (unsigned int more = 1u; (more < length) && ((at + more) < args->size); more += 1u)
        {
            character = (character << 6u) | ((unsigned char)args->text[at + more] & 0x3fu);
        }
        args->characters[count] = character;
        count += 1ULL;
        at += length;
    }
    return count;
}

// `/<source>/: <why> at <place>` reported: REGEXP_FAILED
unsigned int regexp_fail(const RegexpReadErrorCtx *args)
{
    if (!args->reader->failed)
    {
        mmgr_line_ch(args->reader->err_msg, '/');
        mmgr_line_put(args->reader->err_msg, args->reader->source);
        mmgr_line_put(args->reader->err_msg, "/: ");
        mmgr_line_put(args->reader->err_msg, args->why);
        mmgr_line_put(args->reader->err_msg, " at ");
        mmgr_line_decimal(args->reader->err_msg, args->reader->at, 0u);
    }
    args->reader->failed = 1;
    return REGEXP_FAILED;
}

int regexp_peek(const RegexpReadLetterTypeCtx *args)
{
    unsigned int *character = args->character;
    if (args->reader_read->at >= args->reader_read->length)
    {
        return 0;
    }
    *character = args->reader_read->characters[args->reader_read->at];
    return 1;
}

int regexp_next(const RegexpReadLetterCtx *args)
{
    const int there = EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx,
                                 .reader_read = args->reader, .character = args->character);
    args->reader->at += there ? 1u : 0u;
    return there;
}

int regexp_eat(const RegexpReadCtx *args)
{
    unsigned int character = 0u;
    if (EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = args->reader,
                   .character = &character) &&
        (character == args->required))
    {
        args->reader->at += 1u;
        return 1;
    }
    return 0;
}

// a node of `type` made, its fields zero but its group none: its index, or REGEXP_FAILED where the pattern contains
// as many as it can
unsigned int regexp_node(const RegexpReadTypeCtx *args)
{
    unsigned int type = args->type;
    Regexp *const regexp = args->reader->regexp;
    if (regexp->nodes >= REGEXP_NODES)
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                          .why = "more nodes than a pattern allows");
    }
    const unsigned int made = regexp->nodes;
    memor.set(&(MemoriaCfg){.dst = &regexp->node[made], .val = 0, .bytes = sizeof(regexp->node[made])});
    regexp->node[made].type = type;
    regexp->node[made].group = REGEXP_NONE;
    regexp->nodes += 1u;
    return made;
}

// the nodes `children`, `count` of them, put in the parts as one node of `type`: its index
unsigned int regexp_parent(const RegexpReadTypeCtx *args)
{
    unsigned int count = args->count;
    Regexp *const regexp = args->reader->regexp;
    if ((regexp->parts + count) > REGEXP_PARTS)
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                          .why = "more parts than a pattern allows");
    }
    const unsigned int made = regexp_read_type_internal.node(args);
    if (made == REGEXP_FAILED)
    {
        return made;
    }
    regexp->node[made].first = regexp->parts;
    regexp->node[made].count = count;
    memor.cpy(&(MemoriaCfg){
        .dst = &regexp->part[regexp->parts], .src = args->children, .bytes = count * sizeof(args->children[0])});
    regexp->parts += count;
    return made;
}

int regexp_number(const RegexpReadValueCtx *args)
{
    unsigned int *value = args->value;
    const unsigned long long start = args->reader->at;
    unsigned long long read = 0ULL;
    unsigned int character = 0u;
    while (EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = args->reader,
                      .character = &character) &&
           (character >= '0') && (character <= '9'))
    {
        read = (read * 10ULL) + (character - '0');
        args->reader->at += 1u;
    }
    // a count past what an unsigned int represents reads as the max
    *value = (read < REGEXP_UNBOUNDED) ? (unsigned int)read : (REGEXP_UNBOUNDED - 1u);
    return args->reader->at != start;
}

typedef struct
{
    RegexpReader *reader;
    unsigned int atom;
    Regexp *regexp;
    const Regexp *regexp_read;
    unsigned int index;
    const char *source;
    const char *flags;
    VerbaLinea *err_msg;
} RegexpReadCtxState;
// the quantifier after `atom`, where one is: `* + ? {n} {n,} {n,m}`, lazy after `?`; `{` that opens no quantifier
// is the atom's alone
EMBED_INLINE unsigned int regexp_quantified(const RegexpReadCtxState *args)
{
    RegexpReader *reader = args->reader;
    unsigned int character = 0u;
    unsigned int min = 0u;
    unsigned int max = REGEXP_UNBOUNDED;
    if (!EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = reader,
                    .character = &character))
    {
        return args->atom;
    }
    switch (character)
    {
    case '*': {
        min = 0u;
        break;
    }
    case '+': {
        min = 1u;
        break;
    }
    case '?': {
        max = 1u;
        break;
    }
    case '{': {
        const unsigned long long back = reader->at;
        reader->at += 1u;
        if (!EMBED_CALL(regexp_read_value_internal.number, RegexpReadValueCtx, .reader = reader, .value = &min))
        {
            reader->at = back;
            return args->atom;
        }
        max = min;
        if (EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = ','))
        {
            unsigned int peeked = 0u;
            const int closes = EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx,
                                          .reader_read = reader, .character = &peeked) &&
                               (peeked == '}');
            max = (!closes &&
                   EMBED_CALL(regexp_read_value_internal.number, RegexpReadValueCtx, .reader = reader, .value = &max))
                      ? max
                      : REGEXP_UNBOUNDED;
        }
        if (!EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '}'))
        {
            reader->at = back;
            return args->atom;
        }
        reader->at -= 1u;
        break;
    }
    default: {
        return args->atom;
    }
    }
    reader->at += 1u;
    const unsigned int type = reader->regexp->node[args->atom].type;
    if ((type == REGEXP_START) || (type == REGEXP_END) || (type == REGEXP_BOUNDARY) || (type == REGEXP_LOOK))
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                          .why = "a quantifier after an assertion");
    }
    const int greedy = !EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '?');
    const unsigned int made =
        EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = REGEXP_REPEAT);
    if (made != REGEXP_FAILED)
    {
        RegexpNode *const node = &reader->regexp->node[made];
        node->child = args->atom;
        node->min = min;
        node->max = max;
        node->greedy = greedy;
    }
    return made;
}

// a sequence of atoms, each with its quantifier, up to `|` or `)`
EMBED_INLINE unsigned int regexp_sequence(const RegexpReadCtxState *args)
{
    RegexpReader *reader = args->reader;
    unsigned int parts[REGEXP_LEVEL_MAX];
    unsigned int count = 0u;
    unsigned int character = 0u;
    while (EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = reader,
                      .character = &character) &&
           (character != '|') && (character != ')'))
    {
        const unsigned int atom = EMBED_CALL(regexp_read_internal.atom, RegexpReadCtx, .reader = reader);
        const unsigned int quantified =
            (atom == REGEXP_FAILED) ? atom
                                    : EMBED_CALL(regexp_quantified, RegexpReadCtxState, .reader = reader, .atom = atom);
        if (quantified == REGEXP_FAILED)
        {
            return quantified;
        }
        if (count >= REGEXP_LEVEL_MAX)
        {
            return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                              .why = "more parts than a sequence allows");
        }
        parts[count] = quantified;
        count += 1u;
    }
    if (count == 0u)
    {
        return EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = REGEXP_EMPTY);
    }
    return (count == 1u) ? parts[0]
                         : EMBED_CALL(regexp_read_type_internal.parent, RegexpReadTypeCtx, .reader = reader,
                                      .type = REGEXP_CONCAT, .children = parts, .count = count);
}

unsigned int regexp_alternation(const RegexpReadCtx *args)
{
    unsigned int options[REGEXP_LEVEL_MAX];
    unsigned int count = 0u;
    do
    {
        const unsigned int option = EMBED_CALL(regexp_sequence, RegexpReadCtxState, .reader = args->reader);
        if (option == REGEXP_FAILED)
        {
            return option;
        }
        if (count >= REGEXP_LEVEL_MAX)
        {
            return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                              .why = "more alternatives than an alternation allows");
        }
        options[count] = option;
        count += 1u;
    } while (EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = args->reader, .required = '|'));
    return (count == 1u) ? options[0]
                         : EMBED_CALL(regexp_read_type_internal.parent, RegexpReadTypeCtx, .reader = args->reader,
                                      .type = REGEXP_ALT, .children = options, .count = count);
}

// Each alternative of every alternation gets its first characters, and a match passes over a place whose character
// none of them can start with. Each repeat gets whether a pass takes the text one way or none. Each one-character
// node gets the ASCII characters it takes, as bits, and the matcher tests a character below 128 in one step
EMBED_INLINE void regexp_ascii_know(const RegexpReadCtxState *args)
{
    Regexp *regexp = args->regexp;
    for (unsigned int index = 0u; index < regexp->nodes; index += 1u)
    {
        RegexpNode *const node = &regexp->node[index];
        if ((node->type != REGEXP_CHAR) && (node->type != REGEXP_ANY) && (node->type != REGEXP_CLASS))
        {
            continue;
        }
        node->ascii[0] = 0ULL;
        node->ascii[1] = 0ULL;
        node->ascii_known = 0;
        for (unsigned int character = 0u; character < 128u; character += 1u)
        {
            node->ascii[character / 64u] |= (EMBED_CALL(regexp_match_internal.regexp_one, RegexpMatchCtx,
                                                        .regexp = regexp, .index = index, .character = character) == 1)
                                                ? (1ULL << (character % 64u))
                                                : 0ULL;
        }
        node->ascii_known = 1;
    }
}

// whether a match of the node must start at the text's first character: the node is ^, or a group or sequence that
// starts with one
static int regexp_anchor(const RegexpReadCtxState *args)
{
    const RegexpNode *const node = &args->regexp_read->node[args->index];
    if (node->type == REGEXP_START)
    {
        return 1;
    }
    if (node->type == REGEXP_GROUP)
    {
        return EMBED_CALL(regexp_anchor, RegexpReadCtxState, .regexp_read = args->regexp_read, .index = node->child);
    }
    return (node->type == REGEXP_CONCAT) && (node->count > 0u) &&
           EMBED_CALL(regexp_anchor, RegexpReadCtxState, .regexp_read = args->regexp_read,
                      .index = args->regexp_read->part[node->first]);
}

static void regexp_on_fill(const RegexpReadCtxState *args)
{
    Regexp *regexp = args->regexp;
    unsigned int index = args->index;
    RegexpNode *const node = &regexp->node[index];
    if ((node->type == REGEXP_ALT) || (node->type == REGEXP_CONCAT))
    {
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            EMBED_CALL(regexp_on_fill, RegexpReadCtxState, .regexp = regexp, .index = regexp->part[node->first + at]);
        }
        for (unsigned int at = 0u; (node->type == REGEXP_ALT) && (at < node->count); at += 1u)
        {
            regexp->part_first[node->first + at] = EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = regexp,
                                                              .index = regexp->part[node->first + at]);
        }
    }
    else if (node->type == REGEXP_REPEAT)
    {
        EMBED_CALL(regexp_on_fill, RegexpReadCtxState, .regexp = regexp, .index = node->child);
        node->one_way = EMBED_CALL(regexp_index.one_way, RegexpIndexCfg, .regexp = regexp, .index = node->child);
    }
    else if ((node->type == REGEXP_GROUP) || (node->type == REGEXP_LOOK))
    {
        EMBED_CALL(regexp_on_fill, RegexpReadCtxState, .regexp = regexp, .index = node->child);
    }
}

void regexp_on_capacity(const RegexpWorkStateCfg *args)
{
    args->regexp_out->node = args->work_state->node;
    args->regexp_out->item = args->work_state->item;
    args->regexp_out->part = args->work_state->part;
    args->regexp_out->part_first = args->work_state->part_first;
}

// the pattern `source` with `flags` read into `regexp` by `reader`, made empty: 1, or 0 with why given
EMBED_INLINE int regexp_read_by(const RegexpReadCtxState *args)
{
    RegexpReader *reader = args->reader;
    Regexp *regexp = args->regexp;
    const char *source = args->source;
    VerbaLinea *err_msg = args->err_msg;
    reader->length = EMBED_CALL(regexp_text.characters, RegexpTextCfg, .text = source,
                                .size = cellul.len(&(CatenaFinitaCfg){.src = source, .cap = REGEXP_TEXT_CAP}),
                                .characters = reader->characters, .capacity_u64 = REGEXP_CHARACTERS);
    reader->unicode = cellul.chr(&(CatenaFinitaCfg){.src = args->flags, .cap = REGEXP_TEXT_CAP, .byte = 'u'}) != NULL;
    reader->regexp = regexp;
    reader->source = source;
    reader->err_msg = err_msg;
    regexp->nodes = 0u;
    regexp->items = 0u;
    regexp->parts = 0u;
    regexp->fold = cellul.chr(&(CatenaFinitaCfg){.src = args->flags, .cap = REGEXP_TEXT_CAP, .byte = 'i'}) != NULL;
    const unsigned int root = EMBED_CALL(regexp_read_internal.alternation, RegexpReadCtx, .reader = reader);
    if (root == REGEXP_FAILED)
    {
        return 0;
    }
    if (reader->at < reader->length)
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader, .why = "an unmatched )");
        return 0;
    }
    regexp->root = root;
    regexp->groups = reader->groups;
    EMBED_CALL(regexp_on_fill, RegexpReadCtxState, .regexp = regexp, .index = root);
    regexp->first = EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = regexp, .index = root);
    EMBED_CALL(regexp_ascii_know, RegexpReadCtxState, .regexp = regexp);
    regexp->anchored = EMBED_CALL(regexp_anchor, RegexpReadCtxState, .regexp_read = regexp, .index = root);
    return 1;
}

int regexp_read(const RegexpSourceCfg *args)
{
    // many threads read patterns at once, each with a reader in a cell of its own
    RegexpReader *const reader = regexp_cell_taken(sizeof(RegexpReader));
    if (reader == NULL)
    {
        mmgr_line_put(args->err_msg, "the pattern could not be read: the cells cannot meet it");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = reader, .val = 0, .bytes = sizeof(*reader)});
    const int read = EMBED_CALL(regexp_read_by, RegexpReadCtxState, .reader = reader, .regexp = args->regexp_out,
                                .source = args->source, .flags = args->flags, .err_msg = args->err_msg);
    regexp_cell_released(reader);
    return read;
}
