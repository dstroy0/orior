// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls_libssl.c: TLS through the system's libssl on Linux and the other Unix systems, its calls found in the library as
// it runs, so nothing of it is linked
#if !defined(_WIN32) && !defined(__APPLE__)
// dlopen and inet_pton are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
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
// the values of libssl's calls orior_cli hands it: load the library's error strings and ciphers, a host's name as the
// SSL_ctrl that sets the name it is reached by, the option that reads an end with no close as an end, and what
// SSL_get_error returns
#define CLI_SSL_NOW 2

#define CLI_SSL_SET_HOST_NAME 55

#define CLI_SSL_IGNORE_UNEXPECTED_EOF (1ull << 7u)

#define CLI_SSL_WANT_READ 2

#define CLI_SSL_WANT_WRITE 3

#define CLI_SSL_SYSCALL 5

#define CLI_SSL_ZERO_RETURN 6

// The calls of libssl orior_cli makes, found in the system's libssl as it runs; `set_options` and `verify_text` may be
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

static CliTls s_sessions[CLI_TLS_MOST];
static CliLock s_lock = CLI_LOCK_INIT;
static CliSsl s_ssl;
// 0 where the library was not looked for, 1 where it was found whole, -1 where it was not, and what was missing
static int s_loaded;
static char s_missing[CLI_NAME_MAX_LEN];

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
// a call found in the library, a function's address as the symbol gives it
EMBED_INLINE void *cli_on_find(const TlsLibsslCtx *args)
{
    void *const found = dlsym(args->handle, args->name);
    if ((found == NULL) && args->needed && (s_missing[0] == '\0'))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_missing,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return found;
}

// the library found and its calls read: 1, or 0 where it is not installed or lacks one. Called under the lock
static int cli_load(void)
{
    if (s_loaded != 0)
    {
        return s_loaded > 0;
    }
    static const char *const s_names[3] = {"libssl.so.3", "libssl.so.1.1", "libssl.so"};
    void *handle = NULL;
    for (unsigned int at = 0u; (handle == NULL) && (at < 3u); at += 1u)
    {
        handle = dlopen(s_names[at], CLI_SSL_NOW);
    }
    if (handle == NULL)
    {
        s_loaded = -1;
        return 0;
    }
    // a symbol's address is the function's: POSIX gives dlsym's object pointers as functions' where they are ones
    *(void **)&s_ssl.init =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "OPENSSL_init_ssl", .needed = 1);
    *(void **)&s_ssl.client_method =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "TLS_client_method", .needed = 1);
    *(void **)&s_ssl.context_new =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_CTX_new", .needed = 1);
    *(void **)&s_ssl.context_free =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_CTX_free", .needed = 1);
    *(void **)&s_ssl.default_paths = EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle,
                                                .name = "SSL_CTX_set_default_verify_paths", .needed = 1);
    *(void **)&s_ssl.set_verify =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_CTX_set_verify", .needed = 1);
    *(void **)&s_ssl.set_options =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_CTX_set_options", .needed = 0);
    *(void **)&s_ssl.new_session =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_new", .needed = 1);
    *(void **)&s_ssl.free_session =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_free", .needed = 1);
    *(void **)&s_ssl.set_fd =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_set_fd", .needed = 1);
    *(void **)&s_ssl.control = EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_ctrl", .needed = 1);
    *(void **)&s_ssl.set_host =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_set1_host", .needed = 1);
    *(void **)&s_ssl.connect =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_connect", .needed = 1);
    *(void **)&s_ssl.error =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_get_error", .needed = 1);
    *(void **)&s_ssl.read = EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_read", .needed = 1);
    *(void **)&s_ssl.write = EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_write", .needed = 1);
    *(void **)&s_ssl.shutdown =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_shutdown", .needed = 1);
    *(void **)&s_ssl.verify_result =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "SSL_get_verify_result", .needed = 1);
    *(void **)&s_ssl.verify_text =
        EMBED_CALL(cli_on_find, TlsLibsslCtx, .handle = handle, .name = "X509_verify_cert_error_string", .needed = 0);
    s_loaded = (s_missing[0] == '\0') ? 1 : -1;
    return s_loaded > 0;
}

// whether `host` is an address and not a name
EMBED_INLINE int cli_address(const TlsLibsslCtx *args)
{
    unsigned char bytes[16];
    return (inet_pton(AF_INET, args->host, bytes) == 1) || (inet_pton(AF_INET6, args->host, bytes) == 1);
}

CliTls *cli_tls_connect(const CliTlsCfg *args)
{
    CliSocket *socket = args->socket;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const int loaded = cli_load();
    CliTls *tls = NULL;
    for (unsigned int at = 0u; loaded && (tls == NULL) && (at < CLI_TLS_MOST); at += 1u)
    {
        tls = s_sessions[at].used ? NULL : &s_sessions[at];
    }
    if (tls != NULL)
    {
        tls->used = 1;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!loaded)
    {
        mmgr_line_put(args->err_msg, (s_missing[0] != '\0')
                                         ? "the system's libssl has no "
                                         : "no libssl is installed, and TLS on this system goes through it");
        mmgr_line_put(args->err_msg, s_missing);
        return NULL;
    }
    if (tls == NULL)
    {
        mmgr_line_put(args->err_msg, "TLS: more sessions are open than orior_cli supports");
        return NULL;
    }
    tls->socket = socket;
    s_ssl.init(0ull, NULL);
    tls->context = s_ssl.context_new(s_ssl.client_method());
    if (tls->context == NULL)
    {
        tls->used = 0;
        mmgr_line_put(args->err_msg, "TLS: libssl made no context");
        return NULL;
    }
    if (s_ssl.set_options != NULL)
    {
        s_ssl.set_options(tls->context, CLI_SSL_IGNORE_UNEXPECTED_EOF);
    }
    if (args->verify)
    {
        s_ssl.default_paths(tls->context);
    }
    s_ssl.set_verify(tls->context, args->verify ? 1 : 0, NULL);
    tls->ssl = s_ssl.new_session(tls->context);
    if (tls->ssl == NULL)
    {
        s_ssl.context_free(tls->context);
        tls->used = 0;
        mmgr_line_put(args->err_msg, "TLS: libssl made no session");
        return NULL;
    }
    // the socket's descriptor, which an int stores
    s_ssl.set_fd(tls->ssl, (int)(socket->handle - 1ull));
    if (!EMBED_CALL(cli_address, TlsLibsslCtx, .host = args->host))
    {
        // the name handed as SSL_ctrl takes it, in room of its own
        char name[CLI_PATH_MAX_LEN];
        memor.cpy(
            &(MemoriaCfg){.dst = name,
                          .src = args->host,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->host, .cap = CLI_CELLS_POOL}) + 1u});
        s_ssl.control(tls->ssl, CLI_SSL_SET_HOST_NAME, 0l, name);
    }
    if (args->verify)
    {
        s_ssl.set_host(tls->ssl, args->host);
    }
    if (s_ssl.connect(tls->ssl) != 1)
    {
        const long code = s_ssl.verify_result(tls->ssl);
        mmgr_line_put(args->err_msg, "TLS: ");
        if (code == 0l)
        {
            mmgr_line_put(args->err_msg, "the handshake failed");
        }
        else if (s_ssl.verify_text != NULL)
        {
            mmgr_line_put(args->err_msg, s_ssl.verify_text(code));
        }
        else
        {
            mmgr_line_put(args->err_msg, "the certificate did not check (");
            mmgr_line_decimal_signed(args->err_msg, code);
            mmgr_line_ch(args->err_msg, ')');
        }
        EMBED_CALL(cli_tls_out.close, CliTlsOutCfg, .tls = tls);
        return NULL;
    }
    return tls;
}

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
        *result = 0ll;
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

long long cli_tls_read(const CliTlsOutCfg *args)
{
    // a piece is at most what an int counts
    const int piece = (int)((args->room < 0x7fffffffull) ? args->room : 0x7fffffffull);
    for (;;)
    {
        const int read = s_ssl.read(args->tls->ssl, args->out, piece);
        long long result = 0ll;
        if (read > 0)
        {
            return read;
        }
        if (!EMBED_CALL(cli_retry, TlsLibsslCtx, .tls = args->tls, .returned = read, .result = &result,
                        .err_msg = args->err_msg))
        {
            return result;
        }
    }
}

long long cli_tls_write(const CliTlsDataCfg *args)
{
    const int piece = (int)((args->size < 0x7fffffffull) ? args->size : 0x7fffffffull);
    for (;;)
    {
        const int written = s_ssl.write(args->tls->ssl, args->data, piece);
        long long result = 0ll;
        if (written > 0)
        {
            return written;
        }
        if (!EMBED_CALL(cli_retry, TlsLibsslCtx, .tls = args->tls, .returned = written, .result = &result,
                        .err_msg = args->err_msg))
        {
            return (result == 0ll) ? CLI_SOCKET_FAILED : result;
        }
    }
}

void cli_tls_close(const CliTlsOutCfg *args)
{
    s_ssl.shutdown(args->tls->ssl);
    s_ssl.free_session(args->tls->ssl);
    s_ssl.context_free(args->tls->context);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    args->tls->used = 0;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
#endif
