// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_path.c: where a path of the tree runs into an archive, and on into one inside it; what a folder within an
// archive contains; and a file within one read
#include "../command_line/cells.h"
#include "../text/text.h"
#include "archive_internal.h"

CliLevels *g_cli_archive_levels;

// the capacities of the levels, in one list
#define CLI_ARCHIVE_BUFFERS ((3u * CLI_ARCHIVE_DEPTH) - 1u + 1u)

typedef struct
{
    unsigned int at;
    CliChildren *found;
    const char *bytes;
    unsigned long long length;
    const char *text;
    const char *one;
    const char *other;
    const CliDescent *descent;
    const CliParts *parts;
    const char *prefix;
} ArchivePathCtx;
EMBED_INLINE CliByteBuffer *cli_archive_buffers(const ArchivePathCtx *args)
{
    CliLevels *const levels = g_cli_archive_levels;
    return (args->at < CLI_ARCHIVE_DEPTH)                 ? &levels->unpacked[args->at]
           : (args->at < ((2u * CLI_ARCHIVE_DEPTH) - 1u)) ? &levels->inner[args->at - CLI_ARCHIVE_DEPTH]
           : (args->at < ((3u * CLI_ARCHIVE_DEPTH) - 1u)) ? &levels->written[args->at - ((2u * CLI_ARCHIVE_DEPTH) - 1u)]
                                                          : &levels->tar;
}

int cli_archive_lock(const CliArchiveErrMsgPartsCtx *args)
{
    g_cli_archive_levels = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_archive_levels), &g_cli_editor_cells);
    if (g_cli_archive_levels == NULL)
    {
        mmgr_line_put(args->err_msg, "the archives a path runs through could not be taken");
        return 0;
    }
    for (unsigned int at = 0u; at < CLI_ARCHIVE_BUFFERS; at += 1u)
    {
        *EMBED_CALL(cli_archive_buffers, ArchivePathCtx, .at = at) = (CliByteBuffer){NULL, 0ULL};
    }
    return 1;
}

void cli_archive_release(void)
{
    for (unsigned int at = 0u; at < CLI_ARCHIVE_BUFFERS; at += 1u)
    {
        g_cli_cells->persistent_buf_release(EMBED_CALL(cli_archive_buffers, ArchivePathCtx, .at = at)->bytes,
                                            &g_cli_editor_cells);
    }
    g_cli_cells->persistent_buf_release(g_cli_archive_levels, &g_cli_editor_cells);
    g_cli_archive_levels = NULL;
}

int cli_archive_buffer_reserve(const CliArchiveErrMsgBufferCtx *args)
{
    CliByteBuffer *buffer = args->buffer_pointer;
    if (args->need <= buffer->capacity)
    {
        return 1;
    }
    if (args->need > CLI_ARCHIVE_MAX)
    {
        mmgr_line_put(args->err_msg, args->what);
        mmgr_line_put(args->err_msg, " is larger than orior_cli supports");
        return 0;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(args->need, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, args->what);
        mmgr_line_put(args->err_msg, " could not be taken");
        return 0;
    }
    g_cli_cells->persistent_buf_release(buffer->bytes, &g_cli_editor_cells);
    buffer->bytes = cell;
    buffer->capacity = args->need;
    return 1;
}

unsigned long long cli_archive_grow(const CliArchiveBytesSizeAtCtx *args)
{
    return args->size + (args->size / 8ULL) + 4096ULL;
}

void cli_archive_parts_get(const CliArchiveErrMsgCtx *args)
{
    args->parts->count = 0u;
    for (const char *at = args->path; at[0] != '\0';)
    {
        const char *stop = at;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "/\\");
        if ((stop > at) && (args->parts->count < CLI_ARCHIVE_PARTS))
        {
            VerbaLinea line = {args->parts->part[args->parts->count], CLI_NAME_MAX_LEN, 0ULL};
            mmgr_line_put_n(&line, at, (unsigned long long)(stop - at));
            mmgr_line_finish(&line);
            args->parts->count += 1u;
        }
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
}

void cli_archive_join(const CliArchiveOutCtx *args)
{
    VerbaLinea line = {args->out_capacity, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        mmgr_line_put(&line, (at != args->from) ? "/" : "");
        mmgr_line_put(&line, args->parts_read->part[at]);
    }
    mmgr_line_finish(&line);
}

const CliMember *cli_archive_file_by_name(const CliArchiveCtx *args)
{
    for (unsigned int at = 0u; at < args->members_read->count; at += 1u)
    {
        if (!args->members_read->item[at].dir &&
            (cellul.eq(&(CatenaFinitaCfg){.src = args->members_read->text + args->members_read->item[at].name,
                                          .other = args->name,
                                          .cap = CLI_CELLS_POOL})))
        {
            return &args->members_read->item[at];
        }
    }
    return NULL;
}

int cli_archive_split(const CliArchiveRootCfg *args)
{
    char full[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->path, .joined = full,
                    .err_msg = &err_msg))
    {
        return 0;
    }
    // the nearest folder up the path that is there
    memor.cpy(&(MemoriaCfg){.dst = args->file,
                            .src = full,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
    while (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->file) &&
           !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->file))
    {
        char above[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->file, .out = above))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->file,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
        : (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                      .byte = '\\') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1)
            : args->file;
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->file) ||
        (EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = base) == CLI_ARCHIVE_NONE))
    {
        return 0;
    }
    const char *rest = full + cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL});
    rest += ((rest[0] == '/') || (rest[0] == '\\')) ? 1 : 0;
    memor.cpy(&(MemoriaCfg){.dst = args->inner,
                            .src = rest,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->inner,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->inner, .cap = CLI_CELLS_POOL}), .from = '\\',
               .to = '/');
    return 1;
}

int cli_archive_descend(const CliArchiveErrMsgPartsCtx *args)
{
    unsigned int buffer = 0u;
    for (;;)
    {
        char inner[CLI_NAME_MAX_LEN];
        unsigned long long unpacked = 0ULL;
        EMBED_CALL(cli_archive_name_out_internal.gz_inner, CliArchiveNameOutCtx, .name = args->descent->name,
                   .out_capacity = inner);
        if (!EMBED_CALL(cli_archive_bytes_size_err_msg_internal.members_get, CliArchiveBytesSizeErrMsgCtx,
                        .bytes = args->descent->bytes, .size = args->descent->size, .kind = args->descent->kind,
                        .inner = inner, .members = &g_cli_archive_levels->members[0],
                        .unpacked = &g_cli_archive_levels->unpacked[0], .unpacked_size = &unpacked,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        int deeper = 0;
        for (unsigned int cut = args->parts_read->count; !deeper && (cut > args->descent->left); cut -= 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_archive_out_internal.join, CliArchiveOutCtx, .parts_read = args->parts_read,
                       .from = args->descent->left, .to = cut, .out_capacity = path);
            const CliMember *const member = EMBED_CALL(cli_archive_internal.file_by_name, CliArchiveCtx,
                                                       .members_read = &g_cli_archive_levels->members[0], .name = path);
            const unsigned int nested = EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = path);
            if ((member == NULL) || (nested == CLI_ARCHIVE_NONE))
            {
                continue;
            }
            unsigned long long size = 0ULL;
            if (!EMBED_CALL(cli_archive_bytes_internal.data_get, CliArchiveBytesCtx, .bytes = args->descent->bytes,
                            .size = args->descent->size, .kind = args->descent->kind,
                            .unpacked_bytes = g_cli_archive_levels->unpacked[0].bytes, .unpacked_size_value = unpacked,
                            .members_read = &g_cli_archive_levels->members[0], .member = member,
                            .out_buffer_pointer = &g_cli_archive_levels->inner[buffer], .out_size = &size,
                            .err_msg = args->err_msg))
            {
                return 0;
            }
            args->descent->bytes = g_cli_archive_levels->inner[buffer].bytes;
            args->descent->size = size;
            args->descent->kind = nested;
            memor.cpy(&(MemoriaCfg){.dst = args->descent->name,
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            args->descent->left = cut;
            buffer = 1u - buffer;
            deeper = 1;
        }
        if (!deeper)
        {
            return 1;
        }
    }
}

// the hex of `length` bytes, two lower case digits each, put in the children's texts: its place
EMBED_INLINE unsigned int cli_hex_store(const ArchivePathCtx *args)
{
    static const char s_hex[17] = "0123456789abcdef";
    if ((args->found->used + (args->length * 2ULL) + 1ULL) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int stores
    const unsigned int at = (unsigned int)args->found->used;
    for (unsigned long long byte = 0ULL; byte < args->length; byte += 1ULL)
    {
        args->found->text[at + (byte * 2ULL)] = s_hex[(unsigned char)args->bytes[byte] >> 4u];
        args->found->text[at + (byte * 2ULL) + 1ULL] = s_hex[(unsigned char)args->bytes[byte] & 0x0fu];
    }
    args->found->text[at + (args->length * 2ULL)] = '\0';
    args->found->used += (args->length * 2ULL) + 1ULL;
    return at;
}

// a text put in the children's texts: its place
EMBED_INLINE unsigned int cli_text_store(const ArchivePathCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    if ((args->found->used + length + 1ULL) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int stores
    const unsigned int at = (unsigned int)args->found->used;
    memor.cpy(&(MemoriaCfg){.dst = args->found->text + at, .src = args->text, .bytes = length + 1ULL});
    args->found->used += length + 1ULL;
    return at;
}

// whether a text sorts before another, ASCII's case aside
EMBED_INLINE int cli_before(const ArchivePathCtx *args)
{
    const char *one = args->one;
    const char *other = args->other;
    for (;; one += 1, other += 1)
    {
        const char left = ((one[0] >= 'A') && (one[0] <= 'Z')) ? (char)(one[0] + 32) : one[0];
        const char right = ((other[0] >= 'A') && (other[0] <= 'Z')) ? (char)(other[0] + 32) : other[0];
        if ((left != right) || (left == '\0'))
        {
            return (unsigned char)left < (unsigned char)right;
        }
    }
}

// the children of the folder the path's parts from `left` lead to within the archive at `descent`, its members read,
// each path prefixed with `prefix`, folders first, then by name, case aside
EMBED_INLINE void cli_children(const ArchivePathCtx *args)
{
    const CliParts *parts = args->parts;
    const CliMembers *const members = &g_cli_archive_levels->members[0];
    const unsigned int depth = parts->count - args->descent->left;
    args->found->count = 0u;
    args->found->used = 1ULL;
    args->found->text[0] = '\0';
    for (unsigned int index = 0u; index < members->count; index += 1u)
    {
        const CliMember *const member = &members->item[index];
        CliParts own;
        EMBED_CALL(cli_archive_err_msg_internal.parts_get, CliArchiveErrMsgCtx, .path = members->text + member->name,
                   .parts = &own);
        int under = own.count > depth;
        {
            unsigned int at = 0u;
            while (under && (at < depth))
            {
                under = cellul.eq(&(CatenaFinitaCfg){
                    .src = own.part[at], .other = parts->part[args->descent->left + at], .cap = CLI_CELLS_POOL});
                at += 1u;
            }
        }
        if (!under)
        {
            continue;
        }
        const char *const child = own.part[depth];
        const int deeper = own.count > (depth + 1u);
        unsigned int seen = args->found->count;
        for (unsigned int at = 0u; (seen == args->found->count) && (at < args->found->count); at += 1u)
        {
            seen = (cellul.eq(&(CatenaFinitaCfg){
                       .src = args->found->text + args->found->item[at].name, .other = child, .cap = CLI_CELLS_POOL}))
                       ? at
                       : seen;
        }
        if (seen < args->found->count)
        {
            args->found->item[seen].dir = args->found->item[seen].dir || deeper || member->dir;
            continue;
        }
        if (args->found->count >= CLI_ARCHIVE_CHILDREN)
        {
            continue;
        }
        const int archive = !deeper && !member->dir &&
                            (EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = child) != CLI_ARCHIVE_NONE);
        CliChild *const one = &args->found->item[args->found->count];
        args->found->count += 1u;
        one->name = EMBED_CALL(cli_text_store, ArchivePathCtx, .found = args->found, .text = child);
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ULL};
        mmgr_line_put(&line, args->prefix);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, child);
        mmgr_line_finish(&line);
        one->path = EMBED_CALL(cli_text_store, ArchivePathCtx, .found = args->found, .text = path);
        one->dir = deeper || member->dir || archive;
        one->archive = archive;
        one->has_raw = member->has_raw && !deeper;
        one->raw = 0u;
        if (one->has_raw)
        {
            // the name's own bytes past its last slash, or all of them where it ends with one
            const char *const raw = members->text + member->raw;
            const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = raw, .cap = CLI_CELLS_POOL});
            unsigned long long end = length;
            end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = raw, .at = end,
                             .set = "/");
            unsigned long long start = end;
            start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = raw, .at = start,
                               .set = "/");
            one->raw = (end > start) ? EMBED_CALL(cli_hex_store, ArchivePathCtx, .found = args->found,
                                                  .bytes = raw + start, .length = end - start)
                                     : EMBED_CALL(cli_hex_store, ArchivePathCtx, .found = args->found, .bytes = raw,
                                                  .length = length);
        }
    }
    // folders first, then by name, case aside
    for (unsigned int at = 1u; at < args->found->count; at += 1u)
    {
        const CliChild swap = args->found->item[at];
        unsigned int place = at;
        while (place > 0u)
        {
            const CliChild *const before = &args->found->item[place - 1u];
            const int first = (swap.dir && !before->dir) ||
                              ((swap.dir == before->dir) &&
                               EMBED_CALL(cli_before, ArchivePathCtx, .one = args->found->text + swap.name,
                                          .other = args->found->text + before->name));
            if (!first)
            {
                break;
            }
            args->found->item[place] = *before;
            place -= 1u;
        }
        args->found->item[place] = swap;
    }
}

int cli_archive_open(const CliArchiveErrMsgCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    char *file = args->file;
    char inner[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = root, .path = path, .file = file,
                    .inner = inner))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, args->missing);
        return 0;
    }
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
        : (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}),
                      .byte = '\\') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1)
            : file;
    args->descent->kind = EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = base);
    args->descent->bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = file,
                                      .size_pointer = &args->descent->size, .owner = &g_cli_editor_cells);
    if (args->descent->bytes == NULL)
    {
        mmgr_line_put(args->err_msg, file);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->descent->name,
                            .src = base,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
    args->descent->left = 0u;
    EMBED_CALL(cli_archive_err_msg_internal.parts_get, CliArchiveErrMsgCtx, .path = inner, .parts = args->parts);
    return 1;
}

int cli_archive_list(const CliArchiveErrMsgRootCfg *args)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_archive_err_msg_parts_internal.lock, CliArchiveErrMsgPartsCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_archive_err_msg_internal.open, CliArchiveErrMsgCtx, .root = args->root, .path = args->path,
                    .missing = " is no archive", .descent = &descent, .parts = &s_parts, .file = file,
                    .err_msg = args->err_msg))
    {
        cli_archive_release();
        return 0;
    }
    const unsigned char *const mapped = descent.bytes;
    const int done = EMBED_CALL(cli_archive_err_msg_parts_internal.descend, CliArchiveErrMsgPartsCtx,
                                .descent = &descent, .parts_read = &s_parts, .err_msg = args->err_msg);
    if (done)
    {
        char prefix[CLI_PATH_MAX_LEN];
        memor.cpy(
            &(MemoriaCfg){.dst = prefix,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
        length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = prefix, .at = length,
                            .set = "/");
        prefix[length] = '\0';
        EMBED_CALL(cli_children, ArchivePathCtx, .descent = &descent, .parts = &s_parts, .prefix = prefix,
                   .found = args->found);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_editor_cells);
    cli_archive_release();
    return done;
}

char *cli_archive_alloc(const CliArchiveErrMsgCfg *args)
{
    unsigned long long *size = args->size_pointer;
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_archive_err_msg_parts_internal.lock, CliArchiveErrMsgPartsCtx, .err_msg = args->err_msg))
    {
        return NULL;
    }
    if (!EMBED_CALL(cli_archive_err_msg_internal.open, CliArchiveErrMsgCtx, .root = args->root, .path = args->path,
                    .missing = " is in no archive", .descent = &descent, .parts = &s_parts, .file = file,
                    .err_msg = args->err_msg))
    {
        cli_archive_release();
        return NULL;
    }
    const unsigned char *const mapped = descent.bytes;
    int done = EMBED_CALL(cli_archive_err_msg_parts_internal.descend, CliArchiveErrMsgPartsCtx, .descent = &descent,
                          .parts_read = &s_parts, .err_msg = args->err_msg);
    char required[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_archive_out_internal.join, CliArchiveOutCtx, .parts_read = &s_parts, .from = descent.left,
               .to = s_parts.count, .out_capacity = required);
    char inner[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_archive_name_out_internal.gz_inner, CliArchiveNameOutCtx, .name = descent.name,
               .out_capacity = inner);
    unsigned long long unpacked = 0ULL;
    done =
        done && EMBED_CALL(cli_archive_bytes_size_err_msg_internal.members_get, CliArchiveBytesSizeErrMsgCtx,
                           .bytes = descent.bytes, .size = descent.size, .kind = descent.kind, .inner = inner,
                           .members = &g_cli_archive_levels->members[0], .unpacked = &g_cli_archive_levels->unpacked[0],
                           .unpacked_size = &unpacked, .err_msg = args->err_msg);
    const CliMember *const member =
        done ? EMBED_CALL(cli_archive_internal.file_by_name, CliArchiveCtx,
                          .members_read = &g_cli_archive_levels->members[0], .name = required)
             : NULL;
    if (done && (member == NULL))
    {
        mmgr_line_put(args->err_msg, descent.name);
        mmgr_line_put(args->err_msg, " contains no ");
        mmgr_line_put(args->err_msg, required);
        done = 0;
    }
    unsigned long long data_size = 0ULL;
    done = done && EMBED_CALL(cli_archive_bytes_internal.data_get, CliArchiveBytesCtx, .bytes = descent.bytes,
                              .size = descent.size, .kind = descent.kind,
                              .unpacked_bytes = g_cli_archive_levels->unpacked[0].bytes,
                              .unpacked_size_value = unpacked, .members_read = &g_cli_archive_levels->members[0],
                              .member = member, .out_buffer_pointer = &g_cli_archive_levels->written[0],
                              .out_size = &data_size, .err_msg = args->err_msg);
    char *const cell = done ? g_cli_cells->temporary_buf_alloc(data_size + 1ULL, args->owner) : NULL;
    if (cell != NULL)
    {
        // a member unpacked whole, which a size_t counts
        memor.cpy(
            &(MemoriaCfg){.dst = cell, .src = g_cli_archive_levels->written[0].bytes, .bytes = (size_t)data_size});
        cell[data_size] = '\0';
        *size = data_size;
    }
    else if (done)
    {
        mmgr_line_put(args->err_msg, "the file read out of the archive could not be stored");
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_editor_cells);
    cli_archive_release();
    return cell;
}
