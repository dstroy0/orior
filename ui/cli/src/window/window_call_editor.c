// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_editor.c: the window's calls on the editor's view of a text stored as a branch opened it: the view opened
// on the text under a key, sized to the window's rows and columns, each input the window sends taken, the minimap's
// lines asked for, and the view closed; an open and every input answered with the view's frame, which the window
// draws. The view is handed to its file's language server from the branch's own text and the server told each change
// as the window asks after the frame that shows it; the hover at a place, the completions at the cursor and the
// signature of the call the cursor is in are answered from the tree's declarations and, where the view is served and
// the call's `server` is not false, the server, whose answer waits on it: the window asks without the server first and
// with it after
#include "../command_line/file_writing.h"
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../servers/servers.h"
#include "../servers/servers_internal.h"
#include "window_internal.h"

// the bytes an answer of the tree's or the server's is begun in before it grows, the most declarations a hover
// shows, and the most bytes a word takes
#define CLI_EDITOR_ANSWER_FIRST 4096u
#define CLI_EDITOR_HOVER_MOST 6u
#define CLI_EDITOR_WORD 256u

// the server of the view under `key` told the view's text where the view is served and its text changed since it was
// last told
static void editor_server_told(const char *key)
{
    char full[CLI_PATH_MAX_LEN];
    if (!cli_view_served(key, full) || !cli_view_changed_taken(key))
    {
        return;
    }
    unsigned long long size = 0ull;
    char *const text = cli_document_text_taken(key, &size, &g_cli_window_cells);
    if (text != NULL)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ull};
        (void)cli_servers_change(full, text, size, &ignored);
        cli_cell_release(text, &g_cli_window_cells);
    }
}

// the view opened on the text under `key`, its indentation the file at `path`'s, `rows` by `columns`, answered with its
// first frame
static int editor_view_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long rows = 0ull;
    unsigned long long columns = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "rows", &rows, err_msg) ||
        !cli_serve_whole(args, "columns", &columns, err_msg))
    {
        return 0;
    }
    if (!cli_view_opened(key, cli_serve_string_or(args, "path", "")))
    {
        mmgr_line_put(err_msg, key);
        mmgr_line_put(err_msg, ": no text is stored under the key, or no more views can be held");
        return 0;
    }
    // the rows and columns a window shows are far inside what an unsigned int represents
    const CliViewInput sized = {CLI_VIEW_SIZE, NULL, NULL, 0ull, 0u, 0ull, 0ull, 0u, 0, 0ull, (unsigned int)rows,
                                (unsigned int)columns, 0};
    return cli_view_input(key, &sized, err_msg) && cli_view_frame_written(key, out);
}

// one input of the window's taken by the view under `key`, `{"key", "kind", ...}` with what the kind carries: a key's
// `name`; a `text`; the pointer's `pointer`, `row`, `column`, `clicks`, `shift` and `gutter`; a scroll's `top`; a
// size's `rows` and `columns`; answered with the view's frame
static int editor_view_input(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long kind = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "kind", &kind, err_msg))
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    // a kind, a pointer's event, a click count and a size are each far inside what an unsigned int represents
    input.kind = (unsigned int)kind;
    input.name = cli_serve_string_or(args, "name", "");
    input.text = (text != NULL) ? text : "";
    input.length = (text != NULL) ? length : 0ull;
    input.pointer = (unsigned int)cli_serve_whole_or(args, "pointer", 0ull);
    input.row = cli_serve_whole_or(args, "row", 0ull);
    input.column = cli_serve_whole_or(args, "column", 0ull);
    input.clicks = (unsigned int)cli_serve_whole_or(args, "clicks", 1ull);
    input.shift = cli_serve_bool_or(args, "shift", 0);
    input.top = cli_serve_whole_or(args, "top", 0ull);
    input.rows = (unsigned int)cli_serve_whole_or(args, "rows", 0ull);
    input.columns = (unsigned int)cli_serve_whole_or(args, "columns", 0ull);
    input.gutter = cli_serve_bool_or(args, "gutter", 0);
    if (!cli_view_input(key, &input, err_msg))
    {
        return 0;
    }
    return cli_view_frame_written(key, out);
}

// the server of the view under `key` told the view's text where it changed since it was last told: null; the window
// calls this off the inputs' lane after each frame, the telling waiting on the server
static int editor_view_told(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    if (key == NULL)
    {
        return 0;
    }
    editor_server_told(key);
    mmgr_line_put(out, "null");
    return 1;
}

// the view under `key` handed to its file's language server, where the text is read whole and its language has a
// server that takes it: `{"served": true}`, else `{"served": false}`
static int editor_view_serve(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    char path[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (key == NULL)
    {
        return 0;
    }
    if (!cli_view_path(key, path))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    cli_full(root, path, full);
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    cli_plugins_languages(&languages);
    const int known = cli_plugins_language_for(&languages, path, id);
    cli_plugins_languages_given(&languages);
    unsigned long long size = 0ull;
    char *const text = known ? cli_document_text_taken(key, &size, &g_cli_window_cells) : NULL;
    int served = 0;
    if (text != NULL)
    {
        char said_text[16];
        VerbaLinea said = {said_text, sizeof(said_text), 0ull};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ull};
        const CliServersSink sink = cli_serve_emitter(root);
        served = cli_servers_open(root, full, id, text, size, &sink, &said, &refused) && mmgr_line_ok(&said) &&
                 (said.at == 4ull) && (memor.cmp(&(MemoriaCfg){.src = said.out, .other = "true", .bytes = 4u}) == 0);
        cli_cell_release(text, &g_cli_window_cells);
    }
    if (served)
    {
        (void)cli_view_served_set(key, full);
    }
    mmgr_line_put(out, served ? "{\"served\":true}" : "{\"served\":false}");
    return 1;
}

// the hover at the cell `column` of the row `row` from the top of the view under `key`: the word the place touches,
// `{"from", "to", "parts"}`, the places `{"line", "col"}` in units, the parts the tree's declarations of the word, each
// its own line as code with its kind and place after, and the server's hover as Markdown where the view is served;
// null where no word stands at the place
static int editor_view_hover(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "row", &row, err_msg) ||
        !cli_serve_whole(args, "column", &column, err_msg))
    {
        return 0;
    }
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if (!cli_view_place_at(key, row, column, &line, &col))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[CLI_EDITOR_WORD];
    if (!cli_view_word_at(key, line, col, &from, &to, word, sizeof(word)))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_put(out, "{\"from\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, "},\"to\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, to, 1u);
    mmgr_line_put(out, "},\"parts\":[");
    unsigned int parts = 0u;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    // the tree's declarations of the word
    char query_text[CLI_EDITOR_WORD + 32u];
    VerbaLinea query = {query_text, sizeof(query_text), 0ull};
    mmgr_line_put(&query, "{\"query\":");
    cli_window_quoted(&query, word);
    mmgr_line_ch(&query, '}');
    VerbaLinea found;
    if (cli_growing_line_made(&found, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
    {
        CliJson json;
        const unsigned int root =
            (mmgr_line_ok(&query) && cli_window_tree_call("symbols_find", query.out, query.at, &found, &ignored) &&
             mmgr_line_ok(&found))
                ? cli_json_taken(&json, found.out, found.at, 0u, 0ull, &ignored, &g_cli_window_cells)
                : CLI_JSON_NONE;
        const unsigned int symbols = (root != CLI_JSON_NONE) ? cli_json_member(&json, root, "symbols") : CLI_JSON_NONE;
        const unsigned int count =
            ((symbols != CLI_JSON_NONE) && cli_json_is(&json, symbols, CFG_JSON_ARRAY)) ? json.node[symbols].count : 0u;
        for (unsigned int at = 0u; (at < count) && (parts < CLI_EDITOR_HOVER_MOST); at += 1u)
        {
            const unsigned int one = cli_json_item(&json, symbols, at);
            unsigned long long name_length = 0ull;
            const char *const name = cli_json_string(&json, cli_json_member(&json, one, "name"), &name_length);
            if ((name == NULL) || (name_length != cellul.len(&(CatenaFinitaCfg){.src = word, .cap = sizeof(word)})) ||
                (memor.cmp(&(MemoriaCfg){.src = name, .other = word, .bytes = name_length}) != 0))
            {
                continue;
            }
            unsigned long long text_length = 0ull;
            const char *const text = cli_json_string(&json, cli_json_member(&json, one, "text"), &text_length);
            unsigned long long kind_length = 0ull;
            const char *const kind = cli_json_string(&json, cli_json_member(&json, one, "kind"), &kind_length);
            unsigned long long path_length = 0ull;
            const char *const path = cli_json_string(&json, cli_json_member(&json, one, "path"), &path_length);
            const unsigned long long declared = cli_servers_whole(&json, cli_json_member(&json, one, "line"), 0ull);
            VerbaLinea part;
            if (!cli_growing_line_made(&part, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
            {
                break;
            }
            if ((text != NULL) && (text_length != 0ull))
            {
                mmgr_line_put(&part, "```\n");
                mmgr_line_put_n(&part, text, text_length);
                mmgr_line_put(&part, "\n```\n\n");
            }
            mmgr_line_put_n(&part, (kind != NULL) ? kind : "", (kind != NULL) ? kind_length : 0ull);
            mmgr_line_put(&part, " `");
            mmgr_line_put_n(&part, (path != NULL) ? path : "", (path != NULL) ? path_length : 0ull);
            mmgr_line_ch(&part, ':');
            mmgr_line_decimal(&part, declared + 1ull, 1u);
            mmgr_line_ch(&part, '`');
            if (mmgr_line_ok(&part))
            {
                mmgr_line_put(out, (parts > 0u) ? "," : "");
                cli_json_quoted(out, part.out, part.at);
                parts += 1u;
            }
            cli_cell_release(part.out, &g_cli_window_cells);
        }
        if (root != CLI_JSON_NONE)
        {
            cli_json_given(&json);
        }
        cli_cell_release(found.out, &g_cli_window_cells);
    }
    // the server's hover, a Markdown string, where the view is served and the server is asked
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea said;
    if (cli_serve_bool_or(args, "server", 1) && cli_view_served(key, full) &&
        cli_growing_line_made(&said, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
    {
        // a place in a text a view shows stands below 2^32 lines and columns
        if (cli_servers_hover(full, (unsigned int)line, (unsigned int)col, &said, &ignored) && mmgr_line_ok(&said) &&
            (said.at > 0ull) && (said.out[0] == '"'))
        {
            mmgr_line_put(out, (parts > 0u) ? "," : "");
            mmgr_line_put_n(out, said.out, said.at);
            parts += 1u;
        }
        cli_cell_release(said.out, &g_cli_window_cells);
    }
    mmgr_line_put(out, "]}");
    return 1;
}

// the completions at the cursor of the view under `key`: `{"from", "prefix", "items", "words"}`, the prefix the word
// before the cursor and `from` its first unit, the items the server's where the view is served, else none, and the
// words the file's own near the cursor
static int editor_view_complete(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long from = 0ull;
    char prefix[CLI_EDITOR_WORD];
    CliPlace cursor;
    CliPlace anchor;
    if (key == NULL)
    {
        return 0;
    }
    if (!cli_view_prefix(key, &from, prefix, sizeof(prefix)) || !cli_view_cursor(key, &cursor, &anchor))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    mmgr_line_put(out, "{\"from\":{\"line\":");
    mmgr_line_decimal(out, cursor.line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, "},\"prefix\":");
    cli_window_quoted(out, prefix);
    mmgr_line_put(out, ",\"items\":");
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea said;
    int answered = 0;
    if (cli_serve_bool_or(args, "server", 1) && cli_view_served(key, full) &&
        cli_growing_line_made(&said, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ull};
        // a place in a text a view shows stands below 2^32 lines and columns
        if (cli_servers_complete(full, (unsigned int)cursor.line, (unsigned int)cursor.col, &said, &ignored) &&
            mmgr_line_ok(&said) && (said.at > 0ull) && (said.out[0] == '['))
        {
            mmgr_line_put_n(out, said.out, said.at);
            answered = 1;
        }
        cli_cell_release(said.out, &g_cli_window_cells);
    }
    mmgr_line_put(out, answered ? ",\"words\":" : "[],\"words\":");
    (void)cli_view_words_written(key, out);
    mmgr_line_ch(out, '}');
    return 1;
}

// the signature of the call the cursor of the view under `key` is in: `{"own", "server"}`, the tree's as
// `symbols_signature` writes it, or null, and the server's where the view is served, or null
static int editor_view_signature(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    char path[CLI_PATH_MAX_LEN];
    CliPlace cursor;
    CliPlace anchor;
    if (key == NULL)
    {
        return 0;
    }
    if (!cli_view_path(key, path) || !cli_view_cursor(key, &cursor, &anchor))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    mmgr_line_put(out, "{\"own\":");
    VerbaLinea own;
    int answered = 0;
    if (cli_growing_line_made(&own, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
    {
        if (cli_window_branch_signature(key, path, cursor.line, cursor.col, &own, &ignored) && mmgr_line_ok(&own) &&
            (own.at > 0ull))
        {
            mmgr_line_put_n(out, own.out, own.at);
            answered = 1;
        }
        cli_cell_release(own.out, &g_cli_window_cells);
    }
    mmgr_line_put(out, answered ? ",\"server\":" : "null,\"server\":");
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea said;
    answered = 0;
    if (cli_serve_bool_or(args, "server", 1) && cli_view_served(key, full) &&
        cli_growing_line_made(&said, CLI_EDITOR_ANSWER_FIRST, &g_cli_window_cells))
    {
        // a place in a text a view shows stands below 2^32 lines and columns
        if (cli_servers_signature(full, (unsigned int)cursor.line, (unsigned int)cursor.col, &said, &ignored) &&
            mmgr_line_ok(&said) && (said.at > 0ull))
        {
            mmgr_line_put_n(out, said.out, said.at);
            answered = 1;
        }
        cli_cell_release(said.out, &g_cli_window_cells);
    }
    mmgr_line_put(out, answered ? "}" : "null}");
    return 1;
}

// the minimap's lines from `first` to `last` of the view under `key`, answered as their bars
static int editor_view_map(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "first", &first, err_msg) ||
        !cli_serve_whole(args, "last", &last, err_msg))
    {
        return 0;
    }
    if (!cli_view_map_written(key, first, last, out))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    return 1;
}

// the view under `key` closed
static int editor_view_close(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    if (key == NULL)
    {
        return 0;
    }
    // the server let go of the file as the view closes
    char full[CLI_PATH_MAX_LEN];
    if (cli_view_served(key, full))
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ull};
        (void)cli_servers_close(full, &ignored);
    }
    cli_view_closed(key);
    mmgr_line_put(out, "null");
    return 1;
}

const CliServeCall g_cli_window_editor_calls[] = {
    {"editor_view_open", editor_view_open},
    {"editor_view_input", editor_view_input},
    {"editor_view_told", editor_view_told},
    {"editor_view_map", editor_view_map},
    {"editor_view_serve", editor_view_serve},
    {"editor_view_hover", editor_view_hover},
    {"editor_view_complete", editor_view_complete},
    {"editor_view_signature", editor_view_signature},
    {"editor_view_close", editor_view_close},
};
const unsigned int g_cli_window_editor_count = sizeof(g_cli_window_editor_calls) / sizeof(g_cli_window_editor_calls[0]);
