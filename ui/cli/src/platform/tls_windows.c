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

// the bytes of what came and is not yet opened, and of what is opened and not yet read, each session holds
#define CLI_SEALED_BYTES (1u << 17u)

#define CLI_PLAIN_BYTES (1u << 17u)

struct CliTls
{
    int used;
    CliSocket *socket;
    CredHandle credential;
    CtxtHandle context;
    SecPkgContext_StreamSizes sizes;
    unsigned char sealed[CLI_SEALED_BYTES];
    unsigned long sealed_size;
    unsigned char plain[CLI_PLAIN_BYTES];
    unsigned long long plain_size;
    int ended;
};

static CliTls s_sessions[CLI_TLS_MOST];
static CliLock s_lock = CLI_LOCK_INIT;

// What a status Schannel gives says, in words where it is one a reader meets
static void cli_said(CliLine *said, SECURITY_STATUS status)
{
    // a status is read as its bit pattern
    const unsigned long code = (unsigned long)status;
    cli_line_text(said, "TLS: ");
    cli_line_text(said, (code == 0x80090325ul)   ? "the server's certificate is not signed by a root this system trusts"
                        : (code == 0x80090322ul) ? "the server's certificate names another host"
                        : (code == 0x80090328ul) ? "the server's certificate has expired"
                        : (code == 0x80090331ul) ? "the server and this system share no TLS version or cipher"
                        : (code == 0x80090326ul) ? "the server sent a message TLS does not allow"
                        : (code == 0x80090327ul) ? "the server's certificate could not be read"
                                                 : "Schannel said 0x");
    if ((code != 0x80090325ul) && (code != 0x80090322ul) && (code != 0x80090328ul) && (code != 0x80090331ul) &&
        (code != 0x80090326ul) && (code != 0x80090327ul))
    {
        cli_line_hex(said, code, 8u);
    }
}

// more of what came read onto the sealed bytes: the count, 0 where the peer closed or there is no room
static long long cli_fill(CliTls *tls)
{
    const long long read =
        cli_socket_some(tls->socket, tls->sealed + tls->sealed_size, CLI_SEALED_BYTES - tls->sealed_size);
    // a count read is below the room, which an unsigned long counts
    tls->sealed_size += (read > 0ll) ? (unsigned long)read : 0ul;
    return read;
}

// the last `keep` sealed bytes moved to the front, the rest let go
static void cli_kept(CliTls *tls, unsigned long keep)
{
    const unsigned long from = tls->sealed_size - keep;
    for (unsigned long at = 0ul; at < keep; at += 1ul)
    {
        tls->sealed[at] = tls->sealed[from + at];
    }
    tls->sealed_size = keep;
}

static int cli_handshake(CliTls *tls, const char *host, CliLine *said)
{
    wchar_t target[CLI_PATH_LONGEST];
    if (MultiByteToWideChar(CP_UTF8, 0, host, -1, target, CLI_PATH_LONGEST) == 0)
    {
        cli_line_text(said, "TLS: the host's name is too long");
        return 0;
    }
    const unsigned long wanted = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                                 ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_EXTENDED_ERROR;
    int first = 1;
    for (;;)
    {
        SecBuffer input[2] = {{tls->sealed_size, SECBUFFER_TOKEN, tls->sealed}, {0ul, SECBUFFER_EMPTY, NULL}};
        SecBuffer output[1] = {{0ul, SECBUFFER_TOKEN, NULL}};
        SecBufferDesc input_list = {SECBUFFER_VERSION, 2ul, input};
        SecBufferDesc output_list = {SECBUFFER_VERSION, 1ul, output};
        unsigned long given = 0ul;
        const SECURITY_STATUS status =
            InitializeSecurityContextW(&tls->credential, first ? NULL : &tls->context, target, wanted, 0ul, 0ul,
                                       first ? NULL : &input_list, 0ul, &tls->context, &output_list, &given, NULL);
        first = 0;
        if (output[0].pvBuffer != NULL)
        {
            const int sent =
                (output[0].cbBuffer == 0ul) || cli_socket_written(tls->socket, output[0].pvBuffer, output[0].cbBuffer);
            FreeContextBuffer(output[0].pvBuffer);
            if (!sent)
            {
                cli_line_text(said, "TLS: the server could not be written to in the handshake");
                return 0;
            }
        }
        const unsigned long extra = (input[1].BufferType == SECBUFFER_EXTRA) ? input[1].cbBuffer : 0ul;
        if (status == SEC_E_OK)
        {
            cli_kept(tls, extra);
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
                cli_kept(tls, extra);
            }
            if (((status == SEC_E_INCOMPLETE_MESSAGE) || (tls->sealed_size == 0ul)) && (cli_fill(tls) <= 0ll))
            {
                cli_line_text(said, "TLS: the server closed the connection in the handshake");
                return 0;
            }
            continue;
        }
        cli_said(said, status);
        return 0;
    }
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
    memor.set(&(MemoriaCfg){.dst = &tls->credential, .val = 0, .bytes = sizeof(tls->credential)});
    memor.set(&(MemoriaCfg){.dst = &tls->context, .val = 0, .bytes = sizeof(tls->context)});
    tls->sealed_size = 0ul;
    tls->plain_size = 0ull;
    tls->ended = 0;
    SCHANNEL_CRED credentials;
    memor.set(&(MemoriaCfg){.dst = &credentials, .val = 0, .bytes = sizeof(credentials)});
    credentials.dwVersion = SCHANNEL_CRED_VERSION;
    credentials.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO |
                          (verify ? SCH_CRED_AUTO_CRED_VALIDATION : SCH_CRED_MANUAL_CRED_VALIDATION);
    SECURITY_STATUS status = AcquireCredentialsHandleW(NULL, (LPWSTR)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL,
                                                       &credentials, NULL, NULL, &tls->credential, NULL);
    if (status != SEC_E_OK)
    {
        cli_said(said, status);
        tls->used = 0;
        return NULL;
    }
    if (!cli_handshake(tls, host, said))
    {
        cli_tls_closed(tls);
        return NULL;
    }
    status = QueryContextAttributesW(&tls->context, SECPKG_ATTR_STREAM_SIZES, &tls->sizes);
    if (status != SEC_E_OK)
    {
        cli_said(said, status);
        cli_tls_closed(tls);
        return NULL;
    }
    return tls;
}

long long cli_tls_read(CliTls *tls, void *out, unsigned long long room, CliLine *said)
{
    while ((tls->plain_size == 0ull) && !tls->ended)
    {
        if (tls->sealed_size == 0ul)
        {
            const long long read = cli_fill(tls);
            if (read == CLI_SOCKET_WAITED)
            {
                return CLI_SOCKET_WAITED;
            }
            if (read <= 0ll)
            {
                tls->ended = 1;
                break;
            }
        }
        SecBuffer buffers[4] = {{tls->sealed_size, SECBUFFER_DATA, tls->sealed},
                                {0ul, SECBUFFER_EMPTY, NULL},
                                {0ul, SECBUFFER_EMPTY, NULL},
                                {0ul, SECBUFFER_EMPTY, NULL}};
        SecBufferDesc list = {SECBUFFER_VERSION, 4ul, buffers};
        const SECURITY_STATUS status = DecryptMessage(&tls->context, &list, 0ul, NULL);
        if ((status == SEC_E_OK) || (status == SEC_I_RENEGOTIATE))
        {
            unsigned long extra = 0ul;
            for (unsigned int at = 0u; at < 4u; at += 1u)
            {
                if ((buffers[at].BufferType == SECBUFFER_DATA) && (buffers[at].cbBuffer > 0ul) &&
                    ((tls->plain_size + buffers[at].cbBuffer) <= CLI_PLAIN_BYTES))
                {
                    memor.cpy(&(MemoriaCfg){.dst = tls->plain + tls->plain_size,
                                            .src = buffers[at].pvBuffer,
                                            .bytes = buffers[at].cbBuffer});
                    tls->plain_size += buffers[at].cbBuffer;
                }
                extra = ((buffers[at].BufferType == SECBUFFER_EXTRA) && (extra == 0ul)) ? buffers[at].cbBuffer : extra;
            }
            cli_kept(tls, extra);
            if ((status == SEC_I_RENEGOTIATE) && (tls->plain_size == 0ull))
            {
                cli_line_text(said, "TLS: the server asked to renegotiate");
                return CLI_SOCKET_FAILED;
            }
        }
        else if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            const long long read = cli_fill(tls);
            if (read == CLI_SOCKET_WAITED)
            {
                return CLI_SOCKET_WAITED;
            }
            tls->ended = read <= 0ll;
        }
        else if (status == SEC_I_CONTEXT_EXPIRED)
        {
            tls->ended = 1;
        }
        else
        {
            cli_said(said, status);
            return CLI_SOCKET_FAILED;
        }
    }
    const unsigned long long taken = (room < tls->plain_size) ? room : tls->plain_size;
    memor.cpy(&(MemoriaCfg){.dst = out, .src = tls->plain, .bytes = taken});
    for (unsigned long long at = taken; at < tls->plain_size; at += 1ull)
    {
        tls->plain[at - taken] = tls->plain[at];
    }
    tls->plain_size -= taken;
    // a count opened is below the room a session holds
    return (long long)taken;
}

long long cli_tls_write(CliTls *tls, const void *data, unsigned long long size, CliLine *said)
{
    static unsigned char s_message[1u << 17u];
    static CliLock s_writing = CLI_LOCK_INIT;
    const unsigned long header = tls->sizes.cbHeader;
    const unsigned long trailer = tls->sizes.cbTrailer;
    const unsigned long most = tls->sizes.cbMaximumMessage;
    const unsigned long part = (size < most) ? (unsigned long)size : most;
    if ((header + part + trailer) > sizeof(s_message))
    {
        cli_line_text(said, "TLS: a record is larger than orior seals");
        return CLI_SOCKET_FAILED;
    }
    cli_lock_take(&s_writing);
    memor.cpy(&(MemoriaCfg){.dst = s_message + header, .src = data, .bytes = part});
    SecBuffer buffers[4] = {{header, SECBUFFER_STREAM_HEADER, s_message},
                            {part, SECBUFFER_DATA, s_message + header},
                            {trailer, SECBUFFER_STREAM_TRAILER, s_message + header + part},
                            {0ul, SECBUFFER_EMPTY, NULL}};
    SecBufferDesc list = {SECBUFFER_VERSION, 4ul, buffers};
    const SECURITY_STATUS status = EncryptMessage(&tls->context, 0ul, &list, 0ul);
    const int sent = (status == SEC_E_OK) && cli_socket_written(tls->socket, s_message,
                                                                (unsigned long long)buffers[0].cbBuffer +
                                                                    buffers[1].cbBuffer + buffers[2].cbBuffer);
    cli_lock_give(&s_writing);
    if (status != SEC_E_OK)
    {
        cli_said(said, status);
        return CLI_SOCKET_FAILED;
    }
    if (!sent)
    {
        cli_line_text(said, "TLS: the server could not be written to");
        return CLI_SOCKET_FAILED;
    }
    return (long long)part;
}

void cli_tls_closed(CliTls *tls)
{
    DeleteSecurityContext(&tls->context);
    FreeCredentialsHandle(&tls->credential);
    cli_lock_take(&s_lock);
    tls->used = 0;
    cli_lock_give(&s_lock);
}
#endif
