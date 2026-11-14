// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp.c: a running language server: what it writes read on a thread of its own, each answer handed to the request
// waiting on it and what it sends unprompted to its receiver, what it writes to its errors retained, requests and
// notifications sent, and the server ended
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "lsp_internal.h"

// the id of the request the reading thread hands to its receiver, as its JSON, and whether its receiver answers later
static _Thread_local const char *t_request_id;
static _Thread_local unsigned long long t_request_id_size;
static _Thread_local int t_later;

// a piece of what the server writes read
static unsigned long long lsp_out_read(const CliSourcePieceCapacityCfg *args)
{
    char *piece = args->piece;
    unsigned long long capacity = args->capacity;
    return EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                      .pipe_handle = ((CliLsp *)args->source)->spawned.out, .piece = piece, .capacity = capacity);
}

typedef struct
{
    const char *text;
    unsigned long long length;
    unsigned long long *value;
    CliLsp *lsp;
    unsigned long long id;
    char *message;
    const char *result;
    const char *error;
    const char *body;
    unsigned long long size;
    unsigned long long method;
    unsigned long long method_size;
    char *body_out;
    const char *line;
} LspCtx;
// whether `length` bytes are a whole number as serde reads a u64, into `value`
EMBED_INLINE int lsp_id(const LspCtx *args)
{
    unsigned long long *value = args->value;
    unsigned long long sum = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const unsigned long long digit = (unsigned long long)(args->text[at] - '0');
        if ((args->text[at] < '0') || (args->text[at] > '9') || (sum > (~0ULL - digit) / 10ULL))
        {
            return 0;
        }
        sum = (sum * 10ULL) + digit;
    }
    *value = sum;
    return args->length > 0ULL;
}

// an answer handed to the request of `id` that waits on it: `message` with its result in it, or its error: 1 where the
// message went to the request, which gives it back
EMBED_INLINE int lsp_answer(const LspCtx *args)
{
    int transferred = 0;
    for (unsigned int at = 0u; at < CLI_LSP_WAITERS; at += 1u)
    {
        CliLspWaiter *const waiter = &args->lsp->waiters[at];
        if (waiter->waiting && (waiter->id == args->id))
        {
            if (args->error != NULL)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = waiter->error,
                    .src = args->error,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->error, .cap = CLI_POOL_BYTES}) + 1u});
            }
            else
            {
                *waiter->answer = (CliLspAnswer){args->message, args->result, args->length};
                transferred = 1;
            }
            waiter->answered = 1;
            waiter->waiting = 0;
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->lsp->wake);
            break;
        }
    }
    return transferred;
}

// what the server sends unprompted received, and a request of its answered with what its receiver writes
EMBED_INLINE void lsp_server_message(const LspCtx *args)
{
    const char *body = args->body;
    unsigned long long size = args->size;
    char name[256];
    if (!EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = body + args->method,
                    .length = args->method_size, .out_capacity = name, .capacity = sizeof(name)))
    {
        return;
    }
    unsigned long long params = 0ULL;
    unsigned long long params_size = 0ULL;
    const int has_params = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size,
                                      .name = "params", .start = &params, .length_pointer = &params_size);
    unsigned long long id = 0ULL;
    unsigned long long id_size = 0ULL;
    const int has_id = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size, .name = "id",
                                  .start = &id, .length_pointer = &id_size);
    // a request's answer on a line grown to it; what the server sends unprompted is answered with nothing
    CliGrownLine answer;
    cli_grown_begin(&answer, CLI_LSP_ANSWERED, g_cli_servers_owner);
    t_request_id = has_id ? (body + id) : NULL;
    t_request_id_size = has_id ? id_size : 0ULL;
    t_later = 0;
    EMBED_CALL(args->lsp->recv, CliLspRecvCfg, .context = args->lsp->context, .method = name,
               .params = has_params ? body + params : "null", .size = has_params ? params_size : 4ULL,
               .answer = &answer.line);
    t_request_id = NULL;
    if (has_id && !t_later)
    {
        const unsigned long long answer_size = cli_grown_size(&answer);
        const int answered = mmgr_line_ok(&answer.line) && (answer_size > 0ULL);
        const CliLspPart parts[5] = {CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"id\":"),
                                     {body + id, id_size, 0},
                                     CLI_LSP_RAW(",\"result\":"),
                                     {answered ? answer.cell : "null", answered ? answer_size : 4ULL, 0},
                                     CLI_LSP_RAW("}")};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unsent = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input,
                         .parts = parts, .count = 5u, .err_msg = &unsent);
    }
    cli_grown_end(&answer);
}

// a message the server sent, `size` bytes of `body`, ended by a 0: what it sends unprompted received, an answer handed
// to its request: 1 where `body` went with the answer
EMBED_INLINE int lsp_message(const LspCtx *args)
{
    char *body = args->body_out;
    unsigned long long size = args->size;
    unsigned long long method = 0ULL;
    unsigned long long method_size = 0ULL;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size, .name = "method",
                   .start = &method, .length_pointer = &method_size) &&
        (body[method] == '"'))
    {
        EMBED_CALL(lsp_server_message, LspCtx, .lsp = args->lsp, .body = body, .size = size, .method = method,
                   .method_size = method_size);
        return 0;
    }
    unsigned long long id_at = 0ULL;
    unsigned long long id_size = 0ULL;
    unsigned long long id = 0ULL;
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size, .name = "id", .start = &id_at,
                    .length_pointer = &id_size) ||
        !EMBED_CALL(lsp_id, LspCtx, .text = body + id_at, .length = id_size, .value = &id))
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    unsigned long long length = 0ULL;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size, .name = "error", .start = &at,
                   .length_pointer = &length))
    {
        char error[1024] = "the server refused";
        unsigned long long message = 0ULL;
        unsigned long long message_size = 0ULL;
        if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body + at, .size_value = length, .name = "message",
                        .start = &message, .length_pointer = &message_size) ||
            !EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = body + at + message, .length = message_size,
                        .out_capacity = error, .capacity = sizeof(error)))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = error,
                .src = "the server refused",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the server refused", .cap = CLI_POOL_BYTES}) + 1u});
        }
        return EMBED_CALL(lsp_answer, LspCtx, .lsp = args->lsp, .id = id, .message = NULL, .result = NULL,
                          .length = 0ULL, .error = error);
    }
    const int has_result = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = body, .size_value = size,
                                      .name = "result", .start = &at, .length_pointer = &length);
    if (has_result)
    {
        // the result ended by a 0 where it stands: the byte after it is the message's own, read no more
        body[at + length] = '\0';
    }
    return EMBED_CALL(lsp_answer, LspCtx, .lsp = args->lsp, .id = id, .message = body,
                      .result = has_result ? body + at : "null", .length = has_result ? length : 4ULL, .error = NULL);
}

// what the server writes read until it ends: each message handed on, and every request still waiting notified the
// server stopped
static void lsp_read(void *context)
{
    CliLsp *const lsp = (CliLsp *)context;
    unsigned long long size = 0ULL;
    char *body = NULL;
    // each message read into a cell of its length, given back here unless it went with an answer
    while ((body = EMBED_CALL(cli_lsp_reader_size.message_alloc, CliLspReaderSizeCfg, .reader = &lsp->reader,
                              .size = &size, .owner = g_cli_servers_owner)) != NULL)
    {
        if (!EMBED_CALL(lsp_message, LspCtx, .lsp = lsp, .body_out = body, .size = size))
        {
            g_cli_guard->persistent_buf_release(body, g_cli_servers_owner);
        }
    }
    for (unsigned int at = 0u; at < CLI_LSP_WAITERS; at += 1u)
    {
        if (lsp->waiters[at].waiting)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = lsp->waiters[at].error,
                .src = "the server stopped",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the server stopped", .cap = CLI_POOL_BYTES}) + 1u});
            lsp->waiters[at].answered = 1;
            lsp->waiters[at].waiting = 0;
        }
    }
    lsp->read_state -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &lsp->wake);
}

// a line the server wrote to its errors retained, trimmed with "\n" after it, the retained text cut to its last
// CLI_LSP_COMPLAINT bytes at a letter's start
static void lsp_complaint_line(const LspCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long start = 0ULL;
    while ((start < size) && (cellul.chr(&(CatenaFinitaCfg){
                                  .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->line[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((size > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->line[size - 1ULL]}) != NULL))
    {
        size -= 1ULL;
    }
    const unsigned long long length = (size - start > CLI_LSP_COMPLAINT) ? CLI_LSP_COMPLAINT : size - start;
    if (args->lsp->complaint_size + length + 1ULL > sizeof(args->lsp->complaint))
    {
        unsigned long long cut = args->lsp->complaint_size + length + 1ULL - CLI_LSP_COMPLAINT;
        cut = (cut > args->lsp->complaint_size) ? args->lsp->complaint_size : cut;
        while ((cut < args->lsp->complaint_size) && (((unsigned char)args->lsp->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ULL;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = args->lsp->complaint, .src = args->lsp->complaint + cut, .bytes = args->lsp->complaint_size - cut});
        args->lsp->complaint_size -= cut;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = args->lsp->complaint + args->lsp->complaint_size, .src = args->line + start, .bytes = length});
    args->lsp->complaint[args->lsp->complaint_size + length] = '\n';
    args->lsp->complaint_size += length + 1ULL;
    if (args->lsp->complaint_size > CLI_LSP_COMPLAINT)
    {
        unsigned long long cut = args->lsp->complaint_size - CLI_LSP_COMPLAINT;
        while ((cut < args->lsp->complaint_size) && (((unsigned char)args->lsp->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ULL;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = args->lsp->complaint, .src = args->lsp->complaint + cut, .bytes = args->lsp->complaint_size - cut});
        args->lsp->complaint_size -= cut;
    }
}

// a line the pipe's reader cut from the server's errors, retained
static void lsp_complaint_each(void *context, char *line, unsigned long long used)
{
    EMBED_CALL(lsp_complaint_line, LspCtx, .lsp = (CliLsp *)context, .line = line, .size = used);
}

// what the server writes to its errors read until it ends, line by line
static void lsp_complaints_read(void *context)
{
    CliLsp *const lsp = (CliLsp *)context;
    char line[CLI_LSP_COMPLAINT];
    EMBED_CALL(cli_spawn_lines_internal.read, CliSpawnLinesCtx, .pipe_handle = lsp->spawned.err, .line = line,
               .capacity = sizeof(line), .each = lsp_complaint_each, .context = lsp);
    lsp->read_state -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &lsp->wake);
}

void cli_lsp_readers_start(const CliLspCtx *args)
{
    args->lsp->reader.read = lsp_out_read;
    args->lsp->reader.source = args->lsp;
    args->lsp->reader.start = 0ULL;
    args->lsp->reader.end = 0ULL;
    CliThread *const threads[2] = {&args->lsp->reader_thread, &args->lsp->complaint_thread};
    void (*const runs[2])(void *context) = {lsp_read, lsp_complaints_read};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        args->lsp->read_state += 1u;
        *threads[at] = (CliThread){runs[at], args->lsp};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = threads[at]))
        {
            runs[at](args->lsp);
        }
    }
}

int cli_lsp_notify(const CliLspErrMsgLengthCfg *args)
{
    const CliLspPart parts[5] = {
        CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"method\":\""),
        {args->method, cellul.len(&(CatenaFinitaCfg){.src = args->method, .cap = CLI_POOL_BYTES}), 1},
        CLI_LSP_RAW("\",\"params\":"),
        {args->params, args->length, 0},
        CLI_LSP_RAW("}")};
    return EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input, .parts = parts,
                      .count = 5u, .err_msg = args->err_msg);
}

int cli_lsp_request(const CliLspErrMsgLengthCfg *args)
{
    CliLspAnswer *answer = args->answer;
    *answer = (CliLspAnswer){NULL, NULL, 0ULL};
    const unsigned long long id = args->lsp->next;
    args->lsp->next += 1ULL;
    CliLspWaiter *waiter = NULL;
    {
        unsigned int at = 0u;
        while ((at < CLI_LSP_WAITERS) && (waiter == NULL))
        {
            waiter = args->lsp->waiters[at].waiting ? NULL : &args->lsp->waiters[at];
            at += 1u;
        }
    }
    if (waiter != NULL)
    {
        *waiter = (CliLspWaiter){1, id, answer, 0, ""};
    }
    if (waiter == NULL)
    {
        mmgr_line_put(args->err_msg, "no more requests wait on the server at once");
        return 0;
    }
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ULL};
    mmgr_line_decimal(&number_line, id, 1u);
    const CliLspPart parts[7] = {
        CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"id\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW(",\"method\":\""),
        {args->method, cellul.len(&(CatenaFinitaCfg){.src = args->method, .cap = CLI_POOL_BYTES}), 1},
        CLI_LSP_RAW("\",\"params\":"),
        {args->params, args->length, 0},
        CLI_LSP_RAW("}")};
    const int sent = EMBED_CALL(cli_lsp_writer_lock.send_parts, CliLspWriterLockCfg, .writer = &args->lsp->input,
                                .parts = parts, .count = 7u, .err_msg = args->err_msg);
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    while (sent && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->lsp->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int answered = waiter->answered;
    char error[1024];
    memor.cpy(&(MemoriaCfg){.dst = error, .src = waiter->error, .bytes = sizeof(error)});
    waiter->waiting = 0;
    if (!sent)
    {
        return 0;
    }
    if (!answered)
    {
        char cancel[64];
        VerbaLinea cancel_line = {cancel, sizeof(cancel), 0ULL};
        mmgr_line_put(&cancel_line, "{\"id\":");
        mmgr_line_put_n(&cancel_line, number, number_line.at);
        mmgr_line_ch(&cancel_line, '}');
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unsent = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = args->lsp,
                         .method = "$/cancelRequest", .params = cancel, .length = cancel_line.at, .err_msg = &unsent);
        mmgr_line_put(args->err_msg, args->method);
        mmgr_line_put(args->err_msg, " had no answer in ");
        mmgr_line_decimal(args->err_msg, args->wait / 1000ULL, 1u);
        mmgr_line_put(args->err_msg, " s");
        return 0;
    }
    if (error[0] != '\0')
    {
        mmgr_line_put(args->err_msg, error);
        return 0;
    }
    return 1;
}

int cli_lsp_answer_later(const CliLspCfg *args)
{
    if ((t_request_id == NULL) || ((t_request_id_size + 1ULL) > args->capacity))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->id, .src = t_request_id, .bytes = t_request_id_size});
    args->id[t_request_id_size] = '\0';
    t_later = 1;
    return 1;
}

const char *cli_lsp_capabilities(const CliLspSizeTypeCfg *args)
{
    unsigned long long *size = args->size;
    *size = (args->lsp_read->capabilities != NULL) ? args->lsp_read->capabilities_size : 0ULL;
    if ((args->out == NULL) || (args->capacity == 0ULL))
    {
        return "";
    }
    // the text is its first bytes where it passes the caller's capacity
    const unsigned long long copied = (*size < args->capacity) ? *size : (args->capacity - 1ULL);
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->lsp_read->capabilities, .offset = 0ULL, .dst = args->out, .bytes = copied});
    }
    args->out[copied] = '\0';
    return args->out;
}

unsigned long long cli_lsp_pid(const CliLspSizeTypeCfg *args)
{
    return args->lsp_read->spawned.id;
}

int cli_lsp_alive(const CliLspContextCfg *args)
{
    long long code = 0LL;
    if (!args->lsp->ended &&
        EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &args->lsp->spawned, .code = &code))
    {
        args->lsp->ended = 1;
    }
    const int alive = !args->lsp->ended;
    return alive;
}

void cli_lsp_complaint(const CliLspPathOutCfg *args)
{
    unsigned long long end = args->lsp->complaint_size;
    while ((end > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->lsp->complaint[end - 1ULL]}) != NULL))
    {
        end -= 1ULL;
    }
    unsigned long long start = end;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->lsp->complaint,
                       .at = start, .set = "\n");
    mmgr_line_put_n(args->out, args->lsp->complaint + start, end - start);
}

void cli_lsp_end(const CliLspCtx *args)
{
    if (!args->lsp->ended && (args->lsp->spawned.process != 0ULL))
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &args->lsp->spawned);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &args->lsp->spawned);
        args->lsp->ended = 1;
    }
}

void cli_lsp_stop(const CliLspContextCfg *args)
{
    CliLspAnswer answer;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = args->lsp, .method = "shutdown",
                     .params = "null", .length = 4ULL, .wait = 2000ULL, .answer = &answer, .err_msg = &err_buf);
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
    (void)EMBED_CALL(cli_lsp_err_msg_length.notify, CliLspErrMsgLengthCfg, .lsp = args->lsp, .method = "exit",
                     .params = "null", .length = 4ULL, .err_msg = &err_buf);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    EMBED_CALL(cli_lsp_internal.end, CliLspCtx, .lsp = args->lsp);
}

void cli_lsp_ref(const CliLspContextCfg *args)
{
    args->lsp->refs += 1u;
}

void cli_lsp_unref(const CliLspContextCfg *args)
{
    args->lsp->refs -= 1u;
    const int last = (args->lsp->refs == 0u);
    if (!last)
    {
        return;
    }
    // a server let go is ended, as its process is with it; its place is given back once its readers end
    EMBED_CALL(cli_lsp_internal.end, CliLspCtx, .lsp = args->lsp);
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &args->lsp->spawned);
    const unsigned long long until = cli_clock_milliseconds() + 5000ULL;
    while ((args->lsp->read_state > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->lsp->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int read = (args->lsp->read_state == 0u);
    EMBED_CALL(cli_lsp_type_internal.files_let_go, CliLspTypeCtx, .lsp_read = args->lsp);
    if (!read)
    {
        return;
    }
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &args->lsp->spawned);
    if (args->lsp->capabilities != NULL)
    {
        (void)g_cli_guard->movable_buf_release(args->lsp->capabilities, g_cli_servers_owner);
    }
    args->lsp->reserved = 0;
}
