// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format.h: formatting a text with the formatter toolchains.json gives its language: Black for Python, clang-format
// for C, C++ and CUDA, rustfmt for Rust, and Prettier for JavaScript, CSS, HTML, JSON, Markdown and YAML. The
// formatter is found as File, Toolchains finds it, runs in the file's folder with the PATH orior's runs get, reads the
// text on its input and is told the file's path, and so keeps to the project's own settings: a pyproject.toml, a
// .clang-format, a rustfmt.toml or a .prettierrc. What it writes back is the text formatted; a formatter that fails
// gives what it said (format.c, format_width.c)
#ifndef FORMAT_H
#define FORMAT_H

#include "../command_line/cli_internal.h"

// how long a formatter may take before it is stopped, in milliseconds
#define CLI_FORMAT_PATIENCE 30000ull

// the language a file opens in, by its extension, as the window chooses it, into `id`, which holds CLI_NAME_LONGEST:
// one of the reader's plugins ahead of one that comes with orior, and of two that come with orior the first by id. 1,
// or 0 where none takes it; every language a formatter formats, written as a JSON array; and the program that formats
// `language` into `program`, which holds CLI_NAME_LONGEST: 1, or 0 where orior has no formatter for it
int cli_format_language_of(const char *path, char *id);
void cli_format_languages(CliLine *out);
int cli_format_program_of(const char *language, char *program);

// the width the formatter of `language` keeps lines of the file at `path` to into `width`: the project's own setting
// for it, Black's `line-length` in a pyproject.toml, rustfmt's `max_width`, clang-format's `ColumnLimit` and
// Prettier's `printWidth`, or the formatter's own where the project sets none. 0 for a language no formatter formats,
// for a limit of 0, which clang-format reads as none, and for Markdown where Prettier's `proseWrap` is not `always`:
// Prettier leaves its prose's lines as they are. And the same with the name of the formatter that keeps lines to it
// into `by`, which holds CLI_NAME_LONGEST. Each 1, or 0 where it gives no width (format_width.c)
int cli_format_width(const char *path, const char *language, unsigned int *width);
int cli_format_margin(const char *path, const char *language, unsigned int *width, char *by);

// `length` bytes of `text`, the text of the file at `path` in `language`, formatted, written as a JSON string. `{file}`
// in a formatter's word is the file's path, and `{edition}` the Rust edition of the crate that holds it. 1, or 0 with
// why said
int cli_format(const char *path, const char *language, const char *text, unsigned long long length, CliLine *out,
               CliLine *said);

#endif
