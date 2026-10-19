// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap_within.c: a debug adapter that runs in orior on a thread of its own, spoken to through two queues framed as a
// program's input and output are. Each message waits in a cell of its own size: a request the client writes is taken
// into one as its head gives its length, and a message the adapter sends is framed as the client reads it
#include "../command_line/cells.h"
#include "dap_internal.h"

// A message waiting one way between orior and its own adapter, in a cell of its size: the one after it, its size and
// its bytes
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
static CliDapQueue s_asked;
static CliDapQueue s_told;
static CliLock s_queue_lock = CLI_LOCK_INIT;
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

// a message of `size` bytes in a cell of its size, its bytes to be put in, or NULL where the cells cannot meet it
static CliDapQueued *within_message_taken(unsigned long long size)
{
    CliDapQueued *const message = cli_cell_taken(sizeof(CliDapQueued) + size, "a message of orior's adapter");
    if (message != NULL)
    {
        message->next = NULL;
        message->size = size;
    }
    return message;
}

// a message put on a queue: 1, or 0 where no more come, the message given back
static int within_queued(CliDapQueue *queue, CliDapQueued *message)
{
    cli_lock_take(&s_queue_lock);
    if (queue->closed)
    {
        cli_lock_give(&s_queue_lock);
        cli_cell_given(message);
        return 0;
    }
    if (queue->last != NULL)
    {
        queue->last->next = message;
    }
    else
    {
        queue->first = message;
    }
    queue->last = message;
    cli_wake_all(&s_queue_wake);
    cli_lock_give(&s_queue_lock);
    return 1;
}

// `size` bytes of a message put on a queue in a cell of their size: 1, or 0 where no more come or the cells cannot meet
// it
static int within_pushed(CliDapQueue *queue, const char *bytes, unsigned long long size)
{
    CliDapQueued *const message = within_message_taken(size);
    if (message == NULL)
    {
        return 0;
    }
    // a message's size, which a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = message->bytes, .src = bytes, .bytes = (size_t)size});
    return within_queued(queue, message);
}

// the next message of a queue, waiting while it is empty, for the caller to give back: the message, or NULL where no
// more come
static CliDapQueued *within_popped(CliDapQueue *queue)
{
    cli_lock_take(&s_queue_lock);
    while ((queue->first == NULL) && !queue->closed)
    {
        cli_wake_wait(&s_queue_wake, &s_queue_lock);
    }
    CliDapQueued *const message = queue->first;
    if (message != NULL)
    {
        queue->first = message->next;
        queue->last = (queue->first != NULL) ? queue->last : NULL;
    }
    cli_lock_give(&s_queue_lock);
    return message;
}

// a queue told no more comes
static void within_closed(CliDapQueue *queue)
{
    cli_lock_take(&s_queue_lock);
    queue->closed = 1;
    cli_wake_all(&s_queue_wake);
    cli_lock_give(&s_queue_lock);
}

// a queue's messages given back and the queue made open again
static void within_emptied(CliDapQueue *queue)
{
    CliDapQueued *message = queue->first;
    while (message != NULL)
    {
        CliDapQueued *const next = message->next;
        cli_cell_given(message);
        message = next;
    }
    *queue = (CliDapQueue){NULL, NULL, 0};
}

int cli_dap_told(CliDap *dap, const char *message, unsigned long long size)
{
    (void)dap;
    return within_pushed(&s_told, message, size);
}

int cli_dap_itself(CliDap *dap, const char *message, unsigned long long size)
{
    (void)dap;
    return within_pushed(&s_asked, message, size);
}

// the Content-Length the head of `head` bytes of s_head gives into `length`: 1, or 0 where it gives none
static int within_length(unsigned long long head, unsigned long long *length)
{
    static const char s_length[] = "Content-Length:";
    int has_length = 0;
    for (unsigned long long line = 0ull; !has_length && (line < head);)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = s_head + line, .val = '\n', .bytes = head - line});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - s_head) : head;
        if ((end - line >= sizeof(s_length) - 1ull) &&
            (memor.cmp(&(MemoriaCfg){.src = s_head + line, .other = s_length, .bytes = sizeof(s_length) - 1ull}) == 0))
        {
            unsigned long long at = line + sizeof(s_length) - 1ull;
            unsigned long long stop = end;
            while ((at < stop) &&
                   (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = s_head[at]}) != NULL))
            {
                at += 1ull;
            }
            while ((stop > at) && (cellul.chr(&(CatenaFinitaCfg){
                                       .src = " \t\r", .cap = CLI_CELLS_POOL, .byte = s_head[stop - 1ull]}) != NULL))
            {
                stop -= 1ull;
            }
            has_length = (stop > at);
            *length = 0ull;
            for (unsigned long long digit = at; has_length && (digit < stop); digit += 1ull)
            {
                has_length = (s_head[digit] >= '0') && (s_head[digit] <= '9');
                *length = (*length * 10ull) + (unsigned long long)(s_head[digit] - '0');
            }
        }
        line = end + 1ull;
    }
    return has_length;
}

// the request coming whole handed to the adapter as a request: 1, or 0 where it is no JSON or no more come
static int within_request_whole(void)
{
    char tiny[1];
    CliLine checked = {tiny, sizeof(tiny), 0ull};
    CliDapQueued *const request = s_coming;
    s_coming = NULL;
    if (!cli_json_compacted(request->bytes, request->size, &checked))
    {
        cli_cell_given(request);
        return 0;
    }
    return within_queued(&s_asked, request);
}

// bytes the client writes to its adapter in orior: the head of each message read to the empty line that ends it, the
// request taken for the length it gives and filled as its bytes come, and handed to the adapter whole
static int within_framed(void *sink, const void *bytes, unsigned long long size)
{
    (void)sink;
    const char *at = (const char *)bytes;
    unsigned long long left = size;
    while (left > 0ull)
    {
        if (s_coming == NULL)
        {
            if (s_head_size == sizeof(s_head))
            {
                return 0;
            }
            s_head[s_head_size] = *at;
            s_head_size += 1ull;
            at += 1;
            left -= 1ull;
            unsigned long long length = 0ull;
            if ((s_head_size < 4ull) ||
                (memor.cmp(&(MemoriaCfg){.src = s_head + s_head_size - 4ull, .other = "\r\n\r\n", .bytes = 4u}) != 0))
            {
                continue;
            }
            if (!within_length(s_head_size - 4ull, &length))
            {
                return 0;
            }
            s_head_size = 0ull;
            s_coming = within_message_taken(length);
            s_coming_got = 0ull;
            if (s_coming == NULL)
            {
                return 0;
            }
        }
        else
        {
            const unsigned long long wanted = s_coming->size - s_coming_got;
            const unsigned long long taken = (left < wanted) ? left : wanted;
            // a piece of a message, which a size_t counts
            memor.cpy(&(MemoriaCfg){.dst = s_coming->bytes + s_coming_got, .src = at, .bytes = (size_t)taken});
            s_coming_got += taken;
            at += taken;
            left -= taken;
        }
        if ((s_coming != NULL) && (s_coming_got == s_coming->size) && !within_request_whole())
        {
            return 0;
        }
    }
    return 1;
}

// what the client reads of its adapter in orior: each message it sends framed as one from a program, the head before
// its bytes
static unsigned long long within_unframed(void *source, char *piece, unsigned long long room)
{
    (void)source;
    if ((s_pending == NULL) || (s_pending_at >= s_pending_head_size + s_pending->size))
    {
        cli_cell_given(s_pending);
        s_pending = within_popped(&s_told);
        if (s_pending == NULL)
        {
            return 0ull;
        }
        CliLine line = {s_pending_head, sizeof(s_pending_head), 0ull};
        cli_line_text(&line, "Content-Length: ");
        cli_line_decimal(&line, s_pending->size, 1u);
        cli_line_text(&line, "\r\n\r\n");
        s_pending_head_size = line.at;
        s_pending_at = 0ull;
    }
    const int in_head = (s_pending_at < s_pending_head_size);
    const char *const from =
        in_head ? (s_pending_head + s_pending_at) : (s_pending->bytes + (s_pending_at - s_pending_head_size));
    const unsigned long long left =
        in_head ? (s_pending_head_size - s_pending_at) : (s_pending_head_size + s_pending->size - s_pending_at);
    const unsigned long long count = (room < left) ? room : left;
    // a piece of a message, which a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = piece, .src = from, .bytes = (size_t)count});
    s_pending_at += count;
    return count;
}

// each request handed to the adapter in orior until it says it goes no further or no more come; then the client told
// no more comes from it
static void within_served(void *context)
{
    CliDap *const dap = (CliDap *)context;
    CliDapQueued *request = NULL;
    while ((request = within_popped(&s_asked)) != NULL)
    {
        const int goes_on = s_serve(s_serve_context, dap, request->bytes, request->size);
        cli_cell_given(request);
        if (!goes_on)
        {
            break;
        }
    }
    within_closed(&s_asked);
    within_closed(&s_told);
    cli_lock_take(&s_queue_lock);
    s_serving = 0;
    cli_wake_all(&s_queue_wake);
    cli_lock_give(&s_queue_lock);
}

void cli_dap_within_let_go(CliDap *dap)
{
    (void)dap;
    within_closed(&s_asked);
    within_closed(&s_told);
}

CliDap *cli_dap_within(CliDapServe serve, void *serve_context, CliDapHeard heard, void *context, CliLine *said)
{
    cli_lock_take(&s_queue_lock);
    while (s_serving)
    {
        cli_wake_wait(&s_queue_wake, &s_queue_lock);
    }
    cli_lock_give(&s_queue_lock);
    CliDap *const dap = cli_dap_place_taken();
    if (dap == NULL)
    {
        cli_line_text(said, "no more debug adapters are held at once");
        return NULL;
    }
    s_within = dap;
    // what the adapter before this one left given back
    within_emptied(&s_asked);
    within_emptied(&s_told);
    s_head_size = 0ull;
    cli_cell_given(s_coming);
    s_coming = NULL;
    cli_cell_given(s_pending);
    s_pending = NULL;
    s_pending_head_size = 0ull;
    s_pending_at = 0ull;
    s_serve = serve;
    s_serve_context = serve_context;
    dap->kind = CLI_DAP_WITHIN;
    dap->ends_program = 1;
    dap->heard = heard;
    dap->context = context;
    dap->input = (CliLspWriter){within_framed, NULL};
    dap->reader.read = within_unframed;
    dap->reader.source = dap;
    dap->reader.start = 0ull;
    dap->reader.end = 0ull;
    s_serving = 1;
    s_serve_thread = (CliThread){within_served, dap};
    if (!cli_thread_started(&s_serve_thread))
    {
        s_serving = 0;
        cli_lock_take(&g_cli_daps_lock);
        dap->taken = 0;
        cli_lock_give(&g_cli_daps_lock);
        cli_line_text(said, "the adapter's thread did not start");
        return NULL;
    }
    cli_dap_readers_started(dap, 0);
    return dap;
}
