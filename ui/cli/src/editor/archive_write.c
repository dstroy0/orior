// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_write.c: a file within an archive saved: each archive the path runs through written anew with the file its
// path names changed, the outermost beside the old one and then put in its place
#include "archive_internal.h"

// The bytes of an archive of `kind` named `name`, at `depth` among those the path runs through, with the member the
// parts from `from` lead to set to `data`, a path that runs on into an archive inside it changing that one, written
// into the level's `written`: 1 with their count into `size`, or 0 with why given
static int cli_replace(unsigned int depth, const unsigned char *bytes, unsigned long long bytes_size, unsigned int kind,
                       const char *name, const CliParts *parts, unsigned int from, const unsigned char *data,
                       unsigned long long data_size, unsigned long long *size, VerbaLinea *err_msg)
{
    CliLevels *const levels = g_cli_archive_levels;
    if (kind == CLI_ARCHIVE_QRY)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is a run's record of what it did, and is not written back");
        return 0;
    }
    if (depth >= CLI_ARCHIVE_DEPTH)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " lies deeper in archives than orior_cli writes back");
        return 0;
    }
    char inner[CLI_NAME_MAX_LEN];
    unsigned long long unpacked = 0ull;
    cli_gz_inner(name, inner);
    if (!cli_members_of(bytes, bytes_size, kind, inner, &levels->members[depth], &levels->unpacked[depth], &unpacked,
                        err_msg))
    {
        return 0;
    }
    for (unsigned int cut = parts->count; cut > from; cut -= 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        cli_archive_joined(parts, from, cut, path);
        const CliMember *const member = cli_archive_file_named(&levels->members[depth], path);
        if (member == NULL)
        {
            continue;
        }
        const unsigned char *changed = data;
        unsigned long long changed_size = data_size;
        if (cut != parts->count)
        {
            const unsigned int nested = cli_archive_kind_of(path);
            if (nested == CLI_ARCHIVE_NONE)
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, " is a file, and contains nothing");
                return 0;
            }
            if (depth >= (CLI_ARCHIVE_DEPTH - 1u))
            {
                mmgr_line_put(err_msg, path);
                mmgr_line_put(err_msg, " lies deeper in archives than orior_cli writes back");
                return 0;
            }
            unsigned long long nested_size = 0ull;
            if (!cli_data_of(bytes, bytes_size, kind, levels->unpacked[depth].bytes, unpacked, &levels->members[depth],
                             member, &levels->inner[depth], &nested_size, err_msg) ||
                !cli_replace(depth + 1u, levels->inner[depth].bytes, nested_size, nested, path, parts, cut, data,
                             data_size, &changed_size, err_msg))
            {
                return 0;
            }
            changed = levels->written[depth + 1u].bytes;
        }
        // the level written anew contains the archive as it was and the member changed, with an eighth and a page over
        CliRoom *const written = &levels->written[depth];
        const unsigned long long whole = (kind == CLI_ARCHIVE_TARGZ) ? unpacked : bytes_size;
        if (!cli_archive_room(written, cli_archive_grown(whole + changed_size), "an archive written anew", err_msg))
        {
            return 0;
        }
        if (kind == CLI_ARCHIVE_ZIP)
        {
            return cli_zip_with(bytes, bytes_size, member, changed, changed_size, written->bytes, written->room, size,
                                err_msg);
        }
        if (kind == CLI_ARCHIVE_TAR)
        {
            return cli_tar_with(bytes, bytes_size, member, changed, changed_size, written->bytes, written->room, size,
                                err_msg);
        }
        unsigned long long tar_size = 0ull;
        if ((kind == CLI_ARCHIVE_TARGZ) &&
            (!cli_archive_room(&levels->tar, cli_archive_grown(unpacked + changed_size), "a tar written anew",
                               err_msg) ||
             !cli_tar_with(levels->unpacked[depth].bytes, unpacked, member, changed, changed_size, levels->tar.bytes,
                           levels->tar.room, &tar_size, err_msg)))
        {
            return 0;
        }
        *size = (kind == CLI_ARCHIVE_TARGZ) ? cli_gzip(levels->tar.bytes, tar_size, written->bytes, written->room)
                                            : cli_gzip(changed, changed_size, written->bytes, written->room);
        if (*size == 0ull)
        {
            mmgr_line_put(err_msg, name);
            mmgr_line_put(err_msg, " is larger than orior_cli supports");
            return 0;
        }
        return 1;
    }
    char wanted[CLI_PATH_MAX_LEN];
    cli_archive_joined(parts, from, parts->count, wanted);
    mmgr_line_put(err_msg, name);
    mmgr_line_put(err_msg, " contains no ");
    mmgr_line_put(err_msg, wanted);
    return 0;
}

int cli_archive_write(const char *root, const char *path, const unsigned char *data, unsigned long long size,
                      VerbaLinea *err_msg)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_MAX_LEN];
    if (!cli_archive_locked(err_msg))
    {
        return 0;
    }
    if (!cli_archive_opened(root, path, " is in no archive", &descent, &s_parts, file, err_msg))
    {
        cli_archive_released();
        return 0;
    }
    unsigned long long written = 0ull;
    int done = cli_replace(0u, descent.bytes, descent.size, descent.kind, descent.name, &s_parts, 0u, data, size,
                           &written, err_msg);
    cli_file_let_go(descent.bytes, descent.size);
    // the archive written beside the old one as `.<name>.saving`, then put in its place
    char beside[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {beside, sizeof(beside), 0ull};
    if (cli_path_parent(file, folder))
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, descent.name);
    mmgr_line_put(&line, ".saving");
    if (done && ((mmgr_line_finish(&line) == 0ull) ||
                 !query_file_written(beside, g_cli_archive_levels->written[0].bytes, written)))
    {
        mmgr_line_put(err_msg, beside);
        mmgr_line_put(err_msg, ": it could not be written");
        done = 0;
    }
    if (done && !query_file_renamed(beside, file))
    {
        query_removed(beside);
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, ": it could not be replaced");
        done = 0;
    }
    cli_archive_released();
    return done;
}
