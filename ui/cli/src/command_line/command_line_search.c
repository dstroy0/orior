// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_search.c: `orior_cli edit search`, every line in the tree's files that contains a text, written as grep
// writes it
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "command_line_internal.h"

// the bytes a search's hits are written in, and a hit's line
#define CLI_SEARCH_HITS_CAPACITY (32ULL << 20u)
#define CLI_SEARCH_LINE_MORE 64ULL

typedef struct
{
    const CliJson *hits;
    unsigned int hit;
} CommandLineSearchCtx;
// one hit of the tree `hits` written to the output as path:line:column: line
EMBED_INLINE void search_hit_write(const CommandLineSearchCtx *args)
{
    unsigned long long path_length = 0ULL;
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->hits,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->hits,
                                      .object = args->hit, .name = "path"),
                   .length_pointer = &path_length);
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->hits,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->hits,
                                      .object = args->hit, .name = "text"),
                   .length_pointer = &length);
    const unsigned long long line_capacity = path_length + length + CLI_SEARCH_LINE_MORE;
    char *const line_cell = g_cli_cells->temporary_buf_alloc(line_capacity, &g_cli_command_line_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_capacity);
    mmgr_line_put(&line, (path != NULL) ? path : "");
    static const char *const s_numbers[2] = {"line", "col"};
    for (unsigned int number = 0u; number < 2u; number += 1u)
    {
        const unsigned int node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = args->hits, .object = args->hit, .name = s_numbers[number]);
        mmgr_line_ch(&line, ':');
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->hits, .node = node,
                       .kind = CFG_JSON_NUMBER))
        {
            mmgr_line_put_n(&line, args->hits->text + args->hits->node[node].text, args->hits->node[node].length);
        }
    }
    mmgr_line_put(&line, ": ");
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    const char *const found = (text != NULL) ? text : "";
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = found, .size = length, .from_u64s = &from,
               .to_u64s = &to);
    mmgr_line_put_n(&line, found + from, to - from);
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = line.out,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    g_cli_cells->persistent_buf_release(line.out, &g_cli_command_line_cells);
}

int cli_console_search(const CliCommandLineWordsCountCtx *args)
{
    const char *named = args->named;
    CliSearching how = {0, 0, 0};
    static char s_query[1u << 12u];
    VerbaLinea query = {s_query, sizeof(s_query), 0ULL};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        how.match_case =
            how.match_case ||
            cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--case", .cap = CLI_CELLS_POOL});
        how.word_text = how.word_text ||
                        cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--word", .cap = CLI_CELLS_POOL});
        how.regex = how.regex ||
                    cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--regex", .cap = CLI_CELLS_POOL});
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--case", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--word", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--regex", .cap = CLI_CELLS_POOL})))
        {
            mmgr_line_put(&query, (query.at > 0ULL) ? " " : "");
            mmgr_line_put(&query, args->words[at]);
        }
    }
    if (mmgr_line_finish(&query) == 0ULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "search needs text: orior_cli edit search <text>");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    char why[1u << 14u];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    static CliPatterns s_none;
    char *const found_cell = g_cli_cells->temporary_buf_alloc(CLI_SEARCH_HITS_CAPACITY, &g_cli_command_line_cells);
    VerbaLinea found = mmgr_line_over(found_cell, CLI_SEARCH_HITS_CAPACITY);
    if (found.out == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "the hits of a search could not be given a cell");
        return CLI_WRONG;
    }
    if (!EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root, .query = s_query,
                    .how = how, .set = &s_none, .out = &found, .err_msg = &err_msg))
    {
        g_cli_cells->persistent_buf_release(found.out, &g_cli_command_line_cells);
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_WRONG;
    }
    CliJson hits;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &hits,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    err_msg.at = 0ULL;
    const unsigned int list =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &hits,
                   .text = found.out, .size = mmgr_line_ok(&found) ? found.at : 0ULL, .nodes_more = 0u,
                   .text_more = 0ULL, .err_msg = &err_msg, .owner = &g_cli_command_line_cells);
    g_cli_cells->persistent_buf_release(found.out, &g_cli_command_line_cells);
    const int any = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &hits,
                               .node = list, .kind = CFG_JSON_ARRAY) &&
                    (hits.node[list].count != 0u);
    for (unsigned int hit = any ? hits.node[list].first : CLI_JSON_NONE; hit != CLI_JSON_NONE;
         hit = hits.node[hit].next)
    {
        EMBED_CALL(search_hit_write, CommandLineSearchCtx, .hits = &hits, .hit = hit);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &hits);
    return any ? 0 : 1;
}
