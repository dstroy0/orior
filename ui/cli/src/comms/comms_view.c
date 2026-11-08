// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_view.c: the Comms pane as the window reads it. The Channels view lists every part and every channel that has
// carried or refused a message, each list by name, as items in one cell: the items, the row each item's row stands at,
// the messages stored under each channel opened, and every text the rows read, written by the CLI. The cell is sized
// to what the layer contains as it is listed: the texts are written once into a line that only counts them, then once
// into the cell. The layer is listed again only where its change stamp moved since it was last listed, so a frame reads
// numbers already laid and a row is a binary search over the rows the items start at
#include "../command_line/callback_cfg.h"
#include "../requests/requests.h"
#include "comms_internal.h"

// no channel; the group rows, Parts and Channels; and the bytes a counting line takes its texts in at once
#define COMMS_NONE 0xffffffffu
#define COMMS_GROUPS 2u
#define COMMS_COUNTING 64u

// An item of the Channels view: its type, the part's or the channel's place in the layer's tables, its name, status
// and detail as offsets into the text and their bytes, the first of its messages and how many stand under it, and
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

// The Channels view: its one cell and, in it, the items, the row each item's row stands at with one more than the
// items, the messages and the texts; how many items and messages; the summary's offset and bytes; a byte a channel, 1
// where it is open; the channel last opened or closed and the row it stands at, -1 for none; the stamp the layer had
// as it was last listed and whether it was; the millisecond the layer was last looked at; and the view shown
typedef struct
{
    unsigned char *cell;
    CommsItem *item;
    unsigned int *first_row;
    CommsMessage *message;
    char *text;
    unsigned int items;
    unsigned int messages;
    unsigned long long summary;
    unsigned long long summary_length;
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
    unsigned int messages_most;
    unsigned int *messages_out;
    unsigned long long count;
    const char *word;
    const char (*grants)[CLI_NAME_MAX_LEN];
    unsigned int grants_count;
    const char *channel;
    unsigned int at;
    unsigned int key;
} CommsViewCtx;

// a counting line's bytes taken and let go, its flushed count the bytes of every text written to it
static int comms_counted(void *context, const char *bytes, size_t size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

// the place the next text of `line` starts at: the bytes it handed on and those it has
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

// the grants written joined by a comma, the word nothing where there are none
EMBED_INLINE void comms_grants_put(const CommsViewCtx *args)
{
    if (args->grants_count == 0u)
    {
        mmgr_line_put(args->line, "nothing");
        return;
    }
    for (unsigned int at = 0u; at < args->grants_count; at += 1u)
    {
        mmgr_line_put(args->line, (at != 0u) ? ", " : "");
        mmgr_line_put(args->line, args->grants[at]);
    }
}

// the parts listening on `channel` written joined by a comma, each once, the words no part where none does
EMBED_INLINE void comms_listeners_put(const CommsViewCtx *args)
{
    unsigned int written = 0u;
    for (unsigned int one = 0u; one < g_cli_comms.listeners_capacity; one += 1u)
    {
        const CliCommsListener *const listener = &g_cli_comms.listeners[one];
        if (!listener->reserved || (listener->part >= g_cli_comms.parts_capacity) ||
            !EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = listener->pattern, .channel = args->channel))
        {
            continue;
        }
        int before = 0;
        for (unsigned int earlier = 0u; (earlier < one) && !before; earlier += 1u)
        {
            const CliCommsListener *const other = &g_cli_comms.listeners[earlier];
            before =
                other->reserved && (other->part == listener->part) &&
                EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = other->pattern, .channel = args->channel);
        }
        if (before)
        {
            continue;
        }
        mmgr_line_put(args->line, (written != 0u) ? ", " : "");
        mmgr_line_put(args->line, g_cli_comms.parts[listener->part].name);
        written += 1u;
    }
    if (written == 0u)
    {
        mmgr_line_put(args->line, "no part");
    }
}

// the name of the table place `at`: a part's where `key` is 1, else a channel's. Each name stands in an array of
// CLI_NAME_MAX_LEN bytes cleared as it was registered, so two compare whole
EMBED_INLINE const char *comms_name_of(const CommsViewCtx *args)
{
    return (args->key != 0u) ? g_cli_comms.parts[args->at].name : g_cli_comms.traffic[args->at].name;
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
        const char *const name = EMBED_CALL(comms_name_of, CommsViewCtx, .at = moving, .key = args->key);
        unsigned int place = next;
        while (place > args->at)
        {
            const char *const before =
                EMBED_CALL(comms_name_of, CommsViewCtx, .at = args->order[place - 1u], .key = args->key);
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

// every text of the view written into `line`, and where `item` is not NULL each item laid with its texts' places, the
// messages under the channels opened laid into `message`, no more than `messages_most` of them: how many messages into
// `messages_out`, and the summary's place laid
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
        CommsItem laid = {0};
        laid.type = (parts_group || channels_group) ? CLI_COMMS_ROW_GROUP
                    : is_part                       ? CLI_COMMS_ROW_PART
                                                    : CLI_COMMS_ROW_CHANNEL;
        laid.index = (laid.type == CLI_COMMS_ROW_GROUP) ? COMMS_NONE : args->order[is_part ? at - 1u : at - 2u];
        laid.message_first = messages;
        laid.name = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
        if (laid.type == CLI_COMMS_ROW_GROUP)
        {
            mmgr_line_put(line, parts_group ? "Parts" : "Channels");
            laid.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.name;
            laid.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            mmgr_line_decimal(line, parts_group ? args->parts : args->channels, 1u);
            laid.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.status;
            laid.detail = laid.status + laid.status_length;
        }
        else if (laid.type == CLI_COMMS_ROW_PART)
        {
            const CliCommsPart *const part = &g_cli_comms.parts[laid.index];
            mmgr_line_put(line, part->name);
            laid.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.name;
            laid.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            mmgr_line_decimal(line, part->sent_count, 1u);
            mmgr_line_put(line, " sent");
            if (part->refused != 0ULL)
            {
                mmgr_line_put(line, ", ");
                mmgr_line_decimal(line, part->refused, 1u);
                mmgr_line_put(line, " refused");
            }
            laid.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.status;
            laid.detail = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            if (part->has_process)
            {
                mmgr_line_put(line, "process ");
                mmgr_line_decimal(line, part->process, 1u);
            }
            else
            {
                mmgr_line_put(line, "in orior");
            }
            mmgr_line_put(line, "; listens on ");
            EMBED_CALL(comms_grants_put, CommsViewCtx, .line = line,
                       .grants = (const char (*)[CLI_NAME_MAX_LEN])part->listen, .grants_count = part->listens);
            mmgr_line_put(line, "; speaks on ");
            EMBED_CALL(comms_grants_put, CommsViewCtx, .line = line,
                       .grants = (const char (*)[CLI_NAME_MAX_LEN])part->send_grant, .grants_count = part->send_grants);
            laid.detail_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.detail;
            laid.refused = part->refused != 0ULL;
        }
        else
        {
            const CliCommsTraffic *const traffic = &g_cli_comms.traffic[laid.index];
            mmgr_line_put(line, traffic->name);
            laid.name_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.name;
            laid.status = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic->messages, .word = "message");
            if (traffic->refused != 0ULL)
            {
                mmgr_line_put(line, ", ");
                mmgr_line_decimal(line, traffic->refused, 1u);
                mmgr_line_put(line, " refused");
            }
            laid.status_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.status;
            laid.detail = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
            EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic->bytes, .word = "byte");
            if (traffic->transferred != 0ULL)
            {
                mmgr_line_put(line, ", ");
                EMBED_CALL(comms_plural, CommsViewCtx, .line = line, .count = traffic->transferred, .word = "byte");
                mmgr_line_put(line, " of them through the transfer layer");
            }
            mmgr_line_put(line, "; heard by ");
            EMBED_CALL(comms_listeners_put, CommsViewCtx, .line = line, .channel = traffic->name);
            laid.detail_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - laid.detail;
            laid.refused = traffic->refused != 0ULL;
            // the messages stored under a channel opened, the oldest first, no more than the cell was sized for
            const unsigned int stored = s_pane.open[laid.index] ? traffic->count : 0u;
            for (unsigned int one = 0u; one < stored; one += 1u)
            {
                if ((args->item != NULL) && (messages >= args->messages_most))
                {
                    break;
                }
                const CliCommsStoredMsg *const stored_msg =
                    &traffic->last[(traffic->first + one) % CLI_COMMS_STORED_MAX];
                CommsMessage written = {0};
                written.from = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
                mmgr_line_put(line, (stored_msg->from != NULL) ? stored_msg->from : "");
                written.from_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - written.from;
                written.body = EMBED_CALL(comms_place, CommsViewCtx, .line = line);
                if (stored_msg->body != NULL)
                {
                    (void)mmgr_line_put_n(line, stored_msg->body, stored_msg->size);
                }
                written.body_length = EMBED_CALL(comms_place, CommsViewCtx, .line = line) - written.body;
                if (args->message != NULL)
                {
                    args->message[messages] = written;
                }
                messages += 1u;
                laid.messages += 1u;
            }
        }
        if (args->item != NULL)
        {
            args->item[at] = laid;
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
    for (unsigned int at = 0u; (g_cli_comms.parts != NULL) && (at < g_cli_comms.parts_capacity); at += 1u)
    {
        parts += g_cli_comms.parts[at].reserved ? 1u : 0u;
    }
    const unsigned int channels = (g_cli_comms.traffic != NULL) ? g_cli_comms.channels : 0u;
    // the parts' places then the channels', each put in order of their names, for this listing alone
    unsigned int *const order =
        g_cli_guard->temporary_buf_alloc((parts + channels + 1ULL) * sizeof(unsigned int), g_cli_comms_owner);
    if (order == NULL)
    {
        return;
    }
    unsigned int put = 0u;
    for (unsigned int at = 0u; (g_cli_comms.parts != NULL) && (at < g_cli_comms.parts_capacity) && (put < parts);
         at += 1u)
    {
        if (g_cli_comms.parts[at].reserved)
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
    // the texts counted, then the cell taken for the items, their rows, the messages and the texts, and laid
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
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(
        item_bytes + row_bytes + message_bytes + text_bytes + 1ULL, g_cli_comms_owner);
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(order, g_cli_comms_owner);
        return;
    }
    CommsItem *const item = (CommsItem *)(void *)cell;
    unsigned int *const first_row = (unsigned int *)(void *)(cell + item_bytes);
    CommsMessage *const message = (CommsMessage *)(void *)(cell + item_bytes + row_bytes);
    char *const text = (char *)(cell + item_bytes + row_bytes + message_bytes);
    VerbaLinea text_line = {text, text_bytes + 1ULL, 0ULL};
    unsigned int laid_messages = 0u;
    EMBED_CALL(comms_written, CommsViewCtx, .line = &text_line, .order = order, .parts = parts, .channels = channels,
               .item = item, .message = message, .messages_most = messages, .messages_out = &laid_messages);
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
    g_cli_guard->persistent_buf_release(s_pane.cell, g_cli_comms_owner);
    s_pane.cell = cell;
    s_pane.item = item;
    s_pane.first_row = first_row;
    s_pane.message = message;
    s_pane.text = text;
    s_pane.items = items;
    s_pane.messages = laid_messages;
    s_pane.stamp = stamp;
    s_pane.listed = 1;
}

// the item a row stands under: the last whose first row is at most `at`, among `key` items
EMBED_INLINE unsigned int comms_item_under(const CommsViewCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->key;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_pane.first_row[middle] <= args->at)
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
    if (args->shown && (s_pane.view == CLI_COMMS_VIEW_CHANNELS))
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
                   .shown = args->shown && (s_pane.view == CLI_COMMS_VIEW_REQUESTS));
    return listed || stepped;
}

unsigned long long cli_comms_pane_deadline_ms(const CliCommsPaneCfg *args)
{
    return (args->shown && (s_pane.view == CLI_COMMS_VIEW_CHANNELS)) ? s_pane.read_ms + CLI_COMMS_REFRESH_MS : 0ULL;
}

unsigned int cli_comms_pane_rows(const CliCommsPaneCfg *args)
{
    (void)args;
    return (s_pane.cell != NULL) ? s_pane.first_row[s_pane.items] : 0u;
}

int cli_comms_pane_row(const CliCommsPaneCfg *args)
{
    if ((s_pane.cell == NULL) || (args->at >= s_pane.first_row[s_pane.items]))
    {
        return 0;
    }
    const unsigned int place = EMBED_CALL(comms_item_under, CommsViewCtx, .at = args->at, .key = s_pane.items);
    const CommsItem *const item = &s_pane.item[place];
    const unsigned int inner = args->at - s_pane.first_row[place];
    CliCommsRow *const row = args->row;
    *row = (CliCommsRow){0};
    if (inner != 0u)
    {
        const CommsMessage *const message = &s_pane.message[item->message_first + inner - 1u];
        row->type = CLI_COMMS_ROW_MESSAGE;
        row->name = s_pane.text + message->from;
        row->name_length = message->from_length;
        row->status = s_pane.text + message->body;
        row->status_length = message->body_length;
        row->detail = row->status + row->status_length;
        return 1;
    }
    row->type = item->type;
    row->name = s_pane.text + item->name;
    row->name_length = item->name_length;
    row->status = s_pane.text + item->status;
    row->status_length = item->status_length;
    row->detail = s_pane.text + item->detail;
    row->detail_length = item->detail_length;
    row->refused = item->refused;
    row->open = (item->type == CLI_COMMS_ROW_CHANNEL) && s_pane.open[item->index];
    row->current = (item->type == CLI_COMMS_ROW_CHANNEL) && (item->index == s_pane.current);
    return 1;
}

void cli_comms_pane_toggle(const CliCommsPaneCfg *args)
{
    if ((s_pane.cell == NULL) || (args->at >= s_pane.first_row[s_pane.items]))
    {
        return;
    }
    const unsigned int place = EMBED_CALL(comms_item_under, CommsViewCtx, .at = args->at, .key = s_pane.items);
    const CommsItem *const item = &s_pane.item[place];
    if ((item->type != CLI_COMMS_ROW_CHANNEL) || (args->at != s_pane.first_row[place]))
    {
        return;
    }
    s_pane.open[item->index] ^= 1u;
    s_pane.current = item->index;
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

const char *cli_comms_pane_summary(const CliCommsPaneCfg *args)
{
    *args->length_out = (s_pane.cell != NULL) ? s_pane.summary_length : 0ULL;
    return (s_pane.cell != NULL) ? s_pane.text + s_pane.summary : "";
}

const char *cli_comms_pane_label(const CliCommsPaneCfg *args)
{
    static const char *const s_labels[CLI_COMMS_READ] = {"Channels", "Requests", "Network", "Report", "Read Again"};
    const unsigned int place = args->at - CLI_COMMS_CHOOSE;
    const char *const label = ((args->at >= CLI_COMMS_CHOOSE) && (place < CLI_COMMS_READ)) ? s_labels[place] : "";
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 16ULL});
    return label;
}
