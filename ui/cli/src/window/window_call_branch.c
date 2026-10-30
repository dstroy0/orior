// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_branch.c: a branch of the tree opened in the editor: the file read as the tree reads it, a text read
// whole stored under the page's key, one too large to read whole opened in the pages under it, and one as a commit left
// it stored read only, each colored by the grammar its extension points to, the lines in view colored at once and the
// rest walked out from them, each line colored after the answer handed to the page as `branch-colors`; the view moved,
// and the lines of a branch the page mirrors; the language a branch is colored in chosen, its lines colored again and
// handed to the page; the tree's signature for a call in a branch and the bracket pair at a place in one, each read off
// the branch's own lines; the colors of a branch's lines as a print of them asks; and a notebook's cells, each a branch
// stored and colored, and a cell the reader adds
#include "../code/symbols.h"
#include "../command_line/file_writing.h"
#include "../editor/document.h"
#include "../editor/pages.h"
#include "../plugins/plugins.h"
#include "window_internal.h"

// the most lines above a call's line its signature is read from, the most lines above and below a place its bracket
// pair is read across, the room the tree's call is begun in, a state's longest text, and the first lines of a branch
// stored for itself, a cell's, colored as it is stored
#define CLI_BRANCH_SIGNATURE_LINES 4000ull
#define CLI_BRANCH_PAIR_LINES 3000ull
#define CLI_BRANCH_CALL_FIRST 4096u
#define CLI_BRANCH_STATE 1024u
#define CLI_BRANCH_FIRST_LINES 120ull

// a branch's colors handed to the page as they are worked out
static void branch_colors_sent(const char *body, unsigned long long size)
{
    cli_serve_emitted("branch-colors", body, size);
}

// the key the grammar of the language `id` is stored under for the colorer into `grammar`, sized for CLI_NAME_MAX_LEN,
// empty where the language has none
static void branch_grammar_named(const char *id, char *grammar)
{
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((id[0] == '\0') || !cli_plugins_grammar_stored(id, grammar, &err_msg))
    {
        grammar[0] = '\0';
    }
}

// the branch under `key` colored in the language `id`, empty for none: the names its tokens take written into `out`
// as cli_document_names_written writes them, where `out` is not NULL, else handed on with its first colors
static void branch_colored_in(const char *key, const char *id, VerbaLinea *out)
{
    char grammar[CLI_NAME_MAX_LEN];
    branch_grammar_named(id, grammar);
    (void)cli_document_colored_by(key, grammar, out != NULL);
    if (out != NULL)
    {
        cli_document_names_written(grammar, out);
    }
}

// the language the extension of `path` points to, by its id, into `id`, sized for CLI_NAME_MAX_LEN, empty for none
static void branch_language_of(const char *path, char *id)
{
    CliLanguages languages;
    cli_plugins_languages(&languages);
    if (!cli_plugins_language_for(&languages, path, id))
    {
        id[0] = '\0';
    }
    cli_plugins_languages_given(&languages);
}

// `size` bytes of `text` the branch under `key`, the page mirroring its lines from `first` to `last`: 1, or 0 where it
// could not be stored
static int branch_stored(const char *key, const char *text, unsigned long long size, unsigned long long first,
                         unsigned long long last)
{
    CliShape shape;
    return cli_document_store(key, text, size, &shape) && cli_document_held(key, first, last + 1ull);
}

// the branch under `key` colored in the language `id` and its lines from `first` to `last` colored, written into `out`
// as `,"names":` the names its grammar's tokens take `,"colors":` those lines' colors
static void branch_colors_answered(const char *key, const char *id, unsigned long long first, unsigned long long last,
                                   VerbaLinea *out)
{
    mmgr_line_put(out, ",\"names\":");
    branch_colored_in(key, id, out);
    mmgr_line_put(out, ",\"colors\":");
    (void)cli_document_viewed(key, first, last, out);
}

// the file at `path` read as file_read reads it, a text read whole stored under `key` and one file_read windows opened
// in the pages under `key`, either one's lines from `first` to `last` the page's mirror and the text colored in the
// language its extension points to, each line as far as `columns` letters where it is named, and the lines from
// `first` to `last` colored: file_read's answer with the letters each line is colored to, `columns`, 0 for every one,
// the paged
// text's shape, `shape`, null where the pages could not open it, the names its grammar's tokens take, `names`, and
// those lines' colors, `colors`, where it is stored
static int branch_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((path == NULL) || !cli_serve_whole(args, "first", &first, err_msg) ||
        !cli_serve_whole(args, "last", &last, err_msg))
    {
        return 0;
    }
    cli_document_colors_to(branch_colors_sent, 1);
    CliJson json;
    const unsigned int opened = cli_window_tree_called(&json, "file_read", path, "", err_msg);
    if (opened == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&json, cli_json_member(&json, opened, "text"), &length);
    const int windowed = cli_json_is(&json, cli_json_member(&json, opened, "windowed"), CFG_JSON_TRUE);
    // a text read whole a branch whose lines in view are the page's mirror, until it names more
    const int whole = (text != NULL) && branch_stored(key, text, length, first, last);
    const unsigned long long start = out->at;
    cli_json_write(&json, opened, out, 0u, 0);
    cli_json_given(&json);
    // the answer's closing brace, put back after the branch's names and colors
    out->at -= (out->at > start) ? 1ull : 0ull;
    // a text too large to read whole opened in the pages under the key, its shape answered, and a branch whose lines in
    // view are the page's mirror
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (windowed)
    {
        mmgr_line_put(out, ",\"shape\":");
    }
    const unsigned long long shaped = out->at;
    const int paged = windowed && cli_serve_at(args, root, full, err_msg) && cli_pages_open(key, full, out, err_msg) &&
                      cli_document_paged(key, first, last + 1ull);
    if (windowed && !paged)
    {
        out->at = shaped;
        mmgr_line_put(out, "null");
    }
    if (whole || paged)
    {
        // each line's runs handed as far as the view draws, where it names how far, and the answer says how far
        const unsigned long long columns = cli_serve_whole_or(args, "columns", 0ull);
        if (columns != 0ull)
        {
            (void)cli_document_columns(key, columns);
        }
        mmgr_line_put(out, ",\"columns\":");
        mmgr_line_decimal(out, columns, 1u);
        char id[CLI_NAME_MAX_LEN];
        branch_language_of(path, id);
        branch_colors_answered(key, id, first, last, out);
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// the file at `path` as the commit `at` left it, read through `file_at`, the branch under `key`, read only, its lines
// from `first` to `last` the page's mirror and the text colored in the language its extension points to: `{"text",
// "names", "colors"}`, the names and colors where it is stored
static int branch_at(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    const char *const at = (path != NULL) ? cli_serve_string(args, "at", err_msg) : NULL;
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((at == NULL) || !cli_serve_whole(args, "first", &first, err_msg) ||
        !cli_serve_whole(args, "last", &last, err_msg))
    {
        return 0;
    }
    char more[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea commit = {more, sizeof(more), 0ull};
    mmgr_line_put(&commit, ",\"id\":");
    cli_window_quoted(&commit, at);
    if (mmgr_line_finish(&commit) == 0ull)
    {
        mmgr_line_put(err_msg, "the commit's id is longer than an id");
        return 0;
    }
    cli_document_colors_to(branch_colors_sent, 1);
    CliJson json;
    const unsigned int read = cli_window_tree_called(&json, "file_at", path, more, err_msg);
    if (read == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&json, read, &length);
    const int stored = (text != NULL) && branch_stored(key, text, length, first, last);
    mmgr_line_put(out, "{\"text\":");
    cli_json_write(&json, read, out, 0u, 0);
    cli_json_given(&json);
    if (stored)
    {
        char id[CLI_NAME_MAX_LEN];
        branch_language_of(path, id);
        branch_colors_answered(key, id, first, last, out);
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// the view of the branch under `key` moved to the lines from `first` to `last`, those not colored yet colored and
// handed to the page, the lines the page mirrors about the view, from `from` to before `to`, and the letters of each
// line the view draws, `columns`, 0 for every one, where it names them
static int branch_view(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "first", &first, err_msg) ||
        !cli_serve_whole(args, "last", &last, err_msg))
    {
        return 0;
    }
    const unsigned long long none = ~0ull;
    const unsigned long long columns = cli_serve_whole_or(args, "columns", none);
    if (columns != none)
    {
        (void)cli_document_columns(key, columns);
    }
    const unsigned long long from = cli_serve_whole_or(args, "from", none);
    const unsigned long long to = cli_serve_whole_or(args, "to", none);
    if ((from != none) && (to != none))
    {
        (void)cli_document_held(key, from, to);
    }
    (void)cli_document_viewed(key, first, last, NULL);
    mmgr_line_put(out, "null");
    return 1;
}

// the branch under `key` colored in the language `id` from here on, empty for none, its lines colored again from the
// view out and handed to the page, the names its grammar's tokens take with the first
static int branch_language(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const id = (key != NULL) ? cli_serve_string_or(args, "id", "") : NULL;
    if (id == NULL)
    {
        return 0;
    }
    branch_colored_in(key, id, NULL);
    mmgr_line_put(out, "null");
    return 1;
}

// the lines of the branch under `key` read about its line `line`, as many as `above` above it and `below` below it at
// the most and none outside those read for the branch, a paged text's those the page holds: the first into `*first`
// and the last into `*last`, each the line itself where it stands past them
static void branch_lines_about(const char *key, unsigned long long line, unsigned long long above,
                               unsigned long long below, unsigned long long *first, unsigned long long *last)
{
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    (void)cli_document_bounds(key, &low, &high);
    const unsigned long long reach = (line > above) ? (line - above) : 0ull;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long end = ((high != 0ull) && ((line + below) >= high)) ? (high - 1ull) : (line + below);
    *first = (start < line) ? start : line;
    *last = (end > line) ? end : line;
}

// the tree's signature for the call at the line `line`, column `col`, of the branch under `key`, the file at
// `path`: the text's lines from as far above as CLI_BRANCH_SIGNATURE_LINES reach to the line, a paged text's those the
// page holds, read as the tree reads a text from the state the first was entered in, the tree's answer as
// `symbols_signature` gives it
static int branch_signature(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if ((path == NULL) || !cli_serve_whole(args, "line", &line, err_msg) ||
        !cli_serve_whole(args, "col", &col, err_msg))
    {
        return 0;
    }
    return cli_window_branch_signature(key, path, line, col, out, err_msg);
}

int cli_window_branch_signature(const char *key, const char *path, unsigned long long line, unsigned long long col,
                                VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long top = 0ull;
    unsigned long long bottom = 0ull;
    branch_lines_about(key, line, CLI_BRANCH_SIGNATURE_LINES - 1ull, 0ull, &top, &bottom);
    VerbaLinea call;
    if (!cli_growing_line_made(&call, CLI_BRANCH_CALL_FIRST, &g_cli_window_cells))
    {
        mmgr_line_put(err_msg, "the signature's call could not be given a cell");
        return 0;
    }
    char state[CLI_BRANCH_STATE];
    mmgr_line_put(&call, "{\"path\":");
    cli_window_quoted(&call, path);
    mmgr_line_put(&call, ",\"text\":");
    const int stored = cli_document_text_written(key, top, line, &call, state, sizeof(state));
    mmgr_line_put(&call, ",\"state\":");
    cli_window_quoted(&call, (state[0] != '\0') ? state : "root");
    mmgr_line_put(&call, ",\"line\":");
    mmgr_line_decimal(&call, line - top, 1u);
    mmgr_line_put(&call, ",\"col\":");
    mmgr_line_decimal(&call, col, 1u);
    mmgr_line_ch(&call, '}');
    int answered = 0;
    if (!stored)
    {
        mmgr_line_put(err_msg, "no branch is stored under ");
        mmgr_line_put(err_msg, key);
    }
    else if (mmgr_line_ok(&call))
    {
        answered = cli_window_tree_call("symbols_signature", call.out, call.at, out, err_msg);
    }
    cli_cell_release(call.out, &g_cli_window_cells);
    return answered;
}

// the bracket pair at the line `line`, column `col`, of the branch under `key`, the file at `path`: the text's lines
// from as far above and below as CLI_BRANCH_PAIR_LINES reach, a paged text's those the page holds, read as the tree
// reads a text from the state the first was entered in, the bracket that ends at the place, else the one that starts at
// it, and the one it pairs with, written as `[{"line", "col"}, {"line", "col"}]`, or null where no bracket of a pair
// stands there
static int branch_pair(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if ((path == NULL) || !cli_serve_whole(args, "line", &line, err_msg) ||
        !cli_serve_whole(args, "col", &col, err_msg))
    {
        return 0;
    }
    unsigned long long top = 0ull;
    unsigned long long bottom = 0ull;
    branch_lines_about(key, line, CLI_BRANCH_PAIR_LINES, CLI_BRANCH_PAIR_LINES, &top, &bottom);
    char state[CLI_BRANCH_STATE];
    unsigned long long size = 0ull;
    char *const text = cli_document_lines_taken(key, top, bottom, &size, state, sizeof(state), &g_cli_window_cells);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "no branch is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    CliSymbolPlace pair[2] = {{0u, 0u}, {0u, 0u}};
    // a place in a text the page holds stands below 2^32 lines and columns
    const int paired = cli_symbols_pair(path, (state[0] != '\0') ? state : "root", text, size,
                                        (CliSymbolPlace){(unsigned int)(line - top), (unsigned int)col}, pair);
    cli_cell_release(text, &g_cli_window_cells);
    if (!paired)
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? ",{\"line\":" : "{\"line\":");
        mmgr_line_decimal(out, top + pair[at].line, 1u);
        mmgr_line_put(out, ",\"col\":");
        mmgr_line_decimal(out, pair[at].col, 1u);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_ch(out, ']');
    return 1;
}

// `size` bytes of `text` the branch under `key`, its first lines the page's mirror and colored in the language `id`,
// written into `out` as `{"key", "names", "colors"}`, or null where it could not be stored
static void branch_text_answered(const char *key, const char *text, unsigned long long size, const char *id,
                                 VerbaLinea *out)
{
    if (!branch_stored(key, text, size, 0ull, CLI_BRANCH_FIRST_LINES - 1ull))
    {
        mmgr_line_put(out, "null");
        return;
    }
    mmgr_line_put(out, "{\"key\":");
    cli_window_quoted(out, key);
    branch_colors_answered(key, id, 0ull, CLI_BRANCH_FIRST_LINES - 1ull, out);
    mmgr_line_ch(out, '}');
}

// the cells of the notebook at `path`, read as notebook_read reads them, each cell's text a branch under `key`, `#` and
// its place among the cells, a code cell colored in the language `code` and a Markdown cell in `markdown`:
// `[{"key", "names", "colors"}]` in the cells' order, null for a cell of another kind
static int branch_cells(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", err_msg) : NULL;
    if (path == NULL)
    {
        return 0;
    }
    const char *const code = cli_serve_string_or(args, "code", "");
    const char *const markdown = cli_serve_string_or(args, "markdown", "");
    cli_document_colors_to(branch_colors_sent, 1);
    CliJson json;
    const unsigned int book = cli_window_tree_called(&json, "notebook_read", path, "", err_msg);
    if (book == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        return 0;
    }
    const unsigned int cells = cli_json_member(&json, book, "cells");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; cli_json_item(&json, cells, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int cell = cli_json_item(&json, cells, at);
        const char *const kind = cli_json_string(&json, cli_json_member(&json, cell, "cell_type"), NULL);
        const int markdown_cell =
            (kind != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "markdown", .cap = CLI_CELLS_POOL});
        const int code_cell =
            (kind == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "code", .cap = CLI_CELLS_POOL});
        unsigned long long length = 0ull;
        const char *const source = cli_json_string(&json, cli_json_member(&json, cell, "source"), &length);
        char cell_key[CLI_NAME_MAX_LEN];
        VerbaLinea named = {cell_key, sizeof(cell_key), 0ull};
        mmgr_line_put(&named, key);
        mmgr_line_ch(&named, '#');
        mmgr_line_decimal(&named, at, 1u);
        const int keyed = mmgr_line_finish(&named) != 0ull;
        mmgr_line_put(out, (at != 0u) ? "," : "");
        if (keyed && (markdown_cell || code_cell))
        {
            branch_text_answered(cell_key, (source != NULL) ? source : "", (source != NULL) ? length : 0ull,
                                 markdown_cell ? markdown : code, out);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
    }
    mmgr_line_ch(out, ']');
    cli_json_given(&json);
    return 1;
}

// a branch of no text under `key`, a cell the reader adds, colored in the language `id`: `{"key", "names", "colors"}`
static int branch_new(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    if (key == NULL)
    {
        return 0;
    }
    cli_document_colors_to(branch_colors_sent, 1);
    branch_text_answered(key, "", 0ull, cli_serve_string_or(args, "id", ""), out);
    return 1;
}

// the colors of the lines from `first` to `last` of the branch under `key`, as a print of them asks, whatever lines
// the page mirrors, written as `[{"from", "runs"}]`
static int branch_runs(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const key = cli_serve_string(args, "key", err_msg);
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "first", &first, err_msg) ||
        !cli_serve_whole(args, "last", &last, err_msg))
    {
        return 0;
    }
    if (!cli_document_runs_written(key, first, last, 0, out))
    {
        mmgr_line_put(err_msg, "no branch is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    return 1;
}

const CliServeCall g_cli_window_branch_calls[] = {
    {"branch_open", branch_open},           {"branch_at", branch_at},
    {"branch_view", branch_view},           {"branch_language", branch_language},
    {"branch_signature", branch_signature}, {"branch_pair", branch_pair},
    {"branch_runs", branch_runs},           {"branch_cells", branch_cells},
    {"branch_new", branch_new},
};
const unsigned int g_cli_window_branch_count = sizeof(g_cli_window_branch_calls) / sizeof(g_cli_window_branch_calls[0]);
