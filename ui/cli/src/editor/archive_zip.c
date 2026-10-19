// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_zip.c: a zip read and written: its directory found at its end, zip64's among them, each central record and
// its member's name, UTF-8 or the old DOS page; a member's bytes, stored or deflated and checked; and the zip written
// anew with one member changed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "archive_internal.h"

// the old DOS page's characters for the bytes from 128 up
static const unsigned int s_dos[128] = {
    0x00C7u, 0x00FCu, 0x00E9u, 0x00E2u, 0x00E4u, 0x00E0u, 0x00E5u, 0x00E7u, 0x00EAu, 0x00EBu, 0x00E8u, 0x00EFu, 0x00EEu,
    0x00ECu, 0x00C4u, 0x00C5u, 0x00C9u, 0x00E6u, 0x00C6u, 0x00F4u, 0x00F6u, 0x00F2u, 0x00FBu, 0x00F9u, 0x00FFu, 0x00D6u,
    0x00DCu, 0x00A2u, 0x00A3u, 0x00A5u, 0x20A7u, 0x0192u, 0x00E1u, 0x00EDu, 0x00F3u, 0x00FAu, 0x00F1u, 0x00D1u, 0x00AAu,
    0x00BAu, 0x00BFu, 0x2310u, 0x00ACu, 0x00BDu, 0x00BCu, 0x00A1u, 0x00ABu, 0x00BBu, 0x2591u, 0x2592u, 0x2593u, 0x2502u,
    0x2524u, 0x2561u, 0x2562u, 0x2556u, 0x2555u, 0x2563u, 0x2551u, 0x2557u, 0x255Du, 0x255Cu, 0x255Bu, 0x2510u, 0x2514u,
    0x2534u, 0x252Cu, 0x251Cu, 0x2500u, 0x253Cu, 0x255Eu, 0x255Fu, 0x255Au, 0x2554u, 0x2569u, 0x2566u, 0x2560u, 0x2550u,
    0x256Cu, 0x2567u, 0x2568u, 0x2564u, 0x2565u, 0x2559u, 0x2558u, 0x2552u, 0x2553u, 0x256Bu, 0x256Au, 0x2518u, 0x250Cu,
    0x2588u, 0x2584u, 0x258Cu, 0x2590u, 0x2580u, 0x03B1u, 0x00DFu, 0x0393u, 0x03C0u, 0x03A3u, 0x03C3u, 0x00B5u, 0x03C4u,
    0x03A6u, 0x0398u, 0x03A9u, 0x03B4u, 0x221Eu, 0x03C6u, 0x03B5u, 0x2229u, 0x2261u, 0x00B1u, 0x2265u, 0x2264u, 0x2320u,
    0x2321u, 0x00F7u, 0x2248u, 0x00B0u, 0x2219u, 0x00B7u, 0x221Au, 0x207Fu, 0x00B2u, 0x25A0u, 0x00A0u};

void cli_dos(const unsigned char *bytes, unsigned long long size, CliLine *out)
{
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        cli_utf8_put(out, (bytes[at] < 128u) ? bytes[at] : s_dos[bytes[at] - 128u]);
    }
}

// a zip's central records: where they start, and how many: 1, or 0 with why said
static int cli_zip_directory(const unsigned char *bytes, unsigned long long size, unsigned long long *start,
                             unsigned long long *count, unsigned long long *end, CliLine *said)
{
    const unsigned long long floor = (size > (22ull + 65535ull)) ? (size - 22ull - 65535ull) : 0ull;
    if (size < 22ull)
    {
        cli_line_text(said, "no zip's directory is at its end");
        return 0;
    }
    int found = 0;
    for (unsigned long long at = size - 22ull + 1ull; !found && (at > floor);)
    {
        at -= 1ull;
        unsigned long long mark = 0ull;
        if (cli_archive_number(bytes, size, at, 4u, &mark, said) && (mark == 0x06054b50ull))
        {
            *end = at;
            found = 1;
        }
        if (at == floor)
        {
            break;
        }
    }
    if (!found)
    {
        cli_line_text(said, "no zip's directory is at its end");
        return 0;
    }
    if (!cli_archive_number(bytes, size, *end + 10ull, 2u, count, said) ||
        !cli_archive_number(bytes, size, *end + 16ull, 4u, start, said))
    {
        return 0;
    }
    // a zip64's directory is named by a locator just before the end record
    unsigned long long locator = 0ull;
    if (((*count == 0xffffull) || (*start == 0xffffffffull)) && (*end >= 20ull) &&
        cli_archive_number(bytes, size, *end - 20ull, 4u, &locator, said) && (locator == 0x07064b50ull))
    {
        unsigned long long record = 0ull;
        unsigned long long mark = 0ull;
        if (!cli_archive_number(bytes, size, *end - 12ull, 8u, &record, said) ||
            !cli_archive_number(bytes, size, record, 4u, &mark, said))
        {
            return 0;
        }
        if ((mark == 0x06064b50ull) && (!cli_archive_number(bytes, size, record + 32ull, 8u, count, said) ||
                                        !cli_archive_number(bytes, size, record + 48ull, 8u, start, said)))
        {
            return 0;
        }
    }
    return 1;
}

// A zip's central record: its fields as the record and its zip64 extra give them
typedef struct
{
    unsigned long long flags;
    unsigned long long method;
    unsigned long long crc;
    unsigned long long packed;
    unsigned long long size;
    unsigned long long local;
    const unsigned char *name;
    unsigned long long name_length;
    unsigned long long length;
} CliCentral;

// the central record at `at`: 1, or 0 with why said
static int cli_central(const unsigned char *bytes, unsigned long long size, unsigned long long at, CliCentral *central,
                       CliLine *said)
{
    unsigned long long mark = 0ull;
    unsigned long long extra_length = 0ull;
    unsigned long long comment_length = 0ull;
    if (!cli_archive_number(bytes, size, at, 4u, &mark, said))
    {
        return 0;
    }
    if (mark != 0x02014b50ull)
    {
        cli_line_text(said, "a zip's directory record is broken");
        return 0;
    }
    if (!cli_archive_number(bytes, size, at + 28ull, 2u, &central->name_length, said) ||
        !cli_archive_number(bytes, size, at + 30ull, 2u, &extra_length, said) ||
        !cli_archive_number(bytes, size, at + 32ull, 2u, &comment_length, said) ||
        !cli_archive_number(bytes, size, at + 20ull, 4u, &central->packed, said) ||
        !cli_archive_number(bytes, size, at + 24ull, 4u, &central->size, said) ||
        !cli_archive_number(bytes, size, at + 42ull, 4u, &central->local, said) ||
        !cli_archive_number(bytes, size, at + 8ull, 2u, &central->flags, said) ||
        !cli_archive_number(bytes, size, at + 10ull, 2u, &central->method, said) ||
        !cli_archive_number(bytes, size, at + 16ull, 4u, &central->crc, said))
    {
        return 0;
    }
    if ((at + 46ull + central->name_length) > size)
    {
        cli_line_text(said, "a zip's name is cut");
        return 0;
    }
    central->name = bytes + at + 46ull;
    // the zip64 extra: the sizes and the local header's place each set to all ones are past their fields, in it
    const unsigned long long extra_at = at + 46ull + central->name_length;
    const unsigned long long extra_end = ((extra_at + extra_length) <= size) ? (extra_at + extra_length) : extra_at;
    unsigned long long field = extra_at;
    while ((field + 4ull) <= extra_end)
    {
        unsigned long long id = 0ull;
        unsigned long long length = 0ull;
        if (!cli_archive_number(bytes, extra_end, field, 2u, &id, said) ||
            !cli_archive_number(bytes, extra_end, field + 2ull, 2u, &length, said))
        {
            return 0;
        }
        if (id == 1ull)
        {
            unsigned long long value = field + 4ull;
            unsigned long long *const slots[3] = {&central->size, &central->packed, &central->local};
            for (unsigned int slot = 0u; slot < 3u; slot += 1u)
            {
                if (*slots[slot] != 0xffffffffull)
                {
                    continue;
                }
                if (!cli_archive_number(bytes, extra_end, value, 8u, slots[slot], said))
                {
                    return 0;
                }
                value += 8ull;
            }
        }
        field += 4ull + length;
    }
    central->length = 46ull + central->name_length + extra_length + comment_length;
    return 1;
}

int cli_zip_members(const unsigned char *bytes, unsigned long long size, CliMembers *members, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long count = 0ull;
    unsigned long long end = 0ull;
    if (!cli_zip_directory(bytes, size, &start, &count, &end, said))
    {
        return 0;
    }
    unsigned long long at = start;
    for (unsigned long long index = 0ull; index < count; index += 1ull)
    {
        CliCentral record;
        if (!cli_central(bytes, size, at, &record, said))
        {
            return 0;
        }
        const int utf8 = (record.flags & (1ull << 11u)) != 0ull;
        char text[CLI_PATH_LONGEST * 2u];
        CliLine line = {text, sizeof(text), 0ull};
        if (utf8)
        {
            cli_lossy(&line, record.name, record.name_length);
        }
        else
        {
            cli_dos(record.name, record.name_length, &line);
        }
        const unsigned long long length = cli_line_closed(&line);
        int ascii = 1;
        for (unsigned long long letter = 0ull; ascii && (letter < record.name_length); letter += 1ull)
        {
            ascii = record.name[letter] < 0x80u;
        }
        for (unsigned long long letter = 0ull; letter < length; letter += 1ull)
        {
            text[letter] = (text[letter] == '\\') ? '/' : text[letter];
        }
        const int dir = (length > 0ull) && (text[length - 1ull] == '/');
        const int plain = utf8 || ascii;
        const unsigned int name = cli_archive_name_kept(members, text, length);
        const unsigned int raw =
            plain ? 0u : cli_archive_name_kept(members, (const char *)record.name, record.name_length);
        cli_archive_member_put(members, name, raw, !plain, dir, record.size, at);
        at += record.length;
    }
    return 1;
}

int cli_zip_data(const unsigned char *bytes, unsigned long long size, const CliMembers *members,
                 const CliMember *member, unsigned char *out, unsigned long long room, unsigned long long *out_size,
                 CliLine *said)
{
    const char *const name = members->text + member->name;
    CliCentral record;
    if (!cli_central(bytes, size, member->at, &record, said))
    {
        return 0;
    }
    if ((record.flags & 1ull) != 0ull)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is encrypted");
        return 0;
    }
    unsigned long long mark = 0ull;
    unsigned long long name_length = 0ull;
    unsigned long long extra_length = 0ull;
    if (!cli_archive_number(bytes, size, record.local, 4u, &mark, said))
    {
        return 0;
    }
    if (mark != 0x04034b50ull)
    {
        cli_line_text(said, name);
        cli_line_text(said, "'s own header is broken");
        return 0;
    }
    if (!cli_archive_number(bytes, size, record.local + 26ull, 2u, &name_length, said) ||
        !cli_archive_number(bytes, size, record.local + 28ull, 2u, &extra_length, said))
    {
        return 0;
    }
    const unsigned long long start = record.local + 30ull + name_length + extra_length;
    if ((start > size) || (record.packed > (size - start)))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is cut");
        return 0;
    }
    const unsigned char *const packed = bytes + start;
    if (record.method == 0ull)
    {
        if (record.packed > room)
        {
            cli_line_text(said, name);
            cli_line_text(said, " is larger than orior holds");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = out, .src = packed, .bytes = record.packed});
        *out_size = record.packed;
    }
    else if (record.method == 8ull)
    {
        unsigned long long used = 0ull;
        if (!cli_inflate(packed, record.packed, out, room, out_size, &used, said))
        {
            return 0;
        }
    }
    else
    {
        cli_line_text(said, name);
        cli_line_text(said, " is packed by method ");
        cli_line_decimal(said, record.method, 1u);
        cli_line_text(said, ", which orior does not read");
        return 0;
    }
    if (cli_crc32(out, *out_size) != record.crc)
    {
        cli_line_text(said, name);
        cli_line_text(said, " does not check: its bytes are not those the zip holds");
        return 0;
    }
    return 1;
}

// a little-endian number of `width` bytes put at `at` of `bytes`
static void cli_put_number(unsigned char *bytes, unsigned long long at, unsigned long long value, unsigned int width)
{
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        // each byte of the number, its low eight bits
        bytes[at + byte] = (unsigned char)(value >> (8u * byte));
    }
}

int cli_zip_with(const unsigned char *bytes, unsigned long long size, const CliMember *target,
                 const unsigned char *data, unsigned long long data_size, unsigned char *out, unsigned long long room,
                 unsigned long long *out_size, CliLine *said)
{
    // the directory written again and a member deflated, a cell taken for the writing
    typedef struct
    {
        unsigned char directory[CLI_ARCHIVE_BYTES];
        unsigned char deflated[CLI_ARCHIVE_BYTES];
    } CliZipRoom;
    unsigned long long start = 0ull;
    unsigned long long count = 0ull;
    unsigned long long end = 0ull;
    if (!cli_zip_directory(bytes, size, &start, &count, &end, said))
    {
        return 0;
    }
    CliZipRoom *const writing = cli_cell_taken(sizeof(*writing), "a zip's writing");
    if (writing == NULL)
    {
        cli_line_text(said, "a zip's writing could not be taken");
        return 0;
    }
    unsigned long long used = 0ull;
    unsigned long long directory = 0ull;
    unsigned long long at = start;
    int done = 1;
    for (unsigned long long index = 0ull; done && (index < count); index += 1ull)
    {
        CliCentral record;
        done = cli_central(bytes, size, at, &record, said);
        if (done && ((record.packed > 0xffffffffull) || (record.size > 0xffffffffull) || (used > 0xffffffffull)))
        {
            cli_line_text(said, "the zip is too large for orior to write back");
            done = 0;
        }
        unsigned long long name_length = 0ull;
        unsigned long long extra_length = 0ull;
        done = done && cli_archive_number(bytes, size, record.local + 26ull, 2u, &name_length, said) &&
               cli_archive_number(bytes, size, record.local + 28ull, 2u, &extra_length, said);
        const unsigned long long head_length = 30ull + name_length + extra_length;
        if (done && ((record.local + head_length) > size))
        {
            cli_line_text(said, "a zip's own header is cut");
            done = 0;
        }
        if (!done)
        {
            break;
        }
        // the member's body: the changed one deflated where that is smaller, each other's as it was
        unsigned long long method = record.method;
        unsigned long long crc = record.crc;
        unsigned long long packed = record.packed;
        unsigned long long length = record.size;
        const unsigned char *body = NULL;
        if (at == target->at)
        {
            const unsigned long long deflated =
                cli_deflate(data, data_size, writing->deflated, sizeof(writing->deflated));
            const int smaller = (deflated != 0ull) && (deflated < data_size);
            method = smaller ? 8ull : 0ull;
            body = smaller ? writing->deflated : data;
            packed = smaller ? deflated : data_size;
            crc = cli_crc32(data, data_size);
            length = data_size;
        }
        else
        {
            const unsigned long long from = record.local + head_length;
            if ((from > size) || (record.packed > (size - from)))
            {
                cli_line_text(said, "a zip's data is cut");
                done = 0;
                break;
            }
            body = bytes + from;
        }
        // the local header as it was, its sizes and check set here, and no record after the data
        if ((used + head_length + packed) > room)
        {
            cli_line_text(said, "the zip is larger than orior holds");
            done = 0;
            break;
        }
        const unsigned long long offset = used;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = bytes + record.local, .bytes = head_length});
        unsigned long long flags = 0ull;
        cli_archive_number(bytes, size, record.local + 6ull, 2u, &flags, said);
        cli_put_number(out, used + 6ull, flags & ~(1ull << 3u), 2u);
        cli_put_number(out, used + 8ull, method, 2u);
        cli_put_number(out, used + 14ull, crc, 4u);
        cli_put_number(out, used + 18ull, packed, 4u);
        cli_put_number(out, used + 22ull, length, 4u);
        used += head_length;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = body, .bytes = packed});
        used += packed;
        if ((directory + record.length) > sizeof(writing->directory))
        {
            cli_line_text(said, "the zip's directory is larger than orior holds");
            done = 0;
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = writing->directory + directory, .src = bytes + at, .bytes = record.length});
        unsigned long long central_flags = 0ull;
        cli_archive_number(bytes, size, at + 8ull, 2u, &central_flags, said);
        cli_put_number(writing->directory, directory + 8ull, central_flags & ~(1ull << 3u), 2u);
        cli_put_number(writing->directory, directory + 10ull, method, 2u);
        cli_put_number(writing->directory, directory + 16ull, crc, 4u);
        cli_put_number(writing->directory, directory + 20ull, packed, 4u);
        cli_put_number(writing->directory, directory + 24ull, length, 4u);
        cli_put_number(writing->directory, directory + 42ull, offset, 4u);
        directory += record.length;
        at += record.length;
    }
    const unsigned long long directory_at = used;
    const unsigned long long tail = size - end;
    if (done && ((used + directory + tail) > room))
    {
        cli_line_text(said, "the zip is larger than orior holds");
        done = 0;
    }
    if (done)
    {
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = writing->directory, .bytes = directory});
        used += directory;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = bytes + end, .bytes = tail});
        cli_put_number(out, used + 8ull, count, 2u);
        cli_put_number(out, used + 10ull, count, 2u);
        cli_put_number(out, used + 12ull, directory, 4u);
        cli_put_number(out, used + 16ull, directory_at, 4u);
        used += tail;
        *out_size = used;
    }
    cli_cell_given(writing);
    return done;
}
