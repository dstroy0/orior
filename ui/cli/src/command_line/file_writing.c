// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing.c: files written whole through a stage, each whole stage handed to the sealed writer (ksg_writing.h),
// which writes the file beside its place under a name of this process's own, seals it where it is of a kind ksg seals,
// and puts it in its place once every byte is on the disk, so a write that fails leaves the file it would have
// replaced as it was; and a file written as pieces in turn the same way
#include "cells.h"
#include "cli_internal.h"

// the most bytes of the name a file is written under beside its place: a dot, this process's id, a dot, a count of
// its own, and `.writing`
#define CLI_WRITING_MAX_LEN 64u

int cli_file_begun(CliFileWriting *file, const char *path)
{
    static unsigned long long s_count;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_count += 1ull;
    const unsigned long long count = s_count;
    cli_lock_give(&s_lock);
    char beside[CLI_WRITING_MAX_LEN];
    VerbaLinea named = {beside, sizeof(beside), 0u, NULL, NULL, 0u};
    mmgr_line_ch(&named, '.');
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '.');
    mmgr_line_decimal(&named, count, 1u);
    mmgr_line_put(&named, ".writing");
    return (mmgr_line_finish(&named) != 0u) && ksg_writing_begun(file, path, beside);
}

int cli_file_put(void *context, const char *bytes, size_t size)
{
    return ksg_writing_put(context, bytes, size);
}

int cli_file_ended(CliFileWriting *file, int whole)
{
    return ksg_writing_ended(file, whole);
}

VerbaLinea cli_file_line(CliFileWriting *file, char *stage)
{
    const VerbaLinea line = {stage, CLI_STAGE_BYTES + 1u, 0u, cli_file_put, file, 0u};
    return line;
}

int cli_file_line_ended(CliFileWriting *file, VerbaLinea *line)
{
    const int whole = (line->at == 0u) || mmgr_line_flush(line);
    return cli_file_ended(file, whole && mmgr_line_ok(line));
}

int cli_file_pieces_written(const char *path, const CliFilePiece *pieces, unsigned int count)
{
    static const char s_zeros[4096] = {0};
    static char s_stage[CLI_STAGE_BYTES + 1u];
    static CliFileWriting s_file;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    if (!cli_file_begun(&s_file, path))
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    VerbaLinea line = cli_file_line(&s_file, s_stage);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (pieces[at].bytes != NULL)
        {
            mmgr_line_put_n(&line, (const char *)pieces[at].bytes, pieces[at].size);
            continue;
        }
        for (unsigned long long left = pieces[at].size; left > 0ull;)
        {
            const unsigned long long piece = (left < sizeof(s_zeros)) ? left : sizeof(s_zeros);
            mmgr_line_put_n(&line, s_zeros, piece);
            left -= piece;
        }
    }
    const int written = cli_file_line_ended(&s_file, &line);
    cli_lock_give(&s_lock);
    return written;
}
