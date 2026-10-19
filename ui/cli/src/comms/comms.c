// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the parts registered and their grants, the listeners on each channel, and each message said: refused where
// its speaker may not speak there, else counted, kept among its channel's last, and handed to every listener the
// channel's name is granted to
#include "comms_internal.h"

#include "../text/text.h"

CliCommsRoom g_cli_comms = {.lock = CLI_LOCK_INIT};

int cli_comms_grants(const char *grant, const char *channel)
{
    if ((cellul.eq(&(CatenaFinitaCfg){.src = grant, .other = "*", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = grant, .other = channel, .cap = CLI_CELLS_POOL})))
    {
        return 1;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = grant, .cap = CLI_CELLS_POOL});
    // a grant ending in /* names every channel under the name before it
    if ((length < 2u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = grant + length - 2u, .other = "/*", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const size_t under = length - 2u;
    return cli_text_same(channel, grant, under) && (channel[under] == '/');
}

int cli_comms_within(const char *inner, const char *outer)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = outer, .other = "*", .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = inner, .other = "*", .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    char inner_under[CLI_NAME_LONGEST];
    char outer_under[CLI_NAME_LONGEST];
    const size_t inner_length = cellul.len(&(CatenaFinitaCfg){.src = inner, .cap = CLI_CELLS_POOL});
    const size_t outer_length = cellul.len(&(CatenaFinitaCfg){.src = outer, .cap = CLI_CELLS_POOL});
    const int inner_all =
        (inner_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = inner + inner_length - 2u, .other = "/*", .cap = CLI_CELLS_POOL}));
    const int outer_all =
        (outer_length >= 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = outer + outer_length - 2u, .other = "/*", .cap = CLI_CELLS_POOL}));
    if (inner_all && !outer_all)
    {
        return 0;
    }
    if (!inner_all)
    {
        return cli_comms_grants(outer, inner);
    }
    CliLine inner_line = {inner_under, sizeof(inner_under), 0ull};
    cli_line_counted(&inner_line, inner, inner_length - 2u);
    cli_line_closed(&inner_line);
    CliLine outer_line = {outer_under, sizeof(outer_under), 0ull};
    cli_line_counted(&outer_line, outer, outer_length - 2u);
    cli_line_closed(&outer_line);
    return (cellul.eq(&(CatenaFinitaCfg){.src = inner_under, .other = outer_under, .cap = CLI_CELLS_POOL})) ||
           cli_comms_grants(outer, inner_under);
}

// the place of the part of `key`, CLI_COMMS_PARTS where it is no part's, under the lock
static unsigned int comms_part_of(const char *key)
{
    for (unsigned int at = 0u; at < CLI_COMMS_PARTS; at += 1u)
    {
        if (g_cli_comms.parts[at].taken &&
            (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_comms.parts[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return CLI_COMMS_PARTS;
}

int cli_comms_register(const char *name, int has_process, unsigned int process, const char *const *listen,
                       unsigned int listens, const char *const *speak, unsigned int speaks, char *key, CliLine *said)
{
    unsigned char secret[32];
    if (!cli_random_bytes(secret, sizeof(secret)))
    {
        cli_line_text(said, "the system gave no secret");
        return 0;
    }
    if ((listens > CLI_COMMS_GRANTS) || (speaks > CLI_COMMS_GRANTS))
    {
        cli_line_text(said, "a part holds at most 16 grants to listen and 16 to speak");
        return 0;
    }
    CliLine written = {key, CLI_COMMS_KEY, 0ull};
    for (unsigned int at = 0u; at < sizeof(secret); at += 1u)
    {
        cli_line_character(&written, "0123456789abcdef"[secret[at] >> 4u]);
        cli_line_character(&written, "0123456789abcdef"[secret[at] & 15u]);
    }
    cli_line_closed(&written);
    cli_lock_take(&g_cli_comms.lock);
    CliCommsPart *part = NULL;
    for (unsigned int at = 0u; (at < CLI_COMMS_PARTS) && (part == NULL); at += 1u)
    {
        part = g_cli_comms.parts[at].taken ? NULL : &g_cli_comms.parts[at];
    }
    if (part == NULL)
    {
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, "the comms layer holds as many parts as it can");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = part, .val = 0, .bytes = sizeof(*part)});
    part->taken = 1;
    memor.cpy(&(MemoriaCfg){
        .dst = part->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = part->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    part->has_process = has_process;
    part->process = process;
    for (unsigned int at = 0u; at < listens; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = part->listen[at],
                          .src = listen[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = listen[at], .cap = CLI_CELLS_POOL}) + 1u});
    }
    for (unsigned int at = 0u; at < speaks; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = part->speak[at],
                                .src = speak[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = speak[at], .cap = CLI_CELLS_POOL}) + 1u});
    }
    part->listens = listens;
    part->speaks = speaks;
    cli_lock_give(&g_cli_comms.lock);
    return 1;
}

int cli_comms_register_held(const char *name, const char *const *listen, unsigned int listens, const char *const *speak,
                            unsigned int speaks, char *key, CliLine *said)
{
    if (!cli_comms_register(name, 0, 0u, listen, listens, speak, speaks, key, said))
    {
        return 0;
    }
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].held = 1;
    }
    cli_lock_give(&g_cli_comms.lock);
    return 1;
}

// the first of `asked`, `count` of them, that no grant of `held`, `held_count` of them, holds within it, NULL where
// each is held
static const char *comms_beyond(const char *const *asked, unsigned int count, const char (*held)[CLI_NAME_LONGEST],
                                unsigned int held_count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        int within = 0;
        for (unsigned int grant = 0u; (grant < held_count) && !within; grant += 1u)
        {
            within = cli_comms_within(asked[at], held[grant]);
        }
        if (!within)
        {
            return asked[at];
        }
    }
    return NULL;
}

int cli_comms_register_under(const char *parent, const char *name, int has_process, unsigned int process,
                             const char *const *listen, unsigned int listens, const char *const *speak,
                             unsigned int speaks, char *key, CliLine *said)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(parent);
    if (at == CLI_COMMS_PARTS)
    {
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, "that key is no part's");
        return 0;
    }
    const CliCommsPart *const granter = &g_cli_comms.parts[at];
    const char *beyond =
        comms_beyond(listen, listens, (const char (*)[CLI_NAME_LONGEST])granter->listen, granter->listens);
    beyond = (beyond != NULL)
                 ? beyond
                 : comms_beyond(speak, speaks, (const char (*)[CLI_NAME_LONGEST])granter->speak, granter->speaks);
    if (beyond != NULL)
    {
        cli_line_text(said, granter->name);
        cli_line_text(said, " may not grant ");
        cli_line_text(said, beyond);
        cli_line_text(said, ", which is not its own");
        cli_lock_give(&g_cli_comms.lock);
        return 0;
    }
    cli_lock_give(&g_cli_comms.lock);
    return cli_comms_register(name, has_process, process, listen, listens, speak, speaks, key, said);
}

int cli_comms_held(const char *key)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    const int held = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].held;
    cli_lock_give(&g_cli_comms.lock);
    return held;
}

void cli_comms_bind(const char *key, unsigned int process)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].has_process = 1;
        g_cli_comms.parts[at].process = process;
    }
    cli_lock_give(&g_cli_comms.lock);
}

void cli_comms_leave(const char *key)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at < CLI_COMMS_PARTS)
    {
        g_cli_comms.parts[at].taken = 0;
        for (unsigned int one = 0u; one < CLI_COMMS_LISTENERS; one += 1u)
        {
            CliCommsListener *const listener = &g_cli_comms.listeners[one];
            listener->taken = (listener->taken && (listener->part == at)) ? 0 : listener->taken;
        }
    }
    cli_lock_give(&g_cli_comms.lock);
}

int cli_comms_process_of(const char *key, unsigned int *process)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    const int has = (at < CLI_COMMS_PARTS) && g_cli_comms.parts[at].has_process;
    *process = has ? g_cli_comms.parts[at].process : 0u;
    cli_lock_give(&g_cli_comms.lock);
    return has;
}

int cli_comms_name_of(const char *key, char *name)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at < CLI_COMMS_PARTS)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = name,
            .src = g_cli_comms.parts[at].name,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_comms.parts[at].name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_lock_give(&g_cli_comms.lock);
    return at < CLI_COMMS_PARTS;
}

int cli_comms_listen(const char *key, const char *channel, int queued, CliCommsHeard heard, void *context,
                     unsigned int *listener, CliLine *said)
{
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at == CLI_COMMS_PARTS)
    {
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, "that key is no part's");
        return 0;
    }
    const CliCommsPart *const part = &g_cli_comms.parts[at];
    int granted = 0;
    for (unsigned int grant = 0u; (grant < part->listens) && !granted; grant += 1u)
    {
        granted = cli_comms_within(channel, part->listen[grant]);
    }
    if (!granted)
    {
        cli_line_text(said, part->name);
        cli_line_text(said, " may not listen on ");
        cli_line_text(said, channel);
        cli_lock_give(&g_cli_comms.lock);
        return 0;
    }
    for (unsigned int one = 0u; one < CLI_COMMS_LISTENERS; one += 1u)
    {
        CliCommsListener *const place = &g_cli_comms.listeners[one];
        if (!place->taken)
        {
            *place = (CliCommsListener){1, at, "", queued, heard, context};
            memor.cpy(
                &(MemoriaCfg){.dst = place->pattern,
                              .src = channel,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
            *listener = one;
            cli_lock_give(&g_cli_comms.lock);
            return 1;
        }
    }
    cli_lock_give(&g_cli_comms.lock);
    cli_line_text(said, "the comms layer holds as many listeners as it can");
    return 0;
}

void cli_comms_unlisten(unsigned int listener)
{
    cli_lock_take(&g_cli_comms.lock);
    if (listener < CLI_COMMS_LISTENERS)
    {
        g_cli_comms.listeners[listener].taken = 0;
    }
    cli_lock_give(&g_cli_comms.lock);
}

// the traffic of `channel`, made where it has none, NULL where the channels kept are full, under the lock
static CliCommsTraffic *comms_traffic_of(const char *channel)
{
    for (unsigned int at = 0u; at < g_cli_comms.channels; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_comms.traffic[at].name, .other = channel, .cap = CLI_CELLS_POOL}))
        {
            return &g_cli_comms.traffic[at];
        }
    }
    if (g_cli_comms.channels == CLI_COMMS_CHANNELS)
    {
        return NULL;
    }
    CliCommsTraffic *const made = &g_cli_comms.traffic[g_cli_comms.channels];
    g_cli_comms.channels += 1u;
    memor.set(&(MemoriaCfg){.dst = made, .val = 0, .bytes = sizeof(*made)});
    memor.cpy(&(MemoriaCfg){.dst = made->name,
                            .src = channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
    return made;
}

// a message kept among its channel's last, the oldest let go past the most kept
static void comms_kept(CliCommsTraffic *traffic, const char *from, const char *body, unsigned long long size)
{
    const unsigned int place = (traffic->first + traffic->count) % CLI_COMMS_KEPT;
    CliCommsKept *const kept = &traffic->last[place];
    if (traffic->count == CLI_COMMS_KEPT)
    {
        traffic->first = (traffic->first + 1u) % CLI_COMMS_KEPT;
    }
    else
    {
        traffic->count += 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = kept->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    CliLine line = {kept->body, sizeof(kept->body), 0ull};
    if (size < sizeof(kept->body))
    {
        cli_line_counted(&line, body, size);
    }
    else
    {
        cli_line_text(&line, "\"a body of ");
        cli_line_decimal(&line, size, 1u);
        cli_line_text(&line, " bytes\"");
    }
    kept->size = line.at;
}

int cli_comms_say(const char *key, const char *channel, const char *body, unsigned long long size,
                  unsigned long long *heard, CliLine *said)
{
    // the listeners called once the lock is let go, each its function and context, in this speaker's own room, as a
    // listener called may speak in turn
    CliCommsHeard calls_heard[CLI_COMMS_LISTENERS];
    void *calls_context[CLI_COMMS_LISTENERS];
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int at = comms_part_of(key);
    if (at == CLI_COMMS_PARTS)
    {
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, "that key is no part's");
        return 0;
    }
    CliCommsPart *const part = &g_cli_comms.parts[at];
    char name[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = name,
                            .src = part->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}) + 1u});
    int granted = 0;
    for (unsigned int grant = 0u; (grant < part->speaks) && !granted; grant += 1u)
    {
        granted = cli_comms_grants(part->speak[grant], channel);
    }
    CliCommsTraffic *const traffic = comms_traffic_of(channel);
    if (!granted)
    {
        part->refused += 1ull;
        if (traffic != NULL)
        {
            traffic->refused += 1ull;
        }
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, name);
        cli_line_text(said, " may not speak on ");
        cli_line_text(said, channel);
        return 0;
    }
    part->said += 1ull;
    if (traffic != NULL)
    {
        traffic->messages += 1ull;
        traffic->bytes += size;
        comms_kept(traffic, name, body, size);
    }
    unsigned long long count = 0ull;
    unsigned int calls = 0u;
    for (unsigned int one = 0u; one < CLI_COMMS_LISTENERS; one += 1u)
    {
        CliCommsListener *const listener = &g_cli_comms.listeners[one];
        if (!listener->taken || !cli_comms_grants(listener->pattern, channel))
        {
            continue;
        }
        if (!listener->queued)
        {
            calls_heard[calls] = listener->heard;
            calls_context[calls] = listener->context;
            calls += 1u;
            count += 1ull;
        }
        else
        {
            const int got = listener->heard(listener->context, channel, name, body, size);
            count += (got > 0) ? 1ull : 0ull;
            // a listener whose reader is gone is let go
            listener->taken = (got == 0) ? 0 : listener->taken;
        }
    }
    cli_lock_give(&g_cli_comms.lock);
    for (unsigned int one = 0u; one < calls; one += 1u)
    {
        (void)calls_heard[one](calls_context[one], channel, name, body, size);
    }
    *heard = count;
    return 1;
}

void cli_comms_transferred(const char *channel, unsigned long long bytes)
{
    cli_lock_take(&g_cli_comms.lock);
    CliCommsTraffic *const traffic = comms_traffic_of(channel);
    if (traffic != NULL)
    {
        traffic->transferred += bytes;
    }
    cli_lock_give(&g_cli_comms.lock);
}
