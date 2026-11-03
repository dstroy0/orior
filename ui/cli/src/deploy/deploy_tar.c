// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_tar.c: the tar stream a machine's files go in, written to a file: each file a ustar header and its bytes,
// with its mode and the time given, a path longer than a ustar header fits in a PAX header before it, and two blocks
// of zeros after the last
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

// the bytes of a tar block
#define CLI_DEPLOY_BLOCK 512u

static const unsigned char s_zeros[2u * CLI_DEPLOY_BLOCK];

typedef struct
{
    unsigned char *field;
    unsigned int width;
    unsigned long long value;
    unsigned int digits;
    unsigned char *header;
    const char *name;
    unsigned long long size;
    unsigned int mode;
    unsigned long long time;
    char kind;
    const char *path;
    const void *bytes;
    const CliDeployPair *preserved;
    unsigned int count;
} DeployTarCtx;
// `value` written in octal, `digits` of them at the fewest, and a 0 after, into `field` of `width` bytes, cut to it
EMBED_INLINE void tar_octal(const DeployTarCtx *args)
{
    char text[32];
    unsigned int length = 0u;
    unsigned long long left = args->value;
    while ((left > 0u) || (length == 0u))
    {
        text[length] = (char)('0' + (left % 8u));
        length += 1u;
        left /= 8u;
    }
    char written[40];
    unsigned int at = 0u;
    for (unsigned int pad = length; pad < args->digits; pad += 1u)
    {
        written[at] = '0';
        at += 1u;
    }
    for (unsigned int index = length; index > 0u; index -= 1u)
    {
        written[at] = text[index - 1u];
        at += 1u;
    }
    written[at] = '\0';
    at += 1u;
    memor.cpy(&(MemoriaCfg){.dst = args->field, .src = written, .bytes = (at < args->width) ? at : args->width});
}

// the fields of a ustar header, its checksum filled in, into `header`
EMBED_INLINE void tar_header(const DeployTarCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->header, .bytes = CLI_DEPLOY_BLOCK, .val = 0u});
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = args->header, .src = args->name, .bytes = (length < 100u) ? length : 100u});
    EMBED_CALL(tar_octal, DeployTarCtx, .field = args->header + 100, .width = 8u, .value = args->mode, .digits = 7u);
    memor.cpy(&(MemoriaCfg){.dst = args->header + 108, .src = "0000000", .bytes = 8u});
    memor.cpy(&(MemoriaCfg){.dst = args->header + 116, .src = "0000000", .bytes = 8u});
    EMBED_CALL(tar_octal, DeployTarCtx, .field = args->header + 124, .width = 12u, .value = args->size, .digits = 11u);
    EMBED_CALL(tar_octal, DeployTarCtx, .field = args->header + 136, .width = 12u, .value = args->time, .digits = 11u);
    memor.set(&(MemoriaCfg){.dst = args->header + 148, .bytes = 8u, .val = (uint8_t)' '});
    args->header[156] = (unsigned char)args->kind;
    memor.cpy(&(MemoriaCfg){.dst = args->header + 257, .src = "ustar", .bytes = 6u});
    memor.cpy(&(MemoriaCfg){.dst = args->header + 263, .src = "00", .bytes = 2u});
    unsigned long long sum = 0ULL;
    for (unsigned int at = 0u; at < CLI_DEPLOY_BLOCK; at += 1u)
    {
        sum += args->header[at];
    }
    EMBED_CALL(tar_octal, DeployTarCtx, .field = args->header + 148, .width = 8u, .value = sum, .digits = 6u);
    args->header[155] = ' ';
}

// `size` bytes added to the tar at `path`, then zeros to the end of their last block: 1, or 0 where they could not be
EMBED_INLINE int tar_append(const DeployTarCtx *args)
{
    const char *path = args->path;
    const void *bytes = args->bytes;
    unsigned long long size = args->size;
    const unsigned long long pad = (CLI_DEPLOY_BLOCK - (size % CLI_DEPLOY_BLOCK)) % CLI_DEPLOY_BLOCK;
    return ((size == 0u) || EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path,
                                       .bytes = bytes, .size = size, .fresh = 0)) &&
           ((pad == 0u) || EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path,
                                      .bytes = s_zeros, .size = pad, .fresh = 0));
}

// the PAX header that names a long path, written before its file
EMBED_INLINE int tar_pax(const DeployTarCtx *args)
{
    char record[CLI_PATH_MAX_LEN + 16u];
    VerbaLinea record_line = {record, sizeof(record), 0ULL};
    mmgr_line_put(&record_line, " path=");
    mmgr_line_put(&record_line, args->name);
    mmgr_line_ch(&record_line, '\n');
    // the record's length counts the digits that write it
    unsigned long long length = record_line.at;
    for (;;)
    {
        char digits[24];
        VerbaLinea digits_line = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal(&digits_line, length, 1u);
        if (digits_line.at + record_line.at == length)
        {
            break;
        }
        length = digits_line.at + record_line.at;
    }
    char pax[CLI_PATH_MAX_LEN + 48u];
    VerbaLinea pax_line = {pax, sizeof(pax), 0ULL};
    mmgr_line_decimal(&pax_line, length, 1u);
    mmgr_line_put_n(&pax_line, record, record_line.at);
    unsigned char header[CLI_DEPLOY_BLOCK];
    EMBED_CALL(tar_header, DeployTarCtx, .header = header, .name = "PaxHeader", .size = pax_line.at, .mode = 0644u,
               .time = args->time, .kind = 'x');
    return mmgr_line_ok(&pax_line) &&
           EMBED_CALL(tar_append, DeployTarCtx, .path = args->path, .bytes = header, .size = sizeof(header)) &&
           EMBED_CALL(tar_append, DeployTarCtx, .path = args->path, .bytes = pax, .size = pax_line.at);
}

// whether `name` is among the paths of `preserved`, sorted by their bytes
EMBED_INLINE int tar_is_preserve(const DeployTarCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx,
                                     .left = args->preserved[middle].path, .right = args->name);
        if (order == 0)
        {
            return 1;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return 0;
}

int cli_deploy_tar(const CliDeployErrMsgPathCtx *args)
{
    const CliDeployRun *const run = &g_cli_deploy;
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->path, .bytes = "", .size = 0ULL,
                    .fresh = 1))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    for (unsigned int index = 0u; index < run->sents; index += 1u)
    {
        const CliDeploySent *const file = &run->sent[index];
        if (EMBED_CALL(tar_is_preserve, DeployTarCtx, .name = file->path, .preserved = args->preserved,
                       .count = args->preserved_count))
        {
            continue;
        }
        char full[(2u * CLI_PATH_MAX_LEN) + 2u];
        VerbaLinea full_line = {full, sizeof(full), 0ULL};
        EMBED_CALL(cli_deploy_out_index_internal.full, CliDeployOutIndexCtx, .index = index, .out = &full_line);
        mmgr_line_finish(&full_line);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                      .size_pointer = &size, .owner = &g_cli_deploy_cells);
        if (bytes == NULL)
        {
            mmgr_line_put(args->err_msg, full);
            mmgr_line_put(args->err_msg, ": it could not be read");
            return 0;
        }
        unsigned char header[CLI_DEPLOY_BLOCK];
        EMBED_CALL(tar_header, DeployTarCtx, .header = header, .name = file->path, .size = size, .mode = file->mode,
                   .time = args->time, .kind = '0');
        const int written =
            ((cellul.len(&(CatenaFinitaCfg){.src = file->path, .cap = CLI_CELLS_POOL}) <= 99u) ||
             EMBED_CALL(tar_pax, DeployTarCtx, .path = args->path, .name = file->path, .time = args->time)) &&
            EMBED_CALL(tar_append, DeployTarCtx, .path = args->path, .bytes = header, .size = sizeof(header)) &&
            EMBED_CALL(tar_append, DeployTarCtx, .path = args->path, .bytes = bytes, .size = size);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_deploy_cells);
        if (!written)
        {
            mmgr_line_put(args->err_msg, args->path);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
    }
    if (!EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = args->path, .bytes = s_zeros,
                    .size = sizeof(s_zeros), .fresh = 0))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}
