// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing.c: files written whole through a stage, each whole stage handed to the sealed writer (ksg_writing.h),
// which writes the file beside its place under a name of this process's own, seals it where it is of a kind ksg seals,
// and puts it in its place once every byte is on the disk, so a write that fails leaves the file it would have
// replaced as it was; and a file written as pieces in turn the same way. And the line every text is written through,
// each write MMgr's own
#include "file_writing.h"
#include "cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cli_internal.h"
#include "verba_scribo/verba_scribo.h"

// the most bytes of the name a file is written under beside its place: a dot, this process's id, a dot, a count of
// its own, and `.writing`
#define CLI_WRITING_LONGEST 64u

// the bytes a number is written into before it goes on a line: its most digits, 255 at the fewest asked, and more
#define CLI_LINE_NUMBER 280u

// the spaces a text is padded with, a run of them at a time
static const char s_spaces[32] = "                                ";

int cli_line_flushed(CliLine *line)
{
    if ((line->flush == NULL) || (line->at >= line->capacity))
    {
        return 0;
    }
    if (!line->flush(line->context, line->out, line->at))
    {
        line->at = line->capacity;
        return 0;
    }
    line->flushed += line->at;
    line->at = 0ull;
    return 1;
}

void cli_line_counted(CliLine *line, const char *text, unsigned long long length)
{
    unsigned long long done = 0ull;
    while (done < length)
    {
        // a byte of the line is held back for the terminator, as verba holds it
        const unsigned long long room = (line->at < line->capacity) ? (line->capacity - line->at - 1u) : 0ull;
        const unsigned long long left = length - done;
        if ((left > room) && (line->flush == NULL))
        {
            line->at = line->capacity;
            return;
        }
        const unsigned long long piece = (left < room) ? left : room;
        line->at = verba_textus.put_n(&(VerbaTextusCfg){.out = line->out,
                                                        .cap = (size_t)line->capacity,
                                                        .at = (size_t)line->at,
                                                        .text = text + done,
                                                        .text_len = (size_t)piece});
        done += piece;
        if ((done < length) && !cli_line_flushed(line))
        {
            return;
        }
    }
}

void cli_line_text(CliLine *line, const char *text)
{
    cli_line_counted(line, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_line_text_most(CliLine *line, const char *text, unsigned long long most)
{
    cli_line_counted(line, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = (size_t)most}));
}

void cli_line_text_columns(CliLine *line, const char *text, unsigned int columns)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    cli_line_counted(line, text, length);
    for (unsigned long long pad = (length < columns) ? (columns - length) : 0ull; pad > 0ull;)
    {
        const unsigned long long run = (pad < sizeof(s_spaces)) ? pad : sizeof(s_spaces);
        cli_line_counted(line, s_spaces, run);
        pad -= run;
    }
}

void cli_line_character(CliLine *line, char character)
{
    cli_line_counted(line, &character, 1ull);
}

// a number written into `digits`, which holds CLI_LINE_NUMBER, put on the line: `written` is where the writing ended,
// CLI_LINE_NUMBER where it did not fit
static void line_number(CliLine *line, const char *digits, size_t written)
{
    if (written >= CLI_LINE_NUMBER)
    {
        line->at = line->capacity;
        return;
    }
    cli_line_counted(line, digits, written);
}

void cli_line_decimal(CliLine *line, unsigned long long value, unsigned int digits_least)
{
    char digits[CLI_LINE_NUMBER];
    // a least count past 255 is held at 255, the most a byte of the argument holds
    const uint8_t least = (uint8_t)((digits_least < 255u) ? digits_least : 255u);
    line_number(
        line, digits,
        verba_numerus.u32w(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .val = value, .min = least}));
}

void cli_line_decimal_columns(CliLine *line, unsigned long long value, unsigned int columns)
{
    char digits[CLI_LINE_NUMBER];
    // a column count past 255 is held at 255, the most a byte of the argument holds
    const uint8_t width = (uint8_t)((columns < 255u) ? columns : 255u);
    line_number(line, digits,
                verba_numerus.u64_clip(
                    &(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .val = value, .columns = width}));
}

void cli_line_signed(CliLine *line, long long value)
{
    char digits[CLI_LINE_NUMBER];
    line_number(line, digits,
                verba_numerus.i64(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .sval = value}));
}

void cli_line_hex(CliLine *line, unsigned long long value, unsigned int digits_least)
{
    char digits[CLI_LINE_NUMBER];
    // a least count past 255 is held at 255, the most a byte of the argument holds
    const uint8_t least = (uint8_t)((digits_least < 255u) ? digits_least : 255u);
    line_number(
        line, digits,
        verba_numerus.hex(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .val = value, .min = least}));
}

unsigned long long cli_line_closed(CliLine *line)
{
    return verba_finis.finish(
        &(VerbaFinisCfg){.out = line->out, .cap = (size_t)line->capacity, .at = (size_t)line->at});
}

int cli_line_fits(const CliLine *line)
{
    return verba_finis.ok(&(VerbaFinisCfg){.cap = (size_t)line->capacity, .at = (size_t)line->at});
}

int cli_file_begun(CliFileWriting *file, const char *path)
{
    static unsigned long long s_count;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_count += 1ull;
    const unsigned long long count = s_count;
    cli_lock_give(&s_lock);
    char beside[CLI_WRITING_LONGEST];
    CliLine named = {beside, sizeof(beside), 0ull, NULL, NULL, 0ull};
    cli_line_character(&named, '.');
    cli_line_decimal(&named, cli_process_own(), 1u);
    cli_line_character(&named, '.');
    cli_line_decimal(&named, count, 1u);
    cli_line_text(&named, ".writing");
    return (cli_line_closed(&named) != 0ull) && ksg_writing_begun(file, path, beside);
}

int cli_file_put(void *context, const char *bytes, unsigned long long size)
{
    return ksg_writing_put(context, bytes, size);
}

int cli_file_ended(CliFileWriting *file, int whole)
{
    return ksg_writing_ended(file, whole);
}

CliLine cli_file_line(CliFileWriting *file, char *stage)
{
    const CliLine line = {stage, CLI_STAGE_BYTES + 1u, 0ull, cli_file_put, file, 0ull};
    return line;
}

int cli_file_line_ended(CliFileWriting *file, CliLine *line)
{
    const int whole = (line->at == 0ull) || (cli_line_flushed(line) != 0);
    return cli_file_ended(file, whole && cli_line_fits(line));
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
    CliLine line = cli_file_line(&s_file, s_stage);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (pieces[at].bytes != NULL)
        {
            cli_line_counted(&line, (const char *)pieces[at].bytes, pieces[at].size);
            continue;
        }
        for (unsigned long long left = pieces[at].size; left > 0ull;)
        {
            const unsigned long long piece = (left < sizeof(s_zeros)) ? left : sizeof(s_zeros);
            cli_line_counted(&line, s_zeros, piece);
            left -= piece;
        }
    }
    const int written = cli_file_line_ended(&s_file, &line);
    cli_lock_give(&s_lock);
    return written;
}
