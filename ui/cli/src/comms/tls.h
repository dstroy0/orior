// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls.h: a TLS client over a connected socket, through the system's own TLS: Schannel on Windows
// (platform/tls_windows.c), Secure Transport on macOS (platform/tls_macos.c), and on Linux and the other Unix systems
// the system's libssl, found as it runs (platform/tls_libssl.c). Nothing of TLS is linked from outside the system. The
// peer's certificate is checked by the system against the roots it trusts and against the host's name, unless the
// caller asks for no check. Schannel is given the credentials of TLS 1.2 and before, which every server still speaks;
// libssl speaks TLS 1.3 as well
#ifndef TLS_H
#define TLS_H

#include "../command_line/cli_internal.h"

// the most sessions open at once
#define CLI_TLS_MOST 32u

// A TLS session over a socket: what is written to it goes out sealed, and each read opens what came
typedef struct CliTls CliTls;

// the cells the sessions hold while open, counted (tls.c)
extern CliCellsTally g_cli_tls_cells;

// TLS opened to `host` over `socket`, which the session uses until it is closed, the peer's certificate checked where
// `verify` is 1: the session, or NULL with why said
CliTls *cli_tls_connect(CliSocket *socket, const char *host, int verify, CliLine *said);
// what came opened, at most `room` bytes, into `out`: its count, 0 where the peer closed, CLI_SOCKET_WAITED where the
// wait ran out, CLI_SOCKET_FAILED with why said
long long cli_tls_read(CliTls *tls, void *out, unsigned long long room, CliLine *said);
// `size` bytes of `data` written sealed: the bytes taken, which may be fewer, or CLI_SOCKET_FAILED with why said
long long cli_tls_write(CliTls *tls, const void *data, unsigned long long size, CliLine *said);
// `size` bytes written sealed whole: 1, or 0 with why said
int cli_tls_written(CliTls *tls, const void *data, unsigned long long size, CliLine *said);
// the session closed and its room given back; the socket stays the caller's
void cli_tls_closed(CliTls *tls);

#endif
