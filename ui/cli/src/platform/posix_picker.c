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
#include "../text/text.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if (!defined(_WIN32)) && !((defined(__APPLE__)))
// A message being written: its bytes, how many, and whether every one fit
typedef struct
{
    unsigned char *bytes;
    unsigned long long at;
    unsigned long long capacity;
    int fits;
} CliDbusWritten;
#endif
#if (!defined(_WIN32)) && !((defined(__APPLE__)))
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
#endif
typedef struct
{
#if (!defined(_WIN32)) && !((defined(__APPLE__)))
    int socket;
    const void *bytes_bytess;
    unsigned long long size;
    void *bytes_bytess_other;
    CliDbusWritten *message;
    unsigned long long alignment;
    unsigned int value;
    unsigned long long at;
    const char *text;
    const char *signature;
    unsigned char code;
    char type_value;
    const char *value_text;
    unsigned char *bytes;
    unsigned long long capacity;
    unsigned int serial;
    const char *destination;
    const char *object;
    const char *interface;
    const char *member;
    unsigned long long *body_length_at;
    unsigned long long body_length_at_value;
    unsigned long long head;
    const unsigned char *bytes_read;
    char code_letter;
    const char **type;
    unsigned long long *at_pointer;
    unsigned int depth;
    CliDbusMessage *message_dbus_message_pointer;
    const CliDbusMessage *message_dbus_message_pointer_other;
    char *text_out;
    size_t capacity_bytes;
    const char *uri;
    char *path;
#endif
} PosixPickerCtx;

#if defined(__APPLE__)
int cli_file_pick(const CliPathCapacityCtx *args)
{
    static const char *const s_scripts[3] = {"POSIX path of (choose file)", "POSIX path of (choose folder)",
                                             "POSIX path of (choose file name)"};
    const char *const words[4] = {"osascript", "-e", s_scripts[(args->kind < 3u) ? args->kind : 0u], NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    args->path_out[0] = '\0';
    CliRunResult *const run_result = cli_run_result_alloc();
    // a reader who cancels ends osascript with an error
    int chosen = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                            .count = 0u, .run_result = run_result, .err_msg = &err_msg) &&
                 (run_result->code == 0ll) && (run_result->out_size > 0ull);
    unsigned long long end = chosen ? run_result->out_size : 0ull;
    end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = run_result->out, .at = end,
                     .set = "\n\r");
    // a folder's path is given with a `/` after it, which a path of the window's does not have
    end -= ((end > 1ull) && (run_result->out[end - 1ull] == '/')) ? 1ull : 0ull;
    VerbaLinea chose = {args->path_out, args->capacity_bytes, 0ull};
    mmgr_line_put_n(&chose, run_result->out, end);
    chosen = chosen && (end > 0ull) && (mmgr_line_finish(&chose) != 0ull);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
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
#define CLI_DBUS_MESSAGE_MAX (64u << 10u)

// how deep a value's types nest that this file passes over
#define CLI_DBUS_DEPTH 32u

// `size` bytes written whole to the bus: 1, or 0 where the connection is gone
EMBED_INLINE int dbus_send(const PosixPickerCtx *args)
{
    unsigned long long size = args->size;
    const unsigned char *at = (const unsigned char *)args->bytes_bytess;
    while (size > 0ull)
    {
        const ssize_t wrote = write(args->socket, at, (size_t)size);
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
EMBED_INLINE int dbus_receive(const PosixPickerCtx *args)
{
    unsigned long long size = args->size;
    unsigned char *at = (unsigned char *)args->bytes_bytess_other;
    while (size > 0ull)
    {
        const ssize_t got = read(args->socket, at, (size_t)size);
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
EMBED_INLINE void dbus_align(const PosixPickerCtx *args)
{
    while (args->message->fits && ((args->message->at % args->alignment) != 0ull))
    {
        args->message->fits = args->message->at < args->message->capacity;
        args->message->bytes[args->message->fits ? args->message->at : 0ull] = 0u;
        args->message->at += args->message->fits ? 1ull : 0ull;
    }
}

// `size` bytes put at the message's end
EMBED_INLINE void dbus_append(const PosixPickerCtx *args)
{
    const void *bytes = args->bytes_bytess;
    args->message->fits = args->message->fits && (args->message->at + args->size <= args->message->capacity);
    if (args->message->fits)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->message->bytes + args->message->at, .src = bytes, .bytes = args->size});
        args->message->at += args->size;
    }
}

// a 32-bit number put, aligned, least byte first, as the message's order is
EMBED_INLINE void dbus_u32(const PosixPickerCtx *args)
{
    const unsigned char bytes[4] = {(unsigned char)(args->value & 0xffu), (unsigned char)((args->value >> 8u) & 0xffu),
                                    (unsigned char)((args->value >> 16u) & 0xffu), (unsigned char)(args->value >> 24u)};
    EMBED_CALL(dbus_align, PosixPickerCtx, .message = args->message, .alignment = 4ull);
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = args->message, .bytes_bytess = bytes, .size = sizeof(bytes));
}

// a 32-bit number written back at `at`, as an array's length is once its items are written
EMBED_INLINE void dbus_u32_at(const PosixPickerCtx *args)
{
    if (args->message->fits)
    {
        args->message->bytes[args->at] = (unsigned char)(args->value & 0xffu);
        args->message->bytes[args->at + 1ull] = (unsigned char)((args->value >> 8u) & 0xffu);
        args->message->bytes[args->at + 2ull] = (unsigned char)((args->value >> 16u) & 0xffu);
        args->message->bytes[args->at + 3ull] = (unsigned char)(args->value >> 24u);
    }
}

// a string or an object path put: its length, its bytes, and a 0
EMBED_INLINE void dbus_string(const PosixPickerCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_DBUS_MESSAGE_MAX});
    // a string a message holds is below its capacity, which 32 bits count
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = args->message, .value = (unsigned int)length);
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = args->message, .bytes_bytess = args->text, .size = length + 1u);
}

// a signature put: its length in one byte, its letters, and a 0
EMBED_INLINE void dbus_signature(const PosixPickerCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->signature, .cap = 255u});
    // a signature's length is below 256
    const unsigned char counted = (unsigned char)length;
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = args->message, .bytes_bytess = &counted, .size = 1ull);
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = args->message, .bytes_bytess = args->signature,
               .size = length + 1u);
}

// a field of the header put: its code, and its value as a variant of `type`, a string's or a signature's
EMBED_INLINE void dbus_field(const PosixPickerCtx *args)
{
    unsigned char code = args->code;
    const char signature[2] = {args->type_value, '\0'};
    EMBED_CALL(dbus_align, PosixPickerCtx, .message = args->message, .alignment = 8ull);
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = args->message, .bytes_bytess = &code, .size = 1ull);
    EMBED_CALL(dbus_signature, PosixPickerCtx, .message = args->message, .signature = signature);
    if (args->type_value == 'g')
    {
        EMBED_CALL(dbus_signature, PosixPickerCtx, .message = args->message, .signature = args->value_text);
    }
    else
    {
        EMBED_CALL(dbus_string, PosixPickerCtx, .message = args->message, .text = args->value_text);
    }
}

// a call's head written in `message`, over `capacity` bytes of `bytes`: its serial, the object, interface and member it
// calls at `destination`, and its body's signature, empty for none; the place its body's length is written at once
// known into `body_length_at`
EMBED_INLINE void dbus_call_begin(const PosixPickerCtx *args)
{
    CliDbusWritten *message = args->message;
    unsigned long long *body_length_at = args->body_length_at;
    *message = (CliDbusWritten){args->bytes, 0ull, args->capacity, 1};
    static const unsigned char s_head[4] = {'l', CLI_DBUS_CALL, 0u, 1u};
    EMBED_CALL(dbus_append, PosixPickerCtx, .message = message, .bytes_bytess = s_head, .size = sizeof(s_head));
    *body_length_at = message->at;
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = message, .value = 0u);
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = message, .value = args->serial);
    const unsigned long long fields_at = message->at;
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = message, .value = 0u);
    const unsigned long long fields_start = message->at + 4ull;
    EMBED_CALL(dbus_field, PosixPickerCtx, .message = message, .code = CLI_DBUS_PATH, .type_value = 'o',
               .value_text = args->object);
    EMBED_CALL(dbus_field, PosixPickerCtx, .message = message, .code = CLI_DBUS_INTERFACE, .type_value = 's',
               .value_text = args->interface);
    EMBED_CALL(dbus_field, PosixPickerCtx, .message = message, .code = CLI_DBUS_MEMBER, .type_value = 's',
               .value_text = args->member);
    EMBED_CALL(dbus_field, PosixPickerCtx, .message = message, .code = CLI_DBUS_DESTINATION, .type_value = 's',
               .value_text = args->destination);
    if (args->signature[0] != '\0')
    {
        EMBED_CALL(dbus_field, PosixPickerCtx, .message = message, .code = CLI_DBUS_SIGNATURE, .type_value = 'g',
                   .value_text = args->signature);
    }
    // the fields' array is counted from where its items begin, aligned to 8 after the count
    EMBED_CALL(dbus_u32_at, PosixPickerCtx, .message = message, .at = fields_at,
               .value = (unsigned int)(message->at - ((fields_start + 7ull) & ~7ull)));
    EMBED_CALL(dbus_align, PosixPickerCtx, .message = message, .alignment = 8ull);
}

// the call's body ended: its length written into its head, the head being `head` bytes
EMBED_INLINE void dbus_call_end(const PosixPickerCtx *args)
{
    // a body a message holds is below its capacity, which 32 bits count
    EMBED_CALL(dbus_u32_at, PosixPickerCtx, .message = args->message, .at = args->body_length_at_value,
               .value = (unsigned int)(args->message->at - args->head));
}

// a 32-bit number of a message read at `at`, least byte first
EMBED_INLINE unsigned int dbus_u32_get(const PosixPickerCtx *args)
{
    return (unsigned int)args->bytes_read[args->at] | ((unsigned int)args->bytes_read[args->at + 1ull] << 8u) |
           ((unsigned int)args->bytes_read[args->at + 2ull] << 16u) |
           ((unsigned int)args->bytes_read[args->at + 3ull] << 24u);
}

// the bytes a value of the type `code` aligns to
EMBED_INLINE unsigned long long dbus_alignment_get(const PosixPickerCtx *args)
{
    return ((args->code_letter == 'y') || (args->code_letter == 'g') || (args->code_letter == 'v')) ? 1ull
           : ((args->code_letter == 'n') || (args->code_letter == 'q'))                             ? 2ull
           : ((args->code_letter == 'x') || (args->code_letter == 't') || (args->code_letter == 'd') ||
              (args->code_letter == '(') || (args->code_letter == '{'))
               ? 8ull
               : 4ull;
}

// the one complete type at `*type` in a signature passed over, its end into `*type`: 1, or 0 where it does not close
static int dbus_type_pass(const PosixPickerCtx *args)
{
    const char **type = args->type;
    const char opened = **type;
    *type += 1;
    if (opened == 'a')
    {
        return EMBED_CALL(dbus_type_pass, PosixPickerCtx, .type = type);
    }
    if ((opened != '(') && (opened != '{'))
    {
        return opened != '\0';
    }
    const char closing = (opened == '(') ? ')' : '}';
    while ((**type != closing) && (**type != '\0'))
    {
        if (!EMBED_CALL(dbus_type_pass, PosixPickerCtx, .type = type))
        {
            return 0;
        }
    }
    *type += (**type == closing) ? 1 : 0;
    return 1;
}

// a value of the one complete type at `*type` passed over in `size` bytes of `bytes` from `*at`, `depth` deep: the
// type's end into `*type` and the value's into `*at`: 1, or 0 where the value does not fit or nests too deep
static int dbus_value_pass(const PosixPickerCtx *args)
{
    unsigned long long *at = args->at_pointer;
    const char **type = args->type;
    const char code = **type;
    *at = (*at + EMBED_CALL(dbus_alignment_get, PosixPickerCtx, .code_letter = code) - 1ull) &
          ~(EMBED_CALL(dbus_alignment_get, PosixPickerCtx, .code_letter = code) - 1ull);
    if ((args->depth > CLI_DBUS_DEPTH) || (*at > args->size))
    {
        return 0;
    }
    if ((code == 's') || (code == 'o') || (code == 'g'))
    {
        const unsigned long long length =
            (code == 'g') ? ((*at < args->size) ? args->bytes_read[*at] : 0ull)
                          : ((*at + 4ull <= args->size)
                                 ? EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes_read, .at = *at)
                                 : 0ull);
        *at += ((code == 'g') ? 1ull : 4ull) + length + 1ull;
        *type += 1;
        return *at <= args->size;
    }
    if (code == 'a')
    {
        const unsigned long long length =
            (*at + 4ull <= args->size)
                ? EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes_read, .at = *at)
                : 0ull;
        *at += 4ull;
        *type += 1;
        const char item = **type;
        *at = (*at + EMBED_CALL(dbus_alignment_get, PosixPickerCtx, .code_letter = item) - 1ull) &
              ~(EMBED_CALL(dbus_alignment_get, PosixPickerCtx, .code_letter = item) - 1ull);
        *at += length;
        return EMBED_CALL(dbus_type_pass, PosixPickerCtx, .type = type) && (*at <= args->size);
    }
    if (code == 'v')
    {
        if (*at >= args->size)
        {
            return 0;
        }
        const char *inner = (const char *)args->bytes_read + *at + 1ull;
        *at += 1ull + args->bytes_read[*at] + 1ull;
        *type += 1;
        return (*at <= args->size) &&
               EMBED_CALL(dbus_value_pass, PosixPickerCtx, .bytes_read = args->bytes_read, .size = args->size,
                          .at_pointer = at, .type = &inner, .depth = args->depth + 1u);
    }
    if ((code == '(') || (code == '{'))
    {
        const char closing = (code == '(') ? ')' : '}';
        *type += 1;
        while ((**type != closing) && (**type != '\0'))
        {
            if (!EMBED_CALL(dbus_value_pass, PosixPickerCtx, .bytes_read = args->bytes_read, .size = args->size,
                            .at_pointer = at, .type = type, .depth = args->depth + 1u))
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
    *at += basic ? EMBED_CALL(dbus_alignment_get, PosixPickerCtx, .code_letter = code) : 0ull;
    *type += 1;
    return basic && (*at <= args->size);
}

// the next message the bus sends read into `capacity` bytes of `bytes`, its head's fields this file reads into
// `message`: 1, or 0 where the connection is gone or the message does not fit
static int dbus_message_read(const PosixPickerCtx *args)
{
    CliDbusMessage *message = args->message_dbus_message_pointer;
    if (!EMBED_CALL(dbus_receive, PosixPickerCtx, .socket = args->socket, .bytes_bytess_other = args->bytes,
                    .size = 16ull) ||
        (args->bytes[0] != 'l'))
    {
        return 0;
    }
    const unsigned long long body = EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes, .at = 4ull);
    const unsigned long long fields = EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes, .at = 12ull);
    const unsigned long long head = (16ull + fields + 7ull) & ~7ull;
    if ((head + body > args->capacity) ||
        !EMBED_CALL(dbus_receive, PosixPickerCtx, .socket = args->socket, .bytes_bytess_other = args->bytes + 16,
                    .size = head + body - 16ull))
    {
        return 0;
    }
    *message = (CliDbusMessage){args->bytes[1], 0u, "", "", args->bytes + head, body};
    for (unsigned long long at = 16ull; at + 4ull < 16ull + fields;)
    {
        at = (at + 7ull) & ~7ull;
        const unsigned char code = args->bytes[at];
        const char *type = (const char *)args->bytes + at + 2ull;
        const unsigned long long value = at + 2ull + args->bytes[at + 1ull] + 1ull;
        const unsigned long long string_at = (value + 3ull) & ~3ull;
        if ((code == CLI_DBUS_REPLY_SERIAL) && (type[0] == 'u'))
        {
            message->reply_serial =
                EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes, .at = string_at);
        }
        if (((code == CLI_DBUS_PATH) || (code == CLI_DBUS_MEMBER)) && ((type[0] == 'o') || (type[0] == 's')))
        {
            char *const into = (code == CLI_DBUS_PATH) ? message->path : message->member;
            const size_t into_capacity = (code == CLI_DBUS_PATH) ? sizeof(message->path) : sizeof(message->member);
            VerbaLinea copied = {into, into_capacity, 0ull};
            mmgr_line_put_n(&copied, (const char *)args->bytes + string_at + 4ull,
                            EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->bytes, .at = string_at));
            (void)mmgr_line_finish(&copied);
        }
        unsigned long long passed = value;
        if (!EMBED_CALL(dbus_value_pass, PosixPickerCtx, .bytes_read = args->bytes, .size = 16ull + fields,
                        .at_pointer = &passed, .type = &type, .depth = 0u))
        {
            return 0;
        }
        at = passed;
    }
    return 1;
}

// the reply to the call `serial` waited for, every other message passed over: 1 with it read into `message`, or 0
// where an error came in its place or the connection is gone
EMBED_INLINE int dbus_reply(const PosixPickerCtx *args)
{
    for (;;)
    {
        if (!dbus_message_read(args))
        {
            return 0;
        }
        if (((args->message_dbus_message_pointer->kind == CLI_DBUS_RETURN) ||
             (args->message_dbus_message_pointer->kind == CLI_DBUS_ERROR)) &&
            (args->message_dbus_message_pointer->reply_serial == args->serial))
        {
            return args->message_dbus_message_pointer->kind == CLI_DBUS_RETURN;
        }
    }
}

// a string of a body read at `*at`, its bytes into `text`, sized for `capacity`, the place after it into `*at`: 1, or 0
// where it does not fit the body
EMBED_INLINE int dbus_body_string(const PosixPickerCtx *args)
{
    unsigned long long *at = args->at_pointer;
    *at = (*at + 3ull) & ~3ull;
    if (*at + 4ull > args->message_dbus_message_pointer_other->body_size)
    {
        return 0;
    }
    const unsigned long long length = EMBED_CALL(
        dbus_u32_get, PosixPickerCtx, .bytes_read = args->message_dbus_message_pointer_other->body, .at = *at);
    if (*at + 4ull + length + 1ull > args->message_dbus_message_pointer_other->body_size)
    {
        return 0;
    }
    VerbaLinea copied = {args->text_out, args->capacity_bytes, 0ull};
    mmgr_line_put_n(&copied, (const char *)args->message_dbus_message_pointer_other->body + *at + 4ull, length);
    *at += 4ull + length + 1ull;
    return mmgr_line_finish(&copied) != 0ull;
}

// the session bus reached: the address DBUS_SESSION_BUS_ADDRESS names, a socket's path or an abstract name, else the
// bus in the reader's runtime folder; the reader known to it by their own id and the connection begun: the socket, or
// -1
static int dbus_connect(void)
{
    char address[512];
    char path[256];
    int abstract = 0;
    VerbaLinea named = {path, sizeof(path), 0ull};
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "DBUS_SESSION_BUS_ADDRESS",
                   .value_out = address, .capacity = sizeof(address)))
    {
        const char *const at_path =
            cellul.find(&(CatenaFinitaCfg){.src = address, .cap = sizeof(address), .other = "path=", .other_cap = 8u});
        const char *const at_abstract = cellul.find(
            &(CatenaFinitaCfg){.src = address, .cap = sizeof(address), .other = "abstract=", .other_cap = 16u});
        const char *const from = (at_path != NULL) ? at_path + 5 : ((at_abstract != NULL) ? at_abstract + 9 : NULL);
        abstract = (at_path == NULL) && (at_abstract != NULL);
        if (from != NULL)
        {
            mmgr_line_put_n(
                &named, from,
                (size_t)EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                                   .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}),
                                   .set = ",;"));
        }
    }
    if (named.at == 0ull)
    {
        char folder[256];
        mmgr_line_put(&named, EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                         .name = "XDG_RUNTIME_DIR", .value_out = folder, .capacity = sizeof(folder))
                                  ? folder
                                  : "");
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
    int authorized = mmgr_line_ok(&auth) &&
                     EMBED_CALL(dbus_send, PosixPickerCtx, .socket = bus, .bytes_bytess = line, .size = auth.at);
    while (authorized && (got + 1ull < sizeof(answer)) && ((got < 2ull) || (answer[got - 1ull] != '\n')))
    {
        authorized =
            EMBED_CALL(dbus_receive, PosixPickerCtx, .socket = bus, .bytes_bytess_other = answer + got, .size = 1ull);
        got += 1ull;
    }
    authorized = authorized && (got >= 2ull) && (answer[0] == 'O') && (answer[1] == 'K') &&
                 EMBED_CALL(dbus_send, PosixPickerCtx, .socket = bus, .bytes_bytess = "BEGIN\r\n", .size = 7ull);
    if (!authorized)
    {
        close(bus);
        return -1;
    }
    return bus;
}

// the file URI `uri` made a path into `path`, sized for `capacity`, its host taken off and its escapes read: 1, or 0
// where it is no file URI or does not fit
EMBED_INLINE int dbus_uri_path(const PosixPickerCtx *args)
{
    if (!cellul.starts(&(CatenaFinitaCfg){.src = args->uri, .other = "file://", .cap = CLI_PATH_MAX_LEN}))
    {
        return 0;
    }
    const char *at = cellul.chr(&(CatenaFinitaCfg){.src = args->uri + 7, .cap = CLI_PATH_MAX_LEN, .byte = '/'});
    VerbaLinea decoded = {args->path, args->capacity_bytes, 0ull};
    if (at != NULL)
    {
        EMBED_CALL(cli_text_size_set.append_unescaped, CliTextSizeSetCfg, .out = &decoded, .text = at,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}));
    }
    return mmgr_line_finish(&decoded) != 0ull;
}

// the first file the Response signal's results name, `{"uris": [...]}`, made a path into `path`: 1, or 0 where the
// reader cancelled or no file is named
EMBED_INLINE int dbus_response_read(const PosixPickerCtx *args)
{
    if ((args->message_dbus_message_pointer_other->body_size < 8ull) ||
        (EMBED_CALL(dbus_u32_get, PosixPickerCtx, .bytes_read = args->message_dbus_message_pointer_other->body,
                    .at = 0ull) != 0u))
    {
        return 0;
    }
    const unsigned long long length = EMBED_CALL(
        dbus_u32_get, PosixPickerCtx, .bytes_read = args->message_dbus_message_pointer_other->body, .at = 4ull);
    const unsigned long long end = 8ull + length;
    for (unsigned long long at = 8ull; (at < end) && (end <= args->message_dbus_message_pointer_other->body_size);)
    {
        at = (at + 7ull) & ~7ull;
        char key[64];
        if (!EMBED_CALL(dbus_body_string, PosixPickerCtx,
                        .message_dbus_message_pointer_other = args->message_dbus_message_pointer_other,
                        .at_pointer = &at, .text_out = key, .capacity_bytes = sizeof(key)) ||
            (at >= args->message_dbus_message_pointer_other->body_size))
        {
            return 0;
        }
        const char *type = (const char *)args->message_dbus_message_pointer_other->body + at + 1ull;
        const int uris = cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "uris", .cap = sizeof(key)}) &&
                         (type[0] == 'a') && (type[1] == 's');
        at += 1ull + args->message_dbus_message_pointer_other->body[at] + 1ull;
        if (uris)
        {
            unsigned long long first = ((at + 3ull) & ~3ull) + 4ull;
            char uri[CLI_PATH_MAX_LEN * 3u];
            return (EMBED_CALL(dbus_u32_get, PosixPickerCtx,
                               .bytes_read = args->message_dbus_message_pointer_other->body,
                               .at = (at + 3ull) & ~3ull) > 0u) &&
                   EMBED_CALL(dbus_body_string, PosixPickerCtx,
                              .message_dbus_message_pointer_other = args->message_dbus_message_pointer_other,
                              .at_pointer = &first, .text_out = uri, .capacity_bytes = sizeof(uri)) &&
                   EMBED_CALL(dbus_uri_path, PosixPickerCtx, .uri = uri, .path = args->path,
                              .capacity_bytes = args->capacity_bytes);
        }
        if (!EMBED_CALL(dbus_value_pass, PosixPickerCtx, .bytes_read = args->message_dbus_message_pointer_other->body,
                        .size = args->message_dbus_message_pointer_other->body_size, .at_pointer = &at, .type = &type,
                        .depth = 0u))
        {
            return 0;
        }
    }
    return 0;
}

int cli_file_pick(const CliPathCapacityCtx *args)
{
    static unsigned char s_message[CLI_DBUS_MESSAGE_MAX];
    args->path_out[0] = '\0';
    const int bus = dbus_connect();
    CliDbusWritten message;
    CliDbusMessage read;
    unsigned long long body_at = 0ull;
    int chosen = bus >= 0;
    // the connection named to the bus
    EMBED_CALL(dbus_call_begin, PosixPickerCtx, .message = &message, .bytes = s_message, .capacity = sizeof(s_message),
               .serial = 1u, .destination = "org.freedesktop.DBus", .object = "/org/freedesktop/DBus",
               .interface = "org.freedesktop.DBus", .member = "Hello", .signature = "", .body_length_at = &body_at);
    EMBED_CALL(dbus_call_end, PosixPickerCtx, .message = &message, .body_length_at_value = body_at, .head = message.at);
    chosen = chosen && message.fits &&
             EMBED_CALL(dbus_send, PosixPickerCtx, .socket = bus, .bytes_bytess = s_message, .size = message.at) &&
             EMBED_CALL(dbus_reply, PosixPickerCtx, .socket = bus, .bytes = s_message, .capacity = sizeof(s_message),
                        .serial = 1u, .message_dbus_message_pointer = &read);
    // the portal's answers heard, every request's Response signal
    EMBED_CALL(dbus_call_begin, PosixPickerCtx, .message = &message, .bytes = s_message, .capacity = sizeof(s_message),
               .serial = 2u, .destination = "org.freedesktop.DBus", .object = "/org/freedesktop/DBus",
               .interface = "org.freedesktop.DBus", .member = "AddMatch", .signature = "s", .body_length_at = &body_at);
    unsigned long long head = message.at;
    EMBED_CALL(dbus_string, PosixPickerCtx, .message = &message,
               .text = "type='signal',interface='org.freedesktop.portal.Request',member='Response'");
    EMBED_CALL(dbus_call_end, PosixPickerCtx, .message = &message, .body_length_at_value = body_at, .head = head);
    chosen = chosen && message.fits &&
             EMBED_CALL(dbus_send, PosixPickerCtx, .socket = bus, .bytes_bytess = s_message, .size = message.at) &&
             EMBED_CALL(dbus_reply, PosixPickerCtx, .socket = bus, .bytes = s_message, .capacity = sizeof(s_message),
                        .serial = 2u, .message_dbus_message_pointer = &read);
    // the chooser asked for: a file to open, a folder, or a file to save
    EMBED_CALL(dbus_call_begin, PosixPickerCtx, .message = &message, .bytes = s_message, .capacity = sizeof(s_message),
               .serial = 3u, .destination = "org.freedesktop.portal.Desktop",
               .object = "/org/freedesktop/portal/desktop", .interface = "org.freedesktop.portal.FileChooser",
               .member = (args->kind == CLI_PICK_SAVE) ? "SaveFile" : "OpenFile", .signature = "ssa{sv}",
               .body_length_at = &body_at);
    head = message.at;
    EMBED_CALL(dbus_string, PosixPickerCtx, .message = &message, .text = "");
    EMBED_CALL(dbus_string, PosixPickerCtx, .message = &message,
               .text = (args->kind == CLI_PICK_FOLDER) ? "Choose a folder"
                                                       : ((args->kind == CLI_PICK_SAVE) ? "Save" : "Open"));
    const unsigned long long options_at = message.at;
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = &message, .value = 0u);
    EMBED_CALL(dbus_align, PosixPickerCtx, .message = &message, .alignment = 8ull);
    const unsigned long long options_start = message.at;
    EMBED_CALL(dbus_string, PosixPickerCtx, .message = &message, .text = "modal");
    EMBED_CALL(dbus_signature, PosixPickerCtx, .message = &message, .signature = "b");
    EMBED_CALL(dbus_u32, PosixPickerCtx, .message = &message, .value = 1u);
    if (args->kind == CLI_PICK_FOLDER)
    {
        EMBED_CALL(dbus_align, PosixPickerCtx, .message = &message, .alignment = 8ull);
        EMBED_CALL(dbus_string, PosixPickerCtx, .message = &message, .text = "directory");
        EMBED_CALL(dbus_signature, PosixPickerCtx, .message = &message, .signature = "b");
        EMBED_CALL(dbus_u32, PosixPickerCtx, .message = &message, .value = 1u);
    }
    // the options' array is counted from where its entries begin
    EMBED_CALL(dbus_u32_at, PosixPickerCtx, .message = &message, .at = options_at,
               .value = (unsigned int)(message.at - options_start));
    EMBED_CALL(dbus_call_end, PosixPickerCtx, .message = &message, .body_length_at_value = body_at, .head = head);
    char handle[256];
    unsigned long long handle_at = 0ull;
    chosen = chosen && message.fits &&
             EMBED_CALL(dbus_send, PosixPickerCtx, .socket = bus, .bytes_bytess = s_message, .size = message.at) &&
             EMBED_CALL(dbus_reply, PosixPickerCtx, .socket = bus, .bytes = s_message, .capacity = sizeof(s_message),
                        .serial = 3u, .message_dbus_message_pointer = &read) &&
             EMBED_CALL(dbus_body_string, PosixPickerCtx, .message_dbus_message_pointer_other = &read,
                        .at_pointer = &handle_at, .text_out = handle, .capacity_bytes = sizeof(handle));
    // the reader's answer waited for: the Response signal of the request the portal named
    while (chosen)
    {
        chosen = EMBED_CALL(dbus_message_read, PosixPickerCtx, .socket = bus, .bytes = s_message,
                            .capacity = sizeof(s_message), .message_dbus_message_pointer = &read);
        const int answered = chosen && (read.kind == CLI_DBUS_SIGNAL) &&
                             cellul.eq(&(CatenaFinitaCfg){.src = read.member, .other = "Response", .cap = 64u}) &&
                             cellul.eq(&(CatenaFinitaCfg){.src = read.path, .other = handle, .cap = sizeof(handle)});
        if (answered)
        {
            chosen = EMBED_CALL(dbus_response_read, PosixPickerCtx, .message_dbus_message_pointer_other = &read,
                                .path = args->path_out, .capacity_bytes = args->capacity_bytes);
            break;
        }
    }
    if (bus >= 0)
    {
        close(bus);
    }
    return chosen;
}
#endif
#endif
