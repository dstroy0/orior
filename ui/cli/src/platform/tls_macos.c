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

// what came read for Secure Transport, all it requested where it came: closed where the peer closed, would-block where
// the wait ran out
static OSStatus cli_read_from(SSLConnectionRef connection, void *data, size_t *length)
{
    const CliTls *const tls = (const CliTls *)connection;
    const size_t wanted = *length;
    size_t got = 0u;
    OSStatus status = noErr;
    while (got < wanted)
    {
        const long long read = EMBED_CALL(cli_room_internal.socket_some, CliRoomCtx, .sock = tls->socket,
                                          .bytes_out = (unsigned char *)data + got, .room = wanted - got);
        if (read > 0ll)
        {
            // a count read is below what was requested
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
    if (EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = tls->socket, .bytes = data,
                   .size = *length))
    {
        return noErr;
    }
    *length = 0u;
    return errSSLClosedAbort;
}

CliTls *cli_tls_connect(const CliTlsCfg *args)
{
    CliSocket *socket = args->socket;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    CliTls *tls = NULL;
    for (unsigned int at = 0u; (tls == NULL) && (at < CLI_TLS_MOST); at += 1u)
    {
        tls = s_sessions[at].used ? NULL : &s_sessions[at];
    }
    if (tls != NULL)
    {
        tls->used = 1;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (tls == NULL)
    {
        mmgr_line_put(args->err_msg, "TLS: more sessions are open than orior_cli supports");
        return NULL;
    }
    tls->socket = socket;
    tls->context = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    if (tls->context == NULL)
    {
        tls->used = 0;
        mmgr_line_put(args->err_msg, "TLS: Secure Transport made no session");
        return NULL;
    }
    SSLSetIOFuncs(tls->context, cli_read_from, cli_write_to);
    SSLSetConnection(tls->context, tls);
    SSLSetPeerDomainName(tls->context, args->host,
                         cellul.len(&(CatenaFinitaCfg){.src = args->host, .cap = CLI_CELLS_POOL}));
    if (!args->verify)
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
        if ((status == errSSLServerAuthCompleted) && !args->verify)
        {
            continue;
        }
        if (status == errSSLWouldBlock)
        {
            mmgr_line_put(args->err_msg, "TLS: the server took too long in the handshake");
        }
        else
        {
            mmgr_line_put(args->err_msg, "TLS: Secure Transport reported ");
            mmgr_line_decimal_signed(args->err_msg, status);
        }
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
}

long long cli_tls_read(const CliTlsOutCfg *args)
{
    size_t processed = 0u;
    const OSStatus status = SSLRead(args->tls->context, args->out, (size_t)args->room, &processed);
    if ((status == noErr) || (status == errSSLClosedGraceful) || (status == errSSLClosedAbort) ||
        ((status == errSSLWouldBlock) && (processed > 0u)))
    {
        // a count read is below the room requested
        return (long long)processed;
    }
    if (status == errSSLWouldBlock)
    {
        return CLI_SOCKET_WAITED;
    }
    mmgr_line_put(args->err_msg, "TLS: Secure Transport reported ");
    mmgr_line_decimal_signed(args->err_msg, status);
    return CLI_SOCKET_FAILED;
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    size_t processed = 0u;
    const OSStatus status = SSLWrite(args->tls->context, args->data, (size_t)args->size, &processed);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    args->tls->used = 0;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
#endif
