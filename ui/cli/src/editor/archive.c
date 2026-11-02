// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive.c: the kind of archive a file is, an archive's members read by its kind, and a member's bytes
#include "../runs/qry.h"
#include "../text/text.h"
#include "archive_internal.h"

// the most names of a .qry's blobs read
#define CLI_QRY_NAMES 4096u

unsigned int cli_archive_kind_get(const CliArchiveCfg *args)
{
    static const char *const s_zips[10] = {".zip", ".jar",   ".war",  ".ear", ".whl",
                                           ".apk", ".nupkg", ".vsix", ".xpi", ".epub"};
    char lower[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    length = EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = lower,
                        .capacity = sizeof(lower), .text = args->name);
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

int cli_archive_number(const CliArchiveBytesSizeAtCtx *args)
{
    unsigned long long *value = args->value;
    if ((args->at > args->size) || (args->width > (args->size - args->at)))
    {
        mmgr_line_put(args->err_msg, "the archive is cut");
        return 0;
    }
    *value = 0ull;
    for (unsigned int byte = 0u; byte < args->width; byte += 1u)
    {
        *value |= (unsigned long long)args->bytes[args->at + byte] << (8u * byte);
    }
    return 1;
}

unsigned int cli_archive_name_store(const CliArchiveMembersCtx *args)
{
    const char *name = args->name;
    unsigned long long length = args->length;
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
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                   .text = name, .at = 0ull, .size = length, .set = "/");
        name += lead;
        length -= lead;
    }
    length =
        EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = name, .at = length, .set = "/");
    if ((args->members->used + length + 1ull) > CLI_ARCHIVE_NAMES)
    {
        return 0u;
    }
    // a place is below CLI_ARCHIVE_NAMES, which an unsigned int stores
    const unsigned int at = (unsigned int)args->members->used;
    memor.cpy(&(MemoriaCfg){.dst = args->members->text + at, .src = name, .bytes = length});
    args->members->text[at + length] = '\0';
    args->members->used += length + 1ull;
    return at;
}

void cli_archive_member_append(const CliArchiveSizeCtx *args)
{
    if (args->members->count >= CLI_ARCHIVE_MEMBERS)
    {
        return;
    }
    args->members->item[args->members->count] =
        (CliMember){args->name_u32, args->raw, args->has_raw, args->dir, args->size, args->at};
    args->members->count += 1u;
}

void cli_gz_inner(const CliArchiveNameOutCtx *args)
{
    const char *const base =
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->name,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}),
                    .byte = '/') != NULL)
            ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->name,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1)
            : args->name;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    const int gz =
        (length >= 3ull) &&
        ((cellul.eq(&(CatenaFinitaCfg){.src = base + length - 3ull, .other = ".gz", .cap = CLI_CELLS_POOL})) ||
         (cellul.eq(&(CatenaFinitaCfg){.src = base + length - 3ull, .other = ".GZ", .cap = CLI_CELLS_POOL})));
    VerbaLinea line = {args->out_capacity, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, base, gz ? (length - 3ull) : length);
    mmgr_line_finish(&line);
}

int cli_members_get(const CliArchiveBytesSizeErrMsgCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long *unpacked_size = args->unpacked_size;
    VerbaLinea *err_msg = args->err_msg;
    static char s_names[CLI_QRY_NAMES][CLI_NAME_MAX_LEN];
    args->members->count = 0u;
    args->members->used = 1ull;
    args->members->text[0] = '\0';
    *unpacked_size = 0ull;
    if (args->kind == CLI_ARCHIVE_ZIP)
    {
        return EMBED_CALL(cli_archive_bytes_size_err_msg_internal.zip_members, CliArchiveBytesSizeErrMsgCtx,
                          .bytes = bytes, .size = size, .members = args->members, .err_msg = err_msg);
    }
    if (args->kind == CLI_ARCHIVE_TAR)
    {
        return EMBED_CALL(cli_archive_bytes_size_err_msg_internal.tar_members, CliArchiveBytesSizeErrMsgCtx,
                          .bytes = bytes, .size = size, .members = args->members, .err_msg = err_msg);
    }
    if ((args->kind == CLI_ARCHIVE_TARGZ) || (args->kind == CLI_ARCHIVE_GZ))
    {
        // a gzip's trailer ends with the low 32 bits of what it contains unpacked
        unsigned long long trailer_size = 0ull;
        if ((size < 18ull) ||
            !EMBED_CALL(cli_archive_bytes_size_at_internal.number, CliArchiveBytesSizeAtCtx, .bytes = bytes,
                        .size = size, .at = size - 4ull, .width = 4u, .value = &trailer_size, .err_msg = err_msg))
        {
            mmgr_line_put(err_msg, (size < 18ull) ? "a gzip is cut" : "");
            return 0;
        }
        if (!EMBED_CALL(cli_archive_err_msg_buffer_internal.capacity, CliArchiveErrMsgBufferCtx,
                        .buffer_pointer = args->unpacked, .need = trailer_size + 1ull, .what = "an archive unpacked",
                        .err_msg = err_msg) ||
            !EMBED_CALL(cli_archive_bytes.gunzip, CliArchiveBytesCfg, .bytes = bytes, .size = size,
                        .out = args->unpacked->bytes, .capacity = args->unpacked->capacity, .out_size = unpacked_size,
                        .err_msg = err_msg))
        {
            return 0;
        }
        if (args->kind == CLI_ARCHIVE_TARGZ)
        {
            return EMBED_CALL(cli_archive_bytes_size_err_msg_internal.tar_members, CliArchiveBytesSizeErrMsgCtx,
                              .bytes = args->unpacked->bytes, .size = *unpacked_size, .members = args->members,
                              .err_msg = err_msg);
        }
        EMBED_CALL(cli_archive_size_internal.member_append, CliArchiveSizeCtx, .members = args->members,
                   .name_u32 =
                       EMBED_CALL(cli_archive_members_internal.name_store, CliArchiveMembersCtx,
                                  .members = args->members, .name = args->inner,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = args->inner, .cap = CLI_CELLS_POOL})),
                   .raw = 0u, .has_raw = 0, .dir = 0, .size = *unpacked_size, .at = 0ull);
        return 1;
    }
    // each name's blobs joined, the name a member of its own
    const unsigned int count = EMBED_CALL(cli_qry_data.member_names, CliQryDataCfg, .data = bytes, .size = size,
                                          .names = s_names, .capacity = CLI_QRY_NAMES);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_archive_size_internal.member_append, CliArchiveSizeCtx, .members = args->members,
                   .name_u32 =
                       EMBED_CALL(cli_archive_members_internal.name_store, CliArchiveMembersCtx,
                                  .members = args->members, .name = s_names[at],
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL})),
                   .raw = 0u, .has_raw = 0, .dir = 0, .size = 0ull, .at = 0ull);
    }
    return 1;
}

int cli_data_get(const CliArchiveBytesCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    CliByteBuffer *out = args->out_buffer_pointer;
    unsigned long long *out_size = args->out_size;
    const char *const name = args->members_read->text + args->member->name;
    const unsigned long long length =
        (args->kind == CLI_ARCHIVE_GZ) ? args->unpacked_size_value
        : (args->kind == CLI_ARCHIVE_QRY)
            ? EMBED_CALL(cli_qry_out.member_size, CliQryOutCfg, .data = bytes, .size = size, .name = name)
            : args->member->size;
    if (!EMBED_CALL(cli_archive_err_msg_buffer_internal.capacity, CliArchiveErrMsgBufferCtx, .buffer_pointer = out,
                    .need = length + 1ull, .what = "a member of an archive", .err_msg = args->err_msg))
    {
        return 0;
    }
    if (args->kind == CLI_ARCHIVE_ZIP)
    {
        return EMBED_CALL(cli_archive_bytes_size_err_msg_members_internal.zip_data, CliArchiveBytesSizeErrMsgMembersCtx,
                          .bytes = bytes, .size = size, .members_read = args->members_read, .member = args->member,
                          .out = out->bytes, .capacity = out->capacity, .out_size = out_size, .err_msg = args->err_msg);
    }
    if (args->kind == CLI_ARCHIVE_QRY)
    {
        VerbaLinea line = {(char *)out->bytes, out->capacity, 0ull};
        EMBED_CALL(cli_qry_out.member_join, CliQryOutCfg, .data = bytes, .size = size, .name = name, .out = &line);
        if (!mmgr_line_ok(&line))
        {
            mmgr_line_put(args->err_msg, name);
            mmgr_line_put(args->err_msg, " is larger than orior_cli supports");
            return 0;
        }
        *out_size = line.at;
        return 1;
    }
    const unsigned char *const from = (args->kind == CLI_ARCHIVE_TAR) ? bytes : args->unpacked_bytes;
    const unsigned long long from_size = (args->kind == CLI_ARCHIVE_TAR) ? size : args->unpacked_size_value;
    const unsigned long long start = (args->kind == CLI_ARCHIVE_GZ) ? 0ull : (args->member->at + 512ull);
    if ((start > from_size) || (length > (from_size - start)))
    {
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " is cut");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = out->bytes, .src = from + start, .bytes = length});
    *out_size = length;
    return 1;
}
