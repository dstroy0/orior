// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.c: Lstar.klq, the .klm maps and the rulesets read line by line into the bridge
#include "bridge.h"
#include "../command_line/callback_cfg.h"

#include "../command_line/cells.h"
#include "../text/text.h"

// One line that contains something: its number from 0, its first word and the rest, trimmed
typedef struct
{
    unsigned int line;
    const char *word_text;
    unsigned long long word_length;
    const char *rest;
    unsigned long long remainder_length;
} CliBridgeLine;
typedef struct
{
    CliJson *json;
    unsigned int line;
} CliJsonLineCfg;
// the most nodes and bytes of texts the bridge's tree makes of a line of one of its files, and of a file besides its
// lines: a pair under a key not seen yet is the most a line makes, and a verdict the most text, its line's bytes twice
#define CLI_BRIDGE_LINE_NODES 16u
#define CLI_BRIDGE_LINE_TEXT 128u
#define CLI_BRIDGE_FILE_NODES 8u
#define CLI_BRIDGE_FILE_TEXT 64u

// One file of the bridge's folder: its name, and its bytes mapped
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    const unsigned char *bytes;
    unsigned long long size;
} CliBridgeFile;

// The bridge folder's files listed: those retained, counted where `file` is NULL, and the most retained
typedef struct
{
    CliBridgeFile *file;
    unsigned int count;
    unsigned int most;
} CliBridgeFiles;

typedef struct
{
    char letter;
    const char **at;
    const char *end;
    unsigned int *number;
    CliBridgeLine *parsed;
    const CliBridgeLine *parsed_read;
    const char *word_text;
    const char *text;
    unsigned long long length;
    unsigned int index;
    const char **word_pointer;
    CliJson *json;
    unsigned long long value;
    unsigned long long word_length;
    const char *rest;
    unsigned long long remainder_length;
    unsigned int line;
    unsigned int object;
    const char *name;
    unsigned int (*made)(const CliJsonLineCfg *args);
} BridgeCtx;
EMBED_INLINE int cli_is_space(const BridgeCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// the next line from `*at` before `end` that contains something, `*number` counting lines as they pass: 1, or 0 at the
// end
static int cli_next_line(const BridgeCtx *args)
{
    const char **at = args->at;
    unsigned int *number = args->number;
    CliBridgeLine *parsed = args->parsed;
    while (*at < args->end)
    {
        const char *from = *at;
        const char *stop = from;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(args->end - stop), .set = "\n");
        *at = (stop < args->end) ? (stop + 1) : stop;
        const unsigned int line = *number;
        *number += 1u;
        while ((from < stop) && EMBED_CALL(cli_is_space, BridgeCtx, .letter = from[0]))
        {
            from += 1;
        }
        while ((stop > from) && EMBED_CALL(cli_is_space, BridgeCtx, .letter = stop[-1]))
        {
            stop -= 1;
        }
        if ((from == stop) || (from[0] == '#'))
        {
            continue;
        }
        const char *word_past = from;
        while ((word_past < stop) && !EMBED_CALL(cli_is_space, BridgeCtx, .letter = word_past[0]))
        {
            word_past += 1;
        }
        const char *rest = word_past;
        while ((rest < stop) && EMBED_CALL(cli_is_space, BridgeCtx, .letter = rest[0]))
        {
            rest += 1;
        }
        const CliBridgeLine found = {line, from, (unsigned long long)(word_past - from), rest,
                                     (unsigned long long)(stop - rest)};
        *parsed = found;
        return 1;
    }
    return 0;
}

EMBED_INLINE int cli_word_is(const BridgeCtx *args)
{
    return (args->parsed_read->word_length ==
            cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = args->parsed_read->word_text,
                                    .other = args->word_text,
                                    .bytes = args->parsed_read->word_length}) == 0);
}

// the `index`th word of a text, its start into `word`: its length, 0 where it has none
EMBED_INLINE unsigned long long cli_nth(const BridgeCtx *args)
{
    const char **word_text = args->word_pointer;
    const char *at = args->text;
    const char *const end = args->text + args->length;
    unsigned int each = 0u;
    while (at < end)
    {
        while ((at < end) && EMBED_CALL(cli_is_space, BridgeCtx, .letter = at[0]))
        {
            at += 1;
        }
        const char *stop = at;
        while ((stop < end) && !EMBED_CALL(cli_is_space, BridgeCtx, .letter = stop[0]))
        {
            stop += 1;
        }
        if (each == args->index)
        {
            *word_text = at;
            return (unsigned long long)(stop - at);
        }
        at = stop;
        each += 1u;
    }
    *word_text = end;
    return 0ULL;
}

EMBED_INLINE unsigned int cli_number(const BridgeCtx *args)
{
    CliJson *json = args->json;
    char digits[24];
    VerbaLinea line = {digits, sizeof(digits), 0ULL};
    mmgr_line_decimal(&line, args->value, 1u);
    mmgr_line_finish(&line);
    return EMBED_CALL(cli_json_tree_text_length_kind.json_number_init, CliJsonTreeTextLengthKindCfg, .json = json,
                      .text = digits);
}

// A note: a line's first word, the rest, and its number
EMBED_INLINE unsigned int cli_note(const BridgeCtx *args)
{
    CliJson *json = args->json;
    const unsigned int note =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = note,
               .name = "word",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = args->word_text, .length = args->word_length));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = note,
               .name = "rest",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = args->rest, .length = args->remainder_length));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = note,
               .name = "line", .value = EMBED_CALL(cli_number, BridgeCtx, .json = json, .value = args->line));
    return note;
}

// the member of `object` named by `length` bytes of `name`, made by `made` where it has none
EMBED_INLINE unsigned int cli_entry(const BridgeCtx *args)
{
    CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    char key[CLI_PATH_MAX_LEN];
    VerbaLinea named = {key, sizeof(key), 0ULL};
    mmgr_line_put_n(&named, name, args->length);
    mmgr_line_finish(&named);
    const unsigned int had = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = object, .name = key);
    if (had != CLI_JSON_NONE)
    {
        return had;
    }
    const unsigned int entry = EMBED_CALL(args->made, CliJsonLineCfg, .json = json, .line = args->line);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = object,
               .name = key, .value = entry);
    return entry;
}

static unsigned int cli_key_init(const CliJsonLineCfg *args)
{
    CliJson *json = args->json;
    const unsigned int key =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = key,
               .name = "line", .value = EMBED_CALL(cli_number, BridgeCtx, .json = json, .value = args->line));
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = key, .name = "pairs",
        .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY));
    return key;
}

// a key with no pairs, line 0, where a pair names one not seen yet
static unsigned int cli_key_default(const CliJsonLineCfg *args)
{
    return EMBED_CALL(cli_key_init, CliJsonLineCfg, .json = args->json, .line = 0u);
}

// the verdict `word_text` and its `rest` put after the rest of the identity `object`, two spaces between; a tree whose
// text has too few bytes left marked full
EMBED_INLINE void cli_verdict_join(const BridgeCtx *args)
{
    CliJson *const json = args->json;
    unsigned long long length = 0ULL;
    const unsigned int rest = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->object, .name = "rest");
    const char *const had = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                       .json_read = json, .node = rest, .length_pointer = &length);
    const unsigned long long start = json->used;
    if ((json->used + length + args->word_length + args->remainder_length + 4u) > json->text_max)
    {
        json->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = had, .bytes = length});
    json->used += length;
    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = "  ", .bytes = 2u});
    json->used += 2u;
    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = args->word_text, .bytes = args->word_length});
    json->used += args->word_length;
    json->text[json->used] = ' ';
    json->used += 1u;
    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = args->rest, .bytes = args->remainder_length});
    json->used += args->remainder_length;
    const unsigned int joined =
        EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                   .text = json->text + start, .length = json->used - start);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->object,
               .name = "rest", .value = joined);
}

unsigned int cli_bridge_klq(const CliBridgePathCfg *args)
{
    const unsigned int keys =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    const unsigned int identities =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_ARRAY);
    // the key the lines stand under, as its name in the text
    const char *key = NULL;
    unsigned long long key_length = 0ULL;
    // where the lines under a pair go: the pair's own place in its key, or an identity block
    int under_pair = 0;
    int under_identity = 0;
    const char *at = args->text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (EMBED_CALL(cli_next_line, BridgeCtx, .at = &at, .end = args->text + args->size, .number = &number,
                      .parsed = &parsed))
    {
        if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "klq"))
        {
            continue;
        }
        if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "key"))
        {
            const char *name = NULL;
            const unsigned long long length =
                EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = 0u,
                           .word_pointer = &name);
            const unsigned int entry =
                EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = keys, .name = name, .length = length,
                           .made = cli_key_init, .line = parsed.line);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = entry, .name = "line",
                       .value = EMBED_CALL(cli_number, BridgeCtx, .json = args->json, .value = parsed.line));
            key = name;
            key_length = length;
            under_pair = 0;
            under_identity = 0;
            continue;
        }
        if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "pair"))
        {
            const char *first = NULL;
            const char *second = NULL;
            const unsigned long long first_length =
                EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = 0u,
                           .word_pointer = &first);
            const unsigned long long second_length =
                EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = 1u,
                           .word_pointer = &second);
            const unsigned int owner =
                (key != NULL) ? EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = keys, .name = key,
                                           .length = key_length, .made = cli_key_default, .line = 0u)
                              : EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = keys, .name = first,
                                           .length = first_length, .made = cli_key_default, .line = 0u);
            const unsigned int pair = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                                 .kind = CFG_JSON_OBJECT);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "first",
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                      .json = args->json, .text = first, .length = first_length));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "second",
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                      .json = args->json, .text = second, .length = second_length));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "line",
                       .value = EMBED_CALL(cli_number, BridgeCtx, .json = args->json, .value = parsed.line));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "verdict",
                       .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                           .kind = CFG_JSON_NULL));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "notes",
                       .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                           .kind = CFG_JSON_ARRAY));
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json,
                       .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = owner, .name = "pairs"),
                       .value = pair);
            under_pair = 1;
            under_identity = 0;
            continue;
        }
        if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "text_identity"))
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = identities,
                       .value = EMBED_CALL(cli_note, BridgeCtx, .json = args->json, .word_text = parsed.word_text,
                                           .word_length = parsed.word_length, .rest = parsed.rest,
                                           .remainder_length = parsed.remainder_length, .line = parsed.line));
            under_pair = 0;
            under_identity = 1;
            continue;
        }
        const int verdict = EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "open") ||
                            EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "closed");
        if (under_identity && verdict)
        {
            // the verdict after an identity put after its text, two spaces between
            const unsigned int last = args->json->node[identities].last;
            if (last != CLI_JSON_NONE)
            {
                EMBED_CALL(cli_verdict_join, BridgeCtx, .json = args->json, .object = last,
                           .word_text = parsed.word_text, .word_length = parsed.word_length, .rest = parsed.rest,
                           .remainder_length = parsed.remainder_length);
            }
            under_identity = 0;
            continue;
        }
        if (!under_pair || (key == NULL))
        {
            continue;
        }
        char name[CLI_PATH_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ULL};
        mmgr_line_put_n(&named, key, key_length);
        mmgr_line_finish(&named);
        const unsigned int pairs =
            EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                       .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = keys, .name = name),
                       .name = "pairs");
        const unsigned int pair = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                             .json_read = args->json, .node = pairs, .kind = CFG_JSON_ARRAY)
                                      ? args->json->node[pairs].last
                                      : CLI_JSON_NONE;
        if (pair == CLI_JSON_NONE)
        {
            continue;
        }
        const unsigned int note = EMBED_CALL(cli_note, BridgeCtx, .json = args->json, .word_text = parsed.word_text,
                                             .word_length = parsed.word_length, .rest = parsed.rest,
                                             .remainder_length = parsed.remainder_length, .line = parsed.line);
        if (verdict && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = args->json, .object = pair, .name = "verdict"),
                                  .kind = CFG_JSON_NULL))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = pair,
                       .name = "verdict", .value = note);
        }
        else
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json,
                       .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = pair, .name = "notes"),
                       .value = note);
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = args->json, .object = keys);
    const unsigned int klq =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klq,
               .name = "path",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->json, .text = args->path,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klq,
               .name = "keys", .value = keys);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klq,
               .name = "identities", .value = identities);
    return klq;
}

static unsigned int cli_name_init(const CliJsonLineCfg *args)
{
    CliJson *json = args->json;
    const unsigned int name =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = name,
               .name = "key",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = "", .length = 0u));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = name,
               .name = "line", .value = EMBED_CALL(cli_number, BridgeCtx, .json = json, .value = args->line));
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = name, .name = "kind",
        .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_NULL));
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = name, .name = "breaks",
        .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY));
    return name;
}

unsigned int cli_bridge_klm(const CliBridgePathCfg *args)
{
    const unsigned int names =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    unsigned int language = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                       .json = args->json, .text = "", .length = 0u);
    const char *at = args->text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (EMBED_CALL(cli_next_line, BridgeCtx, .at = &at, .end = args->text + args->size, .number = &number,
                      .parsed = &parsed))
    {
        const char *name = NULL;
        const unsigned long long name_length =
            EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = 0u,
                       .word_pointer = &name);
        // the words after the name, one space between each
        const unsigned long long start = args->json->used;
        const char *word_text = NULL;
        for (unsigned int index = 1u;
             EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = index,
                        .word_pointer = &word_text) != 0ULL;
             index += 1u)
        {
            const unsigned long long length =
                EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = index,
                           .word_pointer = &word_text);
            if ((args->json->used + length + 2u) > args->json->text_max)
            {
                args->json->full = 1;
                break;
            }
            if (index != 1u)
            {
                args->json->text[args->json->used] = ' ';
                args->json->used += 1u;
            }
            memor.cpy(&(MemoriaCfg){.dst = args->json->text + args->json->used, .src = word_text, .bytes = length});
            args->json->used += length;
        }
        const unsigned int value =
            EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                       .json = args->json, .text = args->json->text + start, .length = args->json->used - start);
        if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "klm"))
        {
            language = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                  .json = args->json, .text = name, .length = name_length);
        }
        else if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "key"))
        {
            const unsigned int entry =
                EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = names, .name = name,
                           .length = name_length, .made = cli_name_init, .line = parsed.line);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = entry, .name = "key", .value = value);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = entry, .name = "line",
                       .value = EMBED_CALL(cli_number, BridgeCtx, .json = args->json, .value = parsed.line));
        }
        else if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "kind") ||
                 EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "breaks"))
        {
            const unsigned int entry =
                EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = names, .name = name,
                           .length = name_length, .made = cli_name_init, .line = parsed.line);
            unsigned long long length = 0ULL;
            const char *const joined = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                  .json_read = args->json, .node = value, .length_pointer = &length);
            const unsigned int note = EMBED_CALL(cli_note, BridgeCtx, .json = args->json, .word_text = parsed.word_text,
                                                 .word_length = parsed.word_length, .rest = joined,
                                                 .remainder_length = length, .line = parsed.line);
            if (EMBED_CALL(cli_word_is, BridgeCtx, .parsed_read = &parsed, .word_text = "kind"))
            {
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                           .object = entry, .name = "kind", .value = note);
            }
            else
            {
                EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json,
                           .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = args->json, .object = entry, .name = "breaks"),
                           .value = note);
            }
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = args->json, .object = names);
    const unsigned int klm =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klm,
               .name = "path",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->json, .text = args->path,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klm,
               .name = "language", .value = language);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = klm,
               .name = "names", .value = names);
    return klm;
}

// whether a word is a kind in lower case, and whether a word is a name
EMBED_INLINE int cli_is_kind(const BridgeCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        if (!(((args->word_text[at] >= 'a') && (args->word_text[at] <= 'z')) || (args->word_text[at] == '_')))
        {
            return 0;
        }
    }
    return args->length != 0ULL;
}

EMBED_INLINE int cli_is_name(const BridgeCtx *args)
{
    if ((args->length == 0ULL) ||
        !(((args->word_text[0] | 0x20) >= 'a') && ((args->word_text[0] | 0x20) <= 'z')) && (args->word_text[0] != '_'))
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const char letter = args->word_text[at];
        if (!(((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
              ((letter >= '0') && (letter <= '9')) || (letter == '_') || (letter == '.')))
        {
            return 0;
        }
    }
    return 1;
}

static unsigned int cli_list_init(const CliJsonLineCfg *args)
{
    CliJson *json = args->json;
    return EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
}

// A ruleset's entries by name: every line whose first word is a kind in lower case and whose second is a name
unsigned int cli_bridge_ruleset(const CliBridgePathCfg *args)
{
    const unsigned int entries =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    const char *at = args->text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (EMBED_CALL(cli_next_line, BridgeCtx, .at = &at, .end = args->text + args->size, .number = &number,
                      .parsed = &parsed))
    {
        const char *name = NULL;
        const unsigned long long name_length =
            EMBED_CALL(cli_nth, BridgeCtx, .text = parsed.rest, .length = parsed.remainder_length, .index = 0u,
                       .word_pointer = &name);
        if (EMBED_CALL(cli_is_kind, BridgeCtx, .word_text = parsed.word_text, .length = parsed.word_length) &&
            EMBED_CALL(cli_is_name, BridgeCtx, .word_text = name, .length = name_length))
        {
            const unsigned int list =
                EMBED_CALL(cli_entry, BridgeCtx, .json = args->json, .object = entries, .name = name,
                           .length = name_length, .made = cli_list_init, .line = parsed.line);
            const unsigned int entry = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                                  .kind = CFG_JSON_OBJECT);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = entry, .name = "kind",
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                      .json = args->json, .text = parsed.word_text, .length = parsed.word_length));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = entry, .name = "line",
                       .value = EMBED_CALL(cli_number, BridgeCtx, .json = args->json, .value = parsed.line));
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = list,
                       .value = entry);
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = args->json, .object = entries);
    const unsigned int ruleset =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = ruleset,
               .name = "path",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->json, .text = args->path,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = ruleset,
               .name = "entries", .value = entries);
    return ruleset;
}

// a file of the bridge's folder counted, or retained where the list has capacity for it
static void cli_file_each(const CliEachNameIsFolderCfg *args)
{
    CliBridgeFiles *const files = (CliBridgeFiles *)args->context;
    if (args->is_folder ||
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    if ((files->file != NULL) && (files->count < files->most))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = files->file[files->count].name,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    files->count += ((files->file == NULL) || (files->count < files->most)) ? 1u : 0u;
}

// whether a file's name ends in an extension the bridge reads
EMBED_INLINE int cli_bridge_by_name(const BridgeCtx *args)
{
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->name,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}), .byte = '.');
    const char *const ext = (dot != NULL) ? (dot + 1) : "";
    return (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klq", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klm", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "krs", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "kdm", .cap = CLI_CELLS_POOL}));
}

unsigned int cli_bridge_read(const CliBridgeCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put(&line, args->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, CLI_BRIDGE_DIR);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    CliBridgeFiles files = {NULL, 0u, 0u};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_file_each,
               .context_bytess = &files);
    files = (CliBridgeFiles){
        g_cli_cells->persistent_buf_alloc((files.count + 1ULL) * sizeof(CliBridgeFile), &g_cli_editor_cells), 0u,
        files.count};
    if (files.file == NULL)
    {
        return CLI_JSON_NONE;
    }
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_file_each,
               .context_bytess = &files);
    for (unsigned int at = 1u; at < files.count; at += 1u)
    {
        const CliBridgeFile swap = files.file[at];
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                           .one = files.file[place - 1u].name, .other_pointer = swap.name) > 0))
        {
            files.file[place] = files.file[place - 1u];
            place -= 1u;
        }
        files.file[place] = swap;
    }
    // each file read mapped, and the tree's cell taken once for the most its files' lines make
    unsigned long long nodes = CLI_BRIDGE_FILE_NODES;
    unsigned long long text = CLI_BRIDGE_FILE_TEXT;
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        CliBridgeFile *const one = &files.file[at];
        VerbaLinea file = {path, sizeof(path), 0ULL};
        mmgr_line_put(&file, folder);
        mmgr_line_ch(&file, '/');
        mmgr_line_put(&file, one->name);
        one->bytes = (EMBED_CALL(cli_bridge_by_name, BridgeCtx, .name = one->name) && (mmgr_line_finish(&file) != 0ULL))
                         ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                      .size_pointer = &one->size, .owner = &g_cli_editor_cells)
                         : NULL;
        if (one->bytes != NULL)
        {
            const unsigned long long lines =
                1ULL + EMBED_CALL(cli_text_size_byte.count, CliTextSizeByteCfg, .text = (const char *)one->bytes,
                                  .size = one->size, .byte = '\n');
            nodes += (CLI_BRIDGE_LINE_NODES * lines) + CLI_BRIDGE_FILE_NODES;
            text += (2ULL * one->size) + (CLI_BRIDGE_LINE_TEXT * lines) + CLI_PATH_MAX_LEN;
        }
    }
    // a tree's node count is an unsigned int's, and a bridge past it reads as full
    const int swap =
        (nodes < CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg,
                                              .json = args->json, .nodes_max = (unsigned int)nodes, .text_max = text,
                                              .what = "the bridge read", .owner = &g_cli_editor_cells);
    const unsigned int bridge =
        swap ? EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_OBJECT)
             : CLI_JSON_NONE;
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = bridge,
               .name = "dir",
               .value =
                   EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                              .json = args->json, .text = CLI_BRIDGE_DIR,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = CLI_BRIDGE_DIR, .cap = CLI_CELLS_POOL})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = bridge,
               .name = "klq",
               .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                   .kind = CFG_JSON_NULL));
    const unsigned int maps =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_ARRAY);
    const unsigned int rulesets =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        const CliBridgeFile *const one = &files.file[at];
        if (one->bytes == NULL)
        {
            continue;
        }
        const char *const ext =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = one->name,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}),
                       .byte = '.') +
            1;
        const char *const text_of = (const char *)one->bytes;
        VerbaLinea file = {path, sizeof(path), 0ULL};
        mmgr_line_put(&file, folder);
        mmgr_line_ch(&file, '/');
        mmgr_line_put(&file, one->name);
        mmgr_line_finish(&file);
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = path,
                   .relative_out = relative);
        if (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klq", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = bridge, .name = "klq",
                       .value = EMBED_CALL(cli_bridge_path.klq, CliBridgePathCfg, .json = args->json, .path = relative,
                                           .text = text_of, .size = one->size));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klm", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = maps,
                       .value = EMBED_CALL(cli_bridge_path.klm, CliBridgePathCfg, .json = args->json, .path = relative,
                                           .text = text_of, .size = one->size));
        }
        else
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = rulesets,
                       .value = EMBED_CALL(cli_bridge_path.ruleset, CliBridgePathCfg, .json = args->json,
                                           .path = relative, .text = text_of, .size = one->size));
        }
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)one->bytes, (&g_cli_editor_cells));
    }
    g_cli_cells->persistent_buf_release(files.file, &g_cli_editor_cells);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = bridge,
               .name = "maps", .value = maps);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = bridge,
               .name = "rulesets", .value = rulesets);
    return args->json->full ? CLI_JSON_NONE : bridge;
}
