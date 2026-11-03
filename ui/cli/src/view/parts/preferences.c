// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preferences.c: Preferences. Nothing here holds a setting: the window hands in what the rows stand at and acts on the
// change each key or press answers. A row lays text alone, and how bright its words and its value stand is all that
// marks it
#include "preferences.h"

#include "../../command_line/cli_internal.h"

// the zoom's steps in percent; the sections' names; and what each row of Settings says before its value
static const unsigned int s_zoom_percent[GUI_PREFERENCE_ZOOM_STEPS] = {50u,  67u,  75u,  80u,  90u,  100u, 110u,
                                                                       125u, 150u, 175u, 200u, 250u, 300u};
static const char *const s_sections[GUI_PREFERENCES_SECTIONS] = {"Settings", "Colors"};
static const char *const s_output[GUI_PREFERENCE_ROWS] = {"Zoom is ", "Bar opacity is ", "Editor opacity is ",
                                                          "Reduce Motion is "};

// the most bytes a color's value takes as it is typed, its mark and eight digits
#define PREFERENCES_ENTERED_MOST 9u

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
} PreferencesCtx;
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
    mmgr_line_put(&words, s_output[(args->row < GUI_PREFERENCE_ROWS) ? args->row : 0u]);
    (void)mmgr_line_finish(&words);
    VerbaLinea line = {args->value, args->size, 0ULL};
    if (args->row == GUI_PREFERENCE_REDUCE)
    {
        mmgr_line_put(&line, args->values->reduced ? "on" : "off");
    }
    else
    {
        unsigned int percent;
        if (args->row == GUI_PREFERENCE_ZOOM)
        {
            percent = EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                 .step = args->values->zoom_step);
        }
        else if (args->row == GUI_PREFERENCE_BAR)
        {
            percent = args->values->bar_percent;
        }
        else
        {
            percent = args->values->editor_percent;
        }
        mmgr_line_decimal(&line, percent, 1u);
        mmgr_line_ch(&line, '%');
    }
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
    // sizes at a scale are not negative
    *width = (unsigned int)((2 * edge) +
                            EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_LIST,
                                       .scale = scale) +
                            (2 * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                            .pixels = GUI_PREFERENCES_CELL_WIDTH, .scale = scale)));
    *height = (unsigned int)((2 * edge) + (rows * EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                             .pixels = GUI_PREFERENCES_CELL, .scale = scale)));
}

GuiRect gui_preferences_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    // a client's sides are far inside what an int represents
    return (GuiRect){0, 0, (int)args->preferences_read->width, (int)args->preferences_read->height};
}

GuiRect gui_preferences_close_rect(const GuiPreferencesRowCfg *args)
{
    const int pixel_width = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CLOSE_WIDTH,
                                       .scale = args->preferences_read->scale);
    // a client's side is far inside what an int represents
    return (GuiRect){(int)args->preferences_read->width - pixel_width, 0, pixel_width,
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                .scale = args->preferences_read->scale)};
}

GuiRect gui_preferences_section_rect(const GuiPreferencesPaletteValuesCfg *args)
{
    const GuiRect rect = gui_preferences_palette_values.rect(args);
    const int row = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW,
                               .scale = args->preferences_read->scale);
    // a section's place is far inside what an int represents
    return (GuiRect){rect.x + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                         .scale = args->preferences_read->scale),
                     rect.y +
                         EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                    .scale = args->preferences_read->scale) +
                         ((int)args->at * row),
                     EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                .pixels = GUI_PREFERENCES_LIST - GUI_PREFERENCES_EDGE,
                                .scale = args->preferences_read->scale),
                     row};
}

// what a section shows stands in: past the list, the edge saved all round
EMBED_INLINE GuiRect preferences_content(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    const GuiRect rect = EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg,
                                    .preferences_read = preferences);
    const int edge = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                .scale = preferences->scale);
    const int list = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_LIST,
                                .scale = preferences->scale);
    return (GuiRect){rect.x + edge + list, rect.y + edge, rect.width - (2 * edge) - list, rect.height - (2 * edge)};
}

GuiRect gui_preferences_row_rect(const GuiPreferencesRowCfg *args)
{
    const GuiRect content = EMBED_CALL(preferences_content, PreferencesCtx, .preferences = args->preferences_read);
    const int pixel_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_ROW,
                                        .scale = args->preferences_read->scale);
    // a row's place is far inside what an int represents
    return (GuiRect){content.x, content.y + ((int)args->row * pixel_height), content.width, pixel_height};
}

// what the colors' grid stands in: what a section shows, less the row over it that says which colors are under the
// least contrast
EMBED_INLINE GuiRect preferences_grid(const PreferencesCtx *args)
{
    const GuiRect content = preferences_content(args);
    const int row = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL,
                               .scale = args->preferences->scale);
    return (GuiRect){content.x, content.y + row, content.width, content.height - row};
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

void gui_preferences_resize(const GuiPreferencesWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    int scale = args->scale;
    args->preferences->width = width;
    args->preferences->height = height;
    args->preferences->scale = scale;
    args->preferences->top = 0u;
    EMBED_CALL(preferences_mark_in_view, PreferencesCtx, .preferences_out = args->preferences);
}

// the section `section` shown, its first thing marked
EMBED_INLINE void preferences_section_show(const PreferencesCtx *args)
{
    unsigned int section = args->section;
    args->preferences_out->section = section;
    args->preferences_out->marked = 0u;
    args->preferences_out->top = 0u;
    args->preferences_out->entering = 0;
    args->preferences_out->entered_length = 0u;
}

// the keys while a color's value is being typed
EMBED_INLINE void preferences_entering_key(const PreferencesCtx *args)
{
    const GuiPalette *palette = args->palette;
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Escape"))
    {
        args->preferences_out->entering = 0;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Backspace"))
    {
        args->preferences_out->entered_length -= (args->preferences_out->entered_length != 0u) ? 1u : 0u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter") &&
             EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = args->preferences_out->entered,
                        .length = args->preferences_out->entered_length,
                        .current = EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette,
                                              .at = args->preferences_out->marked),
                        .color_pointer = &args->change->color))
    {
        args->change->kind = GUI_PREFERENCE_COLORED;
        args->change->at = args->preferences_out->marked;
        args->preferences_out->entering = 0;
    }
}

// the keys over the colors' grid: the mark moved among the colors, no further than their ends
EMBED_INLINE void preferences_colors_key(const PreferencesCtx *args)
{
    GuiPreferences *preferences = args->preferences_out;
    const GuiPalette *palette = args->palette;
    const unsigned int across = EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg,
                                           .preferences_read = preferences);
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
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

// the keys over the rows of Settings
EMBED_INLINE void preferences_settings_key(const PreferencesCtx *args)
{
    if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Down") &&
        (args->preferences_out->marked + 1u < GUI_PREFERENCE_ROWS))
    {
        args->preferences_out->marked += 1u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Up") &&
             (args->preferences_out->marked != 0u))
    {
        args->preferences_out->marked -= 1u;
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Right") ||
             EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Left") ||
             EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter"))
    {
        args->change->kind = GUI_PREFERENCE_STEPPED;
        args->change->at = args->preferences_out->marked;
        args->change->by = EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Left") ? -1 : 1;
        args->change->round = EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Enter");
    }
}

void gui_preferences_key(const GuiPreferencesPaletteNameCfg *args)
{
    args->change->kind = GUI_PREFERENCE_NOTHING;
    args->change->at = 0u;
    args->change->by = 0;
    args->change->round = 0;
    args->change->color = 0u;
    if (args->preferences->entering)
    {
        EMBED_CALL(preferences_entering_key, PreferencesCtx, .preferences_out = args->preferences,
                   .palette = args->palette, .name = args->name, .change = args->change);
    }
    else if (EMBED_CALL(preferences_key, PreferencesCtx, .name = args->name, .key = "Escape"))
    {
        args->preferences->displayed = 0;
        args->change->kind = GUI_PREFERENCE_CLOSED;
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
                   .palette = args->palette, .name = args->name);
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
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const char letter = args->text[at];
        const int digit = ((letter >= '0') && (letter <= '9')) || ((letter >= 'a') && (letter <= 'f')) ||
                          ((letter >= 'A') && (letter <= 'F'));
        const int mark = (letter == '#') && (args->preferences->entered_length == 0u);
        if ((digit || mark) && (args->preferences->entered_length < PREFERENCES_ENTERED_MOST))
        {
            args->preferences->entered[args->preferences->entered_length] = letter;
            args->preferences->entered_length += 1u;
        }
    }
    return 1;
}

int gui_preferences_press(const GuiPreferencesPaletteChangeCfg *args)
{
    const GuiPalette *palette = args->palette;
    args->change->kind = GUI_PREFERENCE_NOTHING;
    args->change->at = 0u;
    args->change->by = 0;
    args->change->round = 0;
    args->change->color = 0u;
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg,
                                      .preferences_read = args->preferences),
                   .x = args->x, .y = args->y))
    {
        args->preferences->displayed = 0;
        args->change->kind = GUI_PREFERENCE_CLOSED;
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
            return 1;
        }
    }
    if (args->preferences->section == GUI_PREFERENCES_COLORS)
    {
        const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
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
        return 0;
    }
    for (unsigned int row = 0u; row < GUI_PREFERENCE_ROWS; row += 1u)
    {
        if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                       .rect = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg,
                                          .preferences_read = args->preferences, .row = row),
                       .x = args->x, .y = args->y))
        {
            args->preferences->marked = row;
            args->change->kind = GUI_PREFERENCE_STEPPED;
            args->change->at = row;
            args->change->by = 1;
            args->change->round = 1;
            return 1;
        }
    }
    return 0;
}

int gui_preferences_move(const GuiPreferencesPaletteChangeCfg *args)
{
    const GuiPalette *palette = args->palette;
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
    const unsigned int count = colors
                                   ? EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette)
                                   : GUI_PREFERENCE_ROWS;
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

int gui_preferences_wheel(const GuiPreferencesPaletteCfg *args)
{
    const GuiPalette *palette = args->palette;
    if (args->preferences->section != GUI_PREFERENCES_COLORS)
    {
        return 0;
    }
    const unsigned int across = EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg,
                                           .preferences_read = args->preferences);
    const unsigned int total =
        (EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette) + across - 1u) / across;
    const unsigned int in_view = EMBED_CALL(preferences_rows_in_view, PreferencesCtx, .preferences = args->preferences);
    const unsigned int most = (total > in_view) ? total - in_view : 0u;
    // the first row and the rows moved are far inside what an int represents
    const int required = (int)args->preferences->top + args->rows;
    unsigned int top;
    if (required < 0)
    {
        top = 0u;
    }
    else if ((unsigned int)required > most)
    {
        top = most;
    }
    else
    {
        top = (unsigned int)required;
    }
    const int moved = top != args->preferences->top;
    args->preferences->top = top;
    return moved;
}

// the baseline of a row of text of `size` in the middle of `rect`
EMBED_INLINE int preferences_baseline(const PreferencesCtx *args)
{
    const GuiGlyphs *glyphs = args->glyphs;
    unsigned int face = args->face;
    unsigned int size = args->size;
    const float ascent =
        EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    const float descent =
        EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs, .face = face, .size = size);
    // a row's baseline is far inside what an int represents
    return args->rect.y + (int)((((float)args->rect.height - ascent - descent) / 2.0f) + ascent + 0.5f);
}

// the row of Settings at `row`: the words and its value, the value brighter than the words and the row marked brighter
// than any
EMBED_INLINE void preferences_row_draw(const PreferencesCtx *args)
{
    const GuiPreferences *preferences = args->preferences;
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPreferenceValues *values = args->values;
    unsigned int row = args->row;
    GuiRect clip = args->clip;
    char output[GUI_PREFERENCES_TEXT];
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = values, .row = row,
               .output = output, .value = value, .size = sizeof(output));
    const GuiRect rect =
        EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = preferences, .row = row);
    // a size in pixels is not negative
    const unsigned int size = (unsigned int)EMBED_CALL(
        gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PREFERENCES_SIZE, .scale = preferences->scale);
    const int baseline = EMBED_CALL(preferences_baseline, PreferencesCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                    .size = size, .rect = rect);
    const int marked = preferences->marked == row;
    const float after =
        EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                   .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)rect.x, .y = baseline, .text = output,
                   .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = output),
                   .color = marked ? args->palette->text_2 : args->palette->text_3, .clip = clip);
    (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                     .face = GUI_FACE_HEAD, .size = size, .x_f32 = after, .y = baseline, .text = value,
                     .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = value),
                     .color = marked ? args->palette->text : args->palette->text_2, .clip = clip);
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
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
               .rect =
                   EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                              .one = (GuiRect){cell.x, cell.y + ((cell.height - side) / 2), side, side}, .other = clip),
               .color = EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette, .at = at));
    char name[GUI_PREFERENCES_TEXT];
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = palette, .at = at, .name = name,
               .size = sizeof(name));
    EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg,
               .color = EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = palette, .at = at),
               .text = value, .size = sizeof(value));
    // a size in pixels is not negative
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = (int)GUI_PREFERENCES_CODE, .scale = scale);
    const int baseline = EMBED_CALL(preferences_baseline, PreferencesCtx, .glyphs = glyphs, .face = GUI_FACE_CODE,
                                    .size = size, .rect = cell);
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
    if (EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = palette, .at = at, .tenths = &tenths,
                   .under = under, .size = sizeof(under)))
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
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int tenths = 0u;
        char under[GUI_PREFERENCES_TEXT];
        if (EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = palette, .at = at,
                       .tenths = &tenths, .under = under, .size = sizeof(under)))
        {
            char name[GUI_PREFERENCES_TEXT];
            char ratio[GUI_PREFERENCES_TEXT];
            EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = palette, .at = at, .name = name,
                       .size = sizeof(name));
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
                         .y = EMBED_CALL(preferences_baseline, PreferencesCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                         .size = size, .rect = row),
                         .text = output, .length = length, .color = palette->fail,
                         .clip = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = row, .other = clip));
    }
}

// the close mark, two diagonals in a box in the middle of its rectangle, brighter where the pointer stands on it
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

void gui_preferences_draw(const GuiPreferencesPaletteValuesCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    const GuiRect clip = gui_preferences_palette_values.rect(args);
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster, .rect = clip,
               .color = palette->surface);
    EMBED_CALL(preferences_close_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
               .palette = palette);
    // a size in pixels is not negative
    const unsigned int size =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = (int)GUI_PREFERENCES_SIZE,
                                 .scale = args->preferences_read->scale);
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        const GuiRect rect = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                        .preferences_read = args->preferences_read, .at = section);
        const GuiColor color = (section == args->preferences_read->section) ? palette->text : palette->text_3;
        (void)EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD, .size = size, .x_f32 = (float)rect.x,
                         .y = EMBED_CALL(preferences_baseline, PreferencesCtx, .glyphs = glyphs, .face = GUI_FACE_HEAD,
                                         .size = size, .rect = rect),
                         .text = s_sections[section],
                         .length = EMBED_CALL(preferences_length, PreferencesCtx, .text = s_sections[section]),
                         .color = color, .clip = clip);
    }
    if (args->preferences_read->section == GUI_PREFERENCES_COLORS)
    {
        EMBED_CALL(preferences_contrasts_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
                   .glyphs = glyphs, .palette = palette, .clip = clip);
        const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            EMBED_CALL(preferences_cell_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
                       .glyphs = glyphs, .palette = palette, .at = at, .clip = clip);
        }
        return;
    }
    for (unsigned int row = 0u; row < GUI_PREFERENCE_ROWS; row += 1u)
    {
        EMBED_CALL(preferences_row_draw, PreferencesCtx, .preferences = args->preferences_read, .raster = raster,
                   .glyphs = glyphs, .palette = palette, .values = args->values, .row = row, .clip = clip);
    }
}
