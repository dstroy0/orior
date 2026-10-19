// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format_internal.h: what the format sources share: the manifest read and the toolchain that formats a language, and
// the nearest settings file read, under the formatting's lock
#ifndef FORMAT_INTERNAL_H
#define FORMAT_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "format.h"
#include "toolchains.h"

// The room the formatting reads into: the manifest and a settings file's text, each a cell sized to what it holds, the
// toolchain found, and the formatter's words
typedef struct
{
    CliJson json;
    CliFound found;
    char *settings;
    unsigned long long settings_size;
    char words[CLI_WORDS_TEXT];
    char path[CLI_PATH_TEXT];
} CliFormatRoom;

extern CliFormatRoom g_cli_format;
extern CliLock g_cli_format_lock;

// the room's manifest and settings given back, and the formatting's lock with them (format.c)
void cli_format_released(void);

// the manifest read into the room, and the toolchain that formats `language` in it: the tool, or CLI_JSON_NONE with
// why said where none does (format.c)
unsigned int cli_format_tool(const char *language, CliLine *said);

// the first of the files `names`, `count` of them, in the folder of `path` or the nearest folder above it that holds
// one, read whole into the room's settings: 1, or 0 where none is there or reads as UTF-8 (format_width.c)
int cli_format_nearest(const char *path, const char *const *names, unsigned int count);
// whether `size` bytes are UTF-8 whole; and that `program` could not format the file at `path` said, with the first
// six lines of what it said, trimmed, or that it said nothing (format_width.c)
int cli_format_utf8(const char *bytes, unsigned long long size);
void cli_format_failed(const char *program, const char *path, const CliRan *ran, CliLine *said);
// the whole number set for `key` in a settings file, TOML, YAML or JSON: within the TOML table `table` where one is
// named, and before any table where none is: 1 with it into `number`, or 0 (format_width.c)
int cli_format_number_in(const char *text, const char *table, const char *key, unsigned int *number);
// the Rust edition of the crate that holds the file at `path`, as its Cargo.toml names it, or 2021 where the crate
// names none of its own, into `edition`, which holds CLI_NAME_LONGEST, under the formatting's lock (format.c)
void cli_format_edition_of(const char *path, char *edition);

#endif
