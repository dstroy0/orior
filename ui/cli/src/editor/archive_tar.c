// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_tar.c: a tar read and written, its long names by GNU's and by pax's, and a gzip read and written
#include "../text/json_tree.h"
#include "archive_internal.h"

// a tar member's size, in octal, or in base 256 where its first byte's top bit is set
static unsigned long long cli_tar_size(const unsigned char *field, unsigned int length)
{
    if ((length > 0u) && ((field[0] & 0x80u) != 0u))
    {
        unsigned long long size = 0ull;
        for (unsigned int at = 1u; at < length; at += 1u)
        {
            size = (size << 8u) | field[at];
        }
        return size;
    }
    unsigned long long size = 0ull;
    int digits = 0;
    for (unsigned int at = 0u; (at < length) && (field[at] != 0u) && (field[at] != ' '); at += 1u)
    {
        if ((field[at] < '0') || (field[at] > '7'))
        {
            return 0ull;
        }
        size = (size << 3u) | (unsigned long long)(field[at] - '0');
        digits = 1;
    }
    return digits ? size : 0ull;
}

// a tar header's text field up to its first 0, read as UTF-8 with what is no letter as U+FFFD, after `out`
static void cli_tar_text(const unsigned char *field, unsigned int length, CliLine *out)
{
    unsigned int end = 0u;
    while ((end < length) && (field[end] != 0u))
    {
        end += 1u;
    }
    cli_lossy(out, field, end);
}

int cli_tar_members(const unsigned char *bytes, unsigned long long size, CliMembers *members, CliLine *said)
{
    static char s_long[CLI_PATH_LONGEST * 4u];
    unsigned long long at = 0ull;
    int has_long = 0;
    while ((at + 512ull) <= size)
    {
        const unsigned char *const header = bytes + at;
        int zeros = 1;
        for (unsigned int byte = 0u; zeros && (byte < 512u); byte += 1u)
        {
            zeros = header[byte] == 0u;
        }
        if (zeros)
        {
            break;
        }
        const unsigned long long length = cli_tar_size(header + 124, 12u);
        const unsigned char kind = header[156];
        const unsigned long long data = at + 512ull;
        const unsigned long long next = data + (((length + 511ull) / 512ull) * 512ull);
        if ((kind == 'L') || (kind == 'x'))
        {
            if ((data > size) || (length > (size - data)))
            {
                cli_line_text(said, (kind == 'L') ? "a tar's long name is cut" : "a tar's extended header is cut");
                return 0;
            }
            if (kind == 'L')
            {
                CliLine line = {s_long, sizeof(s_long), 0ull};
                cli_tar_text(bytes + data, (length < 0xffffffffull) ? (unsigned int)length : 0xffffffffu, &line);
                cli_line_closed(&line);
                has_long = 1;
            }
            else
            {
                // each `size path=value` record of the extended header, the path the next member's name
                const unsigned char *record = bytes + data;
                const unsigned char *const end = bytes + data + length;
                while (record < end)
                {
                    const unsigned char *stop = record;
                    while ((stop < end) && (stop[0] != '\n'))
                    {
                        stop += 1;
                    }
                    const unsigned char *space = record;
                    while ((space < stop) && (space[0] != ' '))
                    {
                        space += 1;
                    }
                    if (((space + 6) <= stop) &&
                        (memor.cmp(&(MemoriaCfg){.src = space + 1, .other = "path=", .bytes = 5u}) == 0))
                    {
                        CliLine line = {s_long, sizeof(s_long), 0ull};
                        cli_lossy(&line, space + 6, (unsigned long long)(stop - (space + 6)));
                        cli_line_closed(&line);
                        has_long = 1;
                    }
                    record = (stop < end) ? (stop + 1) : stop;
                }
            }
        }
        else if ((kind == '0') || (kind == 0u) || (kind == '5') || (kind == '7'))
        {
            char name[CLI_PATH_LONGEST * 4u];
            CliLine line = {name, sizeof(name), 0ull};
            if (has_long)
            {
                cli_line_text(&line, s_long);
                has_long = 0;
            }
            else
            {
                char prefix[CLI_PATH_LONGEST];
                CliLine prefixed = {prefix, sizeof(prefix), 0ull};
                cli_tar_text(header + 345, 155u, &prefixed);
                cli_line_closed(&prefixed);
                if ((memor.cmp(&(MemoriaCfg){.src = header + 257, .other = "ustar", .bytes = 5u}) == 0) &&
                    (prefix[0] != '\0'))
                {
                    cli_line_text(&line, prefix);
                    cli_line_character(&line, '/');
                }
                cli_tar_text(header, 100u, &line);
            }
            const unsigned long long name_length = cli_line_closed(&line);
            const int dir = (kind == '5') || ((name_length > 0ull) && (name[name_length - 1ull] == '/'));
            cli_archive_member_put(members, cli_archive_name_kept(members, name, name_length), 0u, 0, dir, length, at);
        }
        else
        {
            has_long = 0;
        }
        at = next;
    }
    return 1;
}

int cli_tar_with(const unsigned char *bytes, unsigned long long size, const CliMember *target,
                 const unsigned char *data, unsigned long long data_size, unsigned char *out, unsigned long long room,
                 unsigned long long *out_size, CliLine *said)
{
    const unsigned long long padded = ((data_size + 511ull) / 512ull) * 512ull;
    const unsigned long long after = target->at + 512ull + (((target->size + 511ull) / 512ull) * 512ull);
    const unsigned long long rest = (after < size) ? (size - after) : 0ull;
    if (((target->at + 512ull) > size) || ((target->at + 512ull + padded + rest) > room))
    {
        cli_line_text(said, "the tar is larger than orior_cli holds");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = out, .src = bytes, .bytes = target->at});
    unsigned char *const header = out + target->at;
    memor.cpy(&(MemoriaCfg){.dst = header, .src = bytes + target->at, .bytes = 512u});
    // the size as eleven octal digits and a 0, and the check as six octal digits, a 0 and a space
    char field[16];
    CliLine sized = {field, sizeof(field), 0ull};
    unsigned long long digits = 1ull;
    for (unsigned long long probe = data_size >> 3u; probe != 0ull; probe >>= 3u)
    {
        digits += 1ull;
    }
    for (unsigned long long pad = digits; pad < 11ull; pad += 1ull)
    {
        cli_line_character(&sized, '0');
    }
    for (unsigned long long place = digits; place > 0ull; place -= 1ull)
    {
        cli_line_character(&sized, (char)('0' + ((data_size >> (3ull * (place - 1ull))) & 7ull)));
    }
    memor.cpy(&(MemoriaCfg){.dst = header + 124, .src = field, .bytes = (sized.at < 11ull) ? sized.at : 11ull});
    header[124 + 11] = 0u;
    memor.set(&(MemoriaCfg){.dst = header + 148, .val = ' ', .bytes = 8u});
    unsigned long long sum = 0ull;
    for (unsigned int byte = 0u; byte < 512u; byte += 1u)
    {
        sum += header[byte];
    }
    for (unsigned int place = 6u; place > 0u; place -= 1u)
    {
        // an octal digit, below 8
        header[148 + 6 - place] = (unsigned char)('0' + ((sum >> (3u * (place - 1u))) & 7ull));
    }
    header[154] = 0u;
    header[155] = ' ';
    unsigned long long used = target->at + 512ull;
    memor.cpy(&(MemoriaCfg){.dst = out + used, .src = data, .bytes = data_size});
    memor.set(&(MemoriaCfg){.dst = out + used + data_size, .val = 0, .bytes = padded - data_size});
    used += padded;
    memor.cpy(&(MemoriaCfg){.dst = out + used, .src = bytes + after, .bytes = rest});
    *out_size = used + rest;
    return 1;
}

int cli_gunzip(const unsigned char *bytes, unsigned long long size, unsigned char *out, unsigned long long room,
               unsigned long long *out_size, CliLine *said)
{
    if ((size < 3ull) || (bytes[0] != 0x1fu) || (bytes[1] != 0x8bu) || (bytes[2] != 8u))
    {
        cli_line_text(said, "not a gzip");
        return 0;
    }
    if (size < 10ull)
    {
        cli_line_text(said, "the archive is cut");
        return 0;
    }
    const unsigned char flags = bytes[3];
    unsigned long long at = 10ull;
    unsigned long long extra = 0ull;
    if (((flags & 4u) != 0u) && !cli_archive_number(bytes, size, at, 2u, &extra, said))
    {
        return 0;
    }
    at += ((flags & 4u) != 0u) ? (2ull + extra) : 0ull;
    static const unsigned char s_named[2] = {8u, 16u};
    for (unsigned int flag = 0u; flag < 2u; flag += 1u)
    {
        if ((flags & s_named[flag]) == 0u)
        {
            continue;
        }
        while ((at < size) && (bytes[at] != 0u))
        {
            at += 1ull;
        }
        if (at >= size)
        {
            cli_line_text(said, "a gzip's header is cut");
            return 0;
        }
        at += 1ull;
    }
    at += ((flags & 2u) != 0u) ? 2ull : 0ull;
    if (at > size)
    {
        cli_line_text(said, "a gzip is cut");
        return 0;
    }
    unsigned long long used = 0ull;
    if (!cli_inflate(bytes + at, size - at, out, room, out_size, &used, said))
    {
        return 0;
    }
    unsigned long long check = 0ull;
    if (!cli_archive_number(bytes, size, at + used, 4u, &check, said))
    {
        return 0;
    }
    if (check != cli_crc32(out, *out_size))
    {
        cli_line_text(said, "the gzip does not check");
        return 0;
    }
    return 1;
}

unsigned long long cli_gzip(const unsigned char *data, unsigned long long size, unsigned char *out,
                            unsigned long long room)
{
    static const unsigned char s_head[10] = {0x1fu, 0x8bu, 8u, 0u, 0u, 0u, 0u, 0u, 0u, 255u};
    if (room < 18ull)
    {
        return 0ull;
    }
    memor.cpy(&(MemoriaCfg){.dst = out, .src = s_head, .bytes = sizeof(s_head)});
    const unsigned long long deflated = cli_deflate(data, size, out + 10, room - 18ull);
    if ((deflated == 0ull) && (size != 0ull))
    {
        return 0ull;
    }
    const unsigned int crc = cli_crc32(data, size);
    unsigned long long at = 10ull + deflated;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // each byte of the check, and of the size's low 32 bits, its low eight bits
        out[at + byte] = (unsigned char)(crc >> (8u * byte));
        out[at + 4u + byte] = (unsigned char)(size >> (8u * byte));
    }
    at += 8ull;
    return at;
}
