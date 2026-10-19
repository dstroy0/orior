// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree.h: the tree's index: each Python and JavaScript file's facts, by its path in the tree, kept in a room of
// its own; what the tree says across its files, an import that leads back round to the file that makes it and an
// attribute no class of its family sets; what each Python class's family sets; and the call hierarchy where no
// language server answers. One index, under its own lock (inspect_tree.c, inspect_tree_*.c)
#ifndef INSPECT_TREE_H
#define INSPECT_TREE_H

#include "inspect.h"

// the facts of the file at `path` set from `size` bytes of its text in `language`, or the file taken out where the
// inspections do not read its language or its facts do not fit: 1 where it is set; a file taken out; every file taken
// out; and whether the index holds a file
int cli_inspect_tree_set(const char *path, const char *language, const char *text, unsigned long long size);
void cli_inspect_tree_remove(const char *path);
void cli_inspect_tree_cleared(void);
int cli_inspect_tree_holds(const char *path);

// each file of the index handed to `each`
void cli_inspect_tree_paths(void (*each)(void *context, const char *path), void *context);

// the function of the file at `path` that holds `line` most closely, and the one whose name starts on `line`, into
// `at`, its place among the file's functions: 1, or 0 where it has none
int cli_inspect_tree_function_at(const char *path, unsigned int line, unsigned int *at);
int cli_inspect_tree_function_on(const char *path, unsigned int line, unsigned int *at);

// A function the call hierarchy reaches: the path of its file, that file's facts and its place among their functions,
// and the spans of the calls that lead there, `count` of them
typedef void (*CliInspectReached)(void *context, const char *path, const CliFacts *facts, unsigned int function,
                                  const CliInspectSpan *sites, unsigned int count);

// the function at `at` of the file at `path` handed to `each` with no calls, under the index's lock: 1, or 0 where the
// index has none there
int cli_inspect_tree_function(const char *path, unsigned int at, CliInspectReached each, void *context);

// the functions the function at `at` of the file at `path` calls, each with the spans of its calls there; and the
// functions that call it, each with the spans of its calls of it; each handed to `each` under the index's lock
void cli_inspect_tree_callees(const char *path, unsigned int at, CliInspectReached each, void *context);
void cli_inspect_tree_callers(const char *path, unsigned int at, CliInspectReached each, void *context);

// every finding in every file: each file's path handed to `each` with no finding first, then each of its findings
// written as the protocol's diagnostic, what the file says alone and then what the tree says
typedef void (*CliInspectFound)(void *context, const char *path, const char *finding, unsigned long long size);
void cli_inspect_tree_findings(CliInspectFound each, void *context);

// what each Python class's family sets, by its file and its name: what it, the classes it comes from, the classes that
// come from it and the classes those come from set, each attribute handed to `each`
typedef void (*CliInspectSupplied)(void *context, const char *path, const char *class_name, const char *attribute);
void cli_inspect_tree_supplied(CliInspectSupplied each, void *context);

#endif
