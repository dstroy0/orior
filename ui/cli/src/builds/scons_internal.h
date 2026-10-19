// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_internal.h: SCons's SConstruct and SConscript files as their reader reads them: Python whose bytes are code, a
// string's or a comment's; the calls of a name and their arguments, the strings and the keywords among them; the names
// a file's `Import` calls take and those it exports; and the SConscripts its `SConscript` calls read (scons.c,
// scons_check.c)
#ifndef SCONS_INTERNAL_H
#define SCONS_INTERNAL_H

#include "../text/text.h"
#include "builds_internal.h"

// what a byte of a file is
#define CLI_SCONS_CODE 0u
#define CLI_SCONS_STRING 1u
#define CLI_SCONS_COMMENT 2u

// the most bytes of a file read
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
extern CliLock g_cli_scons_lock;

// the names SCons gives an SConstruct and an SConscript, an environment's methods, the construction variables an
// environment holds most often, and the calls whose keyword arguments are construction variables (scons.c)
extern const char *const g_cli_scons_globals[76];
extern const char *const g_cli_scons_methods[52];
extern const char *const g_cli_scons_variables[44];
extern const char *const g_cli_scons_setters[16];

// `size` bytes of a file read, its bytes classed into `class_of`, which holds `size`: 1, or 0 where it is larger than
// CLI_SCONS_TEXT (scons.c)
int cli_scons_read(const char *text, unsigned long long size, unsigned char *class_of);

// A span of the file read: where it starts and ends
typedef void (*CliSconsSpan)(void *context, unsigned long long from, unsigned long long to);

// each call of `name` in code, where its arguments start, after its `(`, and where they end, at its `)`; each string
// from `from` to `to`, the span inside its quotes; and each argument of a call, split at its commas that stand outside
// brackets, handed to `each` (scons.c)
void cli_scons_calls(const char *name, CliSconsSpan each, void *context);
void cli_scons_strings(unsigned long long from, unsigned long long to, CliSconsSpan each, void *context);
void cli_scons_arguments(unsigned long long from, unsigned long long to, CliSconsSpan each, void *context);
// the keyword an argument from `from` to `to` gives, where it gives one: 1 with its name's span and where its value
// starts, or 0
int cli_scons_keyword(unsigned long long from, unsigned long long to, unsigned long long *name,
                      unsigned long long *name_size, unsigned long long *value);

// each name the `Import` calls of the file read take, with its span, handed to `each`; and each name the file read
// exports, by `Export`, its strings and its keywords, and by an `exports` given an SConscript call, handed to `named`
void cli_scons_imported(CliSconsSpan each, void *context);
void cli_scons_exported(void (*named)(void *context, const char *name, unsigned long long size), void *context);

// each SConscript the `SConscript` calls of the file read read, as written, with the span of the string that names it,
// handed to `each`
typedef void (*CliSconsScript)(void *context, const char *written, unsigned long long size, unsigned long long from,
                               unsigned long long to);
void cli_scons_read_scripts(CliSconsScript each, void *context);

// the findings of the SConstruct or SConscript at `path` of the tree at `root`, `size` bytes of its text; the names
// SCons gives it, its own and those it imports, each handed to `each`; and what completes the word before byte `at`:
// an environment's methods after its name and a dot, its construction variables in its brackets and as the keywords of
// the calls that set them, and SCons's names (scons_check.c)
void cli_scons_check(const char *root, const char *path, const char *text, unsigned long long size,
                     const CliBuildLines *lines, CliBuildFindings *found);
void cli_scons_given(const char *text, unsigned long long size,
                     void (*each)(void *context, const char *name, unsigned long long size), void *context);
void cli_scons_complete(const char *text, unsigned long long size, unsigned long long at, CliCompletions *items);
// the scons checks' own report, `<pool's bytes> <their mark> 0`, the most their rooms held of the cellblock at once,
// written into `out` (scons_check.c)
void cli_scons_cells_said(CliLine *out);

#endif
