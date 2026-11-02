// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_view.c: the comms layer as the view lists it: every part with its grants and counts, and every channel that has
// carried or refused a message with its counts, its listeners and its last messages, each list by name
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "comms_internal.h"

#include "../text/text.h"

typedef struct
{
    const char (*names)[CLI_NAME_MAX_LEN];
    unsigned int count;
    VerbaLinea *out;
    unsigned int *order;
    const char *(*name_of)(const CliEachPlaceCfg *args);
    const char *name;
} CommsViewCtx;

// `count` names written as a JSON array
EMBED_INLINE void view_names(const CommsViewCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->names[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->names[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(out, ']');
}

// the places `order`, `count` of them, put in the order of the names `name_of` gives each, under the lock
EMBED_INLINE void view_sort(const CommsViewCtx *args)
{
    for (unsigned int one = 1u; one < args->count; one += 1u)
    {
        const unsigned int moved = args->order[one];
        unsigned int at = one;
        while ((at > 0u) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                           .one = EMBED_CALL(args->name_of, CliEachPlaceCfg, .place = args->order[at - 1u]),
                           .other_pointer = EMBED_CALL(args->name_of, CliEachPlaceCfg, .place = moved)) > 0))
        {
            args->order[at] = args->order[at - 1u];
            at -= 1u;
        }
        args->order[at] = moved;
    }
}

static const char *view_part_name(const CliEachPlaceCfg *args)
{
    return g_cli_comms.parts[args->place].name;
}

static const char *view_channel_name(const CliEachPlaceCfg *args)
{
    return g_cli_comms.traffic[args->place].name;
}

void cli_comms_parts(const CliCommsCfg *args)
{
    unsigned int order[CLI_COMMS_PARTS];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < g_cli_comms.parts_room; at += 1u)
    {
        if (g_cli_comms.parts[at].taken)
        {
            order[count] = at;
            count += 1u;
        }
    }
    EMBED_CALL(view_sort, CommsViewCtx, .order = order, .count = count, .name_of = view_part_name);
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCommsPart *const part = &g_cli_comms.parts[order[at]];
        mmgr_line_put(args->out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)part->name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"process\":");
        if (part->has_process)
        {
            mmgr_line_decimal(args->out, part->process, 1u);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_put(args->out, ",\"listen\":");
        EMBED_CALL(view_names, CommsViewCtx, .names = (const char (*)[CLI_NAME_MAX_LEN])part->listen,
                   .count = part->listens, .out = args->out);
        mmgr_line_put(args->out, ",\"send\":");
        EMBED_CALL(view_names, CommsViewCtx, .names = (const char (*)[CLI_NAME_MAX_LEN])part->send_grant,
                   .count = part->send_grants, .out = args->out);
        mmgr_line_put(args->out, ",\"sent\":");
        mmgr_line_decimal(args->out, part->sent_count, 1u);
        mmgr_line_put(args->out, ",\"refused\":");
        mmgr_line_decimal(args->out, part->refused, 1u);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}

// the names of the parts listening on the channel `name`, in the order they began to listen
EMBED_INLINE void view_listeners(const CommsViewCtx *args)
{
    VerbaLinea *out = args->out;
    unsigned int written = 0u;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < g_cli_comms.listeners_room; at += 1u)
    {
        const CliCommsListener *const listener = &g_cli_comms.listeners[at];
        const CliCommsPart *const part = listener->taken ? &g_cli_comms.parts[listener->part] : NULL;
        if ((part == NULL) || !part->taken ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener->pattern, .channel = args->name))
        {
            continue;
        }
        mmgr_line_put(out, (written > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)part->name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = part->name, .cap = CLI_CELLS_POOL}));
        written += 1u;
    }
    mmgr_line_ch(out, ']');
}

void cli_comms_channels(const CliCommsCfg *args)
{
    unsigned int order[CLI_COMMS_CHANNELS];
    const unsigned int count = g_cli_comms.channels;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        order[at] = at;
    }
    EMBED_CALL(view_sort, CommsViewCtx, .order = order, .count = count, .name_of = view_channel_name);
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCommsTraffic *const traffic = &g_cli_comms.traffic[order[at]];
        mmgr_line_put(args->out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)traffic->name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = traffic->name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"messages\":");
        mmgr_line_decimal(args->out, traffic->messages, 1u);
        mmgr_line_put(args->out, ",\"bytes\":");
        mmgr_line_decimal(args->out, traffic->bytes, 1u);
        mmgr_line_put(args->out, ",\"transferred\":");
        mmgr_line_decimal(args->out, traffic->transferred, 1u);
        mmgr_line_put(args->out, ",\"refused\":");
        mmgr_line_decimal(args->out, traffic->refused, 1u);
        mmgr_line_put(args->out, ",\"listeners\":");
        EMBED_CALL(view_listeners, CommsViewCtx, .name = traffic->name, .out = args->out);
        mmgr_line_put(args->out, ",\"last\":[");
        for (unsigned int one = 0u; one < traffic->count; one += 1u)
        {
            const CliCommsStoredMsg *const stored = &traffic->last[(traffic->first + one) % CLI_COMMS_STORED_MAX];
            mmgr_line_put(args->out, (one > 0u) ? ",{\"channel\":" : "{\"channel\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)traffic->name,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = traffic->name, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->out, ",\"from\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)stored->from,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = stored->from, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->out, ",\"body\":");
            mmgr_line_put_n(args->out, stored->body, stored->size);
            mmgr_line_ch(args->out, '}');
        }
        mmgr_line_put(args->out, "]}");
    }
    mmgr_line_ch(args->out, ']');
}
