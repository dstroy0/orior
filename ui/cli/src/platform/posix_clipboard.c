// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_clipboard.c: the system's clipboard on Linux and macOS. On Linux it is the X server's CLIPBOARD selection,
// spoken over the display's socket in X11's own protocol, which Wayland desktops share through XWayland: its text read
// as UTF8_STRING, and text and files put by owning the selection, a thread of the clipboard's own answering each
// program that asks for what it holds, a content past one request's capacity sent in pieces as INCR sends it; files go
// as the file manager puts them, a list of file URIs, a cut said in GNOME's copied-files target. On macOS it is the
// system's pasteboard, its text through pbpaste and pbcopy and its files through osascript's bridge to AppKit
#if !defined(_WIN32)
// sockets, poll, gethostname and the descriptor flags are POSIX, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if (!defined(_WIN32)) && !((defined(__APPLE__)))
#define CLI_X11_NAMED 8u
#endif
#if (!defined(_WIN32)) && !((defined(__APPLE__)))
// One connection to the X server: its socket, the window it reads the clipboard into or owns it with, the most bytes
// one request takes, and the atoms it interned
typedef struct
{
    int socket;
    unsigned int window;
    unsigned long long request_max;
    unsigned int atom[CLI_X11_NAMED];
} CliX11;
#endif
// the file URI a path is written in on the clipboard, and the characters of a path a URI saves as they are
#define CLI_CLIPBOARD_FILE_SCHEME "file://"

typedef struct
{
#if (!defined(_WIN32))
    unsigned char letter;
    VerbaLinea *out;
    const char *path;
    char digit;
    const char *list;
    unsigned long long size;
    void (*each)(const CliEachPathCfg *args);
    void *context;
#endif
#if (!defined(_WIN32)) && !((defined(__APPLE__)))
    unsigned char *at;
    unsigned int value;
    const unsigned char *at_read;
    const CliX11 *x11;
    const void *bytes;
    void *bytes_out;
    int wait;
    unsigned char *storage;
    unsigned long long capacity_bytes;
    unsigned char *packet;
    unsigned char *extra;
    unsigned long long capacity;
    unsigned long long *size_pointer;
    char *host_out;
    embed_word capacity_bytes_other;
    unsigned int *number_pointer;
    const char *host;
    unsigned int number;
    const unsigned char *file;
    unsigned long long *at_u64s;
    unsigned int *length;
    const unsigned char *field;
    unsigned int length_value;
    const char *text;
    unsigned char *cookie;
    CliX11 *x11_out;
    unsigned int *root;
    unsigned int owner;
    unsigned int window;
    unsigned int property;
    unsigned int property_type;
    unsigned int format;
    unsigned int type;
    unsigned int *type_pointer;
    unsigned int target;
    const unsigned char *request;
    const unsigned char *event;
    unsigned int place;
    const char *bytes_text;
    int *cut;
#endif
#if (!defined(_WIN32)) && (defined(__APPLE__))
    const char *const *words;
    const char *input;
#endif
} PosixClipboardCtx;
// whether `letter` stands in a file URI as it is
EMBED_INLINE int clipboard_unreserved(const PosixClipboardCtx *args)
{
    return ((args->letter >= 'a') && (args->letter <= 'z')) || ((args->letter >= 'A') && (args->letter <= 'Z')) ||
           ((args->letter >= '0') && (args->letter <= '9')) || (args->letter == '-') || (args->letter == '.') ||
           (args->letter == '_') || (args->letter == '~') || (args->letter == '/');
}

// `path` written as a file URI, each byte a URI does not save written as `%XX`
EMBED_INLINE void clipboard_uri_write(const PosixClipboardCtx *args)
{
    static const char s_digits[] = "0123456789ABCDEF";
    mmgr_line_put(args->out, CLI_CLIPBOARD_FILE_SCHEME);
    for (const unsigned char *at = (const unsigned char *)args->path; *at != 0u; at += 1)
    {
        if (EMBED_CALL(clipboard_unreserved, PosixClipboardCtx, .letter = *at))
        {
            // a byte a URI saves is an ASCII letter
            mmgr_line_ch(args->out, (char)*at);
        }
        else
        {
            mmgr_line_ch(args->out, '%');
            mmgr_line_ch(args->out, s_digits[*at >> 4u]);
            mmgr_line_ch(args->out, s_digits[*at & 0x0fu]);
        }
    }
}

// each file of a list of URIs, `size` bytes of `list`, one to a line, its comments passed over, handed to `each` as a
// path, its host taken off and its escapes read: how many
static unsigned int clipboard_uris_read(const PosixClipboardCtx *args)
{
    unsigned long long size = args->size;
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; at < size;)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->list, .at = end,
                         .size = size, .set = "\n\r");
        const unsigned long long scheme = sizeof(CLI_CLIPBOARD_FILE_SCHEME) - 1u;
        int file = (end - at > scheme);
        {
            unsigned long long letter = 0ULL;
            while (file && (letter < scheme))
            {
                file = args->list[at + letter] == CLI_CLIPBOARD_FILE_SCHEME[letter];
                letter += 1ULL;
            }
        }
        if (file)
        {
            unsigned long long from = at + scheme;
            from = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->list, .at = from,
                              .size = end, .set = "/");
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea decoded = {path, sizeof(path), 0ULL};
            unsigned long long letter = from;
            while (letter < end)
            {
                const unsigned int high = (letter + 2ULL < end) ? EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg,
                                                                             .byte = args->list[letter + 1ULL])
                                                                : 16u;
                const unsigned int low = (letter + 2ULL < end) ? EMBED_CALL(cli_text_byte.hex_digit, CliTextByteCfg,
                                                                            .byte = args->list[letter + 2ULL])
                                                               : 16u;
                const int escaped = (args->list[letter] == '%') && (high < 16u) && (low < 16u);
                // two hexadecimal digits make a byte, below 256
                mmgr_line_ch(&decoded, escaped ? (char)((high << 4u) | low) : args->list[letter]);
                letter += escaped ? 2ULL : 0ULL;
                letter += 1ULL;
            }
            if (mmgr_line_finish(&decoded) != 0ULL)
            {
                EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = path);
                count += 1u;
            }
        }
        at = end + 1ULL;
    }
    return count;
}

#if defined(__APPLE__)
// the scripts osascript runs in JavaScript, its bridge to AppKit reaching the system's pasteboard: the files on it, one
// path a line; the files read one path a line from the input put on it; and the pasteboard emptied
static const char s_files_held_script[] =
    "ObjC.import('AppKit'); const urls = $.NSPasteboard.generalPasteboard.readObjectsForClassesOptions("
    "$([$.NSURL]), $({NSPasteboardURLReadingFileURLsOnlyKey: true})); const paths = []; "
    "for (let at = 0; urls && at < urls.count; at += 1) paths.push(urls.objectAtIndex(at).path.js); paths.join('\\n');";
static const char s_files_append_script[] =
    "ObjC.import('AppKit'); const input = $.NSFileHandle.fileHandleWithStandardInput.readDataToEndOfFile; "
    "const text = $.NSString.alloc.initWithDataEncoding(input, $.NSUTF8StringEncoding).js; "
    "const urls = text.split('\\n').filter((path) => path.length).map((path) => $.NSURL.fileURLWithPath(path)); "
    "const board = $.NSPasteboard.generalPasteboard; board.clearContents; board.writeObjects($(urls)); '';";
static const char s_emptied_script[] = "ObjC.import('AppKit'); $.NSPasteboard.generalPasteboard.clearContents; '';";

// the encoding pbcopy and pbpaste read and write the text in
static const CliSetting s_utf8[1] = {{"LANG", "en_US.UTF-8"}};

// `words` run, `size` bytes of `input` fed to it, what it wrote handed to `out` where `out` is not NULL: 1, or 0 where
// it did not run or ended with a code other than 0
EMBED_INLINE int clipboard_run(const PosixClipboardCtx *args)
{
    const char *const *words = args->words;
    const char *input = args->input;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int done =
        EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = NULL, .settings = s_utf8,
                   .count = 1u, .input = input, .length = args->size, .run_result = run_result, .err_msg = &err_msg) &&
        (run_result->code == 0LL);
    if (done && (args->out != NULL))
    {
        mmgr_line_put_n(args->out, run_result->out, run_result->out_size);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return done;
}

int cli_clipboard_text_read(const CliCtx *args)
{
    static const char *const s_words[2] = {"pbpaste", NULL};
    return EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL,
                      .out = args->text_verba_linea_pointer);
}

int cli_clipboard_text_write(const CliSizeTextCtx *args)
{
    static const char *const s_words[2] = {"pbcopy", NULL};
    return EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = args->text, .size = args->size,
                      .out = NULL);
}

int cli_clipboard_files_append(const CliCountCtx *args)
{
    // a cut is known to the window alone on macOS, whose pasteboard says no cut of files
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_append_script, NULL};
    const unsigned long long input_capacity = 4096u;
    char *const input_cell = g_cli_guard->temporary_buf_alloc(input_capacity, g_cli_platform_owner);
    VerbaLinea input = mmgr_line_over(input_cell, input_capacity);
    if (input.out == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&input, args->paths[at]);
        mmgr_line_ch(&input, '\n');
    }
    const int put = mmgr_line_ok(&input) && EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words,
                                                       .input = input.out, .size = input.at, .out = NULL);
    g_cli_guard->persistent_buf_release(input.out, g_cli_platform_owner);
    return put;
}

// a path osascript wrote, one to a line, handed on
typedef struct
{
    void (*each)(const CliEachPathCfg *args);
    void *context;
} CliClipboardEach;

unsigned int cli_clipboard_files_read(const CliEachCtx *args)
{
    int *cut = args->cut_pointer;
    *cut = -1;
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_held_script, NULL};
    const unsigned long long paths_capacity = 4096u;
    char *const paths_cell = g_cli_guard->temporary_buf_alloc(paths_capacity, g_cli_platform_owner);
    VerbaLinea paths = mmgr_line_over(paths_cell, paths_capacity);
    if (paths.out == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    if (EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL, .out = &paths) &&
        mmgr_line_ok(&paths))
    {
        for (unsigned long long at = 0ULL; at < paths.at;)
        {
            const unsigned long long from = at;
            const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                      .text = paths.out, .at = from, .size = paths.at, .set = "\n");
            at = end + 1ULL;
            if (end <= from)
            {
                continue;
            }
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ULL};
            mmgr_line_put_n(&line, paths.out + from, end - from);
            if (mmgr_line_finish(&line) != 0ULL)
            {
                EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = path);
                count += 1u;
            }
        }
    }
    g_cli_guard->persistent_buf_release(paths.out, g_cli_platform_owner);
    return count;
}

void cli_clipboard_clear(void)
{
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_emptied_script, NULL};
    (void)EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL, .out = NULL);
}
#else
// the core protocol's atoms this file names by number, the types of what the server sends, the events it reports and
// the mask that asks for property changes, a window's class that takes input alone, and the value-mask bit that sets a
// window's event mask
#define CLI_X11_ATOM_ATOM 4u
#define CLI_X11_ATOM_STRING 31u
#define CLI_X11_ERROR 0u
#define CLI_X11_REPLY 1u
#define CLI_X11_PROPERTY_NOTIFY 28u
#define CLI_X11_SELECTION_CLEAR 29u
#define CLI_X11_SELECTION_REQUEST 30u
#define CLI_X11_SELECTION_NOTIFY 31u
#define CLI_X11_PROPERTY_CHANGES 0x400000u
#define CLI_X11_INPUT_ONLY 2u
#define CLI_X11_EVENT_MASK 0x800u

// the requests this file sends, by their opcodes
#define CLI_X11_CREATE_WINDOW 1u
#define CLI_X11_CHANGE_WINDOW 2u
#define CLI_X11_INTERN_ATOM 16u
#define CLI_X11_CHANGE_PROPERTY 18u
#define CLI_X11_DELETE_PROPERTY 19u
#define CLI_X11_GET_PROPERTY 20u
#define CLI_X11_SET_OWNER 22u
#define CLI_X11_CONVERT 24u
#define CLI_X11_SEND_EVENT 25u

// the milliseconds an answer the clipboard's owner sends is waited for; the bytes of a property read at once; the most
// bytes a content is sent in at once, below one request's capacity; and the most bytes of the reader's Xauthority file
// read
#define CLI_X11_WAIT 2000
#define CLI_X11_PIECE (64u << 10u)
#define CLI_X11_SENT_MAX (256u << 10u)
#define CLI_X11_AUTHORITY_MAX (64u << 10u)

// the atoms this file interns, by place
#define CLI_X11_CLIPBOARD 0u
#define CLI_X11_UTF8 1u
#define CLI_X11_TARGETS 2u
#define CLI_X11_INCR 3u
#define CLI_X11_URIS 4u
#define CLI_X11_COPIED 5u
#define CLI_X11_PROPERTY 6u
#define CLI_X11_TEXT 7u

static const char *const s_atom_names[CLI_X11_NAMED] = {
    "CLIPBOARD",       "UTF8_STRING", "TARGETS", "INCR", "text/uri-list", "x-special/gnome-copied-files",
    "ORIOR_CLIPBOARD", "TEXT",
};

// `value`'s low 16 and 32 bits written least byte first, as the connection's order is; and read back
EMBED_INLINE void x11_put16(const PosixClipboardCtx *args)
{
    args->at[0] = (unsigned char)(args->value & 0xffu);
    args->at[1] = (unsigned char)((args->value >> 8u) & 0xffu);
}

EMBED_INLINE void x11_put32(const PosixClipboardCtx *args)
{
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = args->at, .value = args->value & 0xffffu);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = args->at + 2, .value = args->value >> 16u);
}

EMBED_INLINE unsigned int x11_get16(const PosixClipboardCtx *args)
{
    return (unsigned int)args->at_read[0] | ((unsigned int)args->at_read[1] << 8u);
}

EMBED_INLINE unsigned int x11_get32(const PosixClipboardCtx *args)
{
    return x11_get16(args) | (EMBED_CALL(x11_get16, PosixClipboardCtx, .at_read = args->at_read + 2) << 16u);
}

// `size` bytes written whole to the server: 1, or 0 where the connection is gone
EMBED_INLINE int x11_send(const PosixClipboardCtx *args)
{
    unsigned long long size = args->size;
    const unsigned char *at = (const unsigned char *)args->bytes;
    while (size > 0ULL)
    {
        const ssize_t wrote = write(args->x11->socket, at, (embed_word)size);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote <= 0)
        {
            return 0;
        }
        // a count written is positive here
        at += wrote;
        size -= (unsigned long long)wrote;
    }
    return 1;
}

// `size` bytes read whole from the server, each piece waited for `wait` milliseconds at the most, -1 for as long as it
// takes: 1, or 0 where none came in time or the connection is gone
EMBED_INLINE int x11_receive(const PosixClipboardCtx *args)
{
    unsigned long long size = args->size;
    unsigned char *at = (unsigned char *)args->bytes_out;
    while (size > 0ULL)
    {
        struct pollfd ready = {args->x11->socket, POLLIN, 0};
        const int polled = poll(&ready, 1u, args->wait);
        if ((polled < 0) && (errno == EINTR))
        {
            continue;
        }
        const ssize_t got = (polled > 0) ? read(args->x11->socket, at, (embed_word)size) : 0;
        if ((got < 0) && (errno == EINTR))
        {
            continue;
        }
        if (got <= 0)
        {
            return 0;
        }
        // a count read is positive here
        at += got;
        size -= (unsigned long long)got;
    }
    return 1;
}

// `size` bytes the server sends passed over, read through `storage`, sized for `capacity_bytes`: 1, or 0 where they
// did not come
EMBED_INLINE int x11_pass(const PosixClipboardCtx *args)
{
    unsigned long long size = args->size;
    while (size > 0ULL)
    {
        const unsigned long long piece = (size < args->capacity_bytes) ? size : args->capacity_bytes;
        if (!EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11, .bytes_out = args->storage, .size = piece,
                        .wait = CLI_X11_WAIT))
        {
            return 0;
        }
        size -= piece;
    }
    return 1;
}

// the next thing the server sends, 32 bytes, read into `packet`; a reply's more bytes read into `extra`, sized for
// `capacity`, their count into `size`, the rest passed over: 1, or 0 where nothing came within `wait` milliseconds
EMBED_INLINE int x11_packet(const PosixClipboardCtx *args)
{
    unsigned long long *size = args->size_pointer;
    *size = 0ULL;
    if (!EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11, .bytes_out = args->packet, .size = 32ULL,
                    .wait = args->wait))
    {
        return 0;
    }
    if ((args->packet[0] & 0x7fu) != CLI_X11_REPLY)
    {
        return 1;
    }
    const unsigned long long more =
        (unsigned long long)EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->packet + 4) * 4ULL;
    *size = (more < args->capacity) ? more : args->capacity;
    unsigned char passed[64];
    return EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11, .bytes_out = args->extra, .size = *size,
                      .wait = CLI_X11_WAIT) &&
           EMBED_CALL(x11_pass, PosixClipboardCtx, .x11 = args->x11, .size = more - *size, .storage = passed,
                      .capacity_bytes = sizeof(passed));
}

// the reply to the last request sent read into `packet` and `extra`, events before it passed over: 1, or 0 where an
// error came in its place or nothing came in time
EMBED_INLINE int x11_reply(const PosixClipboardCtx *args)
{
    for (;;)
    {
        if (!EMBED_CALL(x11_packet, PosixClipboardCtx, .x11 = args->x11, .packet = args->packet, .extra = args->extra,
                        .capacity = args->capacity, .size_pointer = args->size_pointer, .wait = CLI_X11_WAIT))
        {
            return 0;
        }
        const unsigned int type = args->packet[0] & 0x7fu;
        if ((type == CLI_X11_REPLY) || (type == CLI_X11_ERROR))
        {
            return type == CLI_X11_REPLY;
        }
    }
}

// the display DISPLAY names: its host into `host`, sized for `capacity`, empty for this machine's own socket, and its
// number: 1, or 0 where DISPLAY names none
EMBED_INLINE int x11_display(const PosixClipboardCtx *args)
{
    embed_word capacity = args->capacity_bytes_other;
    unsigned int *number = args->number_pointer;
    char display[256];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "DISPLAY",
                    .value_out = display, .capacity = sizeof(display)))
    {
        return 0;
    }
    embed_word colon = 0u;
    int found = 0;
    const unsigned long long end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = display,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = display, .cap = CLI_POOL_BYTES}), .set = ":");
    found = end != 0ULL;
    colon = found ? (embed_word)(end - 1ULL) : 0u;
    if (!found)
    {
        return 0;
    }
    VerbaLinea named = {args->host_out, capacity, 0ULL};
    mmgr_line_put_n(&named, display, colon);
    (void)mmgr_line_finish(&named);
    *number = 0u;
    // a display's number is no more than an unsigned int stores
    *number = (unsigned int)EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = display, .at = colon + 1u,
                                       .size = cellul.len(&(CatenaFinitaCfg){.src = display, .cap = CLI_POOL_BYTES}),
                                       .end = NULL);
    return 1;
}

// a connection made to the display: this machine's socket for it where `host` is empty or `unix`, the abstract one on
// Linux where the file is not there, else the host's port 6000 and on: the socket, or -1
EMBED_INLINE int x11_socket(const PosixClipboardCtx *args)
{
    const int local =
        (args->host[0] == '\0') || cellul.eq(&(CatenaFinitaCfg){.src = args->host, .other = "unix", .cap = 8u});
    if (local)
    {
        for (unsigned int abstract = 0u; abstract < 2u; abstract += 1u)
        {
            struct sockaddr_un address;
            memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
            address.sun_family = AF_UNIX;
            VerbaLinea path = {address.sun_path + abstract, sizeof(address.sun_path) - abstract, 0ULL};
            // the X server's socket stands at this path on every system
            mmgr_line_put(&path, "/tmp/.X11-unix/X"); // NOSONAR
            mmgr_line_decimal(&path, args->number, 1u);
            const unsigned long long length = mmgr_line_finish(&path);
            const int made = socket(AF_UNIX, SOCK_STREAM, 0);
            // an address's length is far below what a socklen_t counts
            const socklen_t size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + abstract + length);
            if ((made >= 0) && (length != 0ULL) && (connect(made, (const struct sockaddr *)&address, size) == 0))
            {
                (void)fcntl(made, F_SETFD, FD_CLOEXEC);
                return made;
            }
            if (made >= 0)
            {
                close(made);
            }
        }
        return -1;
    }
    char port[16];
    VerbaLinea port_line = {port, sizeof(port), 0ULL};
    mmgr_line_decimal(&port_line, 6000u + args->number, 1u);
    (void)mmgr_line_finish(&port_line);
    struct addrinfo hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *found = NULL;
    if (getaddrinfo(args->host, port, &hints, &found) != 0)
    {
        return -1;
    }
    int made = -1;
    for (const struct addrinfo *each = found; (made < 0) && (each != NULL); each = each->ai_next)
    {
        made = socket(each->ai_family, each->ai_socktype, each->ai_protocol);
        if ((made >= 0) && (connect(made, each->ai_addr, each->ai_addrlen) != 0))
        {
            close(made);
            made = -1;
        }
    }
    freeaddrinfo(found);
    if (made >= 0)
    {
        (void)fcntl(made, F_SETFD, FD_CLOEXEC);
    }
    return made;
}

// a field of the Xauthority file at `at`, its length first, two bytes most first: its bytes and their count, the
// place after it into `at`, or NULL where the file ends inside it
EMBED_INLINE const unsigned char *x11_authority_field(const PosixClipboardCtx *args)
{
    unsigned long long *at = args->at_u64s;
    unsigned int *length = args->length;
    if (*at + 2ULL > args->size)
    {
        return NULL;
    }
    *length = ((unsigned int)args->file[*at] << 8u) | args->file[*at + 1ULL];
    const unsigned char *const field = args->file + *at + 2ULL;
    *at += 2ULL + *length;
    return (*at <= args->size) ? field : NULL;
}

// whether `length` bytes of `field` are the text `text`
EMBED_INLINE int x11_field_is(const PosixClipboardCtx *args)
{
    const embed_word text_length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    return (args->length_value == text_length) &&
           ((args->length_value == 0u) ||
            (memor.cmp(&(MemoriaCfg){.src = args->field, .other = args->text, .bytes = args->length_value}) == 0));
}

// the MIT-MAGIC-COOKIE-1 the reader's Xauthority file holds for display `number`, for this machine where `host` is
// empty, into `cookie`, 16 bytes: 1, or 0 where it holds none
EMBED_INLINE int x11_cookie(const PosixClipboardCtx *args)
{
    static unsigned char s_file[CLI_X11_AUTHORITY_MAX];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "XAUTHORITY",
                    .value_out = path, .capacity = sizeof(path)) ||
        (path[0] == '\0'))
    {
        VerbaLinea home = {path, sizeof(path), 0ULL};
        char folder[CLI_PATH_MAX_LEN];
        mmgr_line_put(&home, EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                                        .value_out = folder, .capacity = sizeof(folder))
                                 ? folder
                                 : "");
        mmgr_line_put(&home, "/.Xauthority");
        (void)mmgr_line_finish(&home);
    }
    unsigned long long size = 0ULL;
    if (!EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = path, .from = 0ULL,
                    .bytes_bytes = s_file, .count_u64 = sizeof(s_file), .got = &size))
    {
        return 0;
    }
    char own[256];
    own[0] = '\0';
    (void)gethostname(own, sizeof(own) - 1u);
    own[sizeof(own) - 1u] = '\0';
    char display[16];
    VerbaLinea display_line = {display, sizeof(display), 0ULL};
    mmgr_line_decimal(&display_line, args->number, 1u);
    (void)mmgr_line_finish(&display_line);
    // the families of an address the file names: this machine by its name, and any machine
    static const unsigned int s_local = 256u;
    static const unsigned int s_wild = 65535u;
    for (unsigned long long at = 0ULL; at + 2ULL <= size;)
    {
        const unsigned int family = ((unsigned int)s_file[at] << 8u) | s_file[at + 1ULL];
        at += 2ULL;
        unsigned int lengths[4] = {0u, 0u, 0u, 0u};
        const unsigned char *fields[4] = {NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 4u; field += 1u)
        {
            fields[field] = EMBED_CALL(x11_authority_field, PosixClipboardCtx, .file = s_file, .size = size,
                                       .at_u64s = &at, .length = &lengths[field]);
            if (fields[field] == NULL)
            {
                return 0;
            }
        }
        const int machine = (family == s_wild) || (args->host[0] != '\0') ||
                            ((family == s_local) && EMBED_CALL(x11_field_is, PosixClipboardCtx, .field = fields[0],
                                                               .length_value = lengths[0], .text = own));
        const int displayed = (lengths[1] == 0u) || EMBED_CALL(x11_field_is, PosixClipboardCtx, .field = fields[1],
                                                               .length_value = lengths[1], .text = display);
        if (machine && displayed &&
            EMBED_CALL(x11_field_is, PosixClipboardCtx, .field = fields[2], .length_value = lengths[2],
                       .text = "MIT-MAGIC-COOKIE-1") &&
            (lengths[3] == 16u))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->cookie, .src = fields[3], .bytes = 16u});
            return 1;
        }
    }
    return 0;
}

// the server's answer to the connection's setup read: the window's id made from the ids it gives, the root window and
// the most bytes one request takes: 1, or 0 where it refused the connection
EMBED_INLINE int x11_set_up(const PosixClipboardCtx *args)
{
    unsigned int *root = args->root;
    unsigned char head[8];
    unsigned char fixed[32];
    if (!EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11_out, .bytes_out = head, .size = sizeof(head),
                    .wait = CLI_X11_WAIT) ||
        (head[0] != 1u))
    {
        return 0;
    }
    const unsigned long long length =
        (unsigned long long)EMBED_CALL(x11_get16, PosixClipboardCtx, .at_read = head + 6) * 4ULL;
    if ((length < 40ULL) || !EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11_out, .bytes_out = fixed,
                                        .size = sizeof(fixed), .wait = CLI_X11_WAIT))
    {
        return 0;
    }
    const unsigned int base = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = fixed + 4);
    const unsigned int mask_bits = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = fixed + 8);
    const unsigned long long vendor =
        ((unsigned long long)EMBED_CALL(x11_get16, PosixClipboardCtx, .at_read = fixed + 16) + 3ULL) & ~3ULL;
    args->x11_out->request_max =
        (unsigned long long)EMBED_CALL(x11_get16, PosixClipboardCtx, .at_read = fixed + 18) * 4ULL;
    const unsigned long long formats = (unsigned long long)fixed[21] * 8ULL;
    unsigned char capacity[256];
    unsigned char screen[4];
    if (!EMBED_CALL(x11_pass, PosixClipboardCtx, .x11 = args->x11_out, .size = vendor + formats, .storage = capacity,
                    .capacity_bytes = sizeof(capacity)) ||
        !EMBED_CALL(x11_receive, PosixClipboardCtx, .x11 = args->x11_out, .bytes_out = screen, .size = sizeof(screen),
                    .wait = CLI_X11_WAIT))
    {
        return 0;
    }
    *root = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = screen);
    const unsigned long long read = 32ULL + vendor + formats + 4ULL;
    // the window's id is the first the server gives this client
    args->x11_out->window = base | (mask_bits & (~mask_bits + 1u));
    return (read <= length) && EMBED_CALL(x11_pass, PosixClipboardCtx, .x11 = args->x11_out, .size = length - read,
                                          .storage = capacity, .capacity_bytes = sizeof(capacity));
}

// a connection made to the display, set up, a window of its own made that hears its own property changes, and the
// atoms this file names interned: 1, or 0 where the display could not be reached or refused it
static int x11_connect(const PosixClipboardCtx *args)
{
    char host[256];
    unsigned int number = 0u;
    args->x11_out->socket = EMBED_CALL(x11_display, PosixClipboardCtx, .host_out = host,
                                       .capacity_bytes_other = sizeof(host), .number_pointer = &number)
                                ? EMBED_CALL(x11_socket, PosixClipboardCtx, .host = host, .number = number)
                                : -1;
    if (args->x11_out->socket < 0)
    {
        return 0;
    }
    args->x11_out->window = 0u;
    unsigned char cookie[16];
    const int authorized = EMBED_CALL(x11_cookie, PosixClipboardCtx, .host = host, .number = number, .cookie = cookie);
    unsigned char setup[48];
    memor.set(&(MemoriaCfg){.dst = setup, .val = 0, .bytes = sizeof(setup)});
    setup[0] = 'l';
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = setup + 2, .value = 11u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = setup + 6, .value = authorized ? 18u : 0u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = setup + 8, .value = authorized ? 16u : 0u);
    if (authorized)
    {
        memor.cpy(&(MemoriaCfg){.dst = setup + 12, .src = "MIT-MAGIC-COOKIE-1", .bytes = 18u});
        memor.cpy(&(MemoriaCfg){.dst = setup + 32, .src = cookie, .bytes = 16u});
    }
    unsigned int root = 0u;
    int made = EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11_out, .bytes = setup,
                          .size = authorized ? 48ULL : 12ULL) &&
               EMBED_CALL(x11_set_up, PosixClipboardCtx, .x11_out = args->x11_out, .root = &root);
    unsigned char window[36];
    window[0] = (unsigned char)CLI_X11_CREATE_WINDOW;
    window[1] = 0u;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = window + 2, .value = 9u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 4, .value = args->x11_out->window);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 8, .value = root);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 12, .value = 0u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = window + 16, .value = 1u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = window + 18, .value = 1u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = window + 20, .value = 0u);
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = window + 22, .value = CLI_X11_INPUT_ONLY);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 24, .value = 0u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 28, .value = CLI_X11_EVENT_MASK);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = window + 32, .value = CLI_X11_PROPERTY_CHANGES);
    made =
        made && EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11_out, .bytes = window, .size = sizeof(window));
    {
        unsigned int at = 0u;
        while (made && (at < CLI_X11_NAMED))
        {
            unsigned char intern[8 + 32];
            const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = s_atom_names[at], .cap = 32u});
            const embed_word padded = (length + 3u) & ~(embed_word)3u;
            memor.set(&(MemoriaCfg){.dst = intern, .val = 0, .bytes = sizeof(intern)});
            intern[0] = (unsigned char)CLI_X11_INTERN_ATOM;
            // a request's length in words is far below what 16 bits count
            EMBED_CALL(x11_put16, PosixClipboardCtx, .at = intern + 2, .value = (unsigned int)(2u + (padded / 4u)));
            EMBED_CALL(x11_put16, PosixClipboardCtx, .at = intern + 4, .value = (unsigned int)length);
            memor.cpy(&(MemoriaCfg){.dst = intern + 8, .src = s_atom_names[at], .bytes = length});
            made = EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11_out, .bytes = intern, .size = 8u + padded);
            at += 1u;
        }
    }
    {
        unsigned int at = 0u;
        while (made && (at < CLI_X11_NAMED))
        {
            unsigned char reply[32];
            unsigned long long size = 0ULL;
            made = EMBED_CALL(x11_reply, PosixClipboardCtx, .x11 = args->x11_out, .packet = reply, .extra = NULL,
                              .capacity = 0ULL, .size_pointer = &size);
            args->x11_out->atom[at] = made ? EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = reply + 8) : 0u;
            at += 1u;
        }
    }
    if (!made)
    {
        close(args->x11_out->socket);
        args->x11_out->socket = -1;
    }
    return made;
}

// the connection let go
EMBED_INLINE void x11_close(const PosixClipboardCtx *args)
{
    if (args->x11_out->socket >= 0)
    {
        close(args->x11_out->socket);
    }
    args->x11_out->socket = -1;
}

// `owner` made the owner of the clipboard, None where it is 0: 1, or 0 where the connection is gone
EMBED_INLINE int x11_owner_set(const PosixClipboardCtx *args)
{
    unsigned char request[16];
    request[0] = (unsigned char)CLI_X11_SET_OWNER;
    request[1] = 0u;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = request + 2, .value = 4u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 4, .value = args->owner);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 8, .value = args->x11->atom[CLI_X11_CLIPBOARD]);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 12, .value = 0u);
    return EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = request, .size = sizeof(request));
}

// `size` bytes of `bytes` put as the property `property` of `window`, of `property_type` in units of `format` bits,
// replacing what it had: 1, or 0 where the connection is gone
EMBED_INLINE int x11_property_append(const PosixClipboardCtx *args)
{
    unsigned char request[24];
    const unsigned long long padded = (args->size + 3ULL) & ~3ULL;
    static const unsigned char s_pad[4] = {0u, 0u, 0u, 0u};
    request[0] = (unsigned char)CLI_X11_CHANGE_PROPERTY;
    request[1] = 0u;
    // a piece sent is below CLI_X11_SENT_MAX, and its request's words are below what 16 bits count
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = request + 2, .value = (unsigned int)(6ULL + (padded / 4ULL)));
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 4, .value = args->window);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 8, .value = args->property);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 12, .value = args->property_type);
    request[16] = (unsigned char)args->format;
    request[17] = 0u;
    request[18] = 0u;
    request[19] = 0u;
    // the units of the bytes are below what 32 bits count, as a piece is
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 20,
               .value = (unsigned int)(args->size / (args->format / 8u)));
    return EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = request, .size = sizeof(request)) &&
           x11_send(args) &&
           EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = s_pad, .size = padded - args->size);
}

// the property `property` of `window` deleted
EMBED_INLINE int x11_property_delete(const PosixClipboardCtx *args)
{
    unsigned char request[12];
    request[0] = (unsigned char)CLI_X11_DELETE_PROPERTY;
    request[1] = 0u;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = request + 2, .value = 3u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 4, .value = args->window);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 8, .value = args->property);
    return EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = request, .size = sizeof(request));
}

// the next event of `type` for this connection's window, `property` where it is a property's change, waited for, every
// other event passed over: 1, or 0 where none came in time
EMBED_INLINE int x11_event(const PosixClipboardCtx *args)
{
    for (;;)
    {
        unsigned long long size = 0ULL;
        if (!EMBED_CALL(x11_packet, PosixClipboardCtx, .x11 = args->x11, .packet = args->packet, .extra = NULL,
                        .capacity = 0ULL, .size_pointer = &size, .wait = CLI_X11_WAIT))
        {
            return 0;
        }
        const unsigned int received = args->packet[0] & 0x7fu;
        const int ours = (received == CLI_X11_SELECTION_NOTIFY) &&
                         (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->packet + 8) == args->x11->window);
        const int changed =
            (received == CLI_X11_PROPERTY_NOTIFY) &&
            (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->packet + 4) == args->x11->window) &&
            (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->packet + 8) == args->property) &&
            (args->packet[16] == 0u);
        if ((received == args->type) && (ours || changed))
        {
            return 1;
        }
    }
}

// the property this connection reads the clipboard into read whole and appended to `out`, then deleted: its type into
// `type` and its bytes into `size`: 1, or 0 where it could not be read
EMBED_INLINE int x11_property_alloc(const PosixClipboardCtx *args)
{
    unsigned int *type = args->type_pointer;
    unsigned long long *size = args->size_pointer;
    static unsigned char s_piece[CLI_X11_PIECE];
    *size = 0ULL;
    for (unsigned long long offset = 0ULL;;)
    {
        unsigned char request[24];
        request[0] = (unsigned char)CLI_X11_GET_PROPERTY;
        request[1] = 0u;
        EMBED_CALL(x11_put16, PosixClipboardCtx, .at = request + 2, .value = 6u);
        EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 4, .value = args->x11->window);
        EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 8, .value = args->x11->atom[CLI_X11_PROPERTY]);
        EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 12, .value = 0u);
        // an offset in words is below what 32 bits count for any property a server holds
        EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 16, .value = (unsigned int)offset);
        EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 20, .value = CLI_X11_PIECE / 4u);
        unsigned char reply[32];
        unsigned long long got = 0ULL;
        if (!EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = request, .size = sizeof(request)) ||
            !EMBED_CALL(x11_reply, PosixClipboardCtx, .x11 = args->x11, .packet = reply, .extra = s_piece,
                        .capacity = sizeof(s_piece), .size_pointer = &got))
        {
            return 0;
        }
        *type = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = reply + 8);
        const unsigned long long after = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = reply + 12);
        const unsigned long long bytes =
            (unsigned long long)EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = reply + 16) *
            ((reply[1] != 0u) ? reply[1] / 8u : 1u);
        mmgr_line_put_n(args->out, (const char *)s_piece, (bytes < got) ? bytes : got);
        *size += (bytes < got) ? bytes : got;
        if (after == 0ULL)
        {
            break;
        }
        offset += got / 4ULL;
    }
    return EMBED_CALL(x11_property_delete, PosixClipboardCtx, .x11 = args->x11, .window = args->x11->window,
                      .property = args->x11->atom[CLI_X11_PROPERTY]);
}

// the clipboard's content as `target` read into `out`, a content sent in pieces read piece by piece as INCR sends it:
// 1, or 0 where its owner has none as `target` or does not answer in time
static int x11_convert(const PosixClipboardCtx *args)
{
    unsigned char request[24];
    request[0] = (unsigned char)CLI_X11_CONVERT;
    request[1] = 0u;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = request + 2, .value = 6u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 4, .value = args->x11->window);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 8, .value = args->x11->atom[CLI_X11_CLIPBOARD]);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 12, .value = args->target);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 16, .value = args->x11->atom[CLI_X11_PROPERTY]);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = request + 20, .value = 0u);
    unsigned char packet[32];
    if (!EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = args->x11, .bytes = request, .size = sizeof(request)) ||
        !EMBED_CALL(x11_event, PosixClipboardCtx, .x11 = args->x11, .type = CLI_X11_SELECTION_NOTIFY, .property = 0u,
                    .packet = packet) ||
        (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = packet + 20) == 0u))
    {
        return 0;
    }
    unsigned int type = 0u;
    unsigned long long size = 0ULL;
    const unsigned long long before = args->out->at;
    if (!EMBED_CALL(x11_property_alloc, PosixClipboardCtx, .x11 = args->x11, .out = args->out, .type_pointer = &type,
                    .size_pointer = &size))
    {
        return 0;
    }
    if (type != args->x11->atom[CLI_X11_INCR])
    {
        return 1;
    }
    // the INCR property's own bytes, the content's size, taken back off; its deletion asked for the first piece, and
    // each piece is read as it is put, the content's end a piece of none
    args->out->at = before;
    for (size = 1ULL; size > 0ULL;)
    {
        if (!EMBED_CALL(x11_event, PosixClipboardCtx, .x11 = args->x11, .type = CLI_X11_PROPERTY_NOTIFY,
                        .property = args->x11->atom[CLI_X11_PROPERTY], .packet = packet) ||
            !EMBED_CALL(x11_property_alloc, PosixClipboardCtx, .x11 = args->x11, .out = args->out,
                        .type_pointer = &type, .size_pointer = &size))
        {
            return 0;
        }
    }
    return 1;
}

// What the clipboard serves while this window owns it: the connection it owns the clipboard through, whether its
// thread runs, starts or failed to start, and whether the window took the clipboard back since the server last told
// it of a clear; the text, the list of file URIs and GNOME's copied files, each a cell of its own, NULL for none, and
// their bytes; a content sent in pieces: to whom, as what, of what type, how far, and whether it goes on; and the
// thread
typedef struct
{
    CliX11 x11;
    int owning;
    int starting;
    int reowned;
    const CarcerMovableEntry *content[3];
    unsigned long long size[3];
    unsigned int requestor;
    unsigned int property;
    unsigned int type;
    unsigned int piece_of;
    unsigned long long sent;
    int sending;
    CliThread thread;
} CliX11Owner;

static CliX11Owner s_owner;

// every content's cell given back
static void owner_contents_release(void)
{
    for (unsigned int place = 0u; place < 3u; place += 1u)
    {
        if (s_owner.content[place] != NULL)
        {
            (void)g_cli_guard->movable_buf_release(s_owner.content[place], g_cli_platform_owner);
        }
        s_owner.content[place] = NULL;
        s_owner.size[place] = 0ULL;
    }
}

// the contents by place: the text, the URIs and the copied files
#define CLI_X11_OWNER_TEXT 0u
#define CLI_X11_OWNER_URIS 1u
#define CLI_X11_OWNER_COPIED 2u

static CliWake s_started = CLI_WAKE_INIT;

// the place of the content `target` asks for, or 3 where the clipboard holds none as it; under the lock
EMBED_INLINE unsigned int owner_content_get(const PosixClipboardCtx *args)
{
    const unsigned int *const atom = s_owner.x11.atom;
    const int text = (args->target == atom[CLI_X11_UTF8]) || (args->target == atom[CLI_X11_TEXT]) ||
                     (args->target == CLI_X11_ATOM_STRING);
    unsigned int place;
    if (text)
    {
        place = CLI_X11_OWNER_TEXT;
    }
    else if (args->target == atom[CLI_X11_URIS])
    {
        place = CLI_X11_OWNER_URIS;
    }
    else if (args->target == atom[CLI_X11_COPIED])
    {
        place = CLI_X11_OWNER_COPIED;
    }
    else
    {
        place = 3u;
    }
    return ((place < 3u) && (s_owner.content[place] != NULL)) ? place : 3u;
}

// the program that asked told what it was given: the property it was put in, or none where it was refused
EMBED_INLINE void owner_notify(const PosixClipboardCtx *args)
{
    unsigned char event[44];
    memor.set(&(MemoriaCfg){.dst = event, .val = 0, .bytes = sizeof(event)});
    event[0] = (unsigned char)CLI_X11_SEND_EVENT;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = event + 2, .value = 11u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 4,
               .value = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 12));
    event[12] = (unsigned char)CLI_X11_SELECTION_NOTIFY;
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 16,
               .value = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 4));
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 20,
               .value = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 12));
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 24,
               .value = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 16));
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 28,
               .value = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 20));
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = event + 32, .value = args->property);
    (void)EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = &s_owner.x11, .bytes = event, .size = sizeof(event));
}

// a program's request for the clipboard answered: the targets it holds, or its content as the target asked, put on the
// requestor's property whole or, past one request's capacity, written in pieces as INCR sends them; under the lock
EMBED_INLINE void owner_request(const PosixClipboardCtx *args)
{
    const CliX11 *const x11 = &s_owner.x11;
    const unsigned int requestor = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 12);
    const unsigned int target = EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 20);
    // an old program names no property, and is given its content in the property its target names
    const unsigned int property = (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 24) != 0u)
                                      ? EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->request + 24)
                                      : target;
    if (target == x11->atom[CLI_X11_TARGETS])
    {
        unsigned char targets[6 * 4];
        unsigned int count = 0u;
        const unsigned int named[6] = {x11->atom[CLI_X11_TARGETS], x11->atom[CLI_X11_UTF8], CLI_X11_ATOM_STRING,
                                       x11->atom[CLI_X11_TEXT],    x11->atom[CLI_X11_URIS], x11->atom[CLI_X11_COPIED]};
        for (unsigned int at = 0u; at < 6u; at += 1u)
        {
            if ((at == 0u) || (EMBED_CALL(owner_content_get, PosixClipboardCtx, .target = named[at]) < 3u))
            {
                EMBED_CALL(x11_put32, PosixClipboardCtx, .at = targets + (count * 4u), .value = named[at]);
                count += 1u;
            }
        }
        (void)EMBED_CALL(x11_property_append, PosixClipboardCtx, .x11 = x11, .window = requestor, .property = property,
                         .property_type = CLI_X11_ATOM_ATOM, .format = 32u, .bytes = targets, .size = count * 4ULL);
        EMBED_CALL(owner_notify, PosixClipboardCtx, .request = args->request, .property = property);
        return;
    }
    const unsigned int place = EMBED_CALL(owner_content_get, PosixClipboardCtx, .target = target);
    if ((place >= 3u) || s_owner.sending)
    {
        EMBED_CALL(owner_notify, PosixClipboardCtx, .request = args->request, .property = 0u);
        return;
    }
    const unsigned int type = (target == x11->atom[CLI_X11_TEXT]) ? x11->atom[CLI_X11_UTF8] : target;
    const unsigned long long most = (x11->request_max > 64ULL) ? x11->request_max - 64ULL : 0ULL;
    const unsigned long long whole = (most < CLI_X11_SENT_MAX) ? most : CLI_X11_SENT_MAX;
    if (s_owner.size[place] <= whole)
    {
        // the content read out of its cell into a cell of this call and sent whole
        char *const sent = g_cli_guard->temporary_buf_alloc(s_owner.size[place] + 1ULL, g_cli_platform_owner);
        if (sent != NULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_owner.content[place], .offset = 0ULL, .dst = sent, .bytes = s_owner.size[place]});
            (void)EMBED_CALL(x11_property_append, PosixClipboardCtx, .x11 = x11, .window = requestor,
                             .property = property, .property_type = type, .format = 8u, .bytes = sent,
                             .size = s_owner.size[place]);
        }
        g_cli_guard->persistent_buf_release(sent, g_cli_platform_owner);
        EMBED_CALL(owner_notify, PosixClipboardCtx, .request = args->request, .property = property);
        return;
    }
    // the requestor's property changes heard, each deletion asking for the next piece
    unsigned char receive[16];
    receive[0] = (unsigned char)CLI_X11_CHANGE_WINDOW;
    receive[1] = 0u;
    EMBED_CALL(x11_put16, PosixClipboardCtx, .at = receive + 2, .value = 4u);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = receive + 4, .value = requestor);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = receive + 8, .value = CLI_X11_EVENT_MASK);
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = receive + 12, .value = CLI_X11_PROPERTY_CHANGES);
    unsigned char size[4];
    // a content's size is put as INCR's lower bound, cut to what 32 bits count
    EMBED_CALL(x11_put32, PosixClipboardCtx, .at = size,
               .value = (s_owner.size[place] < 0xffffffffULL) ? (unsigned int)s_owner.size[place] : 0xffffffffu);
    (void)EMBED_CALL(x11_send, PosixClipboardCtx, .x11 = x11, .bytes = receive, .size = sizeof(receive));
    (void)EMBED_CALL(x11_property_append, PosixClipboardCtx, .x11 = x11, .window = requestor, .property = property,
                     .property_type = x11->atom[CLI_X11_INCR], .format = 32u, .bytes = size, .size = sizeof(size));
    EMBED_CALL(owner_notify, PosixClipboardCtx, .request = args->request, .property = property);
    s_owner.requestor = requestor;
    s_owner.property = property;
    s_owner.type = type;
    s_owner.piece_of = place;
    s_owner.sent = 0ULL;
    s_owner.sending = 1;
}

// the next piece of a content sent in pieces put, as the requestor deleted the last: the end a piece of none; under the
// lock
EMBED_INLINE void owner_piece_send(const PosixClipboardCtx *args)
{
    const int deleted = (args->event[16] == 1u) &&
                        (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->event + 4) == s_owner.requestor) &&
                        (EMBED_CALL(x11_get32, PosixClipboardCtx, .at_read = args->event + 8) == s_owner.property);
    if (!s_owner.sending || !deleted)
    {
        return;
    }
    // the next piece read out of the content's cell into a cell of this call and sent
    const CarcerMovableEntry *const content = s_owner.content[s_owner.piece_of];
    const unsigned long long left = (content != NULL) ? s_owner.size[s_owner.piece_of] - s_owner.sent : 0ULL;
    const unsigned long long piece = (left < CLI_X11_SENT_MAX) ? left : CLI_X11_SENT_MAX;
    char *const sent = g_cli_guard->temporary_buf_alloc(piece + 1ULL, g_cli_platform_owner);
    if ((sent != NULL) && (piece > 0ULL))
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = content, .offset = s_owner.sent, .dst = sent, .bytes = piece});
    }
    (void)EMBED_CALL(x11_property_append, PosixClipboardCtx, .x11 = &s_owner.x11, .window = s_owner.requestor,
                     .property = s_owner.property, .property_type = s_owner.type, .format = 8u,
                     .bytes = (sent != NULL) ? sent : "", .size = (sent != NULL) ? piece : 0ULL);
    g_cli_guard->persistent_buf_release(sent, g_cli_platform_owner);
    s_owner.sent += piece;
    s_owner.sending = piece > 0ULL;
}

// the clipboard owned and served until another program takes it or it is emptied
static void owner_run(void *context)
{
    (void)context;
    static unsigned char s_reply[CLI_X11_PIECE];
    CliX11 *const x11 = &s_owner.x11;
    s_owner.owning = EMBED_CALL(x11_connect, PosixClipboardCtx, .x11_out = x11) &&
                     EMBED_CALL(x11_owner_set, PosixClipboardCtx, .x11 = x11, .owner = x11->window);
    s_owner.starting = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_started);
    for (int serving = s_owner.owning; serving;)
    {
        unsigned char packet[32];
        unsigned long long size = 0ULL;
        serving = EMBED_CALL(x11_packet, PosixClipboardCtx, .x11 = x11, .packet = packet, .extra = s_reply,
                             .capacity = sizeof(s_reply), .size_pointer = &size, .wait = -1);
        const unsigned int type = packet[0] & 0x7fu;
        if (serving && (type == CLI_X11_SELECTION_CLEAR))
        {
            // a clear that came before this window took the clipboard back is past
            serving = s_owner.reowned;
            s_owner.reowned = 0;
        }
        else if (serving && (type == CLI_X11_SELECTION_REQUEST))
        {
            EMBED_CALL(owner_request, PosixClipboardCtx, .request = packet);
        }
        else if (serving && (type == CLI_X11_PROPERTY_NOTIFY))
        {
            EMBED_CALL(owner_piece_send, PosixClipboardCtx, .event = packet);
        }
    }
    s_owner.owning = 0;
    owner_contents_release();
    s_owner.sending = 0;
    EMBED_CALL(x11_close, PosixClipboardCtx, .x11_out = x11);
}

// `size` bytes of `bytes` written into a movable cell of their own as the content at `place`: 1, or 0 where no cell
// could be taken; under the lock
EMBED_INLINE int owner_content_append(const PosixClipboardCtx *args)
{
    const char *bytes = args->bytes_text;
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(args->size + 1ULL, g_cli_platform_owner);
    if (cell == NULL)
    {
        return 0;
    }
    const char zero = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = bytes, .bytes = args->size});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = args->size, .src = &zero, .bytes = 1ULL});
    s_owner.content[args->place] = cell;
    s_owner.size[args->place] = args->size;
    return 1;
}

// the clipboard owned with the contents put since the last were let go, its thread started where it does not run:
// 1, or 0 where the display could not be reached; under the lock
static int owner_own(void)
{
    if (s_owner.owning)
    {
        s_owner.reowned = 1;
        return EMBED_CALL(x11_owner_set, PosixClipboardCtx, .x11 = &s_owner.x11, .owner = s_owner.x11.window);
    }
    s_owner.starting = 1;
    s_owner.thread = (CliThread){owner_run, NULL, CLI_PRIORITY_WORK};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_owner.thread))
    {
        s_owner.starting = 0;
        return 0;
    }
    while (s_owner.starting)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_started);
    }
    return s_owner.owning;
}

int cli_clipboard_text_read(const CliCtx *args)
{
    const int own = s_owner.owning && (s_owner.content[CLI_X11_OWNER_TEXT] != NULL);
    if (own)
    {
        // the text read out of its cell into a cell of this call, then put on the line
        const unsigned long long size = s_owner.size[CLI_X11_OWNER_TEXT];
        char *const text = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_platform_owner);
        if (text != NULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_owner.content[CLI_X11_OWNER_TEXT], .offset = 0ULL, .dst = text, .bytes = size});
            mmgr_line_put_n(args->text_verba_linea_pointer, text, size);
        }
        g_cli_guard->persistent_buf_release(text, g_cli_platform_owner);
    }
    if (own)
    {
        return 1;
    }
    CliX11 x11;
    const int read = EMBED_CALL(x11_connect, PosixClipboardCtx, .x11_out = &x11) &&
                     (EMBED_CALL(x11_convert, PosixClipboardCtx, .x11 = &x11, .target = x11.atom[CLI_X11_UTF8],
                                 .out = args->text_verba_linea_pointer) ||
                      EMBED_CALL(x11_convert, PosixClipboardCtx, .x11 = &x11, .target = CLI_X11_ATOM_STRING,
                                 .out = args->text_verba_linea_pointer));
    EMBED_CALL(x11_close, PosixClipboardCtx, .x11_out = &x11);
    return read;
}

int cli_clipboard_text_write(const CliSizeTextCtx *args)
{
    owner_contents_release();
    s_owner.sending = 0;
    const int owner = EMBED_CALL(owner_content_append, PosixClipboardCtx, .place = CLI_X11_OWNER_TEXT,
                                 .bytes_text = args->text, .size = args->size) &&
                      owner_own();
    return owner;
}

int cli_clipboard_files_append(const CliCountCtx *args)
{
    // a path's byte is written as at most 3 in a URI, and each path adds its scheme and a line's end
    const unsigned long long uri_capacity = (unsigned long long)args->count * ((3ULL * CLI_PATH_MAX_LEN) + 16ULL);
    const unsigned long long text_capacity = (unsigned long long)args->count * (CLI_PATH_MAX_LEN + 1ULL);
    char *const uris_cell = g_cli_guard->temporary_buf_alloc(uri_capacity + 1ULL, g_cli_platform_owner);
    char *const copied_cell = g_cli_guard->temporary_buf_alloc(uri_capacity + 8ULL, g_cli_platform_owner);
    char *const text_cell = g_cli_guard->temporary_buf_alloc(text_capacity + 1ULL, g_cli_platform_owner);
    VerbaLinea uris = mmgr_line_over(uris_cell, uri_capacity + 1ULL);
    VerbaLinea copied = mmgr_line_over(copied_cell, uri_capacity + 8ULL);
    VerbaLinea text = mmgr_line_over(text_cell, text_capacity + 1ULL);
    mmgr_line_put(&copied, args->cut ? "cut" : "copy");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(clipboard_uri_write, PosixClipboardCtx, .out = &uris, .path = args->paths[at]);
        mmgr_line_put(&uris, "\r\n");
        mmgr_line_ch(&copied, '\n');
        EMBED_CALL(clipboard_uri_write, PosixClipboardCtx, .out = &copied, .path = args->paths[at]);
        mmgr_line_put(&text, (at > 0u) ? "\n" : "");
        mmgr_line_put(&text, args->paths[at]);
    }
    owner_contents_release();
    s_owner.sending = 0;
    const int owner = mmgr_line_ok(&uris) && mmgr_line_ok(&copied) && mmgr_line_ok(&text) &&
                      EMBED_CALL(owner_content_append, PosixClipboardCtx, .place = CLI_X11_OWNER_URIS,
                                 .bytes_text = uris.out, .size = uris.at) &&
                      EMBED_CALL(owner_content_append, PosixClipboardCtx, .place = CLI_X11_OWNER_COPIED,
                                 .bytes_text = copied.out, .size = copied.at) &&
                      EMBED_CALL(owner_content_append, PosixClipboardCtx, .place = CLI_X11_OWNER_TEXT,
                                 .bytes_text = text.out, .size = text.at) &&
                      owner_own();
    g_cli_guard->persistent_buf_release(uris.out, g_cli_platform_owner);
    g_cli_guard->persistent_buf_release(copied.out, g_cli_platform_owner);
    g_cli_guard->persistent_buf_release(text.out, g_cli_platform_owner);
    return owner;
}

// the files GNOME's copied-files target holds, `size` bytes of `list`, its first line `cut` or `copy`, handed to
// `each`, 1 in `cut` where they were cut: how many
EMBED_INLINE unsigned int clipboard_copied_read(const PosixClipboardCtx *args)
{
    unsigned long long size = args->size;
    int *cut = args->cut;
    unsigned long long first = 0ULL;
    first = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->list, .at = first,
                       .size = size, .set = "\n");
    *cut = (first == 3ULL) && (args->list[0] == 'c') && (args->list[1] == 'u') && (args->list[2] == 't');
    return (first < size) ? EMBED_CALL(clipboard_uris_read, PosixClipboardCtx, .list = args->list + first + 1ULL,
                                       .size = size - first - 1ULL, .each = args->each, .context = args->context)
                          : 0u;
}

unsigned int cli_clipboard_files_read(const CliEachCtx *args)
{
    void (*each)(const CliEachPathCfg *) = args->each;
    void *context = args->context;
    int *cut = args->cut_pointer;
    *cut = -1;
    unsigned int count = 0u;
    // a clipboard this window owns holds the files it put, or none where it put text
    const int own = s_owner.owning;
    if (own && (s_owner.content[CLI_X11_OWNER_COPIED] != NULL))
    {
        count = EMBED_CALL(clipboard_copied_read, PosixClipboardCtx, .list = s_owner.content[CLI_X11_OWNER_COPIED],
                           .size = s_owner.size[CLI_X11_OWNER_COPIED], .each = each, .context = context, .cut = cut);
    }
    if (own)
    {
        return count;
    }
    const unsigned long long list_capacity = 4096u;
    char *const list_cell = g_cli_guard->temporary_buf_alloc(list_capacity, g_cli_platform_owner);
    VerbaLinea list = mmgr_line_over(list_cell, list_capacity);
    if (list.out == NULL)
    {
        return 0u;
    }
    CliX11 x11;
    if (EMBED_CALL(x11_connect, PosixClipboardCtx, .x11_out = &x11))
    {
        if (EMBED_CALL(x11_convert, PosixClipboardCtx, .x11 = &x11, .target = x11.atom[CLI_X11_COPIED], .out = &list) &&
            mmgr_line_ok(&list))
        {
            count = EMBED_CALL(clipboard_copied_read, PosixClipboardCtx, .list = list.out, .size = list.at,
                               .each = each, .context = context, .cut = cut);
        }
        else
        {
            list.at = 0ULL;
            count = (EMBED_CALL(x11_convert, PosixClipboardCtx, .x11 = &x11, .target = x11.atom[CLI_X11_URIS],
                                .out = &list) &&
                     mmgr_line_ok(&list))
                        ? EMBED_CALL(clipboard_uris_read, PosixClipboardCtx, .list = list.out, .size = list.at,
                                     .each = each, .context = context)
                        : 0u;
        }
    }
    EMBED_CALL(x11_close, PosixClipboardCtx, .x11_out = &x11);
    g_cli_guard->persistent_buf_release(list.out, g_cli_platform_owner);
    return count;
}

void cli_clipboard_clear(void)
{
    const int own = s_owner.owning;
    if (own)
    {
        // the server tells the owner its clipboard is cleared, which ends its thread
        s_owner.reowned = 0;
        (void)EMBED_CALL(x11_owner_set, PosixClipboardCtx, .x11 = &s_owner.x11, .owner = 0u);
    }
    if (!own)
    {
        CliX11 x11;
        if (EMBED_CALL(x11_connect, PosixClipboardCtx, .x11_out = &x11))
        {
            (void)EMBED_CALL(x11_owner_set, PosixClipboardCtx, .x11 = &x11, .owner = 0u);
        }
        EMBED_CALL(x11_close, PosixClipboardCtx, .x11_out = &x11);
    }
}
#endif
#endif
