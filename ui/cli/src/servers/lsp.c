// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp.c: a running language server: what it writes read on a thread of its own, each answer handed to the request
// waiting on it and what it sends unprompted to its receiver, what it writes to its errors retained, requests and
// notifications sent, and the server ended
#include "../command_line/cells.h"
#include "../text/text.h"
#include "lsp_internal.h"

CliLock g_cli_lsps_lock = CLI_LOCK_INIT;

// the id of the request the reading thread hands to its receiver, as its JSON, and whether its receiver answers later
static _Thread_local const char *t_request_id;
static _Thread_local unsigned long long t_request_id_size;
static _Thread_local int t_later;

// a piece of what the server writes read
static unsigned long long lsp_out_read(void *source, char *piece, unsigned long long room)
{
    return cli_spawn_read(((CliLsp *)source)->spawned.out, piece, room);
}

// whether `length` bytes are a whole number as serde reads a u64, into `value`
static int lsp_id(const char *text, unsigned long long length, unsigned long long *value)
{
    unsigned long long sum = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if ((text[at] < '0') || (text[at] > '9') || (sum > (~0ull - digit) / 10ull))
        {
            return 0;
        }
        sum = (sum * 10ull) + digit;
    }
    *value = sum;
    return length > 0ull;
}

// an answer handed to the request of `id` that waits on it: `message` with its result in it, or its error: 1 where the
// message went to the request, which gives it back
static int lsp_answered(CliLsp *lsp, unsigned long long id, char *message, const char *result,
                        unsigned long long length, const char *error)
{
    int handed = 0;
    cli_lock_take(&lsp->lock);
    for (unsigned int at = 0u; at < CLI_LSP_WAITERS; at += 1u)
    {
        CliLspWaiter *const waiter = &lsp->waiters[at];
        if (waiter->waiting && (waiter->id == id))
        {
            if (error != NULL)
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = waiter->error,
                                  .src = error,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = error, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else
            {
                *waiter->answer = (CliLspAnswer){message, result, length};
                handed = 1;
            }
            waiter->answered = 1;
            waiter->waiting = 0;
            cli_wake_all(&lsp->wake);
            break;
        }
    }
    cli_lock_give(&lsp->lock);
    return handed;
}

// what the server sends unprompted received, and a request of its answered with what its receiver writes
static void lsp_server_message(CliLsp *lsp, const char *body, unsigned long long size, unsigned long long method,
                               unsigned long long method_size)
{
    char name[256];
    if (!cli_lsp_string(body + method, method_size, name, sizeof(name)))
    {
        return;
    }
    unsigned long long params = 0ull;
    unsigned long long params_size = 0ull;
    const int has_params = cli_lsp_member(body, size, "params", &params, &params_size);
    unsigned long long id = 0ull;
    unsigned long long id_size = 0ull;
    const int has_id = cli_lsp_member(body, size, "id", &id, &id_size);
    // a request's answer in a cell of the most it holds; what the server sends unprompted is answered with nothing
    char *const answer_text = has_id ? g_cli_cells->temporary_buf_alloc(CLI_LSP_ANSWERED, &g_cli_servers_cells) : NULL;
    VerbaLinea answer = mmgr_line_over(answer_text, CLI_LSP_ANSWERED);
    t_request_id = has_id ? (body + id) : NULL;
    t_request_id_size = has_id ? id_size : 0ull;
    t_later = 0;
    lsp->recv(lsp->context, name, has_params ? body + params : "null", has_params ? params_size : 4ull, &answer);
    t_request_id = NULL;
    if (has_id && !t_later)
    {
        const int answered = mmgr_line_ok(&answer) && (answer.at > 0ull);
        const CliLspPart parts[5] = {CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"id\":"),
                                     {body + id, id_size, 0},
                                     CLI_LSP_RAW(",\"result\":"),
                                     {answered ? answer.out : "null", answered ? answer.at : 4ull, 0},
                                     CLI_LSP_RAW("}")};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unsent = {why, sizeof(why), 0ull};
        (void)cli_lsp_send_parts(&lsp->input, &lsp->input_lock, parts, 5u, &unsent);
    }
    g_cli_cells->persistent_buf_release(answer_text, &g_cli_servers_cells);
}

// a message the server sent, `size` bytes of `body`, ended by a 0: what it sends unprompted received, an answer handed
// to its request: 1 where `body` went with the answer
static int lsp_message(CliLsp *lsp, char *body, unsigned long long size)
{
    unsigned long long method = 0ull;
    unsigned long long method_size = 0ull;
    if (cli_lsp_member(body, size, "method", &method, &method_size) && (body[method] == '"'))
    {
        lsp_server_message(lsp, body, size, method, method_size);
        return 0;
    }
    unsigned long long id_at = 0ull;
    unsigned long long id_size = 0ull;
    unsigned long long id = 0ull;
    if (!cli_lsp_member(body, size, "id", &id_at, &id_size) || !lsp_id(body + id_at, id_size, &id))
    {
        return 0;
    }
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (cli_lsp_member(body, size, "error", &at, &length))
    {
        char error[1024] = "the server refused";
        unsigned long long message = 0ull;
        unsigned long long message_size = 0ull;
        if (!cli_lsp_member(body + at, length, "message", &message, &message_size) ||
            !cli_lsp_string(body + at + message, message_size, error, sizeof(error)))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = error,
                .src = "the server refused",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the server refused", .cap = CLI_CELLS_POOL}) + 1u});
        }
        return lsp_answered(lsp, id, NULL, NULL, 0ull, error);
    }
    const int has_result = cli_lsp_member(body, size, "result", &at, &length);
    if (has_result)
    {
        // the result ended by a 0 where it stands: the byte after it is the message's own, read no more
        body[at + length] = '\0';
    }
    return lsp_answered(lsp, id, body, has_result ? body + at : "null", has_result ? length : 4ull, NULL);
}

// what the server writes read until it ends: each message handed on, and every request still waiting notified the
// server stopped
static void lsp_read(void *context)
{
    CliLsp *const lsp = (CliLsp *)context;
    unsigned long long size = 0ull;
    char *body = NULL;
    // each message read into a cell of its length, given back here unless it went with an answer
    while ((body = cli_lsp_message_taken(&lsp->reader, &size, &g_cli_servers_cells)) != NULL)
    {
        if (!lsp_message(lsp, body, size))
        {
            g_cli_cells->persistent_buf_release(body, &g_cli_servers_cells);
        }
    }
    cli_lock_take(&lsp->lock);
    for (unsigned int at = 0u; at < CLI_LSP_WAITERS; at += 1u)
    {
        if (lsp->waiters[at].waiting)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = lsp->waiters[at].error,
                .src = "the server stopped",
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the server stopped", .cap = CLI_CELLS_POOL}) + 1u});
            lsp->waiters[at].answered = 1;
            lsp->waiters[at].waiting = 0;
        }
    }
    lsp->reading -= 1u;
    cli_wake_all(&lsp->wake);
    cli_lock_give(&lsp->lock);
}

// a line the server wrote to its errors retained, trimmed with "\n" after it, the retained text cut to its last
// CLI_LSP_COMPLAINT bytes at a letter's start
static void lsp_complaint_line(CliLsp *lsp, const char *line, unsigned long long size)
{
    unsigned long long start = 0ull;
    while ((start < size) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = line[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((size > start) && (cellul.chr(&(CatenaFinitaCfg){
                                  .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = line[size - 1ull]}) != NULL))
    {
        size -= 1ull;
    }
    const unsigned long long length = (size - start > CLI_LSP_COMPLAINT) ? CLI_LSP_COMPLAINT : size - start;
    cli_lock_take(&lsp->lock);
    if (lsp->complaint_size + length + 1ull > sizeof(lsp->complaint))
    {
        unsigned long long cut = lsp->complaint_size + length + 1ull - CLI_LSP_COMPLAINT;
        cut = (cut > lsp->complaint_size) ? lsp->complaint_size : cut;
        while ((cut < lsp->complaint_size) && (((unsigned char)lsp->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ull;
        }
        memor.move_down(
            &(MemoriaCfg){.dst = lsp->complaint, .src = lsp->complaint + cut, .bytes = lsp->complaint_size - cut});
        lsp->complaint_size -= cut;
    }
    memor.cpy(&(MemoriaCfg){.dst = lsp->complaint + lsp->complaint_size, .src = line + start, .bytes = length});
    lsp->complaint[lsp->complaint_size + length] = '\n';
    lsp->complaint_size += length + 1ull;
    if (lsp->complaint_size > CLI_LSP_COMPLAINT)
    {
        unsigned long long cut = lsp->complaint_size - CLI_LSP_COMPLAINT;
        while ((cut < lsp->complaint_size) && (((unsigned char)lsp->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ull;
        }
        memor.move_down(
            &(MemoriaCfg){.dst = lsp->complaint, .src = lsp->complaint + cut, .bytes = lsp->complaint_size - cut});
        lsp->complaint_size -= cut;
    }
    cli_lock_give(&lsp->lock);
}

// what the server writes to its errors read until it ends, line by line
static void lsp_complaints_read(void *context)
{
    CliLsp *const lsp = (CliLsp *)context;
    char line[CLI_LSP_COMPLAINT];
    unsigned long long used = 0ull;
    char piece[4096];
    for (;;)
    {
        const unsigned long long count = cli_spawn_read(lsp->spawned.err, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if ((piece[at] == '\n') || (used == sizeof(line)))
            {
                lsp_complaint_line(lsp, line, used);
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
        lsp_complaint_line(lsp, line, used);
    }
    cli_lock_take(&lsp->lock);
    lsp->reading -= 1u;
    cli_wake_all(&lsp->wake);
    cli_lock_give(&lsp->lock);
}

void cli_lsp_readers_started(CliLsp *lsp)
{
    lsp->reader.read = lsp_out_read;
    lsp->reader.source = lsp;
    lsp->reader.start = 0ull;
    lsp->reader.end = 0ull;
    CliThread *const threads[2] = {&lsp->reader_thread, &lsp->complaint_thread};
    void (*const runs[2])(void *context) = {lsp_read, lsp_complaints_read};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        cli_lock_take(&lsp->lock);
        lsp->reading += 1u;
        cli_lock_give(&lsp->lock);
        *threads[at] = (CliThread){runs[at], lsp};
        if (!cli_thread_started(threads[at]))
        {
            runs[at](lsp);
        }
    }
}

int cli_lsp_notified(CliLsp *lsp, const char *method, const char *params, unsigned long long length,
                     VerbaLinea *err_msg)
{
    const CliLspPart parts[5] = {CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"method\":\""),
                                 {method, cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}), 1},
                                 CLI_LSP_RAW("\",\"params\":"),
                                 {params, length, 0},
                                 CLI_LSP_RAW("}")};
    return cli_lsp_send_parts(&lsp->input, &lsp->input_lock, parts, 5u, err_msg);
}

int cli_lsp_requested(CliLsp *lsp, const char *method, const char *params, unsigned long long length,
                      unsigned long long wait, CliLspAnswer *answer, VerbaLinea *err_msg)
{
    *answer = (CliLspAnswer){NULL, NULL, 0ull};
    cli_lock_take(&lsp->lock);
    const unsigned long long id = lsp->next;
    lsp->next += 1ull;
    CliLspWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_LSP_WAITERS) && (waiter == NULL); at += 1u)
    {
        waiter = lsp->waiters[at].waiting ? NULL : &lsp->waiters[at];
    }
    if (waiter != NULL)
    {
        *waiter = (CliLspWaiter){1, id, answer, 0, ""};
    }
    cli_lock_give(&lsp->lock);
    if (waiter == NULL)
    {
        mmgr_line_put(err_msg, "no more requests wait on the server at once");
        return 0;
    }
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, id, 1u);
    const CliLspPart parts[7] = {CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"id\":"),
                                 {number, number_line.at, 0},
                                 CLI_LSP_RAW(",\"method\":\""),
                                 {method, cellul.len(&(CatenaFinitaCfg){.src = method, .cap = CLI_CELLS_POOL}), 1},
                                 CLI_LSP_RAW("\",\"params\":"),
                                 {params, length, 0},
                                 CLI_LSP_RAW("}")};
    const int sent = cli_lsp_send_parts(&lsp->input, &lsp->input_lock, parts, 7u, err_msg);
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&lsp->lock);
    while (sent && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&lsp->wake, &lsp->lock, until - cli_clock_milliseconds());
    }
    const int answered = waiter->answered;
    char error[1024];
    memor.cpy(&(MemoriaCfg){.dst = error, .src = waiter->error, .bytes = sizeof(error)});
    waiter->waiting = 0;
    cli_lock_give(&lsp->lock);
    if (!sent)
    {
        return 0;
    }
    if (!answered)
    {
        char cancel[64];
        VerbaLinea cancel_line = {cancel, sizeof(cancel), 0ull};
        mmgr_line_put(&cancel_line, "{\"id\":");
        mmgr_line_put_n(&cancel_line, number, number_line.at);
        mmgr_line_ch(&cancel_line, '}');
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unsent = {why, sizeof(why), 0ull};
        (void)cli_lsp_notified(lsp, "$/cancelRequest", cancel, cancel_line.at, &unsent);
        mmgr_line_put(err_msg, method);
        mmgr_line_put(err_msg, " had no answer in ");
        mmgr_line_decimal(err_msg, wait / 1000ull, 1u);
        mmgr_line_put(err_msg, " s");
        return 0;
    }
    if (error[0] != '\0')
    {
        mmgr_line_put(err_msg, error);
        return 0;
    }
    return 1;
}

int cli_lsp_answer_later(char *id, unsigned long long room)
{
    if ((t_request_id == NULL) || ((t_request_id_size + 1ull) > room))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = id, .src = t_request_id, .bytes = t_request_id_size});
    id[t_request_id_size] = '\0';
    t_later = 1;
    return 1;
}

int cli_lsp_answer(CliLsp *lsp, const char *id, const char *result, unsigned long long size, VerbaLinea *err_msg)
{
    const CliLspPart parts[5] = {CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"id\":"),
                                 {id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}), 0},
                                 CLI_LSP_RAW(",\"result\":"),
                                 {result, size, 0},
                                 CLI_LSP_RAW("}")};
    return cli_lsp_send_parts(&lsp->input, &lsp->input_lock, parts, 5u, err_msg);
}

const char *cli_lsp_capabilities(const CliLsp *lsp, unsigned long long *size)
{
    *size = (lsp->capabilities != NULL) ? lsp->capabilities_size : 0ull;
    return (lsp->capabilities != NULL) ? lsp->capabilities : "";
}

unsigned long long cli_lsp_pid(const CliLsp *lsp)
{
    return lsp->spawned.id;
}

int cli_lsp_alive(CliLsp *lsp)
{
    cli_lock_take(&lsp->lock);
    long long code = 0ll;
    if (!lsp->ended && cli_spawn_poll(&lsp->spawned, &code))
    {
        lsp->ended = 1;
    }
    const int alive = !lsp->ended;
    cli_lock_give(&lsp->lock);
    return alive;
}

void cli_lsp_complaint(CliLsp *lsp, VerbaLinea *out)
{
    cli_lock_take(&lsp->lock);
    unsigned long long end = lsp->complaint_size;
    while ((end > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = lsp->complaint[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    start = cli_text_back_any(lsp->complaint, start, "\n");
    mmgr_line_put_n(out, lsp->complaint + start, end - start);
    cli_lock_give(&lsp->lock);
}

void cli_lsp_ended(CliLsp *lsp)
{
    cli_lock_take(&lsp->lock);
    if (!lsp->ended && (lsp->spawned.process != 0ull))
    {
        cli_spawn_killed(&lsp->spawned);
        (void)cli_spawn_wait(&lsp->spawned);
        lsp->ended = 1;
    }
    cli_lock_give(&lsp->lock);
}

void cli_lsp_stopped(CliLsp *lsp)
{
    CliLspAnswer answer;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    (void)cli_lsp_requested(lsp, "shutdown", "null", 4ull, 2000ull, &answer, &err_buf);
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    (void)cli_lsp_notified(lsp, "exit", "null", 4ull, &err_buf);
    query_paused(100u);
    cli_lsp_ended(lsp);
}

void cli_lsp_ref(CliLsp *lsp)
{
    cli_lock_take(&g_cli_lsps_lock);
    lsp->refs += 1u;
    cli_lock_give(&g_cli_lsps_lock);
}

void cli_lsp_unref(CliLsp *lsp)
{
    cli_lock_take(&g_cli_lsps_lock);
    lsp->refs -= 1u;
    const int last = (lsp->refs == 0u);
    cli_lock_give(&g_cli_lsps_lock);
    if (!last)
    {
        return;
    }
    // a server let go is ended, as its process is with it; its place is given back once its readers end
    cli_lsp_ended(lsp);
    cli_spawn_input_closed(&lsp->spawned);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    cli_lock_take(&lsp->lock);
    while ((lsp->reading > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&lsp->wake, &lsp->lock, until - cli_clock_milliseconds());
    }
    const int read = (lsp->reading == 0u);
    cli_lock_give(&lsp->lock);
    cli_lsp_files_let_go(lsp);
    if (!read)
    {
        return;
    }
    cli_spawn_closed(&lsp->spawned);
    g_cli_cells->persistent_buf_release(lsp->capabilities, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(lsp, &g_cli_servers_cells);
}
