// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.c: the parts registered and their grants, the listeners on each channel, and each message said: refused where
// its speaker may not speak there, else counted, kept among its channel's last, and handed to every listener the
// channel's name is granted to
#include "comms_internal.h"

#include "../text/text.h"

CliCommsRoom g_cli_comms = {.lock = CLI_LOCK_INIT};
CliCellsTally g_cli_comms_cells = CLI_CELLS_TALLY("cli_comms");

// a table of `*room` places of `width` bytes grown to twice as many, or to CLI_COMMS_FIRST where it holds none, up to
// `most`, the places added empty and `table` given back: the table, or NULL where it holds `most` or the cell is
// refused and `table` is kept, under the lock
static void *comms_grown(void *table, unsigned int *room, unsigned int most, unsigned long long width, const char *what)
{
    if (*room >= most)
    {
        return NULL;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_COMMS_FIRST : (2u * *room);
    const unsigned int taken = (doubled < most) ? doubled : most;
    void *const grown = cli_cell_taken_by(&g_cli_comms_cells, (unsigned long long)taken * width, what);
    if (grown == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = grown, .val = 0, .bytes = (size_t)((unsigned long long)taken * width)});
    if (table != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = table, .bytes = (size_t)((unsigned long long)*room * width)});
        cli_cell_given_by(&g_cli_comms_cells, table);
    }
    *room = taken;
    return grown;
}

// the place of the part of `key`, CLI_COMMS_PARTS where it is no part's, under the lock
static unsigned int comms_part_of(const char *key)
{
    for (unsigned int at = 0u; at < g_cli_comms.parts_room; at += 1u)
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
    for (unsigned int at = 0u; (at < g_cli_comms.parts_room) && (part == NULL); at += 1u)
    {
        part = g_cli_comms.parts[at].taken ? NULL : &g_cli_comms.parts[at];
    }
    if (part == NULL)
    {
        const unsigned int place = g_cli_comms.parts_room;
        CliCommsPart *const grown = comms_grown(g_cli_comms.parts, &g_cli_comms.parts_room, CLI_COMMS_PARTS,
                                                sizeof(CliCommsPart), "the comms layer's parts");
        g_cli_comms.parts = (grown != NULL) ? grown : g_cli_comms.parts;
        part = (grown != NULL) ? &g_cli_comms.parts[place] : NULL;
    }
    const unsigned int grants = listens + speaks;
    char (*const granted)[CLI_NAME_LONGEST] =
        ((part != NULL) && (grants != 0u))
            ? cli_cell_taken_by(&g_cli_comms_cells, (unsigned long long)grants * CLI_NAME_LONGEST, "a part's grants")
            : NULL;
    if ((part == NULL) || ((grants != 0u) && (granted == NULL)))
    {
        cli_lock_give(&g_cli_comms.lock);
        cli_line_text(said, "the comms layer holds as many parts as it can");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = part, .val = 0, .bytes = sizeof(*part)});
    part->taken = 1;
    part->listen = granted;
    part->speak = (granted != NULL) ? (granted + listens) : NULL;
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

int cli_comms_register_kept(const char *name, const char *const *listen, unsigned int listens, const char *const *speak,
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
        cli_cell_given_by(&g_cli_comms_cells, g_cli_comms.parts[at].listen);
        g_cli_comms.parts[at].listen = NULL;
        g_cli_comms.parts[at].speak = NULL;
        for (unsigned int one = 0u; one < g_cli_comms.listeners_room; one += 1u)
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
    unsigned int vacant = g_cli_comms.listeners_room;
    for (unsigned int one = 0u; (one < g_cli_comms.listeners_room) && (vacant == g_cli_comms.listeners_room); one += 1u)
    {
        vacant = g_cli_comms.listeners[one].taken ? vacant : one;
    }
    if (vacant == g_cli_comms.listeners_room)
    {
        CliCommsListener *const grown =
            comms_grown(g_cli_comms.listeners, &g_cli_comms.listeners_room, CLI_COMMS_LISTENERS,
                        sizeof(CliCommsListener), "the comms layer's listeners");
        g_cli_comms.listeners = (grown != NULL) ? grown : g_cli_comms.listeners;
    }
    if (vacant < g_cli_comms.listeners_room)
    {
        CliCommsListener *const place = &g_cli_comms.listeners[vacant];
        *place = (CliCommsListener){1, at, "", queued, heard, context};
        memor.cpy(&(MemoriaCfg){.dst = place->pattern,
                                .src = channel,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
        *listener = vacant;
        cli_lock_give(&g_cli_comms.lock);
        return 1;
    }
    cli_lock_give(&g_cli_comms.lock);
    cli_line_text(said, "the comms layer holds as many listeners as it can");
    return 0;
}

void cli_comms_unlisten(unsigned int listener)
{
    cli_lock_take(&g_cli_comms.lock);
    if (listener < g_cli_comms.listeners_room)
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
    if (g_cli_comms.channels == g_cli_comms.channels_room)
    {
        CliCommsTraffic *const grown = comms_grown(g_cli_comms.traffic, &g_cli_comms.channels_room, CLI_COMMS_CHANNELS,
                                                   sizeof(CliCommsTraffic), "the comms layer's channels");
        if (grown == NULL)
        {
            return NULL;
        }
        g_cli_comms.traffic = grown;
    }
    CliCommsTraffic *const made = &g_cli_comms.traffic[g_cli_comms.channels];
    g_cli_comms.channels += 1u;
    memor.set(&(MemoriaCfg){.dst = made, .val = 0, .bytes = sizeof(*made)});
    memor.cpy(&(MemoriaCfg){.dst = made->name,
                            .src = channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
    return made;
}

// a message kept among its channel's last in a cell of its own, the oldest let go past the most kept; kept not at all
// where the cell is refused
static void comms_kept(CliCommsTraffic *traffic, const char *from, const char *body, unsigned long long size)
{
    char sized[64];
    CliLine sized_line = {sized, sizeof(sized), 0ull};
    cli_line_text(&sized_line, "\"a body of ");
    cli_line_decimal(&sized_line, size, 1u);
    cli_line_text(&sized_line, " bytes\"");
    const int whole = size < CLI_COMMS_KEPT_BODY;
    const char *const said = whole ? body : sized;
    const unsigned long long said_size = whole ? size : sized_line.at;
    const unsigned long long from_size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u;
    char *const cell = cli_cell_taken_by(&g_cli_comms_cells, from_size + said_size, "a message kept");
    if (cell == NULL)
    {
        return;
    }
    const unsigned int place = (traffic->first + traffic->count) % CLI_COMMS_KEPT;
    CliCommsKept *const kept = &traffic->last[place];
    if (traffic->count == CLI_COMMS_KEPT)
    {
        traffic->first = (traffic->first + 1u) % CLI_COMMS_KEPT;
        cli_cell_given_by(&g_cli_comms_cells, kept->from);
    }
    else
    {
        traffic->count += 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = from, .bytes = (size_t)from_size});
    memor.cpy(&(MemoriaCfg){.dst = cell + from_size, .src = said, .bytes = (size_t)said_size});
    kept->from = cell;
    kept->body = cell + from_size;
    kept->size = said_size;
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
    for (unsigned int one = 0u; one < g_cli_comms.listeners_room; one += 1u)
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
