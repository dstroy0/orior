// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls.h: a TLS client over a connected socket, through the system's own TLS: Schannel on Windows
// (platform/tls_windows.c), Secure Transport on macOS (platform/tls_macos.c), and on Linux and the other Unix systems
// the system's libssl, found as it runs (platform/tls_libssl.c). Nothing of TLS is linked from outside the system. The
// peer's certificate is checked by the system against the roots it trusts and against the host's name, unless the
// caller requests no check. Schannel is given the credentials of TLS 1.2 and before, which every server still supports;
// libssl supports TLS 1.3 as well
#ifndef TLS_H
#define TLS_H
typedef struct CliTlsCfg CliTlsCfg;
typedef struct CliTlsOutCfg CliTlsOutCfg;
typedef struct CliTlsDataCfg CliTlsDataCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the most sessions open at once
#define CLI_TLS_MAX 32u

// A TLS session over a socket: what is written to it goes out sealed, and each read opens what came
typedef struct CliTls CliTls;

// the cells the sessions occupy while open, counted (tls.c)
extern CellOwner g_cli_tls_cells;

// `size` bytes of `data` written sealed: the bytes taken, which may be fewer, or CLI_SOCKET_FAILED with why given
long long cli_tls_write(const CliTlsDataCfg *args);
// `size` bytes written sealed whole: 1, or 0 with why given
int cli_tls_on_write(const CliTlsDataCfg *args);
// the session closed and its capacity given back; the socket stays the caller's
void cli_tls_close(const CliTlsOutCfg *args);

struct CliTlsCfg
{
    CliSocket *socket;
    const char *host;
    int verify;
    VerbaLinea *err_msg;
};

struct CliTlsOutCfg
{
    VerbaLinea *err_msg;
    CliTls *tls;
    void *out;
    unsigned long long capacity;
};

struct CliTlsDataCfg
{
    VerbaLinea *err_msg;
    CliTls *tls;
    const void *data;
    unsigned long long size;
};

typedef struct
{
    void (*close)(const CliTlsOutCfg *args);
} CliTlsOutNs;
EMBED_TABLE_LAYOUT(CliTlsOutNs, close);
EMBED_TABLE_STORAGE CliTlsOutNs cli_tls_out EMBED_UNUSED = {
    .close = cli_tls_close,
};

typedef struct
{
    long long (*write)(const CliTlsDataCfg *args);
    int (*written)(const CliTlsDataCfg *args);
} CliTlsDataNs;
EMBED_TABLE_LAYOUT(CliTlsDataNs, write, written);
EMBED_TABLE_STORAGE CliTlsDataNs cli_tls_data EMBED_UNUSED = {
    .write = cli_tls_write,
    .written = cli_tls_on_write,
};

#endif
