// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// menubar.c: the menu bar. What is open, what is marked and what can be chosen are the controller's and come in the
// menus' snapshot; where each title and menu stands, and which pixel stands on what, are worked out here
#include "menubar.h"

#include "chrome.h"

#include <math.h>

// the most titles the bar shows at a window width in the page's pixels, narrowest last, as the page folds them
typedef struct
{
    int width;
    unsigned int titles;
} GuiTitlesAt;

static const GuiTitlesAt s_titles_at[] = {{1600, GUI_MENUBAR_TITLES}, {1440, 13u}, {1280, 11u}, {1120, 9u}, {0, 7u}};

void gui_menubar_init(const GuiMenubarCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->menubar, .val = 0, .bytes = sizeof(*args->menubar)});
    args->menubar->pointed = -1;
}

typedef struct
{
    unsigned long long *length;
    const GuiMenubar *menubar;
    const GuiTitle *title;
    GuiGlyphs *glyphs;
    unsigned int size;
    int scale;
    const CliMenuRow *row;
    unsigned int level;
    unsigned int index;
    int x;
    int y;
    GuiMenubar *menubar_out;
    const GuiFrame *frame;
    const GuiRect *adjacent;
    GuiRaster *raster;
    GuiRect rect;
    int thick;
    GuiColor color;
} MenubarCtx;

// a title's label: the menu's title, or the folded mark
EMBED_INLINE const char *menubar_title_text(const MenubarCtx *args)
{
    unsigned long long *length = args->length;
    if (args->title->folded)
    {
        *length = 3ULL;
        return "\xe2\x80\xa6";
    }
    if (args->menubar->menus == NULL)
    {
        *length = 0ULL;
        return "";
    }
    const char *const title = args->menubar->menus->title[args->title->menu];
    *length = cellul.len(&(CatenaFinitaCfg){.src = title, .cap = CLI_CELLS_POOL});
    return title;
}

// a title's width for its text: the text and its padding each side
EMBED_INLINE int menubar_title_width(const MenubarCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    unsigned int size = args->size;
    int scale = args->scale;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar, .title = args->title, .length = &length);
    return (int)roundf(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                                  .face = GUI_FACE_HEAD, .size = size, .text = text, .length = length)) +
           (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_PAD, .scale = scale));
}

// a row's height: a line's or an item's
EMBED_INLINE int menubar_row_height(const MenubarCtx *args)
{
    int scale = args->scale;
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = (args->row->kind == CLI_MENU_ROW_LINE) ? GUI_MENU_LINE_HEIGHT : GUI_ITEM_HEIGHT,
                      .scale = scale);
}

// a row's label's start from its left at the scale: further in where any row of the open menu at `level` shows a
// state on or off, which sets every label in
EMBED_INLINE int menubar_label_x(const MenubarCtx *args)
{
    const CliMenuLevel *const level = &args->menubar->menus->level[args->level];
    int checks = 0;
    for (unsigned int at = 0u; (at < level->rows) && !checks; at += 1u)
    {
        checks = level->row[at].checked >= 0;
    }
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                      .pixels = checks ? GUI_ITEM_CHECKS_PAD_X : GUI_ITEM_PAD_X, .scale = args->scale);
}

// the rectangle of the row `index` of the open menu at `level`
EMBED_INLINE GuiRect menubar_row_rect(const MenubarCtx *args)
{
    const GuiMenubar *menubar = args->menubar;
    int scale = args->scale;
    const CliMenuLevel *const level = &menubar->menus->level[args->level];
    const GuiRect menu = menubar->menu_rect[args->level];
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_PAD, .scale = scale);
    int y = menu.y + pad;
    for (unsigned int at = 0u; at < args->index; at += 1u)
    {
        y += EMBED_CALL(menubar_row_height, MenubarCtx, .row = &level->row[at], .scale = scale);
    }
    return (GuiRect){menu.x + pad, y, menu.width - (2 * pad),
                     EMBED_CALL(menubar_row_height, MenubarCtx, .row = &level->row[args->index], .scale = scale)};
}

// the menu at `level` sized for its rows and placed with its top left at x, y, turned in from the window's edges;
// where it opens beside an item, `adjacent` that item's rectangle, it goes to the item's other side where the right
// has no capacity
static void menubar_menu_position(const MenubarCtx *args)
{
    GuiMenubar *menubar = args->menubar_out;
    GuiGlyphs *glyphs = args->glyphs;
    const CliMenuLevel *const level = &menubar->menus->level[args->level];
    const int scale = args->frame->scale;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = scale);
    const unsigned int keys_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_KEYS_SIZE, .scale = scale);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_PAD, .scale = scale);
    const int item_pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_PAD_X, .scale = scale);
    const int label_x =
        EMBED_CALL(menubar_label_x, MenubarCtx, .menubar = menubar, .level = args->level, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_GAP, .scale = scale);
    int pixel_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_WIDTH_MIN, .scale = scale);
    int pixel_height = 2 * pad;
    for (unsigned int at = 0u; at < level->rows; at += 1u)
    {
        const CliMenuRow *const row = &level->row[at];
        pixel_height += EMBED_CALL(menubar_row_height, MenubarCtx, .row = row, .scale = scale);
        if (row->kind == CLI_MENU_ROW_LINE)
        {
            continue;
        }
        const float label =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                       .size = size, .text = row->label,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = row->label, .cap = CLI_CELLS_POOL}));
        const float right =
            (row->kind == CLI_MENU_ROW_MORE)
                ? (float)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 8, .scale = scale)
                : EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                             .face = GUI_FACE_CODE, .size = keys_size, .text = row->keys,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = row->keys, .cap = CLI_CELLS_POOL}));
        const int needed = (int)roundf(label + (float)gap + right) + label_x + item_pad + (2 * pad);
        pixel_width = (needed > pixel_width) ? needed : pixel_width;
    }
    const int edge = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_EDGE, .scale = scale);
    // the window's sides are far inside what an int represents
    const int window_width = (int)args->frame->width;
    const int window_height = (int)args->frame->height;
    int left = args->x;
    if ((args->adjacent != NULL) && (left + pixel_width > window_width - edge))
    {
        left = args->adjacent->x - pixel_width +
               EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_ADJACENT, .scale = scale);
    }
    left = (left > window_width - pixel_width - edge) ? window_width - pixel_width - edge : left;
    left = (left < edge) ? edge : left;
    pixel_height = (pixel_height > window_height - (2 * edge)) ? window_height - (2 * edge) : pixel_height;
    int top = args->y;
    if (top + pixel_height > window_height - edge)
    {
        top = (args->adjacent != NULL) ? window_height - edge - pixel_height : args->y - pixel_height;
    }
    top = (top < edge) ? edge : top;
    menubar->menu_rect[args->level] = (GuiRect){left, top, pixel_width, pixel_height};
}

void gui_menubar_menus_layout(const GuiMenubarBarCfg *args)
{
    GuiMenubar *const menubar = args->menubar;
    const CliMenus *const menus = menubar->menus;
    if ((menus == NULL) || (menus->levels == 0u) || (menus->open < 0) || (menubar->titles == 0u))
    {
        return;
    }
    // the open title is not negative past the test, and a title the bar no longer shows opens from the last it does
    const unsigned int open =
        ((unsigned int)menus->open < menubar->titles) ? (unsigned int)menus->open : (menubar->titles - 1u);
    EMBED_CALL(menubar_menu_position, MenubarCtx, .menubar_out = menubar, .glyphs = args->glyphs, .frame = args->frame,
               .level = 0u, .x = menubar->title[open].rect.x,
               .y = args->frame->bar.y + args->frame->bar.height +
                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_BELOW,
                               .scale = args->frame->scale),
               .adjacent = NULL);
    for (unsigned int level = 1u; (level < menus->levels) && (level < GUI_MENU_LEVELS); level += 1u)
    {
        const int opened = menus->level[level - 1u].opened;
        // the row its items open from is among the rows; where none is told the menu follows the one before
        const GuiRect adjacent = (opened >= 0)
                                     ? EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = menubar, .level = level - 1u,
                                                  .index = (unsigned int)opened, .scale = args->frame->scale)
                                     : menubar->menu_rect[level - 1u];
        EMBED_CALL(menubar_menu_position, MenubarCtx, .menubar_out = menubar, .glyphs = args->glyphs,
                   .frame = args->frame, .level = level,
                   .x = adjacent.x + adjacent.width -
                        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_ADJACENT,
                                   .scale = args->frame->scale),
                   .y = adjacent.y - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENU_ABOVE,
                                                .scale = args->frame->scale),
                   .adjacent = &adjacent);
    }
}

void gui_menubar_titles_alloc(const GuiMenubarFrameCfg *args)
{
    memor.cpy(&(MemoriaCfg){
        .dst = args->menubar->title, .src = args->title, .bytes = (unsigned long long)args->count * sizeof(GuiTitle)});
    args->menubar->titles = args->count;
    args->menubar->folded_from = args->folded_from;
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = args->menubar, .glyphs = args->glyphs,
               .frame = args->frame);
}

void gui_menubar_titles_layout(const GuiMenubarBarCfg *args)
{
    GuiMenubar *const menubar = args->menubar;
    const CliMenus *const menus = menubar->menus;
    menubar->titles = 0u;
    // the bar moves under a pointer standing still, which marks no title until it moves, while a menu open saves its
    // own
    if ((menus == NULL) || (menus->levels == 0u))
    {
        menubar->pointed = -1;
    }
    if ((menus == NULL) || (menus->titles == 0u))
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const unsigned int count = menus->titles;
    unsigned int most = GUI_MENUBAR_TITLES;
    for (unsigned int at = 0u; at < sizeof(s_titles_at) / sizeof(s_titles_at[0]); at += 1u)
    {
        if ((int)args->frame->width >= EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                  .pixels = s_titles_at[at].width, .scale = args->frame->scale))
        {
            most = s_titles_at[at].titles;
            break;
        }
    }
    // the capacity the titles have: from the bar's start to where the tree's name saves its capacity
    // and the eye's capacity before it, given up as the claim runs
    const long long logo_capacity =
        ((long long)(GUI_FIXED_ONE - args->frame->claim.claim) *
         (long long)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENUBAR_LOGO_CAPACITY,
                               .scale = args->frame->scale)) /
        GUI_FIXED_ONE;
    // the capacity is less than the window's width: it is far inside what an int represents
    const int start = args->frame->bar.x +
                      EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_MENUBAR_START,
                                 .scale = args->frame->scale) +
                      (int)logo_capacity;
    const int capacity_end =
        args->frame->controls.x - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                             .pixels = GUI_MENUBAR_NAME_CAPACITY, .scale = args->frame->scale);
    const int margin_x = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_MARGIN_X,
                                    .scale = args->frame->scale);
    const int margin_y = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TITLE_MARGIN_Y,
                                    .scale = args->frame->scale);
    const int pixel_height = args->frame->bar.height - (2 * margin_y);
    GuiTitle folded = {0u, 1, {0, 0, 0, 0}};
    const int folded_width = EMBED_CALL(menubar_title_width, MenubarCtx, .menubar = menubar, .title = &folded,
                                        .glyphs = args->glyphs, .size = size, .scale = args->frame->scale);
    int x = start;
    menubar->folded_from = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        GuiTitle title = {at, 0, {0, 0, 0, 0}};
        const int pixel_width = EMBED_CALL(menubar_title_width, MenubarCtx, .menubar = menubar, .title = &title,
                                           .glyphs = args->glyphs, .size = size, .scale = args->frame->scale);
        // the title fits where it and the folded title after it would, or it is the last and fits alone
        const int reach =
            x + margin_x + pixel_width + margin_x + (((at + 1u) < count) ? (margin_x + folded_width + margin_x) : 0);
        const int fits =
            (menubar->titles < most) && (reach <= capacity_end) && ((menubar->titles + 1u) < GUI_MENUBAR_TITLES);
        if (!fits)
        {
            menubar->folded_from = at;
            break;
        }
        title.rect = (GuiRect){x + margin_x, args->frame->bar.y + margin_y, pixel_width, pixel_height};
        menubar->title[menubar->titles] = title;
        menubar->titles += 1u;
        x += margin_x + pixel_width + margin_x;
    }
    if (menubar->folded_from < count)
    {
        folded.rect = (GuiRect){x + margin_x, args->frame->bar.y + margin_y, folded_width, pixel_height};
        menubar->title[menubar->titles] = folded;
        menubar->titles += 1u;
    }
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = menubar, .glyphs = args->glyphs,
               .frame = args->frame);
}

// the title at x, y, -1 for none
EMBED_INLINE int menubar_title_at(const MenubarCtx *args)
{
    for (unsigned int at = 0u; at < args->menubar->titles; at += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->menubar->title[at].rect,
                       .x = args->x, .y = args->y))
        {
            // the titles are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

int gui_menubar_holds(const GuiMenubarXCfg *args)
{
    unsigned int level = 0u;
    unsigned int index = 0u;
    return EMBED_CALL(gui_menubar_x.hit, GuiMenubarXCfg, .menubar_read = args->menubar_read, .scale = args->scale,
                      .x = args->x, .y = args->y, .level = &level, .index = &index) != CLI_HIT_NONE;
}

unsigned int gui_menubar_hit(const GuiMenubarXCfg *args)
{
    const GuiMenubar *const menubar = args->menubar_read;
    const int title = EMBED_CALL(menubar_title_at, MenubarCtx, .menubar = menubar, .x = args->x, .y = args->y);
    if (title >= 0)
    {
        // the title is among the titles
        *args->index = (unsigned int)title;
        return CLI_HIT_MENU_TITLE;
    }
    const CliMenus *const menus = menubar->menus;
    for (unsigned int level = (menus != NULL) ? menus->levels : 0u; level > 0u; level -= 1u)
    {
        if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = menubar->menu_rect[level - 1u],
                        .x = args->x, .y = args->y))
        {
            continue;
        }
        *args->level = level - 1u;
        // a pixel in the menu on no row names a place past its rows, which nothing can be chosen at
        *args->index = menus->level[level - 1u].rows;
        for (unsigned int at = 0u; at < menus->level[level - 1u].rows; at += 1u)
        {
            if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                           .rect = EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = menubar, .level = level - 1u,
                                              .index = at, .scale = args->scale),
                           .x = args->x, .y = args->y))
            {
                *args->index = at;
                break;
            }
        }
        return CLI_HIT_MENU_ROW;
    }
    return CLI_HIT_NONE;
}

int gui_menubar_move(const GuiMenubarGlyphsFrameXCfg *args)
{
    const int title = EMBED_CALL(menubar_title_at, MenubarCtx, .menubar = args->menubar, .x = args->x, .y = args->y);
    const int changed = title != args->menubar->pointed;
    args->menubar->pointed = title;
    return changed;
}

void gui_menubar_titles_draw(const GuiMenubarGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = args->frame->scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    // the titles stand in the bar's pane, the slat its edge shows
    const GuiRect pane = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->frame->bar_band,
                                    .other = args->frame->bar);
    const int open = (args->menubar_read->menus != NULL) ? args->menubar_read->menus->open : -1;
    for (unsigned int at = 0u; at < args->menubar_read->titles; at += 1u)
    {
        const GuiTitle *const title = &args->menubar_read->title[at];
        // the titles are fewer than an int counts
        const int lit = ((int)at == open) || ((int)at == args->menubar_read->pointed);
        if (lit)
        {
            EMBED_CALL(
                gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster,
                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = title->rect, .other = pane),
                .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                   .pixels = GUI_TITLE_RADIUS, .scale = args->frame->scale),
                .color = args->palette->bar_title_hover);
        }
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(menubar_title_text, MenubarCtx, .menubar = args->menubar_read,
                                            .title = title, .length = &length);
        const int baseline =
            title->rect.y + (int)((((float)title->rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = size,
                         .x_f32 =
                             (float)(title->rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                .pixels = GUI_TITLE_PAD, .scale = args->frame->scale)),
                         .y = baseline, .text = text, .length = length,
                         .color = lit ? args->palette->bar_bright : args->palette->bar_title, .clip = pane);
    }
}

// a box's outline drawn, `thick` pixels wide inside it
EMBED_INLINE void menubar_box_draw(const MenubarCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiRect rect = args->rect;
    GuiColor color = args->color;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y, rect.width, args->thick}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y + rect.height - args->thick, rect.width, args->thick}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x, rect.y, args->thick, rect.height}, .color = color);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){rect.x + rect.width - args->thick, rect.y, args->thick, rect.height}, .color = color);
}

void gui_menubar_menus_draw(const GuiMenubarGlyphsFrameCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const int scale = args->frame->scale;
    const CliMenus *const menus = args->menubar_read->menus;
    if (menus == NULL)
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_HEAD_SIZE, .scale = scale);
    const unsigned int keys_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = (int)GUI_KEYS_SIZE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size);
    const int item_pad =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_PAD_X, .scale = scale);
    const int check_x =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_ITEM_CHECK_X, .scale = scale);
    for (unsigned int level = 0u; (level < menus->levels) && (level < GUI_MENU_LEVELS); level += 1u)
    {
        const CliMenuLevel *const menu = &menus->level[level];
        const GuiRect menu_rect = args->menubar_read->menu_rect[level];
        const int label_x =
            EMBED_CALL(menubar_label_x, MenubarCtx, .menubar = args->menubar_read, .level = level, .scale = scale);
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = menu_rect,
                   .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                      .pixels = GUI_MENU_RADIUS, .scale = scale),
                   .color = args->palette->surface);
        EMBED_CALL(menubar_box_draw, MenubarCtx, .raster = raster, .rect = menu_rect,
                   .thick = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale),
                   .color = args->palette->widget_line);
        for (unsigned int at = 0u; at < menu->rows; at += 1u)
        {
            const CliMenuRow *const row = &menu->row[at];
            const GuiRect rect = EMBED_CALL(menubar_row_rect, MenubarCtx, .menubar = args->menubar_read, .level = level,
                                            .index = at, .scale = scale);
            if (row->kind == CLI_MENU_ROW_LINE)
            {
                const int inset = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 6, .scale = scale);
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                           .rect = (GuiRect){rect.x + inset, rect.y + (rect.height / 2), rect.width - (2 * inset),
                                             EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1,
                                                        .scale = scale)},
                           .color = args->palette->line);
                continue;
            }
            // the rows are fewer than an int counts
            if ((int)at == menu->pointed)
            {
                EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = rect,
                           .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4,
                                                              .scale = scale),
                           .color = args->palette->selection);
            }
            else if ((int)at == menu->opened)
            {
                EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = rect,
                           .radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 4,
                                                              .scale = scale),
                           .color = args->palette->selection_idle);
            }
            const int baseline = rect.y + (int)((((float)rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
            const GuiColor color = row->disabled ? args->palette->text_3 : args->palette->text;
            if (row->checked > 0)
            {
                (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                 .glyphs = glyphs, .face = GUI_FACE_HEAD, .size = size,
                                 .x_f32 = (float)(rect.x + check_x), .y = baseline, .text = "\xe2\x9c\x93",
                                 .length = 3ULL, .color = args->palette->signal, .clip = rect);
            }
            (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                             .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)(rect.x + label_x), .y = baseline,
                             .text = row->label,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = row->label, .cap = CLI_CELLS_POOL}),
                             .color = color, .clip = rect);
            const unsigned long long keys_length =
                cellul.len(&(CatenaFinitaCfg){.src = row->keys, .cap = CLI_CELLS_POOL});
            if (row->kind == CLI_MENU_ROW_MORE)
            {
                // a chevron pointing to where the items open
                const int arm = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
                const int tip_x = rect.x + rect.width - item_pad;
                const int middle = rect.y + (rect.height / 2);
                EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = tip_x - arm,
                           .y0 = middle - arm, .x1 = tip_x, .y1 = middle, .color = color);
                EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = tip_x - arm,
                           .y0 = middle + arm, .x1 = tip_x, .y1 = middle, .color = color);
            }
            else if (keys_length != 0ULL)
            {
                const float keys_width =
                    EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                               .face = GUI_FACE_CODE, .size = keys_size, .text = row->keys, .length = keys_length);
                (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                 .glyphs = glyphs, .face = GUI_FACE_CODE, .size = keys_size,
                                 .x_f32 = (float)(rect.x + rect.width - item_pad) - keys_width, .y = baseline,
                                 .text = row->keys, .length = keys_length, .color = args->palette->text_3,
                                 .clip = rect);
            }
        }
    }
}
