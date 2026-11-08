// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_within.c: a debug adapter that runs in orior_cli on a thread of its own, connected to through two queues framed
// as a program's input and output are. Each message waits in a cell of its own size: a request the client writes is
// taken into one as its head gives its length, and a message the adapter sends is framed as the client reads it
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "dap_internal.h"

// A message waiting one way between orior_cli and its own adapter, in a cell of its size: the one after it, its size
// and its bytes
typedef struct CliDapQueuedStruct
{
    struct CliDapQueuedStruct *next;
    unsigned long long size;
    char bytes[];
} CliDapQueued;
// The messages waiting one way: the first and the last, and whether no more come
typedef struct
{
    CliDapQueued *first;
    CliDapQueued *last;
    int closed;
} CliDapQueue;

// The adapter in orior, one at a time: the adapter, the requests it is handed and the messages it sends the client,
// under their lock and woken by their wake; what the client wrote and not yet framed whole, the head as it comes and
// the request its length is taken for, as much of it as came; what the client reads, the head that frames a message,
// the message, and how much of the two is read; and the function that serves a request, its context, its thread and
// whether it serves
static CliDap *s_within;
static CliDapQueue s_incoming;
static CliDapQueue s_outgoing;
static CliWake s_queue_wake = CLI_WAKE_INIT;
static char s_head[1024];
static unsigned long long s_head_size;
static CliDapQueued *s_coming;
static unsigned long long s_coming_got;
static char s_pending_head[64];
static unsigned long long s_pending_head_size;
static CliDapQueued *s_pending;
static unsigned long long s_pending_at;
static CliDapServe s_serve;
static void *s_serve_context;
static CliThread s_serve_thread;
static int s_serving;

typedef struct
{
    unsigned long long size;
    CliDapQueue *queue;
    CliDapQueued *message;
    const char *bytes;
    unsigned long long head;
    unsigned long long *length;
} DapWithinCtx;
// a message of `size` bytes in a cell of its size, its bytes to be put in, or NULL where the cells cannot meet it
EMBED_INLINE CliDapQueued *within_message_alloc(const DapWithinCtx *args)
{
    unsigned long long size = args->size;
    CliDapQueued *const message = g_cli_guard->persistent_buf_alloc(sizeof(CliDapQueued) + size, g_cli_debug_owner);
    if (message != NULL)
    {
        message->next = NULL;
        message->size = size;
    }
    return message;
}

// a message put on a queue: 1, or 0 where no more come, the message given back
EMBED_INLINE int within_queue(const DapWithinCtx *args)
{
    if (args->queue->closed)
    {
        g_cli_guard->persistent_buf_release(args->message, g_cli_debug_owner);
        return 0;
    }
    if (args->queue->last != NULL)
    {
        args->queue->last->next = args->message;
    }
    else
    {
        args->queue->first = args->message;
    }
    args->queue->last = args->message;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
    return 1;
}

// `size` bytes of a message put on a queue in a cell of their size: 1, or 0 where no more come or the cells cannot meet
// it
EMBED_INLINE int within_push(const DapWithinCtx *args)
{
    const char *bytes = args->bytes;
    CliDapQueued *const message = within_message_alloc(args);
    if (message == NULL)
    {
        return 0;
    }
    // a message's size, which a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = message->bytes, .src = bytes, .bytes = (size_t)args->size});
    return EMBED_CALL(within_queue, DapWithinCtx, .queue = args->queue, .message = message);
}

// the next message of a queue, waiting while it is empty, for the caller to give back: the message, or NULL where no
// more come
EMBED_INLINE CliDapQueued *within_pop(const DapWithinCtx *args)
{
    while ((args->queue->first == NULL) && !args->queue->closed)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_queue_wake);
    }
    CliDapQueued *const message = args->queue->first;
    if (message != NULL)
    {
        args->queue->first = message->next;
        args->queue->last = (args->queue->first != NULL) ? args->queue->last : NULL;
    }
    return message;
}

// a queue notified no more comes
EMBED_INLINE void within_close(const DapWithinCtx *args)
{
    args->queue->closed = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
}

// a queue's messages given back and the queue made open again
EMBED_INLINE void within_clear(const DapWithinCtx *args)
{
    CliDapQueue *queue = args->queue;
    CliDapQueued *message = queue->first;
    while (message != NULL)
    {
        CliDapQueued *const next = message->next;
        g_cli_guard->persistent_buf_release(message, g_cli_debug_owner);
        message = next;
    }
    *queue = (CliDapQueue){NULL, NULL, 0};
}

int cli_dap_send(const CliDapMessageCfg *args)
{
    return EMBED_CALL(within_push, DapWithinCtx, .queue = &s_outgoing, .bytes = args->message, .size = args->size);
}

int cli_dap_itself(const CliDapMessageCfg *args)
{
    return EMBED_CALL(within_push, DapWithinCtx, .queue = &s_incoming, .bytes = args->message, .size = args->size);
}

// the Content-Length the head of `head` bytes of s_head gives into `length`: 1, or 0 where it gives none
EMBED_INLINE int within_length(const DapWithinCtx *args)
{
    unsigned long long *length = args->length;
    static const char s_length[] = "Content-Length:";
    int has_length = 0;
    for (unsigned long long line = 0ULL; !has_length && (line < args->head);)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = s_head, .at = line, .size = args->head).stop;
        if ((end - line >= sizeof(s_length) - 1ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = s_head + line, .other = s_length, .bytes = sizeof(s_length) - 1ULL}) == 0))
        {
            unsigned long long at = line + sizeof(s_length) - 1ULL;
            unsigned long long stop = end;
            while ((at < stop) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_POOL_BYTES, .byte = s_head[at]}) != NULL))
            {
                at += 1ULL;
            }
            while ((stop > at) && (cellul.chr(&(CatenaFinitaCfg){
                                       .src = " \t\r", .cap = CLI_POOL_BYTES, .byte = s_head[stop - 1ULL]}) != NULL))
            {
                stop -= 1ULL;
            }
            has_length = (stop > at);
            *length = 0ULL;
            unsigned long long digit = at;
            while (has_length && (digit < stop))
            {
                has_length = (s_head[digit] >= '0') && (s_head[digit] <= '9');
                *length = (*length * 10ULL) + (unsigned long long)(s_head[digit] - '0');
                digit += 1ULL;
            }
        }
        line = end + 1ULL;
    }
    return has_length;
}

// the request coming whole handed to the adapter as a request: 1, or 0 where it is no JSON or no more come
static int within_request_complete(void)
{
    char tiny[1];
    VerbaLinea checked = {tiny, sizeof(tiny), 0ULL};
    CliDapQueued *const request = s_coming;
    s_coming = NULL;
    if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = request->bytes, .size = request->size,
                    .out = &checked))
    {
        g_cli_guard->persistent_buf_release(request, g_cli_debug_owner);
        return 0;
    }
    return EMBED_CALL(within_queue, DapWithinCtx, .queue = &s_incoming, .message = request);
}

// bytes the client writes to its adapter in orior: the head of each message read to the empty line that ends it, the
// request taken for the length it gives and filled as its bytes come, and handed to the adapter whole
static int within_frame(const CliSinkBytesSizeCfg *args)
{
    const void *bytes = args->bytes;
    unsigned long long size = args->size;
    const char *at = (const char *)bytes;
    unsigned long long left = size;
    while (left > 0ULL)
    {
        if (s_coming == NULL)
        {
            if (s_head_size == sizeof(s_head))
            {
                return 0;
            }
            s_head[s_head_size] = *at;
            s_head_size += 1ULL;
            at += 1;
            left -= 1ULL;
            unsigned long long length = 0ULL;
            if ((s_head_size < 4ULL) ||
                (memor.cmp(&(MemoriaCfg){.src = s_head + s_head_size - 4ULL, .other = "\r\n\r\n", .bytes = 4u}) != 0))
            {
                continue;
            }
            if (!EMBED_CALL(within_length, DapWithinCtx, .head = s_head_size - 4ULL, .length = &length))
            {
                return 0;
            }
            s_head_size = 0ULL;
            s_coming = EMBED_CALL(within_message_alloc, DapWithinCtx, .size = length);
            s_coming_got = 0ULL;
            if (s_coming == NULL)
            {
                return 0;
            }
        }
        else
        {
            const unsigned long long required = s_coming->size - s_coming_got;
            const unsigned long long reserved = (left < required) ? left : required;
            // a piece of a message, which a size_t counts
            memor.cpy(&(MemoriaCfg){.dst = s_coming->bytes + s_coming_got, .src = at, .bytes = (size_t)reserved});
            s_coming_got += reserved;
            at += reserved;
            left -= reserved;
        }
        if ((s_coming != NULL) && (s_coming_got == s_coming->size) && !within_request_complete())
        {
            return 0;
        }
    }
    return 1;
}

// what the client reads of its adapter in orior: each message it sends framed as one from a program, the head before
// its bytes
static unsigned long long within_unframed(const CliSourcePieceCapacityCfg *args)
{
    if ((s_pending == NULL) || (s_pending_at >= s_pending_head_size + s_pending->size))
    {
        g_cli_guard->persistent_buf_release(s_pending, g_cli_debug_owner);
        s_pending = EMBED_CALL(within_pop, DapWithinCtx, .queue = &s_outgoing);
        if (s_pending == NULL)
        {
            return 0ULL;
        }
        VerbaLinea line = {s_pending_head, sizeof(s_pending_head), 0ULL};
        mmgr_line_put(&line, "Content-Length: ");
        mmgr_line_decimal(&line, s_pending->size, 1u);
        mmgr_line_put(&line, "\r\n\r\n");
        s_pending_head_size = line.at;
        s_pending_at = 0ULL;
    }
    const int in_head = (s_pending_at < s_pending_head_size);
    const char *const from =
        in_head ? (s_pending_head + s_pending_at) : (s_pending->bytes + (s_pending_at - s_pending_head_size));
    const unsigned long long left =
        in_head ? (s_pending_head_size - s_pending_at) : (s_pending_head_size + s_pending->size - s_pending_at);
    const unsigned long long count = (args->capacity < left) ? args->capacity : left;
    // a piece of a message, which a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = args->piece, .src = from, .bytes = (size_t)count});
    s_pending_at += count;
    return count;
}

// each request handed to the adapter in orior_cli until it reports it goes no further or no more come; then the client
// notified no more comes from it
static void within_serve(void *context)
{
    CliDap *const dap = (CliDap *)context;
    CliDapQueued *request = NULL;
    while ((request = EMBED_CALL(within_pop, DapWithinCtx, .queue = &s_incoming)) != NULL)
    {
        const int goes_on = EMBED_CALL(s_serve, CliDapServeCfg, .context = s_serve_context, .dap = dap,
                                       .request = request->bytes, .size = request->size);
        g_cli_guard->persistent_buf_release(request, g_cli_debug_owner);
        if (!goes_on)
        {
            break;
        }
    }
    EMBED_CALL(within_close, DapWithinCtx, .queue = &s_incoming);
    EMBED_CALL(within_close, DapWithinCtx, .queue = &s_outgoing);
    s_serving = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
}

void cli_dap_within_let_go(const CliDapCtx *args)
{
    (void)args;
    EMBED_CALL(within_close, DapWithinCtx, .queue = &s_incoming);
    EMBED_CALL(within_close, DapWithinCtx, .queue = &s_outgoing);
}

CliDap *cli_dap_within(const CliDapEmitCfg *args)
{
    CliDapEventFn emit = args->emit;
    void *context = args->context;
    while (s_serving)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_queue_wake);
    }
    CliDap *const dap = cli_dap_position_alloc();
    if (dap == NULL)
    {
        mmgr_line_put(args->err_msg, "no more debug adapters are open at once");
        return NULL;
    }
    s_within = dap;
    // what the adapter before this one left given back
    EMBED_CALL(within_clear, DapWithinCtx, .queue = &s_incoming);
    EMBED_CALL(within_clear, DapWithinCtx, .queue = &s_outgoing);
    s_head_size = 0ULL;
    g_cli_guard->persistent_buf_release(s_coming, g_cli_debug_owner);
    s_coming = NULL;
    g_cli_guard->persistent_buf_release(s_pending, g_cli_debug_owner);
    s_pending = NULL;
    s_pending_head_size = 0ULL;
    s_pending_at = 0ULL;
    s_serve = args->serve;
    s_serve_context = args->serve_context;
    dap->type = CLI_DAP_WITHIN;
    dap->ends_program = 1;
    dap->emit = emit;
    dap->context = context;
    dap->input = (CliLspWriter){within_frame, NULL};
    dap->reader.read = within_unframed;
    dap->reader.source = dap;
    dap->reader.start = 0ULL;
    dap->reader.end = 0ULL;
    s_serving = 1;
    s_serve_thread = (CliThread){within_serve, dap};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_serve_thread))
    {
        s_serving = 0;
        dap->reserved = 0;
        mmgr_line_put(args->err_msg, "the adapter's thread did not start");
        return NULL;
    }
    EMBED_CALL(cli_dap_internal.readers_start, CliDapCtx, .dap = dap, .complaints = 0);
    return dap;
}
