// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_tar.c: a tar read and written, its long names by GNU's and by pax's, and a gzip read and written
#include "../text/json_tree.h"
#include "../text/text.h"
#include "archive_internal.h"

typedef struct
{
    const unsigned char *field;
    unsigned int length;
    VerbaLinea *out;
    unsigned long long size;
    unsigned char kind;
    unsigned long long at;
    const char *long_name;
    CliMembers *members;
} ArchiveTarCtx;

// a tar member's size, in octal, or in base 256 where its first byte's top bit is set
EMBED_INLINE unsigned long long cli_tar_size(const ArchiveTarCtx *args)
{
    if ((args->length > 0u) && ((args->field[0] & 0x80u) != 0u))
    {
        unsigned long long size = 0ULL;
        for (unsigned int at = 1u; at < args->length; at += 1u)
        {
            size = (size << 8u) | args->field[at];
        }
        return size;
    }
    unsigned long long size = 0ULL;
    int digits = 0;
    for (unsigned int at = 0u; (at < args->length) && (args->field[at] != 0u) && (args->field[at] != ' '); at += 1u)
    {
        if ((args->field[at] < '0') || (args->field[at] > '7'))
        {
            return 0ULL;
        }
        size = (size << 3u) | (unsigned long long)(args->field[at] - '0');
        digits = 1;
    }
    return digits ? size : 0ULL;
}

// a tar header's text field up to its first 0, read as UTF-8 with what is no letter as U+FFFD, after `out`
EMBED_INLINE void cli_tar_text(const ArchiveTarCtx *args)
{
    unsigned int length = args->length;
    VerbaLinea *out = args->out;
    unsigned int end = 0u;
    const unsigned char *const zero = memor.chr(&(MemoriaCfg){.src = args->field, .val = 0, .bytes = length});
    // a 0 found lies in the field, whose length an unsigned int stores
    end = (zero != NULL) ? (unsigned int)(zero - args->field) : length;
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = out, .text_bytes = args->field,
               .length = end);
}

// whether the `length` bytes at `field` are each 0
EMBED_INLINE int cli_tar_zeros(const ArchiveTarCtx *args)
{
    for (unsigned int byte = 0u; byte < args->length; byte += 1u)
    {
        if (args->field[byte] != 0u)
        {
            return 0;
        }
    }
    return 1;
}

// the path a pax extended header gives, the `size` bytes at `field`: each `size path=value` record's value read into
// `out` over the one before it; 1 where a record gives one
EMBED_INLINE int cli_tar_pax_path(const ArchiveTarCtx *args)
{
    int found = 0;
    const unsigned char *record = args->field;
    const unsigned char *const end = args->field + args->size;
    while (record < end)
    {
        const unsigned char *stop = record;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)stop,
                           .at = 0ULL, .size = (unsigned long long)(end - stop), .set = "\n");
        const unsigned char *space = record;
        space += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)space,
                            .at = 0ULL, .size = (unsigned long long)(stop - space), .set = " ");
        if (((space + 6) <= stop) && (memor.cmp(&(MemoriaCfg){.src = space + 1, .other = "path=", .bytes = 5u}) == 0))
        {
            args->out->at = 0ULL;
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = space + 6, .length = (unsigned long long)(stop - (space + 6)));
            mmgr_line_finish(args->out);
            found = 1;
        }
        record = (stop < end) ? (stop + 1) : stop;
    }
    return found;
}

// the name the next member goes by, read into `out` from the `size` bytes at `field` of a GNU long name member, `kind`
// 'L', or of a pax extended header, `kind` 'x': 1 where it gives one
EMBED_INLINE int cli_tar_long_read(const ArchiveTarCtx *args)
{
    if (args->kind == 'L')
    {
        EMBED_CALL(cli_tar_text, ArchiveTarCtx, .field = args->field,
                   .length = (args->size < 0xffffffffULL) ? (unsigned int)args->size : 0xffffffffu, .out = args->out);
        mmgr_line_finish(args->out);
        return 1;
    }
    return EMBED_CALL(cli_tar_pax_path, ArchiveTarCtx, .field = args->field, .size = args->size, .out = args->out);
}

// the member whose header is at `field`, of `kind`, `size` bytes, the header `at` its place in the tar, put among
// `members`: named `long_name` where one went before it, else by its header, behind the ustar prefix it gives
EMBED_INLINE void cli_tar_member_put(const ArchiveTarCtx *args)
{
    const unsigned char *const header = args->field;
    char name[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea line = {name, sizeof(name), 0ULL};
    if (args->long_name != NULL)
    {
        mmgr_line_put(&line, args->long_name);
    }
    else
    {
        char prefix[CLI_PATH_MAX_LEN];
        VerbaLinea prefixed = {prefix, sizeof(prefix), 0ULL};
        EMBED_CALL(cli_tar_text, ArchiveTarCtx, .field = header + 345, .length = 155u, .out = &prefixed);
        mmgr_line_finish(&prefixed);
        if ((memor.cmp(&(MemoriaCfg){.src = header + 257, .other = "ustar", .bytes = 5u}) == 0) && (prefix[0] != '\0'))
        {
            mmgr_line_put(&line, prefix);
            mmgr_line_ch(&line, '/');
        }
        EMBED_CALL(cli_tar_text, ArchiveTarCtx, .field = header, .length = 100u, .out = &line);
    }
    const unsigned long long name_length = mmgr_line_finish(&line);
    const int dir = (args->kind == '5') || ((name_length > 0ULL) && (name[name_length - 1ULL] == '/'));
    EMBED_CALL(cli_archive_size_internal.member_append, CliArchiveSizeCtx, .members = args->members,
               .name_u32 = EMBED_CALL(cli_archive_members_internal.name_store, CliArchiveMembersCtx,
                                      .members = args->members, .name = name, .length = name_length),
               .raw = 0u, .has_raw = 0, .dir = dir, .size = args->size, .at = args->at);
}

int cli_tar_members(const CliArchiveBytesSizeErrMsgCtx *args)
{
    const unsigned char *bytes = args->bytes;
    static char s_long[CLI_PATH_MAX_LEN * 4u];
    unsigned long long at = 0ULL;
    int has_long = 0;
    while ((at + 512ULL) <= args->size)
    {
        const unsigned char *const header = bytes + at;
        if (EMBED_CALL(cli_tar_zeros, ArchiveTarCtx, .field = header, .length = 512u))
        {
            break;
        }
        const unsigned long long length = EMBED_CALL(cli_tar_size, ArchiveTarCtx, .field = header + 124, .length = 12u);
        const unsigned char kind = header[156];
        const unsigned long long data = at + 512ULL;
        const unsigned long long next = data + (((length + 511ULL) / 512ULL) * 512ULL);
        const int naming = (kind == 'L') || (kind == 'x');
        if (naming && ((data > args->size) || (length > (args->size - data))))
        {
            mmgr_line_put(args->err_msg, (kind == 'L') ? "a tar's long name is cut" : "a tar's extended header is cut");
            return 0;
        }
        if (naming)
        {
            VerbaLinea line = {s_long, sizeof(s_long), 0ULL};
            has_long = EMBED_CALL(cli_tar_long_read, ArchiveTarCtx, .kind = kind, .field = bytes + data, .size = length,
                                  .out = &line) ||
                       has_long;
        }
        else if ((kind == '0') || (kind == 0u) || (kind == '5') || (kind == '7'))
        {
            EMBED_CALL(cli_tar_member_put, ArchiveTarCtx, .field = header, .kind = kind, .size = length, .at = at,
                       .members = args->members, .long_name = has_long ? s_long : NULL);
            has_long = 0;
        }
        else
        {
            has_long = 0;
        }
        at = next;
    }
    return 1;
}

int cli_tar_with(const CliArchiveBytesSizeCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long *out_size = args->out_size;
    const unsigned long long padded = ((args->data_size + 511ULL) / 512ULL) * 512ULL;
    const unsigned long long after = args->target->at + 512ULL + (((args->target->size + 511ULL) / 512ULL) * 512ULL);
    const unsigned long long rest = (after < args->size) ? (args->size - after) : 0ULL;
    if (((args->target->at + 512ULL) > args->size) || ((args->target->at + 512ULL + padded + rest) > args->capacity))
    {
        mmgr_line_put(args->err_msg, "the tar is larger than orior_cli supports");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->out, .src = bytes, .bytes = args->target->at});
    unsigned char *const header = args->out + args->target->at;
    memor.cpy(&(MemoriaCfg){.dst = header, .src = bytes + args->target->at, .bytes = 512u});
    // the size as eleven octal digits and a 0, and the check as six octal digits, a 0 and a space
    char field[16];
    VerbaLinea sized = {field, sizeof(field), 0ULL};
    unsigned long long digits = 1ULL;
    for (unsigned long long probe = args->data_size >> 3u; probe != 0ULL; probe >>= 3u)
    {
        digits += 1ULL;
    }
    for (unsigned long long pad = digits; pad < 11ULL; pad += 1ULL)
    {
        mmgr_line_ch(&sized, '0');
    }
    for (unsigned long long place = digits; place > 0ULL; place -= 1ULL)
    {
        mmgr_line_ch(&sized, (char)('0' + ((args->data_size >> (3ULL * (place - 1ULL))) & 7ULL)));
    }
    memor.cpy(&(MemoriaCfg){.dst = header + 124, .src = field, .bytes = (sized.at < 11ULL) ? sized.at : 11ULL});
    header[124 + 11] = 0u;
    memor.set(&(MemoriaCfg){.dst = header + 148, .val = ' ', .bytes = 8u});
    unsigned long long sum = 0ULL;
    for (unsigned int byte = 0u; byte < 512u; byte += 1u)
    {
        sum += header[byte];
    }
    for (unsigned int place = 6u; place > 0u; place -= 1u)
    {
        // an octal digit, below 8
        header[148 + 6 - place] = (unsigned char)('0' + ((sum >> (3u * (place - 1u))) & 7ULL));
    }
    header[154] = 0u;
    header[155] = ' ';
    unsigned long long used = args->target->at + 512ULL;
    memor.cpy(&(MemoriaCfg){.dst = args->out + used, .src = args->data, .bytes = args->data_size});
    memor.set(&(MemoriaCfg){.dst = args->out + used + args->data_size, .val = 0, .bytes = padded - args->data_size});
    used += padded;
    memor.cpy(&(MemoriaCfg){.dst = args->out + used, .src = bytes + after, .bytes = rest});
    *out_size = used + rest;
    return 1;
}

int cli_gunzip(const CliArchiveBytesCfg *args)
{
    const unsigned char *bytes = args->bytes;
    if ((args->size < 3ULL) || (bytes[0] != 0x1fu) || (bytes[1] != 0x8bu) || (bytes[2] != 8u))
    {
        mmgr_line_put(args->err_msg, "not a gzip");
        return 0;
    }
    if (args->size < 10ULL)
    {
        mmgr_line_put(args->err_msg, "the archive is cut");
        return 0;
    }
    const unsigned char flags = bytes[3];
    unsigned long long at = 10ULL;
    unsigned long long extra = 0ULL;
    if (((flags & 4u) != 0u) &&
        !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                    .size = args->size, .at = at, .width = 2u, .value = &extra, .err_msg = args->err_msg))
    {
        return 0;
    }
    at += ((flags & 4u) != 0u) ? (2ULL + extra) : 0ULL;
    static const unsigned char s_named[2] = {8u, 16u};
    for (unsigned int flag = 0u; flag < 2u; flag += 1u)
    {
        if ((flags & s_named[flag]) == 0u)
        {
            continue;
        }
        const unsigned char *const zero = memor.chr(
            &(MemoriaCfg){.src = bytes + at, .val = 0, .bytes = (at < args->size) ? (args->size - at) : 0ULL});
        at = (zero != NULL) ? (unsigned long long)(zero - bytes) : args->size;
        if (at >= args->size)
        {
            mmgr_line_put(args->err_msg, "a gzip's header is cut");
            return 0;
        }
        at += 1ULL;
    }
    at += ((flags & 2u) != 0u) ? 2ULL : 0ULL;
    if (at > args->size)
    {
        mmgr_line_put(args->err_msg, "a gzip is cut");
        return 0;
    }
    unsigned long long used = 0ULL;
    if (!EMBED_CALL(cli_archive_size.inflate, CliArchiveSizeCfg, .data = bytes + at, .size = args->size - at,
                    .out = args->out, .capacity = args->capacity, .out_size = args->out_size, .used = &used,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    unsigned long long check = 0ULL;
    if (!EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                    .size = args->size, .at = at + used, .width = 4u, .value = &check, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (check !=
        EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = args->out, .size = *args->out_size))
    {
        mmgr_line_put(args->err_msg, "the gzip does not check");
        return 0;
    }
    return 1;
}

unsigned long long cli_gzip(const CliArchiveSizeCfg *args)
{
    static const unsigned char s_head[10] = {0x1fu, 0x8bu, 8u, 0u, 0u, 0u, 0u, 0u, 0u, 255u};
    if (args->capacity < 18ULL)
    {
        return 0ULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->out, .src = s_head, .bytes = sizeof(s_head)});
    const unsigned long long deflated =
        EMBED_CALL(cli_archive_size.deflate, CliArchiveSizeCfg, .data = args->data, .size = args->size,
                   .out = args->out + 10, .capacity = args->capacity - 18ULL);
    if ((deflated == 0ULL) && (args->size != 0ULL))
    {
        return 0ULL;
    }
    const unsigned int crc =
        EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = args->data, .size = args->size);
    unsigned long long at = 10ULL + deflated;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // each byte of the check, and of the size's low 32 bits, its low eight bits
        args->out[at + byte] = (unsigned char)(crc >> (8u * byte));
        args->out[at + 4u + byte] = (unsigned char)(args->size >> (8u * byte));
    }
    at += 8ULL;
    return at;
}
