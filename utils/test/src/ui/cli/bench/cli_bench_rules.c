// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench_rules.c: the C grammar's root rules, each read as a pattern of its own and tried at every letter of every
// loaded file as the colorer tries it, the letters moved past a match; a row a rule, named by the pattern's start
#include "../../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../../ui/cli/src/text/json_tree.h"
#include "../../../../../../ui/cli/src/text/regexp.h"
#include "bench.h"

#include <stdio.h>

#define BENCH_RULE_NAME 28u

static CliRegexpWorkState s_work_state;

// the pattern tried over `count` loaded files, the letters decoded into `letters` each time: the measure
static BenchMeasure rules_tried(const CliRegexp *regexp, const BenchFile *files, unsigned int count,
                                unsigned int repeat, unsigned int *letters, unsigned long long *matched)
{
    BenchMeasure measure = {0ull, ~0ull};
    CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
    for (unsigned int round = 0u; round < repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        unsigned long long hits = 0ull;
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int file = 0u; file < count; file += 1u)
        {
            if (files[file].bytes == NULL)
            {
                continue;
            }
            const unsigned long long length =
                EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = (const char *)files[file].bytes, .size = files[file].size, .letters = letters, .capacity_u64 = files[file].size);
            for (unsigned long long at = 0ull; at < length;)
            {
                const int found = EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = regexp, .text_u32s = letters, .length = length, .at = at, .groups = groups);
                const unsigned long long span = (found && groups[0].set) ? (groups[0].to - groups[0].from) : 0ull;
                hits += found ? 1ull : 0ull;
                at += (span > 0ull) ? span : 1ull;
            }
            bytes += files[file].size;
        }
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = bytes;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
        *matched = hits;
    }
    return measure;
}

// The grammars cut down from the C grammar's: each a name and its root's rules as JSON, every one with the C grammar's
// word lists and whitespace state
typedef struct
{
    const char *name;
    const char *rules;
} BenchCutDown;

static const BenchCutDown s_cut_down[5] = {
    {"color:none", "[]"},
    {"color:whitespace", "[{\"include\":\"@whitespace\"}]"},
    {"color:identifier-plain", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",\"identifier\"]]"},
    {"color:identifier-cases", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\","
                               "\"@words\":\"keyword\",\"@default\":\"identifier\"}}]]"},
    {"color:identifier-number-string",
     "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\",\"@words\":\"keyword\","
     "\"@default\":\"identifier\"}}],[\"(0[xX][\\\\da-fA-F_']+|0[bB][01_']+|(?:\\\\d[\\\\d_']*)?\\\\.?\\\\d[\\\\d_']*"
     "(?:[eE][+-]?\\\\d+)?)([fFuUlLjJzZ]*)\",[\"number\",\"number.suffix\"]],[\"[uUL]?\\\"\",\"string\",\"@string\"]]"},
};

// the corpus colored with each cut-down grammar, cached under bench-cut from JSON written here: the C grammar's words
// and types, the root given, and the C grammar's whitespace and string states
static void rules_cut_down(const CliJson *json, unsigned int grammar, const BenchFile *files, unsigned int count,
                           unsigned int repeat, unsigned long long largest)
{
    const unsigned int tokenizer = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = grammar, .name = "tokenizer");
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const unsigned long long created_capacity = 1ull << 16u;
        char *const created_cell = g_cli_cells->temporary_buf_alloc(created_capacity, &g_checks_cells);
        VerbaLinea made = mmgr_line_over(created_cell, created_capacity);
        if (made.out == NULL)
        {
            return;
        }
        mmgr_line_put(&made, "{\"words\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = grammar, .name = "words"), .out = &made, .indent = 0u, .sorted = 0);
        mmgr_line_put(&made, ",\"types\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = grammar, .name = "types"), .out = &made, .indent = 0u, .sorted = 0);
        mmgr_line_put(&made, ",\"tokenizer\":{\"root\":");
        mmgr_line_put(&made, s_cut_down[at].rules);
        mmgr_line_put(&made, ",\"whitespace\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tokenizer, .name = "whitespace"), .out = &made, .indent = 0u, .sorted = 0);
        mmgr_line_put(&made, ",\"comment\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tokenizer, .name = "comment"), .out = &made, .indent = 0u, .sorted = 0);
        mmgr_line_put(&made, ",\"string\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tokenizer, .name = "string"), .out = &made, .indent = 0u, .sorted = 0);
        mmgr_line_put(&made, "}}");
        CliJson cut;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        const unsigned int top = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &cut, .text = made.out, .size = made.at, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
        if ((top != CLI_JSON_NONE) && EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = "bench-cut", .json = &cut, .def = top, .err_msg = &err_msg))
        {
            bench_row(s_cut_down[at].name, bench_colored("bench-cut", files, count, repeat, largest));
        }
        else
        {
            printf("%s\tnot cached: %.*s\n", s_cut_down[at].name, (int)err_msg.at, why);
        }
        g_cli_cells->persistent_buf_release(cut.node,
                                            EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = cut.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&cut),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(made.out, &g_checks_cells);
    }
}

void bench_rules(const char *root, const BenchFile *files, unsigned int count, unsigned int repeat,
                 unsigned long long largest)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "ui/plugins/c/plugin.json", .joined = path);
    unsigned long long size = 0ull;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size, .owner = &g_checks_cells);
    if (text == NULL)
    {
        return;
    }
    CliJson json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int top = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = (const char *)text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    const unsigned int grammar = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = top, .name = "grammar");
    const unsigned int rules = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = grammar, .name = "tokenizer"), .name = "root");
    const int fold = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = grammar, .name = "ignoreCase"), .kind = CFG_JSON_TRUE);
    unsigned int *const letters =
        g_cli_cells->persistent_buf_alloc((largest + 1ull) * sizeof(unsigned int), &g_checks_cells);
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json, .array = rules, .index = index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int rule = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json, .array = rules, .index = index);
        unsigned long long length = 0ull;
        const char *const source = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json, .array = rule, .index = 0u), .length_pointer = &length);
        if (source == NULL)
        {
            continue;
        }
        // the pattern as a terminated text of its own, the tree's being bounded by its length alone
        char pattern[1024];
        VerbaLinea pattern_line = {pattern, sizeof(pattern), 0ull};
        mmgr_line_put_n(&pattern_line, source, length);
        if (mmgr_line_finish(&pattern_line) == 0ull)
        {
            continue;
        }
        CliRegexp regexp;
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &regexp, .work_state = &s_work_state);
        err_msg.at = 0ull;
        if (!EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp, .source = pattern, .flags = fold ? "i" : "", .err_msg = &err_msg))
        {
            continue;
        }
        unsigned long long matched = 0ull;
        const BenchMeasure measure = rules_tried(&regexp, files, count, repeat, letters, &matched);
        char name[BENCH_RULE_NAME + 24u];
        VerbaLinea name_line = {name, sizeof(name), 0ull};
        mmgr_line_put(&name_line, "rule:");
        mmgr_line_put_most(&name_line, pattern, BENCH_RULE_NAME);
        mmgr_line_put(&name_line, " hits=");
        mmgr_line_decimal(&name_line, matched, 1u);
        (void)mmgr_line_finish(&name_line);
        bench_row(name, measure);
    }
    g_cli_cells->persistent_buf_release(letters, &g_checks_cells);
    rules_cut_down(&json, grammar, files, count, repeat, largest);
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)text, (&g_checks_cells));
}
