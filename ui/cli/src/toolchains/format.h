// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format.h: formatting a text with the formatter toolchains.json gives its language: Black for Python, clang-format
// for C, C++ and CUDA, rustfmt for Rust, and Prettier for JavaScript, CSS, HTML, JSON, Markdown and YAML. The
// formatter is found as File, Toolchains finds it, runs in the file's folder with the PATH orior's runs get, reads the
// text on its input and is given the file's path, and so follows the project's own settings: a pyproject.toml, a
// .clang-format, a rustfmt.toml or a .prettierrc. What it writes back is the text formatted; a formatter that fails
// gives what it printed (format.c, format_width.c)
#ifndef FORMAT_H
#define FORMAT_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliFormatPathCfg CliFormatPathCfg;
typedef struct CliFormatPathLanguageCfg CliFormatPathLanguageCfg;
typedef struct CliFormatCfg CliFormatCfg;
typedef struct CliFormatLanguageCfg CliFormatLanguageCfg;

// how long a formatter may take before it is stopped, in milliseconds
#define CLI_FORMAT_PATIENCE 30000ull

// the language a file opens in, by its extension, as the window chooses it, into `id`, sized for CLI_NAME_MAX_LEN:
// one of the reader's plugins ahead of one that comes with orior, and of two that come with orior the first by id. 1,
// or 0 where none takes it; every language a formatter formats, written as a JSON array; and the program that formats
// `language` into `program`, sized for CLI_NAME_MAX_LEN: 1, or 0 where impero has no formatter for it
int cli_format_language_get(const CliFormatCfg *args);
int cli_format_program_get(const CliFormatLanguageCfg *args);

// the width the formatter of `language` wraps lines of the file at `path` to into `width`: the project's own setting
// for it, Black's `line-length` in a pyproject.toml, rustfmt's `max_width`, clang-format's `ColumnLimit` and
// Prettier's `printWidth`, or the formatter's own where the project sets none. 0 for a language no formatter formats,
// for a limit of 0, which clang-format reads as none, and for Markdown where Prettier's `proseWrap` is not `always`:
// Prettier leaves its prose's lines as they are. And the same with the name of the formatter that wraps lines to it
// into `by`, sized for CLI_NAME_MAX_LEN. Each 1, or 0 where it gives no width (format_width.c)
int cli_format_width(const CliFormatPathLanguageCfg *args);
int cli_format_margin(const CliFormatPathLanguageCfg *args);

// `length` bytes of `text`, the text of the file at `path` in `language`, formatted, written as a JSON string. `{file}`
// in a formatter's word is the file's path, and `{edition}` the Rust edition of the crate that contains it. 1, or 0
// with why given
int cli_format(const CliFormatPathCfg *args);

struct CliFormatPathCfg
{
    const char *path;
    VerbaLinea *out;
    const char *language;
    const char *text;
    unsigned long long length;
    VerbaLinea *err_msg;
};

struct CliFormatPathLanguageCfg
{
    const char *path;
    const char *language;
    unsigned int *width;
    char *by;
};

struct CliFormatCfg
{
    const char *path;
    char *id;
};

struct CliFormatLanguageCfg
{
    const char *language;
    char *program;
};

typedef struct
{
    int (*language_get)(const CliFormatCfg *args);
} CliFormatNs;
EMBED_TABLE_LAYOUT(CliFormatNs, language_get);
EMBED_TABLE_STORAGE CliFormatNs cli_format_table EMBED_UNUSED = {
    .language_get = cli_format_language_get,
};

typedef struct
{
    int (*format)(const CliFormatPathCfg *args);
} CliFormatPathNs;
EMBED_TABLE_LAYOUT(CliFormatPathNs, format);
EMBED_TABLE_STORAGE CliFormatPathNs cli_format_path EMBED_UNUSED = {
    .format = cli_format,
};

typedef struct
{
    int (*program_get)(const CliFormatLanguageCfg *args);
} CliFormatLanguageNs;
EMBED_TABLE_LAYOUT(CliFormatLanguageNs, program_get);
EMBED_TABLE_STORAGE CliFormatLanguageNs cli_format_language EMBED_UNUSED = {
    .program_get = cli_format_program_get,
};

typedef struct
{
    int (*width)(const CliFormatPathLanguageCfg *args);
    int (*margin)(const CliFormatPathLanguageCfg *args);
} CliFormatPathLanguageNs;
EMBED_TABLE_LAYOUT(CliFormatPathLanguageNs, width, margin);
EMBED_TABLE_STORAGE CliFormatPathLanguageNs cli_format_path_language EMBED_UNUSED = {
    .width = cli_format_width,
    .margin = cli_format_margin,
};

#endif
