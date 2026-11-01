// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_notebook.c: the page's calls on notebooks: a notebook read, written, written for a diff, merged from a
// merge's stages and written without its outputs, each read into a tree sized to the notebooks it reads
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../notebooks/notebook.h"
#include "../text/text.h"
#include "serve_internal.h"

// the most bytes of a notebook, the most nodes a notebook's tree contains, the bytes past its own a notebook's text is
// written in, and the bytes a stage of a merge is read in
#define CLI_SERVE_NOTEBOOK (32ull << 20u)
#define CLI_SERVE_BOOK_NODES (1u << 19u)
#define CLI_SERVE_BOOK_MORE (64ull << 10u)
#define CLI_SERVE_STAGE_ROOM (64ull << 20u)

// A notebook's room: the tree its notebooks are read into and the tokens each is read with; the nodes and bytes of the
// tree, from what the notebooks contain, the tokens of the largest, and four times the nodes, the bytes and the lines
// of each for what reading it makes of it
typedef struct
{
    CliJson json;
    CfgJsonToken *tokens;
    unsigned int tokens_most;
    unsigned int nodes;
    unsigned long long text;
} CliBookRoom;

// a notebook's text of `size` bytes added to what the room is taken for
static void book_room_added(CliBookRoom *room, const char *text, unsigned long long size)
{
    const unsigned int tokens = cli_json_tokens_counted(text, size) + 1u;
    const unsigned int lines = (unsigned int)cli_text_counted(text, size, '\n') + 1u;
    room->tokens_most = (tokens > room->tokens_most) ? tokens : room->tokens_most;
    const unsigned long long nodes = (unsigned long long)room->nodes + (4ull * tokens) + (4ull * lines) + 64ull;
    room->nodes = (nodes < CLI_SERVE_BOOK_NODES) ? (unsigned int)nodes : CLI_SERVE_BOOK_NODES;
    room->text += (4ull * size) + 4096ull;
}

// the room's tree and tokens taken: 1, or 0 with why given where the cells cannot meet them
static int book_room_taken(CliBookRoom *room, VerbaLinea *err_msg)
{
    room->tokens = g_cli_cells->persistent_buf_alloc((unsigned long long)room->tokens_most * sizeof(CfgJsonToken),
                                                     &g_cli_window_cells);
    if ((room->tokens == NULL) ||
        !cli_json_reserved(&room->json, room->nodes, room->text, "a notebook's tree", &g_cli_window_cells))
    {
        g_cli_cells->persistent_buf_release(room->tokens, &g_cli_window_cells);
        room->tokens = NULL;
        mmgr_line_put(err_msg, "the notebook's cells could not be taken");
        return 0;
    }
    return 1;
}

// the room's tree and tokens given back
static void book_room_given(CliBookRoom *room)
{
    cli_json_given(&room->json);
    g_cli_cells->persistent_buf_release(room->tokens, &g_cli_window_cells);
    room->tokens = NULL;
}

// the file of the argument `path` mapped, its path into `full`: its bytes and size, or NULL with why given where it
// cannot be read, is larger than a notebook is read or is not whole UTF-8
static const char *notebook_file(const CliServeArgs *args, char *full, unsigned long long *size, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    *size = 0ull;
    if (!cli_serve_at(args, root, full, err_msg))
    {
        return NULL;
    }
    const unsigned char *const bytes = cli_file_taken(full, size, &g_cli_window_cells);
    // the file's bytes read as the text they are
    const char *const text = (const char *)bytes;
    if ((bytes == NULL) || (*size > CLI_SERVE_NOTEBOOK) || !cli_utf8_whole(text, *size))
    {
        cli_file_given(bytes, &g_cli_window_cells);
        mmgr_line_put(err_msg, full);
        mmgr_line_put(err_msg, ": it could not be read");
        return NULL;
    }
    return text;
}

static int notebook_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char full[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    const char *const text = notebook_file(args, full, &size, err_msg);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const unsigned int book =
        book_room_taken(&room, err_msg)
            ? cli_notebook_read(&room.json, full, text, size, room.tokens, room.tokens_most, err_msg)
            : CLI_JSON_NONE;
    if (book != CLI_JSON_NONE)
    {
        cli_json_write(&room.json, book, out, 0u, 0);
    }
    book_room_given(&room);
    // the bytes were mapped from the text
    cli_file_given((const unsigned char *)text, &g_cli_window_cells);
    return book != CLI_JSON_NONE;
}

static int notebook_write(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    if ((path == NULL) || !cli_serve_at(args, root, full, err_msg))
    {
        return 0;
    }
    const unsigned long long text_room = cli_json_written_room(args->json) + CLI_SERVE_BOOK_MORE;
    char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_window_cells);
    VerbaLinea text = mmgr_line_over(text_cell, text_room);
    // the arguments' tree is the call's own room, which the writer may add nodes to
    cli_notebook_write((CliJson *)args->json, full, cli_serve_arg(args, "book"), &text);
    const int done = mmgr_line_ok(&text) && cli_serve_file_write(path, text.out, text.at, err_msg);
    g_cli_cells->persistent_buf_release(text.out, &g_cli_window_cells);
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int notebook_diff_text(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    const char *const text = cli_serve_string(args, "text", err_msg);
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &size);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const int done = book_room_taken(&room, err_msg) &&
                     cli_notebook_for_diff(&room.json, text, size, room.tokens, room.tokens_most, out, err_msg);
    book_room_given(&room);
    return done;
}

static int notebook_without_outputs(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char full[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    const char *const text = notebook_file(args, full, &size, err_msg);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const int done = book_room_taken(&room, err_msg) &&
                     cli_notebook_without_outputs(&room.json, text, size, room.tokens, room.tokens_most, out, err_msg);
    book_room_given(&room);
    // the bytes were mapped from the text
    cli_file_given((const unsigned char *)text, &g_cli_window_cells);
    return done;
}

// stage `stage` of a merge of the notebook `given` read into `text`, a line: 1, or 0 with why given
static int notebook_stage(const char *root, const char *given, unsigned int stage, VerbaLinea *text,
                          VerbaLinea *err_msg)
{
    char *const text_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_STAGE_ROOM, &g_cli_window_cells);
    *text = mmgr_line_over(text_cell, CLI_SERVE_STAGE_ROOM);
    if (!cli_git_stage_text(root, given, stage, text) || !mmgr_line_ok(text))
    {
        mmgr_line_put(err_msg, given);
        mmgr_line_put(err_msg, " contains no stage ");
        mmgr_line_decimal(err_msg, stage, 1u);
        mmgr_line_put(err_msg, " of a merge");
        return 0;
    }
    return 1;
}

// the merge of the stages read into the room's tree and written into `out`: 1 where it is, else 0 with why given
static int notebook_merged(CliBookRoom *room, const VerbaLinea *stages, int has_base, VerbaLinea *out,
                           VerbaLinea *err_msg)
{
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int base = has_base ? cli_notebook_read(&room->json, "x.ipynb", stages[0].out, stages[0].at, room->tokens,
                                                     room->tokens_most, &ignored)
                                 : CLI_JSON_NONE;
    if (base == CLI_JSON_NONE)
    {
        base = cli_json_made(&room->json, CFG_JSON_OBJECT);
        cli_json_set(&room->json, base, "cells", cli_json_made(&room->json, CFG_JSON_ARRAY));
    }
    const unsigned int ours = cli_notebook_read(&room->json, "x.ipynb", stages[1].out, stages[1].at, room->tokens,
                                                room->tokens_most, err_msg);
    const unsigned int theirs = (ours != CLI_JSON_NONE)
                                    ? cli_notebook_read(&room->json, "x.ipynb", stages[2].out, stages[2].at,
                                                        room->tokens, room->tokens_most, err_msg)
                                    : CLI_JSON_NONE;
    if (theirs != CLI_JSON_NONE)
    {
        cli_json_write(&room->json, cli_notebook_merge(&room->json, base, ours, theirs), out, 0u, 0);
    }
    return theirs != CLI_JSON_NONE;
}

static int notebook_merge(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const given = cli_serve_string(args, "path", err_msg);
    if ((given == NULL) || !cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    VerbaLinea stages[3] = {{NULL, 0ull, 0ull, NULL, NULL, 0ull},
                            {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                            {NULL, 0ull, 0ull, NULL, NULL, 0ull}};
    const int has_base = notebook_stage(root, given, 1u, &stages[0], &ignored);
    int read = notebook_stage(root, given, 2u, &stages[1], err_msg);
    read = read && notebook_stage(root, given, 3u, &stages[2], err_msg);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    // the merge contains the three sides and copies of their cells, a fourth reading
    for (unsigned int at = 0u; read && (at < 4u); at += 1u)
    {
        const unsigned int side = (at < 3u) ? at : 1u;
        book_room_added(&room, stages[side].out, mmgr_line_ok(&stages[side]) ? stages[side].at : 0ull);
    }
    const int done = read && book_room_taken(&room, err_msg) && notebook_merged(&room, stages, has_base, out, err_msg);
    book_room_given(&room);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        g_cli_cells->persistent_buf_release(stages[at].out, &g_cli_window_cells);
    }
    return done;
}

const CliServeCall g_cli_serve_notebook_calls[] = {
    {"notebook_read", notebook_read},
    {"notebook_write", notebook_write},
    {"notebook_diff_text", notebook_diff_text},
    {"notebook_merge", notebook_merge},
    {"notebook_without_outputs", notebook_without_outputs},
};
const unsigned int g_cli_serve_notebook_count =
    sizeof(g_cli_serve_notebook_calls) / sizeof(g_cli_serve_notebook_calls[0]);
