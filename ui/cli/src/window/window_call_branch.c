// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_branch.c: a branch of the tree opened in the editor: the file read as the tree reads it, a text read whole
// held under the page's key, and one too large to read whole opened in the pages under it, colored by the grammar its
// extension points to, the lines in view colored at once and the rest walked out from them, each line colored after the
// answer handed to the page as `branch-colors`; the view moved, and the lines of a paged text the page holds; the
// language a branch is colored in chosen, its lines colored again and handed to the page; the tree's signature for a
// call in a branch, read off the branch's own lines; and the colors of a branch's lines as a print of them asks
#include "../command_line/file_writing.h"
#include "../editor/document.h"
#include "../editor/highlight.h"
#include "../editor/pages.h"
#include "../plugins/plugins.h"
#include "window_internal.h"

// the most lines above a call's line its signature is read from, the room the tree's call is begun in, and a state's
// longest text
#define CLI_BRANCH_SIGNATURE_LINES 4000ull
#define CLI_BRANCH_CALL_FIRST 4096u
#define CLI_BRANCH_STATE 1024u

// a held text's colors handed to the page as they are worked out
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

// the text held under `key` colored in the language `id`, empty for none: the names its grammar's tokens take written
// into `out`, `[""]` where it has no grammar, where `out` is not NULL, else handed on with its first colors
static void branch_colored_in(const char *key, const char *id, VerbaLinea *out)
{
    char grammar[CLI_NAME_MAX_LEN];
    branch_grammar_named(id, grammar);
    (void)cli_document_colored_by(key, grammar, out != NULL);
    if ((out != NULL) && ((grammar[0] == '\0') || !cli_highlight_names_written(grammar, out)))
    {
        mmgr_line_put(out, "[\"\"]");
    }
}

// the file at `path` read as file_read reads it, a text read whole held under `key` and one file_read windows opened in
// the pages under `key`, either one's lines from `first` to `last` held by the page and the text colored in the language
// its extension points to, and the lines from `first` to `last` colored: file_read's answer with the paged text's shape,
// `shape`, null where the pages could not open it, the names its grammar's tokens take, `names`, and those lines'
// colors, `colors`, where it is held
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
    CliShape shape;
    // a text read whole held with its lines in view the only ones the page holds, until it names more
    const int held = (text != NULL) && cli_document_store(key, text, length, &shape) &&
                     cli_document_held(key, first, last + 1ull);
    const unsigned long long start = out->at;
    cli_json_write(&json, opened, out, 0u, 0);
    cli_json_given(&json);
    // the answer's closing brace, put back after the branch's names and colors
    out->at -= (out->at > start) ? 1ull : 0ull;
    // a text too large to read whole opened in the pages under the key, its shape answered, and held as a branch whose
    // lines in view the page holds
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
    if (held || paged)
    {
        CliLanguages languages;
        cli_plugins_languages(&languages);
        char id[CLI_NAME_MAX_LEN] = "";
        if (!cli_plugins_language_for(&languages, path, id))
        {
            id[0] = '\0';
        }
        cli_plugins_languages_given(&languages);
        mmgr_line_put(out, ",\"names\":");
        branch_colored_in(key, id, out);
        mmgr_line_put(out, ",\"colors\":");
        (void)cli_document_viewed(key, first, last, out);
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// the view of the text held under `key` moved to the lines from `first` to `last`, those not colored yet colored and
// handed to the page, and the lines the page holds about the view, from `from` to before `to`, where it names them
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

// the text held under `key` colored in the language `id` from here on, empty for none, its lines colored again from the
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

// the tree's signature for the call at the line `line`, column `col`, of the text held under `key`, the file at
// `path`: the text's lines from as far above as CLI_BRANCH_SIGNATURE_LINES reach to the line, read as the tree reads a
// text from the state the first was entered in, the tree's answer as `symbols_signature` gives it
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
    const unsigned long long top = (line >= CLI_BRANCH_SIGNATURE_LINES) ? (line - CLI_BRANCH_SIGNATURE_LINES + 1ull) : 0ull;
    VerbaLinea call;
    if (!cli_growing_line_made(&call, CLI_BRANCH_CALL_FIRST))
    {
        mmgr_line_put(err_msg, "the signature's call could not be given a cell");
        return 0;
    }
    char state[CLI_BRANCH_STATE];
    mmgr_line_put(&call, "{\"path\":");
    cli_window_quoted(&call, path);
    mmgr_line_put(&call, ",\"text\":");
    const int held = cli_document_text_written(key, top, line, &call, state, sizeof(state));
    mmgr_line_put(&call, ",\"state\":");
    cli_window_quoted(&call, (state[0] != '\0') ? state : "root");
    mmgr_line_put(&call, ",\"line\":");
    mmgr_line_decimal(&call, line - top, 1u);
    mmgr_line_put(&call, ",\"col\":");
    mmgr_line_decimal(&call, col, 1u);
    mmgr_line_ch(&call, '}');
    int answered = 0;
    if (!held)
    {
        mmgr_line_put(err_msg, "no text is held under ");
        mmgr_line_put(err_msg, key);
    }
    else if (mmgr_line_ok(&call))
    {
        answered = cli_window_tree_call("symbols_signature", call.out, call.at, out, err_msg);
    }
    g_cli_cells->persistent_buf_release(call.out, NULL);
    return answered;
}

// the colors of the lines from `first` to `last` of the text held under `key`, as a print of them asks, whatever lines
// the page holds, written as `[{"from", "runs"}]`
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
    if (!cli_document_runs_written(key, first, last, out))
    {
        mmgr_line_put(err_msg, "no text is held under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    return 1;
}

const CliServeCall g_cli_window_branch_calls[] = {
    {"branch_open", branch_open},
    {"branch_view", branch_view},
    {"branch_language", branch_language},
    {"branch_signature", branch_signature},
    {"branch_runs", branch_runs},
};
const unsigned int g_cli_window_branch_count = sizeof(g_cli_window_branch_calls) / sizeof(g_cli_window_branch_calls[0]);
