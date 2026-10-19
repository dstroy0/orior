// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_check.c: the checks of orior's C side, each said on the output as it is made, and their count last
#include "cli_check.h"
#include "../../../../../ui/cli/src/command_line/cells.h"

#include "../../../../../ui/cli/src/text/text.h"

static unsigned long long s_checks;
static unsigned long long s_failed;

// a line written to the output
static void check_line(const CliLine *line)
{
    query_terminal_written(0, line->out, cli_line_fits(line) ? line->at : 0ull);
}

void cli_check(int held, const char *what)
{
    char text[4096];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, held ? "  ok   " : "  FAIL ");
    cli_line_text(&line, what);
    cli_line_character(&line, '\n');
    check_line(&line);
    s_checks += 1ull;
    s_failed += held ? 0ull : 1ull;
}

void cli_check_text(const char *text, unsigned long long size, const char *wanted, const char *what)
{
    const int held = (cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL}) == size) &&
                     (memor.cmp(&(MemoriaCfg){.src = text, .other = wanted, .bytes = size}) == 0);
    cli_check(held, what);
    if (!held)
    {
        static char s_came[1u << 16u];
        CliLine line = {s_came, sizeof(s_came), 0ull};
        cli_line_text(&line, "    came: ");
        cli_line_counted(&line, text, size);
        cli_line_text(&line, "\n    wanted: ");
        cli_line_text(&line, wanted);
        cli_line_character(&line, '\n');
        check_line(&line);
    }
}

void cli_check_folder(const char *name, char *full)
{
    if (!query_path_full(name, full, CLI_PATH_LONGEST))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    (void)query_removed(full);
    (void)query_folder_made(full);
}

// The tree a text is parsed into, a cell taken at the first parse and given back as the checks end
typedef struct
{
    CliJsonNode node[1u << 18u];
    char tree_text[32u << 20u];
    CfgJsonToken token[1u << 18u];
} CliCheckTree;

static CliCheckTree *s_tree;
static CliJson s_json;

const CliJson *cli_check_parsed(const char *text, unsigned long long size, unsigned int *root)
{
    *root = CLI_JSON_NONE;
    s_tree = (s_tree != NULL) ? s_tree : cli_cell_taken(sizeof(*s_tree), "a check's parsed tree");
    if (s_tree == NULL)
    {
        cli_check(0, "a check's parsed tree is taken");
        return &s_json;
    }
    cli_json_begun(&s_json, s_tree->node, sizeof(s_tree->node) / sizeof(s_tree->node[0]), s_tree->tree_text,
                   sizeof(s_tree->tree_text));
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    *root = cli_json_read(&s_json, text, size, s_tree->token, sizeof(s_tree->token) / sizeof(s_tree->token[0]), &said);
    return &s_json;
}

const char *cli_check_member_text(unsigned int node, const char *name)
{
    const char *const text = cli_json_string(&s_json, cli_json_member(&s_json, node, name), NULL);
    return (text != NULL) ? text : "";
}

unsigned int cli_check_finding(unsigned int found, const char *code, const char *holds)
{
    for (unsigned int at = 0u; cli_json_item(&s_json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(&s_json, found, at);
        const char *const message = cli_check_member_text(cli_json_member(&s_json, one, "message"), "value");
        if (((code == NULL) ||
             (cellul.eq(&(CatenaFinitaCfg){
                 .src = cli_check_member_text(one, "code"), .other = code, .cap = CLI_CELLS_POOL}))) &&
            ((holds == NULL) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = message, .cap = CLI_CELLS_POOL, .other = holds, .other_cap = CLI_CELLS_POOL}) != NULL)))
        {
            return one;
        }
    }
    return CLI_JSON_NONE;
}

int cli_check_completes(const CliCompletions *items, const char *label)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        if ((item->label_size == cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) ==
             0))
        {
            return 1;
        }
    }
    return 0;
}

const CliCompletions *cli_check_build_completed(const char *root, const char *path, const char *text)
{
    static CliCompletions s_items;
    unsigned int line = 0u;
    unsigned int col = 0u;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        line += (at[0] == '\n') ? 1u : 0u;
        col = (at[0] == '\n') ? 0u : col + 1u;
    }
    cli_completions_begun(&s_items);
    cli_build_complete(root, path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), line, col,
                       &s_items);
    return &s_items;
}

void cli_check_server(const char *name, unsigned int port, char *host, unsigned int *at)
{
    char variable[64];
    CliLine line = {variable, sizeof(variable), 0ull};
    cli_line_text(&line, "ORIOR_");
    cli_line_text(&line, name);
    cli_line_text(&line, "_ADDRESS");
    (void)cli_line_closed(&line);
    char named[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = host,
                            .src = "127.0.0.1",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "127.0.0.1", .cap = CLI_CELLS_POOL}) + 1u});
    *at = port;
    if (!query_environment_read(variable, named, sizeof(named)))
    {
        return;
    }
    char *const colon = cli_text_last(named, cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), ':');
    unsigned long long number = 0ull;
    int digits = (colon != NULL) && (colon[1] != '\0');
    for (const char *letter = (colon != NULL) ? colon + 1 : ""; digits && (letter[0] != '\0'); letter += 1)
    {
        digits = (letter[0] >= '0') && (letter[0] <= '9') && (number < 65536ull);
        number = (number * 10ull) + (unsigned long long)(letter[0] - '0');
    }
    if (digits && (number < 65536ull))
    {
        colon[0] = '\0';
        memor.cpy(&(MemoriaCfg){.dst = host,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        // a port is below 65536, which an unsigned int holds
        *at = (unsigned int)number;
    }
}

int cli_checked(void)
{
    cli_cell_given(s_tree);
    s_tree = NULL;
    char text[128];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_decimal(&line, s_checks, 1u);
    cli_line_text(&line, " checks, ");
    cli_line_decimal(&line, s_failed, 1u);
    cli_line_text(&line, " failed, cells ");
    cli_cells_said(&line);
    cli_line_character(&line, '\n');
    check_line(&line);
    return (s_failed > 0ull) ? 1 : 0;
}
