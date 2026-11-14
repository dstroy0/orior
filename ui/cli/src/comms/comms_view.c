// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_view.c: the Comms pane as the window reads it. The Channels view lists every part and every channel that has
// carried or refused a message, each list by name, as items in one cell: the items, the row each item's row is at,
// the messages stored under each channel opened, and every text the rows read, written by the CLI. The cell is sized
// to what the layer contains as it is listed: the texts are written once into a line that only counts them, then once
// into the cell. The layer is listed again only where its change stamp moved since it was last listed, so a frame reads
// numbers already positioned and a row is a binary search over the rows the items start at
#include "../command_line/callback_cfg.h"
#include "../requests/requests.h"
#include "comms_internal.h"

// no channel; the group rows, Parts and Channels; and the bytes a counting line takes its texts in at once
#define COMMS_NONE 0xffffffffu
#define COMMS_GROUPS 2u
#define COMMS_COUNTING 64u

// An item of the Channels view: its type, the part's or the channel's place in the layer's tables, its name, status
// and detail as offsets into the text and their bytes, the first of its messages and how many are under it, and
// whether it refused a message
typedef struct
{
    unsigned int type;
    unsigned int index;
    unsigned long long name;
    unsigned long long name_length;
    unsigned long long status;
    unsigned long long status_length;
    unsigned long long detail;
    unsigned long long detail_length;
    unsigned int message_first;
    unsigned int messages;
    int refused;
} CommsItem;

// A message stored under a channel opened: its sender and its body as offsets into the text and their bytes
typedef struct
{
    unsigned long long from;
    unsigned long long from_length;
    unsigned long long body;
    unsigned long long body_length;
} CommsMessage;

// The Channels view: its one movable cell and, in it, the places of the items, of the row each item's row is at
// with one more than the items, of the messages and of the texts, each read out by copy; how many items and messages;
// the summary's offset and bytes, and the summary in the pane's own bytes; a byte a channel, 1 where it is open; the
// channel last opened or closed and the row it is at, -1 for none; the stamp the layer had as it was last listed
// and whether it was; the millisecond the layer was last looked at; and the view displayed
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long item_at;
    unsigned long long first_row_at;
    unsigned long long message_at;
    unsigned long long text_at;
    unsigned int items;
    unsigned int messages;
    unsigned long long summary;
    unsigned long long summary_length;
    char summary_text[CLI_ERR_MSG_MAX_LEN];
    unsigned char open[CLI_COMMS_CHANNELS];
    unsigned int current;
    int current_row;
    unsigned int stamp;
    int listed;
    unsigned long long read_ms;
    unsigned int view;
} CommsPane;

static CommsPane s_pane = {.current = COMMS_NONE, .current_row = -1};

typedef struct
{
    VerbaLinea *line;
    unsigned int *order;
    unsigned int parts;
    unsigned int channels;
    CommsItem *item;
    CommsMessage *message;
    unsigned int messages_max;
    unsigned int *messages_out;
    unsigned long long count;
    const char *word;
    const CliCommsPart *part;
    unsigned int grants_from;
    unsigned int grants_count;
    const char *channel;
    unsigned int at;
    unsigned int key;
    char *name_out;
} CommsViewCtx;

// a counting line's bytes taken and let go, its flushed count the bytes of every text written to it
static int comms_counted(void *context, const char *bytes, size_t size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

// the place the next text of `line` starts at: the bytes it passed on and those it has
EMBED_INLINE unsigned long long comms_place(const CommsViewCtx *args)
{
    return args->line->flushed + args->line->at;
}

// `count` written with `word` after it, the word taking an s where the count is not one
EMBED_INLINE void comms_plural(const CommsViewCtx *args)
{
    mmgr_line_decimal(args->line, args->count, 1u);
    mmgr_line_ch(args->line, ' ');
    mmgr_line_put(args->line, args->word);
    if (args->count != 1ULL)
    {
        mmgr_line_ch(args->line, 's');
    }
}

// `part`'s grants from `grants_from`, `grants_count` of them, written joined by a comma, the word nothing where there
// are none
EMBED_INLINE void comms_grants_put(const CommsViewCtx *args)
{
    if (args->grants_count == 0u)
    {
        mmgr_line_put(args->line, "nothing");
        return;
    }
    for (unsigned int at = 0u; at < args->grants_count; at += 1u)
    {
        char grant[CLI_NAME_MAX_LEN];
        grant[0] = '\0';
        if ((args->part->grants != NULL) && ((args->grants_from + at) < args->part->listens + args->part->send_grants))
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = args->part->grants,
                                    .offset = (unsigned long long)(args->grants_from + at) * CLI_NAME_MAX_LEN,
                                    .dst = grant,
                                    .bytes = CLI_NAME_MAX_LEN});
        }
        mmgr_line_put(args->line, (at != 0u) ? ", " : "");
        mmgr_line_put(args->line, grant);
    }
}

// the parts listening on `channel` written joined by a comma, each once, the words no part where none does
EMBED_INLINE void comms_listeners_put(const CommsViewCtx *args)
{
    unsigned int written = 0u;
    for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
    {
        CliCommsListener listener = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_LISTENERS_AT + ((unsigned long long)one * sizeof(listener)),
                                .dst = &listener,
                                .bytes = sizeof(listener)});
        if (!listener.reserved || (listener.part >= g_cli_comms.parts_capacity) ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener.pattern, .channel = args->channel))
        {
            continue;
        }
        int before = 0;
        for (unsigned int earlier = 0u; (earlier < one) && !before; earlier += 1u)
        {
            CliCommsListener other = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                    .offset = COMMS_LISTENERS_AT + ((unsigned long long)earlier * sizeof(other)),
                                    .dst = &other,
                                    .bytes = sizeof(other)});
            before =
                other.reserved && (other.part == listener.part) &&
                EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = other.pattern, .channel = args->channel);
        }
        if (before)
        {
            continue;
        }
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)listener.part * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        mmgr_line_put(args->line, (written != 0u) ? ", " : "");
        mmgr_line_put(args->line, part.name);
        written += 1u;
    }
    if (written == 0u)
    {
        mmgr_line_put(args->line, "no part");
    }
}

// the name of the table place `at` read into `name_out`: a part's where `key` is 1, else a channel's. Each name is
// in an array of CLI_NAME_MAX_LEN bytes cleared as it was registered, so two compare whole
EMBED_INLINE void comms_name_of(const CommsViewCtx *args)
{
    if (args->key != 0u)
    {
        CliCommsPart part = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)args->at * sizeof(part)),
                                .dst = &part,
                                .bytes = sizeof(part)});
        memor.cpy(&(MemoriaCfg){.dst = args->name_out, .src = part.name, .bytes = CLI_NAME_MAX_LEN});
    }
    else
    {
        CliCommsTraffic traffic = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_TRAFFIC_AT + ((unsigned long long)args->at * sizeof(traffic)),
                                .dst = &traffic,
                                .bytes = sizeof(traffic)});
        memor.cpy(&(MemoriaCfg){.dst = args->name_out, .src = traffic.name, .bytes = CLI_NAME_MAX_LEN});
    }
}

// the places from `at` to before `count` of the order put in order of their names, parts where `key` is 1, as each
// list is few
EMBED_INLINE void comms_order_sorted(const CommsViewCtx *args)
{
    // the order's places are fewer than the parts and the channels together
    const unsigned int end = (unsigned int)args->count;
    for (unsigned int next = args->at + 1u; next < end; next += 1u)
    {
        const unsigned int moving = args->order[next];
        char name[CLI_NAME_MAX_LEN];
        EMBED_CALL(comms_name_of, CommsViewCtx, .at = moving, .key = args->key, .name_out = name);
        unsigned int place = next;
        while (place > args->at)
        {
            char before[CLI_NAME_MAX_LEN];
            EMBED_CALL(comms_name_of, CommsViewCtx, .at = args->order[place - 1u], .key = args->key,
                       .name_out = before);
            if (memor.cmp(&(MemoriaCfg){.src = before, .other = name, .bytes = CLI_NAME_MAX_LEN}) <= 0)
            {
                break;
            }
            args->order[place] = args->order[place - 1u];
            place -= 1u;
        }
        args->order[place] = moving;
    }
}

// every text of the view written into `line`, and where `item` is not NULL each item positioned with its texts' places, the
// messages under the channels opened positioned into `message`, no more than `messages_max` of them: how many messages into
// `messages_out`, and the summary's place positioned
EMBED_INLINE void comms_written(const CommsViewCtx *args)
{
    VerbaLinea *const line = args->line;
    const unsigned int items = COMMS_GROUPS + args->parts + args->channels;
    unsigned int messages = 0u;
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        const int parts_group = at == 0u;
        const int channels_group = at == (1u + args->parts);
        const int is_part = !parts_group && (at <= args->parts);
        CommsItem positioned = {0};
        positioned.type = (parts_group || channels_group) ? CLI_COMMS_ROW_GROUP
                    : is_part                       ? CLI_COMMS_ROW_PART
                                                    : CLI_COMMS_ROW_CHANNEL;
        positioned.index = (positioned.type == CLI_COMMS_ROW_GROUP) ? COMMS_NONE : args->order[is_part ? at - 1u : at - 2u];
        positioned.message_first = messages;
        positioned.name = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
        if (positioned.type == CLI_COMMS_ROW_GROUP)
        {
            mmgr_line_put(line, parts_group ? "Parts" : "Channels");
            positioned.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.name;
            positioned.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            mmgr_line_decimal(line, parts_group ? args->parts : args->channels, 1u);
            positioned.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.status;
            positioned.detail = positioned.status + positioned.status_length;
        }
        else if (positioned.type == CLI_COMMS_ROW_PART)
        {
            CliCommsPart part = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                    .offset = COMMS_PARTS_AT + ((unsigned long long)positioned.index * sizeof(part)),
                                    .dst = &part,
                                    .bytes = sizeof(part)});
            mmgr_line_put(line, part.name);
            positioned.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.name;
            positioned.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            mmgr_line_decimal(line, part.sent_count, 1u);
            mmgr_line_put(line, " sent");
            if (part.refused != 0ULL)
            {
                mmgr_line_put(line, ", ");
                mmgr_line_decimal(line, part.refused, 1u);
                mmgr_line_put(line, " refused");
            }
            positioned.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.status;
            positioned.detail = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            if (part.has_process)
            {
                mmgr_line_put(line, "process ");
                mmgr_line_decimal(line, part.process, 1u);
            }
            else
            {
                mmgr_line_put(line, "in orior");
            }
            mmgr_line_put(line, "; listens on ");
            EMBED_CALL(comms_grants_put, CommsViewCtx, .line = line, .part = &part, .grants_from = 0u,
                       .grants_count = part.listens);
            mmgr_line_put(line, "; speaks on ");
            EMBED_CALL(comms_grants_put, CommsViewCtx, .line = line, .part = &part, .grants_from = part.listens,
                       .grants_count = part.send_grants);
            positioned.detail_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.detail;
            positioned.refused = part.refused != 0ULL;
        }
        else
        {
            CliCommsTraffic traffic = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                    .offset = COMMS_TRAFFIC_AT + ((unsigned long long)positioned.index * sizeof(traffic)),
                                    .dst = &traffic,
                                    .bytes = sizeof(traffic)});
            mmgr_line_put(line, traffic.name);
            positioned.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.name;
            positioned.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic.messages, .word = "message");
            if (traffic.refused != 0ULL)
            {
                mmgr_line_put(line, ", ");
                mmgr_line_decimal(line, traffic.refused, 1u);
                mmgr_line_put(line, " refused");
            }
            positioned.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.status;
            positioned.detail = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic.bytes, .word = "byte");
            if (traffic.transferred != 0ULL)
            {
                mmgr_line_put(line, ", ");
                EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic.transferred, .word = "byte");
                mmgr_line_put(line, " of them through the transfer layer");
            }
            mmgr_line_put(line, "; heard by ");
            EMBED_CALL(comms_listeners_put, CommsViewCtx, .line = line, .channel = traffic.name);
            positioned.detail_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - positioned.detail;
            positioned.refused = traffic.refused != 0ULL;
            // the messages stored under a channel opened, the oldest first, no more than the cell was sized for, each
            // read out of its cell into a temporary cell
            CliCommsStored *const ring = &g_cli_comms.stored[positioned.index];
            size_t front = 0u;
            (void)anularis.seg_front(&(AnularisCfg){.ring = &ring->ring, .out_index = &front});
            // the messages stored are at max CLI_COMMS_STORED_MAX, inside an unsigned int
            const unsigned int stored =
                s_pane.open[positioned.index] ? (unsigned int)anularis.seg_inflight(&(AnularisCfg){.ring = &ring->ring}) : 0u;
            for (unsigned int one = 0u; one < stored; one += 1u)
            {
                if ((args->item != NULL) && (messages >= args->messages_max))
                {
                    break;
                }
                const CliCommsStoredMsg *const stored_msg =
                    &ring->last[MMGR_RING_WRAP(front + one, CLI_COMMS_STORED_RING)];
                const unsigned long long bytes = stored_msg->from_size + stored_msg->size;
                char *const text = (bytes != 0ULL) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_comms_owner) : NULL;
                unsigned long long read = 0ULL;
                if ((text != NULL) && (stored_msg->cell != NULL))
                {
                    (void)g_cli_guard->movable_buf_read(
                        &(CarcerMovableCfg){.cell = stored_msg->cell, .offset = 0ULL, .dst = text, .bytes = bytes});
                    read = bytes;
                }
                CommsMessage written = {0};
                written.from = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
                if (read >= stored_msg->from_size)
                {
                    mmgr_line_put_n(line, text, stored_msg->from_size - 1ULL);
                }
                written.from_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - written.from;
                written.body = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
                if (read == bytes)
                {
                    (void)mmgr_line_put_n(line, text + stored_msg->from_size, stored_msg->size);
                }
                g_cli_guard->persistent_buf_release(text, g_cli_comms_owner);
                written.body_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - written.body;
                if (args->message != NULL)
                {
                    args->message[messages] = written;
                }
                messages += 1u;
                positioned.messages += 1u;
            }
        }
        if (args->item != NULL)
        {
            args->item[at] = positioned;
        }
    }
    *args->messages_out = messages;
    s_pane.summary = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
    EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = args->parts, .word = "part");
    mmgr_line_put(line, ", ");
    EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = args->channels, .word = "channel");
    s_pane.summary_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - s_pane.summary;
}

// the layer listed again into a cell sized to it, the cell it had released; the view as it was where the cells cannot
// meet the listing
EMBED_INLINE void comms_listed(const CommsViewCtx *args)
{
    (void)args;
    const unsigned int stamp = EMBED_ATOMIC_LOAD(&g_cli_comms.stamp);
    unsigned int parts = 0u;
    for (unsigned int at = 0u; (g_cli_comms.tables != NULL) && (at < g_cli_comms.parts_capacity); at += 1u)
    {
        CliCommsPart part_1 = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part_1)),
                                .dst = &part_1,
                                .bytes = sizeof(part_1)});

        parts += part_1.reserved ? 1u : 0u;
    }
    const unsigned int channels = (g_cli_comms.tables != NULL) ? g_cli_comms.channels : 0u;
    // the parts' places then the channels', each put in order of their names, for this listing alone
    unsigned int *const order =
        g_cli_guard->temporary_buf_alloc((parts + channels + 1ULL) * sizeof(unsigned int), g_cli_comms_owner);
    if (order == NULL)
    {
        return;
    }
    unsigned int put = 0u;
    for (unsigned int at = 0u; (g_cli_comms.tables != NULL) && (at < g_cli_comms.parts_capacity) && (put < parts);
         at += 1u)
    {
        CliCommsPart part_2 = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_comms.tables,
                                .offset = COMMS_PARTS_AT + ((unsigned long long)at * sizeof(part_2)),
                                .dst = &part_2,
                                .bytes = sizeof(part_2)});

        if (part_2.reserved)
        {
            order[put] = at;
            put += 1u;
        }
    }
    parts = put;
    for (unsigned int at = 0u; at < channels; at += 1u)
    {
        order[parts + at] = at;
    }
    EMBED_CALL(comms_order_sorted, CommsViewCtx, .order = order, .at = 0u, .count = parts, .key = 1u);
    EMBED_CALL(comms_order_sorted, CommsViewCtx, .order = order, .at = parts, .count = parts + channels, .key = 0u);
    // the texts counted, then the cell taken for the items, their rows, the messages and the texts, and positioned
    char counting_bytes[COMMS_COUNTING];
    VerbaLinea counting = {counting_bytes, sizeof(counting_bytes), 0ULL, comms_counted, NULL, 0ULL};
    unsigned int messages = 0u;
    EMBED_CALL(comms_written, CommsViewCtx, .line = &counting, .order = order, .parts = parts, .channels = channels,
               .messages_out = &messages);
    const unsigned long long text_bytes = counting.flushed + counting.at;
    const unsigned int items = COMMS_GROUPS + parts + channels;
    const unsigned long long item_bytes = (unsigned long long)items * sizeof(CommsItem);
    const unsigned long long row_bytes = (((items + 1ULL) * sizeof(unsigned int)) + 7ULL) & ~7ULL;
    const unsigned long long message_bytes = (unsigned long long)messages * sizeof(CommsMessage);
    // the whole positioned in a cell of this call, then written once into a movable cell of its size
    const unsigned long long bytes = item_bytes + row_bytes + message_bytes + text_bytes + 1ULL;
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_comms_owner);
    const CarcerMovableEntry *const stored =
        (cell != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_comms_owner) : NULL;
    if (stored == NULL)
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_comms_owner);
        g_cli_guard->persistent_buf_release(order, g_cli_comms_owner);
        return;
    }
    CommsItem *const item = (CommsItem *)(void *)cell;
    unsigned int *const first_row = (unsigned int *)(void *)(cell + item_bytes);
    CommsMessage *const message = (CommsMessage *)(void *)(cell + item_bytes + row_bytes);
    char *const text = (char *)(cell + item_bytes + row_bytes + message_bytes);
    VerbaLinea text_line = {text, text_bytes + 1ULL, 0ULL};
    unsigned int positioned_messages = 0u;
    EMBED_CALL(comms_written, CommsViewCtx, .line = &text_line, .order = order, .parts = parts, .channels = channels,
               .item = item, .message = message, .messages_max = messages, .messages_out = &positioned_messages);
    (void)mmgr_line_finish(&text_line);
    g_cli_guard->persistent_buf_release(order, g_cli_comms_owner);
    first_row[0] = 0u;
    s_pane.current_row = -1;
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        first_row[at + 1u] = first_row[at] + 1u + item[at].messages;
        // the rows are fewer than an int counts
        s_pane.current_row = ((item[at].type == CLI_COMMS_ROW_CHANNEL) && (item[at].index == s_pane.current))
                                 ? (int)first_row[at]
                                 : s_pane.current_row;
    }
    // the summary, cut to the pane's bytes, read off the positioned text
    const unsigned long long summary_length = (s_pane.summary_length < sizeof(s_pane.summary_text))
                                                  ? s_pane.summary_length
                                                  : (sizeof(s_pane.summary_text) - 1ULL);
    if (summary_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_pane.summary_text, .src = text + s_pane.summary, .bytes = (embed_word)summary_length});
    }
    s_pane.summary_text[summary_length] = '\0';
    s_pane.summary_length = summary_length;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = stored, .offset = 0ULL, .src = cell, .bytes = bytes});
    g_cli_guard->persistent_buf_release(cell, g_cli_comms_owner);
    if (s_pane.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_pane.cell, g_cli_comms_owner);
    }
    s_pane.cell = stored;
    s_pane.item_at = 0ULL;
    s_pane.first_row_at = item_bytes;
    s_pane.message_at = item_bytes + row_bytes;
    s_pane.text_at = item_bytes + row_bytes + message_bytes;
    s_pane.items = items;
    s_pane.messages = positioned_messages;
    s_pane.stamp = stamp;
    s_pane.listed = 1;
}

// the first row of the item `at`, read out of the cell
EMBED_INLINE unsigned int comms_first_row(const CommsViewCtx *args)
{
    unsigned int row = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_pane.cell,
                            .offset = s_pane.first_row_at + ((unsigned long long)args->at * sizeof(row)),
                            .dst = &row,
                            .bytes = sizeof(row)});
    return row;
}

// the item `at` read out of the cell into `item`
EMBED_INLINE void comms_item_read(const CommsViewCtx *args)
{
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_pane.cell,
                            .offset = s_pane.item_at + ((unsigned long long)args->at * sizeof(CommsItem)),
                            .dst = args->item,
                            .bytes = sizeof(CommsItem)});
}

// the message `at` read out of the cell into `message`
EMBED_INLINE void comms_message_read(const CommsViewCtx *args)
{
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_pane.cell,
                            .offset = s_pane.message_at + ((unsigned long long)args->at * sizeof(CommsMessage)),
                            .dst = args->message,
                            .bytes = sizeof(CommsMessage)});
}

// `count` bytes of the text at `key` copied into `name_out`, a buffer of CLI_COMMS_ROW_TEXT bytes, with an end, cut to
// the buffer: the bytes copied
EMBED_INLINE unsigned long long comms_text_read(const CommsViewCtx *args)
{
    const unsigned long long copied =
        (args->count < CLI_COMMS_ROW_TEXT) ? args->count : (unsigned long long)(CLI_COMMS_ROW_TEXT - 1u);
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_pane.cell, .offset = s_pane.text_at + args->key, .dst = args->name_out, .bytes = copied});
    }
    args->name_out[copied] = '\0';
    return copied;
}

// the item a row is under: the last whose first row is at max `at`, among `key` items
EMBED_INLINE unsigned int comms_item_under(const CommsViewCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->key;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(comms_first_row, CommsViewCtx, .at = middle) <= args->at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

void cli_comms_pane_begin(const CliCommsPaneCfg *args)
{
    EMBED_CALL(cli_requests_pane.begin, CliRequestsPaneCfg, .woken = args->woken);
}

int cli_comms_pane_step(const CliCommsPaneCfg *args)
{
    int listed = 0;
    if (args->displayed && (s_pane.view == CLI_COMMS_VIEW_CHANNELS))
    {
        const unsigned int stamp = EMBED_ATOMIC_LOAD(&g_cli_comms.stamp);
        if (args->force || !s_pane.listed || (stamp != s_pane.stamp))
        {
            EMBED_CALL(comms_listed, CommsViewCtx, .at = 0u);
            listed = 1;
        }
        s_pane.read_ms = (args->now_ms != 0ULL) ? args->now_ms : cli_clock_milliseconds();
    }
    const int stepped =
        EMBED_CALL(cli_requests_pane.step, CliRequestsPaneCfg, .root = args->root, .now_ms = args->now_ms,
                   .displayed = args->displayed && (s_pane.view == CLI_COMMS_VIEW_REQUESTS));
    return listed || stepped;
}

unsigned long long cli_comms_pane_deadline_ms(const CliCommsPaneCfg *args)
{
    return (args->displayed && (s_pane.view == CLI_COMMS_VIEW_CHANNELS)) ? s_pane.read_ms + CLI_COMMS_REFRESH_MS : 0ULL;
}

unsigned int cli_comms_pane_rows(const CliCommsPaneCfg *args)
{
    (void)args;
    return (s_pane.cell != NULL) ? EMBED_CALL(comms_first_row, CommsViewCtx, .at = s_pane.items) : 0u;
}

int cli_comms_pane_row(const CliCommsPaneCfg *args)
{
    if ((s_pane.cell == NULL) || (args->at >= EMBED_CALL(comms_first_row, CommsViewCtx, .at = s_pane.items)))
    {
        return 0;
    }
    const unsigned int place = EMBED_CALL(comms_item_under, CommsViewCtx, .at = args->at, .key = s_pane.items);
    CommsItem item;
    EMBED_CALL(comms_item_read, CommsViewCtx, .at = place, .item = &item);
    const unsigned int inner = args->at - EMBED_CALL(comms_first_row, CommsViewCtx, .at = place);
    CliCommsRow *const row = args->row;
    *row = (CliCommsRow){0};
    row->name = row->name_copied;
    row->status = row->status_copied;
    row->detail = row->detail_copied;
    if (inner != 0u)
    {
        CommsMessage message;
        EMBED_CALL(comms_message_read, CommsViewCtx, .at = item.message_first + inner - 1u, .message = &message);
        row->type = CLI_COMMS_ROW_MESSAGE;
        row->name_length = EMBED_CALL(comms_text_read, CommsViewCtx, .key = message.from, .count = message.from_length,
                                      .name_out = row->name_copied);
        row->status_length = EMBED_CALL(comms_text_read, CommsViewCtx, .key = message.body,
                                        .count = message.body_length, .name_out = row->status_copied);
        return 1;
    }
    row->type = item.type;
    row->name_length = EMBED_CALL(comms_text_read, CommsViewCtx, .key = item.name, .count = item.name_length,
                                  .name_out = row->name_copied);
    row->status_length = EMBED_CALL(comms_text_read, CommsViewCtx, .key = item.status, .count = item.status_length,
                                    .name_out = row->status_copied);
    row->detail_length = EMBED_CALL(comms_text_read, CommsViewCtx, .key = item.detail, .count = item.detail_length,
                                    .name_out = row->detail_copied);
    row->refused = item.refused;
    row->open = (item.type == CLI_COMMS_ROW_CHANNEL) && s_pane.open[item.index];
    row->current = (item.type == CLI_COMMS_ROW_CHANNEL) && (item.index == s_pane.current);
    return 1;
}

void cli_comms_pane_toggle(const CliCommsPaneCfg *args)
{
    if ((s_pane.cell == NULL) || (args->at >= EMBED_CALL(comms_first_row, CommsViewCtx, .at = s_pane.items)))
    {
        return;
    }
    const unsigned int place = EMBED_CALL(comms_item_under, CommsViewCtx, .at = args->at, .key = s_pane.items);
    CommsItem item;
    EMBED_CALL(comms_item_read, CommsViewCtx, .at = place, .item = &item);
    if ((item.type != CLI_COMMS_ROW_CHANNEL) || (args->at != EMBED_CALL(comms_first_row, CommsViewCtx, .at = place)))
    {
        return;
    }
    s_pane.open[item.index] ^= 1u;
    s_pane.current = item.index;
    EMBED_CALL(comms_listed, CommsViewCtx, .at = 0u);
}

int cli_comms_pane_current(const CliCommsPaneCfg *args)
{
    (void)args;
    return (s_pane.cell != NULL) ? s_pane.current_row : -1;
}

unsigned int cli_comms_pane_view(const CliCommsPaneCfg *args)
{
    (void)args;
    return s_pane.view;
}

void cli_comms_pane_choose(const CliCommsPaneCfg *args)
{
    if (args->view < CLI_COMMS_VIEWS)
    {
        s_pane.view = args->view;
    }
}

void cli_comms_pane_end(void)
{
    if (s_pane.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_pane.cell, g_cli_comms_owner);
    }
    s_pane = (CommsPane){.current = COMMS_NONE, .current_row = -1};
}

const char *cli_comms_pane_summary(const CliCommsPaneCfg *args)
{
    *args->length_out = (s_pane.cell != NULL) ? s_pane.summary_length : 0ULL;
    return (s_pane.cell != NULL) ? s_pane.summary_text : "";
}

const char *cli_comms_pane_label(const CliCommsPaneCfg *args)
{
    static const char *const s_labels[CLI_COMMS_READ] = {"Channels", "Requests", "Network", "Report", "Read Again"};
    const unsigned int place = args->at - CLI_COMMS_CHOOSE;
    const char *const label = ((args->at >= CLI_COMMS_CHOOSE) && (place < CLI_COMMS_READ)) ? s_labels[place] : "";
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 16ULL});
    return label;
}
