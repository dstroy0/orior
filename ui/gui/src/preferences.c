// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preferences.c: Preferences. Nothing here holds a setting: the window passes in what the rows are at and acts on the
// change each key or press answers. A row positions text alone, and how bright its words and its value are is all that
// marks it
#include "preferences.h"
#include "../../../includes/glass/glass.h"
#include "../../../includes/window/block.h"
#include "../../../includes/window/lights.h"
#include "tabs.h"

#include "../../cli/src/command_line/cli_internal.h"

// the zoom's steps in percent, and the sections' names
static const unsigned int s_zoom_percent[GUI_PREFERENCE_ZOOM_STEPS] = {50u,  67u,  75u,  80u,  90u,  100u, 110u,
                                                                       125u, 150u, 175u, 200u, 250u, 300u};
static const char *const s_sections[GUI_PREFERENCES_SECTIONS] = {
    "Settings",      "Keyboard Shortcuts", "Fonts", "Files", "Modified",
    "Notifications", "Settings Sync",      "Theme", "Colors"};

// the max bytes a color's value takes as it is typed, its mark and eight digits
#define PREFERENCES_ENTERED_MAX 9u

// a scrollbar's width, its arrows' height and the rounding of a field, a button, a tab and a thumb, unscaled pixels,
// as the stylesheet's --radius and its tabs' 6px give them
#define PREFERENCES_SCROLLBAR 10u
#define PREFERENCES_ARROW 10u
#define PREFERENCES_RADIUS 6u

typedef struct
{
    const char *text;
    const char *name;
    const char *key;
    const GuiPreferences *preferences;
    GuiPreferences *preferences_out;
    unsigned int section;
    const GuiPalette *palette;
    GuiPreferenceChange *change;
    GuiGlyphs *glyphs;
    unsigned int face;
    unsigned int size;
    GuiRect rect;
    GuiRaster *raster;
    const GuiPreferenceValues *values;
    unsigned int row;
    GuiRect clip;
    unsigned int at;
    GuiColor color;
    GuiColor ground;
    unsigned int edge;
    unsigned int corners;
    GuiMask *mask;
    unsigned char *cover;
    char *name_out;
    unsigned int *top_pointer;
    unsigned int total;
    unsigned int in_view;
    int rows;
    GuiButtons *buttons;
    unsigned long long time_ms;
} PreferencesCtx;

// the bits of the corners a rectangle rounds: every corner, and the top two alone
#define PREFERENCES_CORNERS_ALL 15u
#define PREFERENCES_CORNERS_TOP 3u
// the bytes of `text`
EMBED_INLINE unsigned long long preferences_length(const PreferencesCtx *args)
{
    return cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = GUI_PREFERENCES_TEXT});
}

// whether a key is the one named
EMBED_INLINE int preferences_key(const PreferencesCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->key, .cap = GUI_PREFERENCES_TEXT});
}

void gui_preferences_init(const GuiPreferencesSizeCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->preferences, .val = 0, .bytes = sizeof(*args->preferences)});
    args->preferences->pointer_x = -1;
    args->preferences->pointer_y = -1;
    EMBED_CALL(gui_buttons.init, GuiButtonsCfg, .buttons = &args->preferences->buttons);
}

void gui_preferences_palettes_clear(const GuiPreferencesSizeCfg *args)
{
    args->preferences->palettes = 0u;
    args->preferences->palette_chosen = 0u;
    args->preferences->palette_top = 0u;
    args->preferences->color_first = 0u;
}

void gui_preferences_palette_add(const GuiPreferencesSizeCfg *args)
{
    GuiPreferences *const preferences = args->preferences;
    if (preferences->palettes >= GUI_PREFERENCES_PALETTES)
    {
        return;
    }
    VerbaLinea line = {preferences->palette_name[preferences->palettes], GUI_PREFERENCES_TEXT, 0ULL};
    mmgr_line_put(&line, args->text);
    (void)mmgr_line_finish(&line);
    preferences->palettes += 1u;
}

// the colors the grid shows: the palette's from the first displayed
EMBED_INLINE unsigned int preferences_color_count(const PreferencesCtx *args)
{
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = args->palette);
    return (count > args->preferences->color_first) ? count - args->preferences->color_first : 0u;
}

// the name of the color `at` of the grid, into `text` of `size` bytes: a plugin's token's own name, its head to the
// first dot cut, where the grid shows a plugin's palette
EMBED_INLINE void preferences_color_name(const PreferencesCtx *args)
{
    char name[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = args->palette,
               .at = args->preferences->color_first + args->at, .name = name, .size = sizeof(name));
    unsigned long long from = 0ULL;
    if (args->preferences->color_first != 0u)
    {
        while ((name[from] != '\0') && (name[from] != '.'))
        {
            from += 1ULL;
        }
        from += (name[from] == '.') ? 1ULL : 0ULL;
    }
    VerbaLinea line = {args->name_out, args->size, 0ULL};
    mmgr_line_put(&line, name + from);
    (void)mmgr_line_finish(&line);
}

unsigned int gui_preference_zoom_percent(const GuiPreferencesStepCfg *args)
{
    return s_zoom_percent[(args->step < GUI_PREFERENCE_ZOOM_STEPS) ? args->step : GUI_PREFERENCE_ZOOM_DEFAULT_STEP];
}

int gui_preference_zoom_step(const GuiPreferencesCfg *args)
{
    unsigned int *step = args->step_pointer;
    for (unsigned int at = 0u; at < GUI_PREFERENCE_ZOOM_STEPS; at += 1u)
    {
        if (s_zoom_percent[at] == args->percent)
        {
            *step = at;
            return 1;
        }
    }
    return 0;
}

unsigned int gui_preference_zoom_move(const GuiPreferencesStepCfg *args)
{
    if (args->by < 0)
    {
        return (args->step == 0u) ? 0u : args->step - 1u;
    }
    if (args->step + 1u < GUI_PREFERENCE_ZOOM_STEPS)
    {
        return args->step + 1u;
    }
    return args->round ? 0u : args->step;
}

unsigned int gui_preference_opacity_move(const GuiPreferencesPercentCfg *args)
{
    if (args->by < 0)
    {
        return (args->percent < GUI_PREFERENCE_OPACITY_STEP) ? 0u : args->percent - GUI_PREFERENCE_OPACITY_STEP;
    }
    if (args->percent < GUI_PREFERENCE_OPACITY_MAX)
    {
        return args->percent + GUI_PREFERENCE_OPACITY_STEP;
    }
    return args->round ? 0u : args->percent;
}

void gui_preference_sentence(const GuiPreferencesValuesCfg *args)
{
    VerbaLinea words = {args->output, args->size, 0ULL};
    VerbaLinea line = {args->value, args->size, 0ULL};
    if (args->row < args->values->rows)
    {
        // the row's label and its value as the window wrote it; a row that nothing sets says so
        const GuiPreferenceRow *const row = &args->values->row[args->row];
        mmgr_line_put(&words, row->label);
        mmgr_line_put(&words, " is ");
        mmgr_line_put(&line, (row->value[0] != '\0') ? row->value : "not set");
    }
    (void)mmgr_line_finish(&words);
    (void)mmgr_line_finish(&line);
}

void gui_preferences_want(const GuiPreferencesScaleCfg *args)
{
    const GuiPalette *palette = args->palette;
    int scale = args->scale;
    unsigned int *width = args->width_pointer;
    unsigned int *height = args->height_pointer;
    const int edge =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE, .scale = scale);
    // the colors' rows in two columns, and the row over them; a count of colors is far inside what an int represents
    const int rows =
        (int)((EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette) + 1u) / 2u) + 1;
    // the tabs' row along the top and the edge under it
    const int tabs =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW, .scale = scale) +
        (edge / 2);
    // sizes at a scale are not negative; the palettes' list's column and its inset are before the two columns
    *width =
        (unsigned int)((2 * edge) +
                       EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                  .pixels = GUI_PREFERENCES_LIST_WIDTH + GUI_PREFERENCES_FIELD_INSET, .scale = scale) +
                       (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL_WIDTH,
                                       .scale = scale)));
    *height = (unsigned int)((2 * edge) + tabs +
                             (rows * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                .pixels = GUI_PREFERENCES_CELL, .scale = scale)));
}

GuiRect gui_preferences_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    // a client's sides are far inside what an int represents
    return (GuiRect){0, 0, (int)args->preferences_read->width, (int)args->preferences_read->height};
}

GuiRect gui_preferences_close_rect(const GuiPreferencesRowCfg *args)
{
    // the close mark is at the right end of the tabs' row, the edge in from the window's right side
    const int scale = args->preferences_read->scale;
    const int pixel_width =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CLOSE_WIDTH, .scale = scale);
    const int edge =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE, .scale = scale);
    // a client's side is far inside what an int represents
    return (GuiRect){
        (int)args->preferences_read->width - edge - pixel_width, edge, pixel_width,
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW, .scale = scale)};
}

GuiRect gui_preferences_section_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    const GuiRect rect = gui_preferences_palette_values.rect(args);
    const int row = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW,
                               .scale = args->preferences_read->scale);
    // a section's tab is along the top after the tabs before it, each as wide as its name makes it
    int before = 0;
    for (unsigned int section = 0u; section < args->at; section += 1u)
    {
        before += args->preferences_read->tab_width[section];
    }
    return (GuiRect){rect.x +
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                    .scale = args->preferences_read->scale) +
                         before,
                     rect.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                         .scale = args->preferences_read->scale),
                     args->preferences_read->tab_width[args->at], row};
}

// what a section shows is in: under the tabs' row, the edge all round and half an edge under the tabs
EMBED_INLINE GuiRect preferences_content(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    const GuiRect rect = EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg,
                                    .preferences_read = preferences);
    const int edge = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                .scale = preferences->scale);
    const int tabs = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW,
                                .scale = preferences->scale) +
                     (edge / 2);
    return (GuiRect){rect.x + edge, rect.y + edge + tabs, rect.width - (2 * edge), rect.height - (2 * edge) - tabs};
}

// the rows of a section in view, one at the least
EMBED_INLINE unsigned int preferences_lines_in_view(const PreferencesCtx *args)
{
    const int row = args->preferences->pixels.row;
    const int rows = (row > 0) ? (preferences_content(args).height / row) : 1;
    // the count is not negative past the test
    return (rows < 1) ? 1u : (unsigned int)rows;
}

// the scrollbar beside a section's rows, the rows' side of what a section shows less its width
EMBED_INLINE GuiRect preferences_scrollbar(const PreferencesCtx *args)
{
    const GuiRect content = preferences_content(args);
    const int width = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_WIDTH,
                                 .scale = args->preferences->scale);
    return (GuiRect){content.x + content.width - width, content.y, width, content.height};
}

GuiRect gui_preferences_row_rect(const GuiPreferencesRowCfg *args)
{
    const GuiPreferences *const preferences = args->preferences_read;
    const GuiRect content = EMBED_CALL(preferences_content, PreferencesCtx, .preferences = preferences);
    const unsigned int lines = EMBED_CALL(preferences_lines_in_view, PreferencesCtx, .preferences = preferences);
    if ((args->row < preferences->top) || (args->row >= preferences->top + lines))
    {
        return (GuiRect){0, 0, 0, 0};
    }
    // the scrollbar is beside the rows only where they are more than the section shows
    const int gutter =
        (preferences->rows > lines) ? (preferences->scrollbar.rect.width + preferences->pixels.inset) : 0;
    // a row's place is far inside what an int represents
    return (GuiRect){content.x, content.y + ((int)(args->row - preferences->top) * preferences->pixels.row),
                     content.width - gutter, preferences->pixels.row};
}

GuiRect gui_preferences_control_rect(const GuiPreferencesRowCfg *args)
{
    const GuiPreferencesPixels *const pixels = &args->preferences_read->pixels;
    const GuiRect row = gui_preferences_row_rect(args);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = row))
    {
        return row;
    }
    const int room = row.width - pixels->label;
    return (GuiRect){row.x + pixels->label, row.y + pixels->inset, (room < pixels->control) ? room : pixels->control,
                     row.height - (2 * pixels->inset)};
}

// the palettes' list's column at the left of what Colors shows: its width and the inset after it, none where no
// palette is listed
EMBED_INLINE int preferences_list_width(const PreferencesCtx *args)
{
    if (args->preferences->palettes == 0u)
    {
        return 0;
    }
    return EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_LIST_WIDTH,
                      .scale = args->preferences->scale) +
           args->preferences->pixels.inset;
}

// what the colors' grid is in: what a section shows, less the row over it that says which colors are under the
// least contrast, the palettes' list's column at its left and the scrollbar's gutter at its right, which is
// whether or not the bar shows, so the cells retain their places as the grid grows past the display
EMBED_INLINE GuiRect preferences_grid(const PreferencesCtx *args)
{
    const GuiRect content = preferences_content(args);
    const int row = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL,
                               .scale = args->preferences->scale);
    const int list = preferences_list_width(args);
    const int gutter = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_WIDTH,
                                  .scale = args->preferences->scale) +
                       args->preferences->pixels.inset;
    return (GuiRect){content.x + list, content.y + row, content.width - list - gutter, content.height - row};
}

unsigned int gui_preferences_cells_across(const GuiPreferencesPaletteValuesCfg *args)
{
    const int across = EMBED_CALL(preferences_grid, PreferencesCtx, .preferences = args->preferences_read).width /
                       EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL_WIDTH,
                                  .scale = args->preferences_read->scale);
    // the count is not negative past the test
    return (across < 1) ? 1u : (unsigned int)across;
}

// the grid's rows in view, one at the least
EMBED_INLINE unsigned int preferences_rows_in_view(const PreferencesCtx *args)
{
    const int rows =
        preferences_grid(args).height / EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                   .pixels = GUI_PREFERENCES_CELL, .scale = args->preferences->scale);
    // the count is not negative past the test
    return (rows < 1) ? 1u : (unsigned int)rows;
}

GuiRect gui_preferences_cell_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    const unsigned int across = gui_preferences_palette_values.cells_across(args);
    const unsigned int row = args->at / across;
    if ((row < args->preferences_read->top) ||
        (row >= args->preferences_read->top +
                    EMBED_CALL(preferences_rows_in_view, PreferencesCtx, .preferences = args->preferences_read)))
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect grid = EMBED_CALL(preferences_grid, PreferencesCtx, .preferences = args->preferences_read);
    const int pixel_width = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL_WIDTH,
                                       .scale = args->preferences_read->scale);
    const int pixel_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL,
                                        .scale = args->preferences_read->scale);
    // a cell's place is far inside what an int represents
    return (GuiRect){grid.x + ((int)(args->at % across) * pixel_width),
                     grid.y + ((int)(row - args->preferences_read->top) * pixel_height), pixel_width, pixel_height};
}

// the list's rows in view, one at the least: the grid's height in rows a cell tall
EMBED_INLINE unsigned int preferences_list_in_view(const PreferencesCtx *args)
{
    return preferences_rows_in_view(args);
}

// the rows the displayed section's scrollbar moves over: the grid's rows of colors in Colors, else the section's rows
EMBED_INLINE unsigned int preferences_scroll_total(const PreferencesCtx *args)
{
    if (args->preferences->section != GUI_PREFERENCES_COLORS)
    {
        return args->preferences->rows;
    }
    const unsigned int across = EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg,
                                           .preferences_read = args->preferences);
    return (preferences_color_count(args) + across - 1u) / across;
}

// the rows of those the displayed section displays at once
EMBED_INLINE unsigned int preferences_scroll_displayed(const PreferencesCtx *args)
{
    return (args->preferences->section == GUI_PREFERENCES_COLORS) ? preferences_rows_in_view(args)
                                                                  : preferences_lines_in_view(args);
}

GuiRect gui_preferences_list_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    const GuiPreferences *const preferences = args->preferences_read;
    if ((preferences->palettes == 0u) || (args->at >= preferences->palettes) || (args->at < preferences->palette_top) ||
        (args->at >=
         preferences->palette_top + EMBED_CALL(preferences_list_in_view, PreferencesCtx, .preferences = preferences)))
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect grid = EMBED_CALL(preferences_grid, PreferencesCtx, .preferences = preferences);
    const int pixel_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL,
                                        .scale = preferences->scale);
    // a row's place is far inside what an int represents
    return (GuiRect){grid.x - EMBED_CALL(preferences_list_width, PreferencesCtx, .preferences = preferences),
                     grid.y + ((int)(args->at - preferences->palette_top) * pixel_height),
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_LIST_WIDTH,
                                .scale = preferences->scale),
                     pixel_height};
}

// the grid's first row in view moved to save the color marked in view
EMBED_INLINE void preferences_mark_in_view(const PreferencesCtx *args)
{
    GuiPreferences *preferences = args->preferences_out;
    const unsigned int row =
        preferences->marked / EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg,
                                         .preferences_read = preferences);
    const unsigned int rows = EMBED_CALL(preferences_rows_in_view, PreferencesCtx, .preferences = preferences);
    if (row < preferences->top)
    {
        preferences->top = row;
    }
    else if (row >= preferences->top + rows)
    {
        preferences->top = row + 1u - rows;
    }
}

// The mask `mask` made a disc of the side `at`, no more than GUI_SCROLLBAR_SIDE_MAX, its coverage in `cover`. Each
// pixel's coverage is counted from 4 by 4 points inside it
EMBED_INLINE void preferences_disc_make(const PreferencesCtx *args)
{
    GuiMask *const mask = args->mask;
    const unsigned int side = args->at;
    const double reach = (double)side / 2.0;
    mask->width = side;
    mask->height = side;
    mask->left = 0;
    mask->top = 0;
    mask->cover = args->cover;
    for (unsigned int y = 0u; y < side; y += 1u)
    {
        for (unsigned int x = 0u; x < side; x += 1u)
        {
            unsigned int inside = 0u;
            for (unsigned int sample = 0u; sample < 16u; sample += 1u)
            {
                const double across = (double)x + (((double)(sample % 4u) + 0.5) / 4.0) - reach;
                const double down = (double)y + (((double)(sample / 4u) + 0.5) / 4.0) - reach;
                inside += (((across * across) + (down * down)) <= (reach * reach)) ? 1u : 0u;
            }
            // the samples inside of sixteen, a share of 255
            mask->cover[(y * side) + x] = (unsigned char)((inside * 255u) / 16u);
        }
    }
}

// Preferences' sizes made at its scale, and as masks a slider's thumb, the ring of ground about it and the corners
EMBED_INLINE void preferences_pixels_scale(const PreferencesCtx *args)
{
    GuiPreferences *const preferences = args->preferences_out;
    GuiPreferencesPixels *const pixels = &preferences->pixels;
    const int scale = preferences->scale;
    const int sizes[10] = {GUI_PREFERENCES_EDGE,          GUI_PREFERENCES_ROW,         GUI_PREFERENCES_LABEL_WIDTH,
                           GUI_PREFERENCES_CONTROL_WIDTH, GUI_PREFERENCES_FIELD_INSET, GUI_PREFERENCES_BUTTON_INSET,
                           GUI_PREFERENCES_TRACK,         GUI_PREFERENCES_THUMB,       GUI_PREFERENCES_BOX,
                           (int)PREFERENCES_RADIUS};
    int scaled[10];
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        scaled[at] = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = sizes[at], .scale = scale);
    }
    pixels->edge = scaled[0];
    pixels->row = scaled[1];
    pixels->label = scaled[2];
    pixels->control = scaled[3];
    pixels->inset = scaled[4];
    pixels->button = scaled[5];
    pixels->track = scaled[6];
    pixels->thumb = (scaled[7] > (int)GUI_SCROLLBAR_SIDE_MAX) ? (int)GUI_SCROLLBAR_SIDE_MAX : scaled[7];
    pixels->box = scaled[8];
    // a rounding and a text's size in pixels are not negative
    pixels->radius = (unsigned int)scaled[9];
    pixels->text = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                            .pixels = (int)GUI_PREFERENCES_SIZE, .scale = scale);
    // the thumb a disc of its side, and the ring of ground about it a disc wider by the ring each side
    const int ring =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_THUMB_RING, .scale = scale);
    const int max_side = (int)GUI_SCROLLBAR_SIDE_MAX;
    const int ring_side = (pixels->thumb + (2 * ring) > max_side) ? max_side : pixels->thumb + (2 * ring);
    EMBED_CALL(preferences_disc_make, PreferencesCtx, .mask = &preferences->thumb, .cover = preferences->thumb_cover,
               .at = (unsigned int)pixels->thumb);
    EMBED_CALL(preferences_disc_make, PreferencesCtx, .mask = &preferences->thumb_ring,
               .cover = preferences->thumb_ring_cover, .at = (unsigned int)ring_side);
    // The corners made as masks, in pairs: the outer edge's at a rounding and the inner edge's one pixel less. The
    // roundings: a box's, half a box's and half the track's thickness. Each pixel of a corner's square is covered by
    // its share inside the quarter circle, counted from 4 by 4 points inside it
    // a rounding in pixels is far inside what an int represents
    const int roundings[GUI_PREFERENCES_CORNER_EDGES / 2u] = {(int)pixels->radius, (int)pixels->radius / 2,
                                                              pixels->track / 2};
    for (unsigned int edge = 0u; edge < GUI_PREFERENCES_CORNER_EDGES; edge += 1u)
    {
        const int reach_out = roundings[edge / 2u] - (int)(edge % 2u);
        const int max = (int)GUI_PREFERENCES_CORNER_MAX;
        const int corner_side = (reach_out < 0) ? 0 : ((reach_out > max) ? max : reach_out);
        const double reach_sq = (double)corner_side * (double)corner_side;
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            GuiMask *const mask = &preferences->corner[edge][corner];
            // a corner's side is no more than GUI_PREFERENCES_CORNER_MAX
            mask->width = (unsigned int)corner_side;
            mask->height = (unsigned int)corner_side;
            mask->left = 0;
            mask->top = 0;
            mask->cover = preferences->corner_cover[edge][corner];
            // the circle's middle: the square's inner corner, the one the rectangle's middle is toward
            const double middle_x =
                ((corner == GUI_PREFERENCES_TOP_LEFT) || (corner == GUI_PREFERENCES_BOTTOM_LEFT)) ? corner_side : 0;
            const double middle_y =
                ((corner == GUI_PREFERENCES_TOP_LEFT) || (corner == GUI_PREFERENCES_TOP_RIGHT)) ? corner_side : 0;
            for (int y = 0; y < corner_side; y += 1)
            {
                for (int x = 0; x < corner_side; x += 1)
                {
                    unsigned int inside = 0u;
                    for (int sample = 0; sample < 16; sample += 1)
                    {
                        const double across = (double)x + (((double)(sample % 4) + 0.5) / 4.0) - middle_x;
                        const double down = (double)y + (((double)(sample / 4) + 0.5) / 4.0) - middle_y;
                        inside += (((across * across) + (down * down)) <= reach_sq) ? 1u : 0u;
                    }
                    // the samples inside of sixteen, a share of 255
                    mask->cover[((unsigned int)y * (unsigned int)corner_side) + (unsigned int)x] =
                        (unsigned char)((inside * 255u) / 16u);
                }
            }
        }
    }
}

void gui_preferences_resize(const GuiPreferencesWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    int scale = args->scale;
    GuiPreferences *const preferences = args->preferences;
    const int rescaled = preferences->scale != scale;
    preferences->width = width;
    preferences->height = height;
    preferences->scale = scale;
    preferences->top = 0u;
    if (rescaled || (preferences->pixels.row == 0))
    {
        EMBED_CALL(preferences_pixels_scale, PreferencesCtx, .preferences_out = preferences);
    }
    // Each tab is as wide as its name and the editor's tab's padding at its left and its right, and the space before
    // the next tab follows it
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                               .pixels = GUI_TAB_PAD_LEFT + GUI_TAB_PAD_RIGHT, .scale = scale) +
                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = scale);
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        // a text's width in pixels is far inside what an int represents
        preferences->tab_width[section] =
            pad +
            (int)EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                            .face = GUI_FACE_CODE,
                            // a size in pixels is not negative
                            .size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = (int)GUI_TAB_SIZE, .scale = scale),
                            .text = s_sections[section],
                            .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = s_sections[section]));
    }
    EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = &preferences->scrollbar,
               .rect = EMBED_CALL(preferences_scrollbar, PreferencesCtx, .preferences = preferences), .across = 0,
               .scale = scale);
    EMBED_CALL(preferences_mark_in_view, PreferencesCtx, .preferences_out = preferences);
}

// the section `section` displayed, its first thing marked
EMBED_INLINE void preferences_section_show(const PreferencesCtx *args)
{
    unsigned int section = args->section;
    args->preferences_out->section = section;
    args->preferences_out->first = args->preferences_out->section_first[section];
    args->preferences_out->rows = args->preferences_out->section_rows[section];
    args->preferences_out->marked = 0u;
    args->preferences_out->top = 0u;
    args->preferences_out->entering = 0;
    args->preferences_out->entered_length = 0u;
    args->preferences_out->capturing = 0;
    args->preferences_out->sliding = 0;
    args->preferences_out->scrolling = 0;
}

// the keys while a value is being typed: a row's text, or a color's value
EMBED_INLINE void preferences_entering_key(const PreferencesCtx *args)
{
    const GuiPalette *palette = args->palette;
    GuiPreferences *const preferences = args->preferences_out;
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Escape"))
    {
        preferences->entering = 0;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Backspace"))
    {
        // the last character goes whole: the bytes that continue a UTF-8 character go with the byte that starts it
        while ((preferences->entered_length != 0u) &&
               ((preferences->entered[preferences->entered_length - 1u] & 0xC0) == 0x80))
        {
            preferences->entered_length -= 1u;
        }
        preferences->entered_length -= (preferences->entered_length != 0u) ? 1u : 0u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter") &&
             (preferences->section != GUI_PREFERENCES_COLORS))
    {
        // the row's text taken: the window saves it
        args->change->type = GUI_PREFERENCE_TYPED;
        args->change->at = preferences->first + preferences->marked;
        preferences->entering = 0;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter") &&
             EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = args->preferences_out->entered,
                        .length = args->preferences_out->entered_length,
                        .current = EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette,
                                              .at = preferences->color_first + preferences->marked),
                        .color_pointer = &args->change->color))
    {
        args->change->type = GUI_PREFERENCE_COLORED;
        args->change->at = preferences->color_first + preferences->marked;
        args->preferences_out->entering = 0;
    }
}

// the palette `at` of the list chosen: the grid starts over at its first color, and the window loads the colors
EMBED_INLINE void preferences_palette_choose(const PreferencesCtx *args)
{
    GuiPreferences *const preferences = args->preferences_out;
    preferences->palette_chosen = args->at;
    preferences->marked = 0u;
    preferences->top = 0u;
    preferences->entering = 0;
    args->change->type = GUI_PREFERENCE_PALETTE;
    args->change->at = args->at;
}

// the keys over the colors' grid: the mark moved among the colors, no further than their ends; and Tab choosing the
// palette after the one chosen, round to the first
EMBED_INLINE void preferences_colors_key(const PreferencesCtx *args)
{
    GuiPreferences *preferences = args->preferences_out;
    const unsigned int across = EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg,
                                           .preferences_read = preferences);
    const unsigned int count =
        EMBED_CALL(preferences_color_count, PreferencesCtx, .preferences = preferences, .palette = args->palette);
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Tab") && (preferences->palettes != 0u))
    {
        EMBED_CALL(preferences_palette_choose, PreferencesCtx, .preferences_out = preferences,
                   .at = (preferences->palette_chosen + 1u) % preferences->palettes, .change = args->change);
        return;
    }
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter"))
    {
        preferences->entering = 1;
        preferences->entered_length = 0u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Right") &&
             (preferences->marked + 1u < count))
    {
        preferences->marked += 1u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Left") &&
             (preferences->marked != 0u))
    {
        preferences->marked -= 1u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Down") &&
             (preferences->marked + across < count))
    {
        preferences->marked += across;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Up") &&
             (preferences->marked >= across))
    {
        preferences->marked -= across;
    }
    EMBED_CALL(preferences_mark_in_view, PreferencesCtx, .preferences_out = preferences);
}

// the first row of a section in view moved to retain the row marked in view
EMBED_INLINE void preferences_row_in_view(const PreferencesCtx *args)
{
    GuiPreferences *const preferences = args->preferences_out;
    const unsigned int lines = EMBED_CALL(preferences_lines_in_view, PreferencesCtx, .preferences = preferences);
    if (preferences->marked < preferences->top)
    {
        preferences->top = preferences->marked;
    }
    else if (preferences->marked >= preferences->top + lines)
    {
        preferences->top = preferences->marked + 1u - lines;
    }
}

// The keys over a section's rows: Up and Down mark the row before or after; Right and Left move the row's value a step;
// Enter presses a button, takes the next keys pressed as a row's keys, or moves the value on and round; Delete takes a
// row's keys off
EMBED_INLINE void preferences_settings_key(const PreferencesCtx *args)
{
    GuiPreferences *const preferences = args->preferences_out;
    const unsigned int at = preferences->first + preferences->marked;
    const unsigned int type = (at < GUI_PREFERENCE_ROWS_MAX) ? preferences->type[at] : GUI_PREFERENCE_TYPE_LINE;
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Down") &&
        (preferences->marked + 1u < preferences->rows))
    {
        preferences->marked += 1u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Up") &&
             (preferences->marked != 0u))
    {
        preferences->marked -= 1u;
    }
    else if (preferences->rows == 0u)
    {
        return;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter") &&
             (type == GUI_PREFERENCE_TYPE_ACTION))
    {
        args->change->type = GUI_PREFERENCE_ACTED;
        args->change->at = at;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter") &&
             (type == GUI_PREFERENCE_TYPE_KEY))
    {
        preferences->capturing = 1;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Delete") &&
             (type == GUI_PREFERENCE_TYPE_KEY))
    {
        args->change->type = GUI_PREFERENCE_UNKEYED;
        args->change->at = at;
    }
    else if ((type != GUI_PREFERENCE_TYPE_LINE) && (type != GUI_PREFERENCE_TYPE_KEY) &&
             (type != GUI_PREFERENCE_TYPE_ACTION) &&
             (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Right") ||
              EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Left") ||
              EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter")))
    {
        args->change->type = GUI_PREFERENCE_STEPPED;
        args->change->at = at;
        args->change->by = EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Left") ? -1 : 1;
        args->change->round = EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter");
    }
    EMBED_CALL(preferences_row_in_view, PreferencesCtx, .preferences_out = preferences);
}

// a key pressed while a row takes its keys: Escape stops the taking, and any other key becomes the row's keys
EMBED_INLINE void preferences_capturing_key(const PreferencesCtx *args)
{
    GuiPreferences *const preferences = args->preferences_out;
    preferences->capturing = 0;
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Escape"))
    {
        return;
    }
    const unsigned long long length =
        cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = sizeof(preferences->entered) - 1u});
    memor.cpy(&(MemoriaCfg){.dst = preferences->entered, .src = args->name, .bytes = length});
    // a key's name is fewer bytes than the typed text holds, far inside an unsigned int
    preferences->entered_length = (unsigned int)length;
    args->change->type = GUI_PREFERENCE_KEYED;
    args->change->at = preferences->first + preferences->marked;
}

void gui_preferences_key(const GuiPreferencesPaletteNameCfg *args)
{
    *args->change = (GuiPreferenceChange){GUI_PREFERENCE_NOTHING, 0u, 0, 0, 0u, 0u};
    if (args->preferences->capturing)
    {
        EMBED_CALL(preferences_capturing_key, PreferencesCtx, .preferences_out = args->preferences, .name = args->name,
                   .change = args->change);
    }
    else if (args->preferences->entering)
    {
        EMBED_CALL(preferences_entering_key, PreferencesCtx, .preferences_out = args->preferences,
                   .palette = args->palette, .name = args->name, .change = args->change);
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Escape"))
    {
        args->preferences->displayed = 0;
        args->change->type = GUI_PREFERENCE_CLOSED;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "PageDown"))
    {
        if (args->preferences->section + 1u < GUI_PREFERENCES_SECTIONS)
        {
            EMBED_CALL(preferences_section_show, PreferencesCtx, .preferences_out = args->preferences,
                       .section = args->preferences->section + 1u);
        }
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "PageUp"))
    {
        if (args->preferences->section != 0u)
        {
            EMBED_CALL(preferences_section_show, PreferencesCtx, .preferences_out = args->preferences,
                       .section = args->preferences->section - 1u);
        }
    }
    else if (args->preferences->section == GUI_PREFERENCES_COLORS)
    {
        EMBED_CALL(preferences_colors_key, PreferencesCtx, .preferences_out = args->preferences,
                   .palette = args->palette, .name = args->name, .change = args->change);
    }
    else
    {
        EMBED_CALL(preferences_settings_key, PreferencesCtx, .preferences_out = args->preferences, .name = args->name,
                   .change = args->change);
    }
}

int gui_preferences_type(const GuiPreferencesSizeCfg *args)
{
    if (!args->preferences->entering)
    {
        return 0;
    }
    // A row's text takes every byte but the control bytes, as many as its value holds. A color's value takes hex digits
    // after an optional mark
    const int colors = args->preferences->section == GUI_PREFERENCES_COLORS;
    const unsigned int max = colors ? PREFERENCES_ENTERED_MAX : (GUI_PREFERENCES_TEXT - 1u);
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const char character = args->text[at];
        const unsigned char byte = (unsigned char)character;
        const int digit = ((character >= '0') && (character <= '9')) || ((character >= 'a') && (character <= 'f')) ||
                          ((character >= 'A') && (character <= 'F'));
        const int mark = (character == '#') && (args->preferences->entered_length == 0u);
        const int printed = (byte >= 0x20u) && (byte != 0x7Fu);
        const int taken = colors ? (digit || mark) : printed;
        if (taken && (args->preferences->entered_length < max))
        {
            args->preferences->entered[args->preferences->entered_length] = character;
            args->preferences->entered_length += 1u;
        }
    }
    return 1;
}

// a press on the displayed section's scrollbar: an arrow moves the rows a row, the track a view, and the thumb is taken
// to be dragged; 1 where the press was on the bar
EMBED_INLINE int preferences_scrollbar_press(const GuiPreferencesPaletteChangeCfg *args)
{
    GuiPreferences *const preferences = args->preferences;
    const GuiScrollbarCfg bar = {
        .bar_read = &preferences->scrollbar,
        .total =
            EMBED_CALL(preferences_scroll_total, PreferencesCtx, .preferences = preferences, .palette = args->palette),
        .displayed = EMBED_CALL(preferences_scroll_displayed, PreferencesCtx, .preferences = preferences),
        .first = preferences->top,
        .x = args->x,
        .y = args->y};
    if (bar.total <= bar.displayed)
    {
        return 0;
    }
    const unsigned int on_bar = gui_scrollbar.hit(&bar);
    if (on_bar == GUI_SCROLLBAR_NONE)
    {
        return 0;
    }
    const int lines = (int)bar.displayed;
    const int moves[6] = {0, -1, 1, 0, -lines, lines};
    if (on_bar == GUI_SCROLLBAR_THUMB)
    {
        preferences->scrolling = 1;
        return 1;
    }
    (void)gui_preferences_palette.wheel(
        &(GuiPreferencesPaletteCfg){.preferences = preferences, .palette = args->palette, .rows = moves[on_bar]});
    return 1;
}

int gui_preferences_press(const GuiPreferencesPaletteChangeCfg *args)
{
    const GuiPalette *palette = args->palette;
    *args->change = (GuiPreferenceChange){GUI_PREFERENCE_NOTHING, 0u, 0, 0, 0u};
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg,
                                      .preferences_read = args->preferences),
                   .x = args->x, .y = args->y))
    {
        args->preferences->displayed = 0;
        args->change->type = GUI_PREFERENCE_CLOSED;
        return 1;
    }
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                          .preferences_read = args->preferences, .at = section),
                       .x = args->x, .y = args->y))
        {
            if (section != args->preferences->section)
            {
                EMBED_CALL(preferences_section_show, PreferencesCtx, .preferences_out = args->preferences,
                           .section = section);
            }
            EMBED_CALL(gui_buttons_move.press, GuiButtonsMoveCfg, .buttons = &args->preferences->buttons,
                       .tool = section, .time_ms = args->time_ms);
            return 1;
        }
    }
    if (args->preferences->section == GUI_PREFERENCES_COLORS)
    {
        for (unsigned int at = 0u; at < args->preferences->palettes; at += 1u)
        {
            if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                           .rect = EMBED_CALL(gui_preferences_palette_values.list_rect, GuiPreferencesPaletteValuesCfg,
                                              .preferences_read = args->preferences, .at = at),
                           .x = args->x, .y = args->y))
            {
                EMBED_CALL(preferences_palette_choose, PreferencesCtx, .preferences_out = args->preferences, .at = at,
                           .change = args->change);
                return 1;
            }
        }
        const unsigned int count =
            EMBED_CALL(preferences_color_count, PreferencesCtx, .preferences = args->preferences, .palette = palette);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                           .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg,
                                              .preferences_read = args->preferences, .at = at),
                           .x = args->x, .y = args->y))
            {
                args->preferences->marked = at;
                args->preferences->entering = 1;
                args->preferences->entered_length = 0u;
                return 1;
            }
        }
        return preferences_scrollbar_press(args);
    }
    GuiPreferences *const preferences = args->preferences;
    if (preferences_scrollbar_press(args))
    {
        return 1;
    }
    for (unsigned int row = preferences->top; row < preferences->rows; row += 1u)
    {
        const GuiRect rect =
            EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = preferences, .row = row);
        if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = rect, .x = args->x, .y = args->y))
        {
            continue;
        }
        const unsigned int at = preferences->first + row;
        const unsigned int type = (at < GUI_PREFERENCE_ROWS_MAX) ? preferences->type[at] : GUI_PREFERENCE_TYPE_LINE;
        preferences->marked = row;
        args->change->at = at;
        if (type == GUI_PREFERENCE_TYPE_ACTION)
        {
            args->change->type = GUI_PREFERENCE_ACTED;
        }
        else if (type == GUI_PREFERENCE_TYPE_KEY)
        {
            preferences->capturing = 1;
        }
        else if ((type == GUI_PREFERENCE_TYPE_PERCENT) || (type == GUI_PREFERENCE_TYPE_ZOOM))
        {
            // a press on a slider takes its value where it lands, and the pointer drags it on from there
            preferences->sliding = 1;
            (void)gui_preferences_slide(args);
        }
        else if (type != GUI_PREFERENCE_TYPE_LINE)
        {
            args->change->type = GUI_PREFERENCE_STEPPED;
            args->change->by = 1;
            args->change->round = 1;
        }
        return 1;
    }
    return 0;
}

// a slider's track inside a row's control: the control less the room its percent is said in on the right
EMBED_INLINE GuiRect preferences_track(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiRect control = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg,
                                       .preferences_read = preferences, .row = args->row);
    const int said = preferences->pixels.label / 4;
    return (GuiRect){control.x + (preferences->pixels.thumb / 2), control.y,
                     control.width - said - preferences->pixels.thumb, control.height};
}

int gui_preferences_slide(const GuiPreferencesPaletteChangeCfg *args)
{
    GuiPreferences *const preferences = args->preferences;
    if (preferences->scrolling)
    {
        unsigned long long first = 0ull;
        gui_scrollbar.first_at(&(GuiScrollbarCfg){
            .bar_read = &preferences->scrollbar,
            .total = EMBED_CALL(preferences_scroll_total, PreferencesCtx, .preferences = preferences,
                                .palette = args->palette),
            .displayed = EMBED_CALL(preferences_scroll_displayed, PreferencesCtx, .preferences = preferences),
            .x = args->x,
            .y = args->y,
            .first_out = &first});
        // a first row is no more than the rows, far inside an unsigned int
        const int moved = (unsigned int)first != preferences->top;
        preferences->top = (unsigned int)first;
        return moved;
    }
    if (!preferences->sliding)
    {
        return 0;
    }
    const GuiRect track =
        EMBED_CALL(preferences_track, PreferencesCtx, .preferences = preferences, .row = preferences->marked);
    const int along = args->x - track.x;
    const int clamped = (along < 0) ? 0 : ((along > track.width) ? track.width : along);
    args->change->type = GUI_PREFERENCE_SLID;
    args->change->at = preferences->first + preferences->marked;
    // the share in thousandths, the track's width far inside what an unsigned int counts
    args->change->share = (track.width > 0) ? (unsigned int)((clamped * 1000) / track.width) : 0u;
    return 1;
}

int gui_preferences_release(const GuiPreferencesPaletteChangeCfg *args)
{
    const int held = args->preferences->sliding || args->preferences->scrolling;
    args->preferences->sliding = 0;
    args->preferences->scrolling = 0;
    EMBED_CALL(gui_buttons_move.release, GuiButtonsMoveCfg, .buttons = &args->preferences->buttons,
               .time_ms = args->time_ms);
    return held;
}

int gui_preferences_move(const GuiPreferencesPaletteChangeCfg *args)
{
    const GuiPalette *palette = args->palette;
    // The system sends a move with no change of place after a frame shows. Such a move marks nothing, so the mark the
    // keys moved stays
    if ((args->x == args->preferences->pointer_x) && (args->y == args->preferences->pointer_y))
    {
        return 0;
    }
    args->preferences->pointer_x = args->x;
    args->preferences->pointer_y = args->y;
    const int close_pointed = EMBED_CALL(
        gui_raster_rect.rect_holds, GuiRasterRectCfg,
        .rect = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = args->preferences),
        .x = args->x, .y = args->y);
    const int close_moved = close_pointed != args->preferences->close_pointed;
    args->preferences->close_pointed = close_pointed;
    if (args->preferences->entering)
    {
        return close_moved;
    }
    const int colors = args->preferences->section == GUI_PREFERENCES_COLORS;
    const unsigned int count = colors ? EMBED_CALL(preferences_color_count, PreferencesCtx,
                                                   .preferences = args->preferences, .palette = palette)
                                      : args->preferences->rows;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const GuiRect rect = colors
                                 ? EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg,
                                              .preferences_read = args->preferences, .at = at)
                                 : EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg,
                                              .preferences_read = args->preferences, .row = at);
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = rect, .x = args->x, .y = args->y))
        {
            const int moved = args->preferences->marked != at;
            args->preferences->marked = at;
            return moved || close_moved;
        }
    }
    return close_moved;
}

// a first row in view `top` moved `rows` among `total` rows of which `in_view` show, no further than its ends, into
// `top`: 1 where it moved
EMBED_INLINE int preferences_top_move(const PreferencesCtx *args)
{
    unsigned int *const top = args->top_pointer;
    const unsigned int max = (args->total > args->in_view) ? args->total - args->in_view : 0u;
    // the first row and the rows moved are far inside what an int represents
    const int required = (int)*top + args->rows;
    unsigned int moved_to;
    if (required < 0)
    {
        moved_to = 0u;
    }
    else if ((unsigned int)required > max)
    {
        moved_to = max;
    }
    else
    {
        moved_to = (unsigned int)required;
    }
    const int moved = moved_to != *top;
    *top = moved_to;
    return moved;
}

// whether the pointer is on the palettes' list's column
EMBED_INLINE int preferences_on_list(const PreferencesCtx *args)
{
    const GuiRect grid = preferences_grid(args);
    const int width = preferences_list_width(args);
    return (width != 0) && (args->preferences->section == GUI_PREFERENCES_COLORS) &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                      .rect = (GuiRect){grid.x - width, grid.y, width, grid.height}, .x = args->preferences->pointer_x,
                      .y = args->preferences->pointer_y);
}

int gui_preferences_wheel(const GuiPreferencesPaletteCfg *args)
{
    const GuiPalette *palette = args->palette;
    const int colors = args->preferences->section == GUI_PREFERENCES_COLORS;
    // the palettes' list under the pointer, else the grid's rows of colors, or a section's own rows
    if (EMBED_CALL(preferences_on_list, PreferencesCtx, .preferences = args->preferences))
    {
        return EMBED_CALL(preferences_top_move, PreferencesCtx, .top_pointer = &args->preferences->palette_top,
                          .total = args->preferences->palettes,
                          .in_view =
                              EMBED_CALL(preferences_list_in_view, PreferencesCtx, .preferences = args->preferences),
                          .rows = args->rows);
    }
    return EMBED_CALL(preferences_top_move, PreferencesCtx, .top_pointer = &args->preferences->top,
                      .total = EMBED_CALL(preferences_scroll_total, PreferencesCtx, .preferences = args->preferences,
                                          .palette = palette),
                      .in_view = EMBED_CALL(preferences_scroll_displayed, PreferencesCtx, .preferences = args->preferences),
                      .rows = args->rows);
}

// A rectangle `rect` filled in `color` inside `clip`, the corners in `corners` rounded. The rounding is the outer
// edge's or, where `edge` is GUI_PREFERENCES_CORNER_INNER, the inner edge's. Each rounded corner is its mask, made at
// the scale; a corner not rounded is filled square, and a rectangle too small for its corners is filled square
EMBED_INLINE void preferences_round_fill(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiRect rect = args->rect;
    // a corner's side is no more than GUI_PREFERENCES_CORNER_MAX
    const int side = (int)preferences->corner[args->edge][0].width;
    if ((side == 0) || (rect.width < (2 * side)) || (rect.height < (2 * side)))
    {
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                   .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = args->clip),
                   .color = args->color);
        return;
    }
    const GuiRect bands[3] = {{rect.x + side, rect.y, rect.width - (2 * side), side},
                              {rect.x, rect.y + side, rect.width, rect.height - (2 * side)},
                              {rect.x + side, rect.y + rect.height - side, rect.width - (2 * side), side}};
    for (unsigned int band = 0u; band < 3u; band += 1u)
    {
        EMBED_CALL(
            gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
            .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = bands[band], .other = args->clip),
            .color = args->color);
    }
    const int corner_x[4] = {rect.x, rect.x + rect.width - side, rect.x, rect.x + rect.width - side};
    const int corner_y[4] = {rect.y, rect.y, rect.y + rect.height - side, rect.y + rect.height - side};
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        if ((args->corners & (1u << corner)) != 0u)
        {
            EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster,
                       .x = corner_x[corner], .y = corner_y[corner],
                       .mask_bits = &preferences->corner[args->edge][corner], .color = args->color, .clip = args->clip);
        }
        else
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
                       .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                          .one = (GuiRect){corner_x[corner], corner_y[corner], side, side},
                                          .other = args->clip),
                       .color = args->color);
        }
    }
}

// A box with a line about it, every corner rounded: `rect` filled in the line's color `color`, and the box one pixel
// inside it filled in the color `ground`. The outer edge's rounding is `edge`, GUI_PREFERENCES_CORNER_OUTER or
// GUI_PREFERENCES_CORNER_SMALL_OUTER, and the inner edge's the one after it. The corners rounded are those in
// `corners`, every corner where it holds none
EMBED_INLINE void preferences_box_draw(const PreferencesCtx *args)
{
    const unsigned int corners = (args->corners != 0u) ? args->corners : PREFERENCES_CORNERS_ALL;
    EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = args->preferences, .raster = args->raster,
               .rect = args->rect, .clip = args->clip, .color = args->color, .edge = args->edge, .corners = corners);
    EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = args->preferences, .raster = args->raster,
               .rect = (GuiRect){args->rect.x + 1, args->rect.y + 1, args->rect.width - 2, args->rect.height - 2},
               .clip = args->clip, .color = args->ground, .edge = args->edge + 1u, .corners = corners);
}

// The text `text` drawn in the face `face` at the left of `rect`, its middle row on the rectangle's, in `color`, inside
// `clip`. It returns where the text ends
EMBED_INLINE float preferences_text_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const unsigned int size =
        (args->face == GUI_FACE_CODE)
            // a size in pixels is not negative
            ? (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PREFERENCES_CODE,
                                       .scale = preferences->scale)
            : preferences->pixels.text;
    return EMBED_CALL(
        gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = args->raster, .glyphs = args->glyphs,
        .face = args->face, .size = size, .x_f32 = (float)args->rect.x,
        .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = args->glyphs, .face = args->face,
                        .size = size, .top = args->rect.y, .pixel_height = args->rect.height),
        .text = args->text,
        .length = (args->size != 0u) ? args->size : EMBED_CALL(preferences_length, PreferencesCtx, .text = args->text),
        .color = args->color, .clip = args->clip);
}

// the width of the text `text` in the face `face`, at the scale
EMBED_INLINE int preferences_text_width(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const unsigned int size =
        (args->face == GUI_FACE_CODE)
            // a size in pixels is not negative
            ? (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PREFERENCES_CODE,
                                       .scale = preferences->scale)
            : preferences->pixels.text;
    // a text's width in pixels is far inside what an int represents
    return (int)EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                           .face = args->face, .size = size, .text = args->text,
                           .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = args->text));
}

// A slider in the row `row`: its track, the part up to the thumb in the signal and the rest in the line, the thumb a
// disc in the signal, and the value said at its right. The share is the percent of the whole, or the zoom's step of its
// steps
EMBED_INLINE void preferences_slider_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiPreferenceRow *const row = &args->values->row[preferences->first + args->row];
    const GuiRect track = EMBED_CALL(preferences_track, PreferencesCtx, .preferences = preferences, .row = args->row);
    const GuiRect control = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg,
                                       .preferences_read = preferences, .row = args->row);
    unsigned int percent = 0u;
    for (unsigned int at = 0u; (row->value[at] >= '0') && (row->value[at] <= '9'); at += 1u)
    {
        // a digit's place is far inside what an unsigned int counts
        percent = (percent * 10u) + (unsigned int)(row->value[at] - '0');
    }
    unsigned int thousandths = (percent > 100u) ? 1000u : percent * 10u;
    unsigned int step = 0u;
    if ((row->type == GUI_PREFERENCE_TYPE_ZOOM) &&
        EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = percent, .step_pointer = &step))
    {
        thousandths = (step * 1000u) / (GUI_PREFERENCE_ZOOM_STEPS - 1u);
    }
    // the thumb's place along the track is far inside what an int represents
    const int along = (int)(((unsigned int)track.width * thousandths) / 1000u);
    // The line, its ends rounded: the part before the thumb in the signal and the part after it in the line's color.
    // Each part stops the ring's width short of the thumb, so the glass shows about the thumb between the two
    const int thick = preferences->pixels.track;
    const int line_top = track.y + ((track.height - thick) / 2);
    // a mask's side is no more than GUI_SCROLLBAR_SIDE_MAX
    const int thumb = (int)preferences->thumb.width;
    const int reach = (int)preferences->thumb_ring.width / 2;
    const int before = along - reach;
    const int after = along + reach;
    if (before > 0)
    {
        EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .rect = (GuiRect){track.x, line_top, before, thick}, .clip = args->clip,
                   .color = args->palette->signal, .edge = GUI_PREFERENCES_CORNER_TRACK_OUTER,
                   .corners = PREFERENCES_CORNERS_ALL);
    }
    if (after < track.width)
    {
        EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .rect = (GuiRect){track.x + after, line_top, track.width - after, thick}, .clip = args->clip,
                   .color = args->palette->line, .edge = GUI_PREFERENCES_CORNER_TRACK_OUTER,
                   .corners = PREFERENCES_CORNERS_ALL);
    }
    EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster,
               .x = track.x + along - (thumb / 2), .y = track.y + ((track.height - thumb) / 2),
               .mask_bits = &preferences->thumb, .color = args->palette->signal, .clip = args->clip);
    const int said_left = track.x + track.width + preferences->pixels.thumb;
    (void)EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                     .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .text = row->value,
                     .rect = (GuiRect){said_left, control.y, control.x + control.width - said_left, control.height},
                     .color = args->color, .clip = args->clip);
}

// A switch in the row `row`: a box, filled in the signal with a check in it where the switch is on, and a line about
// an empty box where it is off
EMBED_INLINE void preferences_switch_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiPreferenceRow *const row = &args->values->row[preferences->first + args->row];
    const GuiRect control = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg,
                                       .preferences_read = preferences, .row = args->row);
    const int box = preferences->pixels.box;
    const GuiRect square = {control.x, control.y + ((control.height - box) / 2), box, box};
    const int on = (row->value[0] == 'o') && (row->value[1] == 'n');
    if (!on)
    {
        EMBED_CALL(preferences_box_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .rect = square, .clip = args->clip, .color = args->color, .ground = args->palette->surface,
                   .edge = GUI_PREFERENCES_CORNER_SMALL_OUTER);
        return;
    }
    EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = preferences, .raster = args->raster,
               .rect = square, .clip = args->clip, .color = args->palette->signal,
               .edge = GUI_PREFERENCES_CORNER_SMALL_OUTER, .corners = PREFERENCES_CORNERS_ALL);
    // the check: a short stroke down to the box's lower middle and a long one up to its right
    const int x0 = square.x + (box / 4);
    const int y0 = square.y + (box / 2);
    const int x1 = square.x + ((box * 2) / 5);
    const int y1 = square.y + ((box * 3) / 4);
    const int x2 = square.x + ((box * 3) / 4);
    const int y2 = square.y + (box / 4);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = x0, .y0 = y0, .x1 = x1,
               .y1 = y1, .color = args->palette->surface);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = args->raster, .x0 = x1, .y0 = y1, .x1 = x2,
               .y1 = y2, .color = args->palette->surface);
}

// A field in the row `row`: a box with a line about it, the value inside, or the text being typed with the caret
// after it. The line is in the hover's color on the row marked
EMBED_INLINE void preferences_field_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiPreferenceRow *const row = &args->values->row[preferences->first + args->row];
    const GuiRect control = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg,
                                       .preferences_read = preferences, .row = args->row);
    const int marked = preferences->marked == args->row;
    const int typing = marked && preferences->entering;
    EMBED_CALL(preferences_box_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
               .rect = control, .clip = args->clip,
               .color = typing ? args->palette->signal : (marked ? args->palette->hover : args->palette->widget_line),
               .ground = args->palette->background_fill);
    const int inset = preferences->pixels.inset * 2;
    const GuiRect inside = {control.x + inset, control.y, control.width - (2 * inset), control.height};
    const GuiRect inside_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = inside, .other = args->clip);
    if (!typing)
    {
        (void)EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                         .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                         .text = (row->value[0] != '\0') ? row->value : "not set", .rect = inside,
                         .color = (row->value[0] != '\0') ? args->color : args->palette->text_3, .clip = inside_clip);
        return;
    }
    const float end = EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences,
                                 .raster = args->raster, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                                 .text = preferences->entered, .size = preferences->entered_length, .rect = inside,
                                 .color = args->palette->text, .clip = inside_clip);
    // a place in pixels is far inside what an int represents
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){(int)end, control.y + preferences->pixels.inset, 1,
                                                   control.height - (2 * preferences->pixels.inset)},
                                  .other = inside_clip),
               .color = args->palette->caret);
}

// A button in the row `row`: its text in a box with a line about it, as wide as the text and its insets. A row of keys
// is such a button with the keys in the code's face, "Set" where it has none, and "Press the keys" while it takes them
EMBED_INLINE void preferences_button_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiPreferenceRow *const row = &args->values->row[preferences->first + args->row];
    const GuiRect control = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg,
                                       .preferences_read = preferences, .row = args->row);
    const int marked = preferences->marked == args->row;
    const int keys = row->type == GUI_PREFERENCE_TYPE_KEY;
    const char *text = row->value;
    unsigned int face = keys ? GUI_FACE_CODE : GUI_FACE_HEAD;
    if (keys && marked && preferences->capturing)
    {
        text = "Press the keys";
        face = GUI_FACE_HEAD;
    }
    else if (row->value[0] == '\0')
    {
        text = keys ? "Set" : row->label;
        face = GUI_FACE_HEAD;
    }
    const int inset = preferences->pixels.button;
    const int wide = EMBED_CALL(preferences_text_width, PreferencesCtx, .preferences = preferences,
                                .glyphs = args->glyphs, .face = face, .text = text) +
                     (2 * inset);
    const GuiRect button = {control.x, control.y, (wide < control.width) ? wide : control.width, control.height};
    // the button a pane of glass, its lip the widget's line, the hover's while marked and the signal while it takes
    // the keys; the corner's side is no more than GUI_PREFERENCES_CORNER_MAX
    EMBED_CALL(gui_glass_button.draw, GuiGlassButtonCfg, .raster = args->raster,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = button, .other = args->clip),
               .radius = preferences->corner[0][0].width, .tint = args->palette->button, .grain = args->palette->grain,
               .lip = (marked && preferences->capturing) ? args->palette->signal
                                                         : (marked ? args->palette->hover : args->palette->widget_line),
               .lip_width = 1);
    const GuiRect inside = {button.x + inset, button.y, button.width - (2 * inset), button.height};
    (void)EMBED_CALL(
        preferences_text_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
        .glyphs = args->glyphs, .face = face, .text = text, .rect = inside, .color = args->color,
        .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = inside, .other = args->clip));
}

// A line in the row `row`, which changes nothing: its value in the code's face and, after it, where the value comes
// from, quieter
EMBED_INLINE void preferences_line_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const GuiPreferenceRow *const row = &args->values->row[preferences->first + args->row];
    const GuiRect rect = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = preferences,
                                    .row = args->row);
    const int left = rect.x + preferences->pixels.label;
    const GuiRect value = {left, rect.y, rect.x + rect.width - left, rect.height};
    const GuiRect value_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = value, .other = args->clip);
    const float end = EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences,
                                 .raster = args->raster, .glyphs = args->glyphs, .face = GUI_FACE_CODE,
                                 .text = row->value, .rect = value, .color = args->color, .clip = value_clip);
    if (row->note[0] == '\0')
    {
        return;
    }
    // a place in pixels is far inside what an int represents
    const int note_left = (int)end + preferences->pixels.button;
    (void)EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                     .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .text = row->note,
                     .rect = (GuiRect){note_left, rect.y, rect.x + rect.width - note_left, rect.height},
                     .color = args->palette->text_3, .clip = value_clip);
}

// The row of a section at `row`: its label in its column, and its control beside it by the row's type. The row
// marked is brighter than the others, its label and its control's text the brightest
EMBED_INLINE void preferences_row_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const preferences = args->preferences;
    const unsigned int at = preferences->first + args->row;
    if (at >= args->values->rows)
    {
        return;
    }
    const GuiPreferenceRow *const row = &args->values->row[at];
    const GuiRect rect = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = preferences,
                                    .row = args->row);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = rect))
    {
        return;
    }
    const int marked = preferences->marked == args->row;
    const GuiRect row_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = rect, .other = args->clip);
    const GuiRect label = {rect.x, rect.y, preferences->pixels.label - preferences->pixels.button, rect.height};
    (void)EMBED_CALL(preferences_text_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                     .glyphs = args->glyphs, .face = GUI_FACE_HEAD, .text = row->label, .rect = label,
                     .color = marked ? args->palette->text : args->palette->text_3,
                     .clip =
                         EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = label, .other = row_clip));
    const GuiColor said = marked ? args->palette->text : args->palette->text_2;
    if ((row->type == GUI_PREFERENCE_TYPE_ZOOM) || (row->type == GUI_PREFERENCE_TYPE_PERCENT))
    {
        EMBED_CALL(preferences_slider_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .glyphs = args->glyphs, .palette = args->palette, .values = args->values, .row = args->row,
                   .color = said, .clip = row_clip);
    }
    else if (row->type == GUI_PREFERENCE_TYPE_SWITCH)
    {
        EMBED_CALL(preferences_switch_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .palette = args->palette, .values = args->values, .row = args->row,
                   .color = marked ? args->palette->hover : args->palette->widget_line, .clip = row_clip);
    }
    else if ((row->type == GUI_PREFERENCE_TYPE_ACTION) || (row->type == GUI_PREFERENCE_TYPE_KEY))
    {
        EMBED_CALL(preferences_button_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .glyphs = args->glyphs, .palette = args->palette, .values = args->values, .row = args->row,
                   .color = said, .clip = row_clip);
    }
    else if (row->type == GUI_PREFERENCE_TYPE_LINE)
    {
        EMBED_CALL(preferences_line_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .glyphs = args->glyphs, .palette = args->palette, .values = args->values, .row = args->row,
                   .color = said, .clip = row_clip);
    }
    else
    {
        EMBED_CALL(preferences_field_draw, PreferencesCtx, .preferences = preferences, .raster = args->raster,
                   .glyphs = args->glyphs, .palette = args->palette, .values = args->values, .row = args->row,
                   .color = said, .clip = row_clip);
    }
}

// the color `at` in its cell: its swatch, its name and its value, or what is being typed for it with the caret after it
EMBED_INLINE void preferences_cell_draw(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    unsigned int at = args->at;
    GuiRect clip = args->clip;
    const GuiRect cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg,
                                    .preferences_read = preferences, .at = at);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = cell))
    {
        return;
    }
    const int scale = preferences->scale;
    const int side =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_SWATCH, .scale = scale);
    const GuiColor color =
        EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette, .at = preferences->color_first + at);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                              .one = (GuiRect){cell.x, cell.y + ((cell.height - side) / 2), side, side}, .other = clip),
               .color = color);
    char name[GUI_PREFERENCES_TEXT];
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(preferences_color_name, PreferencesCtx, .preferences = preferences, .palette = palette, .at = at,
               .name_out = name, .size = sizeof(name));
    EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = color, .text = value,
               .size = sizeof(value));
    // a size in pixels is not negative
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PREFERENCES_CODE, .scale = scale);
    const int baseline = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size, .top = cell.y, .pixel_height = cell.height);
    const int marked = preferences->marked == at;
    const int left = cell.x + side + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 8, .scale = scale);
    const GuiRect name_clip =
        EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                   .one = (GuiRect){left, cell.y,
                                    EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                               .pixels = GUI_PREFERENCES_NAME_WIDTH, .scale = scale),
                                    cell.height},
                   .other = clip);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)left, .y = baseline, .text = name,
                     .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = name),
                     .color = marked ? palette->text : palette->text_2, .clip = name_clip);
    const float value_left = (float)(left + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_PREFERENCES_NAME_WIDTH + 5, .scale = scale));
    const GuiRect value_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){(int)value_left, cell.y, cell.x + cell.width - (int)value_left, cell.height}, .other = clip);
    if (marked && preferences->entering)
    {
        const float end = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster,
                                     .glyphs = glyphs, .face = GUI_FACE_CODE, .size = size, .x_f32 = value_left,
                                     .y = baseline, .text = preferences->entered, .length = preferences->entered_length,
                                     .color = palette->text, .clip = value_clip);
        EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                   .rect =
                       EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){(int)end,
                                                   cell.y + (EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                        .pixels = 4, .scale = scale)),
                                                   1,
                                                   cell.height - EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                            .pixels = 8, .scale = scale)},
                                  .other = value_clip),
                   .color = palette->caret);
    }
    else
    {
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size, .x_f32 = value_left, .y = baseline, .text = value,
                         .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = value),
                         .color = marked ? palette->text_2 : palette->text_3, .clip = value_clip);
    }
    unsigned int tenths = 0u;
    char under[GUI_PREFERENCES_TEXT];
    if (EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = palette,
                   .at = preferences->color_first + at, .tenths = &tenths, .under = under, .size = sizeof(under)))
    {
        char ratio[GUI_PREFERENCES_TEXT];
        EMBED_CALL(gui_palette_size.contrast_text, GuiPaletteSizeCfg, .tenths_value = tenths, .text = ratio,
                   .size = sizeof(ratio));
        const int mark_left = (int)value_left + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                           .pixels = GUI_PREFERENCES_VALUE_WIDTH, .scale = scale);
        const GuiRect mark_clip = EMBED_CALL(
            gui_raster_one.rect_intersect, GuiRasterOneCfg,
            .one = (GuiRect){mark_left, cell.y, cell.x + cell.width - mark_left, cell.height}, .other = clip);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)mark_left, .y = baseline, .text = ratio,
                         .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = ratio),
                         .color = palette->fail, .clip = mark_clip);
    }
}

// the palette `at` of the list in its row: the chosen one's name in the text color on the displayed tab's ground, its
// corners rounded; any other's in the third text color on the ground alone
EMBED_INLINE void preferences_list_row_draw(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    const GuiRect row = EMBED_CALL(gui_preferences_palette_values.list_rect, GuiPreferencesPaletteValuesCfg,
                                   .preferences_read = preferences, .at = args->at);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = row))
    {
        return;
    }
    const int scale = preferences->scale;
    const int chosen = preferences->palette_chosen == args->at;
    if (chosen)
    {
        EMBED_CALL(preferences_round_fill, PreferencesCtx, .preferences = preferences, .raster = raster, .rect = row,
                   .color = palette->tab_background, .clip = args->clip, .corners = PREFERENCES_CORNERS_ALL,
                   .edge = GUI_PREFERENCES_CORNER_SMALL_OUTER);
    }
    // a size in pixels is not negative
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PREFERENCES_SIZE, .scale = scale);
    const int left = row.x + preferences->pixels.button;
    const GuiRect name_clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){left, row.y, row.width - preferences->pixels.button, row.height}, .other = args->clip);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)left,
                     .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD, .size = size, .top = row.y, .pixel_height = row.height),
                     .text = preferences->palette_name[args->at],
                     .length =
                         EMBED_CALL(preferences_length, PreferencesCtx, .text = preferences->palette_name[args->at]),
                     .color = chosen ? palette->text : palette->text_3, .clip = name_clip);
}

// the row over the grid: the least contrast and the colors under it, each with the color it is drawn on and its
// contrast, as many whole as the row's width takes, and nothing where no color is under it
EMBED_INLINE void preferences_contrasts_draw(const PreferencesCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    GuiRect clip = args->clip;
    const GuiRect content = preferences_content(args);
    const GuiRect row = {content.x, content.y, content.width,
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL,
                                    .scale = args->preferences->scale)};
    // a size in pixels is not negative
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PREFERENCES_CODE,
                                 .scale = args->preferences->scale);
    char min[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_palette_size.contrast_text, GuiPaletteSizeCfg, .tenths_value = GUI_PALETTE_CONTRAST_MIN, .text = min,
               .size = sizeof(min));
    char output[GUI_PREFERENCES_CONTRASTS];
    VerbaLinea line = {output, sizeof(output), 0ULL};
    mmgr_line_put(&line, "Under ");
    mmgr_line_put(&line, min);
    mmgr_line_put(&line, ", the contrast WCAG 2 asks of text: ");
    const unsigned long long head = line.at;
    const unsigned int count =
        EMBED_CALL(preferences_color_count, PreferencesCtx, .preferences = args->preferences, .palette = palette);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int tenths = 0u;
        char under[GUI_PREFERENCES_TEXT];
        if (EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = palette,
                       .at = args->preferences->color_first + at, .tenths = &tenths, .under = under,
                       .size = sizeof(under)))
        {
            char name[GUI_PREFERENCES_TEXT];
            char ratio[GUI_PREFERENCES_TEXT];
            EMBED_CALL(preferences_color_name, PreferencesCtx, .preferences = args->preferences, .palette = palette,
                       .at = at, .name_out = name, .size = sizeof(name));
            EMBED_CALL(gui_palette_size.contrast_text, GuiPaletteSizeCfg, .tenths_value = tenths, .text = ratio,
                       .size = sizeof(ratio));
            const unsigned long long before = line.at;
            mmgr_line_put(&line, (before == head) ? "" : "; ");
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, " on ");
            mmgr_line_put(&line, under);
            mmgr_line_put(&line, ", ");
            mmgr_line_put(&line, ratio);
            if (EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs,
                           .face = GUI_FACE_HEAD, .size = size, .text = output, .length = line.at) > (float)row.width)
            {
                line.at = before;
                break;
            }
        }
    }
    if (line.at != head)
    {
        const unsigned long long length = line.at;
        (void)mmgr_line_finish(&line);
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)row.x,
                         .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                         .face = GUI_FACE_HEAD, .size = size, .top = row.y, .pixel_height = row.height),
                         .text = output, .length = length, .color = palette->fail,
                         .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row, .other = clip));
    }
}

// the close mark, two diagonals in a box in the middle of its rectangle, brighter where the pointer is on it
EMBED_INLINE void preferences_close_draw(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    GuiRaster *raster = args->raster;
    const GuiRect rect =
        EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = preferences);
    const int side = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CLOSE_SIDE,
                                .scale = preferences->scale);
    const int left = rect.x + ((rect.width - side) / 2);
    const int top = rect.y + ((rect.height - side) / 2);
    const GuiColor color = preferences->close_pointed ? args->palette->hover : args->palette->text_2;
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = left, .y0 = top,
               .x1 = left + side, .y1 = top + side, .color = color);
    EMBED_CALL(gui_raster_color_x_0.line, GuiRasterColorX0Cfg, .raster = raster, .x0 = left + side, .y0 = top,
               .x1 = left, .y1 = top + side, .color = color);
}

// The tabs' beads of water in the tabs' own shape, one over each tab as the main window's buttons have theirs, drawn as
// their presses are at `time_ms`, none where no buttons are passed in. The atlas is arranged for the widest tab, each
// tab's bead its own width, lit from the window's own lights: the key light over the sheet's top left corner. A tab's
// frames bake the first time it is drawn
EMBED_INLINE void preferences_tabs_beads_draw(const PreferencesCtx *args)
{
    const GuiPreferences *const tabs = args->preferences;
    if (args->buttons == NULL)
    {
        return;
    }
    const int scale = tabs->scale;
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = scale);
    const int reach = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_REACH, .scale = scale);
    const int space = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = scale);
    const int radius = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_RADIUS, .scale = scale);
    GuiRect cells[GUI_PREFERENCES_SECTIONS];
    int widest = 0;
    int tall = 0;
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        const GuiRect box = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                       .preferences_read = tabs, .at = section);
        cells[section] = (GuiRect){box.x - pad, box.y - pad, box.width - space, box.height};
        widest = (cells[section].width > widest) ? cells[section].width : widest;
        tall = box.height;
    }
    GuiLight lights[GUI_LIGHTS_MAX];
    const unsigned int light_count =
        EMBED_CALL(gui_lights_table.placed, GuiLightsCfg, .lights = lights, .width = tabs->width,
                   .height = tabs->height, .key_x = (double)tabs->pixels.edge, .key_y = (double)tabs->pixels.edge);
    // the sides are a few pixels, which an unsigned int holds; the atlas's pixels over the first segment of the
    // block's tab beads part, the program's own
    const GuiButtonsBakeCfg sizes = {.width = (unsigned int)widest,
                                     .height = (unsigned int)tall,
                                     .pad = (unsigned int)pad,
                                     .reach = (reach < 3) ? 3u : (unsigned int)reach,
                                     .tools = GUI_PREFERENCES_SECTIONS};
    unsigned char *const span = EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_TAB_BEADS,
                                           .segment = 0u, .bytes = gui_buttons_bake_table.bytes(&sizes));
    if (!EMBED_CALL(gui_buttons_bake_table.bake, GuiButtonsBakeCfg, .buttons = args->buttons, .palette = args->palette,
                    .width = sizes.width, .height = sizes.height, .pad = sizes.pad, .reach = sizes.reach,
                    .shape = GUI_BEAD_SHAPE_TAB, .radius = (unsigned int)radius, .cells = cells, .tools = sizes.tools,
                    .lights = lights, .light_count = light_count, .pixels = (GuiColor *)(void *)span))
    {
        return;
    }
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        EMBED_CALL(gui_buttons_draw_table.draw, GuiButtonsDrawCfg, .buttons = args->buttons, .raster = args->raster,
                   .tool = section, .x = cells[section].x, .y = cells[section].y, .time_ms = args->time_ms,
                   .clip = args->clip);
    }
}

void gui_preferences_draw(const GuiPreferencesPaletteValuesCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    const GuiRect clip = gui_preferences_palette_values.rect(args);
    const GuiPreferences *const tabs = args->preferences_read;
    // The lattice every pane lies on: Preferences' own, or the deep color where none is positioned. The glass covers the
    // whole window with the sheet's tint and grain, as the main window's glass lies about its work. The clear ground
    // is the same lattice with no tint and no grain
    const GuiUnder table = EMBED_CALL(gui_glass.table, GuiGlassCfg, .background_fill = args->ground,
                                      .color = palette->deep, .glass = palette->glass, .grain = palette->grain);
    const GuiUnder clear = {args->ground, 0u, NULL, 0u, palette->deep};
    // a rounding in pixels is not negative
    const unsigned int pane_radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                              .pixels = GUI_WORK_RADIUS, .scale = tabs->scale);
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = table, .rect = clip, .radius = 0u, .inner = palette->glass);
    // The sheet: one pane in the editor's ground at the editor's opacity on the clear lattice, so the controls are on
    // it as the code is in the editor. It floats in from the window's sides over the glass, as the main window's
    // work floats, and is in the work's lines. While Preferences holds the focus, the signal is along its head and
    // the work's left line down its left, as the main window's work shows the focus; the window's own border is the
    // compositor's, in the system's color
    const int margin = tabs->pixels.edge / 2;
    const GuiRect sheet = {clip.x + margin, clip.y + margin, clip.width - (2 * margin), clip.height - (2 * margin)};
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = clear, .rect = sheet, .radius = pane_radius, .inner = palette->editor_background);
    const GuiColor work_line = palette->work_line;
    const GuiOutline outline = {
        tabs->focused ? palette->signal : work_line,
        work_line,
        work_line,
        tabs->focused ? palette->work_left : work_line,
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_TOP, .scale = tabs->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_RIGHT, .scale = tabs->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_BOTTOM, .scale = tabs->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_LEFT, .scale = tabs->scale)};
    const GuiRect inside = {sheet.x + outline.left_width, sheet.y + outline.top_width,
                            sheet.width - outline.left_width - outline.right_width,
                            sheet.height - outline.top_width - outline.bottom_width};
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = raster, .work = sheet,
               .inside = inside, .radius = pane_radius, .lines = &outline, .holes = NULL, .count = 0u, .table = table,
               .clear = clear);
    // a size in pixels is not negative; the names in the editor's tabs' face and size
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_TAB_SIZE, .scale = tabs->scale);
    // The tabs, as the editor's tabs are in the main window's work: a line along the row's foot and one at each
    // tab's right, and the tab displayed on the tab's ground, its corners rounded, the tab's line along its top and down
    // its left
    const int one = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = tabs->scale);
    const int space =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TAB_SPACE, .scale = tabs->scale);
    // a rounding in pixels is not negative
    const unsigned int tab_radius = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = GUI_TAB_RADIUS, .scale = tabs->scale);
    const GuiRect first_tab = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                         .preferences_read = tabs, .at = 0u);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect = (GuiRect){inside.x, first_tab.y + first_tab.height - one, inside.width, one},
               .color = palette->line);
    // the name's room each side in its tab: half the editor's tab's padding, left and right together
    const int name_room = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                     .pixels = GUI_TAB_PAD_LEFT + GUI_TAB_PAD_RIGHT, .scale = tabs->scale) /
                          2;
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        const GuiRect box = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                       .preferences_read = tabs, .at = section);
        const int displayed = section == tabs->section;
        // each tab a box as the editor's tabs are: its top corners rounded, its foot open on the row's line, a line
        // one pixel wide up its sides and over its top, and a gap before the next
        const GuiRect tab = {box.x, box.y, box.width - space, box.height};
        const unsigned int top = GUI_RASTER_CORNER_TOP_LEFT | GUI_RASTER_CORNER_TOP_RIGHT;
        if (displayed)
        {
            EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = tab,
                       .radius = tab_radius, .color = palette->tab_background, .corners = top);
        }
        EMBED_CALL(gui_raster_rect_color.round, GuiRasterRectColorCfg, .raster = raster, .rect = tab,
                   .radius = tab_radius, .color = displayed ? palette->tab_line : palette->widget_line, .corners = top,
                   .stroke = one);
        // the name centered in its tab, the same room each side
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_CODE, .size = size, .x_f32 = (float)(tab.x + name_room),
                         .y = EMBED_CALL(gui_glyphs_face.baseline, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                         .face = GUI_FACE_CODE, .size = size, .top = box.y, .pixel_height = box.height),
                         .text = s_sections[section],
                         .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = s_sections[section]),
                         .color = displayed ? palette->text : palette->text_2, .clip = clip);
    }
    EMBED_CALL(preferences_tabs_beads_draw, PreferencesCtx, .preferences = tabs, .buttons = args->buttons,
               .raster = raster, .palette = palette, .clip = clip, .time_ms = args->time_ms);
    EMBED_CALL(preferences_close_draw, PreferencesCtx, .preferences = tabs, .raster = raster, .palette = palette);
    if (args->preferences_read->section == GUI_PREFERENCES_COLORS)
    {
        EMBED_CALL(preferences_contrasts_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
                   .glyphs = glyphs, .palette = palette, .clip = clip);
        for (unsigned int at = 0u; at < args->preferences_read->palettes; at += 1u)
        {
            EMBED_CALL(preferences_list_row_draw, PreferencesCtx, .preferences = args->preferences_read,
                       .raster = raster, .glyphs = glyphs, .palette = palette, .at = at, .clip = clip);
        }
        const unsigned int count = EMBED_CALL(preferences_color_count, PreferencesCtx,
                                              .preferences = args->preferences_read, .palette = palette);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            EMBED_CALL(preferences_cell_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
                       .glyphs = glyphs, .palette = palette, .at = at, .clip = clip);
        }
        // the scrollbar beside the grid only where its rows are more than the display shows
        const unsigned int grid_rows = EMBED_CALL(preferences_scroll_total, PreferencesCtx,
                                                  .preferences = args->preferences_read, .palette = palette);
        const unsigned int grid_displayed =
            EMBED_CALL(preferences_scroll_displayed, PreferencesCtx, .preferences = args->preferences_read);
        if (grid_rows > grid_displayed)
        {
            EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg, .bar_read = &args->preferences_read->scrollbar,
                       .raster = raster, .palette = palette, .total = grid_rows, .displayed = grid_displayed,
                       .first = args->preferences_read->top, .pointed = args->preferences_read->scrolling);
        }
        return;
    }
    const GuiPreferences *const preferences = args->preferences_read;
    const unsigned int lines = EMBED_CALL(preferences_lines_in_view, PreferencesCtx, .preferences = preferences);
    for (unsigned int row = preferences->top; (row < preferences->rows) && (row < preferences->top + lines); row += 1u)
    {
        EMBED_CALL(preferences_row_draw, PreferencesCtx, .preferences = preferences, .raster = raster, .glyphs = glyphs,
                   .palette = palette, .values = args->values, .row = row, .clip = clip);
    }
    // the scrollbar beside the rows only where they are more than the section shows
    if (preferences->rows > lines)
    {
        EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg, .bar_read = &preferences->scrollbar, .raster = raster,
                   .palette = palette, .total = preferences->rows, .displayed = lines, .first = preferences->top,
                   .pointed = preferences->scrolling);
    }
}
