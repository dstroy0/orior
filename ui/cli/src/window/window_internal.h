// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_internal.h: what the window's own calls share: the tables of the calls each source answers, read as the
// server's calls are read, their arguments by name; a string written as JSON; and orior's own folder, reported where
// there is none
#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H

#include "../command_line/cells.h"
#include "serve_internal.h"
#include "window.h"

// the cells a call on a text occupies while it answers (window_call_text.c), and a call on the reader's places
// (window_call_places.c)
extern CellOwner g_cli_window_text_cells;
extern CellOwner g_cli_window_places_cells;

// `text` written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them; `size` bytes of it the same;
// and a JSON array of the `count` strings of `items`
void cli_window_quoted(VerbaLinea *out, const char *text);
void cli_window_counted(VerbaLinea *out, const char *text, unsigned long long size);
void cli_window_strings(VerbaLinea *out, const char *const *items, unsigned int count);
// orior's own folder into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 with "orior_cli has no folder of its own"
// given
int cli_window_home(char *folder, VerbaLinea *err_msg);
// the argument `key` written as compact JSON into `out`, `null` where there is none
void cli_window_arg_written(const CliServeArgs *args, const char *key, VerbaLinea *out);
// the command the window was opened to run written as `{root, remote, menu, command, args}`, once, null after and
// where it was opened to run none (window.c)
void cli_window_launch_taken(VerbaLinea *out);
// a terminal's part of the comms layer: a key that listens and sends on `runs/*`, into `key`, sized for CLI_COMMS_KEY,
// and the door's address into `address`, sized for CLI_PATH_MAX_LEN: 1, or 0 where there is no door; the part bound to
// the shell's process; and the part let go as the shell ends (window.c)
int cli_window_terminal_key(char *key, char *address);
void cli_window_terminal_bound(const char *key, unsigned int process);
void cli_window_terminal_left(const char *key);
// the tree's call `name` of `size` bytes of JSON `args` answered, over the link where one is open, else here: its
// answer into `out`, 1, or 0 with why given (window.c)
int cli_window_tree_call(const char *name, const char *args, unsigned long long size, VerbaLinea *out,
                         VerbaLinea *err_msg);
// the tree's call `name` with `{"path": path}` and `more` after it, its answer read into `json`, a tree in a cell sized
// to it, given back with cli_json_given either way: its node, or CLI_JSON_NONE with why given (window_call_text.c)
unsigned int cli_window_tree_called(CliJson *json, const char *name, const char *path, const char *more,
                                    VerbaLinea *err_msg);

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
extern const CliServeCall g_cli_window_branch_calls[];
extern const unsigned int g_cli_window_branch_count;
extern const CliServeCall g_cli_window_code_calls[];
extern const unsigned int g_cli_window_code_count;
extern const CliServeCall g_cli_window_tree_calls[];
extern const unsigned int g_cli_window_tree_count;
extern const CliServeCall g_cli_window_memory_calls[];
extern const unsigned int g_cli_window_memory_count;
extern const CliServeCall g_cli_window_machine_calls[];
extern const unsigned int g_cli_window_machine_count;
extern const CliServeCall g_cli_window_scrollback_calls[];
extern const unsigned int g_cli_window_scrollback_count;
extern const CliServeCall g_cli_window_terminal_calls[];
extern const unsigned int g_cli_window_terminal_count;
extern const CliServeCall g_cli_window_clipboard_calls[];
extern const unsigned int g_cli_window_clipboard_count;
extern const CliServeCall g_cli_window_view_calls[];
extern const unsigned int g_cli_window_view_count;
extern const CliServeCall g_cli_window_editor_calls[];
extern const unsigned int g_cli_window_editor_count;

#endif
