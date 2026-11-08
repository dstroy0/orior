// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate.h: a tool plugin checks a proof with its prover and distinguishes a failure of the mathematics from a limit
// of the machine. A tool plugin is a folder containing plugin.json, as a language plugin is, of type "tool":
//
//   languages               the languages whose files it checks
//   toolchain               the toolchain in the manifest its program is found by
//   run                     { program, args, project, inProject, patience }: the program and its words, `{file}` the
//                           file; the names of the files that mark a project's folder, and the words that run the
//                           program inside the project, from its folder; and the seconds one check may take
//   barriers                each a limit of the machine: `types`, the types of message the prover gives where the
//                           limit stopped the check, or `phrases`, words such a message contains, and `not`, words it
//                           does not; `lift`, the word that sets the limit, `{value}` its value; and `default`, `grow`
//                           and `ceiling`, where it starts, how many times it grows a step, and how far it may go
//   flags                   each a list of `words`, the `severity` a use of one is, and its `message`
//   audit                   { declarations, print, trusted }: the words that open a theorem, the line that requests
//                           the axioms one rests on, `{name}` its name, and the axioms that are trusted
//
// The file is checked as a copy, with the lines that request each theorem's axioms after it. Where a check stops at a
// barrier, it is checked again with that barrier's limit grown a step, until it no longer stops there, the limit
// reaches its ceiling, or CLI_VALIDATE_CHECKS checks have run. A limit the file sets for itself, with
// `set_option <name> <value>`, is grown in the copy too, since the file's own setting comes after the one the program
// is given. What is left once no barrier stops the check is the mathematics (validate.c, validate_tool.c,
// validate_run.c, validate_code.c, validate_messages.c, validate_axioms.c, validate_report.c)
#ifndef VALIDATE_H
#define VALIDATE_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliValidatePathCfg CliValidatePathCfg;
typedef struct CliValidateCfg CliValidateCfg;

// the most checks one validation runs
#define CLI_VALIDATE_CHECKS 16u

// the id of the tool plugin that checks files of `language`, the first by id of those that come with orior and the
// reader's, into `id`, sized for CLI_NAME_MAX_LEN: 1, or 0 where none does (validate_tool.c)
int cli_validate_tool_for(const CliValidateCfg *args);

// the file at `path` validated with the tool plugin `id`: checked, each barrier that stops it lifted as far as its
// ceiling, and what is left reported, the flags and the axioms, written as `{"tool", "file", "project", "passes",
// "verdict", "checks": [{"settings", "seconds", "errors", "barriers"}], "findings": [{"line", "col", "end_line",
// "end_col", "severity", "type", "barrier", "message", "lifted"}]}`: 1, or 0 with why given (validate.c)
int cli_on_validate(const CliValidatePathCfg *args);

struct CliValidatePathCfg
{
    const char *id_read;
    const char *path;
    VerbaLinea *report;
    VerbaLinea *err_msg;
};

struct CliValidateCfg
{
    const char *language;
    char *id;
};

typedef struct
{
    int (*tool_for)(const CliValidateCfg *args);
} CliValidateNs;
EMBED_TABLE_LAYOUT(CliValidateNs, tool_for);
EMBED_TABLE_STORAGE CliValidateNs cli_validate EMBED_UNUSED = {
    .tool_for = cli_validate_tool_for,
};

typedef struct
{
    int (*validate)(const CliValidatePathCfg *args);
} CliValidatePathNs;
EMBED_TABLE_LAYOUT(CliValidatePathNs, validate);
EMBED_TABLE_STORAGE CliValidatePathNs cli_validate_path EMBED_UNUSED = {
    .validate = cli_on_validate,
};

#endif
