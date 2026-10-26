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

#include "../command_line/cli_internal.h"

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
void cli_link_quote(const char *text, unsigned long long size, VerbaLinea *out);
void cli_link_path_word(const char *path, VerbaLinea *out);
// an address read from `text`: 1, or 0 with why given; the address written as that reads it; and the machine, as the
// window names it
int cli_link_address_parse(const char *text, CliLinkAddress *address, VerbaLinea *err_msg);
void cli_link_address_text(const CliLinkAddress *address, VerbaLinea *out);
void cli_link_machine(const CliLinkAddress *address, VerbaLinea *out);
// the program and its words that run the shell line `line` on the machine, with a terminal where `terminal` is 1 and
// over its input and output where it is 0, into `words`; and the shell line that opens the reader's shell in
// `folder`, or the tree's folder where it is NULL, on the machine
void cli_link_runs(const CliLinkAddress *address, const char *line, int terminal, CliWords *words);
void cli_link_shell_line(const CliLinkAddress *address, const char *folder, VerbaLinea *out);
// `line` run on the machine, `size` bytes of `input` written to it where `input` is not NULL, and what it wrote
// written into `out` as Rust's from_utf8_lossy reads it: 1, or 0 with what it reported where it failed (link_address.c)
int cli_link_remote_run(const CliLinkAddress *address, const char *line, const char *input, unsigned long long size,
                        VerbaLinea *out, VerbaLinea *err_msg);

// what is being done while the link is made, reported as its state `state` with `detail` beside it
typedef void (*CliLinkStated)(void *context, const char *state, const char *detail);

// orior's server put on the machine where it is not there, what is being done reported to `stated` as the state
// `preparing` while it waits, and the shell line that starts it on the tree written into `start`: 1, or 0 with why
// given. A server built here for the machine is built by the tree's ui/cli/build.sh, given the machine's target,
// into build/cli/<target>/orior_cli (link_prepare.c)
int cli_link_prepare(const CliLinkAddress *address, CliLinkStated stated, void *context, VerbaLinea *start,
                     VerbaLinea *err_msg);
// whether `uname -sm` reports the machine is Windows; and the machine's target by what it reports, into `target`, which
// is sized for CLI_NAME_MAX_LEN: 1, or 0 with why given (link_prepare.c)
int cli_link_is_windows(const char *system);
int cli_link_target(const char *system, char *target, VerbaLinea *err_msg);

// the link to the tree at `address` opened, made on a thread of its own and made again as it drops, the server's
// events and the link's own handed to `emit`: 1, or 0 with why given where a link is open already; how it stands, as it
// was last reported, written as `{state, machine, address, message}`; `name` called on the tree's side with `size`
// bytes of JSON `args`, waiting up to 60 s for the link to be made, its answer, JSON, written into `out`: 1, or 0 with
// why given; the link ended, and the server on the machine with it; and the server ended, as a dropped connection would
// end it, for the link to make again (link.c)
int cli_link_open(const CliLinkAddress *address, CliLinkEventFn emit, void *context, VerbaLinea *err_msg);
void cli_link_standing(VerbaLinea *out);
int cli_link_call(const char *name, const char *args, unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg);
void cli_link_end(void);
void cli_link_drop_now(void);
// the address of the open link into `address`: 1, or 0 where none is open
int cli_link_address(CliLinkAddress *address);

#endif
