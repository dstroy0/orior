// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.c: the oracle tables, the k-file head and the language tables read from the tree and written as the window
// reads them
#include "defs.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// A file read: its text, mapped, and its size; the text NULL where it could not be read
typedef struct
{
    const char *text;
    unsigned long long size;
} CliDefsFile;
// The names a folder lists, its folders where `folders` is 1, else its files ending `.tsv`, in a cell of `room`
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int room;
    int folders;
} CliDefsNames;
// the most folders or tables a listing contains
#define CLI_DEFS_NAMES 512u

typedef struct
{
    const char *path;
    CliDefsFile file;
    const char *at;
    const char *end;
    const char **stop;
    char letter;
    const char *from;
    const char *to;
    VerbaLinea *out;
    const char *root;
    const char *name;
    CliDefsLine *line;
    unsigned long long *length;
    unsigned long long *number;
    const char *folder;
    int folders;
    CliDefsNames *names;
} DefsCtx;
// the file at `path` mapped whole, let go with cli_unmapped
EMBED_INLINE CliDefsFile cli_map(const DefsCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_defs_cells);
    // a file's bytes are read as its text
    return (CliDefsFile){(const char *)bytes, size};
}

EMBED_INLINE void cli_unmapped(const DefsCtx *args)
{
    if (args->file.text != NULL)
    {
        // the text is the bytes the file was mapped as
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx,
                   .bytes_bytes_other = (const unsigned char *)args->file.text, .owner = &g_cli_defs_cells);
    }
}

// the line from `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`; where the next starts
EMBED_INLINE const char *cli_line(const DefsCtx *args)
{
    const char *at = args->at;
    const char **stop = args->stop;
    const char *next = at;
    next += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = next, .at = 0ull,
                       .size = (unsigned long long)(args->end - next), .set = "\n");
    *stop = ((next > at) && (next[-1] == '\r')) ? (next - 1) : next;
    return (next < args->end) ? (next + 1) : next;
}

EMBED_INLINE int cli_is_space(const DefsCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// whether a line contains no table: a comment, or blank
EMBED_INLINE int cli_skip(const DefsCtx *args)
{
    const char *from = args->from;
    if ((from < args->to) && (from[0] == '#'))
    {
        return 1;
    }
    while ((from < args->to) && EMBED_CALL(cli_is_space, DefsCtx, .letter = from[0]))
    {
        from += 1;
    }
    return from == args->to;
}

// a line's fields between tabs written as a JSON list
EMBED_INLINE void cli_fields_write(const DefsCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    const char *field = args->from;
    for (const char *at = args->from;; at += 1)
    {
        if ((at == args->to) || (at[0] == '\t'))
        {
            mmgr_line_put(out, (field != args->from) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = field,
                       .length = (unsigned long long)(at - field));
            field = at + 1;
            if (at == args->to)
            {
                break;
            }
        }
    }
    mmgr_line_ch(out, ']');
}

// The table `file` contains written, its first line that is not a comment naming its columns; 0 where it has no such
// line, nothing written then
EMBED_INLINE int cli_rows_write(const DefsCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    VerbaLinea *out = args->out;
    const char *const end = args->file.text + args->file.size;
    const char *at = args->file.text;
    const char *stop = NULL;
    const char *next = NULL;
    while (at < end)
    {
        next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        if (!EMBED_CALL(cli_skip, DefsCtx, .from = at, .to = stop))
        {
            break;
        }
        at = next;
    }
    if (at >= end)
    {
        return 0;
    }
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = path, .relative_out = relative);
    mmgr_line_put(out, "{\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = relative,
               .length = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"columns\":");
    EMBED_CALL(cli_fields_write, DefsCtx, .out = out, .from = at, .to = stop);
    mmgr_line_put(out, ",\"rows\":[");
    int first = 1;
    at = next;
    while (at < end)
    {
        next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        if (!EMBED_CALL(cli_skip, DefsCtx, .from = at, .to = stop))
        {
            mmgr_line_put(out, first ? "" : ",");
            EMBED_CALL(cli_fields_write, DefsCtx, .out = out, .from = at, .to = stop);
            first = 0;
        }
        at = next;
    }
    mmgr_line_put(out, "]}");
    return 1;
}

// A tab separated table whose first line that is not a comment names its columns, written; 0 where it has no such line
// or could not be read, nothing written then
EMBED_INLINE int cli_table_write(const DefsCtx *args)
{
    const CliDefsFile file = cli_map(args);
    if (file.text == NULL)
    {
        return 0;
    }
    const int written = EMBED_CALL(cli_rows_write, DefsCtx, .root = args->root, .path = args->path, .name = args->name,
                                   .file = file, .out = args->out);
    EMBED_CALL(cli_unmapped, DefsCtx, .file = file);
    return written;
}

// a line's cells between tabs, from `from` before `to`, into `line`
EMBED_INLINE void cli_cells_get(const DefsCtx *args)
{
    args->line->count = 0u;
    const char *cell = args->from;
    for (const char *at = args->from;; at += 1)
    {
        if ((at == args->to) || (at[0] == '\t'))
        {
            if (args->line->count < CLI_DEFS_CELLS)
            {
                args->line->cell[args->line->count] = cell;
                // a cell lies within its line
                args->line->length[args->line->count] = (unsigned long long)(at - cell);
                args->line->count += 1u;
            }
            cell = at + 1;
            if (at == args->to)
            {
                break;
            }
        }
    }
}

int cli_defs_lines(const CliDefsPathCfg *args)
{
    const CliDefsFile file = EMBED_CALL(cli_map, DefsCtx, .path = args->path);
    if (file.text == NULL)
    {
        return 0;
    }
    const char *const end = file.text + file.size;
    int named = 0;
    for (const char *at = file.text; at < end;)
    {
        const char *stop = NULL;
        const char *const next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        if (!EMBED_CALL(cli_skip, DefsCtx, .from = at, .to = stop))
        {
            CliDefsLine line;
            EMBED_CALL(cli_cells_get, DefsCtx, .from = at, .to = stop, .line = &line);
            EMBED_CALL(args->each, CliDefsLineEachCfg, .context = args->context, .line = &line, .named = !named);
            named = 1;
        }
        at = next;
    }
    EMBED_CALL(cli_unmapped, DefsCtx, .file = file);
    return named;
}

// The comment lines a C header opens with, past its license line, each trimmed of its slashes and joined by a space,
// written as one JSON string, joined in a cell of the header's size
EMBED_INLINE void cli_header_write(const DefsCtx *args)
{
    VerbaLinea *out = args->out;
    const CliDefsFile file = cli_map(args);
    const unsigned long long size = (file.text != NULL) ? file.size : 0ull;
    char *const text = g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_defs_cells);
    VerbaLinea joined = mmgr_line_over(text, size + 1ull);
    const char *const end = file.text + size;
    int first = 1;
    const char *at = file.text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        const char *from = at;
        while ((from < stop) && EMBED_CALL(cli_is_space, DefsCtx, .letter = from[0]))
        {
            from += 1;
        }
        while ((stop > from) && EMBED_CALL(cli_is_space, DefsCtx, .letter = stop[-1]))
        {
            stop -= 1;
        }
        if (((stop - from) < 2) || (from[0] != '/') || (from[1] != '/'))
        {
            break;
        }
        const int licensed = cellul.find(&(CatenaFinitaCfg){
                                 .src = from, .cap = (size_t)(stop - from), .other = "SPDX", .other_cap = 5u}) != NULL;
        from += !licensed ? EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from,
                                       .at = 0ull, .size = (unsigned long long)(stop - from), .set = "/")
                          : 0ull;
        from += !licensed ? EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from,
                                       .at = 0ull, .size = (unsigned long long)(stop - from), .set = CLI_TEXT_WHITE)
                          : 0ull;
        if (!licensed)
        {
            mmgr_line_put(&joined, first ? "" : " ");
            mmgr_line_put_n(&joined, from, (unsigned long long)(stop - from));
            first = 0;
        }
        at = next;
    }
    const unsigned long long length = mmgr_line_ok(&joined) ? joined.at : 0ull;
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = (text != NULL) ? text : "", .length = length);
    g_cli_cells->persistent_buf_release(text, &g_cli_defs_cells);
    EMBED_CALL(cli_unmapped, DefsCtx, .file = file);
}

// A `#define NAME value` read out of the text: where its value starts, trimmed, and its length; NULL where none is
EMBED_INLINE const char *cli_define(const DefsCtx *args)
{
    unsigned long long *length = args->length;
    char key[CLI_NAME_MAX_LEN];
    VerbaLinea line = {key, sizeof(key), 0ull};
    mmgr_line_put(&line, "#define ");
    mmgr_line_put(&line, args->name);
    mmgr_line_ch(&line, ' ');
    const unsigned long long key_length = mmgr_line_finish(&line);
    const char *const end = args->file.text + args->file.size;
    const char *at = args->file.text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        if (((unsigned long long)(stop - at) >= key_length) &&
            (memor.cmp(&(MemoriaCfg){.src = at, .other = key, .bytes = key_length}) == 0))
        {
            const char *from = at + key_length;
            while ((from < stop) && EMBED_CALL(cli_is_space, DefsCtx, .letter = from[0]))
            {
                from += 1;
            }
            while ((stop > from) && EMBED_CALL(cli_is_space, DefsCtx, .letter = stop[-1]))
            {
                stop -= 1;
            }
            *length = (unsigned long long)(stop - from);
            return from;
        }
        at = next;
    }
    return NULL;
}

// a define's value as a whole number, its `u` suffix taken off: 1, or 0 where it is none or past 32 bits
EMBED_INLINE int cli_define_number(const DefsCtx *args)
{
    unsigned long long *number = args->number;
    unsigned long long length = 0ull;
    const char *const value =
        EMBED_CALL(cli_define, DefsCtx, .name = args->name, .file = args->file, .length = &length);
    length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = value, .at = length,
                        .set = "u");
    unsigned long long total = 0ull;
    const unsigned long long sign = ((length != 0ull) && (value[0] == '+')) ? 1ull : 0ull;
    if (length == sign)
    {
        return 0;
    }
    for (unsigned long long at = sign; at < length; at += 1ull)
    {
        if ((value[at] < '0') || (value[at] > '9'))
        {
            return 0;
        }
        total = (total * 10ull) + (unsigned long long)(value[at] - '0');
        if (total > 0xffffffffull)
        {
            return 0;
        }
    }
    *number = total;
    return 1;
}

// the head every k-file opens with, as the header `file` defines it, written; `null` where it defines none
EMBED_INLINE void cli_head_get(const DefsCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned long long bytes = 0ull;
    unsigned long long version = 0ull;
    unsigned long long magic_length = 0ull;
    const char *magic = (args->file.text != NULL) ? EMBED_CALL(cli_define, DefsCtx, .name = "KREP_MAGIC",
                                                               .file = args->file, .length = &magic_length)
                                                  : NULL;
    if ((magic == NULL) ||
        !EMBED_CALL(cli_define_number, DefsCtx, .name = "KREP_HEAD_BYTES", .file = args->file, .number = &bytes) ||
        !EMBED_CALL(cli_define_number, DefsCtx, .name = "KREP_VERSION", .file = args->file, .number = &version))
    {
        mmgr_line_put(out, "null");
        return;
    }
    // the magic's quotes taken off, and its text up to its first escape
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg,
                                                   .text = magic, .at = 0ull, .size = magic_length, .set = "\"");
        magic += lead;
        magic_length -= lead;
    }
    magic_length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = magic,
                              .at = magic_length, .set = "\"");
    unsigned long long len = 0ull;
    len = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = magic, .at = len,
                     .size = magic_length, .set = "\\");
    mmgr_line_put(out, "{\"bytes\":");
    mmgr_line_decimal(out, bytes, 1u);
    mmgr_line_put(out, ",\"magic\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = magic,
               .length = len);
    mmgr_line_put(out, ",\"version\":");
    mmgr_line_decimal(out, version, 1u);
    mmgr_line_ch(out, '}');
}

// the head every k-file opens with, as krep_head.h defines it, written; `null` where it cannot be read
EMBED_INLINE void cli_head_write(const DefsCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/src/cu/includes/file_defs/krep/krep_head.h");
    const CliDefsFile file =
        (mmgr_line_finish(&line) != 0ull) ? EMBED_CALL(cli_map, DefsCtx, .path = path) : (CliDefsFile){NULL, 0ull};
    EMBED_CALL(cli_head_get, DefsCtx, .file = file, .out = args->out);
    EMBED_CALL(cli_unmapped, DefsCtx, .file = file);
}

// The names a folder lists: its folders, or its files ending `.tsv`, in order
static void cli_name_each(const CliEachNameIsFolderCfg *args)
{
    CliDefsNames *const names = args->context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    const int wanted =
        names->folders
            ? args->is_folder
            : ((length > 4u) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = args->name + length - 4u, .other = ".tsv", .cap = CLI_CELLS_POOL})));
    if (wanted && (length < CLI_NAME_MAX_LEN) && (names->count < names->room))
    {
        memor.cpy(&(MemoriaCfg){.dst = names->name[names->count], .src = args->name, .bytes = length + 1u});
        names->count += 1u;
    }
}

// the names `folder` lists into `names`, in order, those it contained before given back
EMBED_INLINE void cli_names_list(const DefsCtx *args)
{
    CliDefsNames *names = args->names;
    g_cli_cells->persistent_buf_release(names->name, &g_cli_defs_cells);
    char (*const taken)[CLI_NAME_MAX_LEN] =
        g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_DEFS_NAMES * CLI_NAME_MAX_LEN, &g_cli_defs_cells);
    *names = (CliDefsNames){taken, 0u, (taken != NULL) ? CLI_DEFS_NAMES : 0u, args->folders};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = cli_name_each, .context_bytess = names);
    char (*const name)[CLI_NAME_MAX_LEN] = names->name;
    for (unsigned int at = 1u; at < names->count; at += 1u)
    {
        char swap_name[CLI_NAME_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = swap_name,
                                .src = name[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = name[place - 1u],
                                           .other_pointer = swap_name) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = name[place],
                .src = name[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = name[place],
                                .src = swap_name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = swap_name, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

void cli_defs_read(const CliDefsCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    CliDefsNames names = {NULL, 0u, 0u, 1};
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/src/cu/includes/file_defs");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_names_list, DefsCtx, .folder = folder, .folders = 1, .names = &names);
    mmgr_line_put(args->out, "{\"types\":[");
    int first = 1;
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        const char *const ext = names.name[at];
        VerbaLinea table = {path, sizeof(path), 0ull};
        mmgr_line_put(&table, folder);
        mmgr_line_ch(&table, '/');
        mmgr_line_put(&table, ext);
        mmgr_line_ch(&table, '/');
        mmgr_line_put(&table, ext);
        mmgr_line_put(&table, ".oracle.tsv");
        if (mmgr_line_finish(&table) == 0ull)
        {
            continue;
        }
        const unsigned long long begun = args->out->at;
        mmgr_line_put(args->out, first ? "{\"ext\":" : ",{\"ext\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = ext,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = ext, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"table\":");
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ull};
        mmgr_line_put(&named, ext);
        mmgr_line_put(&named, ".oracle.tsv");
        mmgr_line_finish(&named);
        if (!EMBED_CALL(cli_table_write, DefsCtx, .root = args->root, .path = path, .name = name, .out = args->out))
        {
            args->out->at = begun;
            continue;
        }
        VerbaLinea header = {path, sizeof(path), 0ull};
        mmgr_line_put(&header, folder);
        mmgr_line_ch(&header, '/');
        mmgr_line_put(&header, ext);
        mmgr_line_ch(&header, '/');
        mmgr_line_put(&header, ext);
        mmgr_line_put(&header, ".h");
        mmgr_line_finish(&header);
        mmgr_line_put(args->out, ",\"header\":");
        EMBED_CALL(cli_header_write, DefsCtx, .path = path, .out = args->out);
        mmgr_line_ch(args->out, '}');
        first = 0;
    }
    mmgr_line_put(args->out, "],\"head\":");
    EMBED_CALL(cli_head_write, DefsCtx, .root = args->root, .out = args->out);
    mmgr_line_put(args->out, ",\"languages\":[");
    VerbaLinea languages = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&languages, args->root);
    mmgr_line_put(&languages, "/src/lng");
    mmgr_line_finish(&languages);
    EMBED_CALL(cli_names_list, DefsCtx, .folder = folder, .folders = 0, .names = &names);
    first = 1;
    for (unsigned int at = 0u; at < names.count; at += 1u)
    {
        VerbaLinea table = {path, sizeof(path), 0ull};
        mmgr_line_put(&table, folder);
        mmgr_line_ch(&table, '/');
        mmgr_line_put(&table, names.name[at]);
        const unsigned long long begun = args->out->at;
        mmgr_line_put(args->out, first ? "" : ",");
        if ((mmgr_line_finish(&table) == 0ull) || !EMBED_CALL(cli_table_write, DefsCtx, .root = args->root,
                                                              .path = path, .name = names.name[at], .out = args->out))
        {
            args->out->at = begun;
            continue;
        }
        first = 0;
    }
    mmgr_line_put(args->out, "]}");
    g_cli_cells->persistent_buf_release(names.name, &g_cli_defs_cells);
}
