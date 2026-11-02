// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_bring.c: files and folders copied into the tree as a paste copies them, each file reported as it is copied, and
// copied into a tree on another machine through calls of the tree's side there, a part at a time
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "files_internal.h"

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
static int cli_brought(const CliPasteEachCfg *args)
{
    int folder = args->folder;
    const CliBringing *const bringing = (const CliBringing *)args->context;
    if (folder)
    {
        if (EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->target))
        {
            return 1;
        }
        mmgr_line_put(args->err_msg, bringing->source);
        mmgr_line_put(args->err_msg, ": it could not be copied");
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = bringing->root, .path = args->target,
               .relative_out = path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->source,
                                                  .size_pointer = &size, .owner = &g_cli_editor_cells);
    int written = bytes != NULL;
    if (written)
    {
        EMBED_CALL(bringing->progress, CliProgressCfg, .context = bringing->context, .path = path, .done = 0ull,
                   .size = size);
    }
    unsigned long long done = 0ull;
    unsigned long long last = cli_clock_milliseconds();
    int fresh = 1;
    while (written && (fresh || (done < size)))
    {
        const unsigned long long piece = ((size - done) < CLI_PART_BYTES) ? (size - done) : CLI_PART_BYTES;
        written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->target,
                             .bytes = bytes + done, .size = piece, .fresh = fresh);
        fresh = 0;
        done += piece;
        if (written && ((cli_clock_milliseconds() - last) >= CLI_BRING_PROGRESS_EVERY))
        {
            EMBED_CALL(bringing->progress, CliProgressCfg, .context = bringing->context, .path = path, .done = done,
                       .size = size);
            last = cli_clock_milliseconds();
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_editor_cells);
    if (!written)
    {
        mmgr_line_put(args->err_msg, bringing->source);
        mmgr_line_put(args->err_msg, ": it could not be copied");
        return 0;
    }
    EMBED_CALL(bringing->progress, CliProgressCfg, .context = bringing->context, .path = path, .done = size,
               .size = size);
    return 1;
}

int cli_files_bring(const CliFilesRootCfg *args)
{
    const char *root = args->root;
    unsigned int count = args->count;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_files_root_out_internal.paste_begin, CliFilesRootOutCtx, .root = root, .into = args->into,
                    .folder_out = folder, .out = args->out, .err_msg = args->err_msg))
    {
        return 0;
    }
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const source = args->from[index];
        char name[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_files_err_msg_internal.paste_by_name, CliFilesErrMsgCtx, .source = source, .name = name,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = source) &&
            EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = source,
                       .path = folder))
        {
            mmgr_line_put(args->err_msg, source);
            mmgr_line_put(args->err_msg, " is not copied into itself");
            return 0;
        }
        char target[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_files_folder_internal.paste_free, CliFilesFolderCtx, .folder = folder, .name_read = name,
                   .path_out = target);
        CliBringing bringing = {root, source, args->progress, args->context};
        if (!EMBED_CALL(cli_files_context_err_msg_internal.paste_walk, CliFilesContextErrMsgCtx, .source = source,
                        .target = target, .each = cli_brought, .context = &bringing, .err_msg = args->err_msg))
        {
            return 0;
        }
        char was[CLI_PATH_MAX_LEN];
        const int inside = EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = root,
                                      .path = source);
        if (inside)
        {
            EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = source,
                       .relative_out = was);
        }
        char to[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = target, .relative_out = to);
        EMBED_CALL(cli_files_out_internal.pasted_write, CliFilesOutCtx, .out = args->out, .first = index == 0u,
                   .from = inside ? was : NULL, .to = to);
        // what the paste left at each end mirrored to its closest parent, the rest of the tree as it stands
        const char *const moved[2] = {to, was};
        EMBED_CALL(cli_symbols_root.tree_mirror, CliSymbolsRootCfg, .root = root, .paths = moved,
                   .count = inside ? 2u : 1u);
    }
    mmgr_line_ch(args->out, ']');
    return 1;
}

typedef struct
{
    CliOver *over;
    const char *name;
    VerbaLinea *err_msg;
    const char *into;
    char *to;
    const char *const *from;
    unsigned int count;
    VerbaLinea *out;
} FilesBringCtx;
// a call of the tree's side with the arguments written into the bring's arguments, its answer into the bring's
// answer: 1, or 0 with what it reported
EMBED_INLINE int cli_over_call(const FilesBringCtx *args)
{
    args->over->answer.at = 0ull;
    if (!mmgr_line_ok(&args->over->arguments))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the call's arguments");
        return 0;
    }
    return EMBED_CALL(args->over->call, CliCallCfg, .context = args->over->call_context, .name = args->name,
                      .arguments = args->over->arguments.out, .out = &args->over->answer, .err_msg = args->err_msg);
}

// a folder made there, or a file read here and written there a part at a time
static int cli_over_each(const CliPasteEachCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliOver *const over = args->context;
    VerbaLinea *const arguments = &over->arguments;
    arguments->at = 0ull;
    if (args->folder)
    {
        mmgr_line_put(arguments, "{\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = arguments,
                   .text = args->target,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->target, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(arguments, '}');
        mmgr_line_finish(arguments);
        return EMBED_CALL(cli_over_call, FilesBringCtx, .over = over, .name = "folder_create", .err_msg = err_msg);
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->source,
                                                  .size_pointer = &size, .owner = &g_cli_editor_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, "it could not be read");
        return 0;
    }
    EMBED_CALL(over->progress, CliProgressCfg, .context = over->context, .path = args->target, .done = 0ull,
               .size = size);
    unsigned long long at = 0ull;
    int sent = 1;
    int last = 0;
    while (sent && !last)
    {
        const unsigned long long read = ((size - at) < CLI_PART_BYTES) ? (size - at) : CLI_PART_BYTES;
        last = ((at + read) >= size) || (read < CLI_PART_BYTES);
        arguments->at = 0ull;
        mmgr_line_put(arguments, "{\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = arguments,
                   .text = args->target,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->target, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(arguments, ",\"at\":");
        mmgr_line_decimal(arguments, at, 0u);
        mmgr_line_put(arguments, ",\"data\":\"");
        EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = arguments, .bytes = bytes + at, .size = read);
        mmgr_line_put(arguments, last ? "\",\"last\":true}" : "\",\"last\":false}");
        mmgr_line_finish(arguments);
        sent = EMBED_CALL(cli_over_call, FilesBringCtx, .over = over, .name = "file_part", .err_msg = err_msg);
        at += read;
        if (sent)
        {
            EMBED_CALL(over->progress, CliProgressCfg, .context = over->context, .path = args->target, .done = at,
                       .size = (size > at) ? size : at);
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_editor_cells);
    return sent;
}

// the tree's side queried for the path of `name` in `into`, numbered where it contains it already, into `to`: 1, or 0
// with why given
EMBED_INLINE int cli_over_by_name(const FilesBringCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    VerbaLinea *const arguments = &args->over->arguments;
    arguments->at = 0ull;
    mmgr_line_put(arguments, "{\"into\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = arguments, .text = args->into,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->into, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(arguments, ",\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = arguments, .text = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(arguments, '}');
    mmgr_line_finish(arguments);
    if (!EMBED_CALL(cli_over_call, FilesBringCtx, .over = args->over, .name = "bring_name", .err_msg = err_msg))
    {
        return 0;
    }
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        mmgr_line_ok(&args->over->answer)
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                         .text = args->over->answer.out, .size = args->over->answer.at, .nodes_more = 0u,
                         .text_more = 0ull, .err_msg = &unread, .owner = &g_cli_editor_cells)
            : CLI_JSON_NONE;
    unsigned long long length = 0ull;
    const char *const named = (root != CLI_JSON_NONE)
                                  ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = &json, .node = root, .length_pointer = &length)
                                  : NULL;
    const int fits = (named != NULL) && (length < CLI_PATH_MAX_LEN);
    if (fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->to, .src = named, .bytes = length});
        args->to[length] = '\0';
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (!fits)
    {
        mmgr_line_put(err_msg, "the tree's side gave no name");
    }
    return fits;
}

// each path of `from` brought over into `into`, the names they took written into `out`: 1, or 0 with why given
EMBED_INLINE int cli_over_brought(const FilesBringCtx *args)
{
    const char *const *from = args->from;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    mmgr_line_ch(out, '[');
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        const char *const source = from[index];
        char name[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_files_path_name_internal.paste_name, CliFilesPathNameCtx, .path = source, .name = name))
        {
            mmgr_line_put(err_msg, source);
            mmgr_line_put(err_msg, " has no name");
            return 0;
        }
        if (!EMBED_CALL(cli_over_by_name, FilesBringCtx, .over = args->over, .into = args->into, .name = name, .to = to,
                        .err_msg = err_msg))
        {
            return 0;
        }
        char why[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea failed = {why, sizeof(why), 0ull};
        if (!EMBED_CALL(cli_files_context_err_msg_internal.paste_walk, CliFilesContextErrMsgCtx, .source = source,
                        .target = to, .each = cli_over_each, .context = args->over, .err_msg = &failed))
        {
            mmgr_line_put(err_msg, source);
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, failed.at);
            return 0;
        }
        EMBED_CALL(cli_files_out_internal.pasted_write, CliFilesOutCtx, .out = out, .first = index == 0u, .from = NULL,
                   .to = to);
    }
    mmgr_line_ch(out, ']');
    return 1;
}

int cli_files_bring_over(const CliFilesContextCfg *args)
{
    char *const arguments_cell = g_cli_cells->temporary_buf_alloc(CLI_OVER_ARGUMENTS_ROOM, &g_cli_editor_cells);
    char *const answer_cell = g_cli_cells->temporary_buf_alloc(CLI_OVER_ANSWER_ROOM, &g_cli_editor_cells);
    CliOver over = {args->call,
                    args->call_context,
                    args->progress,
                    args->context,
                    mmgr_line_over(arguments_cell, CLI_OVER_ARGUMENTS_ROOM),
                    mmgr_line_over(answer_cell, CLI_OVER_ANSWER_ROOM)};
    const int brought =
        EMBED_CALL(cli_over_brought, FilesBringCtx, .over = &over, .into = args->into, .from = args->from,
                   .count = args->count, .out = args->out, .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(over.arguments.out, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(over.answer.out, &g_cli_editor_cells);
    return brought;
}
