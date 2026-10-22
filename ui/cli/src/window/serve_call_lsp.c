// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_lsp.c: the calls of the language servers and of what reads the code: a file opened, changed and closed;
// a hover, the definitions, the completions, the usages, a rename, a signature, the quick fixes and an action carried
// out; a file's colors, spans and hints; the call hierarchy; the tree's check started and stopped; the checkers named;
// a plugin's server started and stopped, asked, told, and its own request answered; and a plugin's secrets kept in
// the system's keychain of the machine its server runs on, and read back. Each path is written as it
// stands in the tree
#include "../editor/files.h"
#include "serve_internal.h"

// the bytes an argument handed to a server is begun to be written in, doubling as it fills
#define CLI_SERVE_LSP_JSON_FIRST 4096u

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

static int lsp_hover_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_hover(full, line, col, out, said);
}

static int lsp_definition_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_definition(root, full, line, col, out, said);
}

static int lsp_complete_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_complete(full, line, col, out, said);
}

static int lsp_references_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_references(root, full, line, col, out, said);
}

static int lsp_renamable_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_renamable(full, line, col, out, said);
}

static int lsp_rename_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    const char *const name = cli_serve_string(args, "name", said);
    return (name != NULL) && lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_rename(root, full, line, col, name, out, said);
}

static int lsp_signature_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_signature(full, line, col, out, said);
}

static int lsp_actions_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    return lsp_file(args, root, full, said) && cli_serve_place(args, "from", &from, said) &&
           cli_serve_place(args, "to", &to, said) && cli_servers_actions(full, from, to, out, said);
}

// the argument `key` written as JSON into `line`, made growing, null where it is not given, the line given back with
// cli_cell_given(line->out) either way: 1, or 0 with why said where the cellblock could not meet it
static int lsp_json_written(const CliServeArgs *args, const char *key, CliLine *line, CliLine *said)
{
    (void)cli_line_growing(line, CLI_SERVE_LSP_JSON_FIRST, "an argument a server is handed");
    const unsigned int given = cli_serve_arg(args, key);
    if (given != CLI_JSON_NONE)
    {
        cli_json_write(args->json, given, line, 0u, 0);
    }
    else
    {
        cli_line_text(line, "null");
    }
    if (!cli_line_fits(line))
    {
        cli_line_text(said, "the cells cannot meet the argument ");
        cli_line_text(said, key);
        return 0;
    }
    return 1;
}

static int lsp_act_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    if (!lsp_file(args, root, full, said))
    {
        return 0;
    }
    CliLine raw = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int done =
        lsp_json_written(args, "raw", &raw, said) && cli_servers_act(root, full, raw.out, raw.at, out, said);
    cli_cell_given(raw.out);
    return done;
}

static int parse_colors_now(const CliServeArgs *args, CliLine *out, CliLine *said)
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

static int parse_spans_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    return lsp_file(args, root, full, said) && cli_serve_place(args, "from", &from, said) &&
           cli_serve_place(args, "to", &to, said) && cli_servers_spans(full, from, to, out, said);
}

static int lsp_hints_now(const CliServeArgs *args, CliLine *out, CliLine *said)
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

static int calls_root_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return lsp_file(args, root, full, said) && lsp_line_col(args, &line, &col, said) &&
           cli_servers_call_root(root, full, line, col, out, said);
}

static int calls_of_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    int incoming = 0;
    if (!lsp_file(args, root, full, said) || !cli_serve_bool(args, "incoming", &incoming, said))
    {
        return 0;
    }
    CliLine item = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int done = lsp_json_written(args, "item", &item, said) &&
                     cli_servers_calls(root, full, item.out, item.at, incoming, out, said);
    cli_cell_given(item.out);
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

// the argument `key` written as JSON into `json`, a growing line given back by the caller, `null` where there is none:
// 1, or 0 with why said where the cells cannot meet it
static int plugin_json_of(const CliServeArgs *args, const char *key, CliLine *json, CliLine *said)
{
    *json = (CliLine){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(json, CLI_SERVE_LSP_JSON_FIRST, "what a plugin hands its server");
    const unsigned int node = cli_serve_arg(args, key);
    if (node == CLI_JSON_NONE)
    {
        cli_line_text(json, "null");
    }
    else
    {
        cli_json_write(args->json, node, json, 0u, 0);
    }
    if (!cli_line_fits(json))
    {
        cli_line_text(said, "the cells cannot meet what the plugin hands its server");
        return 0;
    }
    return 1;
}

static int plugin_server_start(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    char root[CLI_PATH_LONGEST];
    if ((id == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    // the arguments the plugin puts before its plugin.json's, a Java machine's own among them
    const char *first[64];
    unsigned int firsts = 0u;
    CliLine options;
    const int read = plugin_json_of(args, "options", &options, said) &&
                     cli_serve_strings_or(args, "args", first, 64u, &firsts, said);
    const CliServersEmit emit = cli_serve_emitter(root);
    const int started = read && cli_servers_plugin_start(root, id, options.out, options.at, first, firsts, &emit, said);
    cli_cell_given(options.out);
    cli_line_text(out, "null");
    return started;
}

static int plugin_server_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    if (id == NULL)
    {
        return 0;
    }
    cli_servers_plugin_stop(id);
    cli_line_text(out, "null");
    return 1;
}

static int plugin_server_request_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    const char *const method = (id != NULL) ? cli_serve_string(args, "method", said) : NULL;
    CliLine params;
    if ((method == NULL) || !plugin_json_of(args, "params", &params, said))
    {
        return 0;
    }
    const int done = cli_servers_plugin_request(id, method, params.out, params.at,
                                                cli_serve_whole_or(args, "wait", 30000ull), out, said);
    cli_cell_given(params.out);
    return done;
}

static int plugin_server_notify(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    const char *const method = (id != NULL) ? cli_serve_string(args, "method", said) : NULL;
    CliLine params;
    if ((method == NULL) || !plugin_json_of(args, "params", &params, said))
    {
        return 0;
    }
    const int done = cli_servers_plugin_notify(id, method, params.out, params.at, said);
    cli_cell_given(params.out);
    cli_line_text(out, "null");
    return done;
}

static int plugin_server_answer(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    CliLine request;
    CliLine result;
    if ((id == NULL) || !plugin_json_of(args, "request", &request, said))
    {
        return 0;
    }
    const int read = plugin_json_of(args, "result", &result, said);
    // the request's id as the server gave it, a number or a string, ended with a 0 to be written back as it came
    cli_line_character(&request, '\0');
    const int done =
        read && cli_line_fits(&request) && cli_servers_plugin_answer(id, request.out, result.out, result.at, said);
    cli_cell_given(result.out);
    cli_cell_given(request.out);
    cli_line_text(out, "null");
    return done;
}

// the keychain's service for the plugin of the argument `id`, "plugin " and the id, into `service`, which holds
// CLI_NAME_LONGEST: 1, or 0 with why said
static int plugin_service(const CliServeArgs *args, char *service, CliLine *said)
{
    const char *const id = cli_serve_string(args, "id", said);
    CliLine named = {service, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&named, "plugin ");
    cli_line_text(&named, (id != NULL) ? id : "");
    return (id != NULL) && (cli_line_closed(&named) != 0ull);
}

static int plugin_secret_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char service[CLI_NAME_LONGEST];
    const char *const key = plugin_service(args, service, said) ? cli_serve_string(args, "key", said) : NULL;
    if (key == NULL)
    {
        return 0;
    }
    char secret[CLI_PATH_LONGEST];
    if (cli_keychain_read(service, key, secret, sizeof(secret)))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)secret,
                              cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

static int plugin_secret_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char service[CLI_NAME_LONGEST];
    const char *const key = plugin_service(args, service, said) ? cli_serve_string(args, "key", said) : NULL;
    if (key == NULL)
    {
        return 0;
    }
    // a value of null forgets the secret
    const char *const value = cli_serve_string_or(args, "value", NULL);
    const int done =
        (value != NULL) ? cli_keychain_keep(service, key, value, said) : cli_keychain_forget(service, key, said);
    cli_line_text(out, "null");
    return done;
}

// each ask of a server is answered later where the page asks so
CLI_SERVE_ASKED(lsp_hover, lsp_hover_now)
CLI_SERVE_ASKED(lsp_definition, lsp_definition_now)
CLI_SERVE_ASKED(lsp_complete, lsp_complete_now)
CLI_SERVE_ASKED(lsp_references, lsp_references_now)
CLI_SERVE_ASKED(lsp_renamable, lsp_renamable_now)
CLI_SERVE_ASKED(lsp_rename, lsp_rename_now)
CLI_SERVE_ASKED(lsp_signature, lsp_signature_now)
CLI_SERVE_ASKED(lsp_actions, lsp_actions_now)
CLI_SERVE_ASKED(lsp_act, lsp_act_now)
CLI_SERVE_ASKED(parse_colors, parse_colors_now)
CLI_SERVE_ASKED(parse_spans, parse_spans_now)
CLI_SERVE_ASKED(lsp_hints, lsp_hints_now)
CLI_SERVE_ASKED(calls_root, calls_root_now)
CLI_SERVE_ASKED(calls_of, calls_of_now)
CLI_SERVE_ASKED(plugin_server_request, plugin_server_request_now)

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
    {"plugin_server_start", plugin_server_start},
    {"plugin_server_stop", plugin_server_stop},
    {"plugin_server_request", plugin_server_request},
    {"plugin_server_notify", plugin_server_notify},
    {"plugin_server_answer", plugin_server_answer},
    {"plugin_secret_read", plugin_secret_read},
    {"plugin_secret_write", plugin_secret_write},
};
const unsigned int g_cli_serve_lsp_count = sizeof(g_cli_serve_lsp_calls) / sizeof(g_cli_serve_lsp_calls[0]);
