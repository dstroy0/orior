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
    unsigned long sealed_room;
    unsigned char *plain;
    unsigned long long plain_size;
    unsigned long long plain_room;
    unsigned char *message;
    unsigned long long message_room;
    int ended;
};
#endif
// the bytes of what came and is not yet opened a session buffers through its handshake, whose tokens have no bound the
// protocol sets; once the handshake gives the session's largest record, what came, what is opened and what is sealed
// to go are each stored in a cell of one record
#define CLI_SEALED_BYTES (1u << 17u)

static CliTls s_sessions[CLI_TLS_MOST];

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

// more of what came read onto the sealed bytes: the count, 0 where the peer closed or there is no room
EMBED_INLINE long long cli_fill(const TlsWindowsCtx *args)
{
    const long long read = EMBED_CALL(cli_room_internal.socket_some, CliRoomCtx, .sock = args->tls->socket,
                                      .bytes_out = args->tls->sealed + args->tls->sealed_size,
                                      .room = args->tls->sealed_room - args->tls->sealed_size);
    // a count read is below the room, which an unsigned long counts
    args->tls->sealed_size += (read > 0ll) ? (unsigned long)read : 0ul;
    return read;
}

// the last `extra_len` sealed bytes moved to the front, the rest let go
EMBED_INLINE void cli_extra_save(const TlsWindowsCtx *args)
{
    const unsigned long from = args->tls->sealed_size - args->extra_len;
    for (unsigned long at = 0ul; at < args->extra_len; at += 1ul)
    {
        args->tls->sealed[at] = args->tls->sealed[from + at];
    }
    args->tls->sealed_size = args->extra_len;
}

EMBED_INLINE int cli_handshake(const TlsWindowsCtx *args)
{
    wchar_t target[CLI_PATH_MAX_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, args->host, -1, target, CLI_PATH_MAX_LEN) == 0)
    {
        mmgr_line_put(args->err_msg, "TLS: the host's name is too long");
        return 0;
    }
    const unsigned long wanted = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                                 ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_EXTENDED_ERROR;
    int first = 1;
    for (;;)
    {
        SecBuffer input[2] = {{args->tls->sealed_size, SECBUFFER_TOKEN, args->tls->sealed},
                              {0ul, SECBUFFER_EMPTY, NULL}};
        SecBuffer output[1] = {{0ul, SECBUFFER_TOKEN, NULL}};
        SecBufferDesc input_list = {SECBUFFER_VERSION, 2ul, input};
        SecBufferDesc output_list = {SECBUFFER_VERSION, 1ul, output};
        unsigned long given = 0ul;
        const SECURITY_STATUS status = InitializeSecurityContextW(
            &args->tls->credential, first ? NULL : &args->tls->context, target, wanted, 0ul, 0ul,
            first ? NULL : &input_list, 0ul, &args->tls->context, &output_list, &given, NULL);
        first = 0;
        if (output[0].pvBuffer != NULL)
        {
            const int sent =
                (output[0].cbBuffer == 0ul) ||
                EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = args->tls->socket,
                           .bytes = output[0].pvBuffer, .size = output[0].cbBuffer);
            FreeContextBuffer(output[0].pvBuffer);
            if (!sent)
            {
                mmgr_line_put(args->err_msg, "TLS: the server could not be written to in the handshake");
                return 0;
            }
        }
        const unsigned long extra = (input[1].BufferType == SECBUFFER_EXTRA) ? input[1].cbBuffer : 0ul;
        if (status == SEC_E_OK)
        {
            EMBED_CALL(cli_extra_save, TlsWindowsCtx, .tls = args->tls, .extra_len = extra);
            return 1;
        }
        if (status == SEC_I_INCOMPLETE_CREDENTIALS)
        {
            continue;
        }
        if ((status == SEC_I_CONTINUE_NEEDED) || (status == SEC_E_INCOMPLETE_MESSAGE))
        {
            if (status == SEC_I_CONTINUE_NEEDED)
            {
                EMBED_CALL(cli_extra_save, TlsWindowsCtx, .tls = args->tls, .extra_len = extra);
            }
            if (((status == SEC_E_INCOMPLETE_MESSAGE) || (args->tls->sealed_size == 0ul)) && (cli_fill(args) <= 0ll))
            {
                mmgr_line_put(args->err_msg, "TLS: the server closed the connection in the handshake");
                return 0;
            }
            continue;
        }
        EMBED_CALL(cli_status_err_write, TlsWindowsCtx, .err_msg = args->err_msg, .status = status);
        return 0;
    }
}

// the session's cells sized to its largest record once the handshake gives it, what came past the handshake retained:
// 1, or 0 where a cell is refused
EMBED_INLINE int cli_records_reserve(const TlsWindowsCtx *args)
{
    const unsigned long record =
        args->tls->sizes.cbHeader + args->tls->sizes.cbMaximumMessage + args->tls->sizes.cbTrailer;
    const unsigned long sealed_room = (record > args->tls->sealed_size) ? record : args->tls->sealed_size;
    unsigned char *const sealed = g_cli_cells->persistent_buf_alloc(sealed_room, &g_cli_tls_cells);
    args->tls->plain = g_cli_cells->persistent_buf_alloc(args->tls->sizes.cbMaximumMessage, &g_cli_tls_cells);
    args->tls->plain_room = (args->tls->plain != NULL) ? args->tls->sizes.cbMaximumMessage : 0ull;
    args->tls->message = g_cli_cells->persistent_buf_alloc(record, &g_cli_tls_cells);
    args->tls->message_room = (args->tls->message != NULL) ? record : 0ull;
    if (sealed == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = sealed, .src = args->tls->sealed, .bytes = args->tls->sealed_size});
    g_cli_cells->persistent_buf_release(args->tls->sealed, &g_cli_tls_cells);
    args->tls->sealed = sealed;
    args->tls->sealed_room = sealed_room;
    return (args->tls->plain != NULL) && (args->tls->message != NULL);
}

CliTls *cli_tls_connect(const CliTlsCfg *args)
{
    CliSocket *socket = args->socket;
    CliTls *tls = NULL;
    for (unsigned int at = 0u; (tls == NULL) && (at < CLI_TLS_MOST); at += 1u)
    {
        tls = s_sessions[at].used ? NULL : &s_sessions[at];
    }
    if (tls != NULL)
    {
        tls->used = 1;
    }
    if (tls == NULL)
    {
        mmgr_line_put(args->err_msg, "TLS: more sessions are open than orior_cli supports");
        return NULL;
    }
    tls->socket = socket;
    memor.set(&(MemoriaCfg){.dst = &tls->credential, .val = 0, .bytes = sizeof(tls->credential)});
    memor.set(&(MemoriaCfg){.dst = &tls->context, .val = 0, .bytes = sizeof(tls->context)});
    tls->sealed_size = 0ul;
    tls->plain_size = 0ull;
    tls->ended = 0;
    SCHANNEL_CRED credentials;
    memor.set(&(MemoriaCfg){.dst = &credentials, .val = 0, .bytes = sizeof(credentials)});
    credentials.dwVersion = SCHANNEL_CRED_VERSION;
    credentials.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO |
                          (args->verify ? SCH_CRED_AUTO_CRED_VALIDATION : SCH_CRED_MANUAL_CRED_VALIDATION);
    SECURITY_STATUS status = AcquireCredentialsHandleW(NULL, (LPWSTR)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL,
                                                       &credentials, NULL, NULL, &tls->credential, NULL);
    if (status != SEC_E_OK)
    {
        EMBED_CALL(cli_status_err_write, TlsWindowsCtx, .err_msg = args->err_msg, .status = status);
        tls->used = 0;
        return NULL;
    }
    tls->sealed = g_cli_cells->persistent_buf_alloc(CLI_SEALED_BYTES, &g_cli_tls_cells);
    if (tls->sealed == NULL)
    {
        tls->sealed_room = 0ul;
        mmgr_line_put(args->err_msg, "TLS: the handshake could not be stored");
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
    tls->sealed_room = CLI_SEALED_BYTES;
    if (!EMBED_CALL(cli_handshake, TlsWindowsCtx, .tls = tls, .host = args->host, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
    status = QueryContextAttributesW(&tls->context, SECPKG_ATTR_STREAM_SIZES, &tls->sizes);
    if (status != SEC_E_OK)
    {
        EMBED_CALL(cli_status_err_write, TlsWindowsCtx, .err_msg = args->err_msg, .status = status);
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
    if (!EMBED_CALL(cli_records_reserve, TlsWindowsCtx, .tls = tls))
    {
        mmgr_line_put(args->err_msg, "TLS: the session's records could not be stored");
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
    return tls;
}

long long cli_tls_read(const CliTlsOutCfg *args)
{
    while ((args->tls->plain_size == 0ull) && !args->tls->ended)
    {
        if (args->tls->sealed_size == 0ul)
        {
            const long long read = EMBED_CALL(cli_fill, TlsWindowsCtx, .tls = args->tls);
            if (read == CLI_SOCKET_WAITED)
            {
                return CLI_SOCKET_WAITED;
            }
            if (read <= 0ll)
            {
                args->tls->ended = 1;
                break;
            }
        }
        SecBuffer buffers[4] = {{args->tls->sealed_size, SECBUFFER_DATA, args->tls->sealed},
                                {0ul, SECBUFFER_EMPTY, NULL},
                                {0ul, SECBUFFER_EMPTY, NULL},
                                {0ul, SECBUFFER_EMPTY, NULL}};
        SecBufferDesc list = {SECBUFFER_VERSION, 4ul, buffers};
        const SECURITY_STATUS status = DecryptMessage(&args->tls->context, &list, 0ul, NULL);
        if ((status == SEC_E_OK) || (status == SEC_I_RENEGOTIATE))
        {
            unsigned long extra = 0ul;
            for (unsigned int at = 0u; at < 4u; at += 1u)
            {
                if ((buffers[at].BufferType == SECBUFFER_DATA) && (buffers[at].cbBuffer > 0ul) &&
                    ((args->tls->plain_size + buffers[at].cbBuffer) <= args->tls->plain_room))
                {
                    memor.cpy(&(MemoriaCfg){.dst = args->tls->plain + args->tls->plain_size,
                                            .src = buffers[at].pvBuffer,
                                            .bytes = buffers[at].cbBuffer});
                    args->tls->plain_size += buffers[at].cbBuffer;
                }
                extra = ((buffers[at].BufferType == SECBUFFER_EXTRA) && (extra == 0ul)) ? buffers[at].cbBuffer : extra;
            }
            EMBED_CALL(cli_extra_save, TlsWindowsCtx, .tls = args->tls, .extra_len = extra);
            if ((status == SEC_I_RENEGOTIATE) && (args->tls->plain_size == 0ull))
            {
                mmgr_line_put(args->err_msg, "TLS: the server requested to renegotiate");
                return CLI_SOCKET_FAILED;
            }
        }
        else if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            const long long read = EMBED_CALL(cli_fill, TlsWindowsCtx, .tls = args->tls);
            if (read == CLI_SOCKET_WAITED)
            {
                return CLI_SOCKET_WAITED;
            }
            args->tls->ended = read <= 0ll;
        }
        else if (status == SEC_I_CONTEXT_EXPIRED)
        {
            args->tls->ended = 1;
        }
        else
        {
            EMBED_CALL(cli_status_err_write, TlsWindowsCtx, .err_msg = args->err_msg, .status = status);
            return CLI_SOCKET_FAILED;
        }
    }
    const unsigned long long taken = (args->room < args->tls->plain_size) ? args->room : args->tls->plain_size;
    memor.cpy(&(MemoriaCfg){.dst = args->out, .src = args->tls->plain, .bytes = taken});
    for (unsigned long long at = taken; at < args->tls->plain_size; at += 1ull)
    {
        args->tls->plain[at - taken] = args->tls->plain[at];
    }
    args->tls->plain_size -= taken;
    // a count opened is below the room a session has
    return (long long)taken;
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    const unsigned long header = args->tls->sizes.cbHeader;
    const unsigned long trailer = args->tls->sizes.cbTrailer;
    const unsigned long most = args->tls->sizes.cbMaximumMessage;
    const unsigned long part = (args->size < most) ? (unsigned long)args->size : most;
    if ((header + part + trailer) > args->tls->message_room)
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
        mmgr_zero_buf(args->tls->plain, args->tls->plain_room);
    }
    if (args->tls->message != NULL)
    {
        mmgr_zero_buf(args->tls->message, args->tls->message_room);
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
