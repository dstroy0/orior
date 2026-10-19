// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate.h: a tool plugin checks a proof with its prover and tells a failure of the mathematics from a limit of the
// machine. A tool plugin is a folder holding plugin.json, as a language plugin is, of kind "tool":
//
//   languages               the languages whose files it checks
//   toolchain               the toolchain in the manifest its program is found by
//   run                     { program, args, project, inProject, patience }: the program and its words, `{file}` the
//                           file; the names of the files that mark a project's folder, and the words that run the
//                           program inside the project, from its folder; and the seconds one check may take
//   barriers                each a limit of the machine: `kinds`, the kinds of message the prover gives where the
//                           limit stopped the check, or `says`, words such a message holds, and `not`, words it does
//                           not; `lift`, the word that sets the limit, `{value}` its value; and `default`, `grow` and
//                           `ceiling`, where it starts, how many times it grows a step, and how far it may go
//   flags                   each a list of `words`, the `severity` a use of one is, and what it `says`
//   audit                   { declarations, print, trusted }: the words that open a theorem, the line that asks for
//                           the axioms one rests on, `{name}` its name, and the axioms that are trusted
//
// The file is checked as a copy, with the lines that ask for each theorem's axioms after it. Where a check stops at a
// barrier, it is checked again with that barrier's limit grown a step, until it no longer stops there, the limit
// reaches its ceiling, or CLI_VALIDATE_CHECKS checks have run. A limit the file sets for itself, with
// `set_option <name> <value>`, is grown in the copy too, since the file's own setting comes after the one the program
// is given. What is left once no barrier stops the check is the mathematics (validate.c, validate_tool.c,
// validate_run.c, validate_code.c, validate_messages.c, validate_axioms.c, validate_report.c)
#ifndef VALIDATE_H
#define VALIDATE_H

#include "../command_line/cli_internal.h"

// the most checks one validation runs
#define CLI_VALIDATE_CHECKS 16u

// the id of the tool plugin that checks files of `language`, the first by id of those that come with orior and the
// reader's, into `id`, which holds CLI_NAME_LONGEST: 1, or 0 where none does (validate_tool.c)
int cli_validate_tool_for(const char *language, char *id);

// the file at `path` validated with the tool plugin `id`: checked, each barrier that stops it lifted as far as its
// ceiling, and what is left reported, the flags and the axioms, written as `{"tool", "file", "project", "holds",
// "verdict", "checks": [{"settings", "seconds", "errors", "barriers"}], "findings": [{"line", "col", "end_line",
// "end_col", "severity", "kind", "barrier", "message", "lifted"}]}`: 1, or 0 with why said (validate.c)
int cli_validated(const char *id, const char *path, CliLine *report, CliLine *said);

#endif
