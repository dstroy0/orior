// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls_libssl.c: TLS through the system's libssl on Linux and the other Unix systems, its calls found in the library as
// it runs, so nothing of it is linked
#if !defined(_WIN32) && !defined(__APPLE__)
// dlopen and inet_pton are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../comms/tls.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>

#if (!defined(_WIN32) && !defined(__APPLE__))
struct CliTls
{
    int used;
    CliSocket *socket;
    void *context;
    void *ssl;
};
#endif
// the values of libssl's calls impero hands it: load the library's error strings and ciphers, a host's name as the
// SSL_ctrl that sets the name it is reached by, the option that reads an end with no close as an end, and what
// SSL_get_error returns
#define CLI_SSL_NOW 2

#define CLI_SSL_SET_HOST_NAME 55

#define CLI_SSL_IGNORE_UNEXPECTED_EOF (1ULL << 7u)

#define CLI_SSL_WANT_READ 2

#define CLI_SSL_WANT_WRITE 3

#define CLI_SSL_SYSCALL 5

#define CLI_SSL_ZERO_RETURN 6

// The calls of libssl impero makes, found in the system's libssl as it runs; `set_options` and `verify_text` may be
// none
typedef struct
{
    int (*init)(unsigned long long options, const void *settings);
    const void *(*client_method)(void);
    void *(*context_new)(const void *method);
    void (*context_free)(void *context);
    int (*default_paths)(void *context);
    void (*set_verify)(void *context, int mode, const void *callback);
    unsigned long long (*set_options)(void *context, unsigned long long options);
    void *(*new_session)(void *context);
    void (*free_session)(void *ssl);
    int (*set_fd)(void *ssl, int fd);
    long (*control)(void *ssl, int command, long larg, void *parg);
    int (*set_host)(void *ssl, const char *host);
    int (*connect)(void *ssl);
    int (*error)(const void *ssl, int returned);
    int (*read)(void *ssl, void *buffer, int length);
    int (*write)(void *ssl, const void *buffer, int length);
    int (*shutdown)(void *ssl);
    long (*verify_result)(const void *ssl);
    const char *(*verify_text)(long code);
} CliSsl;

static CliSsl s_ssl;

typedef struct
{
#if (!defined(_WIN32) && !defined(__APPLE__))
    void *handle;
    const char *name;
    int needed;
    const char *host;
    const CliTls *tls;
    int returned;
    long long *result;
    VerbaLinea *err_msg;
#endif
} TlsLibsslCtx;
// what a call that took nothing returns: 1 to try again, else its answer into `result`
EMBED_INLINE int cli_retry(const TlsLibsslCtx *args)
{
    long long *result = args->result;
    const int code = s_ssl.error(args->tls->ssl, args->returned);
    if ((code == CLI_SSL_WANT_READ) || (code == CLI_SSL_WANT_WRITE))
    {
        return 1;
    }
    if ((code == CLI_SSL_ZERO_RETURN) || ((code == CLI_SSL_SYSCALL) && (args->returned == 0)))
    {
        *result = 0LL;
        return 0;
    }
    if ((code == CLI_SSL_SYSCALL) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
        *result = CLI_SOCKET_WAITED;
        return 0;
    }
    mmgr_line_put(args->err_msg, "TLS: libssl reported ");
    mmgr_line_decimal_signed(args->err_msg, code);
    *result = CLI_SOCKET_FAILED;
    return 0;
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    const int piece = (int)((args->size < 0x7fffffffULL) ? args->size : 0x7fffffffULL);
    for (;;)
    {
        const int written = s_ssl.write(args->tls->ssl, args->data, piece);
        long long result = 0LL;
        if (written > 0)
        {
            return written;
        }
        if (!EMBED_CALL(cli_retry, TlsLibsslCtx, .tls = args->tls, .returned = written, .result = &result,
                        .err_msg = args->err_msg))
        {
            return (result == 0LL) ? CLI_SOCKET_FAILED : result;
        }
    }
}

void cli_tls_close(const CliTlsOutCfg *args)
{
    s_ssl.shutdown(args->tls->ssl);
    s_ssl.free_session(args->tls->ssl);
    s_ssl.context_free(args->tls->context);
    args->tls->used = 0;
}
#endif
