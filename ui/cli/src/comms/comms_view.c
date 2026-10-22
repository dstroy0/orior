// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_view.c: the comms layer as the view lists it: every part with its grants and counts, and every channel that has
// carried or refused a message with its counts, its listeners and its last messages, each list by name
#include "../text/json_tree.h"
#include "comms_internal.h"

#include "../text/text.h"

// `count` names written as a JSON array
static void view_names(const char (*names)[CLI_NAME_LONGEST], unsigned int count, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)names[at],
                              cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
}

// the places `order`, `count` of them, put in the order of the names `name_of` gives each, under the lock
static void view_sorted(unsigned int *order, unsigned int count, const char *(*name_of)(unsigned int place))
{
    for (unsigned int one = 1u; one < count; one += 1u)
    {
        const unsigned int moved = order[one];
        unsigned int at = one;
        while ((at > 0u) && (cli_text_order(name_of(order[at - 1u]), name_of(moved)) > 0))
        {
            order[at] = order[at - 1u];
            at -= 1u;
        }
        order[at] = moved;
    }
}

static const char *view_part_name(unsigned int place)
{
    return g_cli_comms.parts[place].name;
}

static const char *view_channel_name(unsigned int place)
{
    return g_cli_comms.traffic[place].name;
}

void cli_comms_parts(CliLine *out)
{
    unsigned int order[CLI_COMMS_PARTS];
    unsigned int count = 0u;
    cli_lock_take(&g_cli_comms.lock);
    for (unsigned int at = 0u; at < g_cli_comms.parts_room; at += 1u)
    {
        if (g_cli_comms.parts[at].taken)
        {
            order[count] = at;
            count += 1u;
        }
    }
    view_sorted(order, count, view_part_name);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCommsPart *const part = &g_cli_comms.parts[order[at]];
        cli_line_text(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)part->name,
                              cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"process\":");
        if (part->has_process)
        {
            cli_line_decimal(out, part->process, 1u);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ",\"listen\":");
        view_names((const char (*)[CLI_NAME_LONGEST])part->listen, part->listens, out);
        cli_line_text(out, ",\"speak\":");
        view_names((const char (*)[CLI_NAME_LONGEST])part->speak, part->speaks, out);
        cli_line_text(out, ",\"said\":");
        cli_line_decimal(out, part->said, 1u);
        cli_line_text(out, ",\"refused\":");
        cli_line_decimal(out, part->refused, 1u);
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_lock_give(&g_cli_comms.lock);
}

// the names of the parts listening on the channel `name`, in the order they began to listen
static void view_listeners(const char *name, CliLine *out)
{
    unsigned int written = 0u;
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < g_cli_comms.listeners_room; at += 1u)
    {
        const CliCommsListener *const listener = &g_cli_comms.listeners[at];
        const CliCommsPart *const part = listener->taken ? &g_cli_comms.parts[listener->part] : NULL;
        if ((part == NULL) || !part->taken || !cli_comms_grants(listener->pattern, name))
        {
            continue;
        }
        cli_line_text(out, (written > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)part->name,
                              cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}));
        written += 1u;
    }
    cli_line_character(out, ']');
}

void cli_comms_channels(CliLine *out)
{
    unsigned int order[CLI_COMMS_CHANNELS];
    cli_lock_take(&g_cli_comms.lock);
    const unsigned int count = g_cli_comms.channels;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        order[at] = at;
    }
    view_sorted(order, count, view_channel_name);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCommsTraffic *const traffic = &g_cli_comms.traffic[order[at]];
        cli_line_text(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)traffic->name,
                              cellul.len(&(CatenaFinitaCfg){.src = traffic->name, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"messages\":");
        cli_line_decimal(out, traffic->messages, 1u);
        cli_line_text(out, ",\"bytes\":");
        cli_line_decimal(out, traffic->bytes, 1u);
        cli_line_text(out, ",\"transferred\":");
        cli_line_decimal(out, traffic->transferred, 1u);
        cli_line_text(out, ",\"refused\":");
        cli_line_decimal(out, traffic->refused, 1u);
        cli_line_text(out, ",\"listeners\":");
        view_listeners(traffic->name, out);
        cli_line_text(out, ",\"last\":[");
        for (unsigned int one = 0u; one < traffic->count; one += 1u)
        {
            const CliCommsKept *const kept = &traffic->last[(traffic->first + one) % CLI_COMMS_KEPT];
            cli_line_text(out, (one > 0u) ? ",{\"channel\":" : "{\"channel\":");
            cli_json_quoted_lossy(out, (const unsigned char *)traffic->name,
                                  cellul.len(&(CatenaFinitaCfg){.src = traffic->name, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, ",\"from\":");
            cli_json_quoted_lossy(out, (const unsigned char *)kept->from,
                                  cellul.len(&(CatenaFinitaCfg){.src = kept->from, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, ",\"body\":");
            cli_line_counted(out, kept->body, kept->size);
            cli_line_character(out, '}');
        }
        cli_line_text(out, "]}");
    }
    cli_line_character(out, ']');
    cli_lock_give(&g_cli_comms.lock);
}
