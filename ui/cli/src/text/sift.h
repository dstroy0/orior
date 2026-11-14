// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.h: where a needle of bytes stands in a text (sift.c): every offset the needle stands at, each handed to the
// caller in order, the alignments overlapping. Each start comes from the library's byte find for the needle's first
// byte and is proven by its bounded compare, so a zero byte in the text or the needle is a byte like any other. A
// needle searched without case is searched in ASCII case: each letter of the text and of the needle to its lower case
#ifndef SIFT_H
#define SIFT_H
#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"

typedef struct CliSiftLengthCfg CliSiftLengthCfg;

// every offset `needle`, `needle_length` bytes, stands at in the `length` bytes of `text`, each handed to `found` with
// `context` in order, without case where `fold` is set: their count, 0 for an empty needle or one longer than the text
unsigned long long cli_sift_find_all(const CliSiftLengthCfg *args);
struct CliSiftLengthCfg
{
    unsigned long long length;
    int fold;
    const unsigned char *needle;
    unsigned long long needle_length;
    const unsigned char *text;
    void (*found)(const CliEachAtCfg *args);
    void *context;
};

typedef struct
{
    unsigned long long (*find_all)(const CliSiftLengthCfg *args);
} CliSiftLengthNs;
EMBED_TABLE_LAYOUT(CliSiftLengthNs, find_all);
EMBED_TABLE_STORAGE CliSiftLengthNs cli_sift_length EMBED_UNUSED = {
    .find_all = cli_sift_find_all,
};

#endif
