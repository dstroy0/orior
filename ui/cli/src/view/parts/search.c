// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// search.c: the Search pane's field, choices, set, report and rows as the search pane lists them
#include "search.h"

#include "../../command_line/cli_internal.h"
#include "../../editor/files.h"
#include "explorer.h"

// what the field reads while the query is empty
#define SEARCH_PLACEHOLDER "Search in files"

// A choice beside the field: its action and its label
typedef struct
{
    unsigned int which;
    const char *label;
} SearchOption;

// the choices, left to right at the field's right
static const SearchOption s_options[] = {
    {CLI_SEARCH_CASE, "Aa"},
    {CLI_SEARCH_WORD, "ab"},
    {CLI_SEARCH_REGEX, ".*"},
};

typedef struct
{
    const GuiPaneCfg *pane;
    unsigned int which;
} SearchCtx;

static const char *search_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Search";
}

static unsigned int search_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_search_pane.rows, CliSearchPaneCfg, .at = 0u);
}

// whether Preferences lists a set of files to search in
EMBED_INLINE int search_sets_listed(const SearchCtx *args)
{
    (void)args;
    unsigned int count = 0u;
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_search_pane.set, CliSearchPaneCfg, .count_out = &count, .length_out = &length);
    return count != 0u;
}

// the field's row, the set's row where sets are listed, and the report's row
static int search_head(const GuiPaneCfg *args)
{
    (void)args;
    return GUI_PANE_BOX + (EMBED_CALL(search_sets_listed, SearchCtx, .pane = args) ? GUI_PANE_CHOICE : 0) +
           GUI_SEARCH_REPORT;
}

// the field's rectangle, in the head's first row
EMBED_INLINE GuiRect search_field_rect(const SearchCtx *args)
{
    return EMBED_CALL(gui_pane_field.field_rect, GuiPaneFieldCfg, .pane = args->pane, .y = args->pane->rect.y);
}

// the rectangle of the choice `which` inside the field, the choices standing from the field's right edge inward in
// their order, each its label's width with its padding
EMBED_INLINE GuiRect search_option_rect(const SearchCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect field = EMBED_CALL(search_field_rect, SearchCtx, .pane = pane);
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_SEARCH_OPTION_PAD);
    const int gap = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_SEARCH_OPTION_GAP);
    const int height = field.height - (2 * gap);
    int right = field.x + field.width - gap;
    for (unsigned int at = sizeof(s_options) / sizeof(s_options[0]); at > 0u; at -= 1u)
    {
        const SearchOption *const option = &s_options[at - 1u];
        const int width =
            (int)(EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = pane, .face = GUI_FACE_CODE_BOLD,
                             .size = GUI_SEARCH_OPTION_SIZE, .text = option->label,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = option->label, .cap = 8ULL})) +
                  0.5f) +
            (2 * pad);
        if (option->which == args->which)
        {
            return (GuiRect){right - width, field.y + gap, width, height};
        }
        right -= width + gap;
    }
    return (GuiRect){0, 0, 0, 0};
}

// the set's row, under the field, where sets are listed; and the report's row under that
EMBED_INLINE GuiRect search_set_rect(const SearchCtx *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const int box = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX);
    const int set = EMBED_CALL(search_sets_listed, SearchCtx, .pane = pane)
                        ? EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CHOICE)
                        : 0;
    return (GuiRect){pane->rect.x, pane->rect.y + box, pane->rect.width, set};
}

EMBED_INLINE GuiRect search_report_rect(const SearchCtx *args)
{
    const GuiRect set = EMBED_CALL(search_set_rect, SearchCtx, .pane = args->pane);
    return (GuiRect){args->pane->rect.x, set.y + set.height, args->pane->rect.width,
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane, .x = GUI_SEARCH_REPORT)};
}

// the head drawn: the choices at the field's right each lit where on and on the line's ground where pointed, the field
// with the query cut before them, the set chosen under the field where sets are listed, and the report
static void search_head_draw(const GuiPaneCfg *args)
{
    const GuiPalette *const palette = args->palette;
    const GuiRect field = EMBED_CALL(search_field_rect, SearchCtx, .pane = args);
    // the choices, and the field's text cut before them
    int text_right = field.x + field.width;
    for (unsigned int at = 0u; at < sizeof(s_options) / sizeof(s_options[0]); at += 1u)
    {
        const SearchOption *const option = &s_options[at];
        const GuiRect box = EMBED_CALL(search_option_rect, SearchCtx, .pane = args, .which = option->which);
        const int on = EMBED_CALL(cli_search_pane.on, CliSearchPaneCfg, .which = option->which);
        const int pointed = args->pane_read->action == option->which;
        text_right = (box.x < text_right) ? box.x : text_right;
        if (on || pointed)
        {
            EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = args, .rect = box,
                       .radius = GUI_SEARCH_OPTION_RADIUS, .color = on ? palette->signal : palette->line,
                       .inside = pointed ? palette->line : palette->editor_background);
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = option->label, .cap = 8ULL});
        const float width =
            EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE_BOLD,
                       .size = GUI_SEARCH_OPTION_SIZE, .text = option->label, .length = length);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1,
                         .face = GUI_FACE_CODE_BOLD, .size = GUI_SEARCH_OPTION_SIZE,
                         .x_f32 = (float)box.x + (((float)box.width - width) / 2.0f), .text = option->label,
                         .length = length, .color = on ? palette->signal : palette->text_2, .clip = box);
    }
    unsigned long long query_length = 0ULL;
    const char *const query = EMBED_CALL(cli_search_pane.query, CliSearchPaneCfg, .length_out = &query_length);
    EMBED_CALL(gui_pane_field.field_draw, GuiPaneFieldCfg, .pane = args, .rect = field, .right = text_right,
               .text = query, .length = query_length, .placeholder = SEARCH_PLACEHOLDER, .keys = args->pane_read->keys);
    const GuiRect set = EMBED_CALL(search_set_rect, SearchCtx, .pane = args);
    if (set.height != 0)
    {
        unsigned int count = 0u;
        unsigned long long name_length = 0ULL;
        const char *const name =
            EMBED_CALL(cli_search_pane.set, CliSearchPaneCfg, .count_out = &count, .length_out = &name_length);
        EMBED_CALL(gui_pane_field.choice_draw, GuiPaneFieldCfg, .pane = args, .rect = set, .text = name,
                   .length = name_length, .pointed = args->pane_read->action == CLI_SEARCH_SET);
    }
    unsigned long long reported = 0ULL;
    const char *const report = EMBED_CALL(cli_search_pane.report, CliSearchPaneCfg, .length_out = &reported);
    if (reported != 0ULL)
    {
        const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
        const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = 1);
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_HEAD,
                         .size = GUI_SEARCH_REPORT_SIZE, .x_f32 = (float)(args->rect.x + pad + one), .text = report,
                         .length = reported, .color = palette->text_3,
                         .clip = EMBED_CALL(search_report_rect, SearchCtx, .pane = args));
    }
}

// the action at x, y: a choice where the pixel stands in one, else the field where it stands in it, else the set
static unsigned int search_head_hit(const GuiPaneCfg *args)
{
    for (unsigned int at = 0u; at < sizeof(s_options) / sizeof(s_options[0]); at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(search_option_rect, SearchCtx, .pane = args, .which = s_options[at].which),
                       .x = args->x, .y = args->y))
        {
            return s_options[at].which;
        }
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(search_field_rect, SearchCtx, .pane = args), .x = args->x, .y = args->y))
    {
        return CLI_SEARCH_FIELD;
    }
    const GuiRect set = EMBED_CALL(search_set_rect, SearchCtx, .pane = args);
    if ((set.height != 0) &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = set, .x = args->x, .y = args->y))
    {
        return CLI_SEARCH_SET;
    }
    return 0u;
}

// an action taken: the field takes the keys; a choice flips, or the set moves on, and the tree is searched again
static void search_action(const GuiPaneCfg *args)
{
    if (args->which == CLI_SEARCH_FIELD)
    {
        if (args->pane != NULL)
        {
            args->pane->keys = 1;
        }
        return;
    }
    EMBED_CALL(cli_search_pane.option, CliSearchPaneCfg, .which = args->which, .now_ms = cli_clock_milliseconds());
}

// text typed goes into the query
static int search_text(const GuiPaneCfg *args)
{
    EMBED_CALL(cli_search_pane.type, CliSearchPaneCfg, .text = args->text, .size = args->length,
               .now_ms = cli_clock_milliseconds());
    return 1;
}

// Escape lets the keys go; Backspace and Enter go to the query
static int search_key(const GuiPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        args->pane->keys = 0;
        return 1;
    }
    return EMBED_CALL(cli_search_pane.key, CliSearchPaneCfg, .name = args->name, .now_ms = cli_clock_milliseconds());
}

// a row drawn: a file's arrow, mark, name, folder and its count in a pill at the right; a line's guide and its text
// from a little before the match, the match in signal
static void search_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    const GuiFrame *const frame = args->frame;
    const GuiPalette *const palette = args->palette;
    const GuiRect rect = args->rect;
    CliSearchRow row;
    if (!EMBED_CALL(cli_search_pane.row, CliSearchPaneCfg, .at = args->row, .row = &row))
    {
        return;
    }
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_PAD_LEFT);
    // the rows are fewer than an int counts
    const int pointed = args->pane_read->pointed == (int)args->row;
    if (row.kind == CLI_SEARCH_ROW_FILE)
    {
        int x = rect.x + pad;
        const int twisty = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_EXPLORER_TWISTY);
        const int arrow = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_PANE_ARROW);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster, .x = x + ((twisty - arrow) / 2),
                   .y = rect.y + ((rect.height - arrow) / 2), .side = arrow, .open = row.open,
                   .color = GUI_FRAME_FADED(palette->text_2, frame->side_share), .clip = args->clip);
        x += twisty;
        // the count in a pill at the right
        char digits[16];
        VerbaLinea count = {digits, sizeof(digits), 0ULL};
        mmgr_line_decimal(&count, row.count, 1u);
        const int count_pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = GUI_SEARCH_COUNT_PAD);
        const float width = EMBED_CALL(gui_pane_part.text_width, GuiPanePartCfg, .pane = args, .face = GUI_FACE_CODE,
                                       .size = GUI_SEARCH_OPTION_SIZE, .text = digits, .length = count.at);
        const int pill_width = (int)(width + 0.5f) + (2 * count_pad);
        const int pill_height =
            rect.height - (2 * EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args, .x = 4));
        const GuiRect pill = {rect.x + rect.width - pad - pill_width, rect.y + ((rect.height - pill_height) / 2),
                              pill_width, pill_height};
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = pill, .other = args->clip),
                   .radius = (unsigned int)(pill_height / 2),
                   .color = GUI_FRAME_FADED(pointed ? palette->hover : palette->line, frame->side_share));
        (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                         .size = GUI_SEARCH_OPTION_SIZE, .x_f32 = (float)(pill.x + count_pad), .text = digits,
                         .length = count.at, .color = palette->text_2, .clip = rect);
        const GuiRect text_clip =
            EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                       .one = (GuiRect){rect.x, rect.y, pill.x - pad - rect.x, rect.height}, .other = args->clip);
        (void)EMBED_CALL(gui_explorer_file.file_draw, GuiExplorerFileCfg, .pane = args, .x = x, .name = row.name,
                         .length = row.name_length, .folder = row.path, .folder_length = row.folder_length,
                         .color = pointed ? palette->text : palette->text_2, .clip = text_clip);
        return;
    }
    const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = raster, .palette = palette, .frame = frame,
                             .rect = rect, .clip = args->clip, .depth = 1u);
    // the line from a little before the match where the match stands past the lead, its blanks before the lead
    // passed over, the match in signal, and what follows the match
    unsigned long long from = (row.match_at > GUI_SEARCH_LEAD_PAST) ? row.match_at - GUI_SEARCH_LEAD : 0ULL;
    while ((from < row.match_at) && ((row.text[from] == ' ') || (row.text[from] == '\t')))
    {
        from += 1ULL;
    }
    const unsigned long long match_end =
        (row.match_at + row.match_length < row.text_length) ? row.match_at + row.match_length : row.text_length;
    const GuiColor text_color = pointed ? palette->text : palette->text_2;
    float at = (float)x;
    if (from > 0ULL)
    {
        at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                        .size = GUI_SEARCH_HIT_SIZE, .x_f32 = at, .text = "\xE2\x80\xA6", .length = 3ULL,
                        .color = palette->text_3, .clip = rect);
    }
    at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                    .size = GUI_SEARCH_HIT_SIZE, .x_f32 = at, .text = row.text + from,
                    .length = (row.match_at > from) ? row.match_at - from : 0ULL, .color = text_color, .clip = rect);
    at = EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE_BOLD,
                    .size = GUI_SEARCH_HIT_SIZE, .x_f32 = at, .text = row.text + row.match_at,
                    .length = match_end - row.match_at, .color = palette->signal, .clip = rect);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = args, .clipped = 1, .face = GUI_FACE_CODE,
                     .size = GUI_SEARCH_HIT_SIZE, .x_f32 = at, .text = row.text + match_end,
                     .length = row.text_length - match_end, .color = text_color, .clip = rect);
}

// a file's row pressed closes or opens its lines; a line's opens the file at the match
static int search_press(const GuiPaneCfg *args)
{
    CliSearchRow row;
    if (!EMBED_CALL(cli_search_pane.row, CliSearchPaneCfg, .at = args->row, .row = &row))
    {
        return 0;
    }
    if (row.kind == CLI_SEARCH_ROW_FILE)
    {
        EMBED_CALL(cli_search_pane.toggle, CliSearchPaneCfg, .at = args->row);
        return 0;
    }
    *args->open_out = (GuiPaneOpen){
        row.path, {(row.line > 0ULL) ? row.line - 1ULL : 0ULL, (row.col > 0ULL) ? row.col - 1ULL : 0ULL}, 1};
    return 1;
}

const GuiPaneTable gui_search_table = {
    .tool = "search",
    .title = search_title,
    .rows = search_rows,
    .head = search_head,
    .head_draw = search_head_draw,
    .head_hit = search_head_hit,
    .action = search_action,
    .row_draw = search_row_draw,
    .press = search_press,
    .text = search_text,
    .key = search_key,
};
