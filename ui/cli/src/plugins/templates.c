// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// templates.c: the reader's templates listed, a project begun from one with its name written in, and a tree saved as
// one
#include "templates.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"

int cli_templates_folder(const CliTemplatesCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "templates", .joined = args->folder);
    return 1;
}

// The names listed as a folder is read: counted with their bytes where `name` is NULL, and else put in the cell a
// count took, `most` names and `text_most` bytes of them at the most
typedef struct
{
    const char **name;
    char *text;
    unsigned int count;
    unsigned int most;
    unsigned long long used;
    unsigned long long text_most;
} CliTemplateNames;

static void cli_name_each(const CliEachNameIsFolderCfg *args)
{
    CliTemplateNames *const names = (CliTemplateNames *)args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1ull;
    if (!args->is_folder ||
        ((names->name != NULL) && ((names->count >= names->most) || ((names->used + length) > names->text_most))))
    {
        return;
    }
    if (names->name != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = names->text + names->used, .src = args->name, .bytes = length});
        names->name[names->count] = names->text + names->used;
    }
    names->count += 1u;
    names->used += length;
}

void cli_templates_list(const CliTemplatesOutCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    CliTemplateNames names = {NULL, NULL, 0u, 0u, 0ull, 0ull};
    const int has_folder = EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = folder);
    if (has_folder)
    {
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_name_each,
                   .context_bytess = &names);
    }
    // the names in a cell of the count and bytes a first reading found, read again into it
    const unsigned int most = names.count;
    const unsigned long long text_most = names.used;
    names = (CliTemplateNames){NULL, NULL, 0u, most, 0ull, text_most};
    names.name =
        (has_folder && (most != 0u))
            ? g_cli_cells->persistent_buf_alloc((most * sizeof(*names.name)) + text_most, &g_cli_templates_cells)
            : NULL;
    if (names.name != NULL)
    {
        names.text = (char *)(names.name + most);
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder, .each = cli_name_each,
                   .context_bytess = &names);
    }
    for (unsigned int at = 1u; at < names.count; at += 1u)
    {
        const char *const name = names.name[at];
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = names.name[place - 1u],
                                           .other_pointer = name) > 0))
        {
            names.name[place] = names.name[place - 1u];
            place -= 1u;
        }
        names.name[place] = name;
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = names.name[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = names.name[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(names.name, &g_cli_templates_cells);
}

typedef struct
{
    const char *name;
    const char *text;
    char *out;
    unsigned long long room;
    const char *from;
    const char *to;
    VerbaLinea *err_msg;
    const char *path;
} TemplatesCtx;
// whether a name is one a folder may take: not blank, no separator, no `..`, and none of the characters a path reads
// as more than a name
EMBED_INLINE int cli_plain(const TemplatesCtx *args)
{
    int blank = 1;
    unsigned long long at = 0ull;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    while (at < length)
    {
        unsigned int letter = 0u;
        at +=
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->name, .length = length, .at = at, .letter = &letter);
        blank = blank &&
                ((letter == ' ') || ((letter >= '\t') && (letter <= '\r')) || (letter == 0x85u) || (letter == 0xa0u) ||
                 (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
                 (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u));
    }
    return !blank &&
           (args->name[EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = args->name, .set = "/\\:*?\"<>|")] ==
            '\0') &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = ".", .cap = CLI_CELLS_POOL})) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "..", .cap = CLI_CELLS_POOL}));
}

// `text` with every `{{name}}` in it written as `name`, into `out`, sized for `room`: 1, or 0 where it does not fit
EMBED_INLINE int cli_by_name(const TemplatesCtx *args)
{
    const char *text = args->text;
    const char *name = args->name;
    char *out = args->out;
    VerbaLinea line = {out, args->room, 0ull};
    EMBED_CALL(cli_text_out_name.put_fill, CliTextOutNameCfg, .out = &line, .text = text, .name = "{{name}}",
               .value = name);
    mmgr_line_finish(&line);
    return mmgr_line_ok(&line);
}

// A copy of a template: the project's name and why a copy failed
typedef struct
{
    const char *from;
    const char *to;
    const char *name;
    VerbaLinea *err_msg;
    int failed;
} CliCopy;

EMBED_INLINE int cli_copy(const TemplatesCtx *args);

// that a path could not be copied, reported
EMBED_INLINE int cli_uncopied(const TemplatesCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, ": it could not be copied");
    return 0;
}

// a file copied with `{{name}}` written as the name in its text where it is UTF-8
EMBED_INLINE int cli_file_copy(const TemplatesCtx *args)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->from,
                                                  .size_pointer = &size, .owner = &g_cli_templates_cells);
    if (bytes == NULL)
    {
        return EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->from, .err_msg = args->err_msg);
    }
    int text = 1;
    for (unsigned long long at = 0ull; text && (at < size);)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = bytes, .length = size, .at = at, .letter = &letter);
        text = !((step == 1ull) && (bytes[at] >= 0x80u));
        at += step;
    }
    // the times the text names the project, each a piece before it and the name, and the piece after the last
    unsigned long long named = 0ull;
    for (unsigned long long at = 0ull; text && ((at + 8ull) <= size);)
    {
        const int names = memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = "{{name}}", .bytes = 8u}) == 0;
        named += names ? 1ull : 0ull;
        at += names ? 8ull : 1ull;
    }
    CliFilePiece *const piece =
        g_cli_cells->temporary_buf_alloc(((2ull * named) + 1ull) * sizeof(*piece), &g_cli_templates_cells);
    if (piece == NULL)
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                   .owner = &g_cli_templates_cells);
        return EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->from, .err_msg = args->err_msg);
    }
    unsigned int pieces = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; text && ((at + 8ull) <= size);)
    {
        if (memor.cmp(&(MemoriaCfg){.src = bytes + at, .other = "{{name}}", .bytes = 8u}) != 0)
        {
            at += 1ull;
            continue;
        }
        piece[pieces] = (CliFilePiece){bytes + start, at - start};
        piece[pieces + 1u] =
            (CliFilePiece){args->name, cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL})};
        pieces += 2u;
        at += 8ull;
        start = at;
    }
    piece[pieces] = (CliFilePiece){bytes + start, size - start};
    pieces += 1u;
    // a piece of no bytes contains no zeros either
    const int written = EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = args->to,
                                   .pieces = piece, .count = pieces);
    g_cli_cells->persistent_buf_release(piece, &g_cli_templates_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_templates_cells);
    return written || EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->to, .err_msg = args->err_msg);
}

static void cli_copy_each(const CliEachNameIsFolderCfg *args)
{
    CliCopy *const copy = (CliCopy *)args->context;
    if (copy->failed)
    {
        return;
    }
    char named[CLI_PATH_MAX_LEN];
    char from[CLI_PATH_MAX_LEN];
    char to[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_by_name, TemplatesCtx, .text = args->name, .name = copy->name, .out = named,
                    .room = sizeof(named)))
    {
        copy->failed = !EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->name, .err_msg = copy->err_msg);
        return;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = copy->from, .relative = args->name, .joined = from);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = copy->to, .relative = named, .joined = to);
    copy->failed = args->is_folder ? !EMBED_CALL(cli_copy, TemplatesCtx, .from = from, .to = to, .name = copy->name,
                                                 .err_msg = copy->err_msg)
                                   : !EMBED_CALL(cli_file_copy, TemplatesCtx, .from = from, .to = to,
                                                 .name = copy->name, .err_msg = copy->err_msg);
}

// `from` copied into `to`, made, `{{name}}` written as `name` in each file's name and in each file's text where it is
// text: 1, or 0 with why given
EMBED_INLINE int cli_copy(const TemplatesCtx *args)
{
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->to))
    {
        return EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->to, .err_msg = args->err_msg);
    }
    CliCopy copy = {args->from, args->to, args->name, args->err_msg, 0};
    if (!EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->from,
                    .each = cli_copy_each, .context_bytess = &copy))
    {
        return EMBED_CALL(cli_uncopied, TemplatesCtx, .path = args->from, .err_msg = args->err_msg);
    }
    return !copy.failed;
}

int cli_templates_create(const CliTemplatesTemplateNameCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = folder))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    if (!EMBED_CALL(cli_plain, TemplatesCtx, .name = args->name))
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is no name a folder may take");
        return 0;
    }
    char source[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = args->template_name, .joined = source);
    if (!EMBED_CALL(cli_plain, TemplatesCtx, .name = args->template_name) ||
        !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = source))
    {
        mmgr_line_put(args->err_msg, args->template_name);
        mmgr_line_put(args->err_msg, " is no template of yours: they are the folders in ");
        mmgr_line_put(args->err_msg, folder);
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->parent, .relative = args->name, .joined = args->made);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->made) ||
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->made))
    {
        mmgr_line_put(args->err_msg, args->made);
        mmgr_line_put(args->err_msg, " is there already");
        return 0;
    }
    const int copied = EMBED_CALL(cli_copy, TemplatesCtx, .from = source, .to = args->made, .name = args->name,
                                  .err_msg = args->err_msg);
    return copied;
}

int cli_templates_save(const CliTemplatesNameCfg *args)
{
    if (!EMBED_CALL(cli_plain, TemplatesCtx, .name = args->name))
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is no name a template may take");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = folder))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store templates in");
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = args->name, .joined = args->made);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->made) ||
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->made))
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is a template already");
        return 0;
    }
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = file, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.starts(&(CatenaFinitaCfg){.src = file, .other = ".git/", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        char parent[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = file, .joined = from);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->made, .relative = file, .joined = to);
        if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = to, .out = parent) &&
            !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent))
        {
            EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
            return EMBED_CALL(cli_uncopied, TemplatesCtx, .path = parent, .err_msg = args->err_msg);
        }
        if (!EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from, .to = to))
        {
            const int uncopied = EMBED_CALL(cli_uncopied, TemplatesCtx, .path = file, .err_msg = args->err_msg);
            EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
            return uncopied;
        }
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    return 1;
}
