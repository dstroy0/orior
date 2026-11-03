// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.h: a machine a deployment reaches.
//
// An address names the machine and a folder on it: `user@host:folder` over ssh, `docker:container:folder` in a running
// container over docker exec, and `wsl:distribution:folder` in a WSL distribution.
//
// ssh runs with BatchMode, its keys and agent its only way in: a password has no terminal to be typed at. ORIOR_SSH
// names another ssh, with words of its own before ssh's (link_address.c)
#ifndef LINK_H
#define LINK_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliLinkSizeCfg CliLinkSizeCfg;
typedef struct CliLinkAddressCfg CliLinkAddressCfg;
typedef struct CliLinkTextCfg CliLinkTextCfg;
typedef struct CliLinkTextSizeCfg CliLinkTextSizeCfg;
typedef struct CliLinkOutCfg CliLinkOutCfg;

// how the machine is reached
#define CLI_LINK_SSH 0u
#define CLI_LINK_DOCKER 1u
#define CLI_LINK_WSL 2u

// A machine: how it is reached, its name for that way, `user@host`, the container or the distribution, and the folder
// there
typedef struct
{
    unsigned int way;
    char name[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
} CliLinkAddress;

// `size` bytes of `text` written as one word of a POSIX shell's line
void cli_link_quote(const CliLinkTextSizeCfg *args);
// an address read from `text`: 1, or 0 with why given; the address written as that reads it; and the machine's name
int cli_link_address_parse(const CliLinkTextCfg *args);
void cli_link_address_text(const CliLinkOutCfg *args);
void cli_link_machine(const CliLinkOutCfg *args);
// the program and its words that run the shell line `line` on the machine, with a terminal where `terminal` is 1 and
// over its input and output where it is 0, into `words`
void cli_link_runs(const CliLinkAddressCfg *args);
// `line` run on the machine, `size` bytes of `input` written to it where `input` is not NULL, and what it wrote
// written into `out`, invalid UTF-8 replaced: 1, or 0 with what it reported where it failed (link_address.c)
int cli_link_remote_run(const CliLinkSizeCfg *args);

struct CliLinkSizeCfg
{
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const CliLinkAddress *address;
    const char *line;
    const char *input;
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
    int (*address_parse)(const CliLinkTextCfg *args);
} CliLinkTextNs;
EMBED_TABLE_LAYOUT(CliLinkTextNs, address_parse);
EMBED_TABLE_STORAGE CliLinkTextNs cli_link_text EMBED_UNUSED = {
    .address_parse = cli_link_address_parse,
};

typedef struct
{
    void (*address_text)(const CliLinkOutCfg *args);
    void (*machine)(const CliLinkOutCfg *args);
} CliLinkOutNs;
EMBED_TABLE_LAYOUT(CliLinkOutNs, address_text, machine);
EMBED_TABLE_STORAGE CliLinkOutNs cli_link_out EMBED_UNUSED = {
    .address_text = cli_link_address_text,
    .machine = cli_link_machine,
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

#endif
