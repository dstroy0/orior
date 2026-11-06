// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the parts registered and their grants, the listeners on each channel, and each message sent: refused where
// its sender may not send there, else counted, stored among its channel's last, and handed to every listener the
// channel's name is granted to
#include "../command_line/callback_cfg.h"
#include "comms_internal.h"

#include "../text/text.h"

CliCommsWorkState g_cli_comms = {0};

// the parts, the listeners and the channels' traffic at their most, which one cell holds
typedef struct
{
    CliCommsPart parts[CLI_COMMS_PARTS];
    CliCommsListener listeners[CLI_COMMS_LISTENERS];
    CliCommsTraffic traffic[CLI_COMMS_CHANNELS];
} CliCommsTables;

// the tables taken in their one cell, all empty, as the first of them is needed; under the lock
static void comms_tables_init(void)
{
    if (g_cli_comms.parts != NULL)
    {
        return;
    }
    CliCommsTables *const tables = g_cli_guard->persistent_buf_alloc(sizeof(*tables), g_cli_comms_owner);
    if (tables == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = tables, .val = 0, .bytes = sizeof(*tables)});
    g_cli_comms.parts = tables->parts;
    g_cli_comms.parts_capacity = CLI_COMMS_PARTS;
    g_cli_comms.listeners = tables->listeners;
    g_cli_comms.listeners_capacity = CLI_COMMS_LISTENERS;
    g_cli_comms.traffic = tables->traffic;
    g_cli_comms.channels_capacity = CLI_COMMS_CHANNELS;
}

typedef struct
{
    const char *key;
    const char *const *required;
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
    for (unsigned int at = 0u; at < g_cli_comms.parts_capacity; at += 1u)
    {
        if (g_cli_comms.parts[at].reserved &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_comms.parts[at].key, .other = args->key, .cap = CLI_POOL_BYTES})))
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
    VerbaLinea written = {args->key_out, CLI_COMMS_KEY, 0ULL};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        mmgr_line_ch(&written, "0123456789abcdef"[secret[at] >> 4u]);
        mmgr_line_ch(&written, "0123456789abcdef"[secret[at] & 15u]);
    }
    mmgr_line_finish(&written);
    comms_tables_init();
    CliCommsPart *part = NULL;
    for (unsigned int at = 0u; (at < g_cli_comms.parts_capacity) && (part == NULL); at += 1u)
    {
        part = g_cli_comms.parts[at].reserved ? NULL : &g_cli_comms.parts[at];
    }
    const unsigned int grants = listens + send_grants;
    char (*const granted)[CLI_NAME_MAX_LEN] =
        ((part != NULL) && (grants != 0u))
            ? g_cli_guard->persistent_buf_alloc((unsigned long long)grants * CLI_NAME_MAX_LEN, g_cli_comms_owner)
            : NULL;
    if ((part == NULL) || ((grants != 0u) && (granted == NULL)))
    {
        mmgr_line_put(args->err_msg, "the comms layer has as many parts as it can");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = part, .val = 0, .bytes = sizeof(*part)});
    part->reserved = 1;
    part->listen = granted;
    part->send_grant = (granted != NULL) ? (granted + listens) : NULL;
    memor.cpy(&(MemoriaCfg){.dst = part->key,
                            .src = args->key_out,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key_out, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = part->name,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
    part->has_process = has_process;
    part->process = process;
    for (unsigned int at = 0u; at < listens; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = part->listen[at],
                          .src = listen[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = listen[at], .cap = CLI_POOL_BYTES}) + 1u});
    }
    for (unsigned int at = 0u; at < send_grants; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = part->send_grant[at],
                          .src = send_grant[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = send_grant[at], .cap = CLI_POOL_BYTES}) + 1u});
    }
    part->listens = listens;
    part->send_grants = send_grants;
    (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
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
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key_out);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].persistent = 1;
    }
    return 1;
}

// the first of `required`, `count` of them, that no grant of `granted`, `granted_count` of them, contains within it,
// NULL where each is granted
EMBED_INLINE const char *comms_beyond(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        int within = 0;
        for (unsigned int grant = 0u; (grant < args->granted_count) && !within; grant += 1u)
        {
            within = EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = args->required[at],
                                .outer = args->granted[grant]);
        }
        if (!within)
        {
            return args->required[at];
        }
    }
    return NULL;
}

int cli_comms_register_under(const CliCommsNameCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->parent);
    if (at == CLI_COMMS_PARTS)
    {
        mmgr_line_put(args->err_msg, "that key is no part's");
        return 0;
    }
    const CliCommsPart *const granter = &g_cli_comms.parts[at];
    const char *beyond =
        EMBED_CALL(comms_beyond, CommsCtx, .required = args->listen, .count = args->listens,
                   .granted = (const char (*)[CLI_NAME_MAX_LEN])granter->listen, .granted_count = granter->listens);
    beyond = (beyond != NULL)
                 ? beyond
                 : EMBED_CALL(comms_beyond, CommsCtx, .required = args->send_grant, .count = args->send_grants,
                              .granted = (const char (*)[CLI_NAME_MAX_LEN])granter->send_grant,
                              .granted_count = granter->send_grants);
    if (beyond != NULL)
    {
        mmgr_line_put(args->err_msg, granter->name);
        mmgr_line_put(args->err_msg, " may not grant ");
        mmgr_line_put(args->err_msg, beyond);
        mmgr_line_put(args->err_msg, ", which is not its own");
        return 0;
    }
    return cli_comms_name.comms_register(args);
}

int cli_comms_is_persistent(const CliCommsProcessCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    const int persistent = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].persistent;
    return persistent;
}

void cli_comms_bind(const CliCommsProcessCfg *args)
{
    unsigned int process = args->process;
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].has_process = 1;
        g_cli_comms.parts[at].process = process;
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

void cli_comms_leave(const CliCommsProcessCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].reserved = 0;
        g_cli_guard->persistent_buf_release(g_cli_comms.parts[at].listen, g_cli_comms_owner);
        g_cli_comms.parts[at].listen = NULL;
        g_cli_comms.parts[at].send_grant = NULL;
        for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
        {
            CliCommsListener *const listener = &g_cli_comms.listeners[one];
            listener->reserved = (listener->reserved && (listener->part == at)) ? 0 : listener->reserved;
        }
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

int cli_comms_process_get(const CliCommsKeyProcessCfg *args)
{
    unsigned int *process = args->process_pointer;
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    const int has = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].has_process;
    *process = has ? g_cli_comms.parts[at].process : 0u;
    return has;
}

int cli_comms_name_get(const CliCommsKeyCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->name_out,
            .src = g_cli_comms.parts[at].name,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_comms.parts[at].name, .cap = CLI_POOL_BYTES}) + 1u});
    }
    return at < CLI_COMMS_PARTS;
}

int cli_comms_listen(const CliCommsChannelCfg *args)
{
    unsigned int *listener = args->listener;
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at == CLI_COMMS_PARTS)
    {
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
        return 0;
    }
    comms_tables_init();
    unsigned int vacant = g_cli_comms.listeners_capacity;
    for (unsigned int one = 0u; (one < g_cli_comms.listeners_capacity) && (vacant == g_cli_comms.listeners_capacity);
         one += 1u)
    {
        vacant = g_cli_comms.listeners[one].reserved ? vacant : one;
    }
    if (vacant < g_cli_comms.listeners_capacity)
    {
        CliCommsListener *const place = &g_cli_comms.listeners[vacant];
        *place = (CliCommsListener){1, at, "", args->queued, args->recv, args->context};
        memor.cpy(
            &(MemoriaCfg){.dst = place->pattern,
                          .src = args->channel,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_POOL_BYTES}) + 1u});
        *listener = vacant;
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
        return 1;
    }
    mmgr_line_put(args->err_msg, "the comms layer has as many listeners as it can");
    return 0;
}

void cli_comms_unlisten(const CliCommsListenerCfg *args)
{
    if (args->listener_value < g_cli_comms.listeners_capacity)
    {
        g_cli_comms.listeners[args->listener_value].reserved = 0;
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

// the traffic of `channel`, made where it has none, NULL where the channels stored are full, under the lock
EMBED_INLINE CliCommsTraffic *comms_traffic_get(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_comms.channels; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_comms.traffic[at].name, .other = args->channel, .cap = CLI_POOL_BYTES}))
        {
            return &g_cli_comms.traffic[at];
        }
    }
    comms_tables_init();
    if ((g_cli_comms.traffic == NULL) || (g_cli_comms.channels >= g_cli_comms.channels_capacity))
    {
        return NULL;
    }
    CliCommsTraffic *const made = &g_cli_comms.traffic[g_cli_comms.channels];
    g_cli_comms.channels += 1u;
    memor.set(&(MemoriaCfg){.dst = made, .val = 0, .bytes = sizeof(*made)});
    memor.cpy(&(MemoriaCfg){.dst = made->name,
                            .src = args->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_POOL_BYTES}) + 1u});
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
    VerbaLinea sized_line = {sized, sizeof(sized), 0ULL};
    mmgr_line_put(&sized_line, "\"a body of ");
    mmgr_line_decimal(&sized_line, size, 1u);
    mmgr_line_put(&sized_line, " bytes\"");
    const int whole = size < CLI_COMMS_STORED_BODY_MAX;
    const char *const stored = whole ? body : sized;
    const unsigned long long stored_size = whole ? size : sized_line.at;
    const unsigned long long from_size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES}) + 1u;
    const unsigned long long need = from_size + stored_size;
    const unsigned int place = (args->traffic->first + args->traffic->count) % CLI_COMMS_STORED_MAX;
    CliCommsStoredMsg *const stored_msg = &args->traffic->last[place];
    const int full = args->traffic->count == CLI_COMMS_STORED_MAX;
    const int fits = full && (need <= stored_msg->capacity);
    char *const cell = fits ? stored_msg->from : g_cli_guard->persistent_buf_alloc(need, g_cli_comms_owner);
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
        g_cli_guard->persistent_buf_release(stored_msg->from, g_cli_comms_owner);
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = from, .bytes = (size_t)from_size});
    memor.cpy(&(MemoriaCfg){.dst = cell + from_size, .src = stored, .bytes = (size_t)stored_size});
    stored_msg->from = cell;
    stored_msg->body = cell + from_size;
    stored_msg->size = stored_size;
    stored_msg->capacity = fits ? stored_msg->capacity : need;
}

int cli_comms_send(const CliCommsChannelErrMsgCfg *args)
{
    unsigned long long *delivered = args->delivered;
    // the listeners called once the lock is let go, each its function and context, in this speaker's own work state, as
    // a listener called may send in turn
    CliCommsRecvFn recv_fns[CLI_COMMS_LISTENERS];
    void *calls_context[CLI_COMMS_LISTENERS];
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at == CLI_COMMS_PARTS)
    {
        mmgr_line_put(args->err_msg, "that key is no part's");
        return 0;
    }
    CliCommsPart *const part = &g_cli_comms.parts[at];
    char name[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = name,
                            .src = part->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_POOL_BYTES}) + 1u});
    int granted = 0;
    for (unsigned int grant = 0u; (grant < part->send_grants) && !granted; grant += 1u)
    {
        granted = EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = part->send_grant[grant],
                             .channel = args->channel);
    }
    CliCommsTraffic *const traffic = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (!granted)
    {
        part->refused += 1ULL;
        if (traffic != NULL)
        {
            traffic->refused += 1ULL;
        }
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " may not send on ");
        mmgr_line_put(args->err_msg, args->channel);
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
        return 0;
    }
    part->sent_count += 1ULL;
    if (traffic != NULL)
    {
        traffic->messages += 1ULL;
        traffic->bytes += args->size;
        EMBED_CALL(comms_store, CommsCtx, .traffic = traffic, .from = name, .body = args->body, .size = args->size);
    }
    (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    unsigned long long count = 0ULL;
    unsigned int calls = 0u;
    for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
    {
        CliCommsListener *const listener = &g_cli_comms.listeners[one];
        if (!listener->reserved ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener->pattern, .channel = args->channel))
        {
            continue;
        }
        if (!listener->queued)
        {
            recv_fns[calls] = listener->recv;
            calls_context[calls] = listener->context;
            calls += 1u;
            count += 1ULL;
        }
        else
        {
            const int got = EMBED_CALL(listener->recv, CliCommsRecvCfg, .context = listener->context,
                                       .channel = args->channel, .from = name, .body = args->body, .size = args->size);
            count += (got > 0) ? 1ULL : 0ULL;
            // a listener whose reader is gone is let go
            listener->reserved = (got == 0) ? 0 : listener->reserved;
        }
    }
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
    CliCommsTraffic *const traffic = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (traffic != NULL)
    {
        traffic->transferred += args->bytes;
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}
