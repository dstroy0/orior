// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing.h: a line written into a buffer and handed on as it fills, the state every text of the command line is
// written through and a file is written whole through, each write MMgr's own (verba_scribo.h) (file_writing.c)
#ifndef FILE_WRITING_H
#define FILE_WRITING_H

// where a line hands the `size` bytes at `bytes` it holds once it is full: 1 where they went, 0 where they did not
typedef int (*CliLineFlush)(void *context, const char *bytes, unsigned long long size);

// A line written into `out`, which holds `capacity`, `at` bytes so far, a byte held back for the terminator and `at`
// at `capacity` once a write did not fit. A line with a flush hands what it holds to it as it fills, `flushed` bytes
// so far, and goes on; a flush that fails leaves the line full
typedef struct
{
    char *out;
    unsigned long long capacity;
    unsigned long long at;
    CliLineFlush flush;
    void *context;
    unsigned long long flushed;
} CliLine;

// what the line holds handed to its flush and the line emptied: 1, or 0 where it has no flush or the flush failed;
// `length` bytes of `text` written, a text past the room left handed on as the line fills where it has a flush; a
// terminated text written, and at most `most` bytes of one; a text and spaces after it to `columns`; one character;
// `value` in base ten, `digits_least` digits at the fewest with zeros before; `value` in base ten right aligned in
// `columns` with spaces before; a signed value in base ten; `value` in lower case base sixteen, `digits_least` digits
// at the fewest; the terminator stored: the length, 0 where the line is full; and whether every write fit
int cli_line_flushed(CliLine *line);
void cli_line_counted(CliLine *line, const char *text, unsigned long long length);
void cli_line_text(CliLine *line, const char *text);
void cli_line_text_most(CliLine *line, const char *text, unsigned long long most);
void cli_line_text_columns(CliLine *line, const char *text, unsigned int columns);
void cli_line_character(CliLine *line, char character);
void cli_line_decimal(CliLine *line, unsigned long long value, unsigned int digits_least);
void cli_line_decimal_columns(CliLine *line, unsigned long long value, unsigned int columns);
void cli_line_signed(CliLine *line, long long value);
void cli_line_hex(CliLine *line, unsigned long long value, unsigned int digits_least);
unsigned long long cli_line_closed(CliLine *line);
int cli_line_fits(const CliLine *line);

#endif
