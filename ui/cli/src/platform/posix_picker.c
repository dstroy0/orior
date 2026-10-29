// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_picker.c: the system's picker on Linux and macOS. On Linux it is the desktop portal's FileChooser, asked over
// the session's D-Bus in its own wire protocol: a file to open, a folder, or a file to save, the portal's answer heard
// as the Response signal of the request it names. On macOS it is the system's own chooser, through osascript
#if !defined(_WIN32)
// sockets, poll and getuid are POSIX, outside strict C11
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if defined(__APPLE__)
int cli_file_picked(unsigned int kind, char *path, size_t room)
{
    static const char *const s_scripts[3] = {"POSIX path of (choose file)", "POSIX path of (choose folder)",
                                             "POSIX path of (choose file name)"};
    const char *const words[4] = {"osascript", "-e", s_scripts[(kind < 3u) ? kind : 0u], NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    path[0] = '\0';
    CliRan *const ran = cli_ran_taken();
    // a reader who cancels ends osascript with an error
    int chosen = cli_ran(words, NULL, NULL, 0u, ran, &err_msg) && (ran->code == 0ll) && (ran->out_size > 0ull);
    unsigned long long end = chosen ? ran->out_size : 0ull;
    while ((end > 0ull) && ((ran->out[end - 1ull] == '\n') || (ran->out[end - 1ull] == '\r')))
    {
        end -= 1ull;
    }
    // a folder's path is given with a `/` after it, which a path of the window's does not have
    end -= ((end > 1ull) && (ran->out[end - 1ull] == '/')) ? 1ull : 0ull;
    VerbaLinea chose = {path, room, 0ull};
    mmgr_line_put_n(&chose, ran->out, end);
    chosen = chosen && (end > 0ull) && (mmgr_line_finish(&chose) != 0ull);
    cli_ran_given(ran);
    return chosen;
}
#else
// the kinds of a D-Bus message, the fields of its header this file writes and reads, and its version
#define CLI_DBUS_CALL 1u
#define CLI_DBUS_RETURN 2u
#define CLI_DBUS_ERROR 3u
#define CLI_DBUS_SIGNAL 4u
#define CLI_DBUS_PATH 1u
#define CLI_DBUS_INTERFACE 2u
#define CLI_DBUS_MEMBER 3u
#define CLI_DBUS_REPLY_SERIAL 5u
#define CLI_DBUS_DESTINATION 6u
#define CLI_DBUS_SIGNATURE 8u

// the most bytes of a message written or read
#define CLI_DBUS_MESSAGE_MOST (64u << 10u)

// how deep a value's types nest that this file passes over
#define CLI_DBUS_DEPTH 32u

// A message being written: its bytes, how many, and whether every one fit
typedef struct
{
    unsigned char *bytes;
    unsigned long long at;
    unsigned long long room;
    int fits;
} CliDbusWritten;

// `size` bytes written whole to the bus: 1, or 0 where the connection is gone
static int dbus_sent(int socket, const void *bytes, unsigned long long size)
{
    const unsigned char *at = (const unsigned char *)bytes;
    while (size > 0ull)
    {
        const ssize_t wrote = write(socket, at, (size_t)size);
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

// `size` bytes read whole from the bus, waited for as long as it takes: 1, or 0 where the connection is gone
static int dbus_received(int socket, void *bytes, unsigned long long size)
{
    unsigned char *at = (unsigned char *)bytes;
    while (size > 0ull)
    {
        const ssize_t got = read(socket, at, (size_t)size);
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

// the message padded with zeros to a multiple of `alignment` bytes from its start
static void dbus_aligned(CliDbusWritten *message, unsigned long long alignment)
{
    while (message->fits && ((message->at % alignment) != 0ull))
    {
        message->fits = message->at < message->room;
        message->bytes[message->fits ? message->at : 0ull] = 0u;
        message->at += message->fits ? 1ull : 0ull;
    }
}

// `size` bytes put at the message's end
static void dbus_put(CliDbusWritten *message, const void *bytes, unsigned long long size)
{
    message->fits = message->fits && (message->at + size <= message->room);
    if (message->fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = message->bytes + message->at, .src = bytes, .bytes = size});
        message->at += size;
    }
}

// a 32-bit number put, aligned, least byte first, as the message's order is
static void dbus_u32(CliDbusWritten *message, unsigned int value)
{
    const unsigned char bytes[4] = {(unsigned char)(value & 0xffu), (unsigned char)((value >> 8u) & 0xffu),
                                    (unsigned char)((value >> 16u) & 0xffu), (unsigned char)(value >> 24u)};
    dbus_aligned(message, 4ull);
    dbus_put(message, bytes, sizeof(bytes));
}

// a 32-bit number written back at `at`, as an array's length is once its items are written
static void dbus_u32_at(CliDbusWritten *message, unsigned long long at, unsigned int value)
{
    if (message->fits)
    {
        message->bytes[at] = (unsigned char)(value & 0xffu);
        message->bytes[at + 1ull] = (unsigned char)((value >> 8u) & 0xffu);
        message->bytes[at + 2ull] = (unsigned char)((value >> 16u) & 0xffu);
        message->bytes[at + 3ull] = (unsigned char)(value >> 24u);
    }
}

// a string or an object path put: its length, its bytes, and a 0
static void dbus_string(CliDbusWritten *message, const char *text)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_DBUS_MESSAGE_MOST});
    // a string a message holds is below its room, which 32 bits count
    dbus_u32(message, (unsigned int)length);
    dbus_put(message, text, length + 1u);
}

// a signature put: its length in one byte, its letters, and a 0
static void dbus_signature(CliDbusWritten *message, const char *signature)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = signature, .cap = 255u});
    // a signature's length is below 256
    const unsigned char counted = (unsigned char)length;
    dbus_put(message, &counted, 1ull);
    dbus_put(message, signature, length + 1u);
}

// a field of the header put: its code, and its value as a variant of `type`, a string's or a signature's
static void dbus_field(CliDbusWritten *message, unsigned char code, char type, const char *value)
{
    const char signature[2] = {type, '\0'};
    dbus_aligned(message, 8ull);
    dbus_put(message, &code, 1ull);
    dbus_signature(message, signature);
    if (type == 'g')
    {
        dbus_signature(message, value);
    }
    else
    {
        dbus_string(message, value);
    }
}

// a call's head begun in `message`, over `room` bytes of `bytes`: its serial, the object, interface and member it calls
// at `destination`, and its body's signature, empty for none; the place its body's length is written at once known
// into `body_length_at`
static void dbus_call_begun(CliDbusWritten *message, unsigned char *bytes, unsigned long long room, unsigned int serial,
                            const char *destination, const char *object, const char *interface, const char *member,
                            const char *signature, unsigned long long *body_length_at)
{
    *message = (CliDbusWritten){bytes, 0ull, room, 1};
    static const unsigned char s_head[4] = {'l', CLI_DBUS_CALL, 0u, 1u};
    dbus_put(message, s_head, sizeof(s_head));
    *body_length_at = message->at;
    dbus_u32(message, 0u);
    dbus_u32(message, serial);
    const unsigned long long fields_at = message->at;
    dbus_u32(message, 0u);
    const unsigned long long fields_start = message->at + 4ull;
    dbus_field(message, CLI_DBUS_PATH, 'o', object);
    dbus_field(message, CLI_DBUS_INTERFACE, 's', interface);
    dbus_field(message, CLI_DBUS_MEMBER, 's', member);
    dbus_field(message, CLI_DBUS_DESTINATION, 's', destination);
    if (signature[0] != '\0')
    {
        dbus_field(message, CLI_DBUS_SIGNATURE, 'g', signature);
    }
    // the fields' array is counted from where its items begin, aligned to 8 after the count
    dbus_u32_at(message, fields_at, (unsigned int)(message->at - ((fields_start + 7ull) & ~7ull)));
    dbus_aligned(message, 8ull);
}

// the call's body ended: its length written into its head, the head being `head` bytes
static void dbus_call_ended(CliDbusWritten *message, unsigned long long body_length_at, unsigned long long head)
{
    // a body a message holds is below its room, which 32 bits count
    dbus_u32_at(message, body_length_at, (unsigned int)(message->at - head));
}

// A message read: its kind, the serial it answers, its object path, its member and its body
typedef struct
{
    unsigned int kind;
    unsigned int reply_serial;
    char path[256];
    char member[64];
    const unsigned char *body;
    unsigned long long body_size;
} CliDbusMessage;

// a 32-bit number of a message read at `at`, least byte first
static unsigned int dbus_u32_of(const unsigned char *bytes, unsigned long long at)
{
    return (unsigned int)bytes[at] | ((unsigned int)bytes[at + 1ull] << 8u) | ((unsigned int)bytes[at + 2ull] << 16u) |
           ((unsigned int)bytes[at + 3ull] << 24u);
}

// the bytes a value of the type `code` aligns to
static unsigned long long dbus_alignment_of(char code)
{
    return ((code == 'y') || (code == 'g') || (code == 'v'))                                     ? 1ull
           : ((code == 'n') || (code == 'q'))                                                    ? 2ull
           : ((code == 'x') || (code == 't') || (code == 'd') || (code == '(') || (code == '{')) ? 8ull
                                                                                                 : 4ull;
}

// the one complete type at `*type` in a signature passed over, its end into `*type`: 1, or 0 where it does not close
static int dbus_type_passed(const char **type)
{
    const char opened = **type;
    *type += 1;
    if (opened == 'a')
    {
        return dbus_type_passed(type);
    }
    if ((opened != '(') && (opened != '{'))
    {
        return opened != '\0';
    }
    const char closing = (opened == '(') ? ')' : '}';
    while ((**type != closing) && (**type != '\0'))
    {
        if (!dbus_type_passed(type))
        {
            return 0;
        }
    }
    *type += (**type == closing) ? 1 : 0;
    return 1;
}

// a value of the one complete type at `*type` passed over in `size` bytes of `bytes` from `*at`, `depth` deep: the
// type's end into `*type` and the value's into `*at`: 1, or 0 where the value does not fit or nests too deep
static int dbus_value_passed(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                             const char **type, unsigned int depth)
{
    const char code = **type;
    *at = (*at + dbus_alignment_of(code) - 1ull) & ~(dbus_alignment_of(code) - 1ull);
    if ((depth > CLI_DBUS_DEPTH) || (*at > size))
    {
        return 0;
    }
    if ((code == 's') || (code == 'o') || (code == 'g'))
    {
        const unsigned long long length = (code == 'g') ? ((*at < size) ? bytes[*at] : 0ull)
                                                        : ((*at + 4ull <= size) ? dbus_u32_of(bytes, *at) : 0ull);
        *at += ((code == 'g') ? 1ull : 4ull) + length + 1ull;
        *type += 1;
        return *at <= size;
    }
    if (code == 'a')
    {
        const unsigned long long length = (*at + 4ull <= size) ? dbus_u32_of(bytes, *at) : 0ull;
        *at += 4ull;
        *type += 1;
        const char item = **type;
        *at = (*at + dbus_alignment_of(item) - 1ull) & ~(dbus_alignment_of(item) - 1ull);
        *at += length;
        return dbus_type_passed(type) && (*at <= size);
    }
    if (code == 'v')
    {
        if (*at >= size)
        {
            return 0;
        }
        const char *inner = (const char *)bytes + *at + 1ull;
        *at += 1ull + bytes[*at] + 1ull;
        *type += 1;
        return (*at <= size) && dbus_value_passed(bytes, size, at, &inner, depth + 1u);
    }
    if ((code == '(') || (code == '{'))
    {
        const char closing = (code == '(') ? ')' : '}';
        *type += 1;
        while ((**type != closing) && (**type != '\0'))
        {
            if (!dbus_value_passed(bytes, size, at, type, depth + 1u))
            {
                return 0;
            }
        }
        *type += (**type == closing) ? 1 : 0;
        return 1;
    }
    // a basic value takes as many bytes as it aligns to
    const int basic = (code == 'y') || (code == 'b') || (code == 'h') || (code == 'i') || (code == 'u') ||
                      (code == 'n') || (code == 'q') || (code == 'x') || (code == 't') || (code == 'd');
    *at += basic ? dbus_alignment_of(code) : 0ull;
    *type += 1;
    return basic && (*at <= size);
}

// the next message the bus sends read into `room` bytes of `bytes`, its head's fields this file reads into `message`:
// 1, or 0 where the connection is gone or the message does not fit
static int dbus_message_read(int socket, unsigned char *bytes, unsigned long long room, CliDbusMessage *message)
{
    if (!dbus_received(socket, bytes, 16ull) || (bytes[0] != 'l'))
    {
        return 0;
    }
    const unsigned long long body = dbus_u32_of(bytes, 4ull);
    const unsigned long long fields = dbus_u32_of(bytes, 12ull);
    const unsigned long long head = (16ull + fields + 7ull) & ~7ull;
    if ((head + body > room) || !dbus_received(socket, bytes + 16, head + body - 16ull))
    {
        return 0;
    }
    *message = (CliDbusMessage){bytes[1], 0u, "", "", bytes + head, body};
    for (unsigned long long at = 16ull; at + 4ull < 16ull + fields;)
    {
        at = (at + 7ull) & ~7ull;
        const unsigned char code = bytes[at];
        const char *type = (const char *)bytes + at + 2ull;
        const unsigned long long value = at + 2ull + bytes[at + 1ull] + 1ull;
        const unsigned long long string_at = (value + 3ull) & ~3ull;
        if ((code == CLI_DBUS_REPLY_SERIAL) && (type[0] == 'u'))
        {
            message->reply_serial = dbus_u32_of(bytes, string_at);
        }
        if (((code == CLI_DBUS_PATH) || (code == CLI_DBUS_MEMBER)) && ((type[0] == 'o') || (type[0] == 's')))
        {
            char *const into = (code == CLI_DBUS_PATH) ? message->path : message->member;
            const size_t into_room = (code == CLI_DBUS_PATH) ? sizeof(message->path) : sizeof(message->member);
            VerbaLinea copied = {into, into_room, 0ull};
            mmgr_line_put_n(&copied, (const char *)bytes + string_at + 4ull, dbus_u32_of(bytes, string_at));
            (void)mmgr_line_finish(&copied);
        }
        unsigned long long passed = value;
        if (!dbus_value_passed(bytes, 16ull + fields, &passed, &type, 0u))
        {
            return 0;
        }
        at = passed;
    }
    return 1;
}

// the reply to the call `serial` waited for, every other message passed over: 1 with it read into `message`, or 0
// where an error came in its place or the connection is gone
static int dbus_reply(int socket, unsigned char *bytes, unsigned long long room, unsigned int serial,
                      CliDbusMessage *message)
{
    for (;;)
    {
        if (!dbus_message_read(socket, bytes, room, message))
        {
            return 0;
        }
        if (((message->kind == CLI_DBUS_RETURN) || (message->kind == CLI_DBUS_ERROR)) &&
            (message->reply_serial == serial))
        {
            return message->kind == CLI_DBUS_RETURN;
        }
    }
}

// a string of a body read at `*at`, its bytes into `text`, sized for `room`, the place after it into `*at`: 1, or 0
// where it does not fit the body
static int dbus_body_string(const CliDbusMessage *message, unsigned long long *at, char *text, size_t room)
{
    *at = (*at + 3ull) & ~3ull;
    if (*at + 4ull > message->body_size)
    {
        return 0;
    }
    const unsigned long long length = dbus_u32_of(message->body, *at);
    if (*at + 4ull + length + 1ull > message->body_size)
    {
        return 0;
    }
    VerbaLinea copied = {text, room, 0ull};
    mmgr_line_put_n(&copied, (const char *)message->body + *at + 4ull, length);
    *at += 4ull + length + 1ull;
    return mmgr_line_finish(&copied) != 0ull;
}

// the session bus reached: the address DBUS_SESSION_BUS_ADDRESS names, a socket's path or an abstract name, else the
// bus in the reader's runtime folder; the reader known to it by their own id and the connection begun: the socket, or
// -1
static int dbus_connected(void)
{
    char address[512];
    char path[256];
    int abstract = 0;
    VerbaLinea named = {path, sizeof(path), 0ull};
    if (query_environment_read("DBUS_SESSION_BUS_ADDRESS", address, sizeof(address)))
    {
        const char *const at_path =
            cellul.find(&(CatenaFinitaCfg){.src = address, .cap = sizeof(address), .other = "path=", .other_cap = 8u});
        const char *const at_abstract = cellul.find(
            &(CatenaFinitaCfg){.src = address, .cap = sizeof(address), .other = "abstract=", .other_cap = 16u});
        const char *const from = (at_path != NULL) ? at_path + 5 : ((at_abstract != NULL) ? at_abstract + 9 : NULL);
        abstract = (at_path == NULL) && (at_abstract != NULL);
        for (const char *at = from; (at != NULL) && (*at != '\0') && (*at != ',') && (*at != ';'); at += 1)
        {
            mmgr_line_ch(&named, *at);
        }
    }
    if (named.at == 0ull)
    {
        char folder[256];
        mmgr_line_put(&named, query_environment_read("XDG_RUNTIME_DIR", folder, sizeof(folder)) ? folder : "");
        mmgr_line_put(&named, "/bus");
    }
    const unsigned long long length = mmgr_line_finish(&named);
    struct sockaddr_un socket_address;
    memor.set(&(MemoriaCfg){.dst = &socket_address, .val = 0, .bytes = sizeof(socket_address)});
    socket_address.sun_family = AF_UNIX;
    const unsigned long long offset = abstract ? 1ull : 0ull;
    if ((length == 0ull) || (length + offset >= sizeof(socket_address.sun_path)))
    {
        return -1;
    }
    memor.cpy(&(MemoriaCfg){.dst = socket_address.sun_path + offset, .src = path, .bytes = length});
    const int bus = socket(AF_UNIX, SOCK_STREAM, 0);
    // an address's length is far below what a socklen_t counts
    const socklen_t size = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + offset + length);
    if ((bus < 0) || (connect(bus, (const struct sockaddr *)&socket_address, size) != 0))
    {
        if (bus >= 0)
        {
            close(bus);
        }
        return -1;
    }
    (void)fcntl(bus, F_SETFD, FD_CLOEXEC);
    // the reader's id, its decimal digits each written as two hexadecimal ones, as EXTERNAL authentication names it
    char id[24];
    VerbaLinea id_line = {id, sizeof(id), 0ull};
    mmgr_line_decimal(&id_line, getuid(), 1u);
    (void)mmgr_line_finish(&id_line);
    char line[128];
    VerbaLinea auth = {line, sizeof(line), 0ull};
    mmgr_line_ch(&auth, '\0');
    mmgr_line_put(&auth, "AUTH EXTERNAL ");
    for (const char *digit = id; *digit != '\0'; digit += 1)
    {
        // a digit's byte is below 256
        mmgr_line_hex(&auth, (unsigned char)*digit, 2u);
    }
    mmgr_line_put(&auth, "\r\n");
    char answer[256];
    unsigned long long got = 0ull;
    int authorized = mmgr_line_ok(&auth) && dbus_sent(bus, line, auth.at);
    while (authorized && (got + 1ull < sizeof(answer)) && ((got < 2ull) || (answer[got - 1ull] != '\n')))
    {
        authorized = dbus_received(bus, answer + got, 1ull);
        got += 1ull;
    }
    authorized =
        authorized && (got >= 2ull) && (answer[0] == 'O') && (answer[1] == 'K') && dbus_sent(bus, "BEGIN\r\n", 7ull);
    if (!authorized)
    {
        close(bus);
        return -1;
    }
    return bus;
}

// the file URI `uri` made a path into `path`, sized for `room`, its host taken off and its escapes read: 1, or 0 where
// it is no file URI or does not fit
static int dbus_uri_path(const char *uri, char *path, size_t room)
{
    if (!cellul.starts(&(CatenaFinitaCfg){.src = uri, .other = "file://", .cap = CLI_PATH_MAX_LEN}))
    {
        return 0;
    }
    const char *at = cellul.chr(&(CatenaFinitaCfg){.src = uri + 7, .cap = CLI_PATH_MAX_LEN, .byte = '/'});
    VerbaLinea decoded = {path, room, 0ull};
    for (; (at != NULL) && (*at != '\0'); at += 1)
    {
        unsigned int high = 16u;
        unsigned int low = 16u;
        if (at[0] == '%')
        {
            high = ((at[1] >= '0') && (at[1] <= '9'))   ? (unsigned int)(at[1] - '0')
                   : ((at[1] >= 'A') && (at[1] <= 'F')) ? (unsigned int)(at[1] - 'A') + 10u
                   : ((at[1] >= 'a') && (at[1] <= 'f')) ? (unsigned int)(at[1] - 'a') + 10u
                                                        : 16u;
            low = ((at[2] >= '0') && (at[2] <= '9'))   ? (unsigned int)(at[2] - '0')
                  : ((at[2] >= 'A') && (at[2] <= 'F')) ? (unsigned int)(at[2] - 'A') + 10u
                  : ((at[2] >= 'a') && (at[2] <= 'f')) ? (unsigned int)(at[2] - 'a') + 10u
                                                       : 16u;
        }
        const int escaped = (high < 16u) && (low < 16u);
        // two hexadecimal digits make a byte, below 256
        mmgr_line_ch(&decoded, escaped ? (char)((high << 4u) | low) : at[0]);
        at += escaped ? 2 : 0;
    }
    return mmgr_line_finish(&decoded) != 0ull;
}

// the first file the Response signal's results name, `{"uris": [...]}`, made a path into `path`: 1, or 0 where the
// reader cancelled or no file is named
static int dbus_response_read(const CliDbusMessage *message, char *path, size_t room)
{
    if ((message->body_size < 8ull) || (dbus_u32_of(message->body, 0ull) != 0u))
    {
        return 0;
    }
    const unsigned long long length = dbus_u32_of(message->body, 4ull);
    const unsigned long long end = 8ull + length;
    for (unsigned long long at = 8ull; (at < end) && (end <= message->body_size);)
    {
        at = (at + 7ull) & ~7ull;
        char key[64];
        if (!dbus_body_string(message, &at, key, sizeof(key)) || (at >= message->body_size))
        {
            return 0;
        }
        const char *type = (const char *)message->body + at + 1ull;
        const int uris = cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "uris", .cap = sizeof(key)}) &&
                         (type[0] == 'a') && (type[1] == 's');
        at += 1ull + message->body[at] + 1ull;
        if (uris)
        {
            unsigned long long first = ((at + 3ull) & ~3ull) + 4ull;
            char uri[CLI_PATH_MAX_LEN * 3u];
            return (dbus_u32_of(message->body, (at + 3ull) & ~3ull) > 0u) &&
                   dbus_body_string(message, &first, uri, sizeof(uri)) && dbus_uri_path(uri, path, room);
        }
        if (!dbus_value_passed(message->body, message->body_size, &at, &type, 0u))
        {
            return 0;
        }
    }
    return 0;
}

int cli_file_picked(unsigned int kind, char *path, size_t room)
{
    static unsigned char s_message[CLI_DBUS_MESSAGE_MOST];
    static CliLock s_lock = CLI_LOCK_INIT;
    path[0] = '\0';
    cli_lock_take(&s_lock);
    const int bus = dbus_connected();
    CliDbusWritten message;
    CliDbusMessage read;
    unsigned long long body_at = 0ull;
    int chosen = bus >= 0;
    // the connection named to the bus
    dbus_call_begun(&message, s_message, sizeof(s_message), 1u, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                    "org.freedesktop.DBus", "Hello", "", &body_at);
    dbus_call_ended(&message, body_at, message.at);
    chosen = chosen && message.fits && dbus_sent(bus, s_message, message.at) &&
             dbus_reply(bus, s_message, sizeof(s_message), 1u, &read);
    // the portal's answers heard, every request's Response signal
    dbus_call_begun(&message, s_message, sizeof(s_message), 2u, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                    "org.freedesktop.DBus", "AddMatch", "s", &body_at);
    unsigned long long head = message.at;
    dbus_string(&message, "type='signal',interface='org.freedesktop.portal.Request',member='Response'");
    dbus_call_ended(&message, body_at, head);
    chosen = chosen && message.fits && dbus_sent(bus, s_message, message.at) &&
             dbus_reply(bus, s_message, sizeof(s_message), 2u, &read);
    // the chooser asked for: a file to open, a folder, or a file to save
    dbus_call_begun(&message, s_message, sizeof(s_message), 3u, "org.freedesktop.portal.Desktop",
                    "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser",
                    (kind == CLI_PICK_SAVE) ? "SaveFile" : "OpenFile", "ssa{sv}", &body_at);
    head = message.at;
    dbus_string(&message, "");
    dbus_string(&message, (kind == CLI_PICK_FOLDER) ? "Choose a folder" : ((kind == CLI_PICK_SAVE) ? "Save" : "Open"));
    const unsigned long long options_at = message.at;
    dbus_u32(&message, 0u);
    dbus_aligned(&message, 8ull);
    const unsigned long long options_start = message.at;
    dbus_string(&message, "modal");
    dbus_signature(&message, "b");
    dbus_u32(&message, 1u);
    if (kind == CLI_PICK_FOLDER)
    {
        dbus_aligned(&message, 8ull);
        dbus_string(&message, "directory");
        dbus_signature(&message, "b");
        dbus_u32(&message, 1u);
    }
    // the options' array is counted from where its entries begin
    dbus_u32_at(&message, options_at, (unsigned int)(message.at - options_start));
    dbus_call_ended(&message, body_at, head);
    char handle[256];
    unsigned long long handle_at = 0ull;
    chosen = chosen && message.fits && dbus_sent(bus, s_message, message.at) &&
             dbus_reply(bus, s_message, sizeof(s_message), 3u, &read) &&
             dbus_body_string(&read, &handle_at, handle, sizeof(handle));
    // the reader's answer waited for: the Response signal of the request the portal named
    while (chosen)
    {
        chosen = dbus_message_read(bus, s_message, sizeof(s_message), &read);
        const int answered = chosen && (read.kind == CLI_DBUS_SIGNAL) &&
                             cellul.eq(&(CatenaFinitaCfg){.src = read.member, .other = "Response", .cap = 64u}) &&
                             cellul.eq(&(CatenaFinitaCfg){.src = read.path, .other = handle, .cap = sizeof(handle)});
        if (answered)
        {
            chosen = dbus_response_read(&read, path, room);
            break;
        }
    }
    if (bus >= 0)
    {
        close(bus);
    }
    cli_lock_give(&s_lock);
    return chosen;
}
#endif
#endif
