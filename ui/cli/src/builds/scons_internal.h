// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_internal.h: SCons's SConstruct and SConscript files as their reader reads them: Python whose bytes are code, a
// string's or a comment's; the calls of a name and their arguments, the strings and the keywords among them; the names
// a file's `Import` calls take and those it exports; and the SConscripts its `SConscript` calls read (scons.c,
// scons_check.c)
#ifndef SCONS_INTERNAL_H
#define SCONS_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "builds_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSconsTextCtx CliSconsTextCtx;
typedef struct CliSconsFromCtx CliSconsFromCtx;
typedef struct CliSconsEachCtx CliSconsEachCtx;
typedef struct CliSconsTextSizeCtx CliSconsTextSizeCtx;
typedef struct CliSconsTextSizeContextCtx CliSconsTextSizeContextCtx;
typedef struct CliSconsNameCtx CliSconsNameCtx;
typedef struct CliSconsTextSizeClassOfCtx CliSconsTextSizeClassOfCtx;
typedef struct CliSconsContextCtx CliSconsContextCtx;
typedef struct CliSconsContextEachScriptCtx CliSconsContextEachScriptCtx;
typedef struct CliSconsCtx CliSconsCtx;

// what a byte of a file is
#define CLI_SCONS_CODE 0u
#define CLI_SCONS_STRING 1u
#define CLI_SCONS_COMMENT 2u

// the max bytes of a file read
#define CLI_SCONS_TEXT (1u << 24u)

// A file as read, under the reader's lock: its text and each byte's class, a cell of the scons cellblock's temporary
// end the caller took for the read
typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned char *class_of;
} CliSconsFile;

extern CliSconsFile g_cli_scons;

// the names SCons gives an SConstruct and an SConscript, an environment's methods, the construction variables an
// environment contains usually, and the calls whose keyword arguments are construction variables (scons.c)
extern const char *const g_cli_scons_globals[76];
extern const char *const g_cli_scons_methods[52];
extern const char *const g_cli_scons_variables[44];
extern const char *const g_cli_scons_setters[16];

// `size` bytes of a file read, its bytes classed into `class_of`, sized for `size`: 1, or 0 where it is larger than
// CLI_SCONS_TEXT (scons.c)
int cli_scons_read(const CliSconsTextSizeClassOfCtx *args);

// A span of the file read: where it starts and ends
typedef void (*CliSconsSpan)(const CliSconsSpanCfg *args);

// each call of `name` in code, where its arguments start, after its `(`, and where they end, at its `)`; each string
// from `from` to `to`, the span inside its quotes; and each argument of a call, split at its commas that are outside
// brackets, passed to `each` (scons.c)
void cli_scons_calls(const CliSconsNameCtx *args);
void cli_scons_strings(const CliSconsEachCtx *args);
void cli_scons_arguments(const CliSconsEachCtx *args);
// the keyword an argument from `from` to `to` gives, where it gives one: 1 with its name's span and where its value
// starts, or 0
int cli_scons_keyword(const CliSconsFromCtx *args);

// each name the `Import` calls of the file read take, with its span, passed to `each`; and each name the file read
// exports, by `Export`, its strings and its keywords, and by an `exports` given an SConscript call, passed to `named`
void cli_scons_import(const CliSconsNameCtx *args);
void cli_scons_export(const CliSconsContextCtx *args);

// each SConscript the `SConscript` calls of the file read read, as written, with the span of the string that names it,
// passed to `each`
typedef void (*CliSconsScript)(const CliSconsScriptCfg *args);
void cli_scons_read_scripts(const CliSconsContextEachScriptCtx *args);

// the findings of the SConstruct or SConscript at `path` of the tree at `root`, `size` bytes of its text; the names
// SCons gives it, its own and those it imports, each passed to `each`; and what completes the word before byte `at`:
// an environment's methods after its name and a dot, its construction variables in its brackets and as the keywords of
// the calls that set them, and SCons's names (scons_check.c)
void cli_scons_check(const CliSconsTextCtx *args);
void cli_scons_free(const CliSconsTextSizeContextCtx *args);
void cli_scons_complete(const CliSconsTextSizeCtx *args);
// the scons checks' own report, `<pool's bytes> <their mark> 0`, the max their capacities occupied of the cellblock at
// once, written into `out` (scons_check.c)
void cli_scons_cells_usage_write(const CliSconsCtx *args);

struct CliSconsTextCtx
{
    const char *text;
    unsigned long long size;
    const char *root;
    const char *path;
    const CliBuildLines *lines;
    CliBuildFindings *found;
};

struct CliSconsFromCtx
{
    unsigned long long from;
    unsigned long long to;
    unsigned long long *name_u64s;
    unsigned long long *name_size;
    unsigned long long *value;
};

struct CliSconsEachCtx
{
    CliSconsSpan each;
    void *context;
    unsigned long long from;
    unsigned long long to;
};

struct CliSconsTextSizeCtx
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    CliCompletions *items;
};

struct CliSconsTextSizeContextCtx
{
    const char *text;
    unsigned long long size;
    void *context;
    void (*each_callback)(const CliEachNameSizeCfg *args);
};

struct CliSconsNameCtx
{
    const char *name;
    CliSconsSpan each;
    void *context;
};

struct CliSconsTextSizeClassOfCtx
{
    const char *text;
    unsigned long long size;
    unsigned char *class_of;
};

struct CliSconsContextCtx
{
    void *context;
    void (*named)(const CliEachNameSizeCfg *args);
};

struct CliSconsContextEachScriptCtx
{
    void *context;
    CliSconsScript each_scons_script;
};

struct CliSconsCtx
{
    VerbaLinea *out;
};

typedef struct
{
    int (*read)(const CliSconsTextSizeClassOfCtx *args);
} CliSconsTextSizeClassOfNs;
EMBED_TABLE_LAYOUT(CliSconsTextSizeClassOfNs, read);
EMBED_TABLE_STORAGE CliSconsTextSizeClassOfNs cli_scons_text_size_class_of_internal EMBED_UNUSED = {
    .read = cli_scons_read,
};

typedef struct
{
    void (*calls)(const CliSconsNameCtx *args);
    void (*import)(const CliSconsNameCtx *args);
} CliSconsNameNs;
EMBED_TABLE_LAYOUT(CliSconsNameNs, calls, import);
EMBED_TABLE_STORAGE CliSconsNameNs cli_scons_name_internal EMBED_UNUSED = {
    .calls = cli_scons_calls,
    .import = cli_scons_import,
};

typedef struct
{
    void (*strings)(const CliSconsEachCtx *args);
    void (*arguments)(const CliSconsEachCtx *args);
} CliSconsEachNs;
EMBED_TABLE_LAYOUT(CliSconsEachNs, strings, arguments);
EMBED_TABLE_STORAGE CliSconsEachNs cli_scons_each_internal EMBED_UNUSED = {
    .strings = cli_scons_strings,
    .arguments = cli_scons_arguments,
};

typedef struct
{
    int (*keyword)(const CliSconsFromCtx *args);
} CliSconsFromNs;
EMBED_TABLE_LAYOUT(CliSconsFromNs, keyword);
EMBED_TABLE_STORAGE CliSconsFromNs cli_scons_from_internal EMBED_UNUSED = {
    .keyword = cli_scons_keyword,
};

typedef struct
{
    void (*export)(const CliSconsContextCtx *args);
} CliSconsContextNs;
EMBED_TABLE_LAYOUT(CliSconsContextNs, export);
EMBED_TABLE_STORAGE CliSconsContextNs cli_scons_context_internal EMBED_UNUSED = {
    .export = cli_scons_export,
};

typedef struct
{
    void (*read_scripts)(const CliSconsContextEachScriptCtx *args);
} CliSconsContextEachScriptNs;
EMBED_TABLE_LAYOUT(CliSconsContextEachScriptNs, read_scripts);
EMBED_TABLE_STORAGE CliSconsContextEachScriptNs cli_scons_context_each_script_internal EMBED_UNUSED = {
    .read_scripts = cli_scons_read_scripts,
};

typedef struct
{
    void (*check)(const CliSconsTextCtx *args);
} CliSconsTextNs;
EMBED_TABLE_LAYOUT(CliSconsTextNs, check);
EMBED_TABLE_STORAGE CliSconsTextNs cli_scons_text_internal EMBED_UNUSED = {
    .check = cli_scons_check,
};

typedef struct
{
    void (*free)(const CliSconsTextSizeContextCtx *args);
} CliSconsTextSizeContextNs;
EMBED_TABLE_LAYOUT(CliSconsTextSizeContextNs, free);
EMBED_TABLE_STORAGE CliSconsTextSizeContextNs cli_scons_text_size_context_internal EMBED_UNUSED = {
    .free = cli_scons_free,
};

typedef struct
{
    void (*complete)(const CliSconsTextSizeCtx *args);
} CliSconsTextSizeNs;
EMBED_TABLE_LAYOUT(CliSconsTextSizeNs, complete);
EMBED_TABLE_STORAGE CliSconsTextSizeNs cli_scons_text_size_internal EMBED_UNUSED = {
    .complete = cli_scons_complete,
};

typedef struct
{
    void (*cells_usage_write)(const CliSconsCtx *args);
} CliSconsNs;
EMBED_TABLE_LAYOUT(CliSconsNs, cells_usage_write);
EMBED_TABLE_STORAGE CliSconsNs cli_scons_internal EMBED_UNUSED = {
    .cells_usage_write = cli_scons_cells_usage_write,
};

#endif
