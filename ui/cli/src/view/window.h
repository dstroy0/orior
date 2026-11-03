// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: the view of what the controller publishes, drawn on the view's thread (view.c): its frame laid out
// for its size, its raster drawn with the parts, the glyphs and the palette they draw with; the pointer's place read as
// the caption, a border, a control or the client, and what stands under a pixel written to the controller as an input
// (window.c)
#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H
#include "base.h"
#include "claim.h"
#include "embed_dispatch_layout.h"
#include "framebuffer.h"
#include "ground.h"
#include "parts/busy.h"
#include "parts/editor.h"
#include "parts/explorer.h"
#include "parts/hover.h"
#include "parts/menubar.h"
#include "parts/minimap.h"
#include "parts/preferences.h"
#include "parts/problems.h"
#include "parts/signature.h"
#include "parts/statusbar.h"
#include "parts/strip.h"
#include "parts/suggest.h"
#include "parts/tabs.h"
#include "parts/terminal.h"
#include "parts/wordmark.h"
#include "stages.h"
#include "view.h"

typedef struct GuiWindowWokenCfg GuiWindowWokenCfg;
typedef struct GuiWindowWidthCfg GuiWindowWidthCfg;
typedef struct GuiWindowIconCfg GuiWindowIconCfg;
typedef struct GuiWindowCfg GuiWindowCfg;
typedef struct GuiWindowHitsCfg GuiWindowHitsCfg;
typedef struct GuiWindowAutohideCfg GuiWindowAutohideCfg;
typedef struct GuiWindowInputCfg GuiWindowInputCfg;
typedef struct GuiWindowInputKindCfg GuiWindowInputKindCfg;
typedef struct GuiWindowReducedCfg GuiWindowReducedCfg;
typedef struct GuiWindowDisplayedCfg GuiWindowDisplayedCfg;

// the resize border's width in the page's pixels; the rectangles a map of what a pixel stands on holds, each title,
// each menu open and the eye; and the milliseconds of one refresh of a monitor before the system has said
#define GUI_BORDER 5
#define GUI_WINDOW_HIT_CLIENTS (GUI_MENUBAR_TITLES + GUI_MENU_LEVELS + 1u)
#define GUI_VIEW_FRAME_MS 17u
// The window: its frame; its two framebuffers, the raster the frame is drawn into and the one shown, flipped as a frame
// is presented, the shown one NULL before any; the base every frame starts from and the ground under it, glyphs, icons,
// palette; the parts, each holding what it last took of the controller's; the tabs, the status and the settings as last
// published; when the caret's breath last began, each frame of the view beginning it again, the caret's ink in the last
// frame drawn, and whether its ink alone has changed, which the caret drawn again over the frame pays with no frame
// whole; the busy circle, with whether its stage alone has changed; Preferences, what its rows stand at, the palette of
// the window's own they change, and the scale the system gave, which the zoom lays the frame at a share of; the claim
// and whether its slides ran at the last tick; the controls' tab's shadow; the wordmark; less motion as the system and
// the settings ask it; the count the controller gives each file shown, and what it last told of Preferences and the
// name; what was last said of the editor's size, the bar's titles, the map's lines, the terminal's two panes' columns
// and rows and the pointer, none said again until it moves; whether the pointer drags the terminal's top edge and the
// height it last said; whether the pointer drags in the editor, and the terminal's pane it drags in, one past it, 0
// for none; the presses counted, when
// and where the last was; whether a frame is owed; and the control pressed, for the system to act on, GUI_HIT_CLIENT
// where none
typedef struct GuiWindow
{
    GuiFrame frame;
    GuiFramebuffer framebuffer;
    GuiRaster *raster;
    GuiRaster *displayed;
    GuiBase base;
    GuiBackground background_fill;
    GuiGlyphs glyphs;
    GuiIcons icons;
    const GuiPalette *palette;
    GuiEditor editor;
    GuiMinimap minimap;
    GuiMenubar menubar;
    GuiStrip strip;
    GuiExplorer explorer;
    GuiHover hover;
    GuiSuggest suggest;
    GuiSignature signature;
    GuiProblems problems;
    GuiTerminal terminal;
    const CliTabs *tabs;
    const CliStatus *status;
    const CliSettings *settings;
    unsigned long long caret_blink_start_ms;
    unsigned int caret_color;
    int caret_alone;
    GuiBusy busy;
    int busy_alone;
    GuiPreferences preferences;
    GuiPreferenceValues preference_values;
    GuiRaster preferences_raster;
    unsigned long long preferences_capacity;
    unsigned int display_width;
    unsigned int display_height;
    int preferences_pending;
    int preferences_system_scale;
    GuiPaletteOwn palette_own;
    int system_scale;
    GuiAutohide claim;
    int autohide_moving;
    GuiStages stages;
    const GuiStage *stage;
    int stages_stale;
    GuiMask controls_shadow;
    GuiWordmark wordmark;
    int motion_system;
    int motion_saved;
    int motion_still;
    int off_screen;
    unsigned long long shown;
    int preferences_status;
    unsigned int wordmark_status;
    unsigned int sent_rows;
    unsigned int sent_columns;
    unsigned int sent_shown;
    unsigned int sent_folded;
    int map_sent;
    unsigned long long map_sent_first;
    unsigned long long map_sent_last;
    unsigned int terminal_columns_sent[2];
    unsigned int terminal_rows_sent[2];
    int terminal_gripped;
    unsigned int terminal_height_sent;
    CliInput moved_sent;
    int dragging;
    unsigned int terminal_dragged;
    unsigned int clicks;
    unsigned long long double_click_deadline_ms;
    int clicked_x;
    int clicked_y;
    int pending;
    unsigned int pressed;
} GuiWindow;

// the double-click window, the pixels a press may move and still count, and the caret's blink: its period; each fade,
// down from full ink and up again, which the caret eases through on a cosine, the steady times at full and at dim
// taking what the period leaves; and the dim ink, 0 to 255
#define GUI_DOUBLE_CLICK_MS 500ull
#define GUI_CLICK_ADJACENT 4
#define GUI_CARET_BLINK_PERIOD_MS 1500ull
#define GUI_CARET_FADE_MS 300ull
#define GUI_CARET_DIM 64u

// the window begun: its glyphs read, its palette the dark scheme, its parts showing nothing: 1, or 0 with why given;
// and ended, the controller with it, every cell given back
int gui_window_begin(const GuiWindowWokenCfg *args);
void gui_window_end(const GuiWindowCfg *args);
// the window sized `width` by `height` at `scale`: its frame laid out and its raster made anew: 1, or 0 where the
// raster's cell could not be taken
int gui_window_resize(const GuiWindowWidthCfg *args);
// Preferences' system window, which the program opens and closes as `preferences.shown` says: its client sized at the
// scale the system draws it at, the zoom laid over that; an input in it, none of them the main window's and none of the
// main window's its; where the pointer on the input stands, as the names above give the places; and its frame drawn
// into its own raster, the frame owed no more
void gui_window_preferences_resize(const GuiWindowWidthCfg *args);
void gui_window_preferences_input(const GuiWindowInputKindCfg *args);
unsigned int gui_window_preferences_hit(const GuiWindowInputCfg *args);
void gui_window_preferences_draw(const GuiWindowCfg *args);
// the first frame owed as the window opens; and Preferences' window asked closed by the system
void gui_window_start(const GuiWindowCfg *args);
void gui_window_preferences_close(const GuiWindowCfg *args);
// the frame drawn into the raster not shown, the frame owed no more; and the two flipped once that frame is presented,
// the raster drawn the one shown and the other the one to draw into
void gui_window_draw(const GuiWindowCfg *args);
void gui_window_flip(const GuiWindowCfg *args);
// the caret drawn again on the raster shown, over the frame last drawn, at the ink its breath stands at now, the frame
// owed for its ink alone paid: the caret's rectangle, the rows of which are to be shown again, empty where no caret
// stands; and the busy circle's stage at now blitted again on the raster shown, its step paid: its rectangle, the same
// way
GuiRect gui_window_caret_draw(const GuiWindowCfg *args);
GuiRect gui_window_busy_draw(const GuiWindowCfg *args);
// what a pixel of the main window and of Preferences' window stands on, as they are laid, written into `hits`
void gui_window_hits(const GuiWindowHitsCfg *args);
void gui_window_preferences_hits(const GuiWindowHitsCfg *args);
// where the pointer on the input stands; and an input in the main window, as its kind says
unsigned int gui_window_hit(const GuiWindowInputCfg *args);
void gui_window_input(const GuiWindowInputKindCfg *args);
// the newest of each part taken, on the window's thread, each that changed owing a frame
void gui_window_drain(const GuiWindowCfg *args);
// the tick the window asked for come: the claim's timers run and its parts laid where its slides stand, and the
// ground drawn again where its loop shows a change; and when the window next wants a tick, in milliseconds, 0 for
// never
void gui_window_tick(const GuiWindowCfg *args);
unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args);
// the frame laid for the claim's movement at `time_ms`: while a slide runs the stage standing then, the stages made
// where a slide has been sent since they were; at rest the frame the slides rest at
void gui_window_autohide_layout(const GuiWindowAutohideCfg *args);
// the system's wish for less motion set, 1 where it asks for less: that or View's Reduce Motion, saved in orior's
// folder, stills the lattice, the claim's slides, the name's flight and the caret's breath
void gui_window_motion_wish(const GuiWindowReducedCfg *args);
// the window gone off the screen, `displayed` 0, its ground's clock standing still, or back on it, 1, a frame owed
void gui_window_show(const GuiWindowDisplayedCfg *args);
// the window's icon, the wordmark's eye in the text's color, made into `icon` at `size` pixels a side with its own
// alpha, for the system to take: 1, or 0 where the eye could not be drawn or the raster made; given back as a raster
int gui_window_icon(const GuiWindowIconCfg *args);
struct GuiWindowWokenCfg
{
    GuiWindow *window;
    VerbaLinea *err_msg;
    unsigned int display_width;
    unsigned int display_height;
};

struct GuiWindowHitsCfg
{
    const GuiWindow *window;
    CliViewHits *hits;
};

struct GuiWindowWidthCfg
{
    GuiWindow *window;
    unsigned int width;
    unsigned int height;
    int system_scale;
};

struct GuiWindowIconCfg
{
    GuiWindow *window;
    GuiRaster *icon;
    unsigned int size;
};

struct GuiWindowCfg
{
    GuiWindow *window;
};

struct GuiWindowAutohideCfg
{
    GuiWindow *window;
    unsigned long long time_ms;
};

struct GuiWindowInputCfg
{
    const GuiInput *input;
    const GuiWindow *window_read;
};

struct GuiWindowInputKindCfg
{
    GuiWindow *window;
    const GuiInput *input;
};

struct GuiWindowReducedCfg
{
    GuiWindow *window;
    int reduced;
};

struct GuiWindowDisplayedCfg
{
    GuiWindow *window;
    int displayed;
};

typedef struct
{
    int (*begin)(const GuiWindowWokenCfg *args);
} GuiWindowWokenNs;
EMBED_TABLE_LAYOUT(GuiWindowWokenNs, begin);
EMBED_TABLE_STORAGE GuiWindowWokenNs gui_window_woken EMBED_UNUSED = {
    .begin = gui_window_begin,
};

typedef struct
{
    void (*end)(const GuiWindowCfg *args);
    void (*preferences_draw)(const GuiWindowCfg *args);
    void (*start)(const GuiWindowCfg *args);
    void (*draw)(const GuiWindowCfg *args);
    void (*flip)(const GuiWindowCfg *args);
    GuiRect (*caret_draw)(const GuiWindowCfg *args);
    GuiRect (*busy_draw)(const GuiWindowCfg *args);
    void (*preferences_close)(const GuiWindowCfg *args);
    void (*drain)(const GuiWindowCfg *args);
    void (*tick)(const GuiWindowCfg *args);
    void (*hits)(const GuiWindowHitsCfg *args);
    void (*preferences_hits)(const GuiWindowHitsCfg *args);
} GuiWindowNs;
EMBED_TABLE_LAYOUT(GuiWindowNs, end, preferences_draw, start, draw, flip, caret_draw, busy_draw, preferences_close,
                   drain, tick, hits, preferences_hits);
EMBED_TABLE_STORAGE GuiWindowNs gui_window_table EMBED_UNUSED = {
    .end = gui_window_end,
    .preferences_draw = gui_window_preferences_draw,
    .start = gui_window_start,
    .draw = gui_window_draw,
    .flip = gui_window_flip,
    .caret_draw = gui_window_caret_draw,
    .busy_draw = gui_window_busy_draw,
    .preferences_close = gui_window_preferences_close,
    .drain = gui_window_drain,
    .tick = gui_window_tick,
    .hits = gui_window_hits,
    .preferences_hits = gui_window_preferences_hits,
};

typedef struct
{
    int (*resize)(const GuiWindowWidthCfg *args);
    void (*preferences_resize)(const GuiWindowWidthCfg *args);
} GuiWindowWidthNs;
EMBED_TABLE_LAYOUT(GuiWindowWidthNs, resize, preferences_resize);
EMBED_TABLE_STORAGE GuiWindowWidthNs gui_window_width EMBED_UNUSED = {
    .resize = gui_window_resize,
    .preferences_resize = gui_window_preferences_resize,
};

typedef struct
{
    void (*preferences_input)(const GuiWindowInputKindCfg *args);
    void (*input)(const GuiWindowInputKindCfg *args);
} GuiWindowInputKindNs;
EMBED_TABLE_LAYOUT(GuiWindowInputKindNs, preferences_input, input);
EMBED_TABLE_STORAGE GuiWindowInputKindNs gui_window_input_kind EMBED_UNUSED = {
    .preferences_input = gui_window_preferences_input,
    .input = gui_window_input,
};

typedef struct
{
    unsigned int (*preferences_hit)(const GuiWindowInputCfg *args);
    unsigned int (*hit)(const GuiWindowInputCfg *args);
    unsigned long long (*timeout_ms)(const GuiWindowInputCfg *args);
} CliWindowInputNs;
EMBED_TABLE_LAYOUT(CliWindowInputNs, preferences_hit, hit, timeout_ms);
EMBED_TABLE_STORAGE CliWindowInputNs cli_window_input EMBED_UNUSED = {
    .preferences_hit = gui_window_preferences_hit,
    .hit = gui_window_hit,
    .timeout_ms = gui_window_timeout_ms,
};

typedef struct
{
    void (*autohide_layout)(const GuiWindowAutohideCfg *args);
} GuiWindowAutohideNs;
EMBED_TABLE_LAYOUT(GuiWindowAutohideNs, autohide_layout);
EMBED_TABLE_STORAGE GuiWindowAutohideNs gui_window_autohide EMBED_UNUSED = {
    .autohide_layout = gui_window_autohide_layout,
};

typedef struct
{
    void (*motion_wish)(const GuiWindowReducedCfg *args);
} GuiWindowReducedNs;
EMBED_TABLE_LAYOUT(GuiWindowReducedNs, motion_wish);
EMBED_TABLE_STORAGE GuiWindowReducedNs gui_window_reduced EMBED_UNUSED = {
    .motion_wish = gui_window_motion_wish,
};

typedef struct
{
    void (*show)(const GuiWindowDisplayedCfg *args);
} CliWindowDisplayedNs;
EMBED_TABLE_LAYOUT(CliWindowDisplayedNs, show);
EMBED_TABLE_STORAGE CliWindowDisplayedNs cli_window_shown EMBED_UNUSED = {
    .show = gui_window_show,
};

typedef struct
{
    int (*icon)(const GuiWindowIconCfg *args);
} CliWindowIconNs;
EMBED_TABLE_LAYOUT(CliWindowIconNs, icon);
EMBED_TABLE_STORAGE CliWindowIconNs cli_window_icon EMBED_UNUSED = {
    .icon = gui_window_icon,
};

#endif
