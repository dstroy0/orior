// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_internal.h: what the window's own calls share: the tables of the calls each source answers, read as the
// server's calls are read, their arguments by name; a string written as JSON; and orior's own folder, reported where
// there is none
#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H
typedef struct CliWindowOutCtx CliWindowOutCtx;
typedef struct CliWindowOutSizeCtx CliWindowOutSizeCtx;
typedef struct CliWindowErrMsgCtx CliWindowErrMsgCtx;
typedef struct CliWindowOutRequestCtx CliWindowOutRequestCtx;
typedef struct CliWindowOutTextCtx CliWindowOutTextCtx;
typedef struct CliWindowOutItemsCtx CliWindowOutItemsCtx;
typedef struct CliWindowCtx CliWindowCtx;
typedef struct CliWindowKeyCtx CliWindowKeyCtx;
typedef struct CliWindowKeyAddressCtx CliWindowKeyAddressCtx;

#include "../command_line/cells.h"
#include "embed_dispatch_layout.h"
#include "serve_internal.h"
#include "window.h"

// the cells a call on a text occupies while it answers (window_call_text.c), and a call on the reader's places
// (window_call_places.c)
extern CellOwner g_cli_window_text_cells;
extern CellOwner g_cli_window_places_cells;

// `text` written as a JSON string, its bytes read as Rust's from_utf8_lossy reads them; `size` bytes of it the same;
// and a JSON array of the `count` strings of `items`
void cli_window_quote(const CliWindowOutTextCtx *args);
void cli_window_count(const CliWindowOutTextCtx *args);
void cli_window_strings(const CliWindowOutItemsCtx *args);
// orior's own folder into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 with "orior_cli has no folder of its own"
// given
int cli_window_home(const CliWindowCtx *args);
// the argument `key` written as compact JSON into `out`, `null` where there is none
void cli_window_arg_write(const CliWindowOutRequestCtx *args);
// the command the window was opened to run written as `{root, remote, menu, command, args}`, once, null after and
// where it was opened to run none (window.c)
void cli_window_launch_take(const CliWindowOutRequestCtx *args);
// a terminal's part of the comms layer: a key that listens and sends on `runs/*`, into `key`, sized for CLI_COMMS_KEY,
// and the door's address into `address`, sized for CLI_PATH_MAX_LEN: 1, or 0 where there is no door; the part bound to
// the shell's process; and the part let go as the shell ends (window.c)
int cli_window_terminal_key(const CliWindowKeyAddressCtx *args);
void cli_window_terminal_bind(const CliWindowKeyCtx *args);
void cli_window_terminal_leave(const CliWindowKeyCtx *args);
// the tree's call `name` of `size` bytes of JSON `args` answered, over the link where one is open, else here: its
// answer into `out`, 1, or 0 with why given (window.c)
int cli_window_tree_call(const CliWindowOutSizeCtx *args);
// the tree's call `name` with `{"path": path}` and `more` after it, its answer read into `json`, a tree in a cell sized
// to it, given back with cli_json_free either way: its node, or CLI_JSON_NONE with why given (window_call_text.c)
unsigned int cli_window_tree_on_call(const CliWindowErrMsgCtx *args);
// the tree's signature for the call at `line`, `col` of the branch under `key`, the file at `path`, written as
// `symbols_signature` writes it: 1, or 0 with why given (window_call_branch.c)
int cli_window_branch_signature(const CliWindowOutCtx *args);

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

struct CliWindowOutCtx
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *key;
    const char *path;
    unsigned long long line;
    unsigned long long col;
};

struct CliWindowOutSizeCtx
{
    VerbaLinea *out;
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *name;
    const char *args_text;
};

struct CliWindowErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *name;
    CliJson *json;
    const char *path;
    const char *more;
};

struct CliWindowOutRequestCtx
{
    VerbaLinea *out;
    const CliServeArgs *request;
    const char *key;
};

struct CliWindowOutTextCtx
{
    VerbaLinea *out;
    const char *text;
    unsigned long long size;
};

struct CliWindowOutItemsCtx
{
    VerbaLinea *out;
    const char *const *items;
    unsigned int count;
};

struct CliWindowCtx
{
    char *folder;
    VerbaLinea *err_msg;
};

struct CliWindowKeyCtx
{
    const char *key;
    unsigned int process;
};

struct CliWindowKeyAddressCtx
{
    char *key_out;
    char *address;
};

typedef struct
{
    void (*quote)(const CliWindowOutTextCtx *args);
    void (*count)(const CliWindowOutTextCtx *args);
} CliWindowOutTextNs;
EMBED_TABLE_LAYOUT(CliWindowOutTextNs, quote, count);
EMBED_TABLE_STORAGE CliWindowOutTextNs cli_window_out_text_internal EMBED_UNUSED = {
    .quote = cli_window_quote,
    .count = cli_window_count,
};

typedef struct
{
    void (*strings)(const CliWindowOutItemsCtx *args);
} CliWindowOutItemsNs;
EMBED_TABLE_LAYOUT(CliWindowOutItemsNs, strings);
EMBED_TABLE_STORAGE CliWindowOutItemsNs cli_window_out_items_internal EMBED_UNUSED = {
    .strings = cli_window_strings,
};

typedef struct
{
    int (*home)(const CliWindowCtx *args);
} CliWindowNsTable;
EMBED_TABLE_LAYOUT(CliWindowNsTable, home);
EMBED_TABLE_STORAGE CliWindowNsTable cli_window_internal EMBED_UNUSED = {
    .home = cli_window_home,
};

typedef struct
{
    void (*arg_write)(const CliWindowOutRequestCtx *args);
    void (*launch_take)(const CliWindowOutRequestCtx *args);
} CliWindowOutRequestNs;
EMBED_TABLE_LAYOUT(CliWindowOutRequestNs, arg_write, launch_take);
EMBED_TABLE_STORAGE CliWindowOutRequestNs cli_window_out_request_internal EMBED_UNUSED = {
    .arg_write = cli_window_arg_write,
    .launch_take = cli_window_launch_take,
};

typedef struct
{
    int (*terminal_key)(const CliWindowKeyAddressCtx *args);
} CliWindowKeyAddressNs;
EMBED_TABLE_LAYOUT(CliWindowKeyAddressNs, terminal_key);
EMBED_TABLE_STORAGE CliWindowKeyAddressNs cli_window_key_address_internal EMBED_UNUSED = {
    .terminal_key = cli_window_terminal_key,
};

typedef struct
{
    void (*terminal_bind)(const CliWindowKeyCtx *args);
    void (*terminal_leave)(const CliWindowKeyCtx *args);
} CliWindowKeyNs;
EMBED_TABLE_LAYOUT(CliWindowKeyNs, terminal_bind, terminal_leave);
EMBED_TABLE_STORAGE CliWindowKeyNs cli_window_key_internal EMBED_UNUSED = {
    .terminal_bind = cli_window_terminal_bind,
    .terminal_leave = cli_window_terminal_leave,
};

typedef struct
{
    int (*tree_call)(const CliWindowOutSizeCtx *args);
} CliWindowOutSizeNs;
EMBED_TABLE_LAYOUT(CliWindowOutSizeNs, tree_call);
EMBED_TABLE_STORAGE CliWindowOutSizeNs cli_window_out_size_internal EMBED_UNUSED = {
    .tree_call = cli_window_tree_call,
};

typedef struct
{
    unsigned int (*tree_call)(const CliWindowErrMsgCtx *args);
} CliWindowErrMsgNs;
EMBED_TABLE_LAYOUT(CliWindowErrMsgNs, tree_call);
EMBED_TABLE_STORAGE CliWindowErrMsgNs cli_window_err_msg_internal EMBED_UNUSED = {
    .tree_call = cli_window_tree_on_call,
};

typedef struct
{
    int (*branch_signature)(const CliWindowOutCtx *args);
} CliWindowOutNs;
EMBED_TABLE_LAYOUT(CliWindowOutNs, branch_signature);
EMBED_TABLE_STORAGE CliWindowOutNs cli_window_out_internal EMBED_UNUSED = {
    .branch_signature = cli_window_branch_signature,
};

#endif
