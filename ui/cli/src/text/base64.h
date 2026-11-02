// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base64.h: bytes as base64 and back, the standard letters with `=` to pad, as a file's bytes go in JSON over a link
// (base64.c)
#ifndef BASE64_H
#define BASE64_H
typedef struct CliBase64TextCfg CliBase64TextCfg;
typedef struct CliBase64Cfg CliBase64Cfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// `size` bytes of `bytes` written as base64; and the bytes base64 `text`, `length` bytes, gives, ASCII's whitespace in
// it passed over, into `out`, sized for `capacity`, their count into `size`: 1, or 0 where it is not base64 or they do
// not fit
void cli_base64(const CliBase64Cfg *args);
int cli_unbase64(const CliBase64TextCfg *args);

struct CliBase64TextCfg
{
    const char *text;
    unsigned long long length;
    unsigned char *out_bytes;
    unsigned long long capacity;
    unsigned long long *size_pointer;
};

struct CliBase64Cfg
{
    VerbaLinea *out;
    const unsigned char *bytes;
    unsigned long long size;
};

typedef struct
{
    void (*base64)(const CliBase64Cfg *args);
} CliBase64Ns;
EMBED_TABLE_LAYOUT(CliBase64Ns, base64);
EMBED_TABLE_STORAGE CliBase64Ns cli_base64_table EMBED_UNUSED = {
    .base64 = cli_base64,
};

typedef struct
{
    int (*unbase64)(const CliBase64TextCfg *args);
} CliBase64TextNs;
EMBED_TABLE_LAYOUT(CliBase64TextNs, unbase64);
EMBED_TABLE_STORAGE CliBase64TextNs cli_base64_text EMBED_UNUSED = {
    .unbase64 = cli_unbase64,
};

#endif
