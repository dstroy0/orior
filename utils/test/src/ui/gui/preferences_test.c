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

// whether the first `length` bytes of `bytes` are `text`'s
static int same_n(const char *bytes, const char *text, unsigned long long length)
{
    return (text_length(text) == length) && (memor.cmp(&(MemoriaCfg){.src = bytes, .other = text, .bytes = length}) == 0);
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

// the rows of the check's table, Settings' four and then Fonts' one
#define ROW_ZOOM 0u
#define ROW_BAR 1u
#define ROW_EDITOR 2u
#define ROW_REDUCE 3u
#define ROW_UNSET 4u
#define ROW_SETTINGS 4u

// copy `text` into the row's field `field` of GUI_PREFERENCES_TEXT bytes
static void field_set(char *field, const char *text)
{
    const unsigned long long length = text_length(text);
    memor.cpy(&(MemoriaCfg){.dst = field, .src = text, .bytes = length + 1ull});
}

// add one row to `values`
static void row_add(GuiPreferenceValues *values, const char *label, const char *key, unsigned int type, const char *value)
{
    GuiPreferenceRow *const row = &values->row[values->rows];
    field_set(row->label, label);
    field_set(row->key, key);
    row->type = type;
    field_set(row->value, value);
    values->rows += 1u;
}

// the check's table: the zoom, two opacities, less motion at `reduced`, and a row of text with no value
static void values_set(GuiPreferenceValues *values, const char *reduced)
{
    values->zoom_step = GUI_PREFERENCE_ZOOM_DEFAULT_STEP;
    values->bar_percent = 30u;
    values->editor_percent = 80u;
    values->reduced = (reduced[1] == 'n');
    values->rows = 0u;
    row_add(values, "Zoom", "zoom", GUI_PREFERENCE_TYPE_ZOOM, "100%");
    row_add(values, "Bar opacity", "opacity.bar", GUI_PREFERENCE_TYPE_PERCENT, "30%");
    row_add(values, "Editor opacity", "opacity.editor", GUI_PREFERENCE_TYPE_PERCENT, "80%");
    row_add(values, "Reduce Motion", "reduce-motion", GUI_PREFERENCE_TYPE_SWITCH, reduced);
    row_add(values, "Code font", "fonts.code", GUI_PREFERENCE_TYPE_TEXT, "");
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
    static GuiPreferenceValues values;
    values_set(&values, "off");
    char output[GUI_PREFERENCES_TEXT];
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = ROW_ZOOM, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Zoom is ") && same_text(value, "100%"), "the zoom reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = ROW_BAR, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Bar opacity is ") && same_text(value, "30%"), "the bar's opacity reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = ROW_EDITOR, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Editor opacity is ") && same_text(value, "80%"), "the editor's opacity reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = ROW_REDUCE, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Reduce Motion is ") && same_text(value, "off"), "less motion reads as a sentence");
    EMBED_CALL(gui_preferences_values.preference_sentence, GuiPreferencesValuesCfg, .values = &values, .row = ROW_UNSET, .output = output, .value = value, .size = sizeof(output));
    cli_check(same_text(output, "Code font is ") && same_text(value, "not set"), "a row with no value says it is not set");

    // the keys over Settings
    static GuiPreferences preferences;
    EMBED_CALL(gui_preferences_size.init, GuiPreferencesSizeCfg, .preferences = &preferences);
    cli_check(!preferences.displayed && (preferences.section == GUI_PREFERENCES_SETTINGS), "it starts closed at its first section");
    // Settings holds the first four rows and Fonts the fifth. The other sections hold none
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        preferences.section_first[section] = values.rows;
        preferences.section_rows[section] = 0u;
    }
    preferences.section_first[GUI_PREFERENCES_SETTINGS] = ROW_ZOOM;
    preferences.section_rows[GUI_PREFERENCES_SETTINGS] = ROW_SETTINGS;
    preferences.section_first[GUI_PREFERENCES_FONTS] = ROW_UNSET;
    preferences.section_rows[GUI_PREFERENCES_FONTS] = 1u;
    // the part reads each row's type, as the window gives it
    for (unsigned int row_at = 0u; row_at < values.rows; row_at += 1u)
    {
        preferences.type[row_at] = values.row[row_at].type;
    }
    preferences.rows = ROW_SETTINGS;
    preferences.displayed = 1;
    // its own window: what it covers is the client whole, at the size that shows every color in two columns, and at a scale
    unsigned int requested_width = 0u;
    unsigned int requested_height = 0u;
    EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = palette, .scale = 100, .width_pointer = &requested_width, .height_pointer = &requested_height);
    // the edge all round, the tabs' row and half an edge under it, and the colors' rows in two columns under the row of contrasts
    const unsigned int color_rows = ((EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette) + 1u) / 2u) + 1u;
    const unsigned int wanted_width = (2u * GUI_PREFERENCES_EDGE) + (2u * GUI_PREFERENCES_CELL_WIDTH);
    const unsigned int wanted_height = (2u * GUI_PREFERENCES_EDGE) + GUI_PREFERENCES_ROW + (GUI_PREFERENCES_EDGE / 2u) + (color_rows * GUI_PREFERENCES_CELL);
    cli_check((requested_width == wanted_width) && (requested_height == wanted_height), "the size it asks for shows every color in two columns");
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .glyphs = &glyphs, .width = requested_width, .height = requested_height, .scale = 100);
    cli_check(EMBED_CALL(gui_preferences_palette_values.cells_across, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences) == 2u, "at that size the grid has two columns");
    cli_check(!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette) - 1u)), "and the last color in view");
    EMBED_CALL(gui_preferences_scale.want, GuiPreferencesScaleCfg, .palette = palette, .scale = 200, .width_pointer = &requested_width, .height_pointer = &requested_height);
    cli_check((requested_width == 2u * wanted_width) && (requested_height == 2u * wanted_height), "the size follows the scale");
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .glyphs = &glyphs, .width = 1200u, .height = 800u, .scale = 100);
    const GuiRect whole = EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences);
    cli_check((whole.x == 0) && (whole.y == 0) && (whole.width == 1200) && (whole.height == 800), "it covers its window's client");
    GuiPreferenceChange change;
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    cli_check((preferences.marked == ROW_BAR) && (change.type == GUI_PREFERENCE_NOTHING), "Down marks the next row");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Right", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_STEPPED) && (change.at == ROW_BAR) && (change.by == 1), "Right moves its value on");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Left", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_STEPPED) && (change.by == -1), "Left moves it back");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Up", .change = &change);
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Up", .change = &change);
    cli_check(preferences.marked == ROW_ZOOM, "Up stops at the first row");
    for (unsigned int down = 0u; down < ROW_SETTINGS + 2u; down += 1u)
    {
        EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    }
    cli_check(preferences.marked == ROW_SETTINGS - 1u, "Down stops at the last");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Q", .change = &change);
    cli_check(change.type == GUI_PREFERENCE_NOTHING, "a key that is none of its own changes nothing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_CLOSED) && !preferences.displayed, "Escape closes it");
    preferences.displayed = 1;

    // a press moves a switch on and round, a press on a slider takes the value where it lands, and a press shows a section
    const GuiRect row = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_REDUCE);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = row.x + 5, .y = row.y + 5, .change = &change) && (change.type == GUI_PREFERENCE_STEPPED) &&
                  (change.at == ROW_REDUCE) && (change.by == 1) && (preferences.marked == ROW_REDUCE),
              "a press on a row marks it and moves it a step");
    const GuiRect slider = EMBED_CALL(gui_preferences_row.control_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_EDITOR);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = slider.x, .y = slider.y + (slider.height / 2), .change = &change) && (change.type == GUI_PREFERENCE_SLID) &&
                  (change.at == ROW_EDITOR) && (change.share == 0u) && preferences.sliding && (preferences.marked == ROW_EDITOR),
              "a press at a slider's left end takes none and holds the slider");
    cli_check(EMBED_CALL(gui_preferences_palette_change.slide, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = slider.x + slider.width, .y = slider.y, .change = &change) && (change.type == GUI_PREFERENCE_SLID) && (change.share == 1000u),
              "a drag to its right end takes the whole");
    cli_check(EMBED_CALL(gui_preferences_palette_change.release, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = 0, .y = 0, .change = &change) && !preferences.sliding, "the release lets the slider go");
    cli_check(!EMBED_CALL(gui_preferences_palette_change.slide, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = slider.x, .y = slider.y, .change = &change), "and a move after it slides nothing");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).x + 5, .y = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).y + 5) &&
                  (preferences.marked == ROW_ZOOM),
              "the pointer on a row marks it");
    cli_check(!EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).x + 6, .y = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).y + 5), "and moving on it changes nothing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Down", .change = &change);
    cli_check(!EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).x + 6, .y = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM).y + 5) &&
                  (preferences.marked == ROW_BAR),
              "a move with no change of place keeps the mark the keys moved");
    preferences.marked = ROW_ZOOM;
    cli_check(!EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = 1, .y = 700, .change = &change) && (change.type == GUI_PREFERENCE_NOTHING), "a press on its ground changes nothing");
    // the close mark at its head's right end
    const GuiRect close_mark = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = &preferences);
    cli_check((close_mark.x + close_mark.width == 1200 - GUI_PREFERENCES_EDGE) && (close_mark.y == GUI_PREFERENCES_EDGE) && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = close_mark), "the close mark stands at the right end of the tabs' row");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = close_mark.x + 4, .y = close_mark.y + 4) && preferences.close_pointed, "the pointer on the close mark marks it");
    cli_check(EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = 1, .y = 700) && !preferences.close_pointed, "and moving off it lets it go");
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = close_mark.x + 4, .y = close_mark.y + 4, .change = &change) && (change.type == GUI_PREFERENCE_CLOSED) && !preferences.displayed, "a press on the close mark closes it");
    preferences.displayed = 1;

    // one section drawn at a time, how bright each thing stands telling them apart
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1200u, .height = 800u), "a raster to draw on");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1200, 800}, .color = palette->background_fill);
    static GuiPreferenceValues displayed_values;
    values_set(&displayed_values, "on");
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .raster = &raster, .glyphs = &glyphs, .palette = palette, .values = &displayed_values);
    // the lattice shows clear in the margin, and the sheet floats in from it in the glass's ink
    const GuiColor margin_pixel = raster.pixels[(400u * raster.width) + 2u];
    const GuiColor sheet_pixel = raster.pixels[(780u * raster.width) + 600u];
    cli_check((margin_pixel != sheet_pixel) && (margin_pixel != palette->surface), "the sheet floats on the lattice in from the window's sides");
    const unsigned int section_on = brightest(&raster, EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_SETTINGS));
    const unsigned int section_off = brightest(&raster, EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_COLORS));
    cli_check(section_on > section_off, "the section shown stands brighter than the quiet list");
    cli_check(section_off > brightest(&raster, (GuiRect){EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences).x + 2, EMBED_CALL(gui_preferences_palette_values.rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences).y + 2, 4, 4}), "and the list brighter than the ground");
    const unsigned int marked_row = brightest(&raster, EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM));
    const unsigned int other_row = brightest(&raster, EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_BAR));
    cli_check(marked_row > other_row, "the row marked stands brighter than the others");
    cli_check(other_row > 0u, "and every row is drawn");
    // no ground or line under a row: the row's far end stands as the ground stood
    const GuiRect far = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = ROW_ZOOM);
    cli_check(brightest(&raster, (GuiRect){far.x + far.width - 6, far.y, 4, far.height}) <= brightest(&raster, (GuiRect){700, 760, 300, 30}), "a row lays no ground or line about itself");

    // the colors' grid
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageDown", .change = &change);
    cli_check((preferences.section == GUI_PREFERENCES_KEYS) && (preferences.rows == 0u), "PageDown shows the next section");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check(change.type == GUI_PREFERENCE_NOTHING, "a key over a section with no rows changes nothing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageDown", .change = &change);
    cli_check((preferences.section == GUI_PREFERENCES_FONTS) && (preferences.marked == 0u) && (preferences.first == ROW_UNSET) && (preferences.rows == 1u), "and the next, its first thing marked");
    // a row of text: Enter asks for it, and what is typed in place is taken by Enter
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_STEPPED) && (change.at == ROW_UNSET), "a row's change names the row among every section's rows");
    preferences.entering = 1;
    preferences.entered_length = 0u;
    cli_check(EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "Mono \xC3\xA9\x01", .size = 8u) && (preferences.entered_length == 7u), "a row's text takes every byte but a control byte");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Backspace", .change = &change);
    cli_check(preferences.entered_length == 5u, "Backspace takes the last character off whole");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_TYPED) && (change.at == ROW_UNSET) && !preferences.entering && same_n(preferences.entered, "Mono ", 5u), "Enter takes the text typed for its row");
    for (unsigned int section = GUI_PREFERENCES_FONTS; section < GUI_PREFERENCES_COLORS; section += 1u)
    {
        EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageDown", .change = &change);
    }
    cli_check((preferences.section == GUI_PREFERENCES_COLORS) && (preferences.marked == 0u), "and the sections after it in turn, to Colors");
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
    // what a section shows starts where its first row stands, half an edge under the tabs
    const GuiRect colors_row = EMBED_CALL(gui_preferences_row.row_rect, GuiPreferencesRowCfg, .preferences_read = &preferences, .row = 0u);
    const GuiRect tab = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_COLORS);
    cli_check(colors_row.y == tab.y + tab.height + (GUI_PREFERENCES_EDGE / 2), "what a section shows stands under the tabs");
    cli_check(first_cell.y == colors_row.y + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_CELL, .scale = 100), "the grid stands a row under the row of contrasts");
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
    cli_check(brightest(&raster, (GuiRect){second_cell.x + mark_left, second_cell.y, second_cell.width - mark_left, second_cell.height}) <= brightest(&raster, (GuiRect){700, 760, 300, 30}), "a color over it does not");
    static GuiPaletteOwn bright;
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &bright, .scheme = GUI_SCHEME_DARK);
    bright.colors.number = bright.colors.text;
    for (unsigned int token = 0u; token < bright.colors.token_count; token += 1u)
    {
        bright.token[token].color = bright.colors.text;
    }
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1200, 800}, .color = palette->background_fill);
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .raster = &raster, .glyphs = &glyphs, .palette = &bright.colors, .values = &displayed_values);
    cli_check(brightest(&raster, contrasts) <= brightest(&raster, (GuiRect){700, 760, 300, 30}), "with none under the least the row says nothing");

    // typing a value
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check(preferences.entering && (preferences.entered_length == 0u), "Enter begins typing a value");
    cli_check(EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "#12a", .size = 4ull) && (preferences.entered_length == 4u), "the hex typed is taken");
    cli_check(EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "z", .size = 1ull) && (preferences.entered_length == 4u), "what is no hex is passed over");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Backspace", .change = &change);
    cli_check(preferences.entered_length == 3u, "Backspace takes the last off");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check(preferences.entering && (change.type == GUI_PREFERENCE_NOTHING), "what is no color is not taken and the typing goes on");
    (void)EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "a4c5e6", .size = 6ull);
    cli_check(preferences.entered_length == 7u + 2u, "typing goes on after it");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);
    cli_check(!preferences.entering && preferences.displayed && (change.type == GUI_PREFERENCE_NOTHING), "Escape leaves the typing, Preferences staying");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    (void)EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &preferences, .text = "#a1b2c3", .size = 7ull);
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Enter", .change = &change);
    cli_check((change.type == GUI_PREFERENCE_COLORED) && (change.at == 0u) && (change.color == GUI_RGB(0xa1u, 0xb2u, 0xc3u)) && !preferences.entering, "a color typed is taken");
    const GuiRect pressed_cell = EMBED_CALL(gui_preferences_palette_values.cell_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = 3u);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = pressed_cell.x + 5, .y = pressed_cell.y + 5, .change = &change) && (preferences.marked == 3u) && preferences.entering, "a press on a cell marks it and begins typing");
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "Escape", .change = &change);

    // the grid scrolled, by the wheel and by the keys saving the mark in view
    const unsigned int count = EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = palette);
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .glyphs = &glyphs, .width = 960u, .height = 400u, .scale = 100);
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
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &preferences, .glyphs = &glyphs, .width = 1200u, .height = 800u, .scale = 100);

    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &preferences, .palette = palette, .name = "PageUp", .change = &change);
    cli_check((preferences.section == GUI_PREFERENCES_THEME) && (preferences.marked == 0u) && (preferences.top == 0u), "PageUp shows the section before");
    const GuiRect colors_name = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg, .preferences_read = &preferences, .at = GUI_PREFERENCES_COLORS);
    cli_check(EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg, .preferences = &preferences, .palette = palette, .x = colors_name.x + 3, .y = colors_name.y + 3, .change = &change) && (preferences.section == GUI_PREFERENCES_COLORS), "a press on a section's name shows it");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    return cli_on_check();
}
