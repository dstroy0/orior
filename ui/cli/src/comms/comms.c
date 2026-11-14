// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the parts registered and their grants, the listeners on each channel, and each message sent: refused where
// its sender may not send there, else counted, stored among its channel's last, and handed to every listener the
// channel's name is granted to. The tables, each part's grants and each stored message live in movable cells, read by
// value and written back
#include "../command_line/callback_cfg.h"
#include "comms_internal.h"

#include "../command_line/rings.h"

#include "../text/text.h"

CliCommsWorkState g_cli_comms = {0};

// the tables taken in their one movable cell, all empty, as the first of them is needed; under the lock
static void comms_tables_init(void)
{
    if (g_cli_comms.tables != NULL)
    {
        return;
    }
    const CarcerMovableEntry *const tables = g_cli_guard->movable_buf_alloc(COMMS_TABLES_BYTES, g_cli_comms_owner);
    if (tables == NULL)
    {
        return;
    }
    // the stored rings borrow their bytes, so they stand fixed, outside the pool, where they punch no hole in the
    // persistent tier; emptied here for each begin
    static CliCommsStored s_stored[CLI_COMMS_CHANNELS];
    // every place empty: the whole cell written as zeroes in one write from a temporary cell
    unsigned char *const zeroes = g_cli_guard->temporary_buf_alloc(COMMS_TABLES_BYTES, g_cli_comms_owner);
    if (zeroes == NULL)
    {
        (void)g_cli_guard->movable_buf_release(tables, g_cli_comms_owner);
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_stored, .val = 0, .bytes = sizeof(s_stored)});
    // each channel's messages lie in the channel's span of the comms part of the rings block
    unsigned char *const messages = EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_COMMS);
    for (unsigned int at = 0u; at < CLI_COMMS_CHANNELS; at += 1u)
    {
        s_stored[at].last = (CliCommsStoredMsg *)(void *)(messages + ((unsigned long long)at * CLI_COMMS_STORED_BYTES));
    }
    g_cli_comms.stored = s_stored;
    memor.set(&(MemoriaCfg){.dst = zeroes, .val = 0, .bytes = (embed_word)COMMS_TABLES_BYTES});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = tables, .src = zeroes, .bytes = COMMS_TABLES_BYTES});
    g_cli_guard->persistent_buf_release(zeroes, g_cli_comms_owner);
    g_cli_comms.tables = tables;
    g_cli_comms.parts_capacity = CLI_COMMS_PARTS;
    g_cli_comms.listeners_capacity = CLI_COMMS_LISTENERS;
    g_cli_comms.channels_capacity = CLI_COMMS_CHANNELS;
}

void cli_comms_end(void)
{
    if (g_cli_comms.tables == NULL)
    {
        return;
    }
    // every reserved part's grants cell let go
    for (unsigned int at = 0u; at < g_cli_comms.parts_capacity; at += 1u)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        if (part.reserved && (part.grants != NULL))
        {
            (void)g_cli_guard->movable_buf_release(part.grants, g_cli_comms_owner);
        }
    }
    // every channel's stored messages' cells let go, then the stored rings' cell and the tables
    for (unsigned int at = 0u; at < g_cli_comms.channels; at += 1u)
    {
        CliCommsStored *const ring = &g_cli_comms.stored[at];
        size_t front = 0u;
        (void)anularis.seg_front(&(AnularisCfg){.ring = &ring->ring, .out_index = &front});
        const size_t stored = anularis.seg_inflight(&(AnularisCfg){.ring = &ring->ring});
        for (size_t one = 0u; one < stored; one += 1u)
        {
            (void)g_cli_guard->movable_buf_release(ring->last[MMGR_RING_WRAP(front + one, CLI_COMMS_STORED_RING)].cell,
                                                   g_cli_comms_owner);
        }
    }
    (void)g_cli_guard->movable_buf_release(g_cli_comms.tables, g_cli_comms_owner);
    g_cli_comms = (CliCommsWorkState){0};
}

typedef struct
{
    const char *key;
    const char *const *required;
    unsigned int count;
    const CliCommsPart *part;
    unsigned int from;
    unsigned int granted_count;
    const char *channel;
    unsigned int traffic_at;
    const char *from_name;
    const char *body;
    unsigned long long size;
    unsigned int part_at;
    int refused;
} CommsCtx;
// the place of the part of `key`, CLI_COMMS_PARTS where it is no part's, under the lock
EMBED_INLINE unsigned int comms_part_get(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_comms.parts_capacity; at += 1u)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        if (part.reserved &&
            (cellul.eq(&(CatenaFinitaCfg){.src = part.key, .other = args->key, .cap = CLI_POOL_BYTES})))
        {
            return at;
        }
    }
    return CLI_COMMS_PARTS;
}

// whether `channel` is within one of `part`'s grants from `from`, `granted_count` of them
EMBED_INLINE int comms_granted(const CommsCtx *args)
{
    int granted = 0;
    for (unsigned int grant = 0u; (grant < args->granted_count) && !granted; grant += 1u)
    {
        char outer[CLI_NAME_MAX_LEN];
        outer[0] = '\0';
        if ((args->part->grants != NULL) && ((args->from + grant) < args->part->listens + args->part->send_grants))
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = args->part->grants,
                                    .offset = (unsigned long long)(args->from + grant) * CLI_NAME_MAX_LEN,
                                    .dst = outer,
                                    .bytes = CLI_NAME_MAX_LEN});
        }
        granted = EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = args->channel, .outer = outer);
    }
    return granted;
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
    unsigned int place = CLI_COMMS_PARTS;
    for (unsigned int at = 0u; (at < g_cli_comms.parts_capacity) && (place == CLI_COMMS_PARTS); at += 1u)
    {
        CliCommsPart part_1 = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part_1)),
                                .dst = &part_1,
                                .bytes = sizeof(part_1)});

        place = part_1.reserved ? place : at;
    }
    const unsigned int grants = listens + send_grants;
    const CarcerMovableEntry *const granted =
        ((place != CLI_COMMS_PARTS) && (grants != 0u))
            ? g_cli_guard->movable_buf_alloc((unsigned long long)grants * CLI_NAME_MAX_LEN, g_cli_comms_owner)
            : NULL;
    if ((place == CLI_COMMS_PARTS) || ((grants != 0u) && (granted == NULL)))
    {
        mmgr_line_put(args->err_msg, "the comms layer has as many parts as it can");
        return 0;
    }
    CliCommsPart part;
    memor.set(&(MemoriaCfg){.dst = &part, .val = 0, .bytes = sizeof(part)});
    part.reserved = 1;
    part.grants = granted;
    memor.cpy(&(MemoriaCfg){.dst = part.key,
                            .src = args->key_out,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key_out, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = part.name,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
    part.has_process = has_process;
    part.process = process;
    for (unsigned int at = 0u; at < grants; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        memor.set(&(MemoriaCfg){.dst = name, .val = 0, .bytes = sizeof(name)});
        const char *const grant = (at < listens) ? listen[at] : send_grant[at - listens];
        memor.cpy(&(MemoriaCfg){.dst = name,
                                .src = grant,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = grant, .cap = CLI_POOL_BYTES}) + 1u});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = granted, .offset = (unsigned long long)at * CLI_NAME_MAX_LEN, .src = name, .bytes = sizeof(name)});
    }
    part.listens = listens;
    part.send_grants = send_grants;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)place * sizeof(CliCommsPart)),
                            .src = &part,
                            .bytes = sizeof(CliCommsPart)});
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
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        part.persistent = 1;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(CliCommsPart)),
                                .src = &part,
                                .bytes = sizeof(CliCommsPart)});
    }
    return 1;
}

// the first of `required`, `count` of them, that no grant of `part` from `from`, `granted_count` of them, contains
// within it, NULL where each is granted
EMBED_INLINE const char *comms_beyond(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (!EMBED_CALL(comms_granted, CommsCtx, .part = args->part, .from = args->from,
                        .granted_count = args->granted_count, .channel = args->required[at]))
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
    CliCommsPart granter = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(granter)),
                            .dst = &granter,
                            .bytes = sizeof(granter)});
    const char *beyond = EMBED_CALL(comms_beyond, CommsCtx, .required = args->listen, .count = args->listens,
                                    .part = &granter, .from = 0u, .granted_count = granter.listens);
    beyond = (beyond != NULL)
                 ? beyond
                 : EMBED_CALL(comms_beyond, CommsCtx, .required = args->send_grant, .count = args->send_grants,
                              .part = &granter, .from = granter.listens, .granted_count = granter.send_grants);
    if (beyond != NULL)
    {
        mmgr_line_put(args->err_msg, granter.name);
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
    CliCommsPart part_2 = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part_2)),
                            .dst = &part_2,
                            .bytes = sizeof(part_2)});

    const int persistent = (at < CLI_COMMS_PARTS) && part_2.persistent;
    return persistent;
}

void cli_comms_bind(const CliCommsProcessCfg *args)
{
    unsigned int process = args->process;
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        part.has_process = 1;
        part.process = process;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(CliCommsPart)),
                                .src = &part,
                                .bytes = sizeof(CliCommsPart)});
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

void cli_comms_leave(const CliCommsProcessCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        part.reserved = 0;
        (void)g_cli_guard->movable_buf_release(part.grants, g_cli_comms_owner);
        part.grants = NULL;
        part.listens = 0u;
        part.send_grants = 0u;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(CliCommsPart)),
                                .src = &part,
                                .bytes = sizeof(CliCommsPart)});
        for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
        {
            CliCommsListener listener = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                    .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(listener)),
                                    .dst = &listener,
                                    .bytes = sizeof(listener)});
            if (listener.reserved && (listener.part == at))
            {
                listener.reserved = 0;
                (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = g_cli_comms.tables,
                    .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(CliCommsListener)),
                    .src = &listener,
                    .bytes = sizeof(CliCommsListener)});
            }
        }
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

int cli_comms_process_get(const CliCommsKeyProcessCfg *args)
{
    unsigned int *process = args->process_pointer;
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    CliCommsPart part = {0};
    if (at < CLI_COMMS_PARTS)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
    }
    const int has = (at < CLI_COMMS_PARTS) && part.has_process;
    *process = has ? part.process : 0u;
    return has;
}

int cli_comms_name_get(const CliCommsKeyCfg *args)
{
    const unsigned int at = EMBED_CALL(comms_part_get, CommsCtx, .key = args->key);
    if (at < CLI_COMMS_PARTS)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        memor.cpy(&(MemoriaCfg){.dst = args->name_out,
                                .src = part.name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = part.name, .cap = CLI_POOL_BYTES}) + 1u});
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
    CliCommsPart part = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                            .dst = &part,
                            .bytes = sizeof(part)});
    if (!EMBED_CALL(comms_granted, CommsCtx, .part = &part, .from = 0u, .granted_count = part.listens,
                    .channel = args->channel))
    {
        mmgr_line_put(args->err_msg, part.name);
        mmgr_line_put(args->err_msg, " may not listen on ");
        mmgr_line_put(args->err_msg, args->channel);
        return 0;
    }
    comms_tables_init();
    unsigned int vacant = g_cli_comms.listeners_capacity;
    for (unsigned int one = 0u; (one < g_cli_comms.listeners_capacity) && (vacant == g_cli_comms.listeners_capacity);
         one += 1u)
    {
        CliCommsListener listener_3 = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(listener_3)),
                                .dst = &listener_3,
                                .bytes = sizeof(listener_3)});

        vacant = listener_3.reserved ? vacant : one;
    }
    if (vacant < g_cli_comms.listeners_capacity)
    {
        CliCommsListener place = {1, at, "", args->queued, args->recv, args->context};
        memor.cpy(
            &(MemoriaCfg){.dst = place.pattern,
                          .src = args->channel,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_POOL_BYTES}) + 1u});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_LISTENERS_AT + ((unsigned long long)vacant * sizeof(CliCommsListener)),
                                .src = &place,
                                .bytes = sizeof(CliCommsListener)});
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
        CliCommsListener listener = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = g_cli_comms.tables,
            .offset = COMMS_LISTENERS_AT + ((unsigned long long)args->listener_value * sizeof(listener)),
            .dst = &listener,
            .bytes = sizeof(listener)});
        listener.reserved = 0;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = g_cli_comms.tables,
            .offset = COMMS_LISTENERS_AT + ((unsigned long long)args->listener_value * sizeof(CliCommsListener)),
            .src = &listener,
            .bytes = sizeof(CliCommsListener)});
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}

// the place of the traffic of `channel`, made where it has none, CLI_COMMS_CHANNELS where the channels stored are
// full; under the lock
EMBED_INLINE unsigned int comms_traffic_get(const CommsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_comms.channels; at += 1u)
    {
        CliCommsTraffic traffic = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_TRAFFIC_AT + ((unsigned long long)at * sizeof(traffic)),
                                .dst = &traffic,
                                .bytes = sizeof(traffic)});
        if (cellul.eq(&(CatenaFinitaCfg){.src = traffic.name, .other = args->channel, .cap = CLI_POOL_BYTES}))
        {
            return at;
        }
    }
    comms_tables_init();
    if ((g_cli_comms.tables == NULL) || (g_cli_comms.channels >= g_cli_comms.channels_capacity))
    {
        return CLI_COMMS_CHANNELS;
    }
    const unsigned int at = g_cli_comms.channels;
    g_cli_comms.channels += 1u;
    CliCommsStored *const stored = &g_cli_comms.stored[at];
    (void)anularis.init(&(AnularisCfg){.ring = &stored->ring,
                                       .buf = (embed_u8 *)(void *)stored->last,
                                       .capacity = CLI_COMMS_STORED_BYTES,
                                       .segment_count = CLI_COMMS_STORED_RING,
                                       .segment_bytes = sizeof(CliCommsStoredMsg)});
    CliCommsTraffic made;
    memor.set(&(MemoriaCfg){.dst = &made, .val = 0, .bytes = sizeof(made)});
    memor.cpy(&(MemoriaCfg){.dst = made.name,
                            .src = args->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_POOL_BYTES}) + 1u});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_TRAFFIC_AT + ((unsigned long long)at * sizeof(CliCommsTraffic)),
                            .src = &made,
                            .bytes = sizeof(CliCommsTraffic)});
    return at;
}

// the part `part_at` written back to the parts' table as `part`
EMBED_INLINE void comms_part_write(const CommsCtx *args)
{
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)args->part_at * sizeof(CliCommsPart)),
                            .src = args->part,
                            .bytes = sizeof(CliCommsPart)});
}

// the channel's traffic at `traffic_at` counted, nothing where it is CLI_COMMS_CHANNELS: a send refused where
// `refused`, else a message of `size` bytes
EMBED_INLINE void comms_traffic_counted(const CommsCtx *args)
{
    if (args->traffic_at == CLI_COMMS_CHANNELS)
    {
        return;
    }
    CliCommsTraffic traffic = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_TRAFFIC_AT + ((unsigned long long)args->traffic_at * sizeof(traffic)),
                            .dst = &traffic,
                            .bytes = sizeof(traffic)});
    traffic.refused += args->refused ? 1ULL : 0ULL;
    traffic.messages += args->refused ? 0ULL : 1ULL;
    traffic.bytes += args->refused ? 0ULL : args->size;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_TRAFFIC_AT + ((unsigned long long)args->traffic_at * sizeof(traffic)),
                            .src = &traffic,
                            .bytes = sizeof(traffic)});
}

// a message stored among the traffic at `traffic_at`'s last, past the most stored over the oldest: in the oldest's
// cell where it fits, else in a larger cell with the oldest's given back; stored not at all where the cell is refused
EMBED_INLINE void comms_store(const CommsCtx *args)
{
    const char *from = args->from_name;
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
    CliCommsStored *const ring = &g_cli_comms.stored[args->traffic_at];
    const int full = anularis.seg_inflight(&(AnularisCfg){.ring = &ring->ring}) == CLI_COMMS_STORED_MAX;
    CliCommsStoredMsg oldest = {0};
    if (full)
    {
        size_t front = 0u;
        (void)anularis.seg_front(&(AnularisCfg){.ring = &ring->ring, .out_index = &front});
        oldest = ring->last[front];
    }
    const int fits = full && (need <= oldest.capacity);
    const CarcerMovableEntry *const cell = fits ? oldest.cell : g_cli_guard->movable_buf_alloc(need, g_cli_comms_owner);
    if (cell == NULL)
    {
        return;
    }
    if (full)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &ring->ring});
    }
    if (full && !fits)
    {
        (void)g_cli_guard->movable_buf_release(oldest.cell, g_cli_comms_owner);
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = from, .bytes = from_size});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = from_size, .src = stored, .bytes = stored_size});
    size_t segment = 0u;
    (void)anularis.seg_next(&(AnularisCfg){.ring = &ring->ring, .out_index = &segment});
    ring->last[segment] = (CliCommsStoredMsg){cell, from_size, stored_size, fits ? oldest.capacity : need};
    anularis.seg_publish(&(AnularisCfg){.ring = &ring->ring});
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
    CliCommsPart part = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                            .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part)),
                            .dst = &part,
                            .bytes = sizeof(part)});
    char name[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = name,
                            .src = part.name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = part.name, .cap = CLI_POOL_BYTES}) + 1u});
    const int granted = EMBED_CALL(comms_granted, CommsCtx, .part = &part, .from = part.listens,
                                   .granted_count = part.send_grants, .channel = args->channel);
    const unsigned int traffic_at = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (!granted)
    {
        part.refused += 1ULL;
        EMBED_CALL(comms_part_write, CommsCtx, .part_at = at, .part = &part);
        EMBED_CALL(comms_traffic_counted, CommsCtx, .traffic_at = traffic_at, .refused = 1);
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " may not send on ");
        mmgr_line_put(args->err_msg, args->channel);
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
        return 0;
    }
    part.sent_count += 1ULL;
    EMBED_CALL(comms_part_write, CommsCtx, .part_at = at, .part = &part);
    if (traffic_at != CLI_COMMS_CHANNELS)
    {
        EMBED_CALL(comms_traffic_counted, CommsCtx, .traffic_at = traffic_at, .size = args->size);
        EMBED_CALL(comms_store, CommsCtx, .traffic_at = traffic_at, .from_name = name, .body = args->body,
                   .size = args->size);
    }
    (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    unsigned long long count = 0ULL;
    unsigned int calls = 0u;
    for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
    {
        CliCommsListener listener = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(listener)),
                                .dst = &listener,
                                .bytes = sizeof(listener)});
        if (!listener.reserved ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener.pattern, .channel = args->channel))
        {
            continue;
        }
        if (!listener.queued)
        {
            recv_fns[calls] = listener.recv;
            calls_context[calls] = listener.context;
            calls += 1u;
            count += 1ULL;
        }
        else
        {
            const int got = EMBED_CALL(listener.recv, CliCommsRecvCfg, .context = listener.context,
                                       .channel = args->channel, .from = name, .body = args->body, .size = args->size);
            count += (got > 0) ? 1ULL : 0ULL;
            // a listener whose reader is gone is let go
            if (got == 0)
            {
                listener.reserved = 0;
                (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = g_cli_comms.tables,
                    .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(CliCommsListener)),
                    .src = &listener,
                    .bytes = sizeof(CliCommsListener)});
            }
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
    const unsigned int traffic_at = EMBED_CALL(comms_traffic_get, CommsCtx, .channel = args->channel);
    if (traffic_at != CLI_COMMS_CHANNELS)
    {
        CliCommsTraffic traffic = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_TRAFFIC_AT + ((unsigned long long)traffic_at * sizeof(traffic)),
                                .dst = &traffic,
                                .bytes = sizeof(traffic)});
        traffic.transferred += args->bytes;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_TRAFFIC_AT + ((unsigned long long)traffic_at * sizeof(CliCommsTraffic)),
                                .src = &traffic,
                                .bytes = sizeof(CliCommsTraffic)});
        (void)EMBED_ATOMIC_ADD_32(&g_cli_comms.stamp, 1u);
    }
}
