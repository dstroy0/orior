// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.c: the Outline pane's rows as the symbols' outline is
#include "structure.h"

#include "../../cli/src/code/symbols.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/editor/view.h"
#include "../../cli/src/text/text.h"

// A type's mark: the type, the mark drawn for it and which of the palette's mark colors it takes
typedef struct
{
    const char *type;
    const char *glyph;
    unsigned int color;
} StructureMark;

// the marks, as the page draws them; a type not among them takes the last
static const StructureMark s_structure_marks[] = {
    {"function", "\xc6\x92", 4u}, {"method", "\xc6\x92", 4u},       {"class", "\xe2\x97\x87", 2u},
    {"module", "{}", 3u},         {"constant", "\xe2\x96\xaa", 3u}, {"field", "\xe2\x96\xaa", 5u},
    {"heading", "#", 3u},         {"label", "\xe2\x80\xba", 5u},    {"region", "\xe2\x96\xa4", 1u},
    {"", "\xe2\x96\xaa", 6u},
};

typedef struct
{
    const char *type;
} StructureCtx;

static const char *structure_title(const GuiPaneCfg *args)
{
    (void)args;
    return "Outline";
}

static unsigned int structure_rows(const GuiPaneCfg *args)
{
    (void)args;
    return EMBED_CALL(cli_outline.rows, CliOutlineCfg, .at = 0u);
}

// the row of the last declaration on or above the cursor's line
static int structure_current(const GuiPaneCfg *args)
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

// the mark of `type`
EMBED_INLINE const StructureMark *structure_mark(const StructureCtx *args)
{
    const unsigned int count = sizeof(s_structure_marks) / sizeof(s_structure_marks[0]);
    for (unsigned int at = 0u; at + 1u < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_structure_marks[at].type, .other = args->type, .cap = 32ULL}))
        {
            return &s_structure_marks[at];
        }
    }
    return &s_structure_marks[count - 1u];
}

// a guide down each level the row is under, then the type's mark centered in its box, then the name
static void structure_row_draw(const GuiPaneCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiFrame *const frame = args->frame;
    const GuiRect rect = args->rect;
    const GuiRect on = args->clip;
    const int scale = frame->scale;
    CliOutlineRow read;
    if (!EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = args->row, .row = &read))
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
    // the rows are fewer than an int counts
    const int lit = (args->pane_read->pointed == (int)args->row) || (structure_current(args) == (int)args->row);
    const int x = EMBED_CALL(gui_pane.guides, GuiPaneCfg, .raster = raster, .palette = args->palette, .frame = frame,
                             .rect = rect, .clip = on, .depth = read.depth);
    const StructureMark *const mark = EMBED_CALL(structure_mark, StructureCtx, .type = read.type);
    const unsigned long long glyph_length = cellul.len(&(CatenaFinitaCfg){.src = mark->glyph, .cap = 8ULL});
    const float glyph_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                   .size = mark_size, .text = mark->glyph, .length = glyph_length);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs, .face = GUI_FACE_HEAD,
        .size = mark_size, .x_f32 = (float)x + (((float)mark_box - glyph_width) / 2.0f),
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                        .size = mark_size, .top = rect.y, .pixel_height = rect.height),
        .text = mark->glyph, .length = glyph_length,
        .color = GUI_FRAME_FADED(args->palette->mark[mark->color], frame->side_share), .clip = on);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs, .face = GUI_FACE_HEAD,
        .size = row_size, .x_f32 = (float)(x + mark_box + gap),
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = GUI_FACE_HEAD,
                        .size = row_size, .top = rect.y, .pixel_height = rect.height),
        .text = read.name, .length = read.length,
        .color = GUI_FRAME_FADED(lit ? args->palette->text : args->palette->text_2, frame->side_share), .clip = on);
}

// the row's declaration: the file displayed opened with the cursor on its line
static int structure_press(const GuiPaneCfg *args)
{
    CliOutlineRow row;
    const int displayed = cli_view_displayed();
    if ((displayed < 0) || !EMBED_CALL(cli_outline.row, CliOutlineCfg, .at = args->row, .row = &row))
    {
        return 0;
    }
    // the view displayed is among the views; its path is copied into the pane's own buffer, which the open reads
    static char s_displayed_path[CLI_PATH_MAX_LEN];
    if (!cli_view_path_at((unsigned int)displayed, s_displayed_path))
    {
        return 0;
    }
    *args->open_out = (GuiPaneOpen){s_displayed_path, {row.line, 0ULL}, 1};
    return 1;
}

// the outline read anew of the view displayed as the pane shows and as the view is displayed or edited
static void structure_read(const GuiPaneCfg *args)
{
    if ((args->event == GUI_PANE_DISPLAYED) || (args->event == GUI_PANE_VIEW))
    {
        EMBED_CALL(cli_outline.read, CliOutlineCfg, .key = args->key, .path = args->path);
    }
}

const GuiPaneTable gui_structure_table = {
    .tool = "structure",
    .title = structure_title,
    .rows = structure_rows,
    .row_draw = structure_row_draw,
    .current = structure_current,
    .press = structure_press,
    .read = structure_read,
};
