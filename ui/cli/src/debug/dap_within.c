// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_within.c: a debug adapter that runs in impero on a thread of its own, connected to through two queues framed
// as a program's input and output are. Each queue is a ticket ring of embedded_types': a message waits in a movable
// cell of its size, its token pushed under a ticket of its own, and the queue's one reader pops the tokens in ticket
// order. A request the client writes is written into its cell as its head gives its length, and a message the adapter
// sends is framed as the client reads it
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../command_line/rings.h"
#include "../text/text.h"
#include "dap_internal.h"

// a message's place in its movable cell: its size in the first word, its bytes after it
#define CLI_DAP_BYTES_AT sizeof(unsigned long long)

// The messages waiting one way: the ring, its turnstiles, its slots a run of the adapter's part of the rings block, the
// next ticket a writer takes, the next ticket
// the reader pops, which the reader alone touches, and whether no more come
typedef struct
{
    EmbedTurnstiles turnstiles;
    EmbedTicketRing ring;
    embed_atomic_u32 ticket;
    embed_u32 taken;
    int closed;
} CliDapQueue;

// The adapter in orior, one at a time: the adapter, the requests it is passed and the messages it sends the client,
// woken by their wake; what the client wrote and not yet framed whole, the head as it comes and the request its
// length is taken for, its size and as much of it as came; what the client reads, the head that frames a message, the
// message's token and size, and how much of the two is read; and the function that serves a request, its context,
// its thread and whether it serves
static CliDap *s_within;
static CliDapQueue s_incoming;
static CliDapQueue s_outgoing;
static CliWake s_queue_wake = CLI_WAKE_INIT;
static char s_head[1024];
static unsigned long long s_head_size;
static const CarcerMovableEntry *s_coming;
static unsigned long long s_coming_size;
static unsigned long long s_coming_got;
static char s_pending_head[64];
static unsigned long long s_pending_head_size;
static const CarcerMovableEntry *s_pending;
static unsigned long long s_pending_size;
static unsigned long long s_pending_at;
static CliDapServe s_serve;
static void *s_serve_context;
static CliThread s_serve_thread;
static int s_serving;

typedef struct
{
    unsigned long long size;
    CliDapQueue *queue;
    const CarcerMovableEntry *message;
    const char *bytes;
    unsigned long long head;
    unsigned long long *length;
} DapWithinCtx;
// a message of `size` bytes in a movable cell of its size, the size written before the bytes to be written in: its
// token, or NULL where the cells cannot meet it
EMBED_INLINE const CarcerMovableEntry *within_message_alloc(const DapWithinCtx *args)
{
    const CarcerMovableEntry *const message =
        g_cli_guard->movable_buf_alloc(CLI_DAP_BYTES_AT + args->size, g_cli_debug_owner);
    if (message != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = message, .offset = 0ULL, .src = &args->size, .bytes = sizeof(args->size)});
    }
    return message;
}

// the size a message's cell holds in its first word
EMBED_INLINE unsigned long long within_message_size(const DapWithinCtx *args)
{
    unsigned long long size = 0ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->message, .offset = 0ULL, .dst = &size, .bytes = sizeof(size)});
    return size;
}

// a message's token pushed on a queue under a ticket of its own, this thread asleep while every slot is in flight: 1,
// or 0 where no more come, the message given back
EMBED_INLINE int within_queue(const DapWithinCtx *args)
{
    CliDapQueue *const queue = args->queue;
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&queue->ticket, 1u);
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the slot holds
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)args->message;
    embed_u64 seen = 0u;
    for (;;)
    {
        const unsigned int room = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_queue_wake);
        if (queue->closed)
        {
            (void)g_cli_guard->movable_buf_release(args->message, g_cli_debug_owner);
            return 0;
        }
        if (cli_ticket_room(&queue->ring, ticket) &&
            embed_ticket_ring_granted(&queue->turnstiles, address, &queue->ring, stamp, &seen))
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_queue_wake, .seen = room);
    }
    (void)embed_ticket_ring_pushed(&queue->turnstiles, address, &queue->ring, stamp, (void *)(embed_word)args->message,
                                   &seen);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
    return 1;
}

// `size` bytes of a message put on a queue in a cell of their size: 1, or 0 where no more come or the cells cannot meet
// it
EMBED_INLINE int within_push(const DapWithinCtx *args)
{
    const CarcerMovableEntry *const message = within_message_alloc(args);
    if (message == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = message, .offset = CLI_DAP_BYTES_AT, .src = args->bytes, .bytes = (embed_word)args->size});
    return EMBED_CALL(within_queue, DapWithinCtx, .queue = args->queue, .message = message);
}

// the next message's token of a queue, its slot released for the writers, waiting while the queue is empty, for the
// caller to give back: the token, or NULL where no more come; the queue's one reader calls it
EMBED_INLINE const CarcerMovableEntry *within_pop(const DapWithinCtx *args)
{
    CliDapQueue *const queue = args->queue;
    embed_u64 stamp = 0u;
    for (;;)
    {
        const unsigned int came = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_queue_wake);
        if (cli_ticket_pushed(&queue->ring, queue->taken, &stamp))
        {
            break;
        }
        if (queue->closed)
        {
            return NULL;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_queue_wake, .seen = came);
    }
    void *popped = NULL;
    embed_u64 seen = 0u;
    // Explicit cast carries the queue's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)queue;
    (void)embed_ticket_ring_popped(&queue->turnstiles, address, &queue->ring, stamp, &popped, &seen);
    (void)embed_ticket_ring_released(&queue->turnstiles, &queue->ring, stamp, &seen);
    queue->taken += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
    return popped;
}

// a queue notified no more comes
EMBED_INLINE void within_close(const DapWithinCtx *args)
{
    args->queue->closed = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_queue_wake);
}

// every message pushed on a queue and not popped given back, and the queue positioned again, open
EMBED_INLINE void within_clear(const DapWithinCtx *args)
{
    CliDapQueue *const queue = args->queue;
    for (unsigned int at = 0u; (queue->ring.slots != NULL) && (at < CLI_DAP_RING_SLOTS); at += 1u)
    {
        const embed_u64 stamp = embed_atomic_u64_load(&queue->ring.slots[at].stamp);
        if (((stamp & EMBED_TICKET_PUSHED) != 0u) && ((stamp & EMBED_TICKET_POPPED) == 0u))
        {
            (void)g_cli_guard->movable_buf_release(queue->ring.slots[at].job, g_cli_debug_owner);
        }
    }
    memor.set(&(MemoriaCfg){.dst = &queue->turnstiles, .val = 0, .bytes = sizeof(queue->turnstiles)});
    // the incoming queue's slots first in the adapter's part of the rings block, the outgoing queue's after them
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)(EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_DAP) +
                                        ((queue == &s_incoming)
                                             ? 0ULL
                                             : ((unsigned long long)CLI_DAP_RING_SLOTS * sizeof(EmbedTicketRingSlot))));
    memor.set(&(MemoriaCfg){
        .dst = slots, .val = 0, .bytes = (unsigned long long)CLI_DAP_RING_SLOTS * sizeof(EmbedTicketRingSlot)});
    embed_ticket_ring_begin(&queue->ring, slots, CLI_DAP_RING_SHIFT);
    embed_atomic_u32_store(&queue->ticket, 0u);
    queue->taken = 0u;
    queue->closed = 0;
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

// the request coming whole passed to the adapter as a request, its bytes read into a cell of this call for the JSON
// check: 1, or 0 where it is no JSON or no more come
static int within_request_complete(void)
{
    char tiny[1];
    VerbaLinea checked = {tiny, sizeof(tiny), 0ULL};
    const CarcerMovableEntry *const request = s_coming;
    const unsigned long long size = s_coming_size;
    s_coming = NULL;
    char *const bytes = g_cli_guard->temporary_buf_alloc((size != 0ULL) ? size : 1ULL, g_cli_debug_owner);
    if (bytes != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = request, .offset = CLI_DAP_BYTES_AT, .dst = bytes, .bytes = (embed_word)size});
    }
    const int json = (bytes != NULL) && EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = bytes,
                                                   .size = size, .out = &checked);
    g_cli_guard->persistent_buf_release(bytes, g_cli_debug_owner);
    if (!json)
    {
        (void)g_cli_guard->movable_buf_release(request, g_cli_debug_owner);
        return 0;
    }
    return EMBED_CALL(within_queue, DapWithinCtx, .queue = &s_incoming, .message = request);
}

// bytes the client writes to its adapter in orior: the head of each message read to the empty line that ends it, the
// request's cell taken for the length it gives and written as its bytes come, and passed to the adapter whole
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
            s_coming_size = length;
            s_coming_got = 0ULL;
            if (s_coming == NULL)
            {
                return 0;
            }
        }
        else
        {
            const unsigned long long required = s_coming_size - s_coming_got;
            const unsigned long long reserved = (left < required) ? left : required;
            // a piece of a message, which a embed_word counts
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_coming, .offset = CLI_DAP_BYTES_AT + s_coming_got, .src = at, .bytes = (embed_word)reserved});
            s_coming_got += reserved;
            at += reserved;
            left -= reserved;
        }
        if ((s_coming != NULL) && (s_coming_got == s_coming_size) && !within_request_complete())
        {
            return 0;
        }
    }
    return 1;
}

// what the client reads of its adapter in orior: each message it sends framed as one from a program, the head before
// its bytes, the bytes read out of the message's cell where they lie
static unsigned long long within_unframed(const CliSourcePieceCapacityCfg *args)
{
    if ((s_pending == NULL) || (s_pending_at >= s_pending_head_size + s_pending_size))
    {
        if (s_pending != NULL)
        {
            (void)g_cli_guard->movable_buf_release(s_pending, g_cli_debug_owner);
        }
        s_pending = EMBED_CALL(within_pop, DapWithinCtx, .queue = &s_outgoing);
        if (s_pending == NULL)
        {
            return 0ULL;
        }
        s_pending_size = EMBED_CALL(within_message_size, DapWithinCtx, .message = s_pending);
        VerbaLinea line = {s_pending_head, sizeof(s_pending_head), 0ULL};
        mmgr_line_put(&line, "Content-Length: ");
        mmgr_line_decimal(&line, s_pending_size, 1u);
        mmgr_line_put(&line, "\r\n\r\n");
        s_pending_head_size = line.at;
        s_pending_at = 0ULL;
    }
    const int in_head = (s_pending_at < s_pending_head_size);
    const unsigned long long left =
        in_head ? (s_pending_head_size - s_pending_at) : (s_pending_head_size + s_pending_size - s_pending_at);
    const unsigned long long count = (args->capacity < left) ? args->capacity : left;
    if (in_head)
    {
        // a piece of the head, which a embed_word counts
        memor.cpy(&(MemoriaCfg){.dst = args->piece, .src = s_pending_head + s_pending_at, .bytes = (embed_word)count});
    }
    else
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_pending,
                                .offset = CLI_DAP_BYTES_AT + (s_pending_at - s_pending_head_size),
                                .dst = args->piece,
                                .bytes = (embed_word)count});
    }
    s_pending_at += count;
    return count;
}

// each request passed to the adapter in impero, read into a cell of this call, until it reports it goes no further
// or no more come; then the client notified no more comes from it
static void within_serve(void *context)
{
    CliDap *const dap = (CliDap *)context;
    const CarcerMovableEntry *request = NULL;
    while ((request = EMBED_CALL(within_pop, DapWithinCtx, .queue = &s_incoming)) != NULL)
    {
        const unsigned long long size = EMBED_CALL(within_message_size, DapWithinCtx, .message = request);
        char *const bytes = g_cli_guard->temporary_buf_alloc((size != 0ULL) ? size : 1ULL, g_cli_debug_owner);
        int goes_on = bytes != NULL;
        if (bytes != NULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = request, .offset = CLI_DAP_BYTES_AT, .dst = bytes, .bytes = (embed_word)size});
            goes_on = EMBED_CALL(s_serve, CliDapServeCfg, .context = s_serve_context, .dap = dap, .request = bytes,
                                 .size = size);
        }
        g_cli_guard->persistent_buf_release(bytes, g_cli_debug_owner);
        (void)g_cli_guard->movable_buf_release(request, g_cli_debug_owner);
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
    // what the adapter before this one left given back, the rings positioned
    EMBED_CALL(within_clear, DapWithinCtx, .queue = &s_incoming);
    EMBED_CALL(within_clear, DapWithinCtx, .queue = &s_outgoing);
    s_head_size = 0ULL;
    if (s_coming != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_coming, g_cli_debug_owner);
    }
    s_coming = NULL;
    s_coming_size = 0ULL;
    if (s_pending != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_pending, g_cli_debug_owner);
    }
    s_pending = NULL;
    s_pending_size = 0ULL;
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
