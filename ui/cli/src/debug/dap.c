// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap.c: a debug adapter connected to: what it writes read on a thread of its own, each response handed to the request
// waiting on it, each event received and each request of its refused; requests sent and waited on; and the adapter
// ended
#include "../command_line/cells.h"
#include "../text/text.h"
#include "dap_internal.h"

CliDap g_cli_daps[CLI_DAPS_MOST];
CliLock g_cli_daps_lock = CLI_LOCK_INIT;

// the sequence number the adapter's own requests are refused from, past any the client sends
#define CLI_DAP_REFUSED 1000000ull

CliDap *cli_dap_place_taken(void)
{
    CliDap *dap = NULL;
    cli_lock_take(&g_cli_daps_lock);
    for (unsigned int at = 0u; (at < CLI_DAPS_MOST) && (dap == NULL); at += 1u)
    {
        if (!g_cli_daps[at].taken)
        {
            dap = &g_cli_daps[at];
            dap->taken = 1;
            dap->refs = 1u;
        }
    }
    cli_lock_give(&g_cli_daps_lock);
    if (dap != NULL)
    {
        dap->ended = 0;
        memor.set(&(MemoriaCfg){.dst = &dap->spawned, .val = 0, .bytes = sizeof(dap->spawned)});
        dap->socket.handle = 0ull;
        dap->input_lock = (CliLock)CLI_LOCK_INIT;
        dap->lock = (CliLock)CLI_LOCK_INIT;
        dap->wake = (CliWake)CLI_WAKE_INIT;
        dap->next = 1ull;
        memor.set(&(MemoriaCfg){.dst = dap->waiters, .val = 0, .bytes = sizeof(dap->waiters)});
        dap->complaint_size = 0ull;
        dap->reading = 0u;
    }
    return dap;
}

// the string of a member of a JSON object of `length` bytes into `out`, sized for `room`: 1, or 0 where it has none
static int dap_member_string(const char *object, unsigned long long length, const char *name, char *out,
                             unsigned long long room)
{
    unsigned long long at = 0ull;
    unsigned long long size = 0ull;
    return cli_lsp_member(object, length, name, &at, &size) && cli_lsp_string(object + at, size, out, room);
}

// a response handed to the request of `seq` that waits on it: `message` with its body in it, or its error: 1 where the
// message went to the request, which gives it back
static int dap_answered(CliDap *dap, unsigned long long seq, char *message, const char *body, unsigned long long length,
                        const char *error)
{
    int handed = 0;
    cli_lock_take(&dap->lock);
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        CliDapWaiter *const waiter = &dap->waiters[at];
        if (waiter->waiting && (waiter->seq == seq) && !waiter->answered)
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
                waiter->answer = (CliLspAnswer){message, body, length};
                handed = 1;
            }
            waiter->answered = 1;
            cli_wake_all(&dap->wake);
            break;
        }
    }
    cli_lock_give(&dap->lock);
    return handed;
}

// a response read: its request's sequence number, and its body where it succeeded, else its error, from the body's
// error's format or the response's message: 1 where `message` went with its answer
static int dap_response(CliDap *dap, char *message, unsigned long long size)
{
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    unsigned long long seq = 0ull;
    int whole = cli_lsp_member(message, size, "request_seq", &at, &length) && (length > 0ull);
    for (unsigned long long letter = at; whole && (letter < at + length); letter += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(message[letter] - '0');
        whole = (message[letter] >= '0') && (message[letter] <= '9') && (seq <= (~0ull - digit) / 10ull);
        seq = whole ? (seq * 10ull) + digit : seq;
    }
    if (!whole)
    {
        return 0;
    }
    unsigned long long body = 0ull;
    unsigned long long body_size = 0ull;
    const int has_body = cli_lsp_member(message, size, "body", &body, &body_size);
    const int success = cli_lsp_member(message, size, "success", &at, &length) && (length == 4ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = message + at, .other = "true", .bytes = 4u}) == 0);
    if (success)
    {
        if (has_body)
        {
            // the body ended by a 0 where it stands: the byte after it is the message's own, read no more
            message[body + body_size] = '\0';
        }
        return dap_answered(dap, seq, message, has_body ? message + body : "null", has_body ? body_size : 4ull, NULL);
    }
    char error[1024];
    unsigned long long inner = 0ull;
    unsigned long long inner_size = 0ull;
    const int formatted = has_body && cli_lsp_member(message + body, body_size, "error", &inner, &inner_size) &&
                          dap_member_string(message + body + inner, inner_size, "format", error, sizeof(error));
    if (!formatted && !dap_member_string(message, size, "message", error, sizeof(error)))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = error,
            .src = "the adapter refused",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "the adapter refused", .cap = CLI_CELLS_POOL}) + 1u});
    }
    return dap_answered(dap, seq, NULL, NULL, 0ull, error);
}

// a request of the adapter's own refused, as orior_cli runs the program through the adapter
static void dap_refused(CliDap *dap, const char *message, unsigned long long size, unsigned long long seq)
{
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, seq, 1u);
    const int has_seq = cli_lsp_member(message, size, "seq", &at, &length);
    unsigned long long command = 0ull;
    unsigned long long command_size = 0ull;
    const int has_command = cli_lsp_member(message, size, "command", &command, &command_size);
    const CliLspPart sent[7] = {
        CLI_LSP_RAW("{\"seq\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW(",\"type\":\"response\",\"request_seq\":"),
        {has_seq ? message + at : "null", has_seq ? length : 4ull, 0},
        CLI_LSP_RAW(",\"success\":false,\"command\":"),
        {has_command ? message + command : "null", has_command ? command_size : 4ull, 0},
        CLI_LSP_RAW(",\"message\":\"orior_cli runs the program through the adapter\"}"),
    };
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unsent = {why, sizeof(why), 0ull};
    (void)cli_lsp_send_parts(&dap->input, &dap->input_lock, sent, 7u, &unsent);
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
    while ((message = cli_lsp_message_taken(&dap->reader, &size, &g_cli_debug_cells)) != NULL)
    {
        int handed = 0;
        char kind[32];
        if (!dap_member_string(message, size, "type", kind, sizeof(kind)))
        {
            kind[0] = '\0';
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "event", .cap = CLI_CELLS_POOL}))
        {
            char event[256] = "";
            if (!dap_member_string(message, size, "event", event, sizeof(event)))
            {
                event[0] = '\0';
            }
            unsigned long long body = 0ull;
            unsigned long long body_size = 0ull;
            const int has_body = cli_lsp_member(message, size, "body", &body, &body_size);
            dap->emit(dap->context, event, has_body ? message + body : "null", has_body ? body_size : 4ull);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "response", .cap = CLI_CELLS_POOL}))
        {
            handed = dap_response(dap, message, size);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "request", .cap = CLI_CELLS_POOL}))
        {
            refused += 1ull;
            dap_refused(dap, message, size, refused);
        }
        if (!handed)
        {
            g_cli_cells->persistent_buf_release(message, &g_cli_debug_cells);
        }
    }
    cli_lock_take(&dap->lock);
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
    cli_wake_all(&dap->wake);
    cli_lock_give(&dap->lock);
    dap->emit(dap->context, "adapterStopped", "null", 4ull);
    cli_lock_take(&dap->lock);
    dap->reading -= 1u;
    cli_wake_all(&dap->wake);
    cli_lock_give(&dap->lock);
}

// a line the program adapter wrote to its errors stored, trimmed with "\n" after it, the stored text cut to its last
// CLI_DAP_COMPLAINT bytes at a letter's start
static void dap_complaint_stored(CliDap *dap, const char *line, unsigned long long used)
{
    unsigned long long start = 0ull;
    unsigned long long end = used;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = line[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = line[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    cli_lock_take(&dap->lock);
    memor.cpy(&(MemoriaCfg){.dst = dap->complaint + dap->complaint_size, .src = line + start, .bytes = end - start});
    dap->complaint_size += end - start;
    dap->complaint[dap->complaint_size] = '\n';
    dap->complaint_size += 1ull;
    if (dap->complaint_size > CLI_DAP_COMPLAINT)
    {
        unsigned long long cut = dap->complaint_size - CLI_DAP_COMPLAINT;
        while ((cut < dap->complaint_size) && (((unsigned char)dap->complaint[cut] & 0xc0u) == 0x80u))
        {
            cut += 1ull;
        }
        memor.move_down(
            &(MemoriaCfg){.dst = dap->complaint, .src = dap->complaint + cut, .bytes = dap->complaint_size - cut});
        dap->complaint_size -= cut;
    }
    cli_lock_give(&dap->lock);
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
        const unsigned long long count = cli_spawn_read(dap->spawned.err, piece, sizeof(piece));
        if (count == 0ull)
        {
            break;
        }
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            if ((piece[at] == '\n') || (used == sizeof(line)))
            {
                dap_complaint_stored(dap, line, used);
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
        dap_complaint_stored(dap, line, used);
    }
    cli_lock_take(&dap->lock);
    dap->reading -= 1u;
    cli_wake_all(&dap->wake);
    cli_lock_give(&dap->lock);
}

void cli_dap_readers_started(CliDap *dap, int complaints)
{
    CliThread *const threads[2] = {&dap->reader_thread, &dap->complaint_thread};
    void (*const runs[2])(void *context) = {dap_read, dap_complaints_read};
    for (unsigned int at = 0u; at < (complaints ? 2u : 1u); at += 1u)
    {
        cli_lock_take(&dap->lock);
        dap->reading += 1u;
        cli_lock_give(&dap->lock);
        *threads[at] = (CliThread){runs[at], dap};
        if (!cli_thread_started(threads[at]))
        {
            runs[at](dap);
        }
    }
}

int cli_dap_sent(CliDap *dap, const char *command, const char *arguments, unsigned long long length,
                 unsigned long long *seq, VerbaLinea *err_msg)
{
    // a request with no arguments sends an empty object, which every adapter takes
    const int none =
        (length == 0ull) ||
        ((length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = arguments, .other = "null", .bytes = 4u}) == 0));
    cli_lock_take(&dap->lock);
    *seq = dap->next;
    dap->next += 1ull;
    CliDapWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_DAP_WAITERS) && (waiter == NULL); at += 1u)
    {
        waiter = dap->waiters[at].waiting ? NULL : &dap->waiters[at];
    }
    if (waiter != NULL)
    {
        waiter->waiting = 1;
        waiter->seq = *seq;
        waiter->answered = 0;
        waiter->error[0] = '\0';
        waiter->answer = (CliLspAnswer){NULL, NULL, 0ull};
    }
    cli_lock_give(&dap->lock);
    if (waiter == NULL)
    {
        mmgr_line_put(err_msg, "no more requests wait on the adapter at once");
        return 0;
    }
    char number[24];
    VerbaLinea number_line = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&number_line, *seq, 1u);
    const CliLspPart parts[7] = {CLI_LSP_RAW("{\"seq\":"),
                                 {number, number_line.at, 0},
                                 CLI_LSP_RAW(",\"type\":\"request\",\"command\":\""),
                                 {command, cellul.len(&(CatenaFinitaCfg){.src = command, .cap = CLI_CELLS_POOL}), 1},
                                 CLI_LSP_RAW("\",\"arguments\":"),
                                 {none ? "{}" : arguments, none ? 2ull : length, 0},
                                 CLI_LSP_RAW("}")};
    if (!cli_lsp_send_parts(&dap->input, &dap->input_lock, parts, 7u, err_msg))
    {
        cli_lock_take(&dap->lock);
        waiter->waiting = 0;
        cli_lock_give(&dap->lock);
        return 0;
    }
    return 1;
}

int cli_dap_waited(CliDap *dap, const char *command, unsigned long long seq, unsigned long long wait,
                   CliLspAnswer *answer, VerbaLinea *err_msg)
{
    *answer = (CliLspAnswer){NULL, NULL, 0ull};
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&dap->lock);
    CliDapWaiter *waiter = NULL;
    for (unsigned int at = 0u; (at < CLI_DAP_WAITERS) && (waiter == NULL); at += 1u)
    {
        waiter = (dap->waiters[at].waiting && (dap->waiters[at].seq == seq)) ? &dap->waiters[at] : NULL;
    }
    while ((waiter != NULL) && !waiter->answered && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&dap->wake, &dap->lock, until - cli_clock_milliseconds());
    }
    const int answered = (waiter != NULL) && waiter->answered;
    const int failed = answered && (waiter->error[0] != '\0');
    if (failed)
    {
        mmgr_line_put(err_msg, waiter->error);
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
    cli_lock_give(&dap->lock);
    if (!answered)
    {
        mmgr_line_put(err_msg, command);
        mmgr_line_put(err_msg, " had no answer in ");
        mmgr_line_decimal(err_msg, wait / 1000ull, 1u);
        mmgr_line_put(err_msg, " s");
    }
    return answered && !failed;
}

int cli_dap_requested(CliDap *dap, const char *command, const char *arguments, unsigned long long length,
                      unsigned long long wait, CliLspAnswer *answer, VerbaLinea *err_msg)
{
    *answer = (CliLspAnswer){NULL, NULL, 0ull};
    unsigned long long seq = 0ull;
    return cli_dap_sent(dap, command, arguments, length, &seq, err_msg) &&
           cli_dap_waited(dap, command, seq, wait, answer, err_msg);
}

void cli_dap_complaint(CliDap *dap, VerbaLinea *out)
{
    cli_lock_take(&dap->lock);
    unsigned long long end = dap->complaint_size;
    while ((end > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = dap->complaint[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    unsigned long long start = end;
    start = cli_text_back_any(dap->complaint, start, "\n");
    mmgr_line_put_n(out, dap->complaint + start, end - start);
    cli_lock_give(&dap->lock);
}

void cli_dap_ended(CliDap *dap)
{
    if (dap->socket.handle != 0ull)
    {
        cli_socket_ended(&dap->socket);
    }
    cli_lock_take(&dap->lock);
    if ((dap->kind == CLI_DAP_PROGRAM) && !dap->ended && (dap->spawned.process != 0ull))
    {
        cli_spawn_killed(&dap->spawned);
        (void)cli_spawn_wait(&dap->spawned);
        dap->ended = 1;
    }
    cli_lock_give(&dap->lock);
    if (dap->kind == CLI_DAP_WITHIN)
    {
        cli_dap_within_let_go(dap);
    }
}

void cli_dap_stopped(CliDap *dap)
{
    CliLspAnswer answer;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    const char *const arguments = dap->ends_program ? "{\"terminateDebuggee\":true}" : "{\"terminateDebuggee\":false}";
    (void)cli_dap_requested(dap, "disconnect", arguments,
                            cellul.len(&(CatenaFinitaCfg){.src = arguments, .cap = CLI_CELLS_POOL}), 3000ull, &answer,
                            &err_buf);
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_debug_cells);
    query_paused(100u);
    cli_dap_ended(dap);
}

void cli_dap_ref(CliDap *dap)
{
    cli_lock_take(&g_cli_daps_lock);
    dap->refs += 1u;
    cli_lock_give(&g_cli_daps_lock);
}

void cli_dap_unref(CliDap *dap)
{
    cli_lock_take(&g_cli_daps_lock);
    dap->refs -= 1u;
    const int last = (dap->refs == 0u);
    cli_lock_give(&g_cli_daps_lock);
    if (!last)
    {
        return;
    }
    cli_dap_ended(dap);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    cli_lock_take(&dap->lock);
    while ((dap->reading > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&dap->wake, &dap->lock, until - cli_clock_milliseconds());
    }
    const int read = (dap->reading == 0u);
    cli_lock_give(&dap->lock);
    if (!read)
    {
        return;
    }
    // the answers no request took, with no reader left to hand another
    for (unsigned int at = 0u; at < CLI_DAP_WAITERS; at += 1u)
    {
        g_cli_cells->persistent_buf_release(dap->waiters[at].answer.message, &g_cli_debug_cells);
        dap->waiters[at].answer = (CliLspAnswer){NULL, NULL, 0ull};
    }
    cli_socket_closed(&dap->socket);
    if (dap->kind == CLI_DAP_PROGRAM)
    {
        cli_spawn_closed(&dap->spawned);
    }
    cli_lock_take(&g_cli_daps_lock);
    dap->taken = 0;
    cli_lock_give(&g_cli_daps_lock);
}
