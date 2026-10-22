// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive.c: the kind of archive a file is, an archive's members read by its kind, and a member's bytes
#include "../runs/qry.h"
#include "../text/text.h"
#include "archive_internal.h"

// the most names of a .qry's blobs read
#define CLI_QRY_NAMES 4096u

unsigned int cli_archive_kind_of(const char *name)
{
    static const char *const s_zips[10] = {".zip", ".jar",   ".war",  ".ear", ".whl",
                                           ".apk", ".nupkg", ".vsix", ".xpi", ".epub"};
    char lower[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    for (; (name[length] != '\0') && (length < (sizeof(lower) - 1u)); length += 1ull)
    {
        const char letter = name[length];
        lower[length] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
    }
    lower[length] = '\0';
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_zips[at], .cap = CLI_CELLS_POOL});
        if ((length >= size) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = lower + length - size, .other = s_zips[at], .cap = CLI_CELLS_POOL})))
        {
            return CLI_ARCHIVE_ZIP;
        }
    }
    const struct
    {
        const char *end;
        unsigned int kind;
    } s_ends[5] = {{".qry", CLI_ARCHIVE_QRY},
                   {".tar.gz", CLI_ARCHIVE_TARGZ},
                   {".tgz", CLI_ARCHIVE_TARGZ},
                   {".tar", CLI_ARCHIVE_TAR},
                   {".gz", CLI_ARCHIVE_GZ}};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_ends[at].end, .cap = CLI_CELLS_POOL});
        if ((length >= size) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = lower + length - size, .other = s_ends[at].end, .cap = CLI_CELLS_POOL})))
        {
            return s_ends[at].kind;
        }
    }
    return CLI_ARCHIVE_NONE;
}

int cli_archive_number(const unsigned char *bytes, unsigned long long size, unsigned long long at, unsigned int width,
                       unsigned long long *value, CliLine *said)
{
    if ((at > size) || (width > (size - at)))
    {
        cli_line_text(said, "the archive is cut");
        return 0;
    }
    *value = 0ull;
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        *value |= (unsigned long long)bytes[at + byte] << (8u * byte);
    }
    return 1;
}

unsigned int cli_archive_name_kept(CliMembers *members, const char *name, unsigned long long length)
{
    // a leading `./` and `/` and a trailing `/` left off
    for (;;)
    {
        if ((length >= 2ull) && (name[0] == '.') && (name[1] == '/'))
        {
            name += 2;
            length -= 2ull;
        }
        else
        {
            break;
        }
    }
    while ((length > 0ull) && (name[0] == '/'))
    {
        name += 1;
        length -= 1ull;
    }
    while ((length > 0ull) && (name[length - 1ull] == '/'))
    {
        length -= 1ull;
    }
    if ((members->used + length + 1ull) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int holds
    const unsigned int at = (unsigned int)members->used;
    memor.cpy(&(MemoriaCfg){.dst = members->text + at, .src = name, .bytes = length});
    members->text[at + length] = '\0';
    members->used += length + 1ull;
    return at;
}

void cli_archive_member_put(CliMembers *members, unsigned int name, unsigned int raw, int has_raw, int dir,
                            unsigned long long size, unsigned long long at)
{
    if (members->count >= CLI_ARCHIVE_MEMBERS)
    {
        return;
    }
    members->item[members->count] = (CliMember){name, raw, has_raw, dir, size, at};
    members->count += 1u;
}

void cli_gz_inner(const char *name, char *out)
{
    const char *const base =
        (cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '/') + 1)
            : name;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    const int gz =
        (length >= 3ull) &&
        ((cellul.eq(&(CatenaFinitaCfg){.src = base + length - 3ull, .other = ".gz", .cap = CLI_CELLS_POOL})) ||
         (cellul.eq(&(CatenaFinitaCfg){.src = base + length - 3ull, .other = ".GZ", .cap = CLI_CELLS_POOL})));
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    cli_line_counted(&line, base, gz ? (length - 3ull) : length);
    cli_line_closed(&line);
}

int cli_members_of(const unsigned char *bytes, unsigned long long size, unsigned int kind, const char *inner,
                   CliMembers *members, CliRoom *unpacked, unsigned long long *unpacked_size, CliLine *said)
{
    static char s_names[CLI_QRY_NAMES][CLI_NAME_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    members->count = 0u;
    members->used = 1ull;
    members->text[0] = '\0';
    *unpacked_size = 0ull;
    if (kind == CLI_ARCHIVE_ZIP)
    {
        return cli_zip_members(bytes, size, members, said);
    }
    if (kind == CLI_ARCHIVE_TAR)
    {
        return cli_tar_members(bytes, size, members, said);
    }
    if ((kind == CLI_ARCHIVE_TARGZ) || (kind == CLI_ARCHIVE_GZ))
    {
        // a gzip's trailer ends with the low 32 bits of what it holds unpacked
        unsigned long long told = 0ull;
        if ((size < 18ull) || !cli_archive_number(bytes, size, size - 4ull, 4u, &told, said))
        {
            cli_line_text(said, (size < 18ull) ? "a gzip is cut" : "");
            return 0;
        }
        if (!cli_archive_room(unpacked, told + 1ull, "an archive unpacked", said) ||
            !cli_gunzip(bytes, size, unpacked->bytes, unpacked->room, unpacked_size, said))
        {
            return 0;
        }
        if (kind == CLI_ARCHIVE_TARGZ)
        {
            return cli_tar_members(unpacked->bytes, *unpacked_size, members, said);
        }
        cli_archive_member_put(
            members,
            cli_archive_name_kept(members, inner, cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_CELLS_POOL})),
            0u, 0, 0, *unpacked_size, 0ull);
        return 1;
    }
    // each name's blobs joined, the name a member of its own
    cli_lock_take(&s_lock);
    const unsigned int count = cli_qry_member_names(bytes, size, s_names, CLI_QRY_NAMES);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_archive_member_put(
            members,
            cli_archive_name_kept(members, s_names[at],
                                  cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL})),
            0u, 0, 0, 0ull, 0ull);
    }
    cli_lock_give(&s_lock);
    return 1;
}

int cli_data_of(const unsigned char *bytes, unsigned long long size, unsigned int kind, const unsigned char *unpacked,
                unsigned long long unpacked_size, const CliMembers *members, const CliMember *member, CliRoom *out,
                unsigned long long *out_size, CliLine *said)
{
    const char *const name = members->text + member->name;
    const unsigned long long length = (kind == CLI_ARCHIVE_GZ)    ? unpacked_size
                                      : (kind == CLI_ARCHIVE_QRY) ? cli_qry_member_size(bytes, size, name)
                                                                  : member->size;
    if (!cli_archive_room(out, length + 1ull, "a member of an archive", said))
    {
        return 0;
    }
    if (kind == CLI_ARCHIVE_ZIP)
    {
        return cli_zip_data(bytes, size, members, member, out->bytes, out->room, out_size, said);
    }
    if (kind == CLI_ARCHIVE_QRY)
    {
        CliLine line = {(char *)out->bytes, out->room, 0ull};
        cli_qry_member_joined(bytes, size, name, &line);
        if (!cli_line_fits(&line))
        {
            cli_line_text(said, name);
            cli_line_text(said, " is larger than orior_cli holds");
            return 0;
        }
        *out_size = line.at;
        return 1;
    }
    const unsigned char *const from = (kind == CLI_ARCHIVE_TAR) ? bytes : unpacked;
    const unsigned long long from_size = (kind == CLI_ARCHIVE_TAR) ? size : unpacked_size;
    const unsigned long long start = (kind == CLI_ARCHIVE_GZ) ? 0ull : (member->at + 512ull);
    if ((start > from_size) || (length > (from_size - start)))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is cut");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = out->bytes, .src = from + start, .bytes = length});
    *out_size = length;
    return 1;
}
