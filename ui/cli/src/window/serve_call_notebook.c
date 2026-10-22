// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_notebook.c: the page's calls on notebooks: a notebook read, written, written for a diff, merged from a
// merge's stages and written without its outputs, each read into a tree sized to the notebooks it reads
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../notebooks/notebook.h"
#include "serve_internal.h"

// the most bytes of a notebook, the most nodes a notebook's tree holds, and the bytes a notebook's text is begun to be
// written in, doubling as it is
#define CLI_SERVE_NOTEBOOK (32ull << 20u)
#define CLI_SERVE_BOOK_NODES (1u << 19u)
#define CLI_SERVE_BOOK_FIRST 4096u

// A notebook's room: the tree its notebooks are read into and the tokens each is read with; the nodes and bytes of the
// tree, from what the notebooks hold, the tokens of the largest, and twice the nodes, the bytes and the lines of each
// for what reading it makes of it, doubled where a read finds the tree full
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
    unsigned int lines = 1u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        lines += (text[at] == '\n') ? 1u : 0u;
    }
    room->tokens_most = (tokens > room->tokens_most) ? tokens : room->tokens_most;
    const unsigned long long nodes = (unsigned long long)room->nodes + (2ull * tokens) + (2ull * lines) + 64ull;
    room->nodes = (nodes < CLI_SERVE_BOOK_NODES) ? (unsigned int)nodes : CLI_SERVE_BOOK_NODES;
    room->text += (2ull * size) + 4096ull;
}

// the room's tree and tokens taken: 1, or 0 with why said where the cells cannot meet them
static int book_room_taken(CliBookRoom *room, CliLine *said)
{
    room->tokens = cli_cell_taken((unsigned long long)room->tokens_most * sizeof(CfgJsonToken), "a notebook's tokens");
    if ((room->tokens == NULL) || !cli_json_held(&room->json, room->nodes, room->text, "a notebook's tree"))
    {
        cli_cell_given(room->tokens);
        room->tokens = NULL;
        cli_line_text(said, "the notebook's cells could not be taken");
        return 0;
    }
    return 1;
}

// the room's tree and tokens given back
static void book_room_given(CliBookRoom *room)
{
    cli_json_given(&room->json);
    cli_cell_given(room->tokens);
    room->tokens = NULL;
}

// after a read, the room given back and made twice as large where the read found its tree full and it is below its
// most, `out` and `said` put back as they were before the read: 1 where the read is to be made again, else 0
static int book_room_grown(CliBookRoom *room, CliLine *out, unsigned long long out_at, CliLine *said,
                           unsigned long long said_at)
{
    if (!room->json.full || (room->nodes >= CLI_SERVE_BOOK_NODES))
    {
        return 0;
    }
    book_room_given(room);
    room->nodes = ((room->nodes * 2ull) < CLI_SERVE_BOOK_NODES) ? (room->nodes * 2u) : CLI_SERVE_BOOK_NODES;
    room->text *= 2ull;
    out->at = out_at;
    said->at = said_at;
    return 1;
}

// the file of the argument `path` mapped, its path into `full`: its bytes and size, or NULL with why said where it
// cannot be read, is larger than a notebook is read or is not whole UTF-8
static const char *notebook_file(const CliServeArgs *args, char *full, unsigned long long *size, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    *size = 0ull;
    if (!cli_serve_at(args, root, full, said))
    {
        return NULL;
    }
    const unsigned char *const bytes = cli_file_held(full, size);
    // the file's bytes read as the text they are
    const char *const text = (const char *)bytes;
    if ((bytes == NULL) || (*size > CLI_SERVE_NOTEBOOK) || !cli_utf8_whole(text, *size))
    {
        cli_file_let_go(bytes, *size);
        cli_line_text(said, full);
        cli_line_text(said, ": it could not be read");
        return NULL;
    }
    return text;
}

static int notebook_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const char *const text = notebook_file(args, full, &size, said);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const unsigned long long said_at = said->at;
    unsigned int book = CLI_JSON_NONE;
    int taken = 0;
    do
    {
        taken = book_room_taken(&room, said);
        book = taken ? cli_notebook_read(&room.json, full, text, size, room.tokens, room.tokens_most, said)
                     : CLI_JSON_NONE;
    } while (taken && book_room_grown(&room, out, out->at, said, said_at));
    if (book != CLI_JSON_NONE)
    {
        cli_json_write(&room.json, book, out, 0u, 0);
    }
    book_room_given(&room);
    // the bytes were mapped from the text
    cli_file_let_go((const unsigned char *)text, size);
    return book != CLI_JSON_NONE;
}

static int notebook_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_at(args, root, full, said))
    {
        return 0;
    }
    CliLine text;
    (void)cli_line_growing(&text, CLI_SERVE_BOOK_FIRST, "a notebook's text written");
    // the arguments' tree is the call's own room, which the writer may add nodes to
    cli_notebook_write((CliJson *)args->json, full, cli_serve_arg(args, "book"), &text);
    const int done = cli_line_fits(&text) && cli_serve_file_write(path, text.out, text.at, said);
    cli_cell_given(text.out);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int notebook_diff_text(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    const char *const text = cli_serve_string(args, "text", said);
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &size);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const unsigned long long out_at = out->at;
    const unsigned long long said_at = said->at;
    int done = 0;
    int taken = 0;
    do
    {
        taken = book_room_taken(&room, said);
        done = taken && cli_notebook_for_diff(&room.json, text, size, room.tokens, room.tokens_most, out, said);
    } while (taken && book_room_grown(&room, out, out_at, said, said_at));
    book_room_given(&room);
    return done;
}

static int notebook_without_outputs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const char *const text = notebook_file(args, full, &size, said);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    book_room_added(&room, text, size);
    const unsigned long long out_at = out->at;
    const unsigned long long said_at = said->at;
    int done = 0;
    int taken = 0;
    do
    {
        taken = book_room_taken(&room, said);
        done = taken && cli_notebook_without_outputs(&room.json, text, size, room.tokens, room.tokens_most, out, said);
    } while (taken && book_room_grown(&room, out, out_at, said, said_at));
    book_room_given(&room);
    // the bytes were mapped from the text
    cli_file_let_go((const unsigned char *)text, size);
    return done;
}

// stage `stage` of a merge of the notebook `given` read into `text`, a growing line: 1, or 0 with why said
static int notebook_stage(const char *root, const char *given, unsigned int stage, CliLine *text, CliLine *said)
{
    (void)cli_line_growing(text, CLI_SERVE_BOOK_FIRST, "a merge's stage");
    if (!cli_git_stage_text(root, given, stage, text) || !cli_line_fits(text))
    {
        cli_line_text(said, given);
        cli_line_text(said, " holds no stage ");
        cli_line_decimal(said, stage, 1u);
        cli_line_text(said, " of a merge");
        return 0;
    }
    return 1;
}

// the merge of the stages read into the room's tree and written into `out`: 1 where it is, else 0 with why said
static int notebook_merged(CliBookRoom *room, const CliLine *stages, int has_base, CliLine *out, CliLine *said)
{
    char quiet[1024];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int base = has_base ? cli_notebook_read(&room->json, "x.ipynb", stages[0].out, stages[0].at, room->tokens,
                                                     room->tokens_most, &ignored)
                                 : CLI_JSON_NONE;
    if (base == CLI_JSON_NONE)
    {
        base = cli_json_made(&room->json, CFG_JSON_OBJECT);
        cli_json_set(&room->json, base, "cells", cli_json_made(&room->json, CFG_JSON_ARRAY));
    }
    const unsigned int ours =
        cli_notebook_read(&room->json, "x.ipynb", stages[1].out, stages[1].at, room->tokens, room->tokens_most, said);
    const unsigned int theirs = (ours != CLI_JSON_NONE)
                                    ? cli_notebook_read(&room->json, "x.ipynb", stages[2].out, stages[2].at,
                                                        room->tokens, room->tokens_most, said)
                                    : CLI_JSON_NONE;
    if (theirs != CLI_JSON_NONE)
    {
        cli_json_write(&room->json, cli_notebook_merge(&room->json, base, ours, theirs), out, 0u, 0);
    }
    return theirs != CLI_JSON_NONE;
}

static int notebook_merge(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const given = cli_serve_string(args, "path", said);
    if ((given == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    char quiet[1024];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    CliLine stages[3] = {{NULL, 0ull, 0ull, NULL, NULL, 0ull},
                         {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                         {NULL, 0ull, 0ull, NULL, NULL, 0ull}};
    const int has_base = notebook_stage(root, given, 1u, &stages[0], &ignored);
    int read = notebook_stage(root, given, 2u, &stages[1], said);
    read = read && notebook_stage(root, given, 3u, &stages[2], said);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    // the merge holds the three sides and copies of their cells, a fourth reading
    for (unsigned int at = 0u; read && (at < 4u); at += 1u)
    {
        const unsigned int side = (at < 3u) ? at : 1u;
        book_room_added(&room, stages[side].out, cli_line_fits(&stages[side]) ? stages[side].at : 0ull);
    }
    const unsigned long long out_at = out->at;
    const unsigned long long said_at = said->at;
    int done = 0;
    int taken = 0;
    while (read)
    {
        taken = book_room_taken(&room, said);
        done = taken && notebook_merged(&room, stages, has_base, out, said);
        if (!taken || !book_room_grown(&room, out, out_at, said, said_at))
        {
            break;
        }
    }
    book_room_given(&room);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_cell_given(stages[at].out);
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
