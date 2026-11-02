// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format_internal.h: what the format sources share: the manifest read and the toolchain that formats a language, and
// the nearest settings file read, under the formatting's lock
#ifndef FORMAT_INTERNAL_H
#define FORMAT_INTERNAL_H
typedef struct CliFormatErrMsgCtx CliFormatErrMsgCtx;
typedef struct CliFormatTextCtx CliFormatTextCtx;
typedef struct CliFormatPathCtx CliFormatPathCtx;
typedef struct CliFormatCtx CliFormatCtx;
typedef struct CliFormatLanguageCtx CliFormatLanguageCtx;
typedef struct CliFormatBytesCtx CliFormatBytesCtx;

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "format.h"
#include "toolchains.h"

// The work state the formatting reads into: the manifest and a settings file's text, each a cell sized to what it
// contains, the toolchain found, and the formatter's words
typedef struct
{
    CliJson json;
    CliFound found;
    char *settings;
    unsigned long long settings_size;
    char words[CLI_WORDS_TEXT];
    char path[CLI_PATH_TEXT];
} CliFormatWorkState;

extern CliFormatWorkState g_cli_format;

// the work state's manifest and settings given back, and the formatting's lock with them (format.c)
void cli_format_release(void);

// the manifest read into the work state, and the toolchain that formats `language` in it: the tool, or CLI_JSON_NONE
// with why given where none does (format.c)
unsigned int cli_format_tool(const CliFormatLanguageCtx *args);

// the first of the files `names`, `count` of them, in the folder of `path` or the nearest folder above it that contains
// one, read whole into the work state's settings: 1, or 0 where none is there or reads as UTF-8 (format_width.c)
int cli_format_nearest(const CliFormatPathCtx *args);
// whether `size` bytes are UTF-8 whole; and that `program` could not format the file at `path` reported, with the first
// six lines of what it printed, trimmed, or that it printed nothing (format_width.c)
int cli_format_utf8(const CliFormatBytesCtx *args);
void cli_format_fail(const CliFormatErrMsgCtx *args);
// the whole number set for `key` in a settings file, TOML, YAML or JSON: within the TOML table `table` where one is
// named, and before any table where none is: 1 with it into `number`, or 0 (format_width.c)
int cli_format_number_in(const CliFormatTextCtx *args);
// the Rust edition of the crate that contains the file at `path`, as its Cargo.toml names it, or 2021 where the crate
// names none of its own, into `edition`, sized for CLI_NAME_MAX_LEN, under the formatting's lock (format.c)
void cli_format_edition_get(const CliFormatCtx *args);

struct CliFormatErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *path;
    const char *program;
    const CliRunResult *run_result;
};

struct CliFormatTextCtx
{
    const char *text;
    const char *table;
    const char *key;
    unsigned int *number;
};

struct CliFormatPathCtx
{
    const char *path;
    const char *const *names;
    unsigned int count;
};

struct CliFormatCtx
{
    const char *path;
    char *edition;
};

struct CliFormatLanguageCtx
{
    const char *language;
    VerbaLinea *err_msg;
};

struct CliFormatBytesCtx
{
    const char *bytes;
    unsigned long long size;
};

typedef struct
{
    unsigned int (*tool)(const CliFormatLanguageCtx *args);
} CliFormatLanguageNsTable;
EMBED_TABLE_LAYOUT(CliFormatLanguageNsTable, tool);
EMBED_TABLE_STORAGE CliFormatLanguageNsTable cli_format_language_internal EMBED_UNUSED = {
    .tool = cli_format_tool,
};

typedef struct
{
    int (*nearest)(const CliFormatPathCtx *args);
} CliFormatPathNsTable;
EMBED_TABLE_LAYOUT(CliFormatPathNsTable, nearest);
EMBED_TABLE_STORAGE CliFormatPathNsTable cli_format_path_internal EMBED_UNUSED = {
    .nearest = cli_format_nearest,
};

typedef struct
{
    int (*utf8)(const CliFormatBytesCtx *args);
} CliFormatBytesNs;
EMBED_TABLE_LAYOUT(CliFormatBytesNs, utf8);
EMBED_TABLE_STORAGE CliFormatBytesNs cli_format_bytes_internal EMBED_UNUSED = {
    .utf8 = cli_format_utf8,
};

typedef struct
{
    void (*fail)(const CliFormatErrMsgCtx *args);
} CliFormatErrMsgNs;
EMBED_TABLE_LAYOUT(CliFormatErrMsgNs, fail);
EMBED_TABLE_STORAGE CliFormatErrMsgNs cli_format_err_msg_internal EMBED_UNUSED = {
    .fail = cli_format_fail,
};

typedef struct
{
    int (*number_in)(const CliFormatTextCtx *args);
} CliFormatTextNs;
EMBED_TABLE_LAYOUT(CliFormatTextNs, number_in);
EMBED_TABLE_STORAGE CliFormatTextNs cli_format_text_internal EMBED_UNUSED = {
    .number_in = cli_format_number_in,
};

typedef struct
{
    void (*edition_get)(const CliFormatCtx *args);
} CliFormatNsTable;
EMBED_TABLE_LAYOUT(CliFormatNsTable, edition_get);
EMBED_TABLE_STORAGE CliFormatNsTable cli_format_internal EMBED_UNUSED = {
    .edition_get = cli_format_edition_get,
};

#endif
