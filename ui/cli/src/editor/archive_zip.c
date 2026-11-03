// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_zip.c: a zip read and written: its directory found at its end, zip64's among them, each central record and
// its member's name, UTF-8 or the old DOS page; a member's bytes, stored or deflated and checked; and the zip written
// anew with one member changed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "archive_internal.h"

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

void cli_dos(const CliArchiveBytesSizeCfg *args)
{
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = args->out_verba_linea_pointer,
                   .letter = (args->bytes[at] < 128u) ? args->bytes[at] : s_dos[args->bytes[at] - 128u]);
    }
}

typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long *start;
    unsigned long long *count;
    unsigned long long *end;
    VerbaLinea *err_msg;
    unsigned long long at;
    CliCentral *central;
    unsigned char *bytes_out;
    unsigned long long value;
    unsigned int width;
} ArchiveZipCtx;
// a zip's central records: where they start, and how many: 1, or 0 with why given
static int cli_zip_directory(const ArchiveZipCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long *end = args->end;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned long long floor = (size > (22ULL + 65535ULL)) ? (size - 22ULL - 65535ULL) : 0ULL;
    if (size < 22ULL)
    {
        mmgr_line_put(err_msg, "no zip's directory is at its end");
        return 0;
    }
    int found = 0;
    for (unsigned long long at = size - 22ULL + 1ULL; !found && (at > floor);)
    {
        at -= 1ULL;
        unsigned long long mark = 0ULL;
        if (EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                       .size = size, .at = at, .width = 4u, .value = &mark, .err_msg = err_msg) &&
            (mark == 0x06054b50ULL))
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
        mmgr_line_put(err_msg, "no zip's directory is at its end");
        return 0;
    }
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = *end + 10ULL, .width = 2u, .value = args->count, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = *end + 16ULL, .width = 4u, .value = args->start, .err_msg = err_msg))
    {
        return 0;
    }
    // a zip64's directory is named by a locator just before the end record
    unsigned long long locator = 0ULL;
    if (((*args->count == 0xffffULL) || (*args->start == 0xffffffffULL)) && (*end >= 20ULL) &&
        EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                   .at = *end - 20ULL, .width = 4u, .value = &locator, .err_msg = err_msg) &&
        (locator == 0x07064b50ULL))
    {
        unsigned long long record = 0ULL;
        unsigned long long mark = 0ULL;
        if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                        .size = size, .at = *end - 12ULL, .width = 8u, .value = &record, .err_msg = err_msg) ||
            !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                        .size = size, .at = record, .width = 4u, .value = &mark, .err_msg = err_msg))
        {
            return 0;
        }
        if ((mark == 0x06064b50ULL) &&
            (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                         .size = size, .at = record + 32ULL, .width = 8u, .value = args->count, .err_msg = err_msg) ||
             !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                         .size = size, .at = record + 48ULL, .width = 8u, .value = args->start, .err_msg = err_msg)))
        {
            return 0;
        }
    }
    return 1;
}

// the central record at `at`: 1, or 0 with why given
static int cli_central(const ArchiveZipCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long at = args->at;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long mark = 0ULL;
    unsigned long long extra_length = 0ULL;
    unsigned long long comment_length = 0ULL;
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at, .width = 4u, .value = &mark, .err_msg = err_msg))
    {
        return 0;
    }
    if (mark != 0x02014b50ULL)
    {
        mmgr_line_put(err_msg, "a zip's directory record is broken");
        return 0;
    }
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 28ULL, .width = 2u, .value = &args->central->name_length, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 30ULL, .width = 2u, .value = &extra_length, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 32ULL, .width = 2u, .value = &comment_length, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 20ULL, .width = 4u, .value = &args->central->packed, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 24ULL, .width = 4u, .value = &args->central->size, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 42ULL, .width = 4u, .value = &args->central->local, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 8ULL, .width = 2u, .value = &args->central->flags, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 10ULL, .width = 2u, .value = &args->central->method, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = at + 16ULL, .width = 4u, .value = &args->central->crc, .err_msg = err_msg))
    {
        return 0;
    }
    if ((at + 46ULL + args->central->name_length) > size)
    {
        mmgr_line_put(err_msg, "a zip's name is cut");
        return 0;
    }
    args->central->name = bytes + at + 46ULL;
    // the zip64 extra: the sizes and the local header's place each set to all ones are past their fields, in it
    const unsigned long long extra_at = at + 46ULL + args->central->name_length;
    const unsigned long long extra_end = ((extra_at + extra_length) <= size) ? (extra_at + extra_length) : extra_at;
    unsigned long long field = extra_at;
    while ((field + 4ULL) <= extra_end)
    {
        unsigned long long id = 0ULL;
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                        .size = extra_end, .at = field, .width = 2u, .value = &id, .err_msg = err_msg) ||
            !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                        .size = extra_end, .at = field + 2ULL, .width = 2u, .value = &length, .err_msg = err_msg))
        {
            return 0;
        }
        if (id == 1ULL)
        {
            unsigned long long value = field + 4ULL;
            unsigned long long *const fields[3] = {&args->central->size, &args->central->packed, &args->central->local};
            for (unsigned int index = 0u; index < 3u; index += 1u)
            {
                if (*fields[index] != 0xffffffffULL)
                {
                    continue;
                }
                if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                                .size = extra_end, .at = value, .width = 8u, .value = fields[index],
                                .err_msg = err_msg))
                {
                    return 0;
                }
                value += 8ULL;
            }
        }
        field += 4ULL + length;
    }
    args->central->length = 46ULL + args->central->name_length + extra_length + comment_length;
    return 1;
}

int cli_zip_members(const CliArchiveBytesSizeErrMsgCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long start = 0ULL;
    unsigned long long count = 0ULL;
    unsigned long long end = 0ULL;
    if (!EMBED_CALL(cli_zip_directory, ArchiveZipCtx, .bytes = bytes, .size = size, .start = &start, .count = &count,
                    .end = &end, .err_msg = args->err_msg))
    {
        return 0;
    }
    unsigned long long at = start;
    for (unsigned long long index = 0ULL; index < count; index += 1ULL)
    {
        CliCentral record;
        if (!EMBED_CALL(cli_central, ArchiveZipCtx, .bytes = bytes, .size = size, .at = at, .central = &record,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        const int utf8 = (record.flags & (1ULL << 11u)) != 0ULL;
        char text[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        if (utf8)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line, .text_bytes = record.name,
                       .length = record.name_length);
        }
        else
        {
            EMBED_CALL(cli_archive_bytes_size.dos, CliArchiveBytesSizeCfg, .bytes = record.name,
                       .size = record.name_length, .out_verba_linea_pointer = &line);
        }
        const unsigned long long length = mmgr_line_finish(&line);
        int ascii = 1;
        for (unsigned long long letter = 0ULL; ascii && (letter < record.name_length); letter += 1ULL)
        {
            ascii = record.name[letter] < 0x80u;
        }
        for (unsigned long long letter = 0ULL; letter < length; letter += 1ULL)
        {
            text[letter] = (text[letter] == '\\') ? '/' : text[letter];
        }
        const int dir = (length > 0ULL) && (text[length - 1ULL] == '/');
        const int plain = utf8 || ascii;
        const unsigned int name = EMBED_CALL(cli_archive_members_internal.name_store, CliArchiveMembersCtx,
                                             .members = args->members, .name = text, .length = length);
        const unsigned int raw =
            plain ? 0u
                  : EMBED_CALL(cli_archive_members_internal.name_store, CliArchiveMembersCtx, .members = args->members,
                               .name = (const char *)record.name, .length = record.name_length);
        EMBED_CALL(cli_archive_size_internal.member_append, CliArchiveSizeCtx, .members = args->members,
                   .name_u32 = name, .raw = raw, .has_raw = !plain, .dir = dir, .size = record.size, .at = at);
        at += record.length;
    }
    return 1;
}

int cli_zip_data(const CliArchiveBytesSizeErrMsgMembersCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned char *out = args->out;
    unsigned long long capacity = args->capacity;
    unsigned long long *out_size = args->out_size;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = args->members_read->text + args->member->name;
    CliCentral record;
    if (!EMBED_CALL(cli_central, ArchiveZipCtx, .bytes = bytes, .size = size, .at = args->member->at,
                    .central = &record, .err_msg = err_msg))
    {
        return 0;
    }
    if ((record.flags & 1ULL) != 0ULL)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is encrypted");
        return 0;
    }
    unsigned long long mark = 0ULL;
    unsigned long long name_length = 0ULL;
    unsigned long long extra_length = 0ULL;
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = record.local, .width = 4u, .value = &mark, .err_msg = err_msg))
    {
        return 0;
    }
    if (mark != 0x04034b50ULL)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, "'s own header is broken");
        return 0;
    }
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = record.local + 26ULL, .width = 2u, .value = &name_length, .err_msg = err_msg) ||
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                    .at = record.local + 28ULL, .width = 2u, .value = &extra_length, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned long long start = record.local + 30ULL + name_length + extra_length;
    if ((start > size) || (record.packed > (size - start)))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is cut");
        return 0;
    }
    const unsigned char *const packed = bytes + start;
    if (record.method == 0ULL)
    {
        if (record.packed > capacity)
        {
            mmgr_line_put(err_msg, name);
            mmgr_line_put(err_msg, " is larger than orior_cli supports");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = out, .src = packed, .bytes = record.packed});
        *out_size = record.packed;
    }
    else if (record.method == 8ULL)
    {
        unsigned long long used = 0ULL;
        if (!EMBED_CALL(cli_archive_size.inflate, CliArchiveSizeCfg, .data = packed, .size = record.packed, .out = out,
                        .capacity = capacity, .out_size = out_size, .used = &used, .err_msg = err_msg))
        {
            return 0;
        }
    }
    else
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is packed by method ");
        mmgr_line_decimal(err_msg, record.method, 1u);
        mmgr_line_put(err_msg, ", which orior_cli does not read");
        return 0;
    }
    if (EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = out, .size = *out_size) != record.crc)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " does not check: its bytes are not those the zip contains");
        return 0;
    }
    return 1;
}

// a little-endian number of `width` bytes put at `at` of `bytes`
EMBED_INLINE void cli_append_number(const ArchiveZipCtx *args)
{
    for (unsigned int byte = 0u; byte < args->width; byte += 1u)
    {
        // each byte of the number, its low eight bits
        args->bytes_out[args->at + byte] = (unsigned char)(args->value >> (8u * byte));
    }
}

int cli_zip_with(const CliArchiveBytesSizeCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    const unsigned char *data = args->data;
    unsigned char *out = args->out;
    unsigned long long capacity = args->capacity;
    unsigned long long *out_size = args->out_size;
    unsigned long long start = 0ULL;
    unsigned long long count = 0ULL;
    unsigned long long end = 0ULL;
    if (!EMBED_CALL(cli_zip_directory, ArchiveZipCtx, .bytes = bytes, .size = size, .start = &start, .count = &count,
                    .end = &end, .err_msg = args->err_msg))
    {
        return 0;
    }
    // the directory written again, as long as it was, and the member changed deflated, which is retained only where it
    // is smaller than the member: one cell for the writing, the directory first
    const unsigned long long directory_capacity = end - start;
    const unsigned long long deflated_capacity = args->data_size;
    unsigned char *const writing =
        g_cli_cells->temporary_buf_alloc(directory_capacity + deflated_capacity + 1ULL, &g_cli_editor_cells);
    if (writing == NULL)
    {
        mmgr_line_put(args->err_msg, "a zip's writing could not be taken");
        return 0;
    }
    unsigned char *const written_directory = writing;
    unsigned char *const written_deflated = writing + directory_capacity;
    unsigned long long used = 0ULL;
    unsigned long long directory = 0ULL;
    unsigned long long at = start;
    int done = 1;
    for (unsigned long long index = 0ULL; done && (index < count); index += 1ULL)
    {
        CliCentral record;
        done = EMBED_CALL(cli_central, ArchiveZipCtx, .bytes = bytes, .size = size, .at = at, .central = &record,
                          .err_msg = args->err_msg);
        if (done && ((record.packed > 0xffffffffULL) || (record.size > 0xffffffffULL) || (used > 0xffffffffULL)))
        {
            mmgr_line_put(args->err_msg, "the zip is too large for orior_cli to write back");
            done = 0;
        }
        unsigned long long name_length = 0ULL;
        unsigned long long extra_length = 0ULL;
        done = done &&
               EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                          .size = size, .at = record.local + 26ULL, .width = 2u, .value = &name_length,
                          .err_msg = args->err_msg) &&
               EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                          .size = size, .at = record.local + 28ULL, .width = 2u, .value = &extra_length,
                          .err_msg = args->err_msg);
        const unsigned long long head_length = 30ULL + name_length + extra_length;
        if (done && ((record.local + head_length) > size))
        {
            mmgr_line_put(args->err_msg, "a zip's own header is cut");
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
        if (at == args->target->at)
        {
            const unsigned long long deflated =
                EMBED_CALL(cli_archive_size.deflate, CliArchiveSizeCfg, .data = data, .size = args->data_size,
                           .out = written_deflated, .capacity = deflated_capacity);
            const int smaller = (deflated != 0ULL) && (deflated < args->data_size);
            method = smaller ? 8ULL : 0ULL;
            body = smaller ? written_deflated : data;
            packed = smaller ? deflated : args->data_size;
            crc = EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = data,
                             .size = args->data_size);
            length = args->data_size;
        }
        else
        {
            const unsigned long long from = record.local + head_length;
            if ((from > size) || (record.packed > (size - from)))
            {
                mmgr_line_put(args->err_msg, "a zip's data is cut");
                done = 0;
                break;
            }
            body = bytes + from;
        }
        // the local header as it was, its sizes and check set here, and no record after the data
        if ((used + head_length + packed) > capacity)
        {
            mmgr_line_put(args->err_msg, "the zip is larger than orior_cli supports");
            done = 0;
            break;
        }
        const unsigned long long offset = used;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = bytes + record.local, .bytes = head_length});
        unsigned long long flags = 0ULL;
        EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                   .at = record.local + 6ULL, .width = 2u, .value = &flags, .err_msg = args->err_msg);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 6ULL,
                   .value = flags & ~(1ULL << 3u), .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 8ULL, .value = method, .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 14ULL, .value = crc, .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 18ULL, .value = packed,
                   .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 22ULL, .value = length,
                   .width = 4u);
        used += head_length;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = body, .bytes = packed});
        used += packed;
        if ((directory + record.length) > directory_capacity)
        {
            mmgr_line_put(args->err_msg, "the zip's directory is larger than orior_cli supports");
            done = 0;
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = written_directory + directory, .src = bytes + at, .bytes = record.length});
        unsigned long long central_flags = 0ULL;
        EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes, .size = size,
                   .at = at + 8ULL, .width = 2u, .value = &central_flags, .err_msg = args->err_msg);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 8ULL,
                   .value = central_flags & ~(1ULL << 3u), .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 10ULL,
                   .value = method, .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 16ULL,
                   .value = crc, .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 20ULL,
                   .value = packed, .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 24ULL,
                   .value = length, .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = written_directory, .at = directory + 42ULL,
                   .value = offset, .width = 4u);
        directory += record.length;
        at += record.length;
    }
    const unsigned long long directory_at = used;
    const unsigned long long tail = size - end;
    if (done && ((used + directory + tail) > capacity))
    {
        mmgr_line_put(args->err_msg, "the zip is larger than orior_cli supports");
        done = 0;
    }
    if (done)
    {
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = written_directory, .bytes = directory});
        used += directory;
        memor.cpy(&(MemoriaCfg){.dst = out + used, .src = bytes + end, .bytes = tail});
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 8ULL, .value = count, .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 10ULL, .value = count, .width = 2u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 12ULL, .value = directory,
                   .width = 4u);
        EMBED_CALL(cli_append_number, ArchiveZipCtx, .bytes_out = out, .at = used + 16ULL, .value = directory_at,
                   .width = 4u);
        used += tail;
        *out_size = used;
    }
    g_cli_cells->persistent_buf_release(writing, &g_cli_editor_cells);
    return done;
}
