// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raised.h: writing a file the system refuses to write for the reader, once the system has prompted the reader for the
// rights the write needs: the text put in a file of orior's own first and copied over the file with the rights. A
// reader who declines the prompt leaves the file as it was (raised.c)
#ifndef RAISED_H
#define RAISED_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliRaisedErrMsgCfg CliRaisedErrMsgCfg;
typedef struct CliRaisedFromCfg CliRaisedFromCfg;
typedef struct CliRaisedCfg CliRaisedCfg;

// whether `err_text`, what a write that failed reported, is the system refusing it the rights; the file `from` copied
// over `to` with the rights; and `size` bytes of `text` written to the file at `path` with the rights. Each copy and
// write 1, or 0 with why given
int cli_raised_is_reject(const CliRaisedCfg *args);
int cli_raised_copy_over(const CliRaisedFromCfg *args);
int cli_raised_write(const CliRaisedErrMsgCfg *args);

struct CliRaisedErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *path;
    const void *text;
    unsigned long long size;
};

struct CliRaisedFromCfg
{
    const char *from;
    const char *to;
    VerbaLinea *err_msg;
};

struct CliRaisedCfg
{
    const char *err_text;
};

typedef struct
{
    int (*is_reject)(const CliRaisedCfg *args);
} CliRaisedNs;
EMBED_TABLE_LAYOUT(CliRaisedNs, is_reject);
EMBED_TABLE_STORAGE CliRaisedNs cli_raised EMBED_UNUSED = {
    .is_reject = cli_raised_is_reject,
};

typedef struct
{
    int (*copy_over)(const CliRaisedFromCfg *args);
} CliRaisedFromNs;
EMBED_TABLE_LAYOUT(CliRaisedFromNs, copy_over);
EMBED_TABLE_STORAGE CliRaisedFromNs cli_raised_from EMBED_UNUSED = {
    .copy_over = cli_raised_copy_over,
};

typedef struct
{
    int (*write)(const CliRaisedErrMsgCfg *args);
} CliRaisedErrMsgNs;
EMBED_TABLE_LAYOUT(CliRaisedErrMsgNs, write);
EMBED_TABLE_STORAGE CliRaisedErrMsgNs cli_raised_err_msg EMBED_UNUSED = {
    .write = cli_raised_write,
};

#endif
