// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_tar.c: the tar stream a machine's files go in, written to a file: each file a ustar header and its bytes,
// with its mode and the time given, a path longer than a ustar header holds in a PAX header before it, and two blocks
// of zeros after the last
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

// the bytes of a tar block
#define CLI_DEPLOY_BLOCK 512u

static const unsigned char s_zeros[2u * CLI_DEPLOY_BLOCK];

// `value` written in octal, `digits` of them at the fewest, and a 0 after, into `field` of `width` bytes, cut to it
static void tar_octal(unsigned char *field, unsigned int width, unsigned long long value, unsigned int digits)
{
    char text[32];
    unsigned int length = 0u;
    for (unsigned long long left = value; (left > 0u) || (length == 0u); left /= 8u)
    {
        text[length] = (char)('0' + (left % 8u));
        length += 1u;
    }
    char written[40];
    unsigned int at = 0u;
    for (unsigned int pad = length; pad < digits; pad += 1u)
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
    memor.cpy(&(MemoriaCfg){.dst = field, .src = written, .bytes = (at < width) ? at : width});
}

// the fields of a ustar header, its checksum filled in, into `header`
static void tar_header(unsigned char *header, const char *name, unsigned long long size, unsigned int mode,
                       unsigned long long time, char kind)
{
    memor.set(&(MemoriaCfg){.dst = header, .bytes = CLI_DEPLOY_BLOCK, .val = 0u});
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = header, .src = name, .bytes = (length < 100u) ? length : 100u});
    tar_octal(header + 100, 8u, mode, 7u);
    memor.cpy(&(MemoriaCfg){.dst = header + 108, .src = "0000000", .bytes = 8u});
    memor.cpy(&(MemoriaCfg){.dst = header + 116, .src = "0000000", .bytes = 8u});
    tar_octal(header + 124, 12u, size, 11u);
    tar_octal(header + 136, 12u, time, 11u);
    memor.set(&(MemoriaCfg){.dst = header + 148, .bytes = 8u, .val = (uint8_t)' '});
    header[156] = (unsigned char)kind;
    memor.cpy(&(MemoriaCfg){.dst = header + 257, .src = "ustar", .bytes = 6u});
    memor.cpy(&(MemoriaCfg){.dst = header + 263, .src = "00", .bytes = 2u});
    unsigned long long sum = 0ull;
    for (unsigned int at = 0u; at < CLI_DEPLOY_BLOCK; at += 1u)
    {
        sum += header[at];
    }
    tar_octal(header + 148, 8u, sum, 6u);
    header[155] = ' ';
}

// `size` bytes added to the tar at `path`, then zeros to the end of their last block: 1, or 0 where they could not be
static int tar_put(const char *path, const void *bytes, unsigned long long size)
{
    const unsigned long long pad = (CLI_DEPLOY_BLOCK - (size % CLI_DEPLOY_BLOCK)) % CLI_DEPLOY_BLOCK;
    return ((size == 0u) || cli_file_appended(path, bytes, size, 0)) &&
           ((pad == 0u) || cli_file_appended(path, s_zeros, pad, 0));
}

// the PAX header that names a long path, written before its file
static int tar_pax(const char *path, const char *name, unsigned long long time)
{
    char record[CLI_PATH_LONGEST + 16u];
    CliLine record_line = {record, sizeof(record), 0ull};
    cli_line_text(&record_line, " path=");
    cli_line_text(&record_line, name);
    cli_line_character(&record_line, '\n');
    // the record's length counts the digits that write it
    unsigned long long length = record_line.at;
    for (;;)
    {
        char digits[24];
        CliLine digits_line = {digits, sizeof(digits), 0ull};
        cli_line_decimal(&digits_line, length, 1u);
        if (digits_line.at + record_line.at == length)
        {
            break;
        }
        length = digits_line.at + record_line.at;
    }
    char pax[CLI_PATH_LONGEST + 48u];
    CliLine pax_line = {pax, sizeof(pax), 0ull};
    cli_line_decimal(&pax_line, length, 1u);
    cli_line_counted(&pax_line, record, record_line.at);
    unsigned char header[CLI_DEPLOY_BLOCK];
    tar_header(header, "PaxHeader", pax_line.at, 0644u, time, 'x');
    return cli_line_fits(&pax_line) && tar_put(path, header, sizeof(header)) && tar_put(path, pax, pax_line.at);
}

// whether `name` is among the paths of `kept`, sorted by their bytes
static int tar_kept(const char *name, const CliDeployPair *kept, unsigned int count)
{
    unsigned int low = 0u;
    unsigned int high = count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = cli_deploy_order(kept[middle].path, name);
        if (order == 0)
        {
            return 1;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return 0;
}

int cli_deploy_tar(const char *path, const CliDeployPair *kept, unsigned int kept_count, unsigned long long time,
                   CliLine *said)
{
    const CliDeployRun *const run = &g_cli_deploy;
    if (!cli_file_appended(path, "", 0ull, 1))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    for (unsigned int index = 0u; index < run->sents; index += 1u)
    {
        const CliDeploySent *const file = &run->sent[index];
        if (tar_kept(file->path, kept, kept_count))
        {
            continue;
        }
        char full[(2u * CLI_PATH_LONGEST) + 2u];
        CliLine full_line = {full, sizeof(full), 0ull};
        cli_deploy_full(index, &full_line);
        cli_line_closed(&full_line);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(full, &size);
        if (bytes == NULL)
        {
            cli_line_text(said, full);
            cli_line_text(said, ": it could not be read");
            return 0;
        }
        unsigned char header[CLI_DEPLOY_BLOCK];
        tar_header(header, file->path, size, file->mode, time, '0');
        const int written = ((cellul.len(&(CatenaFinitaCfg){.src = file->path, .cap = CLI_CELLS_POOL}) <= 99u) ||
                             tar_pax(path, file->path, time)) &&
                            tar_put(path, header, sizeof(header)) && tar_put(path, bytes, size);
        cli_file_let_go(bytes, size);
        if (!written)
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
    }
    if (!cli_file_appended(path, s_zeros, sizeof(s_zeros), 0))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}
