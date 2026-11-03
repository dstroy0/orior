// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preferences.h: File, Preferences, in a system window of its own, drawn in the window's own terms: one section shown
// at a time beside a quiet list of the sections, each row of a section a sentence that says what it is set to, and
// nothing but how bright each thing stands to tell them apart, no line or ground about any of them. Colors is the one
// exception, a grid of the palette's colors, each with its swatch, its name and its value, a value typed in as hex, and
// a color of text under the least contrast marked with its ratio, a row over the grid saying what each is drawn on. The
// window holds what each row sets and acts on what is changed here (preferences.c)
#ifndef GUI_PARTS_PREFERENCES_H
#define GUI_PARTS_PREFERENCES_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiPreferencesPaletteValuesCfg GuiPreferencesPaletteValuesCfg;
typedef struct GuiPreferencesValuesCfg GuiPreferencesValuesCfg;
typedef struct GuiPreferencesPaletteChangeCfg GuiPreferencesPaletteChangeCfg;
typedef struct GuiPreferencesPaletteNameCfg GuiPreferencesPaletteNameCfg;
typedef struct GuiPreferencesWidthCfg GuiPreferencesWidthCfg;
typedef struct GuiPreferencesScaleCfg GuiPreferencesScaleCfg;
typedef struct GuiPreferencesPercentCfg GuiPreferencesPercentCfg;
typedef struct GuiPreferencesStepCfg GuiPreferencesStepCfg;
typedef struct GuiPreferencesSizeCfg GuiPreferencesSizeCfg;
typedef struct GuiPreferencesPaletteCfg GuiPreferencesPaletteCfg;
typedef struct GuiPreferencesCfg GuiPreferencesCfg;
typedef struct GuiPreferencesRowCfg GuiPreferencesRowCfg;

// the sections; the rows of Settings; the zoom's steps; the opacities' step and most; and the sizes in unscaled pixels:
// how far everything saves from the edges of what it stands in, the sections' width, a row's height, the text's size, a
// cell of the grid's width and height, the swatch's side, the code's size, and the most bytes a value's text and a
// color's name take
#define GUI_PREFERENCES_SETTINGS 0u
#define GUI_PREFERENCES_COLORS 1u
#define GUI_PREFERENCES_SECTIONS 2u
#define GUI_PREFERENCE_ZOOM 0u
#define GUI_PREFERENCE_BAR 1u
#define GUI_PREFERENCE_EDITOR 2u
#define GUI_PREFERENCE_REDUCE 3u
#define GUI_PREFERENCE_ROWS 4u
#define GUI_PREFERENCE_ZOOM_STEPS 13u
#define GUI_PREFERENCE_ZOOM_DEFAULT_STEP 5u
#define GUI_PREFERENCE_OPACITY_STEP 10u
#define GUI_PREFERENCE_OPACITY_MAX 100u
#define GUI_PREFERENCES_EDGE 24
#define GUI_PREFERENCES_LIST 168
#define GUI_PREFERENCES_ROW 30
#define GUI_PREFERENCES_SIZE 13u
#define GUI_PREFERENCES_CELL_WIDTH 320
#define GUI_PREFERENCES_CELL 24
#define GUI_PREFERENCES_SWATCH 14
#define GUI_PREFERENCES_NAME_WIDTH 175
#define GUI_PREFERENCES_VALUE_WIDTH 66
#define GUI_PREFERENCES_CODE 12u
#define GUI_PREFERENCES_TEXT 64u
#define GUI_PREFERENCES_CONTRASTS 1024u
#define GUI_PREFERENCES_CLOSE_WIDTH 32
#define GUI_PREFERENCES_CLOSE_SIDE 9

// What the rows of Settings stand at: the zoom's step, the bar's and the editor's opacity in percent, and whether
// motion is reduced
typedef struct
{
    unsigned int zoom_step;
    unsigned int bar_percent;
    unsigned int editor_percent;
    int reduced;
} GuiPreferenceValues;

// What a key or a press changed for the window to act on: nothing, Preferences closed, a row of Settings moved `by`
// steps, past its end back round to its start where `round` is set, or the color `at` of the palette's named colors
// made `color`
#define GUI_PREFERENCE_NOTHING 0u
#define GUI_PREFERENCE_CLOSED 1u
#define GUI_PREFERENCE_STEPPED 2u
#define GUI_PREFERENCE_COLORED 3u
typedef struct
{
    unsigned int kind;
    unsigned int at;
    int by;
    int round;
    GuiColor color;
} GuiPreferenceChange;

// Preferences: whether it is shown; the section shown; the row or color marked; the grid's first row in view; whether a
// color's value is being typed, what has been typed and how much; its window's client size and the scale it is laid at,
// a percent; and whether the pointer stands on its close mark
typedef struct
{
    int displayed;
    unsigned int section;
    unsigned int marked;
    unsigned int top;
    int entering;
    char entered[GUI_PREFERENCES_TEXT];
    unsigned int entered_length;
    unsigned int width;
    unsigned int height;
    int scale;
    int close_pointed;
} GuiPreferences;

// a Preferences not shown, at its first section
void gui_preferences_init(const GuiPreferencesSizeCfg *args);
// its window's client `width` by `height` at `scale`, the grid's rows in view following; and the size at `scale` that
// shows every color of `palette` in two columns
void gui_preferences_resize(const GuiPreferencesWidthCfg *args);
void gui_preferences_want(const GuiPreferencesScaleCfg *args);
// the percent of a step of the zoom, and the step of a percent: 1, or 0 where no step has that percent
unsigned int gui_preference_zoom_percent(const GuiPreferencesStepCfg *args);
int gui_preference_zoom_step(const GuiPreferencesCfg *args);
// a value `step` of the zoom or a percent moved `by`, no further than its ends, or past its end back round to its start
// where `round` is set
unsigned int gui_preference_zoom_move(const GuiPreferencesStepCfg *args);
unsigned int gui_preference_opacity_move(const GuiPreferencesPercentCfg *args);
// the row `row` of Settings as its sentence: what comes before its value, ending in `is `, and its value, each into
// `size` bytes
void gui_preference_sentence(const GuiPreferencesValuesCfg *args);
// where it all stands in its window: what it covers, the client whole; the close mark at the head's right end; a
// section's name in the list; a row of Settings; and the colors' grid's cells across and in view, and a color's cell,
// empty where it is out of view
GuiRect gui_preferences_rect(const GuiPreferencesPaletteValuesCfg *args);
GuiRect gui_preferences_close_rect(const GuiPreferencesRowCfg *args);
GuiRect gui_preferences_section_rect(const GuiPreferencesPaletteValuesCfg *args);
GuiRect gui_preferences_row_rect(const GuiPreferencesRowCfg *args);
unsigned int gui_preferences_cells_across(const GuiPreferencesPaletteValuesCfg *args);
GuiRect gui_preferences_cell_rect(const GuiPreferencesPaletteValuesCfg *args);
// the keys of its window, `name` as the editor names a key: Up and Down mark a row or a color, Left and Right move a
// row's value or mark a color beside, PageUp and PageDown show a section, Enter begins typing a color's value and takes
// it, Escape leaves the typing or closes it. The change into `change`
void gui_preferences_key(const GuiPreferencesPaletteNameCfg *args);
// text typed: the hex of a color's value being typed, the rest passed over: 1 where it was taken
int gui_preferences_type(const GuiPreferencesSizeCfg *args);
// a press at x, y of its window: closed by the close mark, a section shown, a row moved a step on and round, a color
// marked and its typing begun: 1 where it landed on one, the change into `change`; the pointer moved to x, y marking
// the row or color under it and the close mark: 1 where a mark moved; and the wheel moving the grid `rows`: 1 where it
// moved
int gui_preferences_press(const GuiPreferencesPaletteChangeCfg *args);
int gui_preferences_move(const GuiPreferencesPaletteChangeCfg *args);
int gui_preferences_wheel(const GuiPreferencesPaletteCfg *args);
// its window's whole client drawn on the widget's ground
void gui_preferences_draw(const GuiPreferencesPaletteValuesCfg *args);
struct GuiPreferencesPaletteValuesCfg
{
    const GuiPalette *palette;
    const GuiPreferenceValues *values;
    const GuiPreferences *preferences_read;
    unsigned int at;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
};

struct GuiPreferencesValuesCfg
{
    const GuiPreferenceValues *values;
    unsigned int row;
    char *output;
    char *value;
    unsigned long long size;
};

struct GuiPreferencesPaletteChangeCfg
{
    GuiPreferences *preferences;
    const GuiPalette *palette;
    GuiPreferenceChange *change;
    int x;
    int y;
};

struct GuiPreferencesPaletteNameCfg
{
    GuiPreferences *preferences;
    const GuiPalette *palette;
    const char *name;
    GuiPreferenceChange *change;
};

struct GuiPreferencesWidthCfg
{
    GuiPreferences *preferences;
    unsigned int width;
    unsigned int height;
    int scale;
};

struct GuiPreferencesScaleCfg
{
    int scale;
    const GuiPalette *palette;
    unsigned int *width_pointer;
    unsigned int *height_pointer;
};

struct GuiPreferencesPercentCfg
{
    unsigned int percent;
    int by;
    int round;
};

struct GuiPreferencesStepCfg
{
    unsigned int step;
    int by;
    int round;
};

struct GuiPreferencesSizeCfg
{
    GuiPreferences *preferences;
    unsigned long long size;
    const char *text;
};

struct GuiPreferencesPaletteCfg
{
    GuiPreferences *preferences;
    const GuiPalette *palette;
    int rows;
};

struct GuiPreferencesCfg
{
    unsigned int percent;
    unsigned int *step_pointer;
};

struct GuiPreferencesRowCfg
{
    unsigned int row;
    const GuiPreferences *preferences_read;
};

typedef struct
{
    void (*init)(const GuiPreferencesSizeCfg *args);
    int (*type)(const GuiPreferencesSizeCfg *args);
} GuiPreferencesSizeNs;
EMBED_TABLE_LAYOUT(GuiPreferencesSizeNs, init, type);
EMBED_TABLE_STORAGE GuiPreferencesSizeNs gui_preferences_size EMBED_UNUSED = {
    .init = gui_preferences_init,
    .type = gui_preferences_type,
};

typedef struct
{
    void (*resize)(const GuiPreferencesWidthCfg *args);
} GuiPreferencesWidthNs;
EMBED_TABLE_LAYOUT(GuiPreferencesWidthNs, resize);
EMBED_TABLE_STORAGE GuiPreferencesWidthNs gui_preferences_width EMBED_UNUSED = {
    .resize = gui_preferences_resize,
};

typedef struct
{
    void (*want)(const GuiPreferencesScaleCfg *args);
} GuiPreferencesScaleNs;
EMBED_TABLE_LAYOUT(GuiPreferencesScaleNs, want);
EMBED_TABLE_STORAGE GuiPreferencesScaleNs gui_preferences_scale EMBED_UNUSED = {
    .want = gui_preferences_want,
};

typedef struct
{
    unsigned int (*preference_zoom_percent)(const GuiPreferencesStepCfg *args);
    unsigned int (*preference_zoom_move)(const GuiPreferencesStepCfg *args);
} GuiPreferencesStepNs;
EMBED_TABLE_LAYOUT(GuiPreferencesStepNs, preference_zoom_percent, preference_zoom_move);
EMBED_TABLE_STORAGE GuiPreferencesStepNs gui_preferences_step EMBED_UNUSED = {
    .preference_zoom_percent = gui_preference_zoom_percent,
    .preference_zoom_move = gui_preference_zoom_move,
};

typedef struct
{
    int (*preference_zoom_step)(const GuiPreferencesCfg *args);
} GuiPreferencesNs;
EMBED_TABLE_LAYOUT(GuiPreferencesNs, preference_zoom_step);
EMBED_TABLE_STORAGE GuiPreferencesNs gui_preferences EMBED_UNUSED = {
    .preference_zoom_step = gui_preference_zoom_step,
};

typedef struct
{
    unsigned int (*preference_opacity_move)(const GuiPreferencesPercentCfg *args);
} GuiPreferencesPercentNs;
EMBED_TABLE_LAYOUT(GuiPreferencesPercentNs, preference_opacity_move);
EMBED_TABLE_STORAGE GuiPreferencesPercentNs gui_preferences_percent EMBED_UNUSED = {
    .preference_opacity_move = gui_preference_opacity_move,
};

typedef struct
{
    void (*preference_sentence)(const GuiPreferencesValuesCfg *args);
} GuiPreferencesValuesNs;
EMBED_TABLE_LAYOUT(GuiPreferencesValuesNs, preference_sentence);
EMBED_TABLE_STORAGE GuiPreferencesValuesNs gui_preferences_values EMBED_UNUSED = {
    .preference_sentence = gui_preference_sentence,
};

typedef struct
{
    GuiRect (*rect)(const GuiPreferencesPaletteValuesCfg *args);
    GuiRect (*section_rect)(const GuiPreferencesPaletteValuesCfg *args);
    unsigned int (*cells_across)(const GuiPreferencesPaletteValuesCfg *args);
    GuiRect (*cell_rect)(const GuiPreferencesPaletteValuesCfg *args);
    void (*draw)(const GuiPreferencesPaletteValuesCfg *args);
} GuiPreferencesPaletteValuesNs;
EMBED_TABLE_LAYOUT(GuiPreferencesPaletteValuesNs, rect, section_rect, cells_across, cell_rect, draw);
EMBED_TABLE_STORAGE GuiPreferencesPaletteValuesNs gui_preferences_palette_values EMBED_UNUSED = {
    .rect = gui_preferences_rect,
    .section_rect = gui_preferences_section_rect,
    .cells_across = gui_preferences_cells_across,
    .cell_rect = gui_preferences_cell_rect,
    .draw = gui_preferences_draw,
};

typedef struct
{
    GuiRect (*close_rect)(const GuiPreferencesRowCfg *args);
    GuiRect (*row_rect)(const GuiPreferencesRowCfg *args);
} GuiPreferencesRowNs;
EMBED_TABLE_LAYOUT(GuiPreferencesRowNs, close_rect, row_rect);
EMBED_TABLE_STORAGE GuiPreferencesRowNs gui_preferences_row EMBED_UNUSED = {
    .close_rect = gui_preferences_close_rect,
    .row_rect = gui_preferences_row_rect,
};

typedef struct
{
    void (*key)(const GuiPreferencesPaletteNameCfg *args);
} GuiPreferencesPaletteNameNs;
EMBED_TABLE_LAYOUT(GuiPreferencesPaletteNameNs, key);
EMBED_TABLE_STORAGE GuiPreferencesPaletteNameNs gui_preferences_palette_name EMBED_UNUSED = {
    .key = gui_preferences_key,
};

typedef struct
{
    int (*press)(const GuiPreferencesPaletteChangeCfg *args);
    int (*move)(const GuiPreferencesPaletteChangeCfg *args);
} GuiPreferencesPaletteChangeNs;
EMBED_TABLE_LAYOUT(GuiPreferencesPaletteChangeNs, press, move);
EMBED_TABLE_STORAGE GuiPreferencesPaletteChangeNs gui_preferences_palette_change EMBED_UNUSED = {
    .press = gui_preferences_press,
    .move = gui_preferences_move,
};

typedef struct
{
    int (*wheel)(const GuiPreferencesPaletteCfg *args);
} GuiPreferencesPaletteNs;
EMBED_TABLE_LAYOUT(GuiPreferencesPaletteNs, wheel);
EMBED_TABLE_STORAGE GuiPreferencesPaletteNs gui_preferences_palette EMBED_UNUSED = {
    .wheel = gui_preferences_wheel,
};

#endif
