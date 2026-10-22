// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_search.c: `orior_cli edit search`, every line in the tree's files that holds a text, written as grep
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
    CliLine line;
    if (!cli_line_growing(&line, CLI_SEARCH_LINE_FIRST, "a search's hit written"))
    {
        return;
    }
    const char *const path = cli_json_string(hits, cli_json_member(hits, hit, "path"), NULL);
    cli_line_text(&line, (path != NULL) ? path : "");
    static const char *const s_numbers[2] = {"line", "col"};
    for (unsigned int number = 0u; number < 2u; number += 1u)
    {
        const unsigned int node = cli_json_member(hits, hit, s_numbers[number]);
        cli_line_character(&line, ':');
        if (cli_json_is(hits, node, CFG_JSON_NUMBER))
        {
            cli_line_counted(&line, hits->text + hits->node[node].text, hits->node[node].length);
        }
    }
    cli_line_text(&line, ": ");
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(hits, cli_json_member(hits, hit, "text"), &length);
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed((text != NULL) ? text : "", (text != NULL) ? length : 0ull, &from, &to);
    cli_line_counted(&line, (text != NULL) ? text + from : "", to - from);
    cli_out_line(line.out, cli_line_fits(&line) ? line.at : 0ull);
    cli_cell_given(line.out);
}

int cli_console_search(const char *named, const char *const *words, unsigned int count)
{
    CliSearching how = {0, 0, 0};
    static char s_query[1u << 12u];
    CliLine query = {s_query, sizeof(s_query), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        how.match_case |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--case", .cap = CLI_CELLS_POOL}));
        how.word |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--word", .cap = CLI_CELLS_POOL}));
        how.regex |= (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--regex", .cap = CLI_CELLS_POOL}));
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--case", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--word", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--regex", .cap = CLI_CELLS_POOL})))
        {
            cli_line_text(&query, (query.at > 0ull) ? " " : "");
            cli_line_text(&query, words[at]);
        }
    }
    if (cli_line_closed(&query) == 0ull)
    {
        cli_err("search needs text: orior_cli edit search <text>");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_LONGEST];
    char why[1u << 14u];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_command_line_tree(named, s_root, &said))
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_NO_CODE;
    }
    static CliPatterns s_none;
    CliLine found;
    if (!cli_line_growing(&found, CLI_SEARCH_HITS_FIRST, "the hits of a search"))
    {
        cli_err("the hits of a search could not be given a cell");
        return CLI_WRONG;
    }
    if (!cli_files_search(s_root, s_query, how, &s_none, &found, &said))
    {
        cli_cell_given(found.out);
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_WRONG;
    }
    CliJson hits;
    cli_json_begun(&hits, NULL, 0u, NULL, 0ull);
    said.at = 0ull;
    const unsigned int list =
        cli_json_taken(&hits, found.out, cli_line_fits(&found) ? found.at : 0ull, 0u, 0ull, &said);
    cli_cell_given(found.out);
    const int any = cli_json_is(&hits, list, CFG_JSON_ARRAY) && (hits.node[list].count != 0u);
    for (unsigned int hit = any ? hits.node[list].first : CLI_JSON_NONE; hit != CLI_JSON_NONE;
         hit = hits.node[hit].next)
    {
        search_hit_written(&hits, hit);
    }
    cli_json_given(&hits);
    return any ? 0 : 1;
}
