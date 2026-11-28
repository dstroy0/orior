// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls_windows.c: TLS through Schannel: the handshake's tokens carried over the socket, and each record sealed and
// opened by the system
#include "../../../ui/cli/src/comms/tls.h"

#if defined(_WIN32)
#define NOMINMAX
#define SECURITY_WIN32
#include <winsock2.h>

#include <windows.h>

#include <schannel.h>
#include <security.h>

#if (defined(_WIN32))
struct CliTls
{
    int used;
    CliSocket *socket;
    CredHandle credential;
    CtxtHandle context;
    SecPkgContext_StreamSizes sizes;
    unsigned char *sealed;
    unsigned long sealed_size;
    unsigned long sealed_capacity;
    unsigned char *plain;
    unsigned long long plain_size;
    unsigned long long plain_capacity;
    unsigned char *message;
    unsigned long long message_capacity;
    int ended;
};
#endif
// the bytes that a session buffers through its handshake, of what came and the session did not open yet. The
// protocol sets no bound on the tokens of the handshake. After the handshake gives the largest record of the session,
// one record stores three cells. They hold what came, what the session opened, and what it sealed to send
#define CLI_SEALED_BYTES (1u << 17u)

static CliTls s_sessions[CLI_TLS_MAX];

typedef struct
{
#if (defined(_WIN32))
    VerbaLinea *err_msg;
    SECURITY_STATUS status;
    CliTls *tls;
    unsigned long extra_len;
    const char *host;
#endif
} TlsWindowsCtx;
// What a status Schannel gives means, in words where it is one a reader meets
EMBED_INLINE void cli_status_err_write(const TlsWindowsCtx *args)
{
    // the call reads a status as its bit pattern
    const unsigned long code = (unsigned long)args->status;
    mmgr_line_put(args->err_msg, "TLS: ");
    const char *reason = "Schannel reported 0x";
    switch (code)
    {
    case 0x80090325UL:
        reason = "the server's certificate is not signed by a root this system trusts";
        break;
    case 0x80090322UL:
        reason = "the server's certificate names another host";
        break;
    case 0x80090328UL:
        reason = "the server's certificate has expired";
        break;
    case 0x80090331UL:
        reason = "the server and this system share no TLS version or cipher";
        break;
    case 0x80090326UL:
        reason = "the server sent a message TLS does not allow";
        break;
    case 0x80090327UL:
        reason = "the server's certificate could not be read";
        break;
    default:
        break;
    }
    mmgr_line_put(args->err_msg, reason);
    if ((code != 0x80090325UL) && (code != 0x80090322UL) && (code != 0x80090328UL) && (code != 0x80090331UL) &&
        (code != 0x80090326UL) && (code != 0x80090327UL))
    {
        mmgr_line_hex(args->err_msg, code, 8u);
    }
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    const unsigned long header = args->tls->sizes.cbHeader;
    const unsigned long trailer = args->tls->sizes.cbTrailer;
    const unsigned long max = args->tls->sizes.cbMaximumMessage;
    const unsigned long part = (args->size < max) ? (unsigned long)args->size : max;
    if ((header + part + trailer) > args->tls->message_capacity)
    {
        mmgr_line_put(args->err_msg, "TLS: a record is larger than impero seals");
        return CLI_SOCKET_FAILED;
    }
    unsigned char *const message = args->tls->message;
    memor.cpy(&(MemoriaCfg){.dst = message + header, .src = args->data, .bytes = part});
    SecBuffer buffers[4] = {{header, SECBUFFER_STREAM_HEADER, message},
                            {part, SECBUFFER_DATA, message + header},
                            {trailer, SECBUFFER_STREAM_TRAILER, message + header + part},
                            {0UL, SECBUFFER_EMPTY, NULL}};
    SecBufferDesc list = {SECBUFFER_VERSION, 4UL, buffers};
    const SECURITY_STATUS status = EncryptMessage(&args->tls->context, 0UL, &list, 0UL);
    const int sent =
        (status == SEC_E_OK) &&
        EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->tls->socket, .bytes = message,
                   .size = (unsigned long long)buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
    if (status != SEC_E_OK)
    {
        EMBED_CALL(cli_status_err_write, TlsWindowsCtx, .err_msg = args->err_msg, .status = status);
        return CLI_SOCKET_FAILED;
    }
    if (!sent)
    {
        mmgr_line_put(args->err_msg, "TLS: the server could not be written to");
        return CLI_SOCKET_FAILED;
    }
    return (long long)part;
}

void cli_tls_close(const CliTlsOutCfg *args)
{
    DeleteSecurityContext(&args->tls->context);
    FreeCredentialsHandle(&args->tls->credential);
    // what came and went in the clear, which the pool would leave as it is
    if (args->tls->plain != NULL)
    {
        mmgr_zero_buf(args->tls->plain, args->tls->plain_capacity);
    }
    if (args->tls->message != NULL)
    {
        mmgr_zero_buf(args->tls->message, args->tls->message_capacity);
    }
    g_platform_guard->persistent_buf_release(args->tls->sealed, g_cli_tls_owner);
    g_platform_guard->persistent_buf_release(args->tls->plain, g_cli_tls_owner);
    g_platform_guard->persistent_buf_release(args->tls->message, g_cli_tls_owner);
    args->tls->sealed = NULL;
    args->tls->plain = NULL;
    args->tls->message = NULL;
    args->tls->used = 0;
}
#endif
