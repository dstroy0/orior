// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_check.c: the checks of orior's C side, each printed on the output as it is made, and their count last
#include "cli_check.h"
#include "../../../../../ui/cli/src/command_line/cells.h"

#include "../../../../../ui/cli/src/text/text.h"

static unsigned long long s_checks;
static unsigned long long s_failed;
// the mark of the lane this process runs its checks on, 0 while it is on none
static size_t s_mark;

void cli_check_begun(void)
{
    s_mark = cli_lane_entered(0u, CLI_LANE_COMMAND_MS);
}

// a line written to the output
static void check_line(const VerbaLinea *line)
{
    query_terminal_written(0, line->out, mmgr_line_ok(line) ? line->at : 0ull);
}

void cli_check(int passed, const char *what)
{
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, passed ? "  ok   " : "  FAIL ");
    mmgr_line_put(&line, what);
    mmgr_line_ch(&line, '\n');
    check_line(&line);
    s_checks += 1ull;
    s_failed += passed ? 0ull : 1ull;
}

void cli_check_text(const char *text, unsigned long long size, const char *wanted, const char *what)
{
    const int matches = (cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL}) == size) &&
                        (memor.cmp(&(MemoriaCfg){.src = text, .other = wanted, .bytes = size}) == 0);
    cli_check(matches, what);
    if (!matches)
    {
        static char s_came[1u << 16u];
        VerbaLinea line = {s_came, sizeof(s_came), 0ull};
        mmgr_line_put(&line, "    came: ");
        mmgr_line_put_n(&line, text, size);
        mmgr_line_put(&line, "\n    wanted: ");
        mmgr_line_put(&line, wanted);
        mmgr_line_ch(&line, '\n');
        check_line(&line);
    }
}

void cli_check_folder(const char *name, char *full)
{
    if (!query_path_full(name, full, CLI_PATH_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    (void)query_removed(full);
    (void)query_folder_made(full);
}

// The tree a text is parsed into, a cell sized to the text at each parse, the one before given back, and the last
// given back as the checks end
static CliJson s_json;

const CliJson *cli_check_parsed(const char *text, unsigned long long size, unsigned int *root)
{
    cli_json_given(&s_json);
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    *root = cli_json_taken(&s_json, text, size, 0u, 0ull, &err_msg, &g_checks_cells);
    return &s_json;
}

const char *cli_check_member_text(unsigned int node, const char *name)
{
    const char *const text = cli_json_string(&s_json, cli_json_member(&s_json, node, name), NULL);
    return (text != NULL) ? text : "";
}

unsigned int cli_check_finding(unsigned int found, const char *code, const char *part)
{
    for (unsigned int at = 0u; cli_json_item(&s_json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(&s_json, found, at);
        const char *const message = cli_check_member_text(cli_json_member(&s_json, one, "message"), "value");
        if (((code == NULL) ||
             (cellul.eq(&(CatenaFinitaCfg){
                 .src = cli_check_member_text(one, "code"), .other = code, .cap = CLI_CELLS_POOL}))) &&
            ((part == NULL) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = message, .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL)))
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
    VerbaLinea line = {variable, sizeof(variable), 0ull};
    mmgr_line_put(&line, "ORIOR_");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "_ADDRESS");
    (void)mmgr_line_finish(&line);
    char named[CLI_NAME_MAX_LEN];
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
        // a port is below 65536, which an unsigned int represents
        *at = (unsigned int)number;
    }
}

int cli_checked(void)
{
    cli_json_given(&s_json);
    cli_lane_rewound(0u, s_mark);
    char text[128];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_decimal(&line, s_checks, 1u);
    mmgr_line_put(&line, " checks, ");
    mmgr_line_decimal(&line, s_failed, 1u);
    mmgr_line_put(&line, " failed, cells ");
    cli_cells_usage_written(&line);
    mmgr_line_ch(&line, '\n');
    check_line(&line);
    return (s_failed > 0ull) ? 1 : 0;
}
