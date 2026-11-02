// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the parts registered and their grants, the listeners on each channel, and each message sent: refused where
// its sender may not send there, else counted, stored among its channel's last, and handed to every listener the
// channel's name is granted to
#include "../command_line/callback_cfg.h"
#include "comms_internal.h"

#include "../text/text.h"

CliCommsRoom g_cli_comms = {.lock = CLI_LOCK_INIT};

// the parts, the listeners and the channels' traffic taken at their most in one cell, all empty, as the first of them
// is needed; under the lock
static void comms_tables_init(void)
{
    if (g_cli_comms.parts != NULL)
    {
        return;
    }
    const unsigned long long part_bytes = (unsigned long long)CLI_COMMS_PARTS * sizeof(CliCommsPart);
    const unsigned long long listener_bytes = (unsigned long long)CLI_COMMS_LISTENERS * sizeof(CliCommsListener);
    const unsigned long long traffic_bytes = (unsigned long long)CLI_COMMS_CHANNELS * sizeof(CliCommsTraffic);
    const unsigned long long bytes = part_bytes + listener_bytes + traffic_bytes;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_comms_cells);
    if (cell == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = cell, .val = 0, .bytes = (size_t)bytes});
    g_cli_comms.parts = (CliCommsPart *)(void *)cell;
    g_cli_comms.parts_room = CLI_COMMS_PARTS;
    g_cli_comms.listeners = (CliCommsListener *)(void *)(cell + part_bytes);
    g_cli_comms.listeners_room = CLI_COMMS_LISTENERS;
    g_cli_comms.traffic = (CliCommsTraffic *)(void *)(cell + part_bytes + listener_bytes);
    g_cli_comms.channels_room = CLI_COMMS_CHANNELS;
}

typedef struct
{
    const char *key;
    const char *const *wanted;
    unsigned int count;
    const char (*granted)[CLI_NAME_MAX_LEN];
    unsigned int granted_count;
    const char *channel;
    CliCommsTraffic *traffic;
    const char *from;
    const char *body;
    unsigned long long size;
} CommsCtx;
// the place of the part of `key`, CLI_COMMS_PARTS where it is no part's, under the lock
EMBED_INLINE unsigned int comms_part_get(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_comms.parts_room; at += 1u)
    {
        if (g_cli_comms.parts[at].taken &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_comms.parts[at].key, .other = args->key, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return CLI_COMMS_PARTS;
}

int cli_comms_register(const CliCommsNameCfg *args)
{
    int has_process = args->has_process;
    unsigned int process = args->process;
    const char *const *listen = args->listen;
    unsigned int listens = args->listens;
    const char *const *send_grant = args->send_grant;
    unsigned int send_grants = args->send_grants;
    unsigned char secret[32];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = secret, .size = sizeof(secret)))
    {
        mmgr_line_put(args->err_msg, "the system gave no secret");
        return 0;
    }
    if ((listens > CLI_COMMS_GRANTS) || (send_grants > CLI_COMMS_GRANTS))
    {
        mmgr_line_put(args->err_msg, "a part has at most 16 grants to listen and 16 to send");
        return 0;
    }
    VerbaLinea written = {args->key_out, CLI_COMMS_KEY, 0ull};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&written, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&written, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&written);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    comms_tables_init();
    CliCommsPart *part = NULL;
    for (unsigned int at = 0u; (at < g_cli_comms.parts_room) && (part == NULL); at += 1u)
    {
        part = g_cli_comms.parts[at].taken ? NULL : &g_cli_comms.parts[at];
    }
    const unsigned int grants = listens + send_grants;
    char (*const granted)[CLI_NAME_MAX_LEN] =
        ((part != NULL) && (grants != 0u))
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)grants * CLI_NAME_MAX_LEN, &g_cli_comms_cells)
            : NULL;
    if ((part == NULL) || ((grants != 0u) && (granted == NULL)))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        mmgr_line_put(args->err_msg, "the comms layer has as many parts as it can");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = part, .val = 0, .bytes = sizeof(*part)});
    part->taken = 1;
    part->listen = granted;
    part->send_grant = (granted != NULL) ? (granted + listens) : NULL;
    memor.cpy(&(MemoriaCfg){.dst = part->key,
                            .src = args->key_out,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key_out, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = part->name,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    part->has_process = has_process;
    part->process = process;
    for (unsigned int at = 0u; at < listens; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = part->listen[at],
                          .src = listen[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = listen[at], .cap = CLI_CELLS_POOL}) + 1u});
    }
    for (unsigned int at = 0u; at < send_grants; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = part->send_grant[at],
                          .src = send_grant[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = send_grant[at], .cap = CLI_CELLS_POOL}) + 1u});
    }
    part->listens = listens;
    part->send_grants = send_grants;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return 1;
}

int cli_comms_register_persistent(const CliCommsNameCfg *args)
{
    if (!EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = args->name, .has_process = 0, .process = 0u,
                    .listen = args->listen, .listens = args->listens, .send_grant = args->send_grant,
                    .send_grants = args->send_grants, .key_out = args->key_out, .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key_out);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].persistent = 1;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return 1;
}

// the first of `wanted`, `count` of them, that no grant of `granted`, `granted_count` of them, contains within it, NULL
// where each is granted
EMBED_INLINE const char *comms_beyond(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        int within = 0;
        for (unsigned int grant = 0u; (grant < args->granted_count) && !within; grant += 1u)
        {
            within = EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = args->wanted[at],
                                .outer = args->granted[grant]);
        }
        if (!within)
        {
            return args->wanted[at];
        }
    }
    return NULL;
}

int cli_comms_register_under(const CliCommsNameCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->parent);
    if (at == CLI_COMMS_PARTS)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        mmgr_line_put(args->err_msg, "that key is no part's");
        return 0;
    }
    const CliCommsPart *const granter = &g_cli_comms.parts[at];
    const char *beyond =
        EMBED_CALL(comms_beyond, CommsCtx, .wanted = args->listen, .count = args->listens,
                   .granted = (const char (*)[CLI_NAME_MAX_LEN])granter->listen, .granted_count = granter->listens);
    beyond = (beyond != NULL)
                 ? beyond
                 : EMBED_CALL(comms_beyond, CommsCtx, .wanted = args->send_grant, .count = args->send_grants,
                              .granted = (const char (*)[CLI_NAME_MAX_LEN])granter->send_grant,
                              .granted_count = granter->send_grants);
    if (beyond != NULL)
    {
        mmgr_line_put(args->err_msg, granter->name);
        mmgr_line_put(args->err_msg, " may not grant ");
        mmgr_line_put(args->err_msg, beyond);
        mmgr_line_put(args->err_msg, ", which is not its own");
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return cli_comms_name.comms_register(args);
}

int cli_comms_is_persistent(const CliCommsProcessCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    const int persistent = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].persistent;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return persistent;
}

void cli_comms_bind(const CliCommsProcessCfg *args)
{
    unsigned int process = args->process;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].has_process = 1;
        g_cli_comms.parts[at].process = process;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
}

void cli_comms_leave(const CliCommsProcessCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].taken = 0;
        g_cli_cells->persistent_buf_release(g_cli_comms.parts[at].listen, &g_cli_comms_cells);
        g_cli_comms.parts[at].listen = NULL;
        g_cli_comms.parts[at].send_grant = NULL;
        for (unsigned int one = 0u; one < g_cli_comms.listeners_room; one += 1u)
        {
            CliCommsListener *const listener = &g_cli_comms.listeners[one];
            listener->taken = (listener->taken && (listener->part == at)) ? 0 : listener->taken;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
}

int cli_comms_process_get(const CliCommsKeyProcessCfg *args)
{
    unsigned int *process = args->process_pointer;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    const int has = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].has_process;
    *process = has ? g_cli_comms.parts[at].process : 0u;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return has;
}

int cli_comms_name_get(const CliCommsKeyCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->name_out,
            .src = g_cli_comms.parts[at].name,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_comms.parts[at].name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    return at < CLI_COMMS_PARTS;
}

int cli_comms_listen(const CliCommsChannelCfg *args)
{
    unsigned int *listener = args->listener;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at == CLI_COMMS_PARTS)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        mmgr_line_put(args->err_msg, "that key is no part's");
        return 0;
    }
    const CliCommsPart *const part = &g_cli_comms.parts[at];
    int granted = 0;
    for (unsigned int grant = 0u; (grant < part->listens) && !granted; grant += 1u)
    {
        granted =
            EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = args->channel, .outer = part->listen[grant]);
    }
    if (!granted)
    {
        mmgr_line_put(args->err_msg, part->name);
        mmgr_line_put(args->err_msg, " may not listen on ");
        mmgr_line_put(args->err_msg, args->channel);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        return 0;
    }
    comms_tables_init();
    unsigned int vacant = g_cli_comms.listeners_room;
    for (unsigned int one = 0u; (one < g_cli_comms.listeners_room) && (vacant == g_cli_comms.listeners_room); one += 1u)
    {
        vacant = g_cli_comms.listeners[one].taken ? vacant : one;
    }
    if (vacant < g_cli_comms.listeners_room)
    {
        CliCommsListener *const place = &g_cli_comms.listeners[vacant];
        *place = (CliCommsListener){1, at, "", args->queued, args->recv, args->context};
        memor.cpy(
            &(MemoriaCfg){.dst = place->pattern,
                          .src = args->channel,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_CELLS_POOL}) + 1u});
        *listener = vacant;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        return 1;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    mmgr_line_put(args->err_msg, "the comms layer has as many listeners as it can");
    return 0;
}

void cli_comms_unlisten(const CliCommsListenerCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    if (args->listener_value < g_cli_comms.listeners_room)
    {
        g_cli_comms.listeners[args->listener_value].taken = 0;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
}

// the traffic of `channel`, made where it has none, NULL where the channels stored are full, under the lock
EMBED_INLINE CliCommsTraffic *comms_traffic_get(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_comms.channels; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_comms.traffic[at].name, .other = args->channel, .cap = CLI_CELLS_POOL}))
        {
            return &g_cli_comms.traffic[at];
        }
    }
    comms_tables_init();
    if (g_cli_comms.channels >= g_cli_comms.channels_room)
    {
        return NULL;
    }
    CliCommsTraffic *const made = &g_cli_comms.traffic[g_cli_comms.channels];
    g_cli_comms.channels += 1u;
    memor.set(&(MemoriaCfg){.dst = made, .val = 0, .bytes = sizeof(*made)});
    memor.cpy(&(MemoriaCfg){.dst = made->name,
                            .src = args->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_CELLS_POOL}) + 1u});
    return made;
}

// a message stored among its channel's last, past the most stored over the oldest: in the oldest's cell where it fits,
// else in a larger cell with the oldest's given back; stored not at all where the cell is refused
EMBED_INLINE void comms_store(const CommsCtx *args)
{
    const char *from = args->from;
    const char *body = args->body;
    unsigned long long size = args->size;
    char sized[64];
    VerbaLinea sized_line = {sized, sizeof(sized), 0ull};
    mmgr_line_put(&sized_line, "\"a body of ");
    mmgr_line_decimal(&sized_line, size, 1u);
    mmgr_line_put(&sized_line, " bytes\"");
    const int whole = size < CLI_COMMS_STORED_BODY_MAX;
    const char *const stored = whole ? body : sized;
    const unsigned long long stored_size = whole ? size : sized_line.at;
    const unsigned long long from_size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u;
    const unsigned long long need = from_size + stored_size;
    const unsigned int place = (args->traffic->first + args->traffic->count) % CLI_COMMS_STORED_MAX;
    CliCommsStoredMsg *const stored_msg = &args->traffic->last[place];
    const int full = args->traffic->count == CLI_COMMS_STORED_MAX;
    const int fits = full && (need <= stored_msg->room);
    char *const cell = fits ? stored_msg->from : g_cli_cells->persistent_buf_alloc(need, &g_cli_comms_cells);
    if (cell == NULL)
    {
        return;
    }
    if (full)
    {
        args->traffic->first = (args->traffic->first + 1u) % CLI_COMMS_STORED_MAX;
    }
    else
    {
        args->traffic->count += 1u;
    }
    if (full && !fits)
    {
        g_cli_cells->persistent_buf_release(stored_msg->from, &g_cli_comms_cells);
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = from, .bytes = (size_t)from_size});
    memor.cpy(&(MemoriaCfg){.dst = cell + from_size, .src = stored, .bytes = (size_t)stored_size});
    stored_msg->from = cell;
    stored_msg->body = cell + from_size;
    stored_msg->size = stored_size;
    stored_msg->room = fits ? stored_msg->room : need;
}

int cli_comms_send(const CliCommsChannelErrMsgCfg *args)
{
    unsigned long long *delivered = args->delivered;
    // the listeners called once the lock is let go, each its function and context, in this speaker's own room, as a
    // listener called may send in turn
    CliCommsRecvFn recv_fns[CLI_COMMS_LISTENERS];
    void *calls_context[CLI_COMMS_LISTENERS];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at == CLI_COMMS_PARTS)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        mmgr_line_put(args->err_msg, "that key is no part's");
        return 0;
    }
    CliCommsPart *const part = &g_cli_comms.parts[at];
    char name[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = name,
                            .src = part->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}) + 1u});
    int granted = 0;
    for (unsigned int grant = 0u; (grant < part->send_grants) && !granted; grant += 1u)
    {
        granted = EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = part->send_grant[grant],
                             .channel = args->channel);
    }
    CliCommsTraffic *const traffic = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (!granted)
    {
        part->refused += 1ull;
        if (traffic != NULL)
        {
            traffic->refused += 1ull;
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " may not send on ");
        mmgr_line_put(args->err_msg, args->channel);
        return 0;
    }
    part->sent_count += 1ull;
    if (traffic != NULL)
    {
        traffic->messages += 1ull;
        traffic->bytes += args->size;
        EMBED_CALL(comms_store, CommsCtx, .traffic = traffic, .from = name, .body = args->body, .size = args->size);
    }
    unsigned long long count = 0ull;
    unsigned int calls = 0u;
    for (unsigned int one = 0u; one < g_cli_comms.listeners_room; one += 1u)
    {
        CliCommsListener *const listener = &g_cli_comms.listeners[one];
        if (!listener->taken ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener->pattern, .channel = args->channel))
        {
            continue;
        }
        if (!listener->queued)
        {
            recv_fns[calls] = listener->recv;
            calls_context[calls] = listener->context;
            calls += 1u;
            count += 1ull;
        }
        else
        {
            const int got = EMBED_CALL(listener->recv, CliCommsRecvCfg, .context = listener->context,
                                       .channel = args->channel, .from = name, .body = args->body, .size = args->size);
            count += (got > 0) ? 1ull : 0ull;
            // a listener whose reader is gone is let go
            listener->taken = (got == 0) ? 0 : listener->taken;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
    for (unsigned int one = 0u; one < calls; one += 1u)
    {
        (void)EMBED_CALL(recv_fns[one], CliCommsRecvCfg, .context = calls_context[one], .channel = args->channel,
                         .from = name, .body = args->body, .size = args->size);
    }
    *delivered = count;
    return 1;
}

void cli_comms_transfer(const CliCommsChannelBytesCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_comms.lock);
    CliCommsTraffic *const traffic = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (traffic != NULL)
    {
        traffic->transferred += args->bytes;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_comms.lock);
}
