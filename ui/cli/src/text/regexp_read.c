// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read.c: a pattern read into nodes: alternation, sequence, quantifiers, escapes, classes and atoms, as
// JavaScript's syntax writes them; and each alternation's first characters and each repeat's one way filled in once
// the pattern is read. Each read takes a reader of its own from the cells, as patterns are read on many threads
#include "../command_line/cells.h"
#include "regexp_match_internal.h"
#include "regexp_read_internal.h"

unsigned long long cli_regexp_letters(const CliRegexpTextCfg *args)
{
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at < args->size) && (count < args->capacity_u64))
    {
        const unsigned char byte = (unsigned char)args->text[at];
        const unsigned int length = (byte < 0x80u) ? 1u : (byte < 0xe0u) ? 2u : (byte < 0xf0u) ? 3u : 4u;
        unsigned int letter = (length == 1u)   ? byte
                              : (length == 2u) ? (byte & 0x1fu)
                              : (length == 3u) ? (byte & 0x0fu)
                                               : (byte & 0x07u);
        for (unsigned int more = 1u; (more < length) && ((at + more) < args->size); more += 1u)
        {
            letter = (letter << 6u) | ((unsigned char)args->text[at + more] & 0x3fu);
        }
        args->letters[count] = letter;
        count += 1ull;
        at += length;
    }
    return count;
}

// `/<source>/: <why> at <place>` reported: CLI_REGEXP_FAILED
unsigned int cli_fail(const CliRegexpReadErrorCtx *args)
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
    return CLI_REGEXP_FAILED;
}

int cli_peek(const CliRegexpReadLetterKindCtx *args)
{
    unsigned int *letter = args->letter;
    if (args->reader_read->at >= args->reader_read->length)
    {
        return 0;
    }
    *letter = args->reader_read->letters[args->reader_read->at];
    return 1;
}

int cli_next(const CliRegexpReadLetterCtx *args)
{
    const int there = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                 .reader_read = args->reader, .letter = args->letter);
    args->reader->at += there ? 1u : 0u;
    return there;
}

int cli_eat(const CliRegexpReadCtx *args)
{
    unsigned int letter = 0u;
    if (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx, .reader_read = args->reader,
                   .letter = &letter) &&
        (letter == args->required))
    {
        args->reader->at += 1u;
        return 1;
    }
    return 0;
}

// a node of `kind` made, its fields zero but its group none: its index, or CLI_REGEXP_FAILED where the pattern contains
// as many as it can
unsigned int cli_node(const CliRegexpReadKindCtx *args)
{
    unsigned int kind = args->kind;
    CliRegexp *const regexp = args->reader->regexp;
    if (regexp->nodes >= CLI_REGEXP_NODES)
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                          .why = "more nodes than a pattern allows");
    }
    const unsigned int made = regexp->nodes;
    memor.set(&(MemoriaCfg){.dst = &regexp->node[made], .val = 0, .bytes = sizeof(regexp->node[made])});
    regexp->node[made].kind = kind;
    regexp->node[made].group = CLI_REGEXP_NONE;
    regexp->nodes += 1u;
    return made;
}

// the nodes `children`, `count` of them, put in the parts as one node of `kind`: its index
unsigned int cli_parent(const CliRegexpReadKindCtx *args)
{
    unsigned int count = args->count;
    CliRegexp *const regexp = args->reader->regexp;
    if ((regexp->parts + count) > CLI_REGEXP_PARTS)
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                          .why = "more parts than a pattern allows");
    }
    const unsigned int made = cli_regexp_read_kind_internal.node(args);
    if (made == CLI_REGEXP_FAILED)
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

int cli_number(const CliRegexpReadValueCtx *args)
{
    unsigned int *value = args->value;
    const unsigned long long start = args->reader->at;
    unsigned long long read = 0ull;
    unsigned int letter = 0u;
    while (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                      .reader_read = args->reader, .letter = &letter) &&
           (letter >= '0') && (letter <= '9'))
    {
        read = (read * 10ull) + (letter - '0');
        args->reader->at += 1u;
    }
    // a count past what an unsigned int represents is read as the most
    *value = (read < CLI_REGEXP_UNBOUNDED) ? (unsigned int)read : (CLI_REGEXP_UNBOUNDED - 1u);
    return args->reader->at != start;
}

typedef struct
{
    CliRegexpReader *reader;
    unsigned int atom;
    CliRegexp *regexp;
    const CliRegexp *regexp_read;
    unsigned int index;
    const char *source;
    const char *flags;
    VerbaLinea *err_msg;
} RegexpReadCtx;
// the quantifier after `atom`, where one stands: `* + ? {n} {n,} {n,m}`, lazy after `?`; `{` that opens no quantifier
// is the atom's alone
EMBED_INLINE unsigned int cli_quantified(const RegexpReadCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int letter = 0u;
    unsigned int min = 0u;
    unsigned int max = CLI_REGEXP_UNBOUNDED;
    if (!EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx, .reader_read = reader,
                    .letter = &letter))
    {
        return args->atom;
    }
    if (letter == '*')
    {
        min = 0u;
    }
    else if (letter == '+')
    {
        min = 1u;
    }
    else if (letter == '?')
    {
        max = 1u;
    }
    else if (letter == '{')
    {
        const unsigned long long back = reader->at;
        reader->at += 1u;
        if (!EMBED_CALL(cli_regexp_read_value_internal.number, CliRegexpReadValueCtx, .reader = reader, .value = &min))
        {
            reader->at = back;
            return args->atom;
        }
        max = min;
        if (EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = ','))
        {
            unsigned int peeked = 0u;
            const int closes = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                          .reader_read = reader, .letter = &peeked) &&
                               (peeked == '}');
            max = (!closes && EMBED_CALL(cli_regexp_read_value_internal.number, CliRegexpReadValueCtx, .reader = reader,
                                         .value = &max))
                      ? max
                      : CLI_REGEXP_UNBOUNDED;
        }
        if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '}'))
        {
            reader->at = back;
            return args->atom;
        }
        reader->at -= 1u;
    }
    else
    {
        return args->atom;
    }
    reader->at += 1u;
    const unsigned int kind = reader->regexp->node[args->atom].kind;
    if ((kind == CLI_REGEXP_START) || (kind == CLI_REGEXP_END) || (kind == CLI_REGEXP_BOUNDARY) ||
        (kind == CLI_REGEXP_LOOK))
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                          .why = "a quantifier after an assertion");
    }
    const int greedy = !EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '?');
    const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = reader,
                                         .kind = CLI_REGEXP_REPEAT);
    if (made != CLI_REGEXP_FAILED)
    {
        CliRegexpNode *const node = &reader->regexp->node[made];
        node->child = args->atom;
        node->min = min;
        node->max = max;
        node->greedy = greedy;
    }
    return made;
}

// a sequence of atoms, each with its quantifier, up to `|` or `)`
EMBED_INLINE unsigned int cli_sequence(const RegexpReadCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int parts[CLI_REGEXP_LEVEL_MAX];
    unsigned int count = 0u;
    unsigned int letter = 0u;
    while (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx, .reader_read = reader,
                      .letter = &letter) &&
           (letter != '|') && (letter != ')'))
    {
        const unsigned int atom = EMBED_CALL(cli_regexp_read_internal.atom, CliRegexpReadCtx, .reader = reader);
        const unsigned int quantified = (atom == CLI_REGEXP_FAILED)
                                            ? atom
                                            : EMBED_CALL(cli_quantified, RegexpReadCtx, .reader = reader, .atom = atom);
        if (quantified == CLI_REGEXP_FAILED)
        {
            return quantified;
        }
        if (count >= CLI_REGEXP_LEVEL_MAX)
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                              .why = "more parts than a sequence allows");
        }
        parts[count] = quantified;
        count += 1u;
    }
    if (count == 0u)
    {
        return EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = reader,
                          .kind = CLI_REGEXP_EMPTY);
    }
    return (count == 1u) ? parts[0]
                         : EMBED_CALL(cli_regexp_read_kind_internal.parent, CliRegexpReadKindCtx, .reader = reader,
                                      .kind = CLI_REGEXP_CONCAT, .children = parts, .count = count);
}

unsigned int cli_alternation(const CliRegexpReadCtx *args)
{
    unsigned int options[CLI_REGEXP_LEVEL_MAX];
    unsigned int count = 0u;
    do
    {
        const unsigned int option = EMBED_CALL(cli_sequence, RegexpReadCtx, .reader = args->reader);
        if (option == CLI_REGEXP_FAILED)
        {
            return option;
        }
        if (count >= CLI_REGEXP_LEVEL_MAX)
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                              .why = "more alternatives than an alternation allows");
        }
        options[count] = option;
        count += 1u;
    } while (EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = args->reader, .required = '|'));
    return (count == 1u)
               ? options[0]
               : EMBED_CALL(cli_regexp_read_kind_internal.parent, CliRegexpReadKindCtx, .reader = args->reader,
                            .kind = CLI_REGEXP_ALT, .children = options, .count = count);
}

// Each alternative of every alternation given its first characters, a place whose character none of them can start
// with passed over; each repeat whether a pass takes the text one way or none
// the ASCII letters each one-letter node takes, as bits, and the matcher tests a letter below 128 in one step
EMBED_INLINE void cli_ascii_know(const RegexpReadCtx *args)
{
    CliRegexp *regexp = args->regexp;
    for (unsigned int index = 0u; index < regexp->nodes; index += 1u)
    {
        CliRegexpNode *const node = &regexp->node[index];
        if ((node->kind != CLI_REGEXP_CHAR) && (node->kind != CLI_REGEXP_ANY) && (node->kind != CLI_REGEXP_CLASS))
        {
            continue;
        }
        node->ascii[0] = 0ull;
        node->ascii[1] = 0ull;
        node->ascii_known = 0;
        for (unsigned int letter = 0u; letter < 128u; letter += 1u)
        {
            node->ascii[letter / 64u] |= (EMBED_CALL(cli_regexp_match_internal.regexp_one, CliRegexpMatchCtx,
                                                     .regexp = regexp, .index = index, .letter = letter) == 1)
                                             ? (1ull << (letter % 64u))
                                             : 0ull;
        }
        node->ascii_known = 1;
    }
}

// whether a match of the node must start at the text's first letter: the node is ^, or a group or sequence that
// starts with one
static int cli_anchor(const RegexpReadCtx *args)
{
    const CliRegexpNode *const node = &args->regexp_read->node[args->index];
    if (node->kind == CLI_REGEXP_START)
    {
        return 1;
    }
    if (node->kind == CLI_REGEXP_GROUP)
    {
        return EMBED_CALL(cli_anchor, RegexpReadCtx, .regexp_read = args->regexp_read, .index = node->child);
    }
    return (node->kind == CLI_REGEXP_CONCAT) && (node->count > 0u) &&
           EMBED_CALL(cli_anchor, RegexpReadCtx, .regexp_read = args->regexp_read,
                      .index = args->regexp_read->part[node->first]);
}

static void cli_on_fill(const RegexpReadCtx *args)
{
    CliRegexp *regexp = args->regexp;
    unsigned int index = args->index;
    CliRegexpNode *const node = &regexp->node[index];
    if ((node->kind == CLI_REGEXP_ALT) || (node->kind == CLI_REGEXP_CONCAT))
    {
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            EMBED_CALL(cli_on_fill, RegexpReadCtx, .regexp = regexp, .index = regexp->part[node->first + at]);
        }
        for (unsigned int at = 0u; (node->kind == CLI_REGEXP_ALT) && (at < node->count); at += 1u)
        {
            regexp->part_first[node->first + at] =
                EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = regexp,
                           .index = regexp->part[node->first + at]);
        }
    }
    else if (node->kind == CLI_REGEXP_REPEAT)
    {
        EMBED_CALL(cli_on_fill, RegexpReadCtx, .regexp = regexp, .index = node->child);
        node->one_way = EMBED_CALL(cli_regexp_index.one_way, CliRegexpIndexCfg, .regexp = regexp, .index = node->child);
    }
    else if ((node->kind == CLI_REGEXP_GROUP) || (node->kind == CLI_REGEXP_LOOK))
    {
        EMBED_CALL(cli_on_fill, RegexpReadCtx, .regexp = regexp, .index = node->child);
    }
}

void cli_regexp_on_capacity(const CliRegexpWorkStateCfg *args)
{
    args->regexp_out->node = args->work_state->node;
    args->regexp_out->item = args->work_state->item;
    args->regexp_out->part = args->work_state->part;
    args->regexp_out->part_first = args->work_state->part_first;
}

// the pattern `source` with `flags` read into `regexp` by `reader`, made empty: 1, or 0 with why given
EMBED_INLINE int cli_regexp_read_by(const RegexpReadCtx *args)
{
    CliRegexpReader *reader = args->reader;
    CliRegexp *regexp = args->regexp;
    const char *source = args->source;
    VerbaLinea *err_msg = args->err_msg;
    reader->length = EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = source,
                                .size = cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}),
                                .letters = reader->letters, .capacity_u64 = CLI_REGEXP_LETTERS);
    reader->unicode = cellul.chr(&(CatenaFinitaCfg){.src = args->flags, .cap = CLI_CELLS_POOL, .byte = 'u'}) != NULL;
    reader->regexp = regexp;
    reader->source = source;
    reader->err_msg = err_msg;
    regexp->nodes = 0u;
    regexp->items = 0u;
    regexp->parts = 0u;
    regexp->fold = cellul.chr(&(CatenaFinitaCfg){.src = args->flags, .cap = CLI_CELLS_POOL, .byte = 'i'}) != NULL;
    const unsigned int root = EMBED_CALL(cli_regexp_read_internal.alternation, CliRegexpReadCtx, .reader = reader);
    if (root == CLI_REGEXP_FAILED)
    {
        return 0;
    }
    if (reader->at < reader->length)
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "an unmatched )");
        return 0;
    }
    regexp->root = root;
    regexp->groups = reader->groups;
    EMBED_CALL(cli_on_fill, RegexpReadCtx, .regexp = regexp, .index = root);
    regexp->first = EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = regexp, .index = root);
    EMBED_CALL(cli_ascii_know, RegexpReadCtx, .regexp = regexp);
    regexp->anchored = EMBED_CALL(cli_anchor, RegexpReadCtx, .regexp_read = regexp, .index = root);
    return 1;
}

int cli_regexp_read(const CliRegexpSourceCfg *args)
{
    // patterns are read on many threads at once, each by a reader in a cell of its own
    CliRegexpReader *const reader = g_cli_cells->temporary_buf_alloc(sizeof(CliRegexpReader), &g_cli_text_cells);
    if (reader == NULL)
    {
        mmgr_line_put(args->err_msg, "the pattern could not be read: the cells cannot meet it");
        return 0;
    }
    mmgr_zero_buf(reader, sizeof(*reader));
    const int read = EMBED_CALL(cli_regexp_read_by, RegexpReadCtx, .reader = reader, .regexp = args->regexp_out,
                                .source = args->source, .flags = args->flags, .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(reader, &g_cli_text_cells);
    return read;
}
