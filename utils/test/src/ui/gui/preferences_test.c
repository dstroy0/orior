// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preferences_test.c: Preferences's check: the zoom's steps and the opacities' ends, each row a sentence, the keys and presses
// marking and moving what they name, one section drawn at a time with how bright each thing stands telling them apart, and the
// colors' grid, a value typed as hex taken and one that is no color refused
#include "../../../../../ui/cli/src/view/parts/preferences.h"
#include "../cli/cli_check.h"

// the bytes of `text`
static unsigned long long text_length(const char *text)
{
    unsigned long long length = 0ull;
    while (text[length] != '\0')
    {
        length += 1ull;
    }
    return length;
}

// whether two texts are the same
static int same_text(const char *one, const char *other)
{
    const unsigned long long length = text_length(one);
    return (length == text_length(other)) && (memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = length}) == 0);
}

// whether a pixel of `rect` differs from `background_fill`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor background_fill)
{
    for (int row = rect.y; row < rect.y + rect.height; row += 1)
    {
        for (int column = rect.x; column < rect.x + rect.width; column += 1)
        {
            // the place lies on the raster in every check here
            if (raster->pixels[((unsigned int)row * raster->width) + (unsigned int)column] != background_fill)
            {
                return 1;
            }
        }
    }
    return 0;
}

// the brightest of the pixels of `rect`: the greatest sum of a pixel's channels
static unsigned int brightest(const GuiRaster *raster, GuiRect rect)
{
    unsigned int most = 0u;
    for (int row = rect.y; row < rect.y + rect.height; row += 1)
    {
        for (int column = rect.x; column < rect.x + rect.width; column += 1)
        {
            // the place lies on the raster in every check here
            const GuiColor pixel = raster->pixels[((unsigned int)row * raster->width) + (unsigned int)column];
            const unsigned int sum = GUI_RED_OF(pixel) + GUI_GREEN_OF(pixel) + GUI_BLUE_OF(pixel);
            most = (sum > most) ? sum : most;
        }
    }
    return most;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    static GuiGlyphs glyphs;
    cli_check(EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why), "the glyphs are read");
    static GuiPaletteOwn own;
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &own, .scheme = GUI_SCHEME_DARK);
    const GuiPalette *const palette = &own.colors;

    // the zoom's steps and the opacities' ends
    cli_check(EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg, .step = GUI_PREFERENCE_ZOOM_DEFAULT_STEP) == 100u, "the whole zoom is a step");
    unsigned int step = 0u;
    cli_check(EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = 125u, .step_pointer = &step) && (EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg, .step = step) == 125u), "a percent that is a step names its step");
    cli_check(!EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = 130u, .step_pointer = &step), "a percent that is no step names none");
    cli_check(EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg, .step = 0u, .by = -1, .round = 0) == 0u, "the zoom stops at its least");
    cli_check(EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg, .step = GUI_PREFERENCE_ZOOM_STEPS - 1u, .by = 1, .round = 0) == GUI_PREFERENCE_ZOOM_STEPS - 1u, "and at its most");
    cli_check(EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg, .step = GUI_PREFERENCE_ZOOM_STEPS - 1u, .by = 1, .round = 1) == 0u, "or runs round where asked");
    cli_check(EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg, .percent = 30u, .by = 1, .round = 0) == 40u, "an opacity moves a step");
    cli_check(EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg, .percent = 100u, .by = 1, .round = 0) == 100u, "and stops at its most");
    cli_check(EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg, .percent = 100u, .by = 1, .round = 1) == 0u, "or runs round where asked");
    cli_check(EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg, .percent = 0u, .by = -1, .round = 0) == 0u, "and stops at none");

    // each row a sentence, the page's own label and its value
    const GuiPreferenceValues values = {GUI_PREFERENCE_ZOOM_DEFAULT_STEP, 30u, 80u, 0};
    char output[GUI_PREFERENCES_TEXT];
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = GUI_PREFERENCE_ZOOM, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Zoom is ") && same_text(value, "100%"), "the zoom reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = GUI_PREFERENCE_BAR, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Bar opacity is ") && same_text(value, "30%"), "the bar's opacity reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = GUI_PREFERENCE_EDITOR, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Editor opacity is ") && same_text(value, "80%"), "the editor's opacity reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = GUI_PREFERENCE_REDUCE, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Reduce Motion is ") && same_text(value, "off"), "less motion reads as a sentence");

    // the keys over Settings
    static GuiPreferences preferences;
    EMBED_CALL(gui_preferences_size.init, GuiPreferencesSizeCfg, .preferences = &preferences);
    cli_check(!preferences.displayed && (preferences.section == GUI_PREFERENCES_SETTINGS), "it starts closed at its first section");
    preferences.displayed = 1;
    // its own window: what it covers is the client whole, at the size that shows every color in two columns, and at a scale
    unsigned int requested_width = 0u;
    unsigned int requested_height = 0u;
    EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = palette, .scale = 100, .width_pointer = &requested_width, .height_pointer = &requested_height);
    cli_check((requested_width == 856u) && (requested_height == 1008u), "the size it asks for shows every color in two columns");
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .width = requested_width, .height = requested_height, .scale = 100);
    cli_check(EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences) == 2u, "at that size the grid has two columns");
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette) - 1u)), "and the last color in view");
    EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = palette, .scale = 200, .width_pointer = &requested_width, .height_pointer = &requested_height);
    cli_check((requested_width == 1712u) && (requested_height == 2016u), "the size follows the scale");
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .width = 1200u, .height = 800u, .scale = 100);
    const GuiRect whole = EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences);
    cli_check((whole.x == 0) && (whole.y == 0) && (whole.width == 1200) && (whole.height == 800), "it covers its window's client");
    GuiPreferenceChange change;
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    cli_check((preferences.marked == GUI_PREFERENCE_BAR) && (change.kind == GUI_PREFERENCE_NOTHING), "Down marks the next row");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Right", .change = &change);
    cli_check((change.kind == GUI_PREFERENCE_STEPPED) && (change.at == GUI_PREFERENCE_BAR) && (change.by == 1), "Right moves its value on");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Left", .change = &change);
    cli_check((change.kind == GUI_PREFERENCE_STEPPED) && (change.by == -1), "Left moves it back");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Up", .change = &change);
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Up", .change = &change);
    cli_check(preferences.marked == GUI_PREFERENCE_ZOOM, "Up stops at the first row");
    for (unsigned int down = 0u; down < GUI_PREFERENCE_ROWS + 2u; down += 1u)
    {
        EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    }
    cli_check(preferences.marked == GUI_PREFERENCE_REDUCE, "Down stops at the last");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Q", .change = &change);
    cli_check(change.kind == GUI_PREFERENCE_NOTHING, "a key that is none of its own changes nothing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);
    cli_check((change.kind == GUI_PREFERENCE_CLOSED) && !preferences.displayed, "Escape closes it");
    preferences.displayed = 1;

    // a press moves a row on and round, and shows a section
    const GuiRect row = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_EDITOR);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = row.x + 5, .y = row.y + 5, .change = &change) && (change.kind == GUI_PREFERENCE_STEPPED) &&
                  (change.at == GUI_PREFERENCE_EDITOR) && (change.by == 1) && (preferences.marked == GUI_PREFERENCE_EDITOR),
              "a press on a row marks it and moves it a step");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM).x + 5, .y = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM).y + 5) &&
                  (preferences.marked == GUI_PREFERENCE_ZOOM),
              "the pointer on a row marks it");
    cli_check(!EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM).x + 6, .y = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM).y + 5), "and moving on it changes nothing");
    cli_check(!EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = 1, .y = 700, .change = &change) && (change.kind == GUI_PREFERENCE_NOTHING), "a press on its ground changes nothing");
    // the close mark at its head's right end
    const GuiRect close_mark = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = &preferences);
    cli_check((close_mark.x + close_mark.width == 1200) && (close_mark.y == 0) && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = close_mark), "the close mark stands at the head's right end");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = close_mark.x + 4, .y = close_mark.y + 4) && preferences.close_pointed, "the pointer on the close mark marks it");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = 1, .y = 700) && !preferences.close_pointed, "and moving off it lets it go");
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = close_mark.x + 4, .y = close_mark.y + 4, .change = &change) && (change.kind == GUI_PREFERENCE_CLOSED) && !preferences.displayed, "a press on the close mark closes it");
    preferences.displayed = 1;

    // one section drawn at a time, how bright each thing stands telling them apart
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1200u, .height = 800u), "a raster to draw on");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1200, 800}, .color = palette->background_fill);
    const GuiPreferenceValues displayed_values = {GUI_PREFERENCE_ZOOM_DEFAULT_STEP, 30u, 80u, 1};
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .raster = &raster, .glyphs = &glyphs, .palette = palette, .values = &displayed_values);
    cli_check(!painted(&raster, (GuiRect){2, 760, 40, 30}, palette->surface), "its window's ground is the widget's");
    const unsigned int section_on = brightest(&raster, EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_SETTINGS));
    const unsigned int section_off = brightest(&raster, EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_COLORS));
    cli_check(section_on > section_off, "the section shown stands brighter than the quiet list");
    cli_check(section_off > brightest(&raster, (GuiRect){EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences).x + 2, EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences).y + 2, 4, 4}), "and the list brighter than the ground");
    const unsigned int marked_row = brightest(&raster, EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM));
    const unsigned int other_row = brightest(&raster, EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_BAR));
    cli_check(marked_row > other_row, "the row marked stands brighter than the others");
    cli_check(other_row > 0u, "and every row is drawn");
    // no ground or line under a row: the row's far end stands as the ground stood
    const GuiRect far = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = GUI_PREFERENCE_ZOOM);
    cli_check(brightest(&raster, (GuiRect){far.x + far.width - 6, far.y, 4, far.height}) == brightest(&raster, (GuiRect){2, 760, 1, 1}), "a row lays no ground or line about itself");

    // the colors' grid
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageDown", .change = &change);
    cli_check((preferences.section == GUI_PREFERENCES_COLORS) && (preferences.marked == 0u), "PageDown shows the next section, its first thing marked");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageDown", .change = &change);
    cli_check(preferences.section == GUI_PREFERENCES_COLORS, "and stops at the last");
    const unsigned int across = EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences);
    cli_check(across >= 2u, "the grid stands in columns");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Right", .change = &change);
    cli_check(preferences.marked == 1u, "Right marks the color beside");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    cli_check(preferences.marked == 1u + across, "Down marks the color below");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Up", .change = &change);
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Left", .change = &change);
    cli_check(preferences.marked == 0u, "Up and Left go back");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1200, 800}, .color = palette->background_fill);
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .raster = &raster, .glyphs = &glyphs, .palette = palette, .values = &displayed_values);
    const GuiRect first_cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = 0u);
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = first_cell), "the first color has its cell");
    // the swatch of the window's ground shows on the widget's; the widget's own would not
    const GuiRect second_cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = 1u);
    const GuiColor swatch = raster.pixels[((unsigned int)(first_cell.y + (first_cell.height / 2)) * raster.width) + (unsigned int)(first_cell.x + 7)];
    cli_check(swatch == palette->background_fill, "a color's swatch stands in its color");

    // the contrast marks: over the grid a row says which colors are under the least, and each such color's cell ends in its ratio
    const GuiRect colors_row = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_SETTINGS);
    cli_check(first_cell.y == colors_row.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL, .scale = 100), "the grid stands a row under the section's head");
    const GuiRect contrasts = {first_cell.x, colors_row.y, (first_cell.width * 2), EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL, .scale = 100)};
    cli_check(painted(&raster, contrasts, palette->surface), "the row over the grid says the colors under the least");
    const int mark_left = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_SWATCH + 8 + GUI_PREFERENCES_NAME_WIDTH + 5 + GUI_PREFERENCES_VALUE_WIDTH, .scale = 100);
    unsigned int low_at = 0u;
    unsigned int low_tenths = 0u;
    char low_under[GUI_PREFERENCES_TEXT];
    while ((low_at < EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette)) && !EMBED_CALL(gui_palette_at_size.name_low, GuiPaletteAtSizeCfg, .palette = palette, .at = low_at, .tenths = &low_tenths, .under = low_under, .size = sizeof(low_under)))
    {
        low_at += 1u;
    }
    const GuiRect low_cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = low_at);
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = low_cell) && painted(&raster, (GuiRect){low_cell.x + mark_left, low_cell.y, low_cell.width - mark_left, low_cell.height}, palette->surface), "a color under the least ends in its ratio");
    cli_check(!painted(&raster, (GuiRect){second_cell.x + mark_left, second_cell.y, second_cell.width - mark_left, second_cell.height}, palette->surface), "a color over it does not");
    static GuiPaletteOwn bright;
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &bright, .scheme = GUI_SCHEME_DARK);
    bright.colors.number = bright.colors.text;
    for (unsigned int token = 0u; token < bright.colors.token_count; token += 1u)
    {
        bright.token[token].color = bright.colors.text;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1200, 800}, .color = palette->background_fill);
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .raster = &raster, .glyphs = &glyphs, .palette = &bright.colors, .values = &displayed_values);
    cli_check(!painted(&raster, contrasts, bright.colors.surface), "with none under the least the row says nothing");

    // typing a value
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check(preferences.entering && (preferences.entered_length == 0u), "Enter begins typing a value");
    cli_check(EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "#12a", .size = 4ull) && (preferences.entered_length == 4u), "the hex typed is taken");
    cli_check(EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "z", .size = 1ull) && (preferences.entered_length == 4u), "what is no hex is passed over");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Backspace", .change = &change);
    cli_check(preferences.entered_length == 3u, "Backspace takes the last off");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check(preferences.entering && (change.kind == GUI_PREFERENCE_NOTHING), "what is no color is not taken and the typing goes on");
    (void)EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "a4c5e6", .size = 6ull);
    cli_check(preferences.entered_length == 7u + 2u, "typing goes on after it");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);
    cli_check(!preferences.entering && preferences.displayed && (change.kind == GUI_PREFERENCE_NOTHING), "Escape leaves the typing, Preferences staying");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    (void)EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "#a1b2c3", .size = 7ull);
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check((change.kind == GUI_PREFERENCE_COLORED) && (change.at == 0u) && (change.color == GUI_RGB(0xa1u, 0xb2u, 0xc3u)) && !preferences.entering, "a color typed is taken");
    const GuiRect pressed_cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = 3u);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = pressed_cell.x + 5, .y = pressed_cell.y + 5, .change = &change) && (preferences.marked == 3u) && preferences.entering, "a press on a cell marks it and begins typing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);

    // the grid scrolled, by the wheel and by the keys saving the mark in view
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .width = 960u, .height = 400u, .scale = 100);
    preferences.marked = 0u;
    cli_check(EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &preferences, .palette = palette, .rows = 2) && (preferences.top == 2u), "the wheel moves the grid");
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = 0u)), "a color above the view has no cell");
    cli_check(EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &preferences, .palette = palette, .rows = -9) && (preferences.top == 0u), "the wheel stops at the top");
    cli_check(!EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &preferences, .palette = palette, .rows = -9), "and moves nothing past it");
    for (unsigned int down = 0u; down < count; down += 1u)
    {
        EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    }
    cli_check(preferences.marked < count, "the mark stays among the colors");
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = preferences.marked)), "and in view");
    preferences.top = 0u;
    cli_check(EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &preferences, .palette = palette, .rows = 900) && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = count - 1u)), "the wheel stops with the last color in view");
    preferences.top = 0u;
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .width = 1200u, .height = 800u, .scale = 100);

    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageUp", .change = &change);
    cli_check((preferences.section == GUI_PREFERENCES_SETTINGS) && (preferences.marked == 0u) && (preferences.top == 0u), "PageUp shows the section before");
    const GuiRect colors_name = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_COLORS);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = colors_name.x + 3, .y = colors_name.y + 3, .change = &change) && (preferences.section == GUI_PREFERENCES_COLORS), "a press on a section's name shows it");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    return cli_on_check();
}
