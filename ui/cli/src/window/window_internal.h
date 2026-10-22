// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_internal.h: what the window's own calls share: the tables of the calls each source answers, read as the
// server's calls are read, their arguments by name; a string written as JSON; and orior's own folder, said where
// there is none
#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H

#include "../command_line/cells.h"
#include "serve_internal.h"
#include "window.h"

// the cells a call on a text holds while it answers (window_call_text.c), and a call on the reader's places
// (window_call_places.c)
extern CliCellsTally g_cli_window_text_cells;
extern CliCellsTally g_cli_window_places_cells;

// `text` written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them; `size` bytes of it the same;
// and a JSON array of the `count` strings of `items`
void cli_window_quoted(CliLine *out, const char *text);
void cli_window_counted(CliLine *out, const char *text, unsigned long long size);
void cli_window_strings(CliLine *out, const char *const *items, unsigned int count);
// orior's own folder into `folder`, which holds CLI_PATH_LONGEST: 1, or 0 with "orior_cli has no folder of its own"
// said
int cli_window_home(char *folder, CliLine *said);
// the argument `key` written as compact JSON into `out`, `null` where there is none
void cli_window_arg_written(const CliServeArgs *args, const char *key, CliLine *out);
// the command the window was opened to run written as `{root, remote, menu, command, args}`, once, null after and
// where it was opened to run none (window.c)
void cli_window_launch_taken(CliLine *out);
// the tree's call `name` of `size` bytes of JSON `args` answered, over the link where one is open, else here: its
// answer into `out`, 1, or 0 with why said (window.c)
int cli_window_tree_call(const char *name, const char *args, unsigned long long size, CliLine *out, CliLine *said);

// the calls of each source, each table's count beside it (window_call_*.c)
extern const CliServeCall g_cli_window_home_calls[];
extern const unsigned int g_cli_window_home_count;
extern const CliServeCall g_cli_window_places_calls[];
extern const unsigned int g_cli_window_places_count;
extern const CliServeCall g_cli_window_net_calls[];
extern const unsigned int g_cli_window_net_count;
extern const CliServeCall g_cli_window_report_calls[];
extern const unsigned int g_cli_window_report_count;
extern const CliServeCall g_cli_window_text_calls[];
extern const unsigned int g_cli_window_text_count;
extern const CliServeCall g_cli_window_code_calls[];
extern const unsigned int g_cli_window_code_count;
extern const CliServeCall g_cli_window_tree_calls[];
extern const unsigned int g_cli_window_tree_count;

#endif
