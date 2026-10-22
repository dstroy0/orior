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

// the values of libssl's calls orior_cli hands it: load the library's error strings and ciphers, a host's name as the
// SSL_ctrl that sets the name it is reached by, the option that reads an end with no close as an end, and what
// SSL_get_error says
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

struct CliTls
{
    int used;
    CliSocket *socket;
    void *context;
    void *ssl;
};

static CliTls s_sessions[CLI_TLS_MOST];
static CliLock s_lock = CLI_LOCK_INIT;
static CliSsl s_ssl;
// 0 where the library was not looked for, 1 where it was found whole, -1 where it was not, and what was missing
static int s_loaded;
static char s_missing[CLI_NAME_LONGEST];

// a call found in the library, a function's address as the symbol gives it
static void *cli_found(void *handle, const char *name, int needed)
{
    void *const found = dlsym(handle, name);
    if ((found == NULL) && needed && (s_missing[0] == '\0'))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_missing,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    return found;
}

// the library found and its calls read: 1, or 0 where it is not installed or lacks one. Called under the lock
static int cli_loaded(void)
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
    *(void **)&s_ssl.init = cli_found(handle, "OPENSSL_init_ssl", 1);
    *(void **)&s_ssl.client_method = cli_found(handle, "TLS_client_method", 1);
    *(void **)&s_ssl.context_new = cli_found(handle, "SSL_CTX_new", 1);
    *(void **)&s_ssl.context_free = cli_found(handle, "SSL_CTX_free", 1);
    *(void **)&s_ssl.default_paths = cli_found(handle, "SSL_CTX_set_default_verify_paths", 1);
    *(void **)&s_ssl.set_verify = cli_found(handle, "SSL_CTX_set_verify", 1);
    *(void **)&s_ssl.set_options = cli_found(handle, "SSL_CTX_set_options", 0);
    *(void **)&s_ssl.new_session = cli_found(handle, "SSL_new", 1);
    *(void **)&s_ssl.free_session = cli_found(handle, "SSL_free", 1);
    *(void **)&s_ssl.set_fd = cli_found(handle, "SSL_set_fd", 1);
    *(void **)&s_ssl.control = cli_found(handle, "SSL_ctrl", 1);
    *(void **)&s_ssl.set_host = cli_found(handle, "SSL_set1_host", 1);
    *(void **)&s_ssl.connect = cli_found(handle, "SSL_connect", 1);
    *(void **)&s_ssl.error = cli_found(handle, "SSL_get_error", 1);
    *(void **)&s_ssl.read = cli_found(handle, "SSL_read", 1);
    *(void **)&s_ssl.write = cli_found(handle, "SSL_write", 1);
    *(void **)&s_ssl.shutdown = cli_found(handle, "SSL_shutdown", 1);
    *(void **)&s_ssl.verify_result = cli_found(handle, "SSL_get_verify_result", 1);
    *(void **)&s_ssl.verify_text = cli_found(handle, "X509_verify_cert_error_string", 0);
    s_loaded = (s_missing[0] == '\0') ? 1 : -1;
    return s_loaded > 0;
}

// whether `host` is an address and not a name
static int cli_address(const char *host)
{
    unsigned char bytes[16];
    return (inet_pton(AF_INET, host, bytes) == 1) || (inet_pton(AF_INET6, host, bytes) == 1);
}

CliTls *cli_tls_connect(CliSocket *socket, const char *host, int verify, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int loaded = cli_loaded();
    CliTls *tls = NULL;
    for (unsigned int at = 0u; loaded && (tls == NULL) && (at < CLI_TLS_MOST); at += 1u)
    {
        tls = s_sessions[at].used ? NULL : &s_sessions[at];
    }
    if (tls != NULL)
    {
        tls->used = 1;
    }
    cli_lock_give(&s_lock);
    if (!loaded)
    {
        cli_line_text(said, (s_missing[0] != '\0') ? "the system's libssl has no "
                                                   : "no libssl is installed, and TLS on this system goes through it");
        cli_line_text(said, s_missing);
        return NULL;
    }
    if (tls == NULL)
    {
        cli_line_text(said, "TLS: more sessions are open than orior_cli holds");
        return NULL;
    }
    tls->socket = socket;
    s_ssl.init(0ull, NULL);
    tls->context = s_ssl.context_new(s_ssl.client_method());
    if (tls->context == NULL)
    {
        tls->used = 0;
        cli_line_text(said, "TLS: libssl made no context");
        return NULL;
    }
    if (s_ssl.set_options != NULL)
    {
        s_ssl.set_options(tls->context, CLI_SSL_IGNORE_UNEXPECTED_EOF);
    }
    if (verify)
    {
        s_ssl.default_paths(tls->context);
    }
    s_ssl.set_verify(tls->context, verify ? 1 : 0, NULL);
    tls->ssl = s_ssl.new_session(tls->context);
    if (tls->ssl == NULL)
    {
        s_ssl.context_free(tls->context);
        tls->used = 0;
        cli_line_text(said, "TLS: libssl made no session");
        return NULL;
    }
    // the socket's descriptor, which an int holds
    s_ssl.set_fd(tls->ssl, (int)(socket->handle - 1ull));
    if (!cli_address(host))
    {
        // the name handed as SSL_ctrl takes it, in room of its own
        char name[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = host,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL}) + 1u});
        s_ssl.control(tls->ssl, CLI_SSL_SET_HOST_NAME, 0l, name);
    }
    if (verify)
    {
        s_ssl.set_host(tls->ssl, host);
    }
    if (s_ssl.connect(tls->ssl) != 1)
    {
        const long code = s_ssl.verify_result(tls->ssl);
        cli_line_text(said, "TLS: ");
        if (code == 0l)
        {
            cli_line_text(said, "the handshake failed");
        }
        else if (s_ssl.verify_text != NULL)
        {
            cli_line_text(said, s_ssl.verify_text(code));
        }
        else
        {
            cli_line_text(said, "the certificate did not check (");
            cli_line_signed(said, code);
            cli_line_character(said, ')');
        }
        cli_tls_closed(tls);
        return NULL;
    }
    return tls;
}

// what a call that took nothing says: 1 to try again, else its answer into `result`
static int cli_retried(const CliTls *tls, int returned, long long *result, CliLine *said)
{
    const int code = s_ssl.error(tls->ssl, returned);
    if ((code == CLI_SSL_WANT_READ) || (code == CLI_SSL_WANT_WRITE))
    {
        return 1;
    }
    if ((code == CLI_SSL_ZERO_RETURN) || ((code == CLI_SSL_SYSCALL) && (returned == 0)))
    {
        *result = 0ll;
        return 0;
    }
    if ((code == CLI_SSL_SYSCALL) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
        *result = CLI_SOCKET_WAITED;
        return 0;
    }
    cli_line_text(said, "TLS: libssl said ");
    cli_line_signed(said, code);
    *result = CLI_SOCKET_FAILED;
    return 0;
}

long long cli_tls_read(CliTls *tls, void *out, unsigned long long room, CliLine *said)
{
    // a piece is at most what an int counts
    const int piece = (int)((room < 0x7fffffffull) ? room : 0x7fffffffull);
    for (;;)
    {
        const int read = s_ssl.read(tls->ssl, out, piece);
        long long result = 0ll;
        if (read > 0)
        {
            return read;
        }
        if (!cli_retried(tls, read, &result, said))
        {
            return result;
        }
    }
}

long long cli_tls_write(CliTls *tls, const void *data, unsigned long long size, CliLine *said)
{
    const int piece = (int)((size < 0x7fffffffull) ? size : 0x7fffffffull);
    for (;;)
    {
        const int written = s_ssl.write(tls->ssl, data, piece);
        long long result = 0ll;
        if (written > 0)
        {
            return written;
        }
        if (!cli_retried(tls, written, &result, said))
        {
            return (result == 0ll) ? CLI_SOCKET_FAILED : result;
        }
    }
}

void cli_tls_closed(CliTls *tls)
{
    s_ssl.shutdown(tls->ssl);
    s_ssl.free_session(tls->ssl);
    s_ssl.context_free(tls->context);
    cli_lock_take(&s_lock);
    tls->used = 0;
    cli_lock_give(&s_lock);
}
#endif
