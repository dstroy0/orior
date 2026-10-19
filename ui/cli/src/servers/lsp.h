// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp.h: a client of a language server, such as clangd, over its input and output: each message a Content-Length
// header and a JSON body, as the Language Server Protocol frames them. A request waits for its answer up to a time it
// is given. What the server says on its own, a notification such as the diagnostics of a file or a request such as an
// edit to apply, goes to the function the server was started with, and a request is answered with what that function
// writes.
//
// The editor counts a line's columns in UTF-16 units, as the protocol does where it is not told otherwise: positions
// pass between them as they are. The whole text goes with each change (lsp.c, lsp_wire.c, lsp_start.c, lsp_files.c)
#ifndef LSP_H
#define LSP_H

#include "../command_line/cli_internal.h"

// the most requests that wait on answers at once, and the most bytes of what the server writes to its errors that are
// kept
#define CLI_LSP_WAITERS 64u
#define CLI_LSP_COMPLAINT 4096u

// A running language server
typedef struct CliLsp CliLsp;

// what the server says on its own: the method and `size` bytes of its params as JSON, the answer to a request written
// into `answer` as JSON, `null` where nothing is written; and what is not sent for a notification
typedef void (*CliLspHeard)(void *context, const char *method, const char *params, unsigned long long size,
                            CliLine *answer);

// Where a framed message is read from: a function that reads a piece of at most `room` bytes, 0 at the end, its
// source, and what was read and not yet taken
typedef struct
{
    unsigned long long (*read)(void *source, char *piece, unsigned long long room);
    void *source;
    char held[65536];
    unsigned long long start;
    unsigned long long end;
} CliLspReader;

// Where a framed message is written: a function that writes `size` bytes whole, 1, or 0 where it could not, and its
// sink
typedef struct
{
    int (*write)(void *sink, const void *bytes, unsigned long long size);
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
// or 0 with why said; one message read from `reader` into `body`, which holds `room`, its size into `size`: 1, or 0
// where the reader's source ended, the message gave no length or the body is no JSON, a body larger than the room read
// and let go and the next one read; one message read into a cell of its length with a 0 after, its size into `size`:
// the cell, given back by the caller, or NULL where the source ended, a body the cells cannot meet or that is no JSON
// read and let go and the next one read; and the member `name` of a message's top object, as its start and length in
// the body: 1, or 0 where it has none (lsp_wire.c)
int cli_lsp_send(const CliLspWriter *writer, CliLock *lock, const char *body, unsigned long long size, CliLine *said);
int cli_lsp_send_parts(const CliLspWriter *writer, CliLock *lock, const CliLspPart *parts, unsigned int count,
                       CliLine *said);
int cli_lsp_read_message(CliLspReader *reader, char *body, unsigned long long room, unsigned long long *size);
char *cli_lsp_message_taken(CliLspReader *reader, unsigned long long *size);
int cli_lsp_member(const char *body, unsigned long long size, const char *name, unsigned long long *start,
                   unsigned long long *length);
// a JSON string of `length` bytes read as its letters into `out`, which holds `room`: 1, or 0 where it is no string
// or does not fit (lsp_wire.c)
int cli_lsp_string(const char *text, unsigned long long length, char *out, unsigned long long room);

// a path as the protocol names a file into `uri`, which holds `room`: file:///D:/a/b%20c on Windows, file:///a/b
// elsewhere; the path a file: URI of `length` bytes names into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where it
// names none; and the one name a file goes by however a server writes its URI into `key`, which holds
// CLI_PATH_LONGEST: its path with forward slashes, in lower case on Windows, whose paths do not tell files apart by
// case
void cli_lsp_uri_of(const char *path, char *uri, unsigned long long room);
int cli_lsp_path_of(const char *uri, unsigned long long length, char *path);
void cli_lsp_key_of(const char *uri, unsigned long long length, char *key);

// the server `program` started with `args`, `count` of them, at `root`, reading the tree there with its own `options`,
// `length` bytes of JSON, with the settings `settings`, `setting_count` of them; `heard` takes what it says on its own:
// the server, held once for its starter, or NULL with why said. The server held once more, and a hold given back, the
// server ended with the last (lsp_start.c)
CliLsp *cli_lsp_started(const char *program, const char *const *args, unsigned int count, const char *root,
                        const char *options, unsigned long long length, const CliSetting *settings,
                        unsigned int setting_count, CliLspHeard heard, void *context, CliLine *said);
void cli_lsp_held(CliLsp *lsp);
void cli_lsp_given(CliLsp *lsp);

// what the server said it can do, as it answered `initialize`, as JSON, its size into `size`; the process the server
// runs as; whether it still runs; the last line it wrote to its errors, or nothing, into `out`; and the server asked to
// end, and ended where it does not (lsp.c)
const char *cli_lsp_capabilities(const CliLsp *lsp, unsigned long long *size);
unsigned long long cli_lsp_pid(const CliLsp *lsp);
int cli_lsp_alive(CliLsp *lsp);
void cli_lsp_complaint(CliLsp *lsp, CliLine *out);
void cli_lsp_stopped(CliLsp *lsp);

// a request of `method` with `length` bytes of JSON params, its answer waited for up to `wait` milliseconds and written
// into `result`; a notification sent; and the params of a request about the place `line`, `col` of `path` written,
// `added`, members of JSON, after the place where it is given. Each 1, or 0 with why said (lsp.c)
int cli_lsp_requested(CliLsp *lsp, const char *method, const char *params, unsigned long long length,
                      unsigned long long wait, CliLine *result, CliLine *said);
int cli_lsp_notified(CliLsp *lsp, const char *method, const char *params, unsigned long long length, CliLine *said);
void cli_lsp_at(const char *path, unsigned int line, unsigned int col, const char *added, CliLine *params);

// the file at `path` opened in the server as `language_id` with `length` bytes of `text`; given its new whole text;
// given the text it was last given again, as a new version, the server then reading again what the file includes or
// imports; and closed. Each 1, or 0 with why said. Whether the server has `path` open; the text it was last given of a
// file it has open into `out`: 1, or 0 where it has none; and each file it has open, with the text it was last given,
// handed to `each` (lsp_files.c)
int cli_lsp_opened(CliLsp *lsp, const char *path, const char *language_id, const char *text, unsigned long long length,
                   CliLine *said);
int cli_lsp_changed(CliLsp *lsp, const char *path, const char *text, unsigned long long length, CliLine *said);
int cli_lsp_read_again(CliLsp *lsp, const char *path, CliLine *said);
int cli_lsp_closed(CliLsp *lsp, const char *path, CliLine *said);
int cli_lsp_has(CliLsp *lsp, const char *path);
int cli_lsp_text(CliLsp *lsp, const char *path, CliLine *out);
void cli_lsp_held_each(CliLsp *lsp,
                       void (*each)(void *context, const char *path, const char *text, unsigned long long length),
                       void *context);

#endif
