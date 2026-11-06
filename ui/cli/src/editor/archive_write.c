// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_write.c: a file within an archive saved: each archive the path runs through written anew with the file its
// path names changed, the outermost beside the old one and then put in its place
#include "../command_line/pool.h"
#include "archive_internal.h"

typedef struct
{
    unsigned int depth;
    const unsigned char *bytes;
    unsigned long long bytes_size;
    unsigned int kind;
    const char *name;
    const CliParts *parts;
    unsigned int from;
    const unsigned char *data;
    unsigned long long data_size;
    unsigned long long *size;
    VerbaLinea *err_msg;
} ArchiveWriteCtx;

// The bytes of an archive of `kind` named `name`, at `depth` among those the path runs through, with the member the
// parts from `from` lead to set to `data`, a path that runs on into an archive inside it changing that one, written
// into the level's `written`: 1 with their count into `size`, or 0 with why given
static int cli_replace(const ArchiveWriteCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned int kind = args->kind;
    const char *name = args->name;
    const CliParts *parts = args->parts;
    unsigned int from = args->from;
    const unsigned char *data = args->data;
    unsigned long long data_size = args->data_size;
    unsigned long long *size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    CliLevels *const levels = g_cli_archive_levels;
    if (kind == CLI_ARCHIVE_QRY)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is a run's record of what it did, and is not written back");
        return 0;
    }
    if (args->depth >= CLI_ARCHIVE_DEPTH)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " lies deeper in archives than orior_cli writes back");
        return 0;
    }
    char inner[CLI_NAME_MAX_LEN];
    unsigned long long unpacked = 0ULL;
    EMBED_CALL(cli_archive_name_out_internal.gz_inner, CliArchiveNameOutCtx, .name = name, .out_capacity = inner);
    if (!EMBED_CALL(cli_archive_bytes_size_err_msg_internal.members_get, CliArchiveBytesSizeErrMsgCtx, .bytes = bytes,
                    .size = args->bytes_size, .kind = kind, .inner = inner, .members = &levels->members[args->depth],
                    .unpacked = &levels->unpacked[args->depth], .unpacked_size = &unpacked, .err_msg = err_msg))
    {
        return 0;
    }
    for (unsigned int cut = parts->count; cut > from; cut -= 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_archive_out_internal.join, CliArchiveOutCtx, .parts_read = parts, .from = from, .to = cut,
                   .out_capacity = path);
        const CliMember *const member = EMBED_CALL(cli_archive_internal.file_by_name, CliArchiveCtx,
                                                   .members_read = &levels->members[args->depth], .name = path);
        if (member == NULL)
        {
            continue;
        }
        const unsigned char *changed = data;
        unsigned long long changed_size = data_size;
        if (cut != parts->count)
        {
            const unsigned int nested = EMBED_CALL(cli_archive.kind_get, CliArchiveCfg, .name = path);
            if (nested == CLI_ARCHIVE_NONE)
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, " is a file, and contains nothing");
                return 0;
            }
            if (args->depth >= (CLI_ARCHIVE_DEPTH - 1u))
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, " lies deeper in archives than orior_cli writes back");
                return 0;
            }
            unsigned long long nested_size = 0ULL;
            if (!EMBED_CALL(
                    cli_archive_bytes_internal.data_get, CliArchiveBytesCtx, .bytes = bytes, .size = args->bytes_size,
                    .kind = kind, .unpacked_bytes = levels->unpacked[args->depth].bytes,
                    .unpacked_size_value = unpacked, .members_read = &levels->members[args->depth], .member = member,
                    .out_buffer_pointer = &levels->inner[args->depth], .out_size = &nested_size, .err_msg = err_msg) ||
                !EMBED_CALL(cli_replace, ArchiveWriteCtx, .depth = args->depth + 1u,
                            .bytes = levels->inner[args->depth].bytes, .bytes_size = nested_size, .kind = nested,
                            .name = path, .parts = parts, .from = cut, .data = data, .data_size = data_size,
                            .size = &changed_size, .err_msg = err_msg))
            {
                return 0;
            }
            changed = levels->written[args->depth + 1u].bytes;
        }
        // the level written anew contains the archive as it was and the member changed, with an eighth and a page over
        CliByteBuffer *const written = &levels->written[args->depth];
        const unsigned long long whole = (kind == CLI_ARCHIVE_TARGZ) ? unpacked : args->bytes_size;
        if (!EMBED_CALL(cli_archive_err_msg_buffer_internal.capacity, CliArchiveErrMsgBufferCtx,
                        .buffer_pointer = written,
                        .need = EMBED_CALL(cli_archive_bytes_size_at_internal.grow, CliArchiveBytesSizeAtCtx,
                                           .size = whole + changed_size),
                        .what = "an archive written anew", .err_msg = err_msg))
        {
            return 0;
        }
        if (kind == CLI_ARCHIVE_ZIP)
        {
            return EMBED_CALL(cli_archive_bytes_size_internal.zip_with, CliArchiveBytesSizeCtx, .bytes = bytes,
                              .size = args->bytes_size, .target = member, .data = changed, .data_size = changed_size,
                              .out = written->bytes, .capacity = written->capacity, .out_size = size,
                              .err_msg = err_msg);
        }
        if (kind == CLI_ARCHIVE_TAR)
        {
            return EMBED_CALL(cli_archive_bytes_size_internal.tar_with, CliArchiveBytesSizeCtx, .bytes = bytes,
                              .size = args->bytes_size, .target = member, .data = changed, .data_size = changed_size,
                              .out = written->bytes, .capacity = written->capacity, .out_size = size,
                              .err_msg = err_msg);
        }
        unsigned long long tar_size = 0ULL;
        if ((kind == CLI_ARCHIVE_TARGZ) &&
            (!EMBED_CALL(cli_archive_err_msg_buffer_internal.capacity, CliArchiveErrMsgBufferCtx,
                         .buffer_pointer = &levels->tar,
                         .need = EMBED_CALL(cli_archive_bytes_size_at_internal.grow, CliArchiveBytesSizeAtCtx,
                                            .size = unpacked + changed_size),
                         .what = "a tar written anew", .err_msg = err_msg) ||
             !EMBED_CALL(cli_archive_bytes_size_internal.tar_with, CliArchiveBytesSizeCtx,
                         .bytes = levels->unpacked[args->depth].bytes, .size = unpacked, .target = member,
                         .data = changed, .data_size = changed_size, .out = levels->tar.bytes,
                         .capacity = levels->tar.capacity, .out_size = &tar_size, .err_msg = err_msg)))
        {
            return 0;
        }
        *size = (kind == CLI_ARCHIVE_TARGZ)
                    ? EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = levels->tar.bytes, .size = tar_size,
                                 .out = written->bytes, .capacity = written->capacity)
                    : EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = changed, .size = changed_size,
                                 .out = written->bytes, .capacity = written->capacity);
        if (*size == 0ULL)
        {
            mmgr_line_put(err_msg, name);
            mmgr_line_put(err_msg, " is larger than orior_cli supports");
            return 0;
        }
        return 1;
    }
    char required[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_archive_out_internal.join, CliArchiveOutCtx, .parts_read = parts, .from = from, .to = parts->count,
               .out_capacity = required);
    mmgr_line_put(err_msg, name);
    mmgr_line_put(err_msg, " contains no ");
    mmgr_line_put(err_msg, required);
    return 0;
}

int cli_archive_write(const CliArchiveSizeDataCfg *args)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_archive_err_msg_parts_internal.lock, CliArchiveErrMsgPartsCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_archive_err_msg_internal.open, CliArchiveErrMsgCtx, .root = args->root, .path = args->path,
                    .missing = " is in no archive", .descent = &descent, .parts = &s_parts, .file = file,
                    .err_msg = args->err_msg))
    {
        cli_archive_release();
        return 0;
    }
    unsigned long long written = 0ULL;
    int done = EMBED_CALL(cli_replace, ArchiveWriteCtx, .depth = 0u, .bytes = descent.bytes, .bytes_size = descent.size,
                          .kind = descent.kind, .name = descent.name, .parts = &s_parts, .from = 0u, .data = args->data,
                          .data_size = args->size, .size = &written, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)descent.bytes, (g_cli_editor_owner));
    // the archive written beside the old one as `.<name>.saving`, then put in its place
    char adjacent[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {adjacent, sizeof(adjacent), 0ULL};
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = file, .out = folder))
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, descent.name);
    mmgr_line_put(&line, ".saving");
    if (done && ((mmgr_line_finish(&line) == 0ULL) ||
                 !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = adjacent,
                             .bytes = g_cli_archive_levels->written[0].bytes, .size = written)))
    {
        mmgr_line_put(args->err_msg, adjacent);
        mmgr_line_put(args->err_msg, ": it could not be written");
        done = 0;
    }
    if (done && !EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = adjacent, .to = file))
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = adjacent);
        mmgr_line_put(args->err_msg, file);
        mmgr_line_put(args->err_msg, ": it could not be replaced");
        done = 0;
    }
    cli_archive_release();
    return done;
}
