// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_bridge.c: the bridge at the command line: its keys listed, each with how many pairs it has and how many
// of them are open and closed, those that contain a word where one is given; and one key shown whole, its pairs with
// their verdicts and notes, the name each .klm gives it, and each ruleset's entries of it, every line with the file and
// line it is read from
#include "../editor/bridge.h"
#include "../text/text.h"
#include "cells.h"
#include "command_line_internal.h"

// The command's work state, a cell taken as it starts and given back as it ends: the bridge's tree, in a cell of its
// own, and a line printed
typedef struct
{
    CliJson json;
    char line[1u << 16u];
} CliBridgeWorkState;

static CliBridgeWorkState *s_work_state;

typedef struct
{
    unsigned int node;
    const char *name;
    VerbaLinea *out;
    const char *path;
    const VerbaLinea *line;
    unsigned int key;
    unsigned int bridge;
    const char *root;
    const char *word_text;
} CommandLineBridgeCtx;
// a string member of a node, the empty text where it has none
EMBED_INLINE const char *bridge_text(const CommandLineBridgeCtx *args)
{
    unsigned int node = args->node;
    const char *name = args->name;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = node, .name = name),
                   .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

// `{path}:{line}` written, the line counted from 1, of a node's `line`, counted from 0
EMBED_INLINE void bridge_at(const CommandLineBridgeCtx *args)
{
    unsigned int node = args->node;
    const unsigned int line = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = node, .name = "line");
    unsigned long long number = 0ULL;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                   .node = line, .kind = CFG_JSON_NUMBER))
    {
        const CliJsonNode *const one = &s_work_state->json.node[line];
        for (unsigned long long at = 0ULL; at < one->length; at += 1ULL)
        {
            const char digit = s_work_state->json.text[one->text + at];
            if ((digit < '0') || (digit > '9'))
            {
                break;
            }
            number = number * 10ULL + (unsigned long long)(digit - '0');
        }
    }
    mmgr_line_put(args->out, args->path);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, number + 1ULL, 1u);
}

// the line written, its spaces at the end not
EMBED_INLINE void bridge_out(const CommandLineBridgeCtx *args)
{
    const unsigned long long size = mmgr_line_ok(args->line) ? args->line->at : 0ULL;
    EMBED_CALL(
        cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = args->line->out,
        .size = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = args->line->out, .size = size));
}

// how many pairs a key has, and how many of them are open and closed, the empty text for a key with none
EMBED_INLINE void bridge_pair_counts(const CommandLineBridgeCtx *args)
{
    const unsigned int pairs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_work_state->json, .object = args->key, .name = "pairs");
    const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = pairs, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[pairs].count
                                   : 0u;
    if (count == 0u)
    {
        return;
    }
    unsigned int open = 0u;
    unsigned int closed = 0u;
    for (unsigned int pair = s_work_state->json.node[pairs].first; pair != CLI_JSON_NONE;
         pair = s_work_state->json.node[pair].next)
    {
        const unsigned int verdict = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &s_work_state->json, .object = pair, .name = "verdict");
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                       .node = verdict, .kind = CFG_JSON_OBJECT))
        {
            open += (cellul.eq(&(CatenaFinitaCfg){
                        .src = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = verdict, .name = "word"),
                        .other = "open",
                        .cap = CLI_CELLS_POOL}))
                        ? 1u
                        : 0u;
            closed += (cellul.eq(&(CatenaFinitaCfg){
                          .src = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = verdict, .name = "word"),
                          .other = "closed",
                          .cap = CLI_CELLS_POOL}))
                          ? 1u
                          : 0u;
        }
    }
    mmgr_line_decimal(args->out, count, 1u);
    mmgr_line_put(args->out, " pairs, ");
    mmgr_line_decimal(args->out, open, 1u);
    mmgr_line_put(args->out, " open, ");
    mmgr_line_decimal(args->out, closed, 1u);
    mmgr_line_put(args->out, " closed");
}

// the key's pairs, each with its verdict and its notes
EMBED_INLINE void bridge_pairs(const CommandLineBridgeCtx *args)
{
    const char *name = args->name;
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "  ");
    EMBED_CALL(bridge_at, CommandLineBridgeCtx, .out = &line, .path = args->path, .node = args->key);
    EMBED_CALL(bridge_out, CommandLineBridgeCtx, .line = &line);
    const unsigned int pairs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_work_state->json, .object = args->key, .name = "pairs");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                    .node = pairs, .kind = CFG_JSON_ARRAY) ||
        (s_work_state->json.node[pairs].count == 0u))
    {
        EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "  no pairs");
        return;
    }
    for (unsigned int pair = s_work_state->json.node[pairs].first; pair != CLI_JSON_NONE;
         pair = s_work_state->json.node[pair].next)
    {
        line.at = 0ULL;
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = pair, .name = "first"));
        mmgr_line_put(&line, " ~ ");
        mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = pair, .name = "second"));
        mmgr_line_put(&line, "  ");
        const unsigned int verdict = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &s_work_state->json, .object = pair, .name = "verdict");
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                       .node = verdict, .kind = CFG_JSON_OBJECT))
        {
            static char s_verdict[1u << 12u];
            VerbaLinea verdict_line = {s_verdict, sizeof(s_verdict), 0ULL};
            mmgr_line_put(&verdict_line,
                          EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = verdict, .name = "word"));
            mmgr_line_ch(&verdict_line, ' ');
            mmgr_line_put(&verdict_line,
                          EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = verdict, .name = "rest"));
            unsigned long long from = 0ULL;
            unsigned long long to = 0ULL;
            EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = s_verdict,
                       .size = mmgr_line_ok(&verdict_line) ? verdict_line.at : 0ULL, .from_u64s = &from,
                       .to_u64s = &to);
            mmgr_line_put_n(&line, s_verdict + from, to - from);
        }
        mmgr_line_put(&line, "  ");
        EMBED_CALL(bridge_at, CommandLineBridgeCtx, .out = &line, .path = args->path, .node = pair);
        EMBED_CALL(bridge_out, CommandLineBridgeCtx, .line = &line);
        const unsigned int notes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &s_work_state->json, .object = pair, .name = "notes");
        const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = &s_work_state->json, .node = notes, .kind = CFG_JSON_ARRAY)
                                       ? s_work_state->json.node[notes].first
                                       : CLI_JSON_NONE;
        for (unsigned int note = first; note != CLI_JSON_NONE; note = s_work_state->json.node[note].next)
        {
            line.at = 0ULL;
            mmgr_line_put(&line, "    ");
            mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = note, .name = "word"));
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = note, .name = "rest"));
            EMBED_CALL(bridge_out, CommandLineBridgeCtx, .line = &line);
        }
    }
}

// the name each .klm gives the key, with its kind and what breaks it
EMBED_INLINE void bridge_maps(const CommandLineBridgeCtx *args)
{
    const char *name = args->name;
    const unsigned int maps = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = args->bridge, .name = "maps");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = maps, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[maps].first
                                   : CLI_JSON_NONE;
    int any = 0;
    for (unsigned int map = first; map != CLI_JSON_NONE; map = s_work_state->json.node[map].next)
    {
        const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &s_work_state->json, .object = map, .name = "names");
        const unsigned int named_first =
            EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                       .node = names, .kind = CFG_JSON_OBJECT)
                ? s_work_state->json.node[names].first
                : CLI_JSON_NONE;
        for (unsigned int entry = named_first; entry != CLI_JSON_NONE; entry = s_work_state->json.node[entry].next)
        {
            const char *const named = s_work_state->json.text + s_work_state->json.node[entry].key;
            const char *const key = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = entry, .name = "key");
            if ((!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = name, .cap = CLI_CELLS_POOL})) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = named, .other = name, .cap = CLI_CELLS_POOL})))
            {
                continue;
            }
            if (!any)
            {
                EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "maps");
                any = 1;
            }
            VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = map, .name = "language"));
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, named);
            if ((key[0] != '\0') && (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = named, .cap = CLI_CELLS_POOL})))
            {
                mmgr_line_put(&line, " -> ");
                mmgr_line_put(&line, key);
            }
            const unsigned int kind = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &s_work_state->json, .object = entry, .name = "kind");
            if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                           .node = kind, .kind = CFG_JSON_OBJECT))
            {
                mmgr_line_put(&line, "  ");
                mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = kind, .name = "rest"));
            }
            const unsigned int breaks = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = &s_work_state->json, .object = entry, .name = "breaks");
            const unsigned int broken_first =
                EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                           .node = breaks, .kind = CFG_JSON_ARRAY)
                    ? s_work_state->json.node[breaks].first
                    : CLI_JSON_NONE;
            for (unsigned int broken = broken_first; broken != CLI_JSON_NONE;
                 broken = s_work_state->json.node[broken].next)
            {
                mmgr_line_put(&line, "  breaks ");
                mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = broken, .name = "rest"));
            }
            mmgr_line_put(&line, "  ");
            EMBED_CALL(bridge_at, CommandLineBridgeCtx, .out = &line,
                       .path = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = map, .name = "path"),
                       .node = entry);
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        }
    }
}

// each ruleset's entries of the key's name, each with its kind
EMBED_INLINE void bridge_rulesets(const CommandLineBridgeCtx *args)
{
    const char *name = args->name;
    const unsigned int rulesets =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                   .object = args->bridge, .name = "rulesets");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = rulesets, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[rulesets].first
                                   : CLI_JSON_NONE;
    int any = 0;
    for (unsigned int ruleset = first; ruleset != CLI_JSON_NONE; ruleset = s_work_state->json.node[ruleset].next)
    {
        const char *const path = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = ruleset, .name = "path");
        const char *const slash =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '/');
        const char *const file = (slash != NULL) ? slash + 1 : path;
        const unsigned int entries =
            EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                       .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_work_state->json, .object = ruleset, .name = "entries"),
                       .name = name);
        const unsigned int entry_first =
            EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                       .node = entries, .kind = CFG_JSON_ARRAY)
                ? s_work_state->json.node[entries].first
                : CLI_JSON_NONE;
        for (unsigned int entry = entry_first; entry != CLI_JSON_NONE; entry = s_work_state->json.node[entry].next)
        {
            if (!any)
            {
                EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = "rulesets");
                any = 1;
            }
            VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, file);
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = entry, .name = "kind"));
            mmgr_line_put(&line, "  ");
            EMBED_CALL(bridge_at, CommandLineBridgeCtx, .out = &line, .path = path, .node = entry);
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        }
    }
}

EMBED_INLINE int bridge_words(const CommandLineBridgeCtx *args)
{
    const char *root = args->root;
    const char *word_text = args->word_text;
    const unsigned int bridge = EMBED_CALL(cli_bridge.read, CliBridgeCfg, .json = &s_work_state->json, .root = root);
    const unsigned int klq = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                        .json_read = &s_work_state->json, .object = bridge, .name = "klq");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                    .node = klq, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "no Lstar.klq in " CLI_BRIDGE_DIR);
        return CLI_NO_CODE;
    }
    const unsigned int keys = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = klq, .name = "keys");
    if (word_text != NULL)
    {
        const unsigned int key = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_work_state->json, .object = keys, .name = word_text);
        if (key != CLI_JSON_NONE)
        {
            EMBED_CALL(bridge_pairs, CommandLineBridgeCtx,
                       .path = EMBED_CALL(bridge_text, CommandLineBridgeCtx, .node = klq, .name = "path"),
                       .name = word_text, .key = key);
            EMBED_CALL(bridge_maps, CommandLineBridgeCtx, .bridge = bridge, .name = word_text);
            EMBED_CALL(bridge_rulesets, CommandLineBridgeCtx, .bridge = bridge, .name = word_text);
            return 0;
        }
    }
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = keys, .kind = CFG_JSON_OBJECT)
                                   ? s_work_state->json.node[keys].first
                                   : CLI_JSON_NONE;
    unsigned long long pad = 0ULL;
    unsigned int count = 0u;
    for (unsigned int key = first; key != CLI_JSON_NONE; key = s_work_state->json.node[key].next)
    {
        const char *const name = s_work_state->json.text + s_work_state->json.node[key].key;
        if ((word_text == NULL) || EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx, .text = name,
                                              .word_text = word_text))
        {
            const unsigned long long letters =
                EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = name,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            pad = (letters > pad) ? letters : pad;
            count += 1u;
        }
    }
    if (count == 0u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "no key contains that word");
        return CLI_NO_CODE;
    }
    for (unsigned int key = first; key != CLI_JSON_NONE; key = s_work_state->json.node[key].next)
    {
        const char *const name = s_work_state->json.text + s_work_state->json.node[key].key;
        if ((word_text != NULL) && !EMBED_CALL(cli_command_line_internal.contains_word, CliCommandLineCtx, .text = name,
                                               .word_text = word_text))
        {
            continue;
        }
        VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
        mmgr_line_put(&line, name);
        for (unsigned long long letters =
                 EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = name,
                            .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
             letters < pad; letters += 1ULL)
        {
            mmgr_line_ch(&line, ' ');
        }
        mmgr_line_put(&line, "  ");
        EMBED_CALL(bridge_pair_counts, CommandLineBridgeCtx, .key = key, .out = &line);
        EMBED_CALL(bridge_out, CommandLineBridgeCtx, .line = &line);
    }
    return 0;
}

int cli_bridge_words(const CliCommandLineWordCtx *args)
{
    s_work_state = g_cli_cells->persistent_buf_alloc(sizeof(*s_work_state), &g_cli_command_line_cells);
    if (s_work_state == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "a showing of the bridge could not be taken");
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(bridge_words, CommandLineBridgeCtx, .root = args->root, .word_text = args->word_text);
    g_cli_cells->persistent_buf_release(
        s_work_state->json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(s_work_state, &g_cli_command_line_cells);
    s_work_state = NULL;
    return code;
}
