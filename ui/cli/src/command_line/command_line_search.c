// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_search.c: `orior_cli edit search`, every line in the tree's files that contains a text, written as grep
// writes it
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "command_line_internal.h"

// the bytes a search's hits are begun in before they grow, and a hit's line before it grows
#define CLI_SEARCH_HITS_FIRST (64ull << 10u)
#define CLI_SEARCH_LINE_FIRST (1ull << 10u)

// one hit of the tree `hits` written to the output as path:line:column: line
static void search_hit_written(const CliJson *hits, unsigned int hit)
{
    VerbaLinea line;
    if (!cli_growing_line_made(&line, CLI_SEARCH_LINE_FIRST, &g_cli_command_line_cells))
    {
        return;
    }
    const char *const path = cli_json_string(hits, cli_json_member(hits, hit, "path"), NULL);
    mmgr_line_put(&line, (path != NULL) ? path : "");
    static const char *const s_numbers[2] = {"line", "col"};
    for (unsigned int number = 0u; number < 2u; number += 1u)
    {
        const unsigned int node = cli_json_member(hits, hit, s_numbers[number]);
        mmgr_line_ch(&line, ':');
        if (cli_json_is(hits, node, CFG_JSON_NUMBER))
        {
            mmgr_line_put_n(&line, hits->text + hits->node[node].text, hits->node[node].length);
        }
    }
    mmgr_line_put(&line, ": ");
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(hits, cli_json_member(hits, hit, "text"), &length);
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed((text != NULL) ? text : "", (text != NULL) ? length : 0ull, &from, &to);
    mmgr_line_put_n(&line, (text != NULL) ? text + from : "", to - from);
    cli_out_line(line.out, mmgr_line_ok(&line) ? line.at : 0ull);
    cli_cell_release(line.out, &g_cli_command_line_cells);
}

int cli_console_search(const char *named, const char *const *words, unsigned int count)
{
    CliSearching how = {0, 0, 0};
    static char s_query[1u << 12u];
    VerbaLinea query = {s_query, sizeof(s_query), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        how.match_case |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--case", .cap = CLI_CELLS_POOL}));
        how.word |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--word", .cap = CLI_CELLS_POOL}));
        how.regex |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--regex", .cap = CLI_CELLS_POOL}));
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--case", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--word", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--regex", .cap = CLI_CELLS_POOL})))
        {
            mmgr_line_put(&query, (query.at > 0ull) ? " " : "");
            mmgr_line_put(&query, words[at]);
        }
    }
    if (mmgr_line_finish(&query) == 0ull)
    {
        cli_err("search needs text: orior_cli edit search <text>");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    char why[1u << 14u];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_command_line_tree(named, s_root, &err_msg))
    {
        cli_err_line(why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_NO_CODE;
    }
    static CliPatterns s_none;
    VerbaLinea found;
    if (!cli_growing_line_made(&found, CLI_SEARCH_HITS_FIRST, &g_cli_command_line_cells))
    {
        cli_err("the hits of a search could not be given a cell");
        return CLI_WRONG;
    }
    if (!cli_files_search(s_root, s_query, how, &s_none, &found, &err_msg))
    {
        cli_cell_release(found.out, &g_cli_command_line_cells);
        cli_err_line(why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    CliJson hits;
    cli_json_begun(&hits, NULL, 0u, NULL, 0ull);
    err_msg.at = 0ull;
    const unsigned int list = cli_json_taken(&hits, found.out, mmgr_line_ok(&found) ? found.at : 0ull, 0u, 0ull,
                                             &err_msg, &g_cli_command_line_cells);
    cli_cell_release(found.out, &g_cli_command_line_cells);
    const int any = cli_json_is(&hits, list, CFG_JSON_ARRAY) && (hits.node[list].count != 0u);
    for (unsigned int hit = any ? hits.node[list].first : CLI_JSON_NONE; hit != CLI_JSON_NONE;
         hit = hits.node[hit].next)
    {
        search_hit_written(&hits, hit);
    }
    cli_json_given(&hits);
    return any ? 0 : 1;
}
