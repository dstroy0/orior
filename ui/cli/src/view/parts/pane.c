// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pane.c: the side pane's arithmetic, drawing and input, each in the frame's words at the frame's scale, the tables
// supplying what differs between the tools
#include "pane.h"

#include "../../command_line/cli_internal.h"
#include "changes.h"
#include "explorer.h"
#include "git.h"
#include "problems.h"
#include "runs.h"
#include "search.h"
#include "structure.h"
#include "tests.h"

// the tables, one a tool that has a pane
static const GuiPaneTable *const s_tables[] = {
    &gui_explorer_table, &gui_search_table, &gui_problems_table, &gui_structure_table,
    &gui_changes_table,  &gui_tests_table,  &gui_git_table,      &gui_runs_table,
};

void gui_pane_init(const GuiPaneCfg *args)
{
    args->pane->pointed = GUI_PANE_NONE;
    args->pane->action = 0u;
    args->pane->scroll = 0u;
    args->pane->keys = 0;
}

const GuiPaneTable *gui_pane_table_of(const GuiPaneCfg *args)
{
    for (unsigned int at = 0u; at < sizeof(s_tables) / sizeof(s_tables[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_tables[at]->tool, .other = args->tool, .cap = 32ULL}))
        {
            return s_tables[at];
        }
    }
    return NULL;
}

typedef struct
{
    const GuiPaneTable *table;
    const GuiFrame *frame;
    int pixels;
} PaneCtx;

// `pixels` of the page at the frame's scale
EMBED_INLINE int pane_scaled(const PaneCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->frame->scale);
}

// the top of the pane's head in the frame's side, under the group's title and the line
EMBED_INLINE int pane_head_top(const PaneCtx *args)
{
    return args->frame->side.y +
           EMBED_CALL(pane_scaled, PaneCtx, .frame = args->frame, .pixels = GUI_PANE_SIDE_HEAD + GUI_PANE_LINE);
}

// the rectangle of what the table stands under the pane's head, empty where it stands nothing
EMBED_INLINE GuiRect pane_head_rect(const GuiPaneCfg *args)
{
    const GuiPaneTable *const table = args->table;
    const GuiFrame *const frame = args->frame;
    const int top =
        EMBED_CALL(pane_head_top, PaneCtx, .frame = frame) +
        EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = (table->title != NULL) ? GUI_PANE_HEAD : 0);
    return (GuiRect){
        frame->side.x, top, frame->side.width - EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 1),
        (table->head != NULL) ? EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = table->head(args)) : 0};
}

int gui_pane_rows_top(const GuiPaneCfg *args)
{
    const GuiRect head = EMBED_CALL(pane_head_rect, GuiPaneCfg, .table = args->table, .frame = args->frame);
    return head.y + head.height;
}

int gui_pane_row_height(const GuiPaneCfg *args)
{
    return EMBED_CALL(pane_scaled, PaneCtx, .frame = args->frame,
                      .pixels = (args->table->row_height != NULL) ? args->table->row_height(args) : GUI_PANE_ROW);
}

GuiRect gui_pane_row_rect(const GuiPaneCfg *args)
{
    const unsigned int scroll = args->pane_read->scroll;
    int y = gui_pane.rows_top(args);
    if (args->table->row_height == NULL)
    {
        // rows of one height: the rows scrolled off are no more than the row, and the rows are far fewer than an int
        // counts
        y += ((int)args->row - (int)scroll) * gui_pane.row_height(args);
    }
    else
    {
        GuiPaneCfg each = *args;
        for (each.row = scroll; each.row < args->row; each.row += 1u)
        {
            y += gui_pane.row_height(&each);
        }
    }
    return (GuiRect){args->frame->side.x, y,
                     args->frame->side.width - EMBED_CALL(pane_scaled, PaneCtx, .frame = args->frame, .pixels = 1),
                     gui_pane.row_height(args)};
}

int gui_pane_hit(const GuiPaneCfg *args)
{
    const int top = gui_pane.rows_top(args);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->frame->side, .x = args->x,
                    .y = args->y) ||
        (args->y < top))
    {
        return 0;
    }
    const unsigned int count = args->table->rows(args);
    if (args->table->row_height == NULL)
    {
        // the pointer stands below the top, and the rows it is down are far fewer than an unsigned int counts
        const unsigned int row = args->pane_read->scroll + (unsigned int)((args->y - top) / gui_pane.row_height(args));
        if (row >= count)
        {
            return 0;
        }
        *args->row_pointer = row;
        return 1;
    }
    int row_top = top;
    GuiPaneCfg each = *args;
    for (each.row = args->pane_read->scroll; each.row < count; each.row += 1u)
    {
        const int pixel_height = gui_pane.row_height(&each);
        if (args->y < row_top + pixel_height)
        {
            *args->row_pointer = each.row;
            return 1;
        }
        row_top += pixel_height;
    }
    return 0;
}

unsigned int gui_pane_head_hit(const GuiPaneCfg *args)
{
    if ((args->table->head == NULL) || (args->table->head_hit == NULL))
    {
        return 0u;
    }
    const GuiRect head = EMBED_CALL(pane_head_rect, GuiPaneCfg, .table = args->table, .frame = args->frame);
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = head, .x = args->x, .y = args->y))
    {
        return 0u;
    }
    GuiPaneCfg hit = *args;
    hit.rect = head;
    return args->table->head_hit(&hit);
}

int gui_pane_move(const GuiPaneCfg *args)
{
    unsigned int row = 0u;
    GuiPaneCfg at = *args;
    at.pane_read = args->pane;
    at.row_pointer = &row;
    // the rows are fewer than an int counts
    const int pointed = gui_pane.hit(&at) ? (int)row : GUI_PANE_NONE;
    const unsigned int action = gui_pane.head_hit(&at);
    const int changed = (pointed != args->pane->pointed) || (action != args->pane->action);
    args->pane->pointed = pointed;
    args->pane->action = action;
    return changed;
}

int gui_pane_wheel(const GuiPaneCfg *args)
{
    const int pixel_height = EMBED_CALL(pane_scaled, PaneCtx, .frame = args->frame, .pixels = GUI_PANE_ROW);
    const int capacity = args->frame->side.y + args->frame->side.height - gui_pane.rows_top(args);
    const int displayed = (pixel_height > 0) ? capacity / pixel_height : 0;
    // the rows are far fewer than an int counts
    const int count = (int)args->table->rows(args);
    const int most = (count > displayed) ? count - displayed : 0;
    const int required = (int)args->pane->scroll + args->rows;
    int clamped;
    if (required < 0)
    {
        clamped = 0;
    }
    else if (required > most)
    {
        clamped = most;
    }
    else
    {
        clamped = required;
    }
    // the scroll clamped is not negative
    if ((unsigned int)clamped == args->pane->scroll)
    {
        return 0;
    }
    args->pane->scroll = (unsigned int)clamped;
    return 1;
}

void gui_pane_arrow_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->side / 2;
    const GuiRect box = {args->x, args->y, args->side, args->side};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = args->clip);
    if ((met.width != box.width) || (met.height != box.height))
    {
        return;
    }
    if (args->open)
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x, .y0 = args->y + 1,
                   .x1 = args->x + half, .y1 = args->y + half + 1, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half,
                   .y0 = args->y + half + 1, .x1 = args->x + args->side - 1, .y1 = args->y + 1, .color = color);
    }
    else
    {
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + 1, .y0 = args->y,
                   .x1 = args->x + half + 1, .y1 = args->y + half, .color = color);
        EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = args->x + half + 1,
                   .y0 = args->y + half, .x1 = args->x + 1, .y1 = args->y + args->side - 1, .color = color);
    }
}

int gui_pane_guides(const GuiPaneCfg *args)
{
    const GuiFrame *const frame = args->frame;
    const int one = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 1);
    const int guide = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = GUI_PANE_GUIDE);
    const int in = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 7);
    int x = args->rect.x + EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = GUI_PANE_PAD_LEFT);
    for (unsigned int level = 0u; level < args->depth; level += 1u)
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect =
                       EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){x + in, args->rect.y, one, args->rect.height}, .other = args->clip),
                   .color = GUI_FRAME_FADED(args->palette->line, frame->side_share));
        x += guide;
    }
    return x;
}

void gui_pane_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiPaneTable *const table = args->table;
    const GuiRect side = frame->side;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = side))
    {
        return;
    }
    const int one = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 1);
    const GuiColor line_color = GUI_FRAME_FADED(args->palette->line, frame->side_share);
    // the side no further than the work's inside, as it slides out of it: its edge, and the group's title at its head
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x + side.width - one, side.y, one, side.height},
                                  .other = frame->inside),
               .color = line_color);
    const GuiRect inside =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, side.y, side.width - one, side.height}, .other = frame->inside);
    const int side_head = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = GUI_PANE_SIDE_HEAD);
    const unsigned int side_head_size =
        (unsigned int)EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = (int)GUI_PANE_SIDE_HEAD_SIZE);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = side_head_size,
                     .x_f32 = (float)(side.x + EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 15)),
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = side_head_size, .top = side.y,
                                     .pixel_height = side_head),
                     .text = args->title, .length = cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = 64ULL}),
                     .color = GUI_FRAME_FADED(args->palette->text_2, frame->side_share), .clip = inside);
    if (table == NULL)
    {
        return;
    }
    // the pane's line, its head with the arrow open and the table's title where it has one, what the table stands under
    // the head, and the body on the editor's ground
    const int line_y = side.y + side_head;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){side.x, line_y, inside.width, one}, .other = inside),
               .color = line_color);
    const char *const title = (table->title != NULL) ? table->title(args) : NULL;
    if (title != NULL)
    {
        const int pane_y = EMBED_CALL(pane_head_top, PaneCtx, .frame = frame);
        const int pane_height = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = GUI_PANE_HEAD);
        const int arrow = EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = GUI_PANE_ARROW);
        const unsigned int head_size =
            (unsigned int)EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = (int)GUI_PANE_HEAD_SIZE);
        const GuiColor head_color = GUI_FRAME_FADED(args->palette->text, frame->side_share);
        EMBED_CALL(gui_pane.arrow_draw, GuiPaneCfg, .raster = raster,
                   .x = side.x + EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 7),
                   .y = pane_y + ((pane_height - arrow) / 2), .side = arrow, .open = 1, .color = head_color,
                   .clip = inside);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = head_size,
                         .x_f32 = (float)(side.x + EMBED_CALL(pane_scaled, PaneCtx, .frame = frame, .pixels = 19)),
                         .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                         .face = GUI_FACE_HEAD, .size = head_size, .top = pane_y,
                                         .pixel_height = pane_height),
                         .text = title, .length = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = 64ULL}),
                         .color = head_color, .clip = inside);
    }
    const GuiRect head = EMBED_CALL(pane_head_rect, GuiPaneCfg, .table = table, .frame = frame);
    if ((head.height > 0) && (table->head_draw != NULL))
    {
        GuiPaneCfg under = *args;
        under.rect = head;
        under.clip = inside;
        table->head_draw(&under);
    }
    const int rows_top = head.y + head.height;
    const GuiRect body =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){side.x, rows_top, inside.width, side.y + side.height - rows_top}, .other = inside);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = body,
               .color = GUI_FRAME_FADED(args->palette->editor_background, frame->side_share));
    // the rows from the one scrolled to, each on its ground where pointed or current, drawn by the table
    const unsigned int count = table->rows(args);
    const int current = (table->current != NULL) ? table->current(args) : GUI_PANE_NONE;
    GuiPaneCfg each = *args;
    for (each.row = args->pane_read->scroll; each.row < count; each.row += 1u)
    {
        each.rect = gui_pane.row_rect(&each);
        if (each.rect.y >= body.y + body.height)
        {
            break;
        }
        each.clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = each.rect, .other = body);
        // the rows are fewer than an int counts
        if (args->pane_read->pointed == (int)each.row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = each.clip,
                       .color = line_color);
        }
        else if (current == (int)each.row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = each.clip,
                       .color = GUI_FRAME_FADED(args->palette->selection_idle, frame->side_share));
        }
        table->row_draw(&each);
    }
}

int gui_pane_press(const GuiPaneCfg *args)
{
    return (args->table->press != NULL) && args->table->press(args);
}

void gui_pane_action(const GuiPaneCfg *args)
{
    if (args->table->action != NULL)
    {
        args->table->action(args);
    }
}

void gui_pane_read(const GuiPaneCfg *args)
{
    if (args->table->read != NULL)
    {
        args->table->read(args);
    }
}

int gui_pane_text(const GuiPaneCfg *args)
{
    return (args->table != NULL) && (args->table->text != NULL) && args->pane->keys && args->table->text(args);
}

int gui_pane_part_scaled(const GuiPanePartCfg *args)
{
    return EMBED_CALL(pane_scaled, PaneCtx, .frame = args->pane->frame, .pixels = args->x);
}

float gui_pane_part_text_width(const GuiPanePartCfg *args)
{
    return EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->pane->glyphs,
                      .face = args->face,
                      .size = (unsigned int)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = args->pane,
                                                       .x = (int)args->size),
                      .text = args->text, .length = args->length);
}

float gui_pane_part_text_draw(const GuiPanePartCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = (int)args->size);
    const GuiRect band = args->clipped ? args->clip : pane->rect;
    return EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = pane->raster,
                      .glyphs = pane->glyphs, .face = args->face, .size = size, .x_f32 = args->x_f32,
                      .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = pane->glyphs,
                                      .face = args->face, .size = size, .top = band.y, .pixel_height = band.height),
                      .text = args->text, .length = args->length,
                      .color = GUI_FRAME_FADED(args->color, pane->frame->side_share),
                      .clip = args->clipped ? EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                                         .one = args->clip, .other = pane->clip)
                                            : pane->clip);
}

void gui_pane_box_draw(const GuiPaneFieldCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiRect rect = args->rect;
    const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = 1);
    const unsigned int radius =
        (unsigned int)EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = args->radius);
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = pane->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = pane->clip),
               .radius = radius, .color = GUI_FRAME_FADED(args->color, pane->frame->side_share));
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = pane->raster,
               .rect = EMBED_CALL(
                   gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){rect.x + one, rect.y + one, rect.width - (2 * one), rect.height - (2 * one)},
                   .other = pane->clip),
               .radius = (radius > (unsigned int)one) ? radius - (unsigned int)one : 0u,
               .color = GUI_FRAME_FADED(args->inside, pane->frame->side_share));
}

GuiRect gui_pane_field_rect(const GuiPaneFieldCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const int box_pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX_PAD);
    return (GuiRect){pane->rect.x + pad, args->y + box_pad, pane->rect.width - (2 * pad),
                     EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX) - (2 * box_pad)};
}

void gui_pane_field_draw(const GuiPaneFieldCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const GuiRect field = args->rect;
    const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = 1);
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = pane, .rect = field, .radius = GUI_PANE_RADIUS,
               .color = args->keys ? palette->signal : palette->line, .inside = palette->editor_background);
    const int field_pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_FIELD_PAD);
    const GuiRect text_clip = {field.x + one, field.y, args->right - field.x - one, field.height};
    const int empty = args->length == 0ULL;
    const float after = EMBED_CALL(
        gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_CODE,
        .size = GUI_PANE_FIELD_SIZE, .x_f32 = (float)(field.x + field_pad),
        .text = empty ? args->placeholder : args->text,
        .length = empty ? cellul.len(&(CatenaFinitaCfg){.src = args->placeholder, .cap = 64ULL}) : args->length,
        .color = empty ? palette->text_3 : palette->text, .clip = text_clip);
    if (!args->keys)
    {
        return;
    }
    // the caret stands after the text, at the field's start while the text is empty
    const int caret_pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_BOX_PAD);
    const int caret_x = empty ? field.x + field_pad : (int)(after + 0.5f);
    EMBED_CALL(
        gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = pane->raster,
        .rect = EMBED_CALL(
            gui_raster_one.rect_intersect, GuiRasterOneCfg,
            .one = (GuiRect){caret_x, field.y + caret_pad,
                             EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_CARET),
                             field.height - (2 * caret_pad)},
            .other = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = text_clip, .other = pane->clip)),
        .color = GUI_FRAME_FADED(palette->caret, pane->frame->side_share));
}

void gui_pane_choice_draw(const GuiPaneFieldCfg *args)
{
    const GuiPaneCfg *const pane = args->pane;
    const GuiPalette *const palette = pane->palette;
    const GuiRect row = args->rect;
    const int pad = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = GUI_PANE_PAD_LEFT);
    const int one = EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane, .x = 1);
    const GuiRect box = {row.x + pad, row.y + one, row.width - (2 * pad), row.height - (2 * one)};
    EMBED_CALL(gui_pane_field.box_draw, GuiPaneFieldCfg, .pane = pane, .rect = box, .radius = GUI_PANE_RADIUS,
               .color = args->pointed ? palette->hover : palette->line, .inside = palette->editor_background);
    (void)EMBED_CALL(gui_pane_part.text_draw, GuiPanePartCfg, .pane = pane, .clipped = 1, .face = GUI_FACE_HEAD,
                     .size = GUI_PANE_CHOICE_SIZE,
                     .x_f32 = (float)(box.x + EMBED_CALL(gui_pane_part.scaled, GuiPanePartCfg, .pane = pane,
                                                         .x = GUI_PANE_FIELD_PAD)),
                     .text = args->text, .length = args->length, .color = palette->text_2, .clip = box);
}

int gui_pane_key(const GuiPaneCfg *args)
{
    return (args->table != NULL) && (args->table->key != NULL) && args->pane->keys && args->table->key(args);
}

int gui_pane_menu(const GuiPaneCfg *args)
{
    return (args->table != NULL) && (args->table->menu != NULL) && args->table->menu(args);
}
