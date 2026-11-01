// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_bring.c: files and folders copied into the tree as a paste copies them, each file reported as it is copied, and
// copied into a tree on another machine through calls of the tree's side there, a part at a time
#include "../command_line/cells.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "files_internal.h"

// how often a file brought into the tree reports how far along its copy is, in milliseconds; and how much of a file
// goes into one write here, or over the link in one part
#define CLI_BRING_PROGRESS_EVERY 100ull
#define CLI_PART_BYTES (1ull << 20u)

// the bytes a call's arguments and an answer of the tree's side are written in
#define CLI_OVER_ARGUMENTS_ROOM (64ull << 20u)
#define CLI_OVER_ANSWER_ROOM (64ull << 20u)

// What a bring copies: the tree, the path it was requested, which a failure names, and who is notified how far it is
typedef struct
{
    const char *root;
    const char *source;
    CliProgressFn progress;
    void *context;
} CliBringing;

// a folder made, or a file copied a piece at a time, reported when it starts, every CLI_BRING_PROGRESS_EVERY and once
// it is whole
static int cli_brought(void *context, const char *source, const char *target, int folder, VerbaLinea *err_msg)
{
    const CliBringing *const bringing = (const CliBringing *)context;
    if (folder)
    {
        if (query_folder_made(target))
        {
            return 1;
        }
        mmgr_line_put(err_msg, bringing->source);
        mmgr_line_put(err_msg, ": it could not be copied");
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    cli_relative(bringing->root, target, path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(source, &size, &g_cli_editor_cells);
    int written = bytes != NULL;
    if (written)
    {
        bringing->progress(bringing->context, path, 0ull, size);
    }
    unsigned long long done = 0ull;
    unsigned long long last = cli_clock_milliseconds();
    int fresh = 1;
    while (written && (fresh || (done < size)))
    {
        const unsigned long long piece = ((size - done) < CLI_PART_BYTES) ? (size - done) : CLI_PART_BYTES;
        written = cli_file_appended(target, bytes + done, piece, fresh);
        fresh = 0;
        done += piece;
        if (written && ((cli_clock_milliseconds() - last) >= CLI_BRING_PROGRESS_EVERY))
        {
            bringing->progress(bringing->context, path, done, size);
            last = cli_clock_milliseconds();
        }
    }
    cli_file_given(bytes, &g_cli_editor_cells);
    if (!written)
    {
        mmgr_line_put(err_msg, bringing->source);
        mmgr_line_put(err_msg, ": it could not be copied");
        return 0;
    }
    bringing->progress(bringing->context, path, size, size);
    return 1;
}

int cli_files_bring(const char *root, const char *into, const char *const *from, unsigned int count,
                    CliProgressFn progress, void *context, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_paste_begun(root, into, folder, out, err_msg))
    {
        return 0;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_MAX_LEN];
        if (!cli_paste_named(source, name, err_msg))
        {
            return 0;
        }
        if (query_is_folder(source) && cli_paste_under(source, folder))
        {
            mmgr_line_put(err_msg, source);
            mmgr_line_put(err_msg, " is not copied into itself");
            return 0;
        }
        char target[CLI_PATH_MAX_LEN];
        cli_paste_free(folder, name, target);
        CliBringing bringing = {root, source, progress, context};
        if (!cli_paste_walked(source, target, cli_brought, &bringing, err_msg))
        {
            return 0;
        }
        char was[CLI_PATH_MAX_LEN];
        const int inside = cli_paste_under(root, source);
        if (inside)
        {
            cli_relative(root, source, was);
        }
        char to[CLI_PATH_MAX_LEN];
        cli_relative(root, target, to);
        cli_pasted_written(out, index == 0u, inside ? was : NULL, to);
    }
    mmgr_line_ch(out, ']');
    cli_files_forget_seen();
    return 1;
}

// What a bring over a link copies: the call of the tree's side, who is notified how far it is, and a call's arguments
// and its answer, each a line given back once the bring ends
typedef struct
{
    CliCall call;
    void *call_context;
    CliProgressFn progress;
    void *context;
    VerbaLinea arguments;
    VerbaLinea answer;
} CliOver;

// a call of the tree's side with the arguments written into the bring's arguments, its answer into the bring's
// answer: 1, or 0 with what it reported
static int cli_over_called(CliOver *over, const char *name, VerbaLinea *err_msg)
{
    over->answer.at = 0ull;
    if (!mmgr_line_ok(&over->arguments))
    {
        mmgr_line_put(err_msg, "the cells cannot meet the call's arguments");
        return 0;
    }
    return over->call(over->call_context, name, over->arguments.out, &over->answer, err_msg);
}

// a folder made there, or a file read here and written there a part at a time
static int cli_over_each(void *context, const char *source, const char *target, int folder, VerbaLinea *err_msg)
{
    CliOver *const over = context;
    VerbaLinea *const arguments = &over->arguments;
    arguments->at = 0ull;
    if (folder)
    {
        mmgr_line_put(arguments, "{\"path\":");
        cli_json_quoted(arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(arguments, '}');
        mmgr_line_finish(arguments);
        return cli_over_called(over, "folder_create", err_msg);
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(source, &size, &g_cli_editor_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, "it could not be read");
        return 0;
    }
    over->progress(over->context, target, 0ull, size);
    unsigned long long at = 0ull;
    int sent = 1;
    int last = 0;
    while (sent && !last)
    {
        const unsigned long long read = ((size - at) < CLI_PART_BYTES) ? (size - at) : CLI_PART_BYTES;
        last = ((at + read) >= size) || (read < CLI_PART_BYTES);
        arguments->at = 0ull;
        mmgr_line_put(arguments, "{\"path\":");
        cli_json_quoted(arguments, target, cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(arguments, ",\"at\":");
        mmgr_line_decimal(arguments, at, 0u);
        mmgr_line_put(arguments, ",\"data\":\"");
        cli_base64(arguments, bytes + at, read);
        mmgr_line_put(arguments, last ? "\",\"last\":true}" : "\",\"last\":false}");
        mmgr_line_finish(arguments);
        sent = cli_over_called(over, "file_part", err_msg);
        at += read;
        if (sent)
        {
            over->progress(over->context, target, at, (size > at) ? size : at);
        }
    }
    cli_file_given(bytes, &g_cli_editor_cells);
    return sent;
}

// the tree's side queried for the path of `name` in `into`, numbered where it contains it already, into `to`: 1, or 0
// with why given
static int cli_over_named(CliOver *over, const char *into, const char *name, char *to, VerbaLinea *err_msg)
{
    VerbaLinea *const arguments = &over->arguments;
    arguments->at = 0ull;
    mmgr_line_put(arguments, "{\"into\":");
    cli_json_quoted(arguments, into, cellul.len(&(CatenaFinitaCfg){.src = into, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(arguments, ",\"name\":");
    cli_json_quoted(arguments, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(arguments, '}');
    mmgr_line_finish(arguments);
    if (!cli_over_called(over, "bring_name", err_msg))
    {
        return 0;
    }
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root = mmgr_line_ok(&over->answer) ? cli_json_taken(&json, over->answer.out, over->answer.at, 0u,
                                                                           0ull, &unread, &g_cli_editor_cells)
                                                          : CLI_JSON_NONE;
    unsigned long long length = 0ull;
    const char *const named = (root != CLI_JSON_NONE) ? cli_json_string(&json, root, &length) : NULL;
    const int fits = (named != NULL) && (length < CLI_PATH_MAX_LEN);
    if (fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = to, .src = named, .bytes = length});
        to[length] = '\0';
    }
    cli_json_given(&json);
    if (!fits)
    {
        mmgr_line_put(err_msg, "the tree's side gave no name");
    }
    return fits;
}

// each path of `from` brought over into `into`, the names they took written into `out`: 1, or 0 with why given
static int cli_over_brought(CliOver *over, const char *into, const char *const *from, unsigned int count,
                            VerbaLinea *out, VerbaLinea *err_msg)
{
    mmgr_line_ch(out, '[');
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        if (!cli_paste_name(source, name))
        {
            mmgr_line_put(err_msg, source);
            mmgr_line_put(err_msg, " has no name");
            return 0;
        }
        if (!cli_over_named(over, into, name, to, err_msg))
        {
            return 0;
        }
        char why[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea failed = {why, sizeof(why), 0ull};
        if (!cli_paste_walked(source, to, cli_over_each, over, &failed))
        {
            mmgr_line_put(err_msg, source);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, failed.at);
            return 0;
        }
        cli_pasted_written(out, index == 0u, NULL, to);
    }
    mmgr_line_ch(out, ']');
    return 1;
}

int cli_files_bring_over(CliCall call, void *call_context, const char *into, const char *const *from,
                         unsigned int count, CliProgressFn progress, void *context, VerbaLinea *out,
                         VerbaLinea *err_msg)
{
    char *const arguments_cell = g_cli_cells->temporary_buf_alloc(CLI_OVER_ARGUMENTS_ROOM, &g_cli_editor_cells);
    char *const answer_cell = g_cli_cells->temporary_buf_alloc(CLI_OVER_ANSWER_ROOM, &g_cli_editor_cells);
    CliOver over = {call,
                    call_context,
                    progress,
                    context,
                    mmgr_line_over(arguments_cell, CLI_OVER_ARGUMENTS_ROOM),
                    mmgr_line_over(answer_cell, CLI_OVER_ANSWER_ROOM)};
    const int brought = cli_over_brought(&over, into, from, count, out, err_msg);
    g_cli_cells->persistent_buf_release(over.arguments.out, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(over.answer.out, &g_cli_editor_cells);
    return brought;
}
