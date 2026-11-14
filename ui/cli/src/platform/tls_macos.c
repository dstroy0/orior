// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls_macos.c: TLS through Secure Transport, which reads and writes the socket through the two calls handed it
#include "../comms/tls.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecureTransport.h>

struct CliTls
{
    int used;
    CliSocket *socket;
    SSLContextRef context;
};

static CliTls s_sessions[CLI_TLS_MAX];

long long cli_tls_write(const CliTlsDataCfg *args)
{
    embed_word processed = 0u;
    const OSStatus status = SSLWrite(args->tls->context, args->data, (embed_word)args->size, &processed);
    if (status == noErr)
    {
        return (long long)processed;
    }
    mmgr_line_put(args->err_msg, "TLS: Secure Transport reported ");
    mmgr_line_decimal_signed(args->err_msg, status);
    return CLI_SOCKET_FAILED;
}

void cli_tls_close(const CliTlsOutCfg *args)
{
    SSLClose(args->tls->context);
    CFRelease(args->tls->context);
    args->tls->used = 0;
}
#endif
