// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_lsp.c: the calls of the language servers and of what reads the code: a file opened, changed and closed;
// a hover, the definitions, the completions, the usages, a rename, a signature, the quick fixes and an action carried
// out; a file's colors, spans and hints; the call hierarchy; the tree's check started and stopped; and the checkers
// named. Each path is written as it stands in the tree
#include "../editor/files.h"
#include "serve_internal.h"

// the tree open into `root` and the file of the argument `path` joined to it into `full`, the servers told where what
// they say goes: 1, or 0 with why said
static int lsp_file(const CliServeArgs *args, char *root, char *full, CliLine *said)
{
    return cli_serve_at(args, root, full, said);
}

// the arguments `line` and `col`: 1, or 0 with why said
static int lsp_line_col(const CliServeArgs *args, unsigned int *line, unsigned int *col, CliLine *said)
{
    unsigned long long read_line = 0ull;
    unsigned long long read_col = 0ull;
    if (!cli_serve_whole(args, "line", &read_line, said) || !cli_serve_whole(args, "col", &read_col, said))
    {
        return 0;
    }
    // a place is cut to its low words, as the protocol counts them
    *line = (unsigned int)read_line;
    *col = (unsigned int)read_col;
    return 1;
}

static int lsp_open(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    const char *const language = cli_serve_string(args, "language", said);
    const char *const text = (language != NULL) ? cli_serve_string(args, "text", said) : NULL;
    if ((text == NULL) || !lsp_file(args, root, full, said))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    const CliServersEmit emit = cli_serve_emitter(root);
    return cli_servers_open(root, full, language, text, length, &emit, out, said);
}

static int lsp_change(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    const char *const text = cli_serve_string(args, "text", said);
    if ((text == NULL) || !lsp_file(args, root, full, said))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    if (!cli_servers_change(full, text, length, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int lsp_close(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!lsp_file(args, root, full, said) || !cli_servers_close(full, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int lsp_hover(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_hover(full, line, col, out, said);
}

static int lsp_definition(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_definition(root, full, line, col, out, said);
}

static int lsp_complete(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_complete(full, line, col, out, said);
}

static int lsp_references(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_references(root, full, line, col, out, said);
}

static int lsp_renamable(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_renamable(full, line, col, out, said);
}

static int lsp_rename(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    const char *const name = cli_serve_string(args, "name", said);
    return (name != NULL) && lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_rename(root, full, line, col, name, out, said);
}

static int lsp_signature(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_signature(full, line, col, out, said);
}

static int lsp_actions(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    return lsp_file(args, root, full, said) && cli_serve_place(args, "from", &from, said) &&
           cli_serve_place(args, "to", &to, said) && cli_servers_actions(full, from, to, out, said);
}

static int lsp_act(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!lsp_file(args, root, full, said))
    {
        return 0;
    }
    static char s_raw[1u << 20u];
    static CliLock s_raw_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_raw_lock);
    CliLine raw = {s_raw, sizeof(s_raw), 0ull};
    const unsigned int given = cli_serve_arg(args, "raw");
    if (given != CLI_JSON_NONE)
    {
        cli_json_write(args->json, given, &raw, 0u, 0);
    }
    else
    {
        cli_line_text(&raw, "null");
    }
    const int done = cli_line_fits(&raw) && cli_servers_act(root, full, s_raw, raw.at, out, said);
    cli_lock_give(&s_raw_lock);
    return done;
}

static int parse_colors(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    // the lines are cut to their low words, as no file holds more
    return lsp_file(args, root, full, said) && cli_serve_whole(args, "from", &from, said) &&
           cli_serve_whole(args, "to", &to, said) &&
           cli_servers_colors(full, (unsigned int)from, (unsigned int)to, out, said);
}

static int parse_spans(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    return lsp_file(args, root, full, said) && cli_serve_place(args, "from", &from, said) &&
           cli_serve_place(args, "to", &to, said) && cli_servers_spans(full, from, to, out, said);
}

static int lsp_hints(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    // the lines are cut to their low words, as no file holds more
    return lsp_file(args, root, full, said) && cli_serve_whole(args, "from", &from, said) &&
           cli_serve_whole(args, "to", &to, said) &&
           cli_servers_hints(full, (unsigned int)from, (unsigned int)to, out, said);
}

static int calls_root(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_call_root(root, full, line, col, out, said);
}

static int calls_of(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    int incoming = 0;
    if (!lsp_file(args, root, full, said) || !cli_serve_bool(args, "incoming", &incoming, said))
    {
        return 0;
    }
    static char s_item[1u << 20u];
    static CliLock s_item_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_item_lock);
    CliLine item = {s_item, sizeof(s_item), 0ull};
    const unsigned int given = cli_serve_arg(args, "item");
    if (given != CLI_JSON_NONE)
    {
        cli_json_write(args->json, given, &item, 0u, 0);
    }
    else
    {
        cli_line_text(&item, "null");
    }
    const int done = cli_line_fits(&item) && cli_servers_calls(root, full, s_item, item.at, incoming, out, said);
    cli_lock_give(&s_item_lock);
    return done;
}

static int problems_check(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    CliServeFiles *const listed = cli_serve_files_listed(root, said);
    if (listed == NULL)
    {
        return 0;
    }
    const CliServersEmit emit = cli_serve_emitter(root);
    cli_line_decimal(out, cli_servers_check_tree(root, listed->paths, listed->count, &emit), 1u);
    cli_serve_files_given(listed);
    return 1;
}

static int problems_check_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_servers_stop_check();
    cli_line_text(out, "null");
    return 1;
}

static int checkers_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_names[CLI_SERVERS_CHECKERS];
    unsigned int count = 0u;
    if (!cli_serve_strings(args, "names", s_names, CLI_SERVERS_CHECKERS, &count, said))
    {
        return 0;
    }
    char root[CLI_PATH_LONGEST];
    const int has_root = cli_serve_root_now(root);
    // with no tree open what the checkers find goes nowhere
    const CliServersEmit emit = has_root ? cli_serve_emitter(root) : (CliServersEmit){NULL, NULL, ""};
    return cli_servers_set_checkers(s_names, count, has_root ? root : NULL, &emit, out, said);
}

const CliServeCall g_cli_serve_lsp_calls[] = {
    {"lsp_open", lsp_open},
    {"lsp_change", lsp_change},
    {"lsp_close", lsp_close},
    {"lsp_hover", lsp_hover},
    {"lsp_definition", lsp_definition},
    {"lsp_complete", lsp_complete},
    {"lsp_references", lsp_references},
    {"lsp_renamable", lsp_renamable},
    {"lsp_rename", lsp_rename},
    {"lsp_signature", lsp_signature},
    {"lsp_actions", lsp_actions},
    {"lsp_act", lsp_act},
    {"parse_colors", parse_colors},
    {"parse_spans", parse_spans},
    {"lsp_hints", lsp_hints},
    {"calls_root", calls_root},
    {"calls_of", calls_of},
    {"problems_check", problems_check},
    {"problems_check_stop", problems_check_stop},
    {"checkers_set", checkers_set},
};
const unsigned int g_cli_serve_lsp_count = sizeof(g_cli_serve_lsp_calls) / sizeof(g_cli_serve_lsp_calls[0]);
