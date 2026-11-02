// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest.c: the completion panel. The items' texts are copied out of the answer's tree into one cell of the part's
// own, each item an offset and a length into it, which lets the tree go as the answer is read and the list stand on
// its own while the prefix changes
#include "suggest.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "../frame.h"

#include <stdlib.h>

int gui_suggest_init(const GuiSuggestRowsCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->suggest, .val = 0, .bytes = sizeof(*args->suggest)});
    args->suggest->item =
        g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_ITEMS_MOST * sizeof(GuiItem), &g_gui_suggest_cells);
    args->suggest->shown =
        g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_ITEMS_MOST * sizeof(unsigned int), &g_gui_suggest_cells);
    if ((args->suggest->item == NULL) || (args->suggest->shown == NULL))
    {
        gui_suggest_rows.free(args);
        return 0;
    }
    return 1;
}

typedef struct
{
    GuiSuggest *suggest;
    char byte;
    const char *text;
    unsigned long long length;
    unsigned long long *at;
    const CliJson *json;
    unsigned int one;
    unsigned int against;
    const GuiPalette *palette;
    const char *kind;
} SuggestCtx;
// the items' texts freed, the line left empty
EMBED_INLINE void suggest_texts_free(const SuggestCtx *args)
{
    g_cli_cells->persistent_buf_release(args->suggest->texts.out, &g_gui_suggest_cells);
    memor.set(&(MemoriaCfg){.dst = &args->suggest->texts, .val = 0, .bytes = sizeof(args->suggest->texts)});
}

void gui_suggest_free(const GuiSuggestRowsCfg *args)
{
    g_cli_cells->persistent_buf_release(args->suggest->item, &g_gui_suggest_cells);
    g_cli_cells->persistent_buf_release(args->suggest->shown, &g_gui_suggest_cells);
    EMBED_CALL(suggest_texts_free, SuggestCtx, .suggest = args->suggest);
    args->suggest->item = NULL;
    args->suggest->shown = NULL;
    args->suggest->items = 0u;
    args->suggest->count = 0u;
    args->suggest->open = 0;
}

void gui_suggest_close(const GuiSuggestJsonCfg *args)
{
    EMBED_CALL(suggest_texts_free, SuggestCtx, .suggest = args->suggest);
    args->suggest->items = 0u;
    args->suggest->count = 0u;
    args->suggest->index = 0u;
    args->suggest->first = 0u;
    args->suggest->open = 0;
    args->suggest->forced = 0;
    args->suggest->rect = (GuiRect){0, 0, 0, 0};
    args->suggest->list = (GuiRect){0, 0, 0, 0};
    args->suggest->detail = (GuiRect){0, 0, 0, 0};
}

// a lower case letter for an ASCII capital, any other byte as it is
EMBED_INLINE char suggest_lower(const SuggestCtx *args)
{
    return ((args->byte >= 'A') && (args->byte <= 'Z')) ? (char)(args->byte + ('a' - 'A')) : args->byte;
}

unsigned int gui_suggest_score(const GuiSuggestLengthCfg *args)
{
    const unsigned long long want = cellul.len(&(CatenaFinitaCfg){.src = args->typed, .cap = GUI_SUGGEST_PREFIX});
    if (want == 0ull)
    {
        return 1u;
    }
    if (want > args->length)
    {
        // a prefix longer than the label has letters the label cannot hold in order: nothing answers
        return 0u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = args->label, .other = args->typed, .bytes = want}) == 0)
    {
        return 5u;
    }
    int same = 1;
    for (unsigned long long at = 0ull; same && (at < want); at += 1ull)
    {
        same = EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->label[at]) ==
               EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->typed[at]);
    }
    if (same)
    {
        return 4u;
    }
    for (unsigned long long start = 0ull; start + want <= args->length; start += 1ull)
    {
        int part = 1;
        for (unsigned long long at = 0ull; part && (at < want); at += 1ull)
        {
            part = EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->label[start + at]) ==
                   EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->typed[at]);
        }
        if (part)
        {
            return 3u;
        }
    }
    unsigned long long at = 0ull;
    for (unsigned long long each = 0ull; (each < args->length) && (at < want); each += 1ull)
    {
        at += (EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->label[each]) ==
               EMBED_CALL(suggest_lower, SuggestCtx, .byte = args->typed[at]))
                  ? 1ull
                  : 0ull;
    }
    return (at == want) ? 2u : 0u;
}

// the items' texts' one cell, taken as the first is stored
EMBED_INLINE void suggest_texts_take(const SuggestCtx *args)
{
    if (args->suggest->texts.out == NULL)
    {
        char *const cell = g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_TEXT_ROOM, &g_gui_suggest_cells);
        args->suggest->texts = mmgr_line_over(cell, GUI_SUGGEST_TEXT_ROOM);
    }
}

// `length` bytes of `text` stored among the items' texts: where they start: 1, or 0 where they do not fit
EMBED_INLINE int suggest_store(const SuggestCtx *args)
{
    unsigned long long *at = args->at;
    suggest_texts_take(args);
    *at = args->suggest->texts.at;
    mmgr_line_put_n(&args->suggest->texts, args->text, args->length);
    return mmgr_line_ok(&args->suggest->texts);
}

// `length` bytes of a snippet `text` stored among the items' texts as the text it inserts: a tab stop with a
// placeholder, `${n:text}`, as its placeholder, one with choices, `${n|a,b|}`, as its first choice, a bare stop,
// `$n` or `${n}`, as nothing, and an escaped byte as itself: where it starts, or 0 with the line full
EMBED_INLINE int suggest_snippet_store(const SuggestCtx *args)
{
    const char *text = args->text;
    unsigned long long *at = args->at;
    suggest_texts_take(args);
    *at = args->suggest->texts.at;
    unsigned long long each = 0ull;
    while (each < args->length)
    {
        const char byte = text[each];
        if ((byte == '\\') && (each + 1ull < args->length))
        {
            mmgr_line_ch(&args->suggest->texts, text[each + 1ull]);
            each += 2ull;
            continue;
        }
        if (byte != '$')
        {
            mmgr_line_ch(&args->suggest->texts, byte);
            each += 1ull;
            continue;
        }
        each += 1ull;
        if ((each < args->length) && (text[each] != '{'))
        {
            // a bare stop: its digits
            each = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                              .size = args->length, .classes = CLI_TEXT_DIGITS, .set = "");
            continue;
        }
        each += (each < args->length) ? 1ull : 0ull;
        each = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = text, .at = each,
                          .size = args->length, .classes = CLI_TEXT_DIGITS, .set = "");
        if ((each < args->length) && (text[each] == ':'))
        {
            // the placeholder's text up to the closing brace, nested stops inside it flattened the same way
            each += 1ull;
            unsigned long long depth = 1ull;
            while ((each < args->length) && (depth > 0ull))
            {
                if ((text[each] == '\\') && (each + 1ull < args->length))
                {
                    mmgr_line_ch(&args->suggest->texts, text[each + 1ull]);
                    each += 2ull;
                    continue;
                }
                if ((text[each] == '$') && (each + 1ull < args->length) && (text[each + 1ull] == '{'))
                {
                    depth += 1ull;
                    each += 2ull;
                    each = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = text,
                                      .at = each, .size = args->length, .classes = CLI_TEXT_DIGITS, .set = "");
                    each += ((each < args->length) && (text[each] == ':')) ? 1ull : 0ull;
                    continue;
                }
                if (text[each] == '}')
                {
                    depth -= 1ull;
                    each += 1ull;
                    continue;
                }
                mmgr_line_ch(&args->suggest->texts, text[each]);
                each += 1ull;
            }
        }
        else if ((each < args->length) && (text[each] == '|'))
        {
            // the first choice
            each += 1ull;
            const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                       .text = text, .at = each, .size = args->length, .set = ",|");
            mmgr_line_put_n(&args->suggest->texts, text + each, (size_t)(stop - each));
            each = stop;
            each = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = each,
                              .size = args->length, .set = "}");
            each += (each < args->length) ? 1ull : 0ull;
        }
        else
        {
            each = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = each,
                              .size = args->length, .set = "}");
            each += (each < args->length) ? 1ull : 0ull;
        }
    }
    return mmgr_line_ok(&args->suggest->texts);
}

// an item of `json` put among the items: 1, or 0 where it has no label, its label is among the first `against` items'
// or there is no room
static int suggest_item_put(const SuggestCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long label_length = 0ull;
    const char *const label =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "label"),
                   .length_pointer = &label_length);
    if ((label == NULL) || (label_length == 0ull) || (args->suggest->items >= GUI_SUGGEST_ITEMS_MOST))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->against; at += 1u)
    {
        const GuiItem *const other = &args->suggest->item[at];
        if ((other->label_length == label_length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->suggest->texts.out + other->label, .other = label, .bytes = label_length}) == 0))
        {
            return 0;
        }
    }
    unsigned long long kind_length = 0ull;
    const char *kind = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = args->one, .name = "kind"),
                                  .length_pointer = &kind_length);
    kind = (kind != NULL) ? kind : "word";
    kind_length = (kind != NULL) ? kind_length : 4ull;
    unsigned long long detail_length = 0ull;
    const char *detail = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = json, .object = args->one, .name = "detail"),
                                    .length_pointer = &detail_length);
    detail = (detail != NULL) ? detail : "";
    unsigned long long insert_length = 0ull;
    const char *insert = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = json, .object = args->one, .name = "insert"),
                                    .length_pointer = &insert_length);
    insert = (insert != NULL) ? insert : label;
    insert_length = (insert != label) ? insert_length : label_length;
    const int snippet = EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->one,
                                   .name = "snippet");
    GuiItem item;
    memor.set(&(MemoriaCfg){.dst = &item, .val = 0, .bytes = sizeof(item)});
    item.label_length = label_length;
    item.kind_length = kind_length;
    item.detail_length = detail_length;
    if (!EMBED_CALL(suggest_store, SuggestCtx, .suggest = args->suggest, .text = label, .length = label_length,
                    .at = &item.label) ||
        !EMBED_CALL(suggest_store, SuggestCtx, .suggest = args->suggest, .text = kind, .length = kind_length,
                    .at = &item.kind) ||
        !EMBED_CALL(suggest_store, SuggestCtx, .suggest = args->suggest, .text = detail, .length = detail_length,
                    .at = &item.detail))
    {
        return 0;
    }
    if (snippet ? !EMBED_CALL(suggest_snippet_store, SuggestCtx, .suggest = args->suggest, .text = insert,
                              .length = insert_length, .at = &item.insert)
                : !EMBED_CALL(suggest_store, SuggestCtx, .suggest = args->suggest, .text = insert,
                              .length = insert_length, .at = &item.insert))
    {
        return 0;
    }
    item.insert_length = args->suggest->texts.at - item.insert;
    args->suggest->item[args->suggest->items] = item;
    args->suggest->items += 1u;
    return 1;
}

// the completion whose items are being ranked, read by the order as it compares two
static const GuiSuggest *s_ranking;

// two items in rank order: the higher score first, then the shorter label, then the labels' bytes
static int suggest_order(const void *one, const void *other)
{
    const GuiItem *const a = &s_ranking->item[*(const unsigned int *)one];
    const GuiItem *const b = &s_ranking->item[*(const unsigned int *)other];
    if (a->score != b->score)
    {
        return (a->score > b->score) ? -1 : 1;
    }
    if (a->label_length != b->label_length)
    {
        return (a->label_length < b->label_length) ? -1 : 1;
    }
    return memor.cmp(&(MemoriaCfg){
        .src = s_ranking->texts.out + a->label, .other = s_ranking->texts.out + b->label, .bytes = a->label_length});
}

// the items shown ranked against the prefix, the row chosen saved on the item it was on where it is still shown
EMBED_INLINE void suggest_rank(const SuggestCtx *args)
{
    const unsigned int saved =
        (args->suggest->count > 0u) ? args->suggest->shown[args->suggest->index] : GUI_SUGGEST_ITEMS_MOST;
    args->suggest->count = 0u;
    for (unsigned int at = 0u; at < args->suggest->items; at += 1u)
    {
        GuiItem *const item = &args->suggest->item[at];
        item->score =
            EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = args->suggest->texts.out + item->label,
                       .length = item->label_length, .typed = args->suggest->prefix);
        if (item->score > 0u)
        {
            args->suggest->shown[args->suggest->count] = at;
            args->suggest->count += 1u;
        }
    }
    s_ranking = args->suggest;
    qsort(args->suggest->shown, args->suggest->count, sizeof(unsigned int), suggest_order);
    s_ranking = NULL;
    args->suggest->index = 0u;
    for (unsigned int at = 0u; at < args->suggest->count; at += 1u)
    {
        if (args->suggest->shown[at] == saved)
        {
            args->suggest->index = at;
        }
    }
}

int gui_suggest_put(const GuiSuggestJsonCfg *args)
{
    const CliJson *json = args->json;
    int forced = args->forced;
    const int was_open = args->suggest->open;
    const unsigned int saved_index = args->suggest->index;
    EMBED_CALL(suggest_texts_free, SuggestCtx, .suggest = args->suggest);
    args->suggest->items = 0u;
    args->suggest->count = 0u;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->root,
                    .kind = CFG_JSON_OBJECT))
    {
        gui_suggest_json.close(args);
        return 0;
    }
    unsigned long long prefix_length = 0ull;
    const char *const prefix =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "prefix"),
                   .length_pointer = &prefix_length);
    VerbaLinea typed = {args->suggest->prefix, sizeof(args->suggest->prefix), 0ull};
    mmgr_line_put_n(&typed, (prefix != NULL) ? prefix : "", prefix_length);
    (void)mmgr_line_finish(&typed);
    const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->root, .name = "from");
    args->suggest->line = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                     .name = "line", .otherwise = 0ull);
    args->suggest->from = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = from,
                                     .name = "col", .otherwise = 0ull);
    const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "items");
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = items, .kind = CFG_JSON_ARRAY)
                                ? json->node[items].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        (void)EMBED_CALL(suggest_item_put, SuggestCtx, .suggest = args->suggest, .json = json, .one = one,
                         .against = 0u);
    }
    const unsigned int given = args->suggest->items;
    const unsigned int words = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "words");
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = words, .kind = CFG_JSON_ARRAY)
                                ? json->node[words].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        (void)EMBED_CALL(suggest_item_put, SuggestCtx, .suggest = args->suggest, .json = json, .one = one,
                         .against = given);
    }
    args->suggest->index = was_open ? saved_index : 0u;
    args->suggest->count = was_open ? 1u : 0u;
    EMBED_CALL(suggest_rank, SuggestCtx, .suggest = args->suggest);
    const GuiItem *const only = (args->suggest->count == 1u) ? &args->suggest->item[args->suggest->shown[0]] : NULL;
    const int alone =
        (only != NULL) && (only->label_length == typed.at) &&
        (memor.cmp(&(MemoriaCfg){
             .src = args->suggest->texts.out + only->label, .other = args->suggest->prefix, .bytes = typed.at}) == 0);
    // nothing typed opens no list unless the keys forced it; a list open stays open on an empty prefix
    const int empty = (typed.at == 0ull) && !forced && !was_open;
    if ((args->suggest->count == 0u) || (alone && !forced) || empty)
    {
        gui_suggest_json.close(args);
        return 0;
    }
    args->suggest->first = was_open ? args->suggest->first : 0u;
    args->suggest->open = 1;
    args->suggest->forced = forced;
    return 1;
}

// the first row shown moved as far as it must to show the row chosen
EMBED_INLINE void suggest_scrolled_to_choose(const SuggestCtx *args)
{
    if (args->suggest->index < args->suggest->first)
    {
        args->suggest->first = args->suggest->index;
    }
    else if (args->suggest->index >= args->suggest->first + GUI_SUGGEST_ROWS)
    {
        args->suggest->first = args->suggest->index - GUI_SUGGEST_ROWS + 1u;
    }
    const unsigned int most = (args->suggest->count > GUI_SUGGEST_ROWS) ? args->suggest->count - GUI_SUGGEST_ROWS : 0u;
    args->suggest->first = (args->suggest->first > most) ? most : args->suggest->first;
}

int gui_suggest_prefix(const GuiSuggestJsonCfg *args)
{
    if (!args->suggest->open)
    {
        return 0;
    }
    VerbaLinea typed = {args->suggest->prefix, sizeof(args->suggest->prefix), 0ull};
    mmgr_line_put_n(&typed, args->prefix, args->length);
    if ((mmgr_line_finish(&typed) == 0ull) || ((args->length == 0ull) && !args->suggest->forced))
    {
        gui_suggest_json.close(args);
        return 0;
    }
    EMBED_CALL(suggest_rank, SuggestCtx, .suggest = args->suggest);
    if (args->suggest->count == 0u)
    {
        gui_suggest_json.close(args);
        return 0;
    }
    EMBED_CALL(suggest_scrolled_to_choose, SuggestCtx, .suggest = args->suggest);
    return 1;
}

unsigned int gui_suggest_key(const GuiSuggestJsonCfg *args)
{
    if (!args->suggest->open || (args->suggest->count == 0u))
    {
        return GUI_SUGGEST_NONE;
    }
    const unsigned int count = args->suggest->count;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Up", .cap = 64ull}))
    {
        args->suggest->index = (args->suggest->index + count - 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Down", .cap = 64ull}))
    {
        args->suggest->index = (args->suggest->index + 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "PageUp", .cap = 64ull}))
    {
        args->suggest->index = (args->suggest->index > GUI_SUGGEST_ROWS) ? args->suggest->index - GUI_SUGGEST_ROWS : 0u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "PageDown", .cap = 64ull}))
    {
        args->suggest->index =
            (args->suggest->index + GUI_SUGGEST_ROWS < count) ? args->suggest->index + GUI_SUGGEST_ROWS : count - 1u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 64ull}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Tab", .cap = 64ull}))
    {
        return GUI_SUGGEST_ACCEPTED;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 64ull}))
    {
        gui_suggest_json.close(args);
        return GUI_SUGGEST_CLOSED;
    }
    else
    {
        return GUI_SUGGEST_NONE;
    }
    EMBED_CALL(suggest_scrolled_to_choose, SuggestCtx, .suggest = args->suggest);
    return GUI_SUGGEST_MOVED;
}

const char *gui_suggest_choose(const GuiSuggestCfg *args)
{
    unsigned long long *length = args->length_pointer;
    if (!args->suggest_read->open || (args->suggest_read->index >= args->suggest_read->count))
    {
        return NULL;
    }
    const GuiItem *const item = &args->suggest_read->item[args->suggest_read->shown[args->suggest_read->index]];
    *length = item->insert_length;
    return args->suggest_read->texts.out + item->insert;
}

int gui_suggest_holds(const GuiSuggestXCfg *args)
{
    return args->suggest_read->open && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                  .rect = args->suggest_read->rect, .x = args->x, .y = args->y);
}

unsigned int gui_suggest_press(const GuiSuggestXYCfg *args)
{
    if (!args->suggest->open || (args->suggest->count == 0u) ||
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->suggest->list, .x = args->x,
                    .y = args->y) ||
        (args->suggest->list.height <= 0))
    {
        return GUI_SUGGEST_NONE;
    }
    // the list's height is a row for each row shown
    const unsigned int rows = (args->suggest->count < GUI_SUGGEST_ROWS) ? args->suggest->count : GUI_SUGGEST_ROWS;
    const int row_tall = args->suggest->list.height / (int)rows;
    const int row = (row_tall > 0) ? (args->y - args->suggest->list.y) / row_tall : 0;
    // a row inside the list is not negative
    const unsigned int at = args->suggest->first + (unsigned int)row;
    if (at >= args->suggest->count)
    {
        return GUI_SUGGEST_NONE;
    }
    args->suggest->index = at;
    return GUI_SUGGEST_ACCEPTED;
}

int gui_suggest_wheel(const GuiSuggestRowsCfg *args)
{
    if (!args->suggest->open || (args->suggest->count <= GUI_SUGGEST_ROWS))
    {
        return 0;
    }
    const unsigned int before = args->suggest->first;
    const unsigned int most = args->suggest->count - GUI_SUGGEST_ROWS;
    if (args->rows < 0)
    {
        const unsigned int up = (unsigned int)(-args->rows);
        args->suggest->first = (args->suggest->first > up) ? args->suggest->first - up : 0u;
    }
    else
    {
        // the rows turned are not negative
        args->suggest->first += (unsigned int)args->rows;
        args->suggest->first = (args->suggest->first > most) ? most : args->suggest->first;
    }
    return args->suggest->first != before;
}

// the color of a kind's mark: the token colors the page gives each kind
EMBED_INLINE GuiColor suggest_kind_color(const SuggestCtx *args)
{
    const GuiPalette *palette = args->palette;
    const char *kind = args->kind;
    unsigned long long length = args->length;
    static const struct
    {
        const char *kind;
        const char *token;
    } s_marks[] = {{"type", "type"},           {"class", "type"},         {"state", "type"},
                   {"function", "predefined"}, {"method", "predefined"},  {"operator", "predefined"},
                   {"field", "attribute"},     {"property", "attribute"}, {"variable", "variable"},
                   {"parameter", "variable"},  {"local", "variable"},     {"constant", "number"},
                   {"snippet", "string"},      {"module", "tag"}};
    for (unsigned int at = 0u; at < sizeof(s_marks) / sizeof(s_marks[0]); at += 1u)
    {
        const unsigned long long known = cellul.len(&(CatenaFinitaCfg){.src = s_marks[at].kind, .cap = 32ull});
        if ((known == length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_marks[at].kind, .other = kind, .bytes = length}) == 0))
        {
            const char *const token = s_marks[at].token;
            return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = token,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = token, .cap = 32ull}));
        }
    }
    return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = "keyword",
                      .length = 7ull);
}

void gui_suggest_draw(const GuiSuggestRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    GuiRect anchor = args->anchor;
    GuiRect bounds = args->bounds;
    int scale = args->scale;
    if (!args->suggest->open || (args->suggest->count == 0u))
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_PANEL_CODE_SIZE, .scale = scale);
    const int row_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_ROW, .scale = scale);
    const int pad_x =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_PAD_X, .scale = scale);
    const int pad_y =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_PAD_Y, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_GAP, .scale = scale);
    const int mark = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_KIND, .scale = scale);
    EMBED_CALL(suggest_scrolled_to_choose, SuggestCtx, .suggest = args->suggest);
    const unsigned int rows = (args->suggest->count < GUI_SUGGEST_ROWS) ? args->suggest->count : GUI_SUGGEST_ROWS;
    // the list's width is its widest row's, between the least and the most
    float widest = 0.0f;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const GuiItem *const item = &args->suggest->item[args->suggest->shown[args->suggest->first + row]];
        float wide =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .text = args->suggest->texts.out + item->label, .length = item->label_length);
        if (item->detail_length > 0ull)
        {
            wide +=
                (float)gap + EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                        .face = GUI_FACE_CODE, .size = size,
                                        .text = args->suggest->texts.out + item->detail, .length = item->detail_length);
        }
        widest = (wide > widest) ? wide : widest;
    }
    int list_wide = (int)(widest + 0.5f) + (2 * pad_x) + mark + gap;
    const int least =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_LEAST, .scale = scale);
    const int most = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_MOST, .scale = scale);
    list_wide = (list_wide < least) ? least : ((list_wide > most) ? most : list_wide);
    // the rows are few: their height is far inside what an int represents
    const int list_tall = ((int)rows * row_tall) + (2 * pad_y);
    const GuiItem *const chosen = &args->suggest->item[args->suggest->shown[args->suggest->index]];
    const int detailed = chosen->detail_length > 0ull;
    const int detail_wide =
        detailed ? EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL, .scale = scale)
                 : 0;
    int wide = list_wide + detail_wide;
    wide = (wide > bounds.width) ? bounds.width : wide;
    args->suggest->rect = EMBED_CALL(
        gui_panel_wide.place, GuiPanelWideCfg, .anchor = anchor, .wide = wide, .tall = list_tall,
        .back = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_BACK, .scale = scale),
        .bounds = bounds, .above = 0, .scale = scale);
    args->suggest->list =
        (GuiRect){args->suggest->rect.x, args->suggest->rect.y + pad_y, list_wide, (int)rows * row_tall};
    args->suggest->detail =
        detailed ? (GuiRect){args->suggest->rect.x + list_wide, args->suggest->rect.y, wide - list_wide, list_tall}
                 : (GuiRect){0, 0, 0, 0};
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = raster, .palette = palette, .rect = args->suggest->rect,
               .scale = scale);
    const GuiRect clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->suggest->rect, .other = bounds);
    const unsigned long long typed =
        cellul.len(&(CatenaFinitaCfg){.src = args->suggest->prefix, .cap = sizeof(args->suggest->prefix)});
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    const int baseline = (int)((((float)row_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const unsigned int at = args->suggest->first + row;
        const GuiItem *const item = &args->suggest->item[args->suggest->shown[at]];
        const GuiRect row_rect = {args->suggest->list.x, args->suggest->list.y + ((int)row * row_tall), list_wide,
                                  row_tall};
        if (at == args->suggest->index)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row_rect, .other = clip),
                       .color = palette->selection);
        }
        const GuiRect mark_rect = {row_rect.x + pad_x, row_rect.y + ((row_tall - mark) / 2), mark, mark};
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = mark_rect, .other = clip),
                   .radius =
                       (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale),
                   .color = EMBED_CALL(suggest_kind_color, SuggestCtx, .palette = palette,
                                       .kind = args->suggest->texts.out + item->kind, .length = item->kind_length));
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){row_rect.x, row_rect.y, list_wide - pad_x, row_tall}, .other = clip);
        float x = (float)(row_rect.x + pad_x + mark + gap);
        const char *const label = args->suggest->texts.out + item->label;
        const int starts = (typed > 0ull) && (typed <= item->label_length) &&
                           (EMBED_CALL(gui_suggest_length.score, GuiSuggestLengthCfg, .label = label,
                                       .length = item->label_length, .typed = args->suggest->prefix) >= 4u);
        if (starts)
        {
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE_BOLD, .size = size, .x_f32 = x, .y = row_rect.y + baseline,
                           .text = label, .length = typed, .color = palette->signal, .clip = text_clip);
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = size, .x_f32 = x, .y = row_rect.y + baseline,
                           .text = label + typed, .length = item->label_length - typed, .color = palette->text,
                           .clip = text_clip);
        }
        else
        {
            x = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .size = size, .x_f32 = x, .y = row_rect.y + baseline, .text = label,
                           .length = item->label_length, .color = palette->text, .clip = text_clip);
        }
        if (item->detail_length > 0ull)
        {
            const float detail_wide_text = EMBED_CALL(
                gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                .size = size, .text = args->suggest->texts.out + item->detail, .length = item->detail_length);
            float detail_x = (float)(row_rect.x + list_wide - pad_x) - detail_wide_text;
            detail_x = (detail_x < x + (float)gap) ? x + (float)gap : detail_x;
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = size, .x_f32 = detail_x, .y = row_rect.y + baseline,
                             .text = args->suggest->texts.out + item->detail, .length = item->detail_length,
                             .color = palette->text_3, .clip = text_clip);
        }
    }
    if (detailed)
    {
        const int line = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->suggest->detail.x, args->suggest->detail.y + line, line,
                                                       args->suggest->detail.height - (2 * line)},
                                      .other = clip),
                   .color = palette->widget_line);
        const int detail_pad_x =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL_PAD_X, .scale = scale);
        const int detail_pad_y =
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SUGGEST_DETAIL_PAD_Y, .scale = scale);
        // the detail is laid as code, the panel's inline code, inside one pair of backticks
        char fenced[GUI_PANEL_RUN + 2u];
        VerbaLinea text = {fenced, sizeof(fenced), 0ull};
        mmgr_line_ch(&text, '`');
        mmgr_line_put_n(&text, args->suggest->texts.out + chosen->detail,
                        (chosen->detail_length < GUI_PANEL_RUN) ? chosen->detail_length : GUI_PANEL_RUN);
        mmgr_line_ch(&text, '`');
        const GuiRect detail_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){args->suggest->detail.x + detail_pad_x, args->suggest->detail.y + detail_pad_y,
                                        args->suggest->detail.width - (2 * detail_pad_x),
                                        args->suggest->detail.height - (2 * detail_pad_y)},
                       .other = clip);
        (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = raster, .glyphs = glyphs,
                         .palette = palette, .text = fenced, .length = mmgr_line_ok(&text) ? text.at : 0ull,
                         .x = args->suggest->detail.x + detail_pad_x, .y = args->suggest->detail.y + detail_pad_y,
                         .wide = args->suggest->detail.width - (2 * detail_pad_x), .scale = scale, .clip = detail_clip);
    }
}
