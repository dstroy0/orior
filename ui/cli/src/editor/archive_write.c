// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_write.c: a file within an archive saved: each archive the path runs through written anew with the file its
// path names changed, the outermost beside the old one and then put in its place
#include "archive_internal.h"

// The bytes of an archive of `kind` named `name`, at `depth` among those the path runs through, with the member the
// parts from `from` lead to set to `data`, a path that runs on into an archive inside it changing that one, written
// into the level's `written`: 1 with their count into `size`, or 0 with why said
static int cli_replace(unsigned int depth, const unsigned char *bytes, unsigned long long bytes_size, unsigned int kind,
                       const char *name, const CliParts *parts, unsigned int from, const unsigned char *data,
                       unsigned long long data_size, unsigned long long *size, CliLine *said)
{
    CliLevels *const levels = g_cli_archive_levels;
    if (kind == CLI_ARCHIVE_QRY)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is a run's record of what it did, and is not written back");
        return 0;
    }
    if (depth >= CLI_ARCHIVE_DEPTH)
    {
        cli_line_text(said, name);
        cli_line_text(said, " lies deeper in archives than orior writes back");
        return 0;
    }
    char inner[CLI_NAME_LONGEST];
    unsigned long long unpacked = 0ull;
    cli_gz_inner(name, inner);
    if (!cli_members_of(bytes, bytes_size, kind, inner, &levels->members[depth], levels->unpacked[depth], &unpacked,
                        said))
    {
        return 0;
    }
    for (unsigned int cut = parts->count; cut > from; cut -= 1u)
    {
        char path[CLI_PATH_LONGEST];
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
                cli_line_text(said, path);
                cli_line_text(said, " is a file, and holds nothing");
                return 0;
            }
            if (depth >= (CLI_ARCHIVE_DEPTH - 1u))
            {
                cli_line_text(said, path);
                cli_line_text(said, " lies deeper in archives than orior writes back");
                return 0;
            }
            unsigned long long nested_size = 0ull;
            if (!cli_data_of(bytes, bytes_size, kind, levels->unpacked[depth], unpacked, &levels->members[depth],
                             member, levels->inner[depth], CLI_ARCHIVE_BYTES, &nested_size, said) ||
                !cli_replace(depth + 1u, levels->inner[depth], nested_size, nested, path, parts, cut, data, data_size,
                             &changed_size, said))
            {
                return 0;
            }
            changed = levels->written[depth + 1u];
        }
        if (kind == CLI_ARCHIVE_ZIP)
        {
            return cli_zip_with(bytes, bytes_size, member, changed, changed_size, levels->written[depth],
                                CLI_ARCHIVE_BYTES, size, said);
        }
        if (kind == CLI_ARCHIVE_TAR)
        {
            return cli_tar_with(bytes, bytes_size, member, changed, changed_size, levels->written[depth],
                                CLI_ARCHIVE_BYTES, size, said);
        }
        unsigned long long tar_size = 0ull;
        if ((kind == CLI_ARCHIVE_TARGZ) && !cli_tar_with(levels->unpacked[depth], unpacked, member, changed,
                                                         changed_size, levels->tar, CLI_ARCHIVE_BYTES, &tar_size, said))
        {
            return 0;
        }
        *size = (kind == CLI_ARCHIVE_TARGZ)
                    ? cli_gzip(levels->tar, tar_size, levels->written[depth], CLI_ARCHIVE_BYTES)
                    : cli_gzip(changed, changed_size, levels->written[depth], CLI_ARCHIVE_BYTES);
        if (*size == 0ull)
        {
            cli_line_text(said, name);
            cli_line_text(said, " is larger than orior holds");
            return 0;
        }
        return 1;
    }
    char wanted[CLI_PATH_LONGEST];
    cli_archive_joined(parts, from, parts->count, wanted);
    cli_line_text(said, name);
    cli_line_text(said, " holds no ");
    cli_line_text(said, wanted);
    return 0;
}

int cli_archive_write(const char *root, const char *path, const unsigned char *data, unsigned long long size,
                      CliLine *said)
{
    static CliParts s_parts;
    CliDescent descent;
    char file[CLI_PATH_LONGEST];
    if (!cli_archive_held(said))
    {
        return 0;
    }
    if (!cli_archive_opened(root, path, " is in no archive", &descent, &s_parts, file, said))
    {
        cli_archive_released();
        return 0;
    }
    unsigned long long written = 0ull;
    int done = cli_replace(0u, descent.bytes, descent.size, descent.kind, descent.name, &s_parts, 0u, data, size,
                           &written, said);
    qry_member_file_unmapped(descent.bytes, descent.size);
    // the archive written beside the old one as `.<name>.saving`, then put in its place
    char beside[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    CliLine line = {beside, sizeof(beside), 0ull};
    if (cli_path_parent(file, folder))
    {
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
    }
    cli_line_character(&line, '.');
    cli_line_text(&line, descent.name);
    cli_line_text(&line, ".saving");
    if (done &&
        ((cli_line_closed(&line) == 0ull) || !query_file_written(beside, g_cli_archive_levels->written[0], written)))
    {
        cli_line_text(said, beside);
        cli_line_text(said, ": it could not be written");
        done = 0;
    }
    if (done && !query_file_renamed(beside, file))
    {
        query_removed(beside);
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be replaced");
        done = 0;
    }
    cli_archive_released();
    return done;
}
