// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_editor.c: the window's calls on the editor's view of a text stored as a branch opened it: the view opened
// on the text under a key, sized to the window's rows and columns, each input the window sends taken, the minimap's
// lines asked for, and the view closed; an open and every input answered with the view's frame, which the window
// draws
#include "../editor/view.h"
#include "window_internal.h"

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
    return cli_view_input(key, &input, err_msg) && cli_view_frame_written(key, out);
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
    cli_view_closed(key);
    mmgr_line_put(out, "null");
    return 1;
}

const CliServeCall g_cli_window_editor_calls[] = {
    {"editor_view_open", editor_view_open},
    {"editor_view_input", editor_view_input},
    {"editor_view_map", editor_view_map},
    {"editor_view_close", editor_view_close},
};
const unsigned int g_cli_window_editor_count = sizeof(g_cli_window_editor_calls) / sizeof(g_cli_window_editor_calls[0]);
