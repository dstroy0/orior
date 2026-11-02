// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp.h: a client of a language server, such as clangd, over its input and output: each message a Content-Length
// header and a JSON body, as the Language Server Protocol frames them. A request waits for its answer up to a time it
// is given. What the server sends unprompted, a notification such as the diagnostics of a file or a request such as an
// edit to apply, goes to the function the server was started with, and a request is answered with what that function
// writes.
//
// The editor counts a line's columns in UTF-16 units, as the protocol does where it is not instructed otherwise:
// positions pass between them as they are. The whole text goes with each change (lsp.c, lsp_wire.c, lsp_start.c,
// lsp_files.c)
#ifndef LSP_H
#define LSP_H
typedef struct CliLspErrMsgCfg CliLspErrMsgCfg;
typedef struct CliLspErrMsgLengthCfg CliLspErrMsgLengthCfg;
typedef struct CliLspErrMsgTextCfg CliLspErrMsgTextCfg;
typedef struct CliLspSizeErrMsgCfg CliLspSizeErrMsgCfg;
typedef struct CliLspPathCfg CliLspPathCfg;
typedef struct CliLspBodyCfg CliLspBodyCfg;
typedef struct CliLspWriterCfg CliLspWriterCfg;
typedef struct CliLspWriterLockCfg CliLspWriterLockCfg;
typedef struct CliLspReaderCfg CliLspReaderCfg;
typedef struct CliLspCapacityCfg CliLspCapacityCfg;
typedef struct CliLspLengthCfg CliLspLengthCfg;
typedef struct CliLspReaderSizeCfg CliLspReaderSizeCfg;
typedef struct CliLspContextCfg CliLspContextCfg;
typedef struct CliLspSizeCfg CliLspSizeCfg;
typedef struct CliLspLengthUriCfg CliLspLengthUriCfg;
typedef struct CliLspPathOutCfg CliLspPathOutCfg;
typedef struct CliLspSizePathCfg CliLspSizePathCfg;
typedef struct CliLspCapacityPathCfg CliLspCapacityPathCfg;
typedef struct CliLspCfg CliLspCfg;
typedef struct CliLspSizeKindCfg CliLspSizeKindCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the most requests that wait on answers at once, and the most bytes of what the server writes to its errors that are
// retained
#define CLI_LSP_WAITERS 64u
#define CLI_LSP_COMPLAINT 4096u

// the most bytes of orior's answer to a request of the server's
#define CLI_LSP_ANSWERED (1ull << 20u)

// A running language server
typedef struct CliLsp CliLsp;

// A request's answer: the message it came in, in a cell of the message's length the request gives back to the owner the
// message was read for; and the part of it that answers, ended by a 0, and that part's size
typedef struct
{
    char *message;
    const char *text;
    unsigned long long size;
} CliLspAnswer;

// what the server sends unprompted: the method and `size` bytes of its params as JSON, the answer to a request written
// into `answer` as JSON, `null` where nothing is written; and what is not sent for a notification
typedef void (*CliLspRecvFn)(const CliLspRecvCfg *args);

// Where a framed message is read from: a function that reads a piece of at most `capacity` bytes, 0 at the end, its
// source, and what was read and not yet taken
typedef struct
{
    unsigned long long (*read)(const CliSourcePieceCapacityCfg *args);
    void *source;
    char buf[65536];
    unsigned long long start;
    unsigned long long end;
} CliLspReader;

// Where a framed message is written: a function that writes `size` bytes whole, 1, or 0 where it could not, and its
// sink
typedef struct
{
    int (*write)(const CliSinkBytesSizeCfg *args);
    void *sink;
} CliLspWriter;

// A piece of a message's body: its bytes, written as they are or, where `quoted`, as a JSON string's letters
typedef struct
{
    const char *bytes;
    unsigned long long size;
    int quoted;
} CliLspPart;

// a piece of a message written as the literal `text_` is, its size counted from the literal
#define CLI_LSP_RAW(text_) {(text_), sizeof(text_) - 1ull, 0}

// `size` bytes of a JSON body framed and written whole by `writer` under `lock`; the same of a body made of `count`
// pieces, each text written as a JSON string's letters as it goes, so that a file's whole text is never copied: each 1,
// or 0 with why given; one message read from `reader` into `body`, sized for `capacity`, its size into `size`: 1, or 0
// where the reader's source ended, the message gave no length or the body is no JSON, a body larger than the capacity
// read and let go and the next one read; one message read into a cell of its length with a 0 after, counted to `owner`,
// its size into `size`: the cell, given back by the caller, or NULL where the source ended, a body the cells cannot
// meet or that is no JSON read and let go and the next one read; and the member `name` of a message's top object, as
// its start and length in the body: 1, or 0 where it has none (lsp_wire.c)
int cli_lsp_send(const CliLspWriterCfg *args);
int cli_lsp_send_parts(const CliLspWriterLockCfg *args);
int cli_lsp_read_message(const CliLspReaderCfg *args);
char *cli_lsp_message_alloc(const CliLspReaderSizeCfg *args);
int cli_lsp_member(const CliLspBodyCfg *args);
// a JSON string of `length` bytes read as its letters into `out`, sized for `capacity`: 1, or 0 where it is no string
// or does not fit (lsp_wire.c)
int cli_lsp_string(const CliLspCapacityCfg *args);

// a path as the protocol names a file into `uri`, sized for `capacity`: file:///D:/a/b%20c on Windows, file:///a/b
// elsewhere; the path a file: URI of `length` bytes names into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it
// names none; and the one name a file goes by however a server writes its URI into `key`, sized for
// CLI_PATH_MAX_LEN: its path with forward slashes, in lower case on Windows, whose paths do not distinguish files by
// case
void cli_lsp_uri_get(const CliLspCapacityPathCfg *args);
int cli_lsp_path_get(const CliLspLengthUriCfg *args);
void cli_lsp_key_get(const CliLspLengthCfg *args);

// the server `program` started with `args`, `count` of them, at `root`, reading the tree there with its own `options`,
// `length` bytes of JSON, with the settings `settings`, `setting_count` of them; `recv` takes what it sends unprompted:
// the server, referenced once for its starter, or NULL with why given. The server referenced once more, and a reference
// given back, the server ended with the last (lsp_start.c)
CliLsp *cli_lsp_start(const CliLspErrMsgCfg *args);
void cli_lsp_ref(const CliLspContextCfg *args);
void cli_lsp_unref(const CliLspContextCfg *args);

// what the server reported it can do, as it answered `initialize`, as JSON, its size into `size`; the process the
// server runs as; whether it still runs; the last line it wrote to its errors, or nothing, into `out`; and the server
// requested to end, and ended where it does not (lsp.c)
const char *cli_lsp_capabilities(const CliLspSizeKindCfg *args);
unsigned long long cli_lsp_pid(const CliLspSizeKindCfg *args);
int cli_lsp_alive(const CliLspContextCfg *args);
void cli_lsp_complaint(const CliLspPathOutCfg *args);
void cli_lsp_stop(const CliLspContextCfg *args);

// a request of `method` with `length` bytes of JSON params, its answer waited for up to `wait` milliseconds and handed
// over in `answer`, whose message the caller gives back to g_cli_servers_cells, NULL where the request failed; and a
// notification sent. Each 1, or 0 with why given (lsp.c)
int cli_lsp_request(const CliLspErrMsgLengthCfg *args);
int cli_lsp_notify(const CliLspErrMsgLengthCfg *args);
// the request the server sent that its receiver is processing now left unanswered, to be answered later, its id's JSON
// into `id`, sized for `capacity`: 1, or 0 where what is received is no request or its id does not fit; and the request
// of the id `id` answered with `size` bytes of JSON `result`: 1, or 0 with why given (lsp.c)
int cli_lsp_answer_later(const CliLspCfg *args);
// the params of a request about the place `line`, `col` of `path` written, `added`, members of JSON, after the place
// where it is given (lsp_wire.c)
void cli_lsp_at(const CliLspPathCfg *args);

// the file at `path` opened in the server as `language_id` with `length` bytes of `text`; given its new whole text;
// given the text it was last given again, as a new version, the server then reading again what the file includes or
// imports; and closed. Each 1, or 0 with why given. The version of `path` the server was last given, 0 where it has
// none open; whether the server has `path` open; the size of the text it was last given of a file it has open into
// `size` and that text into `out`: each 1, or 0 where it has none; the URIs of the files it has open, each ended by a
// 0, in a cell of their size counted to `owner`, their size into `size`, NULL where it has none; and each file it has
// open, with the text it was last given, handed to `each` (lsp_files.c)
int cli_lsp_open(const CliLspErrMsgTextCfg *args);
int cli_lsp_change(const CliLspErrMsgTextCfg *args);
int cli_lsp_read_again(const CliLspErrMsgTextCfg *args);
int cli_lsp_close(const CliLspErrMsgTextCfg *args);
unsigned long long cli_lsp_version(const CliLspPathOutCfg *args);
int cli_lsp_has(const CliLspPathOutCfg *args);
int cli_lsp_text_size(const CliLspSizePathCfg *args);
int cli_lsp_text(const CliLspPathOutCfg *args);
char *cli_lsp_open_uris_alloc(const CliLspSizeCfg *args);
void cli_lsp_open_each(const CliLspContextCfg *args);

struct CliLspErrMsgCfg
{
    VerbaLinea *err_msg;
    unsigned int count;
    unsigned long long length;
    const char *program;
    const char *const *request;
    const char *root;
    const char *options;
    const CliSetting *settings;
    unsigned int setting_count;
    CliLspRecvFn recv;
    void *context;
};

struct CliLspErrMsgLengthCfg
{
    VerbaLinea *err_msg;
    unsigned long long length;
    CliLsp *lsp;
    const char *method;
    const char *params;
    unsigned long long wait;
    CliLspAnswer *answer;
};

struct CliLspErrMsgTextCfg
{
    VerbaLinea *err_msg;
    const char *text;
    unsigned long long length;
    const char *path;
    CliLsp *lsp;
    const char *language_id;
};

struct CliLspSizeErrMsgCfg
{
    unsigned long long size_value;
    VerbaLinea *err_msg;
    CliLsp *lsp;
    const char *id_read;
    const char *result;
};

struct CliLspPathCfg
{
    const char *path;
    unsigned int line;
    unsigned int col;
    const char *added;
    VerbaLinea *params_verba_linea_pointer;
};

struct CliLspBodyCfg
{
    const char *body;
    unsigned long long size_value;
    const char *name;
    unsigned long long *start;
    unsigned long long *length_pointer;
};

struct CliLspWriterCfg
{
    const CliLspWriter *writer;
    const char *body;
    unsigned long long size_value;
    VerbaLinea *err_msg;
};

struct CliLspWriterLockCfg
{
    const CliLspWriter *writer;
    VerbaLinea *err_msg;
    const CliLspPart *parts;
    unsigned int count;
};

struct CliLspReaderCfg
{
    CliLspReader *reader;
    char *body_out;
    unsigned long long capacity;
    unsigned long long *size;
};

struct CliLspCapacityCfg
{
    unsigned long long capacity;
    const char *text;
    unsigned long long length;
    char *out_capacity;
};

struct CliLspLengthCfg
{
    unsigned long long length;
    const char *uri;
    char *key;
};

struct CliLspReaderSizeCfg
{
    CliLspReader *reader;
    unsigned long long *size;
    CellOwner *owner;
};

struct CliLspContextCfg
{
    void *context;
    CliLsp *lsp;
    void (*each)(const CliEachPathTextLengthCfg *args);
};

struct CliLspSizeCfg
{
    unsigned long long *size;
    CellOwner *owner;
    CliLsp *lsp;
};

struct CliLspLengthUriCfg
{
    unsigned long long length;
    const char *uri;
    char *path_out;
};

struct CliLspPathOutCfg
{
    const char *path;
    CliLsp *lsp;
    VerbaLinea *out;
};

struct CliLspSizePathCfg
{
    unsigned long long *size;
    const char *path;
    CliLsp *lsp;
};

struct CliLspCapacityPathCfg
{
    unsigned long long capacity;
    const char *path;
    char *uri_out;
};

struct CliLspCfg
{
    unsigned long long capacity;
    char *id;
};

struct CliLspSizeKindCfg
{
    unsigned long long *size;
    const CliLsp *lsp_read;
};

typedef struct
{
    int (*send)(const CliLspWriterCfg *args);
} CliLspWriterNs;
EMBED_TABLE_LAYOUT(CliLspWriterNs, send);
EMBED_TABLE_STORAGE CliLspWriterNs cli_lsp_writer EMBED_UNUSED = {
    .send = cli_lsp_send,
};

typedef struct
{
    int (*send_parts)(const CliLspWriterLockCfg *args);
} CliLspWriterLockNs;
EMBED_TABLE_LAYOUT(CliLspWriterLockNs, send_parts);
EMBED_TABLE_STORAGE CliLspWriterLockNs cli_lsp_writer_lock EMBED_UNUSED = {
    .send_parts = cli_lsp_send_parts,
};

typedef struct
{
    int (*read_message)(const CliLspReaderCfg *args);
} CliLspReaderNs;
EMBED_TABLE_LAYOUT(CliLspReaderNs, read_message);
EMBED_TABLE_STORAGE CliLspReaderNs cli_lsp_reader EMBED_UNUSED = {
    .read_message = cli_lsp_read_message,
};

typedef struct
{
    char *(*message_alloc)(const CliLspReaderSizeCfg *args);
} CliLspReaderSizeNs;
EMBED_TABLE_LAYOUT(CliLspReaderSizeNs, message_alloc);
EMBED_TABLE_STORAGE CliLspReaderSizeNs cli_lsp_reader_size EMBED_UNUSED = {
    .message_alloc = cli_lsp_message_alloc,
};

typedef struct
{
    int (*member)(const CliLspBodyCfg *args);
} CliLspBodyNs;
EMBED_TABLE_LAYOUT(CliLspBodyNs, member);
EMBED_TABLE_STORAGE CliLspBodyNs cli_lsp_body EMBED_UNUSED = {
    .member = cli_lsp_member,
};

typedef struct
{
    int (*string)(const CliLspCapacityCfg *args);
} CliLspCapacityNs;
EMBED_TABLE_LAYOUT(CliLspCapacityNs, string);
EMBED_TABLE_STORAGE CliLspCapacityNs cli_lsp_capacity EMBED_UNUSED = {
    .string = cli_lsp_string,
};

typedef struct
{
    void (*uri_get)(const CliLspCapacityPathCfg *args);
} CliLspCapacityPathNs;
EMBED_TABLE_LAYOUT(CliLspCapacityPathNs, uri_get);
EMBED_TABLE_STORAGE CliLspCapacityPathNs cli_lsp_capacity_path EMBED_UNUSED = {
    .uri_get = cli_lsp_uri_get,
};

typedef struct
{
    int (*path_get)(const CliLspLengthUriCfg *args);
} CliLspLengthUriNs;
EMBED_TABLE_LAYOUT(CliLspLengthUriNs, path_get);
EMBED_TABLE_STORAGE CliLspLengthUriNs cli_lsp_length_uri EMBED_UNUSED = {
    .path_get = cli_lsp_path_get,
};

typedef struct
{
    void (*key_get)(const CliLspLengthCfg *args);
} CliLspLengthNs;
EMBED_TABLE_LAYOUT(CliLspLengthNs, key_get);
EMBED_TABLE_STORAGE CliLspLengthNs cli_lsp_length EMBED_UNUSED = {
    .key_get = cli_lsp_key_get,
};

typedef struct
{
    CliLsp *(*start)(const CliLspErrMsgCfg *args);
} CliLspErrMsgNs;
EMBED_TABLE_LAYOUT(CliLspErrMsgNs, start);
EMBED_TABLE_STORAGE CliLspErrMsgNs cli_lsp_err_msg EMBED_UNUSED = {
    .start = cli_lsp_start,
};

typedef struct
{
    void (*ref)(const CliLspContextCfg *args);
    void (*unref)(const CliLspContextCfg *args);
    int (*alive)(const CliLspContextCfg *args);
    void (*stop)(const CliLspContextCfg *args);
    void (*open_each)(const CliLspContextCfg *args);
} CliLspContextNs;
EMBED_TABLE_LAYOUT(CliLspContextNs, ref, unref, alive, stop, open_each);
EMBED_TABLE_STORAGE CliLspContextNs cli_lsp_context EMBED_UNUSED = {
    .ref = cli_lsp_ref,
    .unref = cli_lsp_unref,
    .alive = cli_lsp_alive,
    .stop = cli_lsp_stop,
    .open_each = cli_lsp_open_each,
};

typedef struct
{
    const char *(*capabilities)(const CliLspSizeKindCfg *args);
    unsigned long long (*pid)(const CliLspSizeKindCfg *args);
} CliLspSizeKindNs;
EMBED_TABLE_LAYOUT(CliLspSizeKindNs, capabilities, pid);
EMBED_TABLE_STORAGE CliLspSizeKindNs cli_lsp_size_kind EMBED_UNUSED = {
    .capabilities = cli_lsp_capabilities,
    .pid = cli_lsp_pid,
};

typedef struct
{
    void (*complaint)(const CliLspPathOutCfg *args);
    unsigned long long (*version)(const CliLspPathOutCfg *args);
    int (*has)(const CliLspPathOutCfg *args);
    int (*text)(const CliLspPathOutCfg *args);
} CliLspPathOutNs;
EMBED_TABLE_LAYOUT(CliLspPathOutNs, complaint, version, has, text);
EMBED_TABLE_STORAGE CliLspPathOutNs cli_lsp_path_out EMBED_UNUSED = {
    .complaint = cli_lsp_complaint,
    .version = cli_lsp_version,
    .has = cli_lsp_has,
    .text = cli_lsp_text,
};

typedef struct
{
    int (*request)(const CliLspErrMsgLengthCfg *args);
    int (*notify)(const CliLspErrMsgLengthCfg *args);
} CliLspErrMsgLengthNs;
EMBED_TABLE_LAYOUT(CliLspErrMsgLengthNs, request, notify);
EMBED_TABLE_STORAGE CliLspErrMsgLengthNs cli_lsp_err_msg_length EMBED_UNUSED = {
    .request = cli_lsp_request,
    .notify = cli_lsp_notify,
};

typedef struct
{
    int (*answer_later)(const CliLspCfg *args);
} CliLspNs;
EMBED_TABLE_LAYOUT(CliLspNs, answer_later);
EMBED_TABLE_STORAGE CliLspNs cli_lsp EMBED_UNUSED = {
    .answer_later = cli_lsp_answer_later,
};

typedef struct
{
    void (*at)(const CliLspPathCfg *args);
} CliLspPathNs;
EMBED_TABLE_LAYOUT(CliLspPathNs, at);
EMBED_TABLE_STORAGE CliLspPathNs cli_lsp_path EMBED_UNUSED = {
    .at = cli_lsp_at,
};

typedef struct
{
    int (*open)(const CliLspErrMsgTextCfg *args);
    int (*change)(const CliLspErrMsgTextCfg *args);
    int (*read_again)(const CliLspErrMsgTextCfg *args);
    int (*close)(const CliLspErrMsgTextCfg *args);
} CliLspErrMsgTextNs;
EMBED_TABLE_LAYOUT(CliLspErrMsgTextNs, open, change, read_again, close);
EMBED_TABLE_STORAGE CliLspErrMsgTextNs cli_lsp_err_msg_text EMBED_UNUSED = {
    .open = cli_lsp_open,
    .change = cli_lsp_change,
    .read_again = cli_lsp_read_again,
    .close = cli_lsp_close,
};

typedef struct
{
    int (*text_size)(const CliLspSizePathCfg *args);
} CliLspSizePathNs;
EMBED_TABLE_LAYOUT(CliLspSizePathNs, text_size);
EMBED_TABLE_STORAGE CliLspSizePathNs cli_lsp_size_path EMBED_UNUSED = {
    .text_size = cli_lsp_text_size,
};

typedef struct
{
    char *(*open_uris_alloc)(const CliLspSizeCfg *args);
} CliLspSizeNs;
EMBED_TABLE_LAYOUT(CliLspSizeNs, open_uris_alloc);
EMBED_TABLE_STORAGE CliLspSizeNs cli_lsp_size EMBED_UNUSED = {
    .open_uris_alloc = cli_lsp_open_uris_alloc,
};

#endif
