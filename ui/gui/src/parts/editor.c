// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editor.c: the editor part. It draws the view's frame as the controller published it, reading its rows, runs, folds
// and matches where they stand in the frame's cell; a run's column counts the units the view counts, so a letter past
// the basic plane counts two
#include "editor.h"

#include "../../../cli/src/text/text.h"
#include "../frame.h"

#include <math.h>

void gui_editor_init(const GuiEditorCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->editor, .val = 0, .bytes = sizeof(*args->editor)});
    args->editor->caret_color = 255u;
}

typedef struct
{
    unsigned long long lines_u64;
    const GuiEditor *editor;
    const CliViewFrameRow *line_line_pointer;
    unsigned long long col;
    const GuiPalette *palette;
    unsigned int text_color;
    GuiRaster *raster;
    int x_i32;
    int middle;
    int pixel_height;
    GuiColor color;
    GuiRect clip;
    const CliViewFrameFold *fold;
    GuiGlyphs *glyphs;
    float x;
    int top;
    int baseline;
    int line_height;
    float cell;
    unsigned int size;
    int scale;
    int y;
    GuiRect rect;
    int thick;
    GuiRect row_rect;
    float text_x;
    int boxed;
    GuiRect place;
    unsigned long long line;
    unsigned int at;
    const CliViewFrameRow *stored_line;
} EditorCtx;

// how many digits the file's line count takes, three at the least, as the page gives its gutter capacity
EMBED_INLINE unsigned int editor_digits(const EditorCtx *args)
{
    unsigned int digits = 1u;
    for (unsigned long long left = args->lines_u64; left >= 10ull; left /= 10ull)
    {
        digits += 1u;
    }
    return (digits < 3u) ? 3u : digits;
}

int gui_editor_gutter(const GuiEditorPositionCellCfg *args)
{
    int scale = args->scale;
    const float numbers =
        (float)EMBED_CALL(editor_digits, EditorCtx,
                          .lines_u64 = (args->editor_read->frame != NULL) ? args->editor_read->frame->lines : 0ull) *
        args->cell;
    return (int)(numbers + 0.5f) + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                              .pixels = GUI_GUTTER_BEFORE + GUI_GUTTER_AFTER, .scale = scale);
}

// the cell column a line's unit column stands at, tabs laid to their stops, a letter past the basic plane counting
// two units; and the line's cells in all where `col` is past its end
EMBED_INLINE unsigned long long editor_cell_get(const EditorCtx *args)
{
    const char *const text = args->editor->frame->bytes + args->line_line_pointer->text;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long cell = 0ull;
    while ((at < args->line_line_pointer->length) && (units < args->col))
    {
        unsigned long long consumed = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                               .size = args->line_line_pointer->length, .at = at, .length = &consumed);
        at += (consumed != 0ull) ? consumed : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        cell += (letter == '\t') ? (GUI_TAB_COLUMNS - (cell % GUI_TAB_COLUMNS)) : 1ull;
    }
    return cell;
}

int gui_editor_cell_at(const GuiEditorPositionCellScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *row = args->row;
    unsigned long long *column = args->column;
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const int text_x = args->place.x + EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                                  .editor_read = args->editor_read, .cell = args->cell, .scale = scale);
    if ((line_height <= 0) || (args->cell <= 0.0f) || (args->y < args->place.y) ||
        (args->y >= args->place.y + args->place.height) || (args->x < text_x) ||
        (args->x >= args->place.x + args->place.width))
    {
        return 0;
    }
    // the row and the column are not negative, the pixel standing in the text
    *row = (unsigned long long)((args->y - args->place.y) / line_height);
    *column = (unsigned long long)(((float)(args->x - text_x) / args->cell) + 0.5f);
    return 1;
}

int gui_editor_row_at(const GuiEditorPositionCellScaleCfg *args)
{
    int scale = args->scale;
    unsigned long long *row = args->row;
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const int text_x = args->place.x + EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                                  .editor_read = args->editor_read, .cell = args->cell, .scale = scale);
    if ((line_height <= 0) || (args->y < args->place.y) || (args->y >= args->place.y + args->place.height) ||
        (args->x < args->place.x) || (args->x >= text_x))
    {
        return 0;
    }
    // the row is not negative, the pixel standing in the gutter
    *row = (unsigned long long)((args->y - args->place.y) / line_height);
    return 1;
}

GuiRect gui_editor_caret_rect(const GuiEditorPositionCfg *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    const GuiRect none = {0, 0, 0, 0};
    if ((args->editor_read->frame == NULL) ||
        (args->editor_read->frame->find.open && args->editor_read->frame->find.focus) ||
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->place))
    {
        return none;
    }
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_SIZE, .scale = scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs, .size = size);
    const int gutter = EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                  .editor_read = args->editor_read, .cell = cell, .scale = scale);
    const GuiRect text_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
        .other = (GuiRect){args->place.x + gutter, args->place.y, args->place.width - gutter, args->place.height});
    GuiRect at;
    if (!EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg, .editor_read = args->editor_read,
                    .place = args->place, .cell = cell, .scale = scale, .line = args->editor_read->frame->cursor.line,
                    .col = args->editor_read->frame->cursor.col, .rect = &at))
    {
        return none;
    }
    return EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){at.x, at.y, EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale),
                         at.height},
        .other = text_clip);
}

// the caret's color at `text_color`'s share of its alpha: a byte's share of a byte, rounded, is a byte
EMBED_INLINE GuiColor editor_caret_color(const EditorCtx *args)
{
    return GUI_RGBA(GUI_RED_OF(args->palette->caret), GUI_GREEN_OF(args->palette->caret),
                    GUI_BLUE_OF(args->palette->caret),
                    ((GUI_ALPHA_OF(args->palette->caret) * args->text_color) + 127u) / 255u);
}

GuiRect gui_editor_caret_draw(const GuiEditorRasterCfg *args)
{
    args->editor->caret_saved = 0;
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->caret))
    {
        return args->caret;
    }
    // the rectangle's sides are not negative where it is not empty
    const unsigned int pixel_width = (unsigned int)args->caret.width;
    const unsigned int pixel_height = (unsigned int)args->caret.height;
    if (pixel_width * pixel_height <= GUI_CARET_UNDER_MAX)
    {
        for (unsigned int row = 0u; row < pixel_height; row += 1u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = &args->editor->caret_under[row * pixel_width],
                              .src = &args->raster->pixels[(((unsigned int)args->caret.y + row) * args->raster->width) +
                                                           (unsigned int)args->caret.x],
                              .bytes = pixel_width * sizeof(GuiColor)});
        }
        args->editor->caret_rect = args->caret;
        args->editor->caret_saved = 1;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->caret,
               .color = EMBED_CALL(editor_caret_color, EditorCtx, .palette = args->palette,
                                   .text_color = args->editor->caret_color));
    return args->caret;
}

GuiRect gui_editor_caret_redrawn(const GuiEditorRasterCfg *args)
{
    if (!args->editor->caret_saved)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect caret = args->editor->caret_rect;
    // the rectangle's sides are not negative, the rectangle saved being drawn
    const unsigned int pixel_width = (unsigned int)caret.width;
    const unsigned int pixel_height = (unsigned int)caret.height;
    for (unsigned int row = 0u; row < pixel_height; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &args->raster->pixels[(((unsigned int)caret.y + row) * args->raster->width) + (unsigned int)caret.x],
            .src = &args->editor->caret_under[row * pixel_width],
            .bytes = pixel_width * sizeof(GuiColor)});
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = caret,
               .color = EMBED_CALL(editor_caret_color, EditorCtx, .palette = args->palette,
                                   .text_color = args->editor->caret_color));
    return caret;
}

int gui_editor_position_rect(const GuiEditorPositionCellCfg *args)
{
    int scale = args->scale;
    GuiRect *rect = args->rect;
    const CliViewFrame *const frame = args->editor_read->frame;
    if (frame == NULL)
    {
        return 0;
    }
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const float text_x =
        (float)(args->place.x + EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                           .editor_read = args->editor_read, .cell = args->cell, .scale = scale));
    unsigned int start = 0u;
    while ((start < frame->count) && (frame->row[start].number < frame->top))
    {
        start += 1u;
    }
    for (unsigned int at = start; at < frame->count; at += 1u)
    {
        if (frame->row[at].number == args->line)
        {
            // the row's count is far inside what an int represents
            const int row = (int)(at - start);
            const int y = args->place.y + (row * line_height);
            if (y + line_height > args->place.y + args->place.height)
            {
                return 0;
            }
            const int x =
                (int)roundf(text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor_read,
                                                        .line_line_pointer = &frame->row[at], .col = args->col) *
                                      args->cell));
            *rect = (GuiRect){x, y, (int)(args->cell + 0.5f), line_height};
            return 1;
        }
    }
    return 0;
}

// the fold that opens on `line`, or NULL
EMBED_INLINE const CliViewFrameFold *editor_fold_on(const EditorCtx *args)
{
    const CliViewFrame *const frame = args->editor->frame;
    for (unsigned int at = 0u; at < frame->folds; at += 1u)
    {
        if (frame->fold[at].first == args->line)
        {
            return &frame->fold[at];
        }
    }
    return NULL;
}

// the gutter's mark of a fold closed: a triangle pointing right, `pixel_height` pixels tall, its point at the right,
// drawn with its left side at `x` and its middle on `middle`
EMBED_INLINE void editor_fold_mark(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const int half = args->pixel_height / 2;
    for (int step = 0; step <= half; step += 1)
    {
        const int pixel_width = half - step + 1;
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->x_i32, args->middle - step, pixel_width, 1},
                                      .other = args->clip),
                   .color = color);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){args->x_i32, args->middle + step, pixel_width, 1},
                                      .other = args->clip),
                   .color = color);
    }
}

// the folded mark after a line that opens a fold closed, from `x`: a rounded ground two cells wide holding three
// dots, then the closer's text half a cell after it; how far it reaches
EMBED_INLINE float editor_folded_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    float x = args->x;
    unsigned int size = args->size;
    int scale = args->scale;
    GuiRect clip = args->clip;
    const int pixel_width = (int)roundf(2.0f * args->cell);
    const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
    const GuiRect box = {(int)roundf(x), args->top + inset, pixel_width, args->line_height - (2 * inset)};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = box, .other = clip);
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = met,
                   .radius = (unsigned int)inset, .color = args->palette->same);
    }
    const int dot = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 2, .scale = scale);
    const int middle = args->top + (args->line_height / 2);
    for (int at = 0; at < 3; at += 1)
    {
        const int dot_x = box.x + ((pixel_width * (at + 1)) / 4) - (dot / 2);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                      .one = (GuiRect){dot_x, middle - (dot / 2), dot, dot}, .other = clip),
                   .color = args->palette->text_2);
    }
    float reach = x + (float)pixel_width + (args->cell / 2.0f);
    if (args->fold->displayed && (args->fold->closer_length != 0ull))
    {
        const char *const closer = args->editor->frame->bytes + args->editor->frame->closers + args->fold->closer;
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_CODE, .size = size, .x_f32 = reach, .y = args->baseline, .text = closer,
                   .length = args->fold->closer_length, .color = args->palette->editor_text, .clip = clip);
        reach += EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                            .face = GUI_FACE_CODE, .size = size, .text = closer, .length = args->fold->closer_length);
    }
    return reach;
}

GuiColor gui_editor_color(const GuiEditorPaletteCfg *args)
{
    const GuiPalette *palette = args->palette;
    const CliDocumentNames *const names = args->editor_read->names;
    if ((names == NULL) || (args->name >= names->count))
    {
        return palette->editor_text;
    }
    const char *const text = names->name[args->name];
    return EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette, .name_read = text,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// one line's text drawn from `x` on the baseline `y`, each letter in its run's color, a tab laid out to its stop
EMBED_INLINE void editor_line_draw(const EditorCtx *args)
{
    const GuiEditor *editor = args->editor;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    float x = args->x;
    int y = args->y;
    float cell = args->cell;
    unsigned int size = args->size;
    GuiRect clip = args->clip;
    const CliViewFrame *const frame = editor->frame;
    const char *const text = frame->bytes + args->line_line_pointer->text;
    // a line's runs stand in the frame's runs cell as its columns and then its names, none where that cell could not be
    // met
    const CliDocumentLineRuns none = {0u, 0u};
    const CliDocumentLineRuns runs = (frame->runs != NULL) ? frame->runs->line[args->at] : none;
    const unsigned int *const column_of = (frame->runs != NULL) ? (frame->runs->word + runs.at) : NULL;
    const unsigned int *const name_of = (frame->runs != NULL) ? (frame->runs->word + runs.at + runs.runs) : NULL;
    unsigned long long at = 0ull;
    unsigned long long units = 0ull;
    unsigned long long column = 0ull;
    unsigned int next = 0u;
    GuiColor color = palette->editor_text;
    const float start = x;
    while (at < args->line_line_pointer->length)
    {
        while ((next < runs.runs) && (units >= column_of[next]))
        {
            color = EMBED_CALL(gui_editor_palette.color, GuiEditorPaletteCfg, .editor_read = editor, .palette = palette,
                               .name = name_of[next]);
            next += 1u;
        }
        unsigned long long consumed = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                               .size = args->line_line_pointer->length, .at = at, .length = &consumed);
        at += (consumed != 0ull) ? consumed : 1ull;
        units += (letter >= 0x10000u) ? 2ull : 1ull;
        if (letter == '\t')
        {
            column += GUI_TAB_COLUMNS - (column % GUI_TAB_COLUMNS);
        }
        else
        {
            float advance = 0.0f;
            const GuiGlyphCell *const glyph =
                EMBED_CALL(gui_glyphs_face_letter.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs,
                           .face = GUI_FACE_CODE, .letter = letter, .size = size, .color = color, .advance = &advance);
            if (glyph != NULL)
            {
                const float here = start + ((float)column * cell);
                // a pixel's place, rounded, is far inside what an int represents
                EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = raster, .cell = glyph,
                           .x = (int)roundf(here), .y = y, .clip = clip);
            }
            column += 1ull;
        }
        if (start + ((float)column * cell) > (float)(clip.x + clip.width))
        {
            return;
        }
    }
}

// a box drawn around `rect` on `raster`, its lines `thick` pixels wide inside the rect, in `color`, within `clip`
EMBED_INLINE void editor_box_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiRect rect = args->rect;
    GuiColor color = args->color;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y, rect.width, args->thick}, .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y + rect.height - args->thick, rect.width, args->thick},
                                  .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x, rect.y, args->thick, rect.height}, .other = args->clip),
               .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){rect.x + rect.width - args->thick, rect.y, args->thick, rect.height},
                                  .other = args->clip),
               .color = color);
}

// the matches on the line `line` drawn on the row `row_rect`, each cell they span in the match ground, the current one
// in the stronger ground and boxed where `boxed` is 1
EMBED_INLINE void editor_matches_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    int scale = args->scale;
    for (unsigned long long at = 0ull; at < args->editor->frame->matches; at += 1ull)
    {
        if (args->editor->frame->match[at][0] != args->line)
        {
            continue;
        }
        const int current =
            (args->editor->frame->find.current >= 0ll) && ((unsigned long long)args->editor->frame->find.current == at);
        const int left =
            (int)roundf(args->text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                          .line_line_pointer = args->stored_line,
                                                          .col = args->editor->frame->match[at][1]) *
                                        args->cell));
        const int right =
            (int)roundf(args->text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                          .line_line_pointer = args->stored_line,
                                                          .col = args->editor->frame->match[at][2]) *
                                        args->cell));
        const GuiRect span = {left, args->row_rect.y, right - left, args->row_rect.height};
        if (args->boxed)
        {
            if (current)
            {
                EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = span,
                           .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                           .color = args->palette->signal, .clip = args->clip);
            }
        }
        else
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect =
                           EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = span, .other = args->clip),
                       .color = current ? args->palette->match_on : args->palette->match);
        }
    }
}

// find's bar drawn at the top right of `place`: its field with the query, or "Find" dim where the query is empty, the
// caret where the keys are its own, the toggles for case, whole words and a pattern, each lit where it is on, and the
// count, "No results" where there is none
EMBED_INLINE void editor_find_draw(const EditorCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    unsigned int size = args->size;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_PAD, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int field_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_FIELD, .scale = scale);
    const int toggle_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_TOGGLE, .scale = scale);
    const int count_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_COUNT, .scale = scale);
    const int field_height =
        args->line_height + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale);
    const int pixel_width = pad + field_width + gap + (3 * (toggle_width + gap)) + count_width + pad;
    const GuiRect bar = {
        args->place.x + args->place.width -
            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_FIND_RIGHT, .scale = scale) -
            pixel_width,
        args->place.y, pixel_width, field_height + (2 * pad)};
    const GuiRect met = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = bar, .other = args->place);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = met))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = met,
               .radius =
                   (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
               .color = args->palette->surface);
    EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = bar,
               .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
               .color = args->palette->widget_line, .clip = args->place);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const GuiRect field = {bar.x + pad, bar.y + pad, field_width, field_height};
    const int baseline = field.y + (int)((((float)field_height - ascent - descent) / 2.0f) + ascent + 0.5f);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = field, .other = args->place),
               .color = args->palette->editor_background);
    EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = field,
               .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
               .color = args->editor->frame->find.focus ? args->palette->hover : args->palette->line,
               .clip = args->place);
    const float text_x =
        (float)(field.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale));
    const GuiRect field_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = field, .other = args->place);
    if (args->editor->frame->find.query_length == 0ull)
    {
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size, .x_f32 = text_x, .y = baseline, .text = "Find", .length = 4ull,
                   .color = args->palette->text_3, .clip = field_clip);
    }
    else
    {
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size, .x_f32 = text_x, .y = baseline,
                   .text = args->editor->frame->find.query, .length = args->editor->frame->find.query_length,
                   .color = args->palette->editor_text, .clip = field_clip);
    }
    if (args->editor->frame->find.focus)
    {
        // the caret after as many letters as the cursor's units count, a letter past the basic plane two of them
        unsigned long long bytes = 0ull;
        unsigned long long units = 0ull;
        while ((bytes < args->editor->frame->find.query_length) && (units < args->editor->frame->find.at))
        {
            unsigned long long consumed = 0ull;
            const unsigned int letter =
                EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->editor->frame->find.query,
                           .size = args->editor->frame->find.query_length, .at = bytes, .length = &consumed);
            bytes += (consumed != 0ull) ? consumed : 1ull;
            units += (letter >= 0x10000u) ? 2ull : 1ull;
        }
        const float before =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                       .size = size, .text = args->editor->frame->find.query, .length = bytes);
        EMBED_CALL(
            gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
            .rect = EMBED_CALL(
                gui_raster_one.rect_intersect, GuiRasterOneCfg,
                .one = (GuiRect){(int)roundf(text_x + before),
                                 field.y +
                                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale),
                                 EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                                 field_height - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 6,
                                                           .scale = scale)},
                .other = args->place),
            .color = args->palette->caret);
    }
    static const char *const s_toggle[3] = {"Aa", "ab", ".*"};
    const int on[3] = {args->editor->frame->find.match_case, args->editor->frame->find.word,
                       args->editor->frame->find.regex};
    int x = field.x + field_width + gap;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const GuiRect toggle = {x, field.y, toggle_width, field_height};
        if (on[at])
        {
            EMBED_CALL(
                gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = toggle, .other = args->place),
                .radius =
                    (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
                .color = args->palette->selection);
            EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster, .rect = toggle,
                       .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                       .color = args->palette->hover, .clip = args->place);
        }
        const float label_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                             .face = GUI_FACE_HEAD, .size = size, .text = s_toggle[at], .length = 2ull);
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size,
                   .x_f32 = (float)toggle.x + (((float)toggle_width - label_width) / 2.0f), .y = baseline,
                   .text = s_toggle[at], .length = 2ull,
                   .color = on[at] ? args->palette->editor_text : args->palette->text_2, .clip = args->place);
        x += toggle_width + gap;
    }
    char count_text[64];
    VerbaLinea count = {count_text, sizeof(count_text), 0ull};
    if (args->editor->frame->find.count == 0ull)
    {
        mmgr_line_put(&count, "No results");
    }
    else
    {
        // the current match is counted among the matches
        mmgr_line_decimal(&count,
                          (args->editor->frame->find.current >= 0ll)
                              ? (unsigned long long)args->editor->frame->find.current + 1ull
                              : 0ull,
                          1u);
        mmgr_line_put(&count, " of ");
        mmgr_line_decimal(&count, args->editor->frame->find.count, 1u);
    }
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
               .face = GUI_FACE_HEAD, .size = size,
               .x_f32 = (float)(x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale)),
               .y = baseline, .text = count_text, .length = count.at, .color = args->palette->text_2,
               .clip = args->place);
}

void gui_editor_draw(const GuiEditorPositionScaleCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    int scale = args->scale;
    if ((args->editor->frame == NULL) || EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->place))
    {
        return;
    }
    // the code's size and its line in pixels at the scale, the size a whole pixel
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_SIZE, .scale = scale);
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = glyphs, .size = size);
    const int gutter = EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                  .editor_read = args->editor, .cell = cell, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    // the baseline inside a row: the row's height less the face's, halved, then the ascent down
    const int baseline = (int)((((float)line_height - ascent - descent) / 2.0f) + ascent + 0.5f);
    const int numbers_right =
        args->place.x + gutter -
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_GUTTER_AFTER, .scale = scale);
    const float text_x = (float)(args->place.x + gutter);
    const GuiRect text_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->place,
        .other = (GuiRect){args->place.x + gutter, args->place.y, args->place.width - gutter, args->place.height});
    const int rows = (line_height > 0) ? (args->place.height + line_height - 1) / line_height : 0;
    // the selection's ends in order
    const int anchored = (args->editor->frame->anchor.line < args->editor->frame->cursor.line) ||
                         ((args->editor->frame->anchor.line == args->editor->frame->cursor.line) &&
                          (args->editor->frame->anchor.col < args->editor->frame->cursor.col));
    const CliPosition selected_from = anchored ? args->editor->frame->anchor : args->editor->frame->cursor;
    const CliPosition selected_to = anchored ? args->editor->frame->cursor : args->editor->frame->anchor;
    const int selected = ((selected_from.line != selected_to.line) || (selected_from.col != selected_to.col));
    // the first row drawn is the first line stored at or below the view's top
    unsigned int start = 0u;
    while ((start < args->editor->frame->count) && (args->editor->frame->row[start].number < args->editor->frame->top))
    {
        start += 1u;
    }
    for (int row = 0; row < rows; row += 1)
    {
        // a row's count is not negative
        const unsigned int at = start + (unsigned int)row;
        if ((at >= args->editor->frame->count) || (args->editor->frame->row[at].number >= args->editor->frame->lines))
        {
            break;
        }
        const unsigned long long line = args->editor->frame->row[at].number;
        const int y = args->place.y + (row * line_height) + baseline;
        const GuiRect row_rect = {text_clip.x, args->place.y + (row * line_height), text_clip.width, line_height};
        const CliViewFrameRow *const stored_line = &args->editor->frame->row[at];
        const CliViewFrameFold *const fold =
            EMBED_CALL(editor_fold_on, EditorCtx, .editor = args->editor, .line = line);
        // the current line's ground, the selection's span on the line, and the caret
        if ((line == args->editor->frame->cursor.line) && !selected)
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row_rect,
                                          .other = args->place),
                       .color = args->palette->current);
        }
        if (selected && (line >= selected_from.line) && (line <= selected_to.line))
        {
            const unsigned long long start_cell =
                (line == selected_from.line) ? EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                          .line_line_pointer = stored_line, .col = selected_from.col)
                                             : 0ull;
            const unsigned long long end_cell =
                (line == selected_to.line) ? EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                        .line_line_pointer = stored_line, .col = selected_to.col)
                                           : EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                        .line_line_pointer = stored_line, .col = ~0ull) +
                                                 1ull;
            const int left = (int)roundf(text_x + ((float)start_cell * cell));
            const int right = (int)roundf(text_x + ((float)end_cell * cell));
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){left, row_rect.y, right - left, line_height},
                                          .other = text_clip),
                       .color = args->palette->selection);
        }
        if (args->editor->frame->find.open)
        {
            EMBED_CALL(editor_matches_draw, EditorCtx, .editor = args->editor, .stored_line = stored_line, .line = line,
                       .raster = raster, .palette = args->palette, .row_rect = row_rect, .text_x = text_x, .cell = cell,
                       .boxed = 0, .scale = scale, .clip = text_clip);
        }
        char digits[24];
        VerbaLinea number = {digits, sizeof(digits), 0ull};
        mmgr_line_decimal(&number, line + 1ull, 1u);
        const float number_width = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                              .face = GUI_FACE_CODE, .size = size, .text = digits, .length = number.at);
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)numbers_right - number_width, .y = y,
                   .text = digits, .length = number.at, .color = args->palette->number, .clip = args->place);
        if ((fold != NULL) && fold->closed)
        {
            EMBED_CALL(editor_fold_mark, EditorCtx, .raster = raster,
                       .x_i32 = numbers_right +
                                EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4, .scale = scale),
                       .middle = row_rect.y + (line_height / 2),
                       .pixel_height =
                           EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 7, .scale = scale),
                       .color = args->palette->number, .clip = args->place);
        }
        EMBED_CALL(editor_line_draw, EditorCtx, .editor = args->editor, .line_line_pointer = stored_line, .at = at,
                   .raster = raster, .glyphs = glyphs, .palette = args->palette, .x = text_x, .y = y, .cell = cell,
                   .size = size, .clip = text_clip);
        if ((fold != NULL) && fold->closed)
        {
            const float after = text_x + ((float)(EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                             .line_line_pointer = stored_line, .col = ~0ull) +
                                                  1ull) *
                                          cell);
            (void)EMBED_CALL(editor_folded_draw, EditorCtx, .editor = args->editor, .fold = fold, .raster = raster,
                             .glyphs = glyphs, .palette = args->palette, .x = after, .top = row_rect.y, .baseline = y,
                             .line_height = line_height, .cell = cell, .size = size, .scale = scale, .clip = text_clip);
        }
        for (unsigned int each = 0u; args->editor->frame->has_pair && (each < 2u); each += 1u)
        {
            if (args->editor->frame->pair[each].line == line)
            {
                const int left =
                    (int)roundf(text_x + ((float)EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                            .line_line_pointer = stored_line,
                                                            .col = args->editor->frame->pair[each].col) *
                                          cell));
                const int right =
                    (int)roundf(text_x + ((float)(EMBED_CALL(editor_cell_get, EditorCtx, .editor = args->editor,
                                                             .line_line_pointer = stored_line,
                                                             .col = args->editor->frame->pair[each].col) +
                                                  1ull) *
                                          cell));
                EMBED_CALL(editor_box_draw, EditorCtx, .raster = raster,
                           .rect = (GuiRect){left, row_rect.y, right - left, line_height},
                           .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                           .color = args->palette->caret, .clip = text_clip);
            }
        }
        if (args->editor->frame->find.open)
        {
            EMBED_CALL(editor_matches_draw, EditorCtx, .editor = args->editor, .stored_line = stored_line, .line = line,
                       .raster = raster, .palette = args->palette, .row_rect = row_rect, .text_x = text_x, .cell = cell,
                       .boxed = 1, .scale = scale, .clip = text_clip);
        }
    }
    if (args->editor->frame->find.open)
    {
        EMBED_CALL(editor_find_draw, EditorCtx, .editor = args->editor, .raster = raster, .glyphs = glyphs,
                   .palette = args->palette, .place = args->place, .scale = scale, .size = size,
                   .line_height = line_height);
    }
}
