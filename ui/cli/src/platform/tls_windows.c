// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls_windows.c: TLS through Schannel: the handshake's tokens carried over the socket, and each record sealed and
// opened by the system
#include "../comms/tls.h"

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
// the bytes of what came and is not yet opened a session buffers through its handshake, whose tokens have no bound the
// protocol sets; once the handshake gives the session's largest record, what came, what is opened and what is sealed
// to go are each stored in a cell of one record
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
    // a status is read as its bit pattern
    const unsigned long code = (unsigned long)args->status;
    mmgr_line_put(args->err_msg, "TLS: ");
    mmgr_line_put(args->err_msg, (code == 0x80090325ul)
                                     ? "the server's certificate is not signed by a root this system trusts"
                                 : (code == 0x80090322ul) ? "the server's certificate names another host"
                                 : (code == 0x80090328ul) ? "the server's certificate has expired"
                                 : (code == 0x80090331ul) ? "the server and this system share no TLS version or cipher"
                                 : (code == 0x80090326ul) ? "the server sent a message TLS does not allow"
                                 : (code == 0x80090327ul) ? "the server's certificate could not be read"
                                                          : "Schannel reported 0x");
    if ((code != 0x80090325ul) && (code != 0x80090322ul) && (code != 0x80090328ul) && (code != 0x80090331ul) &&
        (code != 0x80090326ul) && (code != 0x80090327ul))
    {
        mmgr_line_hex(args->err_msg, code, 8u);
    }
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    const unsigned long header = args->tls->sizes.cbHeader;
    const unsigned long trailer = args->tls->sizes.cbTrailer;
    const unsigned long most = args->tls->sizes.cbMaximumMessage;
    const unsigned long part = (args->size < most) ? (unsigned long)args->size : most;
    if ((header + part + trailer) > args->tls->message_capacity)
    {
        mmgr_line_put(args->err_msg, "TLS: a record is larger than orior_cli seals");
        return CLI_SOCKET_FAILED;
    }
    unsigned char *const message = args->tls->message;
    memor.cpy(&(MemoriaCfg){.dst = message + header, .src = args->data, .bytes = part});
    SecBuffer buffers[4] = {{header, SECBUFFER_STREAM_HEADER, message},
                            {part, SECBUFFER_DATA, message + header},
                            {trailer, SECBUFFER_STREAM_TRAILER, message + header + part},
                            {0ul, SECBUFFER_EMPTY, NULL}};
    SecBufferDesc list = {SECBUFFER_VERSION, 4ul, buffers};
    const SECURITY_STATUS status = EncryptMessage(&args->tls->context, 0ul, &list, 0ul);
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
    g_cli_cells->persistent_buf_release(args->tls->sealed, &g_cli_tls_cells);
    g_cli_cells->persistent_buf_release(args->tls->plain, &g_cli_tls_cells);
    g_cli_cells->persistent_buf_release(args->tls->message, &g_cli_tls_cells);
    args->tls->sealed = NULL;
    args->tls->plain = NULL;
    args->tls->message = NULL;
    args->tls->used = 0;
}
#endif
