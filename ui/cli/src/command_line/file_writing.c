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

int cli_file_begin(const CliPathFileCtx *args)
{
    static unsigned long long s_count;
    s_count += 1ull;
    const unsigned long long count = s_count;
    char adjacent[CLI_WRITING_MAX_LEN];
    VerbaLinea named = {adjacent, sizeof(adjacent), 0u, NULL, NULL, 0u};
    mmgr_line_ch(&named, '.');
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '.');
    mmgr_line_decimal(&named, count, 1u);
    mmgr_line_put(&named, ".writing");
    return (mmgr_line_finish(&named) != 0u) && ksg_writing_begun(args->file, args->path, adjacent);
}

int cli_file_write_chunk(void *context, const char *bytes, size_t size)
{
    return ksg_writing_put(context, bytes, size);
}

int cli_file_end(const CliFileCtx *args)
{
    return ksg_writing_ended(args->file, args->whole);
}

VerbaLinea cli_file_line(const CliFileStageCtx *args)
{
    const VerbaLinea line = {args->stage, CLI_STAGE_BYTES + 1u, 0u, cli_file_write_chunk, args->file, 0u};
    return line;
}

int cli_file_line_end(const CliLineCtx *args)
{
    const int whole = (args->line->at == 0u) || mmgr_line_flush(args->line);
    return EMBED_CALL(cli_file_internal.file_end, CliFileCtx, .file = args->file,
                      .whole = whole && mmgr_line_ok(args->line));
}

int cli_file_pieces_write(const CliPathCountCtx *args)
{
    static const char s_zeros[4096] = {0};
    static char s_stage[CLI_STAGE_BYTES + 1u];
    static CliFileWriting s_file;
    if (!EMBED_CALL(cli_path_file_internal.file_begin, CliPathFileCtx, .file = &s_file, .path = args->path))
    {
        return 0;
    }
    VerbaLinea line = EMBED_CALL(cli_file_stage_internal.file_line, CliFileStageCtx, .file = &s_file, .stage = s_stage);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (args->pieces[at].bytes != NULL)
        {
            mmgr_line_put_n(&line, (const char *)args->pieces[at].bytes, args->pieces[at].size);
            continue;
        }
        for (unsigned long long left = args->pieces[at].size; left > 0ull;)
        {
            const unsigned long long piece = (left < sizeof(s_zeros)) ? left : sizeof(s_zeros);
            mmgr_line_put_n(&line, s_zeros, piece);
            left -= piece;
        }
    }
    const int written = EMBED_CALL(cli_line_internal.file_line_end, CliLineCtx, .file = &s_file, .line = &line);
    return written;
}
