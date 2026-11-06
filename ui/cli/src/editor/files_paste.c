// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_paste.c: files and folders copied or moved into a folder of the tree, a name the folder contains already
// numbered, and a file brought from another machine written a part at a time beside the file it becomes
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "files_internal.h"

// the bytes of the paths a paste's walk queues to go to, and the most of them; and the most numbers a free name is
// looked for under
#define CLI_PENDING_TEXT (1u << 23u)
#define CLI_PENDING_MAX 65536u
#define CLI_FREE_MAX 1000000u

typedef struct
{
    char letter;
    const char *path;
    const char *source;
    const char *target;
    VerbaLinea *err_msg;
    const char *folder;
    const char *name;
    int moving;
    char *target_out;
} FilesPasteCtx;
// whether a byte parts a path
EMBED_INLINE int cli_parts(const FilesPasteCtx *args)
{
    return (args->letter == '/') || (args->letter == '\\');
}

int cli_paste_name(const CliFilesPathNameCtx *args)
{
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    while ((end > 0ULL) && EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[end - 1ULL]))
    {
        end -= 1ULL;
    }
    unsigned long long start = end;
    while ((start > 0ULL) && !EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[start - 1ULL]))
    {
        start -= 1ULL;
    }
    const unsigned long long length = end - start;
    const int up = (length == 2ULL) && (args->path[start] == '.') && (args->path[start + 1ULL] == '.');
    const int drive = (start == 0ULL) && (length == 2ULL) && (args->path[1] == ':');
    if ((length == 0ULL) || (length >= CLI_PATH_MAX_LEN) || up || drive)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->name, .src = args->path + start, .bytes = length});
    args->name[length] = '\0';
    return 1;
}

int cli_paste_under(const CliFilesPathFolderCtx *args)
{
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_POOL_BYTES});
    while ((length > 0ULL) && EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->folder[length - 1ULL]))
    {
        length -= 1ULL;
    }
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        if ((args->folder[at] != args->path[at]) &&
            !(EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->folder[at]) &&
              EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[at])))
        {
            return 0;
        }
    }
    return (args->path[length] == '\0') || EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[length]);
}

// whether a file or a folder is at `path`
EMBED_INLINE int cli_there(const FilesPasteCtx *args)
{
    const char *path = args->path;
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) ||
           EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path);
}

void cli_paste_free(const CliFilesFolderCtx *args)
{
    VerbaLinea first = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&first, args->folder);
    mmgr_line_ch(&first, '/');
    mmgr_line_put(&first, args->name_read);
    mmgr_line_finish(&first);
    if (!EMBED_CALL(cli_there, FilesPasteCtx, .path = args->path_out))
    {
        return;
    }
    // the stem and the extension, split at the last dot, a dot that starts the name splitting nothing
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->name_read,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name_read, .cap = CLI_POOL_BYTES}), .byte = '.');
    const int split = (dot != NULL) && (dot != args->name_read) &&
                      (!cellul.eq(&(CatenaFinitaCfg){.src = args->name_read, .other = "..", .cap = CLI_POOL_BYTES}));
    // the dot lies in the name, at or past its start
    const unsigned long long stem = split
                                        ? (unsigned long long)(dot - args->name_read)
                                        : cellul.len(&(CatenaFinitaCfg){.src = args->name_read, .cap = CLI_POOL_BYTES});
    char numbered[CLI_PATH_MAX_LEN];
    for (unsigned int number = 2u; number < CLI_FREE_MAX; number += 1u)
    {
        VerbaLinea line = {numbered, sizeof(numbered), 0ULL};
        mmgr_line_put(&line, args->folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put_n(&line, args->name_read, stem);
        mmgr_line_put(&line, " (");
        mmgr_line_decimal(&line, number, 0u);
        mmgr_line_ch(&line, ')');
        mmgr_line_put(&line, split ? dot : "");
        if ((mmgr_line_finish(&line) != 0ULL) && !EMBED_CALL(cli_there, FilesPasteCtx, .path = numbered))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->path_out,
                              .src = numbered,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = numbered, .cap = CLI_POOL_BYTES}) + 1u});
            return;
        }
    }
}

// A walk's paths still to go to, each a pair, where it is and where it goes, the last put first taken; and the names a
// folder contains as it is read
typedef struct
{
    char text[CLI_PENDING_TEXT];
    unsigned long long used;
    unsigned long long start[CLI_PENDING_MAX];
    unsigned int count;
    char names[CLI_PENDING_TEXT];
    unsigned long long names_used;
    unsigned long long name_start[CLI_PENDING_MAX];
    unsigned int name_count;
    int full;
} CliPending;

// the walk's pairs waiting, a cell taken with the walk's lock and given back with it
static CliPending *s_pending;

// a pair put to go to: 1, or 0 where the walk has capacity for no more
EMBED_INLINE int cli_pending_append(const FilesPasteCtx *args)
{
    const unsigned long long source_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->source, .cap = CLI_POOL_BYTES}) + 1ULL;
    const unsigned long long target_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->target, .cap = CLI_POOL_BYTES}) + 1ULL;
    if ((s_pending->count >= CLI_PENDING_MAX) || ((s_pending->used + source_length + target_length) > CLI_PENDING_TEXT))
    {
        s_pending->full = 1;
        return 0;
    }
    s_pending->start[s_pending->count] = s_pending->used;
    memor.cpy(&(MemoriaCfg){.dst = s_pending->text + s_pending->used, .src = args->source, .bytes = source_length});
    memor.cpy(&(MemoriaCfg){
        .dst = s_pending->text + s_pending->used + source_length, .src = args->target, .bytes = target_length});
    s_pending->used += source_length + target_length;
    s_pending->count += 1u;
    return 1;
}

static void cli_pending_name(const CliEachNameIsFolderCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1ULL;
    if ((s_pending->name_count >= CLI_PENDING_MAX) || ((s_pending->names_used + length) > CLI_PENDING_TEXT))
    {
        s_pending->full = 1;
        return;
    }
    s_pending->name_start[s_pending->name_count] = s_pending->names_used;
    memor.cpy(&(MemoriaCfg){.dst = s_pending->names + s_pending->names_used, .src = args->name, .bytes = length});
    s_pending->names_used += length;
    s_pending->name_count += 1u;
}

// a folder's entries put to go to, the first last so it is taken first: 1, or 0 with why given
EMBED_INLINE int cli_pending_entries(const FilesPasteCtx *args)
{
    s_pending->name_count = 0u;
    s_pending->names_used = 0ULL;
    if (!EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->source,
                    .each = cli_pending_name, .context_bytess = NULL))
    {
        mmgr_line_put(args->err_msg, args->source);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    for (unsigned int index = s_pending->name_count; !s_pending->full && (index > 0u); index -= 1u)
    {
        const char *const name = s_pending->names + s_pending->name_start[index - 1u];
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        VerbaLinea from_line = {from, sizeof(from), 0ULL};
        VerbaLinea to_line = {to, sizeof(to), 0ULL};
        mmgr_line_put(&from_line, args->source);
        mmgr_line_ch(&from_line, '/');
        mmgr_line_put(&from_line, name);
        mmgr_line_put(&to_line, args->target);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, name);
        s_pending->full = s_pending->full || (mmgr_line_finish(&from_line) == 0ULL) ||
                          (mmgr_line_finish(&to_line) == 0ULL) ||
                          !EMBED_CALL(cli_pending_append, FilesPasteCtx, .source = from, .target = to);
    }
    if (s_pending->full)
    {
        mmgr_line_put(args->err_msg, args->source);
        mmgr_line_put(args->err_msg, ": it contains more than a paste takes");
        return 0;
    }
    return 1;
}

int cli_paste_walk(const CliFilesContextErrMsgCtx *args)
{
    s_pending = g_cli_guard->persistent_buf_alloc(sizeof(*s_pending), g_cli_editor_owner);
    if (s_pending == NULL)
    {
        mmgr_line_put(args->err_msg, "a paste's walk could not be taken");
        return 0;
    }
    s_pending->count = 0u;
    s_pending->used = 0ULL;
    s_pending->full = 0;
    int walked = EMBED_CALL(cli_pending_append, FilesPasteCtx, .source = args->source, .target = args->target);
    while (walked && (s_pending->count > 0u))
    {
        s_pending->count -= 1u;
        const char *const pair = s_pending->text + s_pending->start[s_pending->count];
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = from,
                                .src = pair,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = to,
            .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_POOL_BYTES}) + 1,
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_POOL_BYTES}) + 1,
                         .cap = CLI_POOL_BYTES}) +
                     1u});
        s_pending->used = s_pending->start[s_pending->count];
        const int folder = EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = from);
        walked = EMBED_CALL(args->each, CliPasteEachCfg, .context = args->context, .source = from, .target = to,
                            .folder = folder, .err_msg = args->err_msg) &&
                 (!folder || EMBED_CALL(cli_pending_entries, FilesPasteCtx, .source = from, .target = to,
                                        .err_msg = args->err_msg));
    }
    g_cli_guard->persistent_buf_release(s_pending, g_cli_editor_owner);
    s_pending = NULL;
    return walked;
}

// What a paste's walk copies: the path it was requested, which a failure names
typedef struct
{
    const char *source;
} CliCopying;

// that a path could not be moved, reported
EMBED_INLINE int cli_unmoved(const FilesPasteCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, ": it could not be moved");
    return 0;
}

int cli_paste_begin(const CliFilesRootOutCtx *args)
{
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->into,
                    .joined = args->folder_out, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folder_out))
    {
        mmgr_line_put(args->err_msg, args->into);
        mmgr_line_put(args->err_msg, " is no folder");
        return 0;
    }
    mmgr_line_ch(args->out, '[');
    return 1;
}

int cli_paste_by_name(const CliFilesErrMsgCtx *args)
{
    if (!EMBED_CALL(cli_files_path_name_internal.paste_name, CliFilesPathNameCtx, .path = args->source,
                    .name = args->name))
    {
        mmgr_line_put(args->err_msg, args->source);
        mmgr_line_put(args->err_msg, " has no name");
        return 0;
    }
    return 1;
}

int cli_files_free_in(const CliFilesRootErrMsgCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->into, .joined = folder,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = folder))
    {
        mmgr_line_put(args->err_msg, args->into);
        mmgr_line_put(args->err_msg, " is no folder");
        return 0;
    }
    char free_path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_files_folder_internal.paste_free, CliFilesFolderCtx, .folder = folder, .name_read = args->name,
               .path_out = free_path);
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = free_path,
               .relative_out = args->path_out);
    return 1;
}

int cli_files_write_part(const CliFilesRootPathCfg *args)
{
    const char *root = args->root;
    char target[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->path, .joined = target,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    char name[CLI_PATH_MAX_LEN];
    char parent[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_files_path_name_internal.paste_name, CliFilesPathNameCtx, .path = target, .name = name) ||
        !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = target, .out = parent))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " has no name");
        return 0;
    }
    // the parts go to a file beside the target, begun afresh by the part at 0
    char part[CLI_PATH_MAX_LEN];
    VerbaLinea line = {part, sizeof(part), 0ULL};
    mmgr_line_put(&line, parent);
    mmgr_line_put(&line, "/.");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, ".orior-part");
    unsigned long long has = 0ULL;
    if ((mmgr_line_finish(&line) == 0ULL) ||
        ((args->at != 0ULL) &&
         !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = part, .size_pointer = &has)))
    {
        mmgr_line_put(args->err_msg, part);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    if (has != args->at)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": a part at ");
        mmgr_line_decimal(args->err_msg, args->at, 0u);
        mmgr_line_put(args->err_msg, " came after ");
        mmgr_line_decimal(args->err_msg, has, 0u);
        mmgr_line_put(args->err_msg, " bytes");
        return 0;
    }
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = part, .bytes = args->data,
                    .size = args->size, .fresh = args->at == 0ULL))
    {
        mmgr_line_put(args->err_msg, part);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    if (args->last)
    {
        if (!EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = part, .to = target))
        {
            return EMBED_CALL(cli_unmoved, FilesPasteCtx, .path = target, .err_msg = args->err_msg);
        }
        const char *const moved[1] = {args->path};
        EMBED_CALL(cli_symbols_root.tree_mirror, CliSymbolsRootCfg, .root = root, .paths = moved, .count = 1u);
    }
    return 1;
}
