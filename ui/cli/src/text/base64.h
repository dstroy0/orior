// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// base64.h: bytes as base64 and back, the standard letters with `=` to pad, as a file's bytes go in JSON over a link
// (base64.c)
#ifndef BASE64_H
#define BASE64_H

#include "../command_line/cli_internal.h"

// `size` bytes of `bytes` written as base64; and the bytes base64 `text`, `length` bytes, gives, ASCII's whitespace in
// it passed over, into `out`, which holds `room`, their count into `size`: 1, or 0 where it is not base64 or they do
// not fit
void cli_base64(CliLine *out, const unsigned char *bytes, unsigned long long size);
int cli_unbase64(const char *text, unsigned long long length, unsigned char *out, unsigned long long room,
                 unsigned long long *size);

#endif
