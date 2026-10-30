// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_clipboard.c: the system's clipboard on Linux and macOS. On Linux it is the X server's CLIPBOARD selection,
// spoken over the display's socket in X11's own protocol, which Wayland desktops share through XWayland: its text read
// as UTF8_STRING, and text and files put by owning the selection, a thread of the clipboard's own answering each
// program that asks for what it holds, a content past one request's room sent in pieces as INCR sends it; files go as
// the file manager puts them, a list of file URIs, a cut said in GNOME's copied-files target. On macOS it is the
// system's pasteboard, its text through pbpaste and pbcopy and its files through osascript's bridge to AppKit
#if !defined(_WIN32)
// sockets, poll, gethostname and the descriptor flags are POSIX, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

// the file URI a path is written in on the clipboard, and the characters of a path a URI keeps as they are
#define CLI_CLIPBOARD_FILE_SCHEME "file://"

// whether `letter` stands in a file URI as it is
static int clipboard_unreserved(unsigned char letter)
{
    return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
           ((letter >= '0') && (letter <= '9')) || (letter == '-') || (letter == '.') || (letter == '_') ||
           (letter == '~') || (letter == '/');
}

// `path` written as a file URI, each byte a URI does not keep written as `%XX`
static void clipboard_uri_written(VerbaLinea *out, const char *path)
{
    static const char s_digits[] = "0123456789ABCDEF";
    mmgr_line_put(out, CLI_CLIPBOARD_FILE_SCHEME);
    for (const unsigned char *at = (const unsigned char *)path; *at != 0u; at += 1)
    {
        if (clipboard_unreserved(*at))
        {
            // a byte a URI keeps is an ASCII letter
            mmgr_line_ch(out, (char)*at);
        }
        else
        {
            mmgr_line_ch(out, '%');
            mmgr_line_ch(out, s_digits[*at >> 4u]);
            mmgr_line_ch(out, s_digits[*at & 0x0fu]);
        }
    }
}

// the value of the hexadecimal digit `digit`, or 16 where it is none
static unsigned int clipboard_hex_of(char digit)
{
    // a digit's distance from the first of its run is below 16 where the digit is one
    return ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
           : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a') + 10u
           : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A') + 10u
                                                : 16u;
}

// each file of a list of URIs, `size` bytes of `list`, one to a line, its comments passed over, handed to `each` as a
// path, its host taken off and its escapes read: how many
static unsigned int clipboard_uris_read(const char *list, unsigned long long size,
                                        void (*each)(void *context, const char *path), void *context)
{
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long end = at;
        while ((end < size) && (list[end] != '\n') && (list[end] != '\r'))
        {
            end += 1ull;
        }
        const unsigned long long scheme = sizeof(CLI_CLIPBOARD_FILE_SCHEME) - 1u;
        int file = (end - at > scheme);
        for (unsigned long long letter = 0ull; file && (letter < scheme); letter += 1ull)
        {
            file = list[at + letter] == CLI_CLIPBOARD_FILE_SCHEME[letter];
        }
        if (file)
        {
            unsigned long long from = at + scheme;
            while ((from < end) && (list[from] != '/'))
            {
                from += 1ull;
            }
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea decoded = {path, sizeof(path), 0ull};
            for (unsigned long long letter = from; letter < end; letter += 1ull)
            {
                const int escaped = (list[letter] == '%') && (letter + 2ull < end) &&
                                    (clipboard_hex_of(list[letter + 1ull]) < 16u) &&
                                    (clipboard_hex_of(list[letter + 2ull]) < 16u);
                // two hexadecimal digits make a byte, below 256
                mmgr_line_ch(&decoded, escaped ? (char)((clipboard_hex_of(list[letter + 1ull]) << 4u) |
                                                        clipboard_hex_of(list[letter + 2ull]))
                                               : list[letter]);
                letter += escaped ? 2ull : 0ull;
            }
            if (mmgr_line_finish(&decoded) != 0ull)
            {
                each(context, path);
                count += 1u;
            }
        }
        at = end + 1ull;
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
static const char s_files_put_script[] =
    "ObjC.import('AppKit'); const input = $.NSFileHandle.fileHandleWithStandardInput.readDataToEndOfFile; "
    "const text = $.NSString.alloc.initWithDataEncoding(input, $.NSUTF8StringEncoding).js; "
    "const urls = text.split('\\n').filter((path) => path.length).map((path) => $.NSURL.fileURLWithPath(path)); "
    "const board = $.NSPasteboard.generalPasteboard; board.clearContents; board.writeObjects($(urls)); '';";
static const char s_emptied_script[] = "ObjC.import('AppKit'); $.NSPasteboard.generalPasteboard.clearContents; '';";

// the encoding pbcopy and pbpaste read and write the text in
static const CliSetting s_utf8[1] = {{"LANG", "en_US.UTF-8"}};

// `words` run, `size` bytes of `input` fed to it, what it wrote handed to `out` where `out` is not NULL: 1, or 0 where
// it did not run or ended with a code other than 0
static int clipboard_ran(const char *const *words, const char *input, unsigned long long size, VerbaLinea *out)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    const int done = cli_ran_fed(words, NULL, s_utf8, 1u, input, size, ran, &err_msg) && (ran->code == 0ll);
    if (done && (out != NULL))
    {
        mmgr_line_put_n(out, ran->out, ran->out_size);
    }
    cli_ran_given(ran);
    return done;
}

int cli_clipboard_text_read(VerbaLinea *text)
{
    static const char *const s_words[2] = {"pbpaste", NULL};
    return clipboard_ran(s_words, "", 0ull, text);
}

int cli_clipboard_text_written(const char *text, unsigned long long size)
{
    static const char *const s_words[2] = {"pbcopy", NULL};
    return clipboard_ran(s_words, text, size, NULL);
}

int cli_clipboard_files_put(const char *const *paths, unsigned int count, int cut)
{
    // a cut is known to the window alone on macOS, whose pasteboard says no cut of files
    (void)cut;
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_put_script, NULL};
    VerbaLinea input;
    if (!cli_growing_line_temporary(&input, 4096u, &g_cli_platform_cells))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&input, paths[at]);
        mmgr_line_ch(&input, '\n');
    }
    const int put = mmgr_line_ok(&input) && clipboard_ran(s_words, input.out, input.at, NULL);
    g_cli_cells->persistent_buf_release(input.out, &g_cli_platform_cells);
    return put;
}

// a path osascript wrote, one to a line, handed on
typedef struct
{
    void (*each)(void *context, const char *path);
    void *context;
} CliClipboardEach;

unsigned int cli_clipboard_files_held(void (*each)(void *context, const char *path), void *context, int *cut)
{
    *cut = -1;
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_held_script, NULL};
    VerbaLinea paths;
    if (!cli_growing_line_temporary(&paths, 4096u, &g_cli_platform_cells))
    {
        return 0u;
    }
    unsigned int count = 0u;
    if (clipboard_ran(s_words, "", 0ull, &paths) && mmgr_line_ok(&paths))
    {
        for (unsigned long long at = 0ull; at < paths.at;)
        {
            unsigned long long end = at;
            while ((end < paths.at) && (paths.out[end] != '\n'))
            {
                end += 1ull;
            }
            if (end > at)
            {
                char path[CLI_PATH_MAX_LEN];
                VerbaLinea line = {path, sizeof(path), 0ull};
                mmgr_line_put_n(&line, paths.out + at, end - at);
                if (mmgr_line_finish(&line) != 0ull)
                {
                    each(context, path);
                    count += 1u;
                }
            }
            at = end + 1ull;
        }
    }
    g_cli_cells->persistent_buf_release(paths.out, &g_cli_platform_cells);
    return count;
}

void cli_clipboard_emptied(void)
{
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_emptied_script, NULL};
    (void)clipboard_ran(s_words, "", 0ull, NULL);
}
#else
// the core protocol's atoms this file names by number, the kinds of what the server sends, the events it reports and
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
// bytes a content is sent in at once, below one request's room; and the most bytes of the reader's Xauthority file read
#define CLI_X11_WAIT 2000
#define CLI_X11_PIECE (64u << 10u)
#define CLI_X11_SENT_MOST (256u << 10u)
#define CLI_X11_AUTHORITY_MOST (64u << 10u)

// the atoms this file interns, by place
#define CLI_X11_CLIPBOARD 0u
#define CLI_X11_UTF8 1u
#define CLI_X11_TARGETS 2u
#define CLI_X11_INCR 3u
#define CLI_X11_URIS 4u
#define CLI_X11_COPIED 5u
#define CLI_X11_PROPERTY 6u
#define CLI_X11_TEXT 7u
#define CLI_X11_NAMED 8u

static const char *const s_atom_names[CLI_X11_NAMED] = {
    "CLIPBOARD",       "UTF8_STRING", "TARGETS", "INCR", "text/uri-list", "x-special/gnome-copied-files",
    "ORIOR_CLIPBOARD", "TEXT",
};

// One connection to the X server: its socket, the window it reads the clipboard into or owns it with, the most bytes
// one request takes, and the atoms it interned
typedef struct
{
    int socket;
    unsigned int window;
    unsigned long long request_most;
    unsigned int atom[CLI_X11_NAMED];
} CliX11;

// `value`'s low 16 and 32 bits written least byte first, as the connection's order is; and read back
static void x11_put16(unsigned char *at, unsigned int value)
{
    at[0] = (unsigned char)(value & 0xffu);
    at[1] = (unsigned char)((value >> 8u) & 0xffu);
}

static void x11_put32(unsigned char *at, unsigned int value)
{
    x11_put16(at, value & 0xffffu);
    x11_put16(at + 2, value >> 16u);
}

static unsigned int x11_get16(const unsigned char *at)
{
    return (unsigned int)at[0] | ((unsigned int)at[1] << 8u);
}

static unsigned int x11_get32(const unsigned char *at)
{
    return x11_get16(at) | (x11_get16(at + 2) << 16u);
}

// `size` bytes written whole to the server: 1, or 0 where the connection is gone
static int x11_sent(const CliX11 *x11, const void *bytes, unsigned long long size)
{
    const unsigned char *at = (const unsigned char *)bytes;
    while (size > 0ull)
    {
        const ssize_t wrote = write(x11->socket, at, (size_t)size);
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
static int x11_received(const CliX11 *x11, void *bytes, unsigned long long size, int wait)
{
    unsigned char *at = (unsigned char *)bytes;
    while (size > 0ull)
    {
        struct pollfd ready = {x11->socket, POLLIN, 0};
        const int polled = poll(&ready, 1u, wait);
        if ((polled < 0) && (errno == EINTR))
        {
            continue;
        }
        const ssize_t got = (polled > 0) ? read(x11->socket, at, (size_t)size) : 0;
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

// `size` bytes the server sends passed over, read through `room`, sized for `room_size`: 1, or 0 where they did not
// come
static int x11_passed(const CliX11 *x11, unsigned long long size, unsigned char *room, unsigned long long room_size)
{
    while (size > 0ull)
    {
        const unsigned long long piece = (size < room_size) ? size : room_size;
        if (!x11_received(x11, room, piece, CLI_X11_WAIT))
        {
            return 0;
        }
        size -= piece;
    }
    return 1;
}

// the next thing the server sends, 32 bytes, read into `packet`; a reply's more bytes read into `extra`, sized for
// `room`, their count into `size`, the rest passed over: 1, or 0 where nothing came within `wait` milliseconds
static int x11_packet(const CliX11 *x11, unsigned char *packet, unsigned char *extra, unsigned long long room,
                      unsigned long long *size, int wait)
{
    *size = 0ull;
    if (!x11_received(x11, packet, 32ull, wait))
    {
        return 0;
    }
    if ((packet[0] & 0x7fu) != CLI_X11_REPLY)
    {
        return 1;
    }
    const unsigned long long more = (unsigned long long)x11_get32(packet + 4) * 4ull;
    *size = (more < room) ? more : room;
    unsigned char passed[64];
    return x11_received(x11, extra, *size, CLI_X11_WAIT) && x11_passed(x11, more - *size, passed, sizeof(passed));
}

// the reply to the last request sent read into `packet` and `extra`, events before it passed over: 1, or 0 where an
// error came in its place or nothing came in time
static int x11_reply(const CliX11 *x11, unsigned char *packet, unsigned char *extra, unsigned long long room,
                     unsigned long long *size)
{
    for (;;)
    {
        if (!x11_packet(x11, packet, extra, room, size, CLI_X11_WAIT))
        {
            return 0;
        }
        const unsigned int kind = packet[0] & 0x7fu;
        if ((kind == CLI_X11_REPLY) || (kind == CLI_X11_ERROR))
        {
            return kind == CLI_X11_REPLY;
        }
    }
}

// the display DISPLAY names: its host into `host`, sized for `room`, empty for this machine's own socket, and its
// number: 1, or 0 where DISPLAY names none
static int x11_display(char *host, size_t room, unsigned int *number)
{
    char display[256];
    if (!query_environment_read("DISPLAY", display, sizeof(display)))
    {
        return 0;
    }
    size_t colon = 0u;
    int found = 0;
    for (size_t at = 0u; display[at] != '\0'; at += 1u)
    {
        found = found || (display[at] == ':');
        colon = (display[at] == ':') ? at : colon;
    }
    if (!found)
    {
        return 0;
    }
    VerbaLinea named = {host, room, 0ull};
    mmgr_line_put_n(&named, display, colon);
    (void)mmgr_line_finish(&named);
    *number = 0u;
    for (size_t at = colon + 1u; (display[at] >= '0') && (display[at] <= '9'); at += 1u)
    {
        *number = (*number * 10u) + (unsigned int)(display[at] - '0');
    }
    return 1;
}

// a connection made to the display: this machine's socket for it where `host` is empty or `unix`, the abstract one on
// Linux where the file is not there, else the host's port 6000 and on: the socket, or -1
static int x11_socket(const char *host, unsigned int number)
{
    const int local = (host[0] == '\0') || cellul.eq(&(CatenaFinitaCfg){.src = host, .other = "unix", .cap = 8u});
    if (local)
    {
        for (unsigned int abstract = 0u; abstract < 2u; abstract += 1u)
        {
            struct sockaddr_un address;
            memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
            address.sun_family = AF_UNIX;
            VerbaLinea path = {address.sun_path + abstract, sizeof(address.sun_path) - abstract, 0ull};
            mmgr_line_put(&path, "/tmp/.X11-unix/X");
            mmgr_line_decimal(&path, number, 1u);
            const unsigned long long length = mmgr_line_finish(&path);
            const int made = socket(AF_UNIX, SOCK_STREAM, 0);
            // an address's length is far below what a socklen_t counts
            const socklen_t size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + abstract + length);
            if ((made >= 0) && (length != 0ull) && (connect(made, (const struct sockaddr *)&address, size) == 0))
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
    VerbaLinea port_line = {port, sizeof(port), 0ull};
    mmgr_line_decimal(&port_line, 6000u + number, 1u);
    (void)mmgr_line_finish(&port_line);
    struct addrinfo hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *found = NULL;
    if (getaddrinfo(host, port, &hints, &found) != 0)
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
static const unsigned char *x11_authority_field(const unsigned char *file, unsigned long long size,
                                                unsigned long long *at, unsigned int *length)
{
    if (*at + 2ull > size)
    {
        return NULL;
    }
    *length = ((unsigned int)file[*at] << 8u) | file[*at + 1ull];
    const unsigned char *const field = file + *at + 2ull;
    *at += 2ull + *length;
    return (*at <= size) ? field : NULL;
}

// whether `length` bytes of `field` are the text `text`
static int x11_field_is(const unsigned char *field, unsigned int length, const char *text)
{
    const size_t text_length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (length == text_length) &&
           ((length == 0u) || (memor.cmp(&(MemoriaCfg){.src = field, .other = text, .bytes = length}) == 0));
}

// the MIT-MAGIC-COOKIE-1 the reader's Xauthority file holds for display `number`, for this machine where `host` is
// empty, into `cookie`, 16 bytes: 1, or 0 where it holds none
static int x11_cookie(const char *host, unsigned int number, unsigned char *cookie)
{
    static unsigned char s_file[CLI_X11_AUTHORITY_MOST];
    char path[CLI_PATH_MAX_LEN];
    if (!query_environment_read("XAUTHORITY", path, sizeof(path)) || (path[0] == '\0'))
    {
        VerbaLinea home = {path, sizeof(path), 0ull};
        char folder[CLI_PATH_MAX_LEN];
        mmgr_line_put(&home, query_environment_read("HOME", folder, sizeof(folder)) ? folder : "");
        mmgr_line_put(&home, "/.Xauthority");
        (void)mmgr_line_finish(&home);
    }
    unsigned long long size = 0ull;
    if (!cli_file_read_at(path, 0ull, s_file, sizeof(s_file), &size))
    {
        return 0;
    }
    char own[256];
    own[0] = '\0';
    (void)gethostname(own, sizeof(own) - 1u);
    own[sizeof(own) - 1u] = '\0';
    char display[16];
    VerbaLinea display_line = {display, sizeof(display), 0ull};
    mmgr_line_decimal(&display_line, number, 1u);
    (void)mmgr_line_finish(&display_line);
    // the families of an address the file names: this machine by its name, and any machine
    static const unsigned int s_local = 256u;
    static const unsigned int s_wild = 65535u;
    for (unsigned long long at = 0ull; at + 2ull <= size;)
    {
        const unsigned int family = ((unsigned int)s_file[at] << 8u) | s_file[at + 1ull];
        at += 2ull;
        unsigned int lengths[4] = {0u, 0u, 0u, 0u};
        const unsigned char *fields[4] = {NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 4u; field += 1u)
        {
            fields[field] = x11_authority_field(s_file, size, &at, &lengths[field]);
            if (fields[field] == NULL)
            {
                return 0;
            }
        }
        const int machine = (family == s_wild) || (host[0] != '\0') ||
                            ((family == s_local) && x11_field_is(fields[0], lengths[0], own));
        const int shown = (lengths[1] == 0u) || x11_field_is(fields[1], lengths[1], display);
        if (machine && shown && x11_field_is(fields[2], lengths[2], "MIT-MAGIC-COOKIE-1") && (lengths[3] == 16u))
        {
            memor.cpy(&(MemoriaCfg){.dst = cookie, .src = fields[3], .bytes = 16u});
            return 1;
        }
    }
    return 0;
}

// the server's answer to the connection's setup read: the window's id made from the ids it gives, the root window and
// the most bytes one request takes: 1, or 0 where it refused the connection
static int x11_set_up(CliX11 *x11, unsigned int *root)
{
    unsigned char head[8];
    unsigned char fixed[32];
    if (!x11_received(x11, head, sizeof(head), CLI_X11_WAIT) || (head[0] != 1u))
    {
        return 0;
    }
    const unsigned long long length = (unsigned long long)x11_get16(head + 6) * 4ull;
    if ((length < 40ull) || !x11_received(x11, fixed, sizeof(fixed), CLI_X11_WAIT))
    {
        return 0;
    }
    const unsigned int base = x11_get32(fixed + 4);
    const unsigned int mask = x11_get32(fixed + 8);
    const unsigned long long vendor = ((unsigned long long)x11_get16(fixed + 16) + 3ull) & ~3ull;
    x11->request_most = (unsigned long long)x11_get16(fixed + 18) * 4ull;
    const unsigned long long formats = (unsigned long long)fixed[21] * 8ull;
    unsigned char room[256];
    unsigned char screen[4];
    if (!x11_passed(x11, vendor + formats, room, sizeof(room)) ||
        !x11_received(x11, screen, sizeof(screen), CLI_X11_WAIT))
    {
        return 0;
    }
    *root = x11_get32(screen);
    const unsigned long long read = 32ull + vendor + formats + 4ull;
    // the window's id is the first the server gives this client
    x11->window = base | (mask & (~mask + 1u));
    return (read <= length) && x11_passed(x11, length - read, room, sizeof(room));
}

// a connection made to the display, set up, a window of its own made that hears its own property changes, and the
// atoms this file names interned: 1, or 0 where the display could not be reached or refused it
static int x11_connected(CliX11 *x11)
{
    char host[256];
    unsigned int number = 0u;
    x11->socket = x11_display(host, sizeof(host), &number) ? x11_socket(host, number) : -1;
    if (x11->socket < 0)
    {
        return 0;
    }
    unsigned char cookie[16];
    const int authorized = x11_cookie(host, number, cookie);
    unsigned char setup[48];
    memor.set(&(MemoriaCfg){.dst = setup, .val = 0, .bytes = sizeof(setup)});
    setup[0] = 'l';
    x11_put16(setup + 2, 11u);
    x11_put16(setup + 6, authorized ? 18u : 0u);
    x11_put16(setup + 8, authorized ? 16u : 0u);
    if (authorized)
    {
        memor.cpy(&(MemoriaCfg){.dst = setup + 12, .src = "MIT-MAGIC-COOKIE-1", .bytes = 18u});
        memor.cpy(&(MemoriaCfg){.dst = setup + 32, .src = cookie, .bytes = 16u});
    }
    unsigned int root = 0u;
    int made = x11_sent(x11, setup, authorized ? 48ull : 12ull) && x11_set_up(x11, &root);
    unsigned char window[36];
    window[0] = (unsigned char)CLI_X11_CREATE_WINDOW;
    window[1] = 0u;
    x11_put16(window + 2, 9u);
    x11_put32(window + 4, x11->window);
    x11_put32(window + 8, root);
    x11_put32(window + 12, 0u);
    x11_put16(window + 16, 1u);
    x11_put16(window + 18, 1u);
    x11_put16(window + 20, 0u);
    x11_put16(window + 22, CLI_X11_INPUT_ONLY);
    x11_put32(window + 24, 0u);
    x11_put32(window + 28, CLI_X11_EVENT_MASK);
    x11_put32(window + 32, CLI_X11_PROPERTY_CHANGES);
    made = made && x11_sent(x11, window, sizeof(window));
    for (unsigned int at = 0u; made && (at < CLI_X11_NAMED); at += 1u)
    {
        unsigned char intern[8 + 32];
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = s_atom_names[at], .cap = 32u});
        const size_t padded = (length + 3u) & ~(size_t)3u;
        memor.set(&(MemoriaCfg){.dst = intern, .val = 0, .bytes = sizeof(intern)});
        intern[0] = (unsigned char)CLI_X11_INTERN_ATOM;
        // a request's length in words is far below what 16 bits count
        x11_put16(intern + 2, (unsigned int)(2u + (padded / 4u)));
        x11_put16(intern + 4, (unsigned int)length);
        memor.cpy(&(MemoriaCfg){.dst = intern + 8, .src = s_atom_names[at], .bytes = length});
        made = x11_sent(x11, intern, 8u + padded);
    }
    for (unsigned int at = 0u; made && (at < CLI_X11_NAMED); at += 1u)
    {
        unsigned char reply[32];
        unsigned long long size = 0ull;
        made = x11_reply(x11, reply, NULL, 0ull, &size);
        x11->atom[at] = made ? x11_get32(reply + 8) : 0u;
    }
    if (!made)
    {
        close(x11->socket);
        x11->socket = -1;
    }
    return made;
}

// the connection let go
static void x11_closed(CliX11 *x11)
{
    if (x11->socket >= 0)
    {
        close(x11->socket);
    }
    x11->socket = -1;
}

// `owner` made the owner of the clipboard, None where it is 0: 1, or 0 where the connection is gone
static int x11_owner_set(const CliX11 *x11, unsigned int owner)
{
    unsigned char request[16];
    request[0] = (unsigned char)CLI_X11_SET_OWNER;
    request[1] = 0u;
    x11_put16(request + 2, 4u);
    x11_put32(request + 4, owner);
    x11_put32(request + 8, x11->atom[CLI_X11_CLIPBOARD]);
    x11_put32(request + 12, 0u);
    return x11_sent(x11, request, sizeof(request));
}

// `size` bytes of `bytes` put as the property `property` of `window`, of `type` in units of `format` bits, replacing
// what it had: 1, or 0 where the connection is gone
static int x11_property_put(const CliX11 *x11, unsigned int window, unsigned int property, unsigned int type,
                            unsigned int format, const void *bytes, unsigned long long size)
{
    unsigned char request[24];
    const unsigned long long padded = (size + 3ull) & ~3ull;
    static const unsigned char s_pad[4] = {0u, 0u, 0u, 0u};
    request[0] = (unsigned char)CLI_X11_CHANGE_PROPERTY;
    request[1] = 0u;
    // a piece sent is below CLI_X11_SENT_MOST, and its request's words are below what 16 bits count
    x11_put16(request + 2, (unsigned int)(6ull + (padded / 4ull)));
    x11_put32(request + 4, window);
    x11_put32(request + 8, property);
    x11_put32(request + 12, type);
    request[16] = (unsigned char)format;
    request[17] = 0u;
    request[18] = 0u;
    request[19] = 0u;
    // the units of the bytes are below what 32 bits count, as a piece is
    x11_put32(request + 20, (unsigned int)(size / (format / 8u)));
    return x11_sent(x11, request, sizeof(request)) && x11_sent(x11, bytes, size) && x11_sent(x11, s_pad, padded - size);
}

// the property `property` of `window` deleted
static int x11_property_deleted(const CliX11 *x11, unsigned int window, unsigned int property)
{
    unsigned char request[12];
    request[0] = (unsigned char)CLI_X11_DELETE_PROPERTY;
    request[1] = 0u;
    x11_put16(request + 2, 3u);
    x11_put32(request + 4, window);
    x11_put32(request + 8, property);
    return x11_sent(x11, request, sizeof(request));
}

// the next event of `kind` for this connection's window, `property` where it is a property's change, waited for, every
// other event passed over: 1, or 0 where none came in time
static int x11_event(const CliX11 *x11, unsigned int kind, unsigned int property, unsigned char *packet)
{
    for (;;)
    {
        unsigned long long size = 0ull;
        if (!x11_packet(x11, packet, NULL, 0ull, &size, CLI_X11_WAIT))
        {
            return 0;
        }
        const unsigned int came = packet[0] & 0x7fu;
        const int ours = (came == CLI_X11_SELECTION_NOTIFY) && (x11_get32(packet + 8) == x11->window);
        const int changed = (came == CLI_X11_PROPERTY_NOTIFY) && (x11_get32(packet + 4) == x11->window) &&
                            (x11_get32(packet + 8) == property) && (packet[16] == 0u);
        if ((came == kind) && (ours || changed))
        {
            return 1;
        }
    }
}

// the property this connection reads the clipboard into read whole and appended to `out`, then deleted: its type into
// `type` and its bytes into `size`: 1, or 0 where it could not be read
static int x11_property_taken(const CliX11 *x11, VerbaLinea *out, unsigned int *type, unsigned long long *size)
{
    static unsigned char s_piece[CLI_X11_PIECE];
    *size = 0ull;
    for (unsigned long long offset = 0ull;;)
    {
        unsigned char request[24];
        request[0] = (unsigned char)CLI_X11_GET_PROPERTY;
        request[1] = 0u;
        x11_put16(request + 2, 6u);
        x11_put32(request + 4, x11->window);
        x11_put32(request + 8, x11->atom[CLI_X11_PROPERTY]);
        x11_put32(request + 12, 0u);
        // an offset in words is below what 32 bits count for any property a server holds
        x11_put32(request + 16, (unsigned int)offset);
        x11_put32(request + 20, CLI_X11_PIECE / 4u);
        unsigned char reply[32];
        unsigned long long got = 0ull;
        if (!x11_sent(x11, request, sizeof(request)) || !x11_reply(x11, reply, s_piece, sizeof(s_piece), &got))
        {
            return 0;
        }
        *type = x11_get32(reply + 8);
        const unsigned long long after = x11_get32(reply + 12);
        const unsigned long long bytes =
            (unsigned long long)x11_get32(reply + 16) * ((reply[1] != 0u) ? reply[1] / 8u : 1u);
        mmgr_line_put_n(out, (const char *)s_piece, (bytes < got) ? bytes : got);
        *size += (bytes < got) ? bytes : got;
        if (after == 0ull)
        {
            break;
        }
        offset += got / 4ull;
    }
    return x11_property_deleted(x11, x11->window, x11->atom[CLI_X11_PROPERTY]);
}

// the clipboard's content as `target` read into `out`, a content sent in pieces read piece by piece as INCR sends it:
// 1, or 0 where its owner has none as `target` or does not answer in time
static int x11_converted(const CliX11 *x11, unsigned int target, VerbaLinea *out)
{
    unsigned char request[24];
    request[0] = (unsigned char)CLI_X11_CONVERT;
    request[1] = 0u;
    x11_put16(request + 2, 6u);
    x11_put32(request + 4, x11->window);
    x11_put32(request + 8, x11->atom[CLI_X11_CLIPBOARD]);
    x11_put32(request + 12, target);
    x11_put32(request + 16, x11->atom[CLI_X11_PROPERTY]);
    x11_put32(request + 20, 0u);
    unsigned char packet[32];
    if (!x11_sent(x11, request, sizeof(request)) || !x11_event(x11, CLI_X11_SELECTION_NOTIFY, 0u, packet) ||
        (x11_get32(packet + 20) == 0u))
    {
        return 0;
    }
    unsigned int type = 0u;
    unsigned long long size = 0ull;
    const unsigned long long before = out->at;
    if (!x11_property_taken(x11, out, &type, &size))
    {
        return 0;
    }
    if (type != x11->atom[CLI_X11_INCR])
    {
        return 1;
    }
    // the INCR property's own bytes, the content's size, taken back off; its deletion asked for the first piece, and
    // each piece is read as it is put, the content's end a piece of none
    out->at = before;
    for (size = 1ull; size > 0ull;)
    {
        if (!x11_event(x11, CLI_X11_PROPERTY_NOTIFY, x11->atom[CLI_X11_PROPERTY], packet) ||
            !x11_property_taken(x11, out, &type, &size))
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
    char *content[3];
    unsigned long long size[3];
    unsigned int requestor;
    unsigned int property;
    unsigned int type;
    unsigned int piece_of;
    unsigned long long sent;
    int sending;
    CliThread thread;
} CliX11Owner;

// the contents by place: the text, the URIs and the copied files
#define CLI_X11_OWNED_TEXT 0u
#define CLI_X11_OWNED_URIS 1u
#define CLI_X11_OWNED_COPIED 2u

static CliX11Owner s_owner;
static CliLock s_lock = CLI_LOCK_INIT;
static CliLock s_read_lock = CLI_LOCK_INIT;
static CliWake s_started = CLI_WAKE_INIT;

// the place of the content `target` asks for, or 3 where the clipboard holds none as it; under the lock
static unsigned int owner_content_of(unsigned int target)
{
    const unsigned int *const atom = s_owner.x11.atom;
    const int text =
        (target == atom[CLI_X11_UTF8]) || (target == atom[CLI_X11_TEXT]) || (target == CLI_X11_ATOM_STRING);
    const unsigned int place = text                               ? CLI_X11_OWNED_TEXT
                               : (target == atom[CLI_X11_URIS])   ? CLI_X11_OWNED_URIS
                               : (target == atom[CLI_X11_COPIED]) ? CLI_X11_OWNED_COPIED
                                                                  : 3u;
    return ((place < 3u) && (s_owner.content[place] != NULL)) ? place : 3u;
}

// the program that asked told what it was given: the property it was put in, or none where it was refused
static void owner_notified(const unsigned char *request, unsigned int property)
{
    unsigned char event[44];
    memor.set(&(MemoriaCfg){.dst = event, .val = 0, .bytes = sizeof(event)});
    event[0] = (unsigned char)CLI_X11_SEND_EVENT;
    x11_put16(event + 2, 11u);
    x11_put32(event + 4, x11_get32(request + 12));
    event[12] = (unsigned char)CLI_X11_SELECTION_NOTIFY;
    x11_put32(event + 16, x11_get32(request + 4));
    x11_put32(event + 20, x11_get32(request + 12));
    x11_put32(event + 24, x11_get32(request + 16));
    x11_put32(event + 28, x11_get32(request + 20));
    x11_put32(event + 32, property);
    (void)x11_sent(&s_owner.x11, event, sizeof(event));
}

// a program's request for the clipboard answered: the targets it holds, or its content as the target asked, put on the
// requestor's property whole or, past one request's room, begun in pieces as INCR sends them; under the lock
static void owner_requested(const unsigned char *request)
{
    const CliX11 *const x11 = &s_owner.x11;
    const unsigned int requestor = x11_get32(request + 12);
    const unsigned int target = x11_get32(request + 20);
    // an old program names no property, and is given its content in the property its target names
    const unsigned int property = (x11_get32(request + 24) != 0u) ? x11_get32(request + 24) : target;
    if (target == x11->atom[CLI_X11_TARGETS])
    {
        unsigned char targets[6 * 4];
        unsigned int count = 0u;
        const unsigned int named[6] = {x11->atom[CLI_X11_TARGETS], x11->atom[CLI_X11_UTF8], CLI_X11_ATOM_STRING,
                                       x11->atom[CLI_X11_TEXT],    x11->atom[CLI_X11_URIS], x11->atom[CLI_X11_COPIED]};
        for (unsigned int at = 0u; at < 6u; at += 1u)
        {
            if ((at == 0u) || (owner_content_of(named[at]) < 3u))
            {
                x11_put32(targets + (count * 4u), named[at]);
                count += 1u;
            }
        }
        (void)x11_property_put(x11, requestor, property, CLI_X11_ATOM_ATOM, 32u, targets, count * 4ull);
        owner_notified(request, property);
        return;
    }
    const unsigned int place = owner_content_of(target);
    if ((place == 3u) || (s_owner.sending && (place < 3u)))
    {
        owner_notified(request, 0u);
        return;
    }
    const unsigned int type = (target == x11->atom[CLI_X11_TEXT]) ? x11->atom[CLI_X11_UTF8] : target;
    const unsigned long long most = (x11->request_most > 64ull) ? x11->request_most - 64ull : 0ull;
    const unsigned long long whole = (most < CLI_X11_SENT_MOST) ? most : CLI_X11_SENT_MOST;
    if (s_owner.size[place] <= whole)
    {
        (void)x11_property_put(x11, requestor, property, type, 8u, s_owner.content[place], s_owner.size[place]);
        owner_notified(request, property);
        return;
    }
    // the requestor's property changes heard, each deletion asking for the next piece
    unsigned char hear[16];
    hear[0] = (unsigned char)CLI_X11_CHANGE_WINDOW;
    hear[1] = 0u;
    x11_put16(hear + 2, 4u);
    x11_put32(hear + 4, requestor);
    x11_put32(hear + 8, CLI_X11_EVENT_MASK);
    x11_put32(hear + 12, CLI_X11_PROPERTY_CHANGES);
    unsigned char size[4];
    // a content's size is put as INCR's lower bound, cut to what 32 bits count
    x11_put32(size, (s_owner.size[place] < 0xffffffffull) ? (unsigned int)s_owner.size[place] : 0xffffffffu);
    (void)x11_sent(x11, hear, sizeof(hear));
    (void)x11_property_put(x11, requestor, property, x11->atom[CLI_X11_INCR], 32u, size, sizeof(size));
    owner_notified(request, property);
    s_owner.requestor = requestor;
    s_owner.property = property;
    s_owner.type = type;
    s_owner.piece_of = place;
    s_owner.sent = 0ull;
    s_owner.sending = 1;
}

// the next piece of a content sent in pieces put, as the requestor deleted the last: the end a piece of none; under the
// lock
static void owner_piece_sent(const unsigned char *event)
{
    const int deleted =
        (event[16] == 1u) && (x11_get32(event + 4) == s_owner.requestor) && (x11_get32(event + 8) == s_owner.property);
    if (!s_owner.sending || !deleted)
    {
        return;
    }
    const char *const content = s_owner.content[s_owner.piece_of];
    const unsigned long long left = (content != NULL) ? s_owner.size[s_owner.piece_of] - s_owner.sent : 0ull;
    const unsigned long long piece = (left < CLI_X11_SENT_MOST) ? left : CLI_X11_SENT_MOST;
    (void)x11_property_put(&s_owner.x11, s_owner.requestor, s_owner.property, s_owner.type, 8u,
                           (content != NULL) ? content + s_owner.sent : "", piece);
    s_owner.sent += piece;
    s_owner.sending = piece > 0ull;
}

// every content the clipboard served let go; under the lock
static void owner_contents_given(void)
{
    for (unsigned int place = 0u; place < 3u; place += 1u)
    {
        g_cli_cells->persistent_buf_release(s_owner.content[place], &g_cli_platform_cells);
        s_owner.content[place] = NULL;
        s_owner.size[place] = 0ull;
    }
    s_owner.sending = 0;
}

// the clipboard owned and served until another program takes it or it is emptied
static void owner_run(void *context)
{
    (void)context;
    static unsigned char s_reply[CLI_X11_PIECE];
    CliX11 *const x11 = &s_owner.x11;
    cli_lock_take(&s_lock);
    s_owner.owning = x11_connected(x11) && x11_owner_set(x11, x11->window);
    s_owner.starting = 0;
    cli_wake_all(&s_started);
    cli_lock_give(&s_lock);
    for (int serving = s_owner.owning; serving;)
    {
        unsigned char packet[32];
        unsigned long long size = 0ull;
        serving = x11_packet(x11, packet, s_reply, sizeof(s_reply), &size, -1);
        const unsigned int kind = packet[0] & 0x7fu;
        cli_lock_take(&s_lock);
        if (serving && (kind == CLI_X11_SELECTION_CLEAR))
        {
            // a clear that came before this window took the clipboard back is past
            serving = s_owner.reowned;
            s_owner.reowned = 0;
        }
        else if (serving && (kind == CLI_X11_SELECTION_REQUEST))
        {
            owner_requested(packet);
        }
        else if (serving && (kind == CLI_X11_PROPERTY_NOTIFY))
        {
            owner_piece_sent(packet);
        }
        cli_lock_give(&s_lock);
    }
    cli_lock_take(&s_lock);
    s_owner.owning = 0;
    owner_contents_given();
    x11_closed(x11);
    cli_lock_give(&s_lock);
}

// `size` bytes of `bytes` taken into a cell of their own as the content at `place`: 1, or 0 where no cell could be
// taken; under the lock
static int owner_content_put(unsigned int place, const char *bytes, unsigned long long size)
{
    char *const cell = g_cli_cells->persistent_buf_alloc(size + 1ull, &g_cli_platform_cells);
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = bytes, .bytes = size});
    cell[size] = '\0';
    s_owner.content[place] = cell;
    s_owner.size[place] = size;
    return 1;
}

// the clipboard owned with the contents put since the last were let go, its thread started where it does not run:
// 1, or 0 where the display could not be reached; under the lock
static int owner_owned(void)
{
    if (s_owner.owning)
    {
        s_owner.reowned = 1;
        return x11_owner_set(&s_owner.x11, s_owner.x11.window);
    }
    s_owner.starting = 1;
    s_owner.thread = (CliThread){owner_run, NULL, CLI_PRIORITY_WORK};
    if (!cli_thread_started(&s_owner.thread))
    {
        s_owner.starting = 0;
        return 0;
    }
    while (s_owner.starting)
    {
        cli_wake_wait(&s_started, &s_lock);
    }
    return s_owner.owning;
}

int cli_clipboard_text_read(VerbaLinea *text)
{
    cli_lock_take(&s_lock);
    const int own = s_owner.owning && (s_owner.content[CLI_X11_OWNED_TEXT] != NULL);
    if (own)
    {
        mmgr_line_put_n(text, s_owner.content[CLI_X11_OWNED_TEXT], s_owner.size[CLI_X11_OWNED_TEXT]);
    }
    cli_lock_give(&s_lock);
    if (own)
    {
        return 1;
    }
    cli_lock_take(&s_read_lock);
    CliX11 x11;
    const int read = x11_connected(&x11) && (x11_converted(&x11, x11.atom[CLI_X11_UTF8], text) ||
                                             x11_converted(&x11, CLI_X11_ATOM_STRING, text));
    x11_closed(&x11);
    cli_lock_give(&s_read_lock);
    return read;
}

int cli_clipboard_text_written(const char *text, unsigned long long size)
{
    cli_lock_take(&s_lock);
    owner_contents_given();
    const int owned = owner_content_put(CLI_X11_OWNED_TEXT, text, size) && owner_owned();
    cli_lock_give(&s_lock);
    return owned;
}

int cli_clipboard_files_put(const char *const *paths, unsigned int count, int cut)
{
    VerbaLinea uris;
    VerbaLinea copied;
    VerbaLinea text;
    if (!cli_growing_line_temporary(&uris, 4096u, &g_cli_platform_cells) ||
        !cli_growing_line_temporary(&copied, 4096u, &g_cli_platform_cells) ||
        !cli_growing_line_temporary(&text, 4096u, &g_cli_platform_cells))
    {
        return 0;
    }
    mmgr_line_put(&copied, cut ? "cut" : "copy");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        clipboard_uri_written(&uris, paths[at]);
        mmgr_line_put(&uris, "\r\n");
        mmgr_line_ch(&copied, '\n');
        clipboard_uri_written(&copied, paths[at]);
        mmgr_line_put(&text, (at > 0u) ? "\n" : "");
        mmgr_line_put(&text, paths[at]);
    }
    cli_lock_take(&s_lock);
    owner_contents_given();
    const int owned = mmgr_line_ok(&uris) && mmgr_line_ok(&copied) && mmgr_line_ok(&text) &&
                      owner_content_put(CLI_X11_OWNED_URIS, uris.out, uris.at) &&
                      owner_content_put(CLI_X11_OWNED_COPIED, copied.out, copied.at) &&
                      owner_content_put(CLI_X11_OWNED_TEXT, text.out, text.at) && owner_owned();
    cli_lock_give(&s_lock);
    g_cli_cells->persistent_buf_release(uris.out, &g_cli_platform_cells);
    g_cli_cells->persistent_buf_release(copied.out, &g_cli_platform_cells);
    g_cli_cells->persistent_buf_release(text.out, &g_cli_platform_cells);
    return owned;
}

// the files GNOME's copied-files target holds, `size` bytes of `list`, its first line `cut` or `copy`, handed to
// `each`, 1 in `cut` where they were cut: how many
static unsigned int clipboard_copied_read(const char *list, unsigned long long size,
                                          void (*each)(void *context, const char *path), void *context, int *cut)
{
    unsigned long long first = 0ull;
    while ((first < size) && (list[first] != '\n'))
    {
        first += 1ull;
    }
    *cut = (first == 3ull) && (list[0] == 'c') && (list[1] == 'u') && (list[2] == 't');
    return (first < size) ? clipboard_uris_read(list + first + 1ull, size - first - 1ull, each, context) : 0u;
}

unsigned int cli_clipboard_files_held(void (*each)(void *context, const char *path), void *context, int *cut)
{
    *cut = -1;
    unsigned int count = 0u;
    cli_lock_take(&s_lock);
    // a clipboard this window owns holds the files it put, or none where it put text
    const int own = s_owner.owning;
    if (own && (s_owner.content[CLI_X11_OWNED_COPIED] != NULL))
    {
        count = clipboard_copied_read(s_owner.content[CLI_X11_OWNED_COPIED], s_owner.size[CLI_X11_OWNED_COPIED], each,
                                      context, cut);
    }
    cli_lock_give(&s_lock);
    if (own)
    {
        return count;
    }
    VerbaLinea list;
    if (!cli_growing_line_temporary(&list, 4096u, &g_cli_platform_cells))
    {
        return 0u;
    }
    cli_lock_take(&s_read_lock);
    CliX11 x11;
    if (x11_connected(&x11))
    {
        if (x11_converted(&x11, x11.atom[CLI_X11_COPIED], &list) && mmgr_line_ok(&list))
        {
            count = clipboard_copied_read(list.out, list.at, each, context, cut);
        }
        else
        {
            list.at = 0ull;
            count = (x11_converted(&x11, x11.atom[CLI_X11_URIS], &list) && mmgr_line_ok(&list))
                        ? clipboard_uris_read(list.out, list.at, each, context)
                        : 0u;
        }
    }
    x11_closed(&x11);
    cli_lock_give(&s_read_lock);
    g_cli_cells->persistent_buf_release(list.out, &g_cli_platform_cells);
    return count;
}

void cli_clipboard_emptied(void)
{
    cli_lock_take(&s_lock);
    const int own = s_owner.owning;
    if (own)
    {
        // the server tells the owner its clipboard is cleared, which ends its thread
        s_owner.reowned = 0;
        (void)x11_owner_set(&s_owner.x11, 0u);
    }
    cli_lock_give(&s_lock);
    if (!own)
    {
        CliX11 x11;
        if (x11_connected(&x11))
        {
            (void)x11_owner_set(&x11, 0u);
        }
        x11_closed(&x11);
    }
}
#endif
#endif
