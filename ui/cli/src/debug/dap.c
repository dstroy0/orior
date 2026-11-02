// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap.c: a debug adapter connected to: what it writes read on a thread of its own, each response handed to the request
// waiting on it, each event received and each request of its refused; requests sent and waited on; and the adapter
// ended
#include "../command_line/cells.h"
#include "../text/text.h"
#include "dap_internal.h"

CliDap g_cli_daps[CLI_DAPS_MOST];

// the sequence number the adapter's own requests are refused from, past any the client sends
#define CLI_DAP_REFUSED 1000000ull

CliDap *cli_dap_place_take(void)
{
    CliDap *dap = NULL;
    for (unsigned int at = 0u; (at < CLI_DAPS_MOST) && (dap == NULL); at += 1u)
    {
        if (!g_cli_daps[at].taken)
        {
            dap = &g_cli_daps[at];
            dap->taken = 1;
            dap->refs = 1u;
        }
    }
    if (dap != NULL)
    {
        dap->ended = 0;
        memor.set(&(MemoriaCfg){.dst = &dap->spawned, .val = 0, .bytes = sizeof(dap->spawned)});
        dap->socket.handle = 0ull;
        dap->wake = (CliWake)CLI_WAKE_INIT;
        dap->next = 1ull;
        memor.set(&(MemoriaCfg){.dst = dap->waiters, .val = 0, .bytes = sizeof(dap->waiters)});
        dap->complaint_size = 0ull;
        dap->reading = 0u;
    }
    return dap;
}

typedef struct
{
    const char *object;
    unsigned long long length;
    const char *name;
    char *out;
    unsigned long long room;
    CliDap *dap;
    unsigned long long seq;
    char *message;
    const char *body;
    const char *error;
    unsigned long long size;
    const char *message_read;
    const char *line;
    unsigned long long used;
} DapCtx;
// the string of a member of a JSON object of `length` bytes into `out`, sized for `room`: 1, or 0 where it has none
EMBED_INLINE int dap_member_string(const DapCtx *args)
{
    unsigned long long length = args->length;
    const char *name = args->name;
    char *out = args->out;
    unsigned long long room = args->room;
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    return EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->object, .size_value = length, .name = name,
                      .start = &at, .length_pointer = &size) &&
           EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->object + at, .length = size, .out_room = out,
                      .room = room);
}

// a response handed to the request of `seq` that waits on it: `message` with its body in it, or its error: 1 where the
// message went to the request, which gives it back
EMBED_INLINE int dap_answer(const DapCtx *args)
{
    int handed = 0;
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        CliDapWaiter *const waiter = &args->dap->waiters[at];
        if (waiter->waiting && (waiter->seq == args->seq) && !waiter->answered)
        {
            if (args->error != NULL)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = waiter->error,
                    .src = args->error,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->error, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else
            {
                waiter->answer = (CliLspAnswer){args->message, args->body, args->length};
                handed = 1;
            }
            waiter->answered = 1;
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->dap->wake);
            break;
        }
    }
    return handed;
}

// a response read: its request's sequence number, and its body where it succeeded, else its error, from the body's
// error's format or the response's message: 1 where `message` went with its answer
EMBED_INLINE int dap_response(const DapCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    unsigned long long seq = 0ull;
    int whole = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message, .size_value = size,
                           .name = "request_seq", .start = &at, .length_pointer = &length) &&
                (length > 0ull);
    for (unsigned long long letter = at; whole && (letter < at + length); letter += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->message[letter] - '0');
        whole = (args->message[letter] >= '0') && (args->message[letter] <= '9') && (seq <= (~0ull - digit) / 10ull);
        seq = whole ? (seq * 10ull) + digit : seq;
    }
    if (!whole)
    {
        return 0;
    }
    unsigned long long body = 0ull;
    unsigned long long body_size = 0ull;
    const int has_body = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message, .size_value = size,
                                    .name = "body", .start = &body, .length_pointer = &body_size);
    const int success = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message, .size_value = size,
                                   .name = "success", .start = &at, .length_pointer = &length) &&
                        (length == 4ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->message + at, .other = "true", .bytes = 4u}) == 0);
    if (success)
    {
        if (has_body)
        {
            // the body ended by a 0 where it stands: the byte after it is the message's own, read no more
            args->message[body + body_size] = '\0';
        }
        return EMBED_CALL(dap_answer, DapCtx, .dap = args->dap, .seq = seq, .message = args->message,
                          .body = has_body ? args->message + body : "null", .length = has_body ? body_size : 4ull,
                          .error = NULL);
    }
    char error[1024];
    unsigned long long inner = 0ull;
    unsigned long long inner_size = 0ull;
    const int formatted =
        has_body &&
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message + body, .size_value = body_size,
                   .name = "error", .start = &inner, .length_pointer = &inner_size) &&
        EMBED_CALL(dap_member_string, DapCtx, .object = args->message + body + inner, .length = inner_size,
                   .name = "format", .out = error, .room = sizeof(error));
    if (!formatted && !EMBED_CALL(dap_member_string, DapCtx, .object = args->message, .length = size, .name = "message",
                                  .out = error, .room = sizeof(error)))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = error,
            .src = "the adapter refused",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the adapter refused", .cap = CLI_CELLS_POOL}) + 1u});
    }
    return EMBED_CALL(dap_answer, DapCtx, .dap = args->dap, .seq = seq, .message = NULL, .body = NULL, .length = 0ull,
                      .error = error);
}

// a request of the adapter's own refused, as orior_cli runs the program through the adapter
EMBED_INLINE void dap_reject(const DapCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, args->seq, 1u);
    const int has_seq = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message_read, .size_value = size,
                                   .name = "seq", .start = &at, .length_pointer = &length);
    unsigned long long command = 0ull;
    unsigned long long command_size = 0ull;
    const int has_command =
        EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message_read, .size_value = size,
                   .name = "command", .start = &command, .length_pointer = &command_size);
    const CliLspPart sent[7] = {
        CLI_LSP_RAW("{\"seq\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW(",\"type\":\"response\",\"request_seq\":"),
        {has_seq ? args->message_read + at : "null", has_seq ? length : 4ull, 0},
        CLI_LSP_RAW(",\"success\":false,\"command\":"),
        {has_command ? args->message_read + command : "null", has_command ? command_size : 4ull, 0},
        CLI_LSP_RAW(",\"message\":\"orior_cli runs the program through the adapter\"}"),
    };
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unsent = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->dap->input, .parts = sent,
                     .count = 7u, .err_msg = &unsent);
}

// what the adapter writes read until it ends: each event received, each response handed to its request, each request of
// its refused; then every request still waiting notified the adapter stopped, and its stop received
static void dap_read(void *context)
{
    CliDap *const dap = (CliDap *)context;
    unsigned long long refused = CLI_DAP_REFUSED;
    unsigned long long size = 0ull;
    char *message = NULL;
    // each message read into a cell of its length, given back here unless it went with an answer
    while ((message = EMBED_CALL(cli_lsp_reader_size.message_take, CliLspReaderSizeCfg, .reader = &dap->reader,
                                 .size = &size, .owner = &g_cli_debug_cells)) != NULL)
    {
        int handed = 0;
        char kind[32];
        if (!EMBED_CALL(dap_member_string, DapCtx, .object = message, .length = size, .name = "type", .out = kind,
                        .room = sizeof(kind)))
        {
            kind[0] = '\0';
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "event", .cap = CLI_CELLS_POOL}))
        {
            char event[256] = "";
            if (!EMBED_CALL(dap_member_string, DapCtx, .object = message, .length = size, .name = "event", .out = event,
                            .room = sizeof(event)))
            {
                event[0] = '\0';
            }
            unsigned long long body = 0ull;
            unsigned long long body_size = 0ull;
            const int has_body = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = message, .size_value = size,
                                            .name = "body", .start = &body, .length_pointer = &body_size);
            dap->emit(dap->context, event, has_body ? message + body : "null", has_body ? body_size : 4ull);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "response", .cap = CLI_CELLS_POOL}))
        {
            handed = EMBED_CALL(dap_response, DapCtx, .dap = dap, .message = message, .size = size);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "request", .cap = CLI_CELLS_POOL}))
        {
            refused += 1ull;
            EMBED_CALL(dap_reject, DapCtx, .dap = dap, .message_read = message, .size = size, .seq = refused);
        }
        if (!handed)
        {
            g_cli_cells->persistent_buf_release(message, &g_cli_debug_cells);
        }
    }
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        if (dap->waiters[at].waiting && !dap->waiters[at].answered)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = dap->waiters[at].error,
                .src = "the adapter stopped",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the adapter stopped", .cap = CLI_CELLS_POOL}) + 1u});
            dap->waiters[at].answered = 1;
        }
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &dap->wake);
    dap->emit(dap->context, "adapterStopped", "null", 4ull);
    dap->reading -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &dap->wake);
}

// a line the program adapter wrote to its errors stored, trimmed with "\n" after it, the stored text cut to its last
// CLI_DAP_COMPLAINT bytes at a letter's start
EMBED_INLINE void dap_complaint_store(const DapCtx *args)
{
    unsigned long long start = 0ull;
    unsigned long long end = args->used;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->line[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->line[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = args->dap->complaint + args->dap->complaint_size, .src = args->line + start, .bytes = end - start});
    args->dap->complaint_size += end - start;
    args->dap->complaint[args->dap->complaint_size] = '\n';
    args->dap->complaint_size += 1ull;
    if (args->dap->complaint_size > CLI_DAP_COMPLAINT)
    {
        unsigned long long cut = args->dap->complaint_size - CLI_DAP_COMPLAINT;
        while ((cut < args->dap->complaint_size) && (((unsigned char)args->dap->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ull;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = args->dap->complaint, .src = args->dap->complaint + cut, .bytes = args->dap->complaint_size - cut});
        args->dap->complaint_size -= cut;
    }
}

// what the program adapter writes to its errors read until it ends, line by line, a line longer than the room stored in
// pieces of the room's length
static void dap_complaints_read(void *context)
{
    CliDap *const dap = (CliDap *)context;
    char line[CLI_DAP_COMPLAINT];
    unsigned long long used = 0ull;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = dap->spawned.err,
                       .piece = piece, .room = sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if ((piece[at] == '\n') || (used == sizeof(line)))
            {
                EMBED_CALL(dap_complaint_store, DapCtx, .dap = dap, .line = line, .used = used);
                used = 0ull;
            }
            if (piece[at] != '\n')
            {
                line[used] = piece[at];
                used += 1ull;
            }
        }
    }
    if (used > 0ull)
    {
        EMBED_CALL(dap_complaint_store, DapCtx, .dap = dap, .line = line, .used = used);
    }
    dap->reading -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &dap->wake);
}

void cli_dap_readers_start(const CliDapCtx *args)
{
    CliThread *const threads[2] = {&args->dap->reader_thread, &args->dap->complaint_thread};
    void (*const runs[2])(void *context) = {dap_read, dap_complaints_read};
    for (unsigned int at = 0u; at < (args->complaints ? 2u : 1u); at += 1u)
    {
        args->dap->reading += 1u;
        *threads[at] = (CliThread){runs[at], args->dap};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = threads[at]))
        {
            runs[at](args->dap);
        }
    }
}

int cli_dap_on_send(const CliDapErrMsgCommandCfg *args)
{
    unsigned long long *seq = args->seq_pointer;
    // a request with no arguments sends an empty object, which every adapter takes
    const int none = (args->length == 0ull) ||
                     ((args->length == 4ull) &&
                      (memor.cmp(&(MemoriaCfg){.src = args->arguments, .other = "null", .bytes = 4u}) == 0));
    *seq = args->dap->next;
    args->dap->next += 1ull;
    CliDapWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_DAP_WAITERS) && (waiter == NULL); at += 1u)
    {
        waiter = args->dap->waiters[at].waiting ? NULL : &args->dap->waiters[at];
    }
    if (waiter != NULL)
    {
        waiter->waiting = 1;
        waiter->seq = *seq;
        waiter->answered = 0;
        waiter->error[0] = '\0';
        waiter->answer = (CliLspAnswer){NULL, NULL, 0ull};
    }
    if (waiter == NULL)
    {
        mmgr_line_put(args->err_msg, "no more requests wait on the adapter at once");
        return 0;
    }
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, *seq, 1u);
    const CliLspPart parts[7] = {
        CLI_LSP_RAW("{\"seq\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW(",\"type\":\"request\",\"command\":\""),
        {args->command, cellul.len(&(CatenaFinitaCfg){.src = args->command, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"arguments\":"),
        {none ? "{}" : args->arguments, none ? 2ull : args->length, 0},
        CLI_LSP_RAW("}")};
    if (!EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->dap->input, .parts = parts,
                    .count = 7u, .err_msg = args->err_msg))
    {
        waiter->waiting = 0;
        return 0;
    }
    return 1;
}

int cli_dap_wait(const CliDapErrMsgCommandSeqCfg *args)
{
    CliLspAnswer *answer = args->answer;
    *answer = (CliLspAnswer){NULL, NULL, 0ull};
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    CliDapWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_DAP_WAITERS) && (waiter == NULL); at += 1u)
    {
        waiter = (args->dap->waiters[at].waiting && (args->dap->waiters[at].seq == args->seq)) ? &args->dap->waiters[at]
                                                                                               : NULL;
    }
    while ((waiter != NULL) && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->dap->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int answered = (waiter != NULL) && waiter->answered;
    const int failed = answered && (waiter->error[0] != '\0');
    if (failed)
    {
        mmgr_line_put(args->err_msg, waiter->error);
    }
    else if (answered)
    {
        *answer = waiter->answer;
        waiter->answer = (CliLspAnswer){NULL, NULL, 0ull};
    }
    if (waiter != NULL)
    {
        waiter->waiting = 0;
    }
    if (!answered)
    {
        mmgr_line_put(args->err_msg, args->command);
        mmgr_line_put(args->err_msg, " had no answer in ");
        mmgr_line_decimal(args->err_msg, args->wait / 1000ull, 1u);
        mmgr_line_put(args->err_msg, " s");
    }
    return answered && !failed;
}

int cli_dap_request(const CliDapErrMsgCfg *args)
{
    CliLspAnswer *answer = args->answer;
    *answer = (CliLspAnswer){NULL, NULL, 0ull};
    unsigned long long seq = 0ull;
    return EMBED_CALL(cli_dap_err_msg_command.send, CliDapErrMsgCommandCfg, .dap = args->dap, .command = args->command,
                      .arguments = args->arguments, .length = args->length, .seq_pointer = &seq,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(cli_dap_err_msg_command_seq.wait, CliDapErrMsgCommandSeqCfg, .dap = args->dap,
                      .command = args->command, .seq = seq, .wait = args->wait, .answer = answer,
                      .err_msg = args->err_msg);
}

void cli_dap_complaint(const CliDapCfg *args)
{
    unsigned long long end = args->dap->complaint_size;
    while ((end > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->dap->complaint[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->dap->complaint,
                       .at = start, .set = "\n");
    mmgr_line_put_n(args->out, args->dap->complaint + start, end - start);
}

void cli_dap_end(const CliDapCtx *args)
{
    if (args->dap->socket.handle != 0ull)
    {
        EMBED_CALL(cli_sock_internal.socket_end, CliSockCtx, .sock = &args->dap->socket);
    }
    if ((args->dap->kind == CLI_DAP_PROGRAM) && !args->dap->ended && (args->dap->spawned.process != 0ull))
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->dap->spawned);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->dap->spawned);
        args->dap->ended = 1;
    }
    if (args->dap->kind == CLI_DAP_WITHIN)
    {
        cli_dap_internal.within_let_go(args);
    }
}

void cli_dap_stop(const CliDapCfg *args)
{
    CliLspAnswer answer;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    const char *const arguments =
        args->dap->ends_program ? "{\"terminateDebuggee\":true}" : "{\"terminateDebuggee\":false}";
    (void)EMBED_CALL(cli_dap_err_msg.request, CliDapErrMsgCfg, .dap = args->dap, .command = "disconnect",
                     .arguments = arguments,
                     .length = cellul.len(&(CatenaFinitaCfg){.src = arguments, .cap = CLI_CELLS_POOL}), .wait = 3000ull,
                     .answer = &answer, .err_msg = &err_buf);
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_debug_cells);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    EMBED_CALL(cli_dap_internal.end, CliDapCtx, .dap = args->dap);
}

void cli_dap_ref(const CliDapCfg *args)
{
    args->dap->refs += 1u;
}

void cli_dap_unref(const CliDapCfg *args)
{
    args->dap->refs -= 1u;
    const int last = (args->dap->refs == 0u);
    if (!last)
    {
        return;
    }
    EMBED_CALL(cli_dap_internal.end, CliDapCtx, .dap = args->dap);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((args->dap->reading > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->dap->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int read = (args->dap->reading == 0u);
    if (!read)
    {
        return;
    }
    // the answers no request took, with no reader left to hand another
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        g_cli_cells->persistent_buf_release(args->dap->waiters[at].answer.message, &g_cli_debug_cells);
        args->dap->waiters[at].answer = (CliLspAnswer){NULL, NULL, 0ull};
    }
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &args->dap->socket);
    if (args->dap->kind == CLI_DAP_PROGRAM)
    {
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->dap->spawned);
    }
    args->dap->taken = 0;
}
