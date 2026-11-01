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

int gui_suggest_made(GuiSuggest *suggest)
{
    memor.set(&(MemoriaCfg){.dst = suggest, .val = 0, .bytes = sizeof(*suggest)});
    suggest->item = g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_ITEMS_MOST * sizeof(GuiItem), &g_gui_suggest_cells);
    suggest->shown =
        g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_ITEMS_MOST * sizeof(unsigned int), &g_gui_suggest_cells);
    if ((suggest->item == NULL) || (suggest->shown == NULL))
    {
        gui_suggest_given(suggest);
        return 0;
    }
    return 1;
}

// the items' texts freed, the line left empty
static void suggest_texts_given(GuiSuggest *suggest)
{
    g_cli_cells->persistent_buf_release(suggest->texts.out, &g_gui_suggest_cells);
    memor.set(&(MemoriaCfg){.dst = &suggest->texts, .val = 0, .bytes = sizeof(suggest->texts)});
}

void gui_suggest_given(GuiSuggest *suggest)
{
    g_cli_cells->persistent_buf_release(suggest->item, &g_gui_suggest_cells);
    g_cli_cells->persistent_buf_release(suggest->shown, &g_gui_suggest_cells);
    suggest_texts_given(suggest);
    suggest->item = NULL;
    suggest->shown = NULL;
    suggest->items = 0u;
    suggest->count = 0u;
    suggest->open = 0;
}

void gui_suggest_closed(GuiSuggest *suggest)
{
    suggest_texts_given(suggest);
    suggest->items = 0u;
    suggest->count = 0u;
    suggest->index = 0u;
    suggest->first = 0u;
    suggest->open = 0;
    suggest->forced = 0;
    suggest->rect = (GuiRect){0, 0, 0, 0};
    suggest->list = (GuiRect){0, 0, 0, 0};
    suggest->detail = (GuiRect){0, 0, 0, 0};
}

// a lower case letter for an ASCII capital, any other byte as it is
static char suggest_lower(char byte)
{
    return ((byte >= 'A') && (byte <= 'Z')) ? (char)(byte + ('a' - 'A')) : byte;
}

unsigned int gui_suggest_score(const char *label, unsigned long long length, const char *typed)
{
    const unsigned long long want = cellul.len(&(CatenaFinitaCfg){.src = typed, .cap = GUI_SUGGEST_PREFIX});
    if (want == 0ull)
    {
        return 1u;
    }
    if (want > length)
    {
        // a prefix longer than the label has letters the label cannot hold in order: nothing answers
        return 0u;
    }
    if (memor.cmp(&(MemoriaCfg){.src = label, .other = typed, .bytes = want}) == 0)
    {
        return 5u;
    }
    int same = 1;
    for (unsigned long long at = 0ull; same && (at < want); at += 1ull)
    {
        same = suggest_lower(label[at]) == suggest_lower(typed[at]);
    }
    if (same)
    {
        return 4u;
    }
    for (unsigned long long start = 0ull; start + want <= length; start += 1ull)
    {
        int part = 1;
        for (unsigned long long at = 0ull; part && (at < want); at += 1ull)
        {
            part = suggest_lower(label[start + at]) == suggest_lower(typed[at]);
        }
        if (part)
        {
            return 3u;
        }
    }
    unsigned long long at = 0ull;
    for (unsigned long long each = 0ull; (each < length) && (at < want); each += 1ull)
    {
        at += (suggest_lower(label[each]) == suggest_lower(typed[at])) ? 1ull : 0ull;
    }
    return (at == want) ? 2u : 0u;
}

// the items' texts' one cell, taken as the first is stored
static void suggest_texts_taken(GuiSuggest *suggest)
{
    if (suggest->texts.out == NULL)
    {
        char *const cell = g_cli_cells->persistent_buf_alloc(GUI_SUGGEST_TEXT_ROOM, &g_gui_suggest_cells);
        suggest->texts = mmgr_line_over(cell, GUI_SUGGEST_TEXT_ROOM);
    }
}

// `length` bytes of `text` stored among the items' texts: where they start: 1, or 0 where they do not fit
static int suggest_stored(GuiSuggest *suggest, const char *text, unsigned long long length, unsigned long long *at)
{
    suggest_texts_taken(suggest);
    *at = suggest->texts.at;
    mmgr_line_put_n(&suggest->texts, text, length);
    return mmgr_line_ok(&suggest->texts);
}

// `length` bytes of a snippet `text` stored among the items' texts as the text it inserts: a tab stop with a
// placeholder, `${n:text}`, as its placeholder, one with choices, `${n|a,b|}`, as its first choice, a bare stop,
// `$n` or `${n}`, as nothing, and an escaped byte as itself: where it starts, or 0 with the line full
static int suggest_snippet_stored(GuiSuggest *suggest, const char *text, unsigned long long length,
                                  unsigned long long *at)
{
    suggest_texts_taken(suggest);
    *at = suggest->texts.at;
    unsigned long long each = 0ull;
    while (each < length)
    {
        const char byte = text[each];
        if ((byte == '\\') && (each + 1ull < length))
        {
            mmgr_line_ch(&suggest->texts, text[each + 1ull]);
            each += 2ull;
            continue;
        }
        if (byte != '$')
        {
            mmgr_line_ch(&suggest->texts, byte);
            each += 1ull;
            continue;
        }
        each += 1ull;
        if ((each < length) && (text[each] != '{'))
        {
            // a bare stop: its digits
            each = cli_text_past_class(text, each, length, CLI_TEXT_DIGITS, "");
            continue;
        }
        each += (each < length) ? 1ull : 0ull;
        each = cli_text_past_class(text, each, length, CLI_TEXT_DIGITS, "");
        if ((each < length) && (text[each] == ':'))
        {
            // the placeholder's text up to the closing brace, nested stops inside it flattened the same way
            each += 1ull;
            unsigned long long depth = 1ull;
            while ((each < length) && (depth > 0ull))
            {
                if ((text[each] == '\\') && (each + 1ull < length))
                {
                    mmgr_line_ch(&suggest->texts, text[each + 1ull]);
                    each += 2ull;
                    continue;
                }
                if ((text[each] == '$') && (each + 1ull < length) && (text[each + 1ull] == '{'))
                {
                    depth += 1ull;
                    each += 2ull;
                    each = cli_text_past_class(text, each, length, CLI_TEXT_DIGITS, "");
                    each += ((each < length) && (text[each] == ':')) ? 1ull : 0ull;
                    continue;
                }
                if (text[each] == '}')
                {
                    depth -= 1ull;
                    each += 1ull;
                    continue;
                }
                mmgr_line_ch(&suggest->texts, text[each]);
                each += 1ull;
            }
        }
        else if ((each < length) && (text[each] == '|'))
        {
            // the first choice
            each += 1ull;
            const unsigned long long stop = cli_text_to_any(text, each, length, ",|");
            mmgr_line_put_n(&suggest->texts, text + each, (size_t)(stop - each));
            each = stop;
            each = cli_text_to_any(text, each, length, "}");
            each += (each < length) ? 1ull : 0ull;
        }
        else
        {
            each = cli_text_to_any(text, each, length, "}");
            each += (each < length) ? 1ull : 0ull;
        }
    }
    return mmgr_line_ok(&suggest->texts);
}

// an item of `json` put among the items: 1, or 0 where it has no label, its label is among the first `against` items'
// or there is no room
static int suggest_item_put(GuiSuggest *suggest, const CliJson *json, unsigned int one, unsigned int against)
{
    unsigned long long label_length = 0ull;
    const char *const label = cli_json_string(json, cli_json_member(json, one, "label"), &label_length);
    if ((label == NULL) || (label_length == 0ull) || (suggest->items >= GUI_SUGGEST_ITEMS_MOST))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < against; at += 1u)
    {
        const GuiItem *const other = &suggest->item[at];
        if ((other->label_length == label_length) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = suggest->texts.out + other->label, .other = label, .bytes = label_length}) == 0))
        {
            return 0;
        }
    }
    unsigned long long kind_length = 0ull;
    const char *kind = cli_json_string(json, cli_json_member(json, one, "kind"), &kind_length);
    kind = (kind != NULL) ? kind : "word";
    kind_length = (kind != NULL) ? kind_length : 4ull;
    unsigned long long detail_length = 0ull;
    const char *detail = cli_json_string(json, cli_json_member(json, one, "detail"), &detail_length);
    detail = (detail != NULL) ? detail : "";
    unsigned long long insert_length = 0ull;
    const char *insert = cli_json_string(json, cli_json_member(json, one, "insert"), &insert_length);
    insert = (insert != NULL) ? insert : label;
    insert_length = (insert != label) ? insert_length : label_length;
    const int snippet = gui_answer_member_true(json, one, "snippet");
    GuiItem item;
    memor.set(&(MemoriaCfg){.dst = &item, .val = 0, .bytes = sizeof(item)});
    item.label_length = label_length;
    item.kind_length = kind_length;
    item.detail_length = detail_length;
    if (!suggest_stored(suggest, label, label_length, &item.label) ||
        !suggest_stored(suggest, kind, kind_length, &item.kind) ||
        !suggest_stored(suggest, detail, detail_length, &item.detail))
    {
        return 0;
    }
    if (snippet ? !suggest_snippet_stored(suggest, insert, insert_length, &item.insert)
                : !suggest_stored(suggest, insert, insert_length, &item.insert))
    {
        return 0;
    }
    item.insert_length = suggest->texts.at - item.insert;
    suggest->item[suggest->items] = item;
    suggest->items += 1u;
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

// the items shown ranked against the prefix, the row chosen kept on the item it was on where it is still shown
static void suggest_ranked(GuiSuggest *suggest)
{
    const unsigned int kept = (suggest->count > 0u) ? suggest->shown[suggest->index] : GUI_SUGGEST_ITEMS_MOST;
    suggest->count = 0u;
    for (unsigned int at = 0u; at < suggest->items; at += 1u)
    {
        GuiItem *const item = &suggest->item[at];
        item->score = gui_suggest_score(suggest->texts.out + item->label, item->label_length, suggest->prefix);
        if (item->score > 0u)
        {
            suggest->shown[suggest->count] = at;
            suggest->count += 1u;
        }
    }
    s_ranking = suggest;
    qsort(suggest->shown, suggest->count, sizeof(unsigned int), suggest_order);
    s_ranking = NULL;
    suggest->index = 0u;
    for (unsigned int at = 0u; at < suggest->count; at += 1u)
    {
        if (suggest->shown[at] == kept)
        {
            suggest->index = at;
        }
    }
}

int gui_suggest_put(GuiSuggest *suggest, const CliJson *json, unsigned int root, int forced)
{
    const int was_open = suggest->open;
    const unsigned int kept_index = suggest->index;
    suggest_texts_given(suggest);
    suggest->items = 0u;
    suggest->count = 0u;
    if (!cli_json_is(json, root, CFG_JSON_OBJECT))
    {
        gui_suggest_closed(suggest);
        return 0;
    }
    unsigned long long prefix_length = 0ull;
    const char *const prefix = cli_json_string(json, cli_json_member(json, root, "prefix"), &prefix_length);
    VerbaLinea typed = {suggest->prefix, sizeof(suggest->prefix), 0ull};
    mmgr_line_put_n(&typed, (prefix != NULL) ? prefix : "", prefix_length);
    (void)mmgr_line_finish(&typed);
    const unsigned int from = cli_json_member(json, root, "from");
    suggest->line = gui_answer_member_whole(json, from, "line", 0ull);
    suggest->from = gui_answer_member_whole(json, from, "col", 0ull);
    const unsigned int items = cli_json_member(json, root, "items");
    for (unsigned int one = cli_json_is(json, items, CFG_JSON_ARRAY) ? json->node[items].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        (void)suggest_item_put(suggest, json, one, 0u);
    }
    const unsigned int given = suggest->items;
    const unsigned int words = cli_json_member(json, root, "words");
    for (unsigned int one = cli_json_is(json, words, CFG_JSON_ARRAY) ? json->node[words].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        (void)suggest_item_put(suggest, json, one, given);
    }
    suggest->index = was_open ? kept_index : 0u;
    suggest->count = was_open ? 1u : 0u;
    suggest_ranked(suggest);
    const GuiItem *const only = (suggest->count == 1u) ? &suggest->item[suggest->shown[0]] : NULL;
    const int alone = (only != NULL) && (only->label_length == typed.at) &&
                      (memor.cmp(&(MemoriaCfg){
                           .src = suggest->texts.out + only->label, .other = suggest->prefix, .bytes = typed.at}) == 0);
    // nothing typed opens no list unless the keys forced it; a list open stays open on an empty prefix
    const int empty = (typed.at == 0ull) && !forced && !was_open;
    if ((suggest->count == 0u) || (alone && !forced) || empty)
    {
        gui_suggest_closed(suggest);
        return 0;
    }
    suggest->first = was_open ? suggest->first : 0u;
    suggest->open = 1;
    suggest->forced = forced;
    return 1;
}

// the first row shown moved as far as it must to show the row chosen
static void suggest_scrolled_to_chosen(GuiSuggest *suggest)
{
    if (suggest->index < suggest->first)
    {
        suggest->first = suggest->index;
    }
    else if (suggest->index >= suggest->first + GUI_SUGGEST_ROWS)
    {
        suggest->first = suggest->index - GUI_SUGGEST_ROWS + 1u;
    }
    const unsigned int most = (suggest->count > GUI_SUGGEST_ROWS) ? suggest->count - GUI_SUGGEST_ROWS : 0u;
    suggest->first = (suggest->first > most) ? most : suggest->first;
}

int gui_suggest_prefixed(GuiSuggest *suggest, const char *prefix, unsigned long long length)
{
    if (!suggest->open)
    {
        return 0;
    }
    VerbaLinea typed = {suggest->prefix, sizeof(suggest->prefix), 0ull};
    mmgr_line_put_n(&typed, prefix, length);
    if ((mmgr_line_finish(&typed) == 0ull) || ((length == 0ull) && !suggest->forced))
    {
        gui_suggest_closed(suggest);
        return 0;
    }
    suggest_ranked(suggest);
    if (suggest->count == 0u)
    {
        gui_suggest_closed(suggest);
        return 0;
    }
    suggest_scrolled_to_chosen(suggest);
    return 1;
}

unsigned int gui_suggest_keyed(GuiSuggest *suggest, const char *name)
{
    if (!suggest->open || (suggest->count == 0u))
    {
        return GUI_SUGGEST_NONE;
    }
    const unsigned int count = suggest->count;
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Up", .cap = 64ull}))
    {
        suggest->index = (suggest->index + count - 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Down", .cap = 64ull}))
    {
        suggest->index = (suggest->index + 1u) % count;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageUp", .cap = 64ull}))
    {
        suggest->index = (suggest->index > GUI_SUGGEST_ROWS) ? suggest->index - GUI_SUGGEST_ROWS : 0u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "PageDown", .cap = 64ull}))
    {
        suggest->index = (suggest->index + GUI_SUGGEST_ROWS < count) ? suggest->index + GUI_SUGGEST_ROWS : count - 1u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Enter", .cap = 64ull}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Tab", .cap = 64ull}))
    {
        return GUI_SUGGEST_ACCEPTED;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ull}))
    {
        gui_suggest_closed(suggest);
        return GUI_SUGGEST_CLOSED;
    }
    else
    {
        return GUI_SUGGEST_NONE;
    }
    suggest_scrolled_to_chosen(suggest);
    return GUI_SUGGEST_MOVED;
}

const char *gui_suggest_chosen(const GuiSuggest *suggest, unsigned long long *length)
{
    if (!suggest->open || (suggest->index >= suggest->count))
    {
        return NULL;
    }
    const GuiItem *const item = &suggest->item[suggest->shown[suggest->index]];
    *length = item->insert_length;
    return suggest->texts.out + item->insert;
}

int gui_suggest_holds(const GuiSuggest *suggest, int x, int y)
{
    return suggest->open && gui_rect_holds(suggest->rect, x, y);
}

unsigned int gui_suggest_pressed(GuiSuggest *suggest, int x, int y)
{
    if (!suggest->open || (suggest->count == 0u) || !gui_rect_holds(suggest->list, x, y) || (suggest->list.height <= 0))
    {
        return GUI_SUGGEST_NONE;
    }
    // the list's height is a row for each row shown
    const unsigned int rows = (suggest->count < GUI_SUGGEST_ROWS) ? suggest->count : GUI_SUGGEST_ROWS;
    const int row_tall = suggest->list.height / (int)rows;
    const int row = (row_tall > 0) ? (y - suggest->list.y) / row_tall : 0;
    // a row inside the list is not negative
    const unsigned int at = suggest->first + (unsigned int)row;
    if (at >= suggest->count)
    {
        return GUI_SUGGEST_NONE;
    }
    suggest->index = at;
    return GUI_SUGGEST_ACCEPTED;
}

int gui_suggest_wheeled(GuiSuggest *suggest, int rows)
{
    if (!suggest->open || (suggest->count <= GUI_SUGGEST_ROWS))
    {
        return 0;
    }
    const unsigned int before = suggest->first;
    const unsigned int most = suggest->count - GUI_SUGGEST_ROWS;
    if (rows < 0)
    {
        const unsigned int up = (unsigned int)(-rows);
        suggest->first = (suggest->first > up) ? suggest->first - up : 0u;
    }
    else
    {
        // the rows turned are not negative
        suggest->first += (unsigned int)rows;
        suggest->first = (suggest->first > most) ? most : suggest->first;
    }
    return suggest->first != before;
}

// the color of a kind's mark: the token colors the page gives each kind
static GuiColor suggest_kind_color(const GuiPalette *palette, const char *kind, unsigned long long length)
{
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
            return gui_palette_token(palette, token, cellul.len(&(CatenaFinitaCfg){.src = token, .cap = 32ull}));
        }
    }
    return gui_palette_token(palette, "keyword", 7ull);
}

void gui_suggest_drawn(GuiSuggest *suggest, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                       GuiRect anchor, GuiRect bounds, float scale)
{
    if (!suggest->open || (suggest->count == 0u))
    {
        return;
    }
    const unsigned int size = (unsigned int)(((float)GUI_PANEL_CODE_SIZE * scale) + 0.5f);
    const int row_tall = gui_scaled(GUI_SUGGEST_ROW, scale);
    const int pad_x = gui_scaled(GUI_SUGGEST_PAD_X, scale);
    const int pad_y = gui_scaled(GUI_SUGGEST_PAD_Y, scale);
    const int gap = gui_scaled(GUI_SUGGEST_GAP, scale);
    const int mark = gui_scaled(GUI_SUGGEST_KIND, scale);
    suggest_scrolled_to_chosen(suggest);
    const unsigned int rows = (suggest->count < GUI_SUGGEST_ROWS) ? suggest->count : GUI_SUGGEST_ROWS;
    // the list's width is its widest row's, between the least and the most
    float widest = 0.0f;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const GuiItem *const item = &suggest->item[suggest->shown[suggest->first + row]];
        float wide = gui_text_width(glyphs, GUI_FACE_CODE, size, suggest->texts.out + item->label, item->label_length);
        if (item->detail_length > 0ull)
        {
            wide += (float)gap +
                    gui_text_width(glyphs, GUI_FACE_CODE, size, suggest->texts.out + item->detail, item->detail_length);
        }
        widest = (wide > widest) ? wide : widest;
    }
    int list_wide = (int)(widest + 0.5f) + (2 * pad_x) + mark + gap;
    const int least = gui_scaled(GUI_SUGGEST_LEAST, scale);
    const int most = gui_scaled(GUI_SUGGEST_MOST, scale);
    list_wide = (list_wide < least) ? least : ((list_wide > most) ? most : list_wide);
    // the rows are few: their height is far inside what an int represents
    const int list_tall = ((int)rows * row_tall) + (2 * pad_y);
    const GuiItem *const chosen = &suggest->item[suggest->shown[suggest->index]];
    const int detailed = chosen->detail_length > 0ull;
    const int detail_wide = detailed ? gui_scaled(GUI_SUGGEST_DETAIL, scale) : 0;
    int wide = list_wide + detail_wide;
    wide = (wide > bounds.width) ? bounds.width : wide;
    suggest->rect = gui_panel_placed(anchor, wide, list_tall, gui_scaled(GUI_SUGGEST_BACK, scale), bounds, 0, scale);
    suggest->list = (GuiRect){suggest->rect.x, suggest->rect.y + pad_y, list_wide, (int)rows * row_tall};
    suggest->detail = detailed ? (GuiRect){suggest->rect.x + list_wide, suggest->rect.y, wide - list_wide, list_tall}
                               : (GuiRect){0, 0, 0, 0};
    gui_panel_box_drawn(raster, palette, suggest->rect, scale);
    const GuiRect clip = gui_rect_met(suggest->rect, bounds);
    const unsigned long long typed =
        cellul.len(&(CatenaFinitaCfg){.src = suggest->prefix, .cap = sizeof(suggest->prefix)});
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_CODE, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_CODE, size);
    const int baseline = (int)((((float)row_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        const unsigned int at = suggest->first + row;
        const GuiItem *const item = &suggest->item[suggest->shown[at]];
        const GuiRect row_rect = {suggest->list.x, suggest->list.y + ((int)row * row_tall), list_wide, row_tall};
        if (at == suggest->index)
        {
            gui_raster_filled(raster, gui_rect_met(row_rect, clip), palette->selection);
        }
        const GuiRect mark_rect = {row_rect.x + pad_x, row_rect.y + ((row_tall - mark) / 2), mark, mark};
        gui_raster_rounded(raster, gui_rect_met(mark_rect, clip), (unsigned int)gui_scaled(2, scale),
                           suggest_kind_color(palette, suggest->texts.out + item->kind, item->kind_length));
        const GuiRect text_clip = gui_rect_met((GuiRect){row_rect.x, row_rect.y, list_wide - pad_x, row_tall}, clip);
        float x = (float)(row_rect.x + pad_x + mark + gap);
        const char *const label = suggest->texts.out + item->label;
        const int starts = (typed > 0ull) && (typed <= item->label_length) &&
                           (gui_suggest_score(label, item->label_length, suggest->prefix) >= 4u);
        if (starts)
        {
            x = gui_text_drawn(raster, glyphs, GUI_FACE_CODE_BOLD, size, x, row_rect.y + baseline, label, typed,
                               palette->signal, text_clip);
            x = gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, x, row_rect.y + baseline, label + typed,
                               item->label_length - typed, palette->text, text_clip);
        }
        else
        {
            x = gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, x, row_rect.y + baseline, label, item->label_length,
                               palette->text, text_clip);
        }
        if (item->detail_length > 0ull)
        {
            const float detail_wide_text =
                gui_text_width(glyphs, GUI_FACE_CODE, size, suggest->texts.out + item->detail, item->detail_length);
            float detail_x = (float)(row_rect.x + list_wide - pad_x) - detail_wide_text;
            detail_x = (detail_x < x + (float)gap) ? x + (float)gap : detail_x;
            (void)gui_text_drawn(raster, glyphs, GUI_FACE_CODE, size, detail_x, row_rect.y + baseline,
                                 suggest->texts.out + item->detail, item->detail_length, palette->text_3, text_clip);
        }
    }
    if (detailed)
    {
        const int line = gui_scaled(1, scale);
        gui_raster_filled(raster,
                          gui_rect_met((GuiRect){suggest->detail.x, suggest->detail.y + line, line,
                                                 suggest->detail.height - (2 * line)},
                                       clip),
                          palette->widget_line);
        const int detail_pad_x = gui_scaled(GUI_SUGGEST_DETAIL_PAD_X, scale);
        const int detail_pad_y = gui_scaled(GUI_SUGGEST_DETAIL_PAD_Y, scale);
        // the detail is laid as code, the panel's inline code, inside one pair of backticks
        char fenced[GUI_PANEL_RUN + 2u];
        VerbaLinea text = {fenced, sizeof(fenced), 0ull};
        mmgr_line_ch(&text, '`');
        mmgr_line_put_n(&text, suggest->texts.out + chosen->detail,
                        (chosen->detail_length < GUI_PANEL_RUN) ? chosen->detail_length : GUI_PANEL_RUN);
        mmgr_line_ch(&text, '`');
        const GuiRect detail_clip = gui_rect_met(
            (GuiRect){suggest->detail.x + detail_pad_x, suggest->detail.y + detail_pad_y,
                      suggest->detail.width - (2 * detail_pad_x), suggest->detail.height - (2 * detail_pad_y)},
            clip);
        (void)gui_panel_markdown(raster, glyphs, palette, fenced, mmgr_line_ok(&text) ? text.at : 0ull,
                                 suggest->detail.x + detail_pad_x, suggest->detail.y + detail_pad_y,
                                 suggest->detail.width - (2 * detail_pad_x), scale, detail_clip);
    }
}
