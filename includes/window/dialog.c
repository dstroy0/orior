// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dialog.c: the modal dialog. The window that owns it hands it the texts and reads its answer. This file holds no
// setting and asks no module: it lays the parts out, draws them and turns each input into a change
#include "dialog.h"
#include "memoria_operor/memoria_operor.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include "../glass/glass.h"


typedef struct
{
    const GuiDialog *dialog_read;
    GuiDialog *dialog;
    GuiGlyphs *glyphs;
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiDialogChange *change;
    char *destination;
    const char *text;
    const char *name;
    const char **line_text;
    unsigned long long *line_length;
    unsigned long long length;
    unsigned int capacity;
    unsigned int at;
    int pixels;
    int scale;
    int x;
    int y;
    GuiRect rect;
    GuiRect clip;
    GuiColor color;
} DialogCtx;

// `pixels` unscaled pixels at the dialog's scale
EMBED_INLINE int dialog_pixels(const DialogCtx *args)
{
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = args->pixels, .scale = args->scale);
}

// the bytes of `text`, no more than `capacity` less one
EMBED_INLINE unsigned long long dialog_length(const DialogCtx *args)
{
    return cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = args->capacity - 1u});
}

// `text` copied into `destination` of `capacity` bytes, cut to fit, with its end byte
EMBED_INLINE void dialog_copy(const DialogCtx *args)
{
    const unsigned long long length =
        (args->text != NULL) ? EMBED_CALL(dialog_length, DialogCtx, .text = args->text, .capacity = args->capacity)
                             : 0ULL;
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->destination, .src = args->text, .bytes = length});
    }
    args->destination[length] = '\0';
}

// the text's size in pixels at `scale`
EMBED_INLINE unsigned int dialog_size(const DialogCtx *args)
{
    // a size in pixels is not negative
    return (unsigned int)EMBED_CALL(dialog_pixels, DialogCtx, .pixels = (int)GUI_DIALOG_SIZE, .scale = args->scale);
}

// the width in pixels of `length` bytes of `text` at `scale`
EMBED_INLINE int dialog_text_width(const DialogCtx *args)
{
    // a text's width in pixels is far inside what an int holds
    return (int)EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                           .face = GUI_FACE_HEAD, .size = EMBED_CALL(dialog_size, DialogCtx, .scale = args->scale),
                           .text = args->text, .length = args->length);
}

// the count of lines in the body: 0 for an empty body, else one more than its line ends
EMBED_INLINE unsigned int dialog_lines(const DialogCtx *args)
{
    const char *const body = args->dialog_read->body;
    if (body[0] == '\0')
    {
        return 0u;
    }
    unsigned int lines = 1u;
    for (unsigned int at = 0u; body[at] != '\0'; at += 1u)
    {
        lines += (body[at] == '\n') ? 1u : 0u;
    }
    return lines;
}

// The line `at` of the body: its first byte into `line_text` and its length into `line_length`. It returns 1, or 0
// where the body holds fewer lines
EMBED_INLINE int dialog_line(const DialogCtx *args)
{
    const char *start = args->dialog_read->body;
    for (unsigned int line = 0u; line < args->at; line += 1u)
    {
        while ((*start != '\0') && (*start != '\n'))
        {
            start += 1;
        }
        if (*start == '\0')
        {
            return 0;
        }
        start += 1;
    }
    unsigned long long count = 0ULL;
    while ((start[count] != '\0') && (start[count] != '\n'))
    {
        count += 1ULL;
    }
    *args->line_text = start;
    *args->line_length = count;
    return 1;
}

// the width in pixels of the button `at` at `scale`: its label and the padding each side, and no less than the least
EMBED_INLINE int dialog_button_measure(const DialogCtx *args)
{
    const char *const label = args->dialog_read->label[args->at];
    const int padding = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_PAD, .scale = args->scale);
    const int least = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_MIN, .scale = args->scale);
    const int wide =
        EMBED_CALL(dialog_text_width, DialogCtx, .glyphs = args->glyphs, .scale = args->scale, .text = label,
                   .length = EMBED_CALL(dialog_length, DialogCtx, .text = label, .capacity = GUI_DIALOG_LABEL)) +
        (2 * padding);
    return (wide > least) ? wide : least;
}

void gui_dialog_init(const GuiDialogCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->dialog, .val = 0, .bytes = sizeof(*args->dialog)});
    args->dialog->pointed = GUI_DIALOG_BUTTONS_MAX;
    args->dialog->pressed = GUI_DIALOG_BUTTONS_MAX;
}

int gui_dialog_ask(const GuiDialogAskCfg *args)
{
    if ((args->count == 0u) || (args->count > GUI_DIALOG_BUTTONS_MAX) || (args->labels == NULL))
    {
        return 0;
    }
    GuiDialog *const dialog = args->dialog;
    EMBED_CALL(dialog_copy, DialogCtx, .destination = dialog->title, .text = args->title, .capacity = GUI_DIALOG_TITLE);
    EMBED_CALL(dialog_copy, DialogCtx, .destination = dialog->body, .text = args->body, .capacity = GUI_DIALOG_BODY);
    for (unsigned int at = 0u; at < GUI_DIALOG_BUTTONS_MAX; at += 1u)
    {
        EMBED_CALL(dialog_copy, DialogCtx, .destination = dialog->label[at],
                   .text = (at < args->count) ? args->labels[at] : NULL, .capacity = GUI_DIALOG_LABEL);
    }
    dialog->buttons = args->count;
    dialog->preferred = (args->preferred < args->count) ? args->preferred : 0u;
    dialog->marked = dialog->preferred;
    dialog->pointed = GUI_DIALOG_BUTTONS_MAX;
    dialog->pressed = GUI_DIALOG_BUTTONS_MAX;
    dialog->close_pointed = 0;
    dialog->close_pressed = 0;
    dialog->error = args->error;
    dialog->displayed = 1;
    return 1;
}

void gui_dialog_want(const GuiDialogWantCfg *args)
{
    const GuiDialog *const dialog = args->dialog_read;
    const int scale = args->scale;
    const int edge = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT + GUI_DIALOG_PAD, .scale = scale);
    const int mat = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT, .scale = scale);
    const int head = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_HEAD, .scale = scale);
    const int line = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_LINE, .scale = scale);
    const int button = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_HEIGHT, .scale = scale);
    const int space = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_SPACE, .scale = scale);
    const int close = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_CLOSE_WIDTH, .scale = scale);
    int widest = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MIN_WIDTH, .scale = scale);
    const int title =
        (2 * edge) + close +
        EMBED_CALL(dialog_text_width, DialogCtx, .glyphs = args->glyphs, .scale = scale, .text = dialog->title,
                   .length = EMBED_CALL(dialog_length, DialogCtx, .text = dialog->title, .capacity = GUI_DIALOG_TITLE));
    widest = (title > widest) ? title : widest;
    const unsigned int lines = EMBED_CALL(dialog_lines, DialogCtx, .dialog_read = dialog);
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        const char *text = NULL;
        unsigned long long length = 0ULL;
        if (EMBED_CALL(dialog_line, DialogCtx, .dialog_read = dialog, .at = at, .line_text = &text,
                       .line_length = &length))
        {
            const int wide = (2 * edge) + EMBED_CALL(dialog_text_width, DialogCtx, .glyphs = args->glyphs,
                                                     .scale = scale, .text = text, .length = length);
            widest = (wide > widest) ? wide : widest;
        }
    }
    int buttons = 2 * edge;
    for (unsigned int at = 0u; at < dialog->buttons; at += 1u)
    {
        buttons += EMBED_CALL(dialog_button_measure, DialogCtx, .dialog_read = dialog, .glyphs = args->glyphs,
                              .scale = scale, .at = at) +
                   ((at != 0u) ? space : 0);
    }
    widest = (buttons > widest) ? buttons : widest;
    // a client's sides are not negative and far inside what an unsigned int holds
    *args->width_pointer = (unsigned int)widest;
    *args->height_pointer = (unsigned int)(mat + head + (edge - mat) + ((int)lines * line) + edge + button + edge);
}

void gui_dialog_resize(const GuiDialogWidthCfg *args)
{
    GuiDialog *const dialog = args->dialog;
    dialog->width = args->width;
    dialog->height = args->height;
    dialog->scale = args->scale;
    for (unsigned int at = 0u; at < GUI_DIALOG_BUTTONS_MAX; at += 1u)
    {
        dialog->button_width[at] = (at < dialog->buttons)
                                       ? EMBED_CALL(dialog_button_measure, DialogCtx, .dialog_read = dialog,
                                                    .glyphs = args->glyphs, .scale = args->scale, .at = at)
                                       : 0;
    }
}

GuiRect gui_dialog_head_rect(const GuiDialogReadCfg *args)
{
    const int scale = args->dialog_read->scale;
    const int mat = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT, .scale = scale);
    // a client's side is far inside what an int holds
    return (GuiRect){0, 0, (int)args->dialog_read->width,
                     mat + EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_HEAD, .scale = scale)};
}

GuiRect gui_dialog_close_rect(const GuiDialogReadCfg *args)
{
    const int scale = args->dialog_read->scale;
    const int edge = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT + GUI_DIALOG_PAD, .scale = scale);
    const int close = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_CLOSE_WIDTH, .scale = scale);
    // a client's side is far inside what an int holds
    return (GuiRect){(int)args->dialog_read->width - edge - close,
                     EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT, .scale = scale), close,
                     EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_HEAD, .scale = scale)};
}

GuiRect gui_dialog_body_rect(const GuiDialogReadCfg *args)
{
    const int scale = args->dialog_read->scale;
    const int edge = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT + GUI_DIALOG_PAD, .scale = scale);
    const int top = edge + EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_HEAD, .scale = scale);
    const int line = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_LINE, .scale = scale);
    // a client's side and a count of lines are far inside what an int holds
    return (GuiRect){edge, top, (int)args->dialog_read->width - (2 * edge),
                     (int)EMBED_CALL(dialog_lines, DialogCtx, .dialog_read = args->dialog_read) * line};
}

GuiRect gui_dialog_button_rect(const GuiDialogReadCfg *args)
{
    const GuiDialog *const dialog = args->dialog_read;
    const int scale = dialog->scale;
    if (args->at >= dialog->buttons)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const int edge = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT + GUI_DIALOG_PAD, .scale = scale);
    const int height = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_HEIGHT, .scale = scale);
    const int space = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_BUTTON_SPACE, .scale = scale);
    // The buttons stand in their order and end at the foot's right edge. Each button after `at` and its space come
    // before that edge
    // a client's side is far inside what an int holds
    int right = (int)dialog->width - edge;
    for (unsigned int after = dialog->buttons - 1u; after > args->at; after -= 1u)
    {
        right -= dialog->button_width[after] + space;
    }
    const int width = dialog->button_width[args->at];
    // a client's side is far inside what an int holds
    return (GuiRect){right - width, (int)dialog->height - edge - height, width, height};
}

// the button under x, y, or GUI_DIALOG_BUTTONS_MAX for none
EMBED_INLINE unsigned int dialog_button_at(const DialogCtx *args)
{
    for (unsigned int at = 0u; at < args->dialog_read->buttons; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg,
                                          .dialog_read = args->dialog_read, .at = at),
                       .x = args->x, .y = args->y))
        {
            return at;
        }
    }
    return GUI_DIALOG_BUTTONS_MAX;
}

// whether x, y stands on the close mark
EMBED_INLINE int dialog_close_at(const DialogCtx *args)
{
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                      .rect =
                          EMBED_CALL(gui_dialog_read.close_rect, GuiDialogReadCfg, .dialog_read = args->dialog_read),
                      .x = args->x, .y = args->y);
}

// the dialog ended: no longer shown, and no press held
EMBED_INLINE void dialog_end(const DialogCtx *args)
{
    args->dialog->displayed = 0;
    args->dialog->pressed = GUI_DIALOG_BUTTONS_MAX;
    args->dialog->close_pressed = 0;
}

// whether the key `name` is `text`
EMBED_INLINE int dialog_key_is(const DialogCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->text, .cap = GUI_DIALOG_LABEL});
}

void gui_dialog_key(const GuiDialogKeyCfg *args)
{
    GuiDialog *const dialog = args->dialog;
    GuiDialogChange *const change = args->change;
    change->type = GUI_DIALOG_NOTHING;
    change->button = GUI_DIALOG_BUTTONS_MAX;
    if (!dialog->displayed || (dialog->buttons == 0u) || (args->name == NULL))
    {
        return;
    }
    const char *const name = args->name;
    if (EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Enter"))
    {
        change->type = GUI_DIALOG_ANSWERED;
        change->button = dialog->marked;
        EMBED_CALL(dialog_end, DialogCtx, .dialog = dialog);
    }
    else if (EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Escape"))
    {
        change->type = GUI_DIALOG_CLOSED;
        EMBED_CALL(dialog_end, DialogCtx, .dialog = dialog);
    }
    else if (EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Tab") ||
             EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Right"))
    {
        dialog->marked = (dialog->marked + 1u) % dialog->buttons;
        change->type = GUI_DIALOG_DRAWN;
    }
    else if (EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Shift+Tab") ||
             EMBED_CALL(dialog_key_is, DialogCtx, .name = name, .text = "Left"))
    {
        dialog->marked = (dialog->marked + dialog->buttons - 1u) % dialog->buttons;
        change->type = GUI_DIALOG_DRAWN;
    }
}

void gui_dialog_move(const GuiDialogPointCfg *args)
{
    GuiDialog *const dialog = args->dialog;
    args->change->type = GUI_DIALOG_NOTHING;
    args->change->button = GUI_DIALOG_BUTTONS_MAX;
    const unsigned int pointed =
        EMBED_CALL(dialog_button_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y);
    const int close_pointed = EMBED_CALL(dialog_close_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y);
    if ((pointed != dialog->pointed) || (close_pointed != dialog->close_pointed))
    {
        dialog->pointed = pointed;
        dialog->close_pointed = close_pointed;
        args->change->type = GUI_DIALOG_DRAWN;
    }
}

void gui_dialog_press(const GuiDialogPointCfg *args)
{
    GuiDialog *const dialog = args->dialog;
    args->change->type = GUI_DIALOG_NOTHING;
    args->change->button = GUI_DIALOG_BUTTONS_MAX;
    dialog->pressed = EMBED_CALL(dialog_button_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y);
    dialog->close_pressed = EMBED_CALL(dialog_close_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y);
    if (dialog->pressed < GUI_DIALOG_BUTTONS_MAX)
    {
        dialog->marked = dialog->pressed;
        args->change->type = GUI_DIALOG_DRAWN;
    }
}

void gui_dialog_release(const GuiDialogPointCfg *args)
{
    GuiDialog *const dialog = args->dialog;
    GuiDialogChange *const change = args->change;
    change->type = GUI_DIALOG_NOTHING;
    change->button = GUI_DIALOG_BUTTONS_MAX;
    const unsigned int under =
        EMBED_CALL(dialog_button_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y);
    if ((dialog->pressed < GUI_DIALOG_BUTTONS_MAX) && (under == dialog->pressed))
    {
        change->type = GUI_DIALOG_ANSWERED;
        change->button = under;
        EMBED_CALL(dialog_end, DialogCtx, .dialog = dialog);
        return;
    }
    if (dialog->close_pressed &&
        EMBED_CALL(dialog_close_at, DialogCtx, .dialog_read = dialog, .x = args->x, .y = args->y))
    {
        change->type = GUI_DIALOG_CLOSED;
        EMBED_CALL(dialog_end, DialogCtx, .dialog = dialog);
        return;
    }
    if ((dialog->pressed < GUI_DIALOG_BUTTONS_MAX) || dialog->close_pressed)
    {
        change->type = GUI_DIALOG_DRAWN;
    }
    dialog->pressed = GUI_DIALOG_BUTTONS_MAX;
    dialog->close_pressed = 0;
}

// `length` bytes of `text` drawn at the left of `rect`, its middle row on the rectangle's, in `color`, inside `clip`
EMBED_INLINE void dialog_text_draw(const DialogCtx *args)
{
    const unsigned int size = EMBED_CALL(dialog_size, DialogCtx, .scale = args->scale);
    (void)EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster, .glyphs = args->glyphs,
        .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)args->rect.x,
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = GUI_FACE_HEAD,
                        .size = size, .top = args->rect.y, .pixel_height = args->rect.height),
        .text = args->text, .length = args->length, .color = args->color,
        .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect, .other = args->clip));
}

// the close mark: two diagonals in a box in the middle of its rectangle, brighter under the pointer
EMBED_INLINE void dialog_close_draw(const DialogCtx *args)
{
    const GuiDialog *const dialog = args->dialog_read;
    const GuiRect rect = EMBED_CALL(gui_dialog_read.close_rect, GuiDialogReadCfg, .dialog_read = dialog);
    const int side = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_CLOSE_SIDE, .scale = dialog->scale);
    const int left = rect.x + ((rect.width - side) / 2);
    const int top = rect.y + ((rect.height - side) / 2);
    const GuiColor color = dialog->close_pointed ? args->palette->hover : args->palette->text_2;
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = left, .y0 = top,
               .x1 = left + side, .y1 = top + side, .color = color);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = left + side, .y0 = top,
               .x1 = left, .y1 = top + side, .color = color);
}

// The button `at`: a box with rounded corners and a line one pixel wide about it. The preferred button stands on the
// shown tab's ground. The line is the signal on the button with the keys while the dialog holds the focus, the hover
// color under the pointer, else the widget's line. The label stands in the middle
EMBED_INLINE void dialog_button_draw(const DialogCtx *args)
{
    const GuiDialog *const dialog = args->dialog_read;
    const GuiPalette *const palette = args->palette;
    const GuiRect box =
        EMBED_CALL(gui_dialog_read.button_rect, GuiDialogReadCfg, .dialog_read = dialog, .at = args->at);
    const unsigned int radius = (unsigned int)EMBED_CALL(
        dialog_pixels, DialogCtx, .pixels = (int)GUI_DIALOG_BUTTON_RADIUS, .scale = dialog->scale);
    const int one = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = 1, .scale = dialog->scale);
    const int keyed = (args->at == dialog->marked) && dialog->focused;
    const GuiColor pointed_line = (args->at == dialog->pointed) ? palette->hover : palette->widget_line;
    // each button a pane of glass, the preferred one tinted as the shown tab's ground
    EMBED_CALL(gui_glass_button.draw, GuiGlassButtonCfg, .raster = args->raster, .rect = box, .radius = radius,
               .tint = (args->at == dialog->preferred)
                           ? palette->tab_background
                           : palette->button,
               .grain = palette->grain, .lip = keyed ? palette->signal : pointed_line, .lip_width = one);
    const char *const label = dialog->label[args->at];
    const unsigned long long length = EMBED_CALL(dialog_length, DialogCtx, .text = label, .capacity = GUI_DIALOG_LABEL);
    const int wide = EMBED_CALL(dialog_text_width, DialogCtx, .glyphs = args->glyphs, .scale = dialog->scale,
                                .text = label, .length = length);
    EMBED_CALL(dialog_text_draw, DialogCtx, .raster = args->raster, .glyphs = args->glyphs, .scale = dialog->scale,
               .rect = (GuiRect){box.x + ((box.width - wide) / 2), box.y, wide, box.height}, .clip = args->clip,
               .text = label, .length = length, .color = palette->text);
}

void gui_dialog_draw(const GuiDialogDrawCfg *args)
{
    const GuiDialog *const dialog = args->dialog_read;
    GuiRaster *const raster = args->raster;
    const GuiPalette *const palette = args->palette;
    const int scale = dialog->scale;
    // a client's sides are far inside what an int holds
    const GuiRect clip = {0, 0, (int)dialog->width, (int)dialog->height};
    const int edge = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT + GUI_DIALOG_PAD, .scale = scale);
    const int mat = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_MAT, .scale = scale);
    const int head = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_HEAD, .scale = scale);
    // The glass covers the whole window, as it does about the main window's work. The clear ground is the same
    // lattice with no tint and no grain
    const GuiUnder table = EMBED_CALL(gui_glass.table, GuiGlassCfg, .background_fill = args->ground,
                                      .color = palette->deep, .glass = palette->glass, .grain = palette->grain);
    const GuiUnder clear = {args->ground, 0u, NULL, 0u, palette->deep};
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = table, .rect = clip, .radius = 0u, .inner = palette->glass);
    // The sheet stands in from the sides in the editor's ground at the editor's opacity. The head band at its top
    // stands in the tabs row's surface, its top corners round with the sheet's
    // a rounding in pixels is not negative
    const unsigned int pane_radius =
        (unsigned int)EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_WORK_RADIUS, .scale = scale);
    const int rounding = (int)pane_radius;
    const GuiRect sheet = {mat, mat, clip.width - (2 * mat), clip.height - (2 * mat)};
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = clear, .rect = sheet, .radius = pane_radius, .inner = palette->editor_background);
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = clear, .rect = (GuiRect){sheet.x, sheet.y, sheet.width, 2 * rounding},
               .radius = pane_radius, .inner = palette->surface);
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = clear, .rect = (GuiRect){sheet.x, sheet.y + rounding, sheet.width, head - rounding},
               .radius = 0u, .inner = palette->surface);
    const int one = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = 1, .scale = scale);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){sheet.x, sheet.y + head - one, sheet.width, one}, .color = palette->line);
    // the work's lines about the sheet, as the main window's work stands in them
    const GuiColor work_line = palette->work_line;
    const GuiOutline outline = {work_line,
                                work_line,
                                work_line,
                                work_line,
                                EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_WORK_PAD_TOP, .scale = scale),
                                EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_WORK_PAD_RIGHT, .scale = scale),
                                EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_WORK_PAD_BOTTOM, .scale = scale),
                                EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_WORK_PAD_LEFT, .scale = scale)};
    const GuiRect inside = {sheet.x + outline.left_width, sheet.y + outline.top_width,
                            sheet.width - outline.left_width - outline.right_width,
                            sheet.height - outline.top_width - outline.bottom_width};
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = raster, .work = sheet,
               .inside = inside, .radius = pane_radius, .lines = &outline, .holes = NULL, .count = 0u, .table = table,
               .clear = clear);
    // the title in the head band, and the close mark at its right
    const GuiRect close = EMBED_CALL(gui_dialog_read.close_rect, GuiDialogReadCfg, .dialog_read = dialog);
    EMBED_CALL(dialog_text_draw, DialogCtx, .raster = raster, .glyphs = args->glyphs, .scale = scale,
               .rect = (GuiRect){edge, mat, close.x - edge, head}, .clip = clip, .text = dialog->title,
               .length = EMBED_CALL(dialog_length, DialogCtx, .text = dialog->title, .capacity = GUI_DIALOG_TITLE),
               .color = palette->text);
    EMBED_CALL(dialog_close_draw, DialogCtx, .dialog_read = dialog, .raster = raster, .palette = palette);
    // each line of the body under the head band
    const GuiRect body = EMBED_CALL(gui_dialog_read.body_rect, GuiDialogReadCfg, .dialog_read = dialog);
    const int line = EMBED_CALL(dialog_pixels, DialogCtx, .pixels = GUI_DIALOG_LINE, .scale = scale);
    const unsigned int lines = EMBED_CALL(dialog_lines, DialogCtx, .dialog_read = dialog);
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        const char *text = NULL;
        unsigned long long length = 0ULL;
        if (EMBED_CALL(dialog_line, DialogCtx, .dialog_read = dialog, .at = at, .line_text = &text,
                       .line_length = &length))
        {
            // a count of lines is far inside what an int holds
            EMBED_CALL(dialog_text_draw, DialogCtx, .raster = raster, .glyphs = args->glyphs, .scale = scale,
                       .rect = (GuiRect){body.x, body.y + ((int)at * line), body.width, line}, .clip = clip,
                       .text = text, .length = length, .color = palette->text);
        }
    }
    for (unsigned int at = 0u; at < dialog->buttons; at += 1u)
    {
        EMBED_CALL(dialog_button_draw, DialogCtx, .dialog_read = dialog, .raster = raster, .glyphs = args->glyphs,
                   .palette = palette, .at = at, .clip = clip);
    }
    // The window's own border, as the compositor draws none: a line one pixel wide about the whole, its corners
    // rounded. While the dialog holds the focus, it stands in the error color for an error, else in the signal.
    // Without the focus it stands in the widget's line
    // a rounding in pixels is not negative
    const unsigned int window_radius =
        (unsigned int)EMBED_CALL(dialog_pixels, DialogCtx, .pixels = (int)GUI_DIALOG_WINDOW_RADIUS, .scale = scale);
    const GuiColor focus_line = dialog->error ? palette->fail : palette->signal;
    EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = clip,
               .radius = window_radius, .color = dialog->focused ? focus_line : palette->widget_line, .stroke = one);
}
