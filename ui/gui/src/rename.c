// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rename.c: Rename Symbol's box: its text and selection, its keys, and its drawing over the name's cell
#include "rename.h"

#include "../../../includes/window/frame.h"
#include "../../../includes/window/panel.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/editor/view.h"

typedef struct
{
    GuiRename *rename;
    const char *name;
    int pixels;
    int scale;
} RenameCtx;

// `pixels` of the page at the box's scale
EMBED_INLINE int rename_scaled(const RenameCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->scale);
}

void gui_rename_init(const GuiRenameCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->rename, .val = 0, .bytes = sizeof(*args->rename)});
}

int gui_rename_open(const GuiRenameCfg *args)
{
    GuiRename *const rename = args->rename;
    CliPosition cursor = {0ULL, 0ULL};
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    if (!cli_view_cursor(&(CliViewCursorCfg){.key = args->key, .cursor = &cursor, .anchor = NULL}) ||
        !EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = args->key, .line_value = cursor.line,
                    .col_value = cursor.col, .from = &from, .to = &to, .word_text = rename->text,
                    .capacity = sizeof(rename->text)))
    {
        return 0;
    }
    rename->open = 1;
    rename->selected = 1;
    rename->from = (CliPosition){cursor.line, from};
    rename->to = (CliPosition){cursor.line, to};
    rename->length = cellul.len(&(CatenaFinitaCfg){.src = rename->text, .cap = sizeof(rename->text)});
    rename->rect = (GuiRect){0, 0, 0, 0};
    return 1;
}

void gui_rename_close(const GuiRenameCfg *args)
{
    gui_rename_init(args);
}

// the last character erased: the continuation bytes before its lead byte go with it
EMBED_INLINE void rename_erased(const RenameCtx *args)
{
    GuiRename *const rename = args->rename;
    while (rename->length > 0ULL)
    {
        rename->length -= 1ULL;
        if (((unsigned char)rename->text[rename->length] & 0xC0u) != 0x80u)
        {
            break;
        }
    }
    rename->text[rename->length] = '\0';
}

unsigned int gui_rename_key(const GuiRenameCfg *args)
{
    GuiRename *const rename = args->rename;
    if (!rename->open)
    {
        return GUI_RENAME_PASSED;
    }
    const int escape = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = CLI_NAME_MAX_LEN});
    const int enter = cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = CLI_NAME_MAX_LEN});
    const int backspace =
        cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = CLI_NAME_MAX_LEN});
    if (escape)
    {
        return GUI_RENAME_CLOSES;
    }
    if (enter)
    {
        return (rename->length != 0ULL) ? GUI_RENAME_ACCEPTS : GUI_RENAME_CLOSES;
    }
    if (backspace)
    {
        if (rename->selected)
        {
            rename->length = 0ULL;
            rename->text[0] = '\0';
            rename->selected = 0;
        }
        else
        {
            EMBED_CALL(rename_erased, RenameCtx, .rename = rename);
        }
    }
    return GUI_RENAME_TAKEN;
}

int gui_rename_text(const GuiRenameCfg *args)
{
    GuiRename *const rename = args->rename;
    if (!rename->open)
    {
        return 0;
    }
    if (rename->selected)
    {
        rename->length = 0ULL;
        rename->selected = 0;
    }
    // a name's bytes are fewer than the box holds; what goes past the end is left off
    const unsigned long long room = sizeof(rename->text) - 1ULL - rename->length;
    const unsigned long long bytes = (args->length < room) ? args->length : room;
    memor.cpy(&(MemoriaCfg){.dst = rename->text + rename->length, .src = args->text, .bytes = (embed_word)bytes});
    rename->length += bytes;
    rename->text[rename->length] = '\0';
    return 1;
}

void gui_rename_draw(const GuiRenameCfg *args)
{
    GuiRename *const rename = args->rename;
    if (!rename->open)
    {
        return;
    }
    const int pad_x = EMBED_CALL(rename_scaled, RenameCtx, .pixels = GUI_RENAME_PAD_X, .scale = args->scale);
    const int pad_y = EMBED_CALL(rename_scaled, RenameCtx, .pixels = GUI_RENAME_PAD_Y, .scale = args->scale);
    const int caret = EMBED_CALL(rename_scaled, RenameCtx, .pixels = GUI_RENAME_CARET, .scale = args->scale);
    const unsigned int size =
        (unsigned int)EMBED_CALL(rename_scaled, RenameCtx, .pixels = (int)GUI_RENAME_SIZE, .scale = args->scale);
    const float text_width =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_CODE,
                   .size = size, .text = rename->text, .length = rename->length);
    const int least = EMBED_CALL(rename_scaled, RenameCtx, .pixels = GUI_RENAME_MIN_WIDTH, .scale = args->scale);
    const int inside_width = (int)(text_width + 0.5f) + caret;
    const int width = ((inside_width > least) ? inside_width : least) + (2 * pad_x);
    const int height = args->anchor.height + (2 * pad_y);
    // the box starts at the name's cell and stays inside the bounds
    int x = args->anchor.x - pad_x;
    int y = args->anchor.y - pad_y;
    const int right = args->bounds.x + args->bounds.width;
    const int foot = args->bounds.y + args->bounds.height;
    x = (x + width > right) ? right - width : x;
    x = (x < args->bounds.x) ? args->bounds.x : x;
    y = (y + height > foot) ? foot - height : y;
    y = (y < args->bounds.y) ? args->bounds.y : y;
    rename->rect = (GuiRect){x, y, width, height};
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = args->raster, .palette = args->palette, .rect = rename->rect,
               .scale = args->scale);
    const int text_x = x + pad_x;
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs,
                                    .face = GUI_FACE_CODE, .size = size, .top = y, .pixel_height = height);
    if (rename->selected && (rename->length != 0ULL))
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = (GuiRect){text_x, y + pad_y, (int)(text_width + 0.5f), args->anchor.height},
                   .color = args->palette->selection);
    }
    const float after =
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster, .glyphs = args->glyphs,
                   .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)text_x, .y = baseline, .text = rename->text,
                   .length = rename->length, .color = args->palette->editor_text, .clip = rename->rect);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = (GuiRect){(int)(after + 0.5f), y + pad_y, caret, args->anchor.height},
               .color = args->palette->caret);
}

int gui_rename_hit(const GuiRenameCfg *args)
{
    const GuiRename *const rename = args->rename_read;
    return rename->open &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = rename->rect, .x = args->x, .y = args->y);
}
