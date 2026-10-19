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

static CliTls s_sessions[CLI_TLS_MOST];
static CliLock s_lock = CLI_LOCK_INIT;

// what came read for Secure Transport, all it asked for where it came: closed where the peer closed, would-block where
// the wait ran out
static OSStatus cli_read_from(SSLConnectionRef connection, void *data, size_t *length)
{
    const CliTls *const tls = (const CliTls *)connection;
    const size_t wanted = *length;
    size_t got = 0u;
    OSStatus status = noErr;
    while (got < wanted)
    {
        const long long read = cli_socket_some(tls->socket, (unsigned char *)data + got, wanted - got);
        if (read > 0ll)
        {
            // a count read is below what was asked
            got += (size_t)read;
            continue;
        }
        status = (read == CLI_SOCKET_CLOSED)   ? errSSLClosedGraceful
                 : (read == CLI_SOCKET_WAITED) ? errSSLWouldBlock
                                               : errSSLClosedAbort;
        break;
    }
    *length = got;
    return status;
}

static OSStatus cli_write_to(SSLConnectionRef connection, const void *data, size_t *length)
{
    const CliTls *const tls = (const CliTls *)connection;
    if (cli_socket_written(tls->socket, data, *length))
    {
        return noErr;
    }
    *length = 0u;
    return errSSLClosedAbort;
}

CliTls *cli_tls_connect(CliSocket *socket, const char *host, int verify, CliLine *said)
{
    cli_lock_take(&s_lock);
    CliTls *tls = NULL;
    for (unsigned int at = 0u; (tls == NULL) && (at < CLI_TLS_MOST); at += 1u)
    {
        tls = s_sessions[at].used ? NULL : &s_sessions[at];
    }
    if (tls != NULL)
    {
        tls->used = 1;
    }
    cli_lock_give(&s_lock);
    if (tls == NULL)
    {
        cli_line_text(said, "TLS: more sessions are open than orior holds");
        return NULL;
    }
    tls->socket = socket;
    tls->context = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    if (tls->context == NULL)
    {
        tls->used = 0;
        cli_line_text(said, "TLS: Secure Transport made no session");
        return NULL;
    }
    SSLSetIOFuncs(tls->context, cli_read_from, cli_write_to);
    SSLSetConnection(tls->context, tls);
    SSLSetPeerDomainName(tls->context, host, cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL}));
    if (!verify)
    {
        SSLSetSessionOption(tls->context, kSSLSessionOptionBreakOnServerAuth, true);
    }
    for (;;)
    {
        const OSStatus status = SSLHandshake(tls->context);
        if (status == noErr)
        {
            return tls;
        }
        if ((status == errSSLServerAuthCompleted) && !verify)
        {
            continue;
        }
        if (status == errSSLWouldBlock)
        {
            cli_line_text(said, "TLS: the server took too long in the handshake");
        }
        else
        {
            cli_line_text(said, "TLS: Secure Transport said ");
            cli_line_signed(said, status);
        }
        cli_tls_closed(tls);
        return NULL;
    }
}

long long cli_tls_read(CliTls *tls, void *out, unsigned long long room, CliLine *said)
{
    size_t processed = 0u;
    const OSStatus status = SSLRead(tls->context, out, (size_t)room, &processed);
    if ((status == noErr) || (status == errSSLClosedGraceful) || (status == errSSLClosedAbort) ||
        ((status == errSSLWouldBlock) && (processed > 0u)))
    {
        // a count read is below the room asked
        return (long long)processed;
    }
    if (status == errSSLWouldBlock)
    {
        return CLI_SOCKET_WAITED;
    }
    cli_line_text(said, "TLS: Secure Transport said ");
    cli_line_signed(said, status);
    return CLI_SOCKET_FAILED;
}

long long cli_tls_write(CliTls *tls, const void *data, unsigned long long size, CliLine *said)
{
    size_t processed = 0u;
    const OSStatus status = SSLWrite(tls->context, data, (size_t)size, &processed);
    if (status == noErr)
    {
        return (long long)processed;
    }
    cli_line_text(said, "TLS: Secure Transport said ");
    cli_line_signed(said, status);
    return CLI_SOCKET_FAILED;
}

void cli_tls_closed(CliTls *tls)
{
    SSLClose(tls->context);
    CFRelease(tls->context);
    cli_lock_take(&s_lock);
    tls->used = 0;
    cli_lock_give(&s_lock);
}
#endif
