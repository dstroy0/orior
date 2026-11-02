// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_paste.c: files and folders copied or moved into a folder of the tree, a name the folder contains already
// numbered, and a file brought from another machine written a part at a time beside the file it becomes
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "files_internal.h"

// the bytes of the paths a paste's walk queues to go to, and the most of them; and the most numbers a free name is
// looked for under
#define CLI_PENDING_TEXT (1u << 23u)
#define CLI_PENDING_MOST 65536u
#define CLI_FREE_MOST 1000000u

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
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    while ((end > 0ull) && EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[end - 1ull]))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    while ((start > 0ull) && !EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->path[start - 1ull]))
    {
        start -= 1ull;
    }
    const unsigned long long length = end - start;
    const int up = (length == 2ull) && (args->path[start] == '.') && (args->path[start + 1ull] == '.');
    const int drive = (start == 0ull) && (length == 2ull) && (args->path[1] == ':');
    if ((length == 0ull) || (length >= CLI_PATH_MAX_LEN) || up || drive)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->name, .src = args->path + start, .bytes = length});
    args->name[length] = '\0';
    return 1;
}

int cli_paste_under(const CliFilesPathFolderCtx *args)
{
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) && EMBED_CALL(cli_parts, FilesPasteCtx, .letter = args->folder[length - 1ull]))
    {
        length -= 1ull;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
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
    VerbaLinea first = {args->path_out, CLI_PATH_MAX_LEN, 0ull};
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
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name_read, .cap = CLI_CELLS_POOL}), .byte = '.');
    const int split = (dot != NULL) && (dot != args->name_read) &&
                      (!cellul.eq(&(CatenaFinitaCfg){.src = args->name_read, .other = "..", .cap = CLI_CELLS_POOL}));
    // the dot lies in the name, at or past its start
    const unsigned long long stem = split
                                        ? (unsigned long long)(dot - args->name_read)
                                        : cellul.len(&(CatenaFinitaCfg){.src = args->name_read, .cap = CLI_CELLS_POOL});
    char numbered[CLI_PATH_MAX_LEN];
    for (unsigned int number = 2u; number < CLI_FREE_MOST; number += 1u)
    {
        VerbaLinea line = {numbered, sizeof(numbered), 0ull};
        mmgr_line_put(&line, args->folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put_n(&line, args->name_read, stem);
        mmgr_line_put(&line, " (");
        mmgr_line_decimal(&line, number, 0u);
        mmgr_line_ch(&line, ')');
        mmgr_line_put(&line, split ? dot : "");
        if ((mmgr_line_finish(&line) != 0ull) && !EMBED_CALL(cli_there, FilesPasteCtx, .path = numbered))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->path_out,
                              .src = numbered,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = numbered, .cap = CLI_CELLS_POOL}) + 1u});
            return;
        }
    }
}

void cli_pasted_write(const CliFilesOutCtx *args)
{
    mmgr_line_put(args->out, args->first ? "{\"from\":" : ",{\"from\":");
    if (args->from != NULL)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->from,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"to\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->to,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->to, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(args->out, '}');
}

// A walk's paths still to go to, each a pair, where it is and where it goes, the last put first taken; and the names a
// folder contains as it is read
typedef struct
{
    char text[CLI_PENDING_TEXT];
    unsigned long long used;
    unsigned long long start[CLI_PENDING_MOST];
    unsigned int count;
    char names[CLI_PENDING_TEXT];
    unsigned long long names_used;
    unsigned long long name_start[CLI_PENDING_MOST];
    unsigned int name_count;
    int full;
} CliPending;

// the walk's pairs waiting, a cell taken with the walk's lock and given back with it
static CliPending *s_pending;

// a pair put to go to: 1, or 0 where the walk has room for no more
EMBED_INLINE int cli_pending_put(const FilesPasteCtx *args)
{
    const unsigned long long source_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->source, .cap = CLI_CELLS_POOL}) + 1ull;
    const unsigned long long target_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->target, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((s_pending->count >= CLI_PENDING_MOST) ||
        ((s_pending->used + source_length + target_length) > CLI_PENDING_TEXT))
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
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((s_pending->name_count >= CLI_PENDING_MOST) || ((s_pending->names_used + length) > CLI_PENDING_TEXT))
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
    s_pending->names_used = 0ull;
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
        VerbaLinea from_line = {from, sizeof(from), 0ull};
        VerbaLinea to_line = {to, sizeof(to), 0ull};
        mmgr_line_put(&from_line, args->source);
        mmgr_line_ch(&from_line, '/');
        mmgr_line_put(&from_line, name);
        mmgr_line_put(&to_line, args->target);
        mmgr_line_ch(&to_line, '/');
        mmgr_line_put(&to_line, name);
        s_pending->full = s_pending->full || (mmgr_line_finish(&from_line) == 0ull) ||
                          (mmgr_line_finish(&to_line) == 0ull) ||
                          !EMBED_CALL(cli_pending_put, FilesPasteCtx, .source = from, .target = to);
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
    s_pending = g_cli_cells->persistent_buf_alloc(sizeof(*s_pending), &g_cli_editor_cells);
    if (s_pending == NULL)
    {
        mmgr_line_put(args->err_msg, "a paste's walk could not be taken");
        return 0;
    }
    s_pending->count = 0u;
    s_pending->used = 0ull;
    s_pending->full = 0;
    int walked = EMBED_CALL(cli_pending_put, FilesPasteCtx, .source = args->source, .target = args->target);
    while (walked && (s_pending->count > 0u))
    {
        s_pending->count -= 1u;
        const char *const pair = s_pending->text + s_pending->start[s_pending->count];
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = from,
                                .src = pair,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = to,
            .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1,
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = pair + cellul.len(&(CatenaFinitaCfg){.src = pair, .cap = CLI_CELLS_POOL}) + 1,
                         .cap = CLI_CELLS_POOL}) +
                     1u});
        s_pending->used = s_pending->start[s_pending->count];
        const int folder = EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = from);
        walked = EMBED_CALL(args->each, CliPasteEachCfg, .context = args->context, .source = from, .target = to,
                            .folder = folder, .err_msg = args->err_msg) &&
                 (!folder || EMBED_CALL(cli_pending_entries, FilesPasteCtx, .source = from, .target = to,
                                        .err_msg = args->err_msg));
    }
    g_cli_cells->persistent_buf_release(s_pending, &g_cli_editor_cells);
    s_pending = NULL;
    return walked;
}

// What a paste's walk copies: the path it was requested, which a failure names
typedef struct
{
    const char *source;
} CliCopying;

// a folder made, or a file copied
static int cli_copy(const CliPasteEachCfg *args)
{
    int folder = args->folder;
    const CliCopying *const copying = (const CliCopying *)args->context;
    if (folder ? EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->target)
               : EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = args->source, .to = args->target))
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, copying->source);
    mmgr_line_put(args->err_msg, ": it could not be copied");
    return 0;
}

// that a path could not be moved, reported
EMBED_INLINE int cli_unmoved(const FilesPasteCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, ": it could not be moved");
    return 0;
}

// one of a paste's paths copied or moved into `folder`, where it went into `target`: 1, or 0 with why given. A move
// between drives is a copy and then a removal
EMBED_INLINE int cli_pasted_one(const FilesPasteCtx *args)
{
    const char *source = args->source;
    const char *folder = args->folder;
    const char *name = args->name;
    char *target = args->target_out;
    VerbaLinea *err_msg = args->err_msg;
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = source) &&
        EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = source, .path = folder))
    {
        mmgr_line_put(err_msg, source);
        mmgr_line_put(err_msg, " is not pasted into itself");
        return 0;
    }
    EMBED_CALL(cli_files_folder_internal.paste_free, CliFilesFolderCtx, .folder = folder, .name_read = name,
               .path_out = target);
    CliCopying copying = {source};
    if (!args->moving)
    {
        return EMBED_CALL(cli_files_context_err_msg_internal.paste_walk, CliFilesContextErrMsgCtx, .source = source,
                          .target = target, .each = cli_copy, .context = &copying, .err_msg = err_msg);
    }
    const int moved = EMBED_CALL(cli_err_msg_from_internal.move, CliErrMsgFromCtx, .from_text = source, .to = target);
    if (moved == CLI_MOVE_FAILED)
    {
        return EMBED_CALL(cli_unmoved, FilesPasteCtx, .path = source, .err_msg = err_msg);
    }
    if (moved == CLI_MOVE_ACROSS)
    {
        if (!EMBED_CALL(cli_files_context_err_msg_internal.paste_walk, CliFilesContextErrMsgCtx, .source = source,
                        .target = target, .each = cli_copy, .context = &copying, .err_msg = err_msg))
        {
            return 0;
        }
        return EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = source) ||
               EMBED_CALL(cli_unmoved, FilesPasteCtx, .path = source, .err_msg = err_msg);
    }
    return 1;
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

int cli_files_paste(const CliFilesRootCountCfg *args)
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
        char was[CLI_PATH_MAX_LEN];
        const int inside = EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = root,
                                      .path = source);
        if (inside)
        {
            EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = source,
                       .relative_out = was);
        }
        // a file moved into the folder it is in stays where it is
        char parent[CLI_PATH_MAX_LEN];
        const int stays =
            args->moving &&
            EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = source, .out = parent) &&
            EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = parent,
                       .path = folder) &&
            EMBED_CALL(cli_files_path_folder_internal.paste_under, CliFilesPathFolderCtx, .folder = folder,
                       .path = parent);
        char target[CLI_PATH_MAX_LEN];
        if (!stays && !EMBED_CALL(cli_pasted_one, FilesPasteCtx, .source = source, .folder = folder, .name = name,
                                  .moving = args->moving, .target_out = target, .err_msg = args->err_msg))
        {
            return 0;
        }
        char to[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = stays ? source : target,
                   .relative_out = to);
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
    VerbaLinea line = {part, sizeof(part), 0ull};
    mmgr_line_put(&line, parent);
    mmgr_line_put(&line, "/.");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, ".orior-part");
    unsigned long long has = 0ull;
    if ((mmgr_line_finish(&line) == 0ull) ||
        ((args->at != 0ull) &&
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
                    .size = args->size, .fresh = args->at == 0ull))
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
