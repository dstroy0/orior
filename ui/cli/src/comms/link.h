// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.h: a tree on another machine, and the window's link to the tree's side there.
//
// An address names the machine and the tree's folder on it: `user@host:folder` over ssh, `docker:container:folder` in
// a running container over docker exec, and `wsl:distribution:folder` in a WSL distribution. The link queries the
// machine what it is, sends it orior's server for it where the machine does not have the one this window was built
// with, and starts `orior_cli serve` there, whose calls it answers by id and whose events it hands on. Where the link
// drops, the calls waiting on it are answered with the drop, and it is made again, sooner at first and then every
// 30 s; each change is reported as the event "link".
//
// ssh runs with BatchMode, its keys and agent its only way in: a password has no terminal to be typed at. ORIOR_SSH
// names another ssh, with words of its own before ssh's, and ORIOR_SERVER the server to send whatever the machine is
// (link_address.c, link_prepare.c, link.c)
#ifndef LINK_H
#define LINK_H
typedef struct CliLinkSizeCfg CliLinkSizeCfg;
typedef struct CliLinkSizeOutCfg CliLinkSizeOutCfg;
typedef struct CliLinkErrMsgCfg CliLinkErrMsgCfg;
typedef struct CliLinkErrMsgAddressCfg CliLinkErrMsgAddressCfg;
typedef struct CliLinkAddressCfg CliLinkAddressCfg;
typedef struct CliLinkTextCfg CliLinkTextCfg;
typedef struct CliLinkTextSizeCfg CliLinkTextSizeCfg;
typedef struct CliLinkOutCfg CliLinkOutCfg;
typedef struct CliLinkErrMsgSystemCfg CliLinkErrMsgSystemCfg;
typedef struct CliLinkOutPathCfg CliLinkOutPathCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// how the machine is reached
#define CLI_LINK_SSH 0u
#define CLI_LINK_DOCKER 1u
#define CLI_LINK_WSL 2u

// A tree on another machine: how it is reached, the machine's name for that way, `user@host`, the container or the
// distribution, and the tree's folder there
typedef struct
{
    unsigned int way;
    char name[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
} CliLinkAddress;

// what the link hands on: an event, `link` for its own, and its body, `size` bytes of JSON
typedef void (*CliLinkEventFn)(void *context, const char *event, const char *body, unsigned long long size);

// `size` bytes of `text` written as one word of a POSIX shell's line; and a path as one, its backslashes made slashes
void cli_link_quote(const CliLinkTextSizeCfg *args);
void cli_link_path_word(const CliLinkOutPathCfg *args);
// an address read from `text`: 1, or 0 with why given; the address written as that reads it; and the machine, as the
// window names it
int cli_link_address_parse(const CliLinkTextCfg *args);
void cli_link_address_text(const CliLinkOutCfg *args);
void cli_link_machine(const CliLinkOutCfg *args);
// the program and its words that run the shell line `line` on the machine, with a terminal where `terminal` is 1 and
// over its input and output where it is 0, into `words`; and the shell line that opens the reader's shell in
// `folder`, or the tree's folder where it is NULL, on the machine
void cli_link_runs(const CliLinkAddressCfg *args);
void cli_link_shell_line(const CliLinkOutCfg *args);
// `line` run on the machine, `size` bytes of `input` written to it where `input` is not NULL, and what it wrote
// written into `out` as Rust's from_utf8_lossy reads it: 1, or 0 with what it reported where it failed (link_address.c)
int cli_link_remote_run(const CliLinkSizeCfg *args);

// what is being done while the link is made, reported as its state `state` with `detail` beside it
typedef void (*CliLinkStated)(const CliLinkStatedCfg *args);

// orior's server put on the machine where it is not there, what is being done reported to `stated` as the state
// `preparing` while it waits, and the shell line that starts it on the tree written into `start`: 1, or 0 with why
// given. A server built here for the machine is built by the tree's ui/cli/build.sh, given the machine's target,
// into build/cli/<target>/orior_cli (link_prepare.c)
int cli_link_prepare(const CliLinkErrMsgCfg *args);
// whether `uname -sm` reports the machine is Windows; and the machine's target by what it reports, into `target`, which
// is sized for CLI_NAME_MAX_LEN: 1, or 0 with why given (link_prepare.c)
int cli_link_is_windows(const CliLinkErrMsgSystemCfg *args);
int cli_link_target(const CliLinkErrMsgSystemCfg *args);

// the link to the tree at `address` opened, made on a thread of its own and made again as it drops, the server's
// events and the link's own handed to `emit`: 1, or 0 with why given where a link is open already; how it stands, as it
// was last reported, written as `{state, machine, address, message}`; `name` called on the tree's side with `size`
// bytes of JSON `args`, waiting up to 60 s for the link to be made, its answer, JSON, written into `out`: 1, or 0 with
// why given; the link ended, and the server on the machine with it; and the server ended, as a dropped connection would
// end it, for the link to make again (link.c)
int cli_link_open(const CliLinkErrMsgAddressCfg *args);
void cli_link_standing(const CliLinkOutPathCfg *args);
int cli_link_call(const CliLinkSizeOutCfg *args);
void cli_link_end(void);
void cli_link_drop_now(void);
// the address of the open link into `address`: 1, or 0 where none is open
int cli_link_address(const CliLinkTextCfg *args);

struct CliLinkSizeCfg
{
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const CliLinkAddress *address;
    const char *line;
    const char *input;
};

struct CliLinkSizeOutCfg
{
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *name;
    const char *request;
};

struct CliLinkErrMsgCfg
{
    VerbaLinea *err_msg;
    const CliLinkAddress *address;
    CliLinkStated stated;
    void *context;
    VerbaLinea *start;
};

struct CliLinkErrMsgAddressCfg
{
    VerbaLinea *err_msg;
    const CliLinkAddress *address;
    void *context;
    CliLinkEventFn emit;
};

struct CliLinkAddressCfg
{
    const CliLinkAddress *address;
    const char *line;
    int terminal;
    CliWords *words;
};

struct CliLinkTextCfg
{
    const char *text;
    CliLinkAddress *address_out;
    VerbaLinea *err_msg;
};

struct CliLinkTextSizeCfg
{
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
};

struct CliLinkOutCfg
{
    VerbaLinea *out;
    const CliLinkAddress *address;
    const char *folder;
};

struct CliLinkErrMsgSystemCfg
{
    VerbaLinea *err_msg;
    const char *system;
    char *target;
};

struct CliLinkOutPathCfg
{
    VerbaLinea *out;
    const char *path;
};

typedef struct
{
    void (*quote)(const CliLinkTextSizeCfg *args);
} CliLinkTextSizeNs;
EMBED_TABLE_LAYOUT(CliLinkTextSizeNs, quote);
EMBED_TABLE_STORAGE CliLinkTextSizeNs cli_link_text_size EMBED_UNUSED = {
    .quote = cli_link_quote,
};

typedef struct
{
    void (*path_word)(const CliLinkOutPathCfg *args);
    void (*standing)(const CliLinkOutPathCfg *args);
} CliLinkOutPathNs;
EMBED_TABLE_LAYOUT(CliLinkOutPathNs, path_word, standing);
EMBED_TABLE_STORAGE CliLinkOutPathNs cli_link_out_path EMBED_UNUSED = {
    .path_word = cli_link_path_word,
    .standing = cli_link_standing,
};

typedef struct
{
    int (*address_parse)(const CliLinkTextCfg *args);
    int (*address)(const CliLinkTextCfg *args);
} CliLinkTextNs;
EMBED_TABLE_LAYOUT(CliLinkTextNs, address_parse, address);
EMBED_TABLE_STORAGE CliLinkTextNs cli_link_text EMBED_UNUSED = {
    .address_parse = cli_link_address_parse,
    .address = cli_link_address,
};

typedef struct
{
    void (*address_text)(const CliLinkOutCfg *args);
    void (*machine)(const CliLinkOutCfg *args);
    void (*shell_line)(const CliLinkOutCfg *args);
} CliLinkOutNs;
EMBED_TABLE_LAYOUT(CliLinkOutNs, address_text, machine, shell_line);
EMBED_TABLE_STORAGE CliLinkOutNs cli_link_out EMBED_UNUSED = {
    .address_text = cli_link_address_text,
    .machine = cli_link_machine,
    .shell_line = cli_link_shell_line,
};

typedef struct
{
    void (*runs)(const CliLinkAddressCfg *args);
} CliLinkAddressNs;
EMBED_TABLE_LAYOUT(CliLinkAddressNs, runs);
EMBED_TABLE_STORAGE CliLinkAddressNs cli_link_address_table EMBED_UNUSED = {
    .runs = cli_link_runs,
};

typedef struct
{
    int (*remote_run)(const CliLinkSizeCfg *args);
} CliLinkSizeNs;
EMBED_TABLE_LAYOUT(CliLinkSizeNs, remote_run);
EMBED_TABLE_STORAGE CliLinkSizeNs cli_link_size EMBED_UNUSED = {
    .remote_run = cli_link_remote_run,
};

typedef struct
{
    int (*prepare)(const CliLinkErrMsgCfg *args);
} CliLinkErrMsgNs;
EMBED_TABLE_LAYOUT(CliLinkErrMsgNs, prepare);
EMBED_TABLE_STORAGE CliLinkErrMsgNs cli_link_err_msg EMBED_UNUSED = {
    .prepare = cli_link_prepare,
};

typedef struct
{
    int (*is_windows)(const CliLinkErrMsgSystemCfg *args);
    int (*target)(const CliLinkErrMsgSystemCfg *args);
} CliLinkErrMsgSystemNs;
EMBED_TABLE_LAYOUT(CliLinkErrMsgSystemNs, is_windows, target);
EMBED_TABLE_STORAGE CliLinkErrMsgSystemNs cli_link_err_msg_system EMBED_UNUSED = {
    .is_windows = cli_link_is_windows,
    .target = cli_link_target,
};

typedef struct
{
    int (*open)(const CliLinkErrMsgAddressCfg *args);
} CliLinkErrMsgAddressNs;
EMBED_TABLE_LAYOUT(CliLinkErrMsgAddressNs, open);
EMBED_TABLE_STORAGE CliLinkErrMsgAddressNs cli_link_err_msg_address EMBED_UNUSED = {
    .open = cli_link_open,
};

typedef struct
{
    int (*call)(const CliLinkSizeOutCfg *args);
} CliLinkSizeOutNs;
EMBED_TABLE_LAYOUT(CliLinkSizeOutNs, call);
EMBED_TABLE_STORAGE CliLinkSizeOutNs cli_link_size_out EMBED_UNUSED = {
    .call = cli_link_call,
};

#endif
