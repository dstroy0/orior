// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.c: the Outline pane. Each declaration is a row as the symbols' outline stands; the pane's scroll and the
// row the pointer marks are the window's own
#include "structure.h"

#include "../../command_line/cli_internal.h"
#include "../../text/text.h"
#include "pane.h"

// A kind's mark: the kind, the mark drawn for it and which of the palette's mark colors it takes
typedef struct
{
    const char *kind;
    const char *glyph;
    unsigned int color;
} StructureMark;

// the marks, as the page draws them; a kind not among them takes the last
static const StructureMark s_structure_marks[] = {
    {"function", "\xc6\x92", 4u}, {"method", "\xc6\x92", 4u},       {"class", "\xe2\x97\x87", 2u},
    {"module", "{}", 3u},         {"constant", "\xe2\x96\xaa", 3u}, {"field", "\xe2\x96\xaa", 5u},
    {"heading", "#", 3u},         {"label", "\xe2\x80\xba", 5u},    {"region", "\xe2\x96\xa4", 1u},
    {"", "\xe2\x96\xaa", 6u},
};

void gui_structure_init(const GuiStructureCfg *args)
{
    args->structure_out->pointed = GUI_PANE_NONE;
    args->structure_out->scroll = 0u;
}

typedef struct
{
    const char *kind;
} StructureCtx;

unsigned int gui_structure_rows(const GuiStructureRowFrameCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
}

int gui_structure_hit(const GuiStructureRowFrameCfg *args)
{
    return EMBED_CALL(gui_pane.hit, GuiPaneCfg, .frame = args->frame, .x = args->x, .y = args->y,
                      .scroll = args->structure->scroll, .count = gui_structure_row_frame.rows(args),
                      .row_pointer = args->row_pointer);
}

int gui_structure_current(const GuiStructureRowFrameCfg *args)
{
    int current = GUI_PANE_NONE;
    CliOutlineRow row;
    for (unsigned int at = 0u; EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = at, .row = &row); at += 1u)
    {
        if (row.line > args->cursor_line)
        {
            break;
        }
        // the rows are fewer than an int counts
        current = (int)at;
    }
    return current;
}

int gui_structure_move(const GuiStructureFrameCfg *args)
{
    unsigned int row = 0u;
    // the rows are fewer than an int counts
    const int pointed =
        EMBED_CALL(gui_structure_row_frame.hit, GuiStructureRowFrameCfg, .structure = args->structure_out,
                   .frame = args->frame, .x = args->x, .y = args->y, .row_pointer = &row)
            ? (int)row
            : GUI_PANE_NONE;
    const int changed = pointed != args->structure_out->pointed;
    args->structure_out->pointed = pointed;
    return changed;
}

int gui_structure_wheel(const GuiStructureRowFrameCfg *args)
{
    return EMBED_CALL(gui_pane.scrolled, GuiPaneCfg, .frame = args->frame,
                      .scroll_pointer = &args->structure_out->scroll, .rows = args->rows,
                      .count = gui_structure_row_frame.rows(args));
}

// the mark of `kind`
EMBED_INLINE const StructureMark *structure_mark(const StructureCtx *args)
{
    const unsigned int count = sizeof(s_structure_marks) / sizeof(s_structure_marks[0]);
    for (unsigned int at = 0u; at + 1u < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_structure_marks[at].kind, .other = args->kind, .cap = 32ULL}))
        {
            return &s_structure_marks[at];
        }
    }
    return &s_structure_marks[count - 1u];
}

void gui_structure_draw(const GuiStructureRowFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const GuiRect body = EMBED_CALL(gui_pane.body, GuiPaneCfg, .raster = raster, .glyphs = glyphs,
                                    .palette = args->palette, .frame = args->frame, .title = "Outline");
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = body))
    {
        return;
    }
    const unsigned int row_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = (int)GUI_PANE_ROW_SIZE, .scale = scale);
    const unsigned int mark_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_STRUCTURE_MARK_SIZE, .scale = scale);
    const int mark_box =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRUCTURE_MARK, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_STRUCTURE_GAP, .scale = scale);
    const int current = gui_structure_row_frame.current(args);
    const unsigned int rows = gui_structure_row_frame.rows(args);
    for (unsigned int row = args->structure->scroll; row < rows; row += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .frame = args->frame, .row = row,
                                        .scroll = args->structure->scroll);
        if (rect.y >= body.y + body.height)
        {
            break;
        }
        CliOutlineRow read;
        if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = row, .row = &read))
        {
            break;
        }
        const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = body);
        // the rows are fewer than an int counts
        const int pointed = args->structure->pointed == (int)row;
        if (pointed)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = on,
                       .color = GUI_FRAME_FADED(args->palette->line, args->frame->side_share));
        }
        else if (current == (int)row)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = on,
                       .color = GUI_FRAME_FADED(args->palette->selection_idle, args->frame->side_share));
        }
        // a guide down each level the row stands under, then the kind's mark centered in its box, then the name
        const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = raster, .palette = args->palette,
                                 .frame = args->frame, .rect = rect, .clip = on, .depth = read.depth);
        const StructureMark *const mark = EMBED_CALL(structure_mark, StructureCtx, .kind = read.kind);
        const unsigned long long glyph_length = cellul.len(&(CatenaFinitaCfg){.src = mark->glyph, .cap = 8ULL});
        const float glyph_width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                       .size = mark_size, .text = mark->glyph, .length = glyph_length);
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_HEAD, .size = mark_size, .x_f32 = (float)x + (((float)mark_box - glyph_width) / 2.0f),
            .y = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = mark_size,
                            .top = rect.y, .pixel_height = rect.height),
            .text = mark->glyph, .length = glyph_length,
            .color = GUI_FRAME_FADED(args->palette->mark[mark->color], args->frame->side_share), .clip = on);
        (void)EMBED_CALL(
            gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
            .face = GUI_FACE_HEAD, .size = row_size, .x_f32 = (float)(x + mark_box + gap),
            .y = EMBED_CALL(gui_pane.baseline, GuiPaneCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = row_size,
                            .top = rect.y, .pixel_height = rect.height),
            .text = read.name, .length = read.length,
            .color = GUI_FRAME_FADED((pointed || (current == (int)row)) ? args->palette->text : args->palette->text_2,
                                     args->frame->side_share),
            .clip = on);
    }
}
