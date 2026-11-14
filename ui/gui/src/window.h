// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: the window on the view's thread (thread.c): its frame laid out for its size, its raster drawn with the
// parts, the glyphs and the palette they draw with, each part reading the command line's modules where their state
// stands; the pointer's place read as the caption, a border, a control or the client, and each input handed straight to
// the module it is for (window.c)
#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H
#include "../../../includes/window/base.h"
#include "../../../includes/window/block.h"
#include "../../../includes/window/busy.h"
#include "../../../includes/window/claim.h"
#include "../../../includes/window/dialog.h"
#include "../../../includes/window/docks.h"
#include "../../../includes/window/ground.h"
#include "../../../includes/window/side_menu.h"
#include "../../../includes/window/tip.h"
#include "../../../includes/window/wordmark.h"
#include "editor.h"
#include "embed_dispatch_layout.h"
#include "hover.h"
#include "menubar.h"
#include "minimap.h"
#include "pane.h"
#include "preferences.h"
#include "problems.h"
#include "signature.h"
#include "statusbar.h"
#include "suggest.h"
#include "tabs.h"
#include "terminal.h"
#include "themes.h"

// the rest after the last edit before auto save writes the changed tabs, in milliseconds, while no setting gives one
#define GUI_WINDOW_AUTOSAVE_DEFAULT_MS 1000ULL
// the most bytes a macro's record holds
#define GUI_WINDOW_MACRO_BYTES 8192u
#include "../../../includes/window/sides.h"
#include "stages.h"
#include "thread.h"

typedef struct GuiWindowWokenCfg GuiWindowWokenCfg;
typedef struct GuiWindowWidthCfg GuiWindowWidthCfg;
typedef struct GuiWindowIconCfg GuiWindowIconCfg;
typedef struct GuiWindowCfg GuiWindowCfg;
typedef struct GuiWindowHitsCfg GuiWindowHitsCfg;
typedef struct GuiWindowAutohideCfg GuiWindowAutohideCfg;
typedef struct GuiWindowInputCfg GuiWindowInputCfg;
typedef struct GuiWindowInputTypeCfg GuiWindowInputTypeCfg;
typedef struct GuiWindowReducedCfg GuiWindowReducedCfg;
typedef struct GuiWindowDisplayedCfg GuiWindowDisplayedCfg;

// the resize border's width in the page's pixels; the rectangles a map of what a pixel stands on holds, each title,
// each menu open and the eye; the milliseconds of one refresh of a monitor before the system has sent its own; and the
// milliseconds between readings of what orior takes in RAM
#define GUI_BORDER 5
#define GUI_WINDOW_HIT_CLIENTS (GUI_MENUBAR_TITLES + GUI_MENU_LEVELS + 1u + GUI_TOP_MENU_TOOLS)
#define GUI_VIEW_FRAME_MS 17u
#define GUI_MEMORY_MS 2000ULL

// The editor's scroll: the rows one notch of the wheel moves; the milliseconds in which an eased scroll covers all but
// a share of 1/e of its way; the most milliseconds one step counts, so that a late frame does not jump; and the part
// of a pixel within which a scroll stops
#define GUI_SCROLL_NOTCH_ROWS 3
#define GUI_SCROLL_EASE_MS 70.0
#define GUI_SCROLL_STEP_MAX_MS 40ULL
#define GUI_SCROLL_STILL 0.25
// The autoscroll a middle press in the text starts. The pixels either side of the press point scroll nothing. Past
// them, each pixel of the pointer's distance moves the scroll this many pixels a second
#define GUI_AUTOSCROLL_REST_PIXELS 8
#define GUI_AUTOSCROLL_PIXELS_A_SECOND 8.0

// What a pixel of the main window stands on, a CLI_HIT_*, with the level and the place among its rows of what it
// stands on, the row and the cell it stands on in the editor or a terminal's pane, and the pixel itself
typedef struct
{
    unsigned int hit;
    unsigned int level;
    unsigned int index;
    unsigned long long row;
    unsigned long long column;
    int x;
    int y;
} GuiWindowHit;

// The window: its frame; its two framebuffers, the raster the frame is drawn into and the one shown, flipped as a frame
// is presented, the shown one NULL before any; the base every frame starts from and the ground under it, glyphs, icons,
// palette; the parts and the side pane's waits, the editor's frame, token table and map, the terminal's panes and their
// frames, each taken of the module whose state it is and released by the window; the hover and the completion as the
// panels last had them; when the caret's blink begins, each frame of the view putting it an input rest later, the
// caret's ink in the last frame drawn, and whether its ink alone has changed, which the caret drawn again over the
// frame pays with no frame whole;
// the busy circle, with whether its stage alone has changed; Preferences, what its rows stand at, the palette of the
// window's own they change, and the scale the system gave, which the zoom lays the frame at a share of; the claim and
// whether its slides ran at the last tick; the controls' tab's shadow; the wordmark; less motion as the system and the
// settings ask it; the tree's name, the last part of its folder; the entries of orior's folder read as the window
// began, which the modules restored from them read in place until the window ends; whether the editor takes the keys
// and whether the window is to close; what orior takes in RAM and when it is next read; the view shown when the frame
// was last taken,
// -1 for none; the editor's rows and columns, the bar's titles shown and whether the rest fold, and the lines the map
// stands for, each as last handed on; the terminal's two panes' columns and rows and its height as last handed on; what
// the pointer last moved onto; whether the pointer drags the terminal's top edge, whether it drags in the editor, and
// the terminal's pane it drags in, one past it, 0 for none; the presses counted, when and where the last was; whether a
// frame is owed; and the control pressed, for the system to act on, GUI_HIT_CLIENT where none
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
    GuiSideMenu side_menu;
    unsigned int top_menu_tool_pointed;
    GuiSides sides;
    GuiDocks docks;
    GuiPane panes[GUI_PANE_TOOLS];
    GuiHover hover;
    GuiSuggest suggest;
    GuiSignature signature;
    GuiTip tip;
    GuiTerminal terminal;
    CliHoverPanel hover_panel;
    CliCompletionPanel completion_panel;
    unsigned long long caret_blink_start_ms;
    unsigned int caret_color;
    int caret_alone;
    GuiBusy busy;
    int busy_alone;
    GuiPreferences preferences;
    GuiPreferenceValues preference_values;
    GuiRaster preferences_raster;
    unsigned long long preferences_capacity;
    GuiBackground preferences_ground;
    GuiRaster preferences_ground_raster;
    unsigned int display_width;
    unsigned int display_height;
    int preferences_pending;
    int preferences_system_scale;
    // The editor's scrollbar down and across, and the tabs row's scrollbar. Then the scrollbar the pointer drags: 0 for
    // none, 1 down, 2 across and 3 the tabs row's. Then the pixels from the thumb's start to the press, and the tab
    // shown when the tabs row last moved to show it
    GuiScrollbar editor_down;
    GuiScrollbar editor_across;
    GuiScrollbar tabs_bar;
    unsigned int editor_bar_dragged;
    int editor_bar_grab;
    int tabs_shown_seen;
    // The editor's scroll in pixels down and across the file's grid: where it stands and where it goes. Each frame
    // moves it a share of the way to where it goes. Then the top line and the first cell column the view was last
    // sent, the clock of the last step in milliseconds, and whether the scroll moves
    // Down, the scroll counts pixels from any start, and the view is sent the whole rows it moves, as a wheel moves
    // them past the folds. Across, it counts pixels from the grid's first cell column. Last, the line a thumb's drag
    // puts the view at, applied at the next frame, ~0 for none
    double scroll_down;
    double scroll_down_to;
    double scroll_across;
    double scroll_across_to;
    unsigned long long scroll_top_sent;
    unsigned long long scroll_left_sent;
    unsigned long long scroll_stepped_ms;
    int scroll_moving;
    unsigned long long scroll_drag_top;
    // The autoscroll: whether it runs, the press point's y, the pointer's y as it last moved, whether the pointer
    // moved past the rest pixels while it ran, and the clock of its last step in milliseconds. It runs from a middle
    // press in the text to the next press, or to the middle button let go after such a move: a drag ends with its
    // button, a click leaves the scroll to run
    int autoscroll;
    int autoscroll_y;
    int autoscroll_pointer_y;
    int autoscroll_dragged;
    unsigned long long autoscroll_stepped_ms;
    // the union of the parts of the frame that changed since it was last shown, empty for none; a frame owed whole,
    // `pending`, draws it all
    GuiRect damage;
    // The base the last whole frame laid, the window's size, and whether it holds that base. A frame of changes alone
    // copies its part of the base from here, since the lattice and the grounds change only with a whole frame
    GuiRaster base_kept;
    int base_kept_whole;
    GuiDialog dialog;
    GuiRaster dialog_raster;
    unsigned long long dialog_capacity;
    GuiBackground dialog_ground;
    GuiRaster dialog_ground_raster;
    int dialog_pending;
    int dialog_system_scale;
    unsigned int dialog_owner;
    int dialog_held;
    GuiDialogChange dialog_answer;
    GuiPaletteOwn palette_own;
    // the palette Preferences shows while a language plugin's is chosen: the window's colors with that plugin's
    // tokens; and the id of the plugin whose tokens the window's palette holds, empty where it holds none
    GuiPaletteOwn palette_choice_own;
    char tokens_id[CLI_NAME_MAX_LEN];
    // the reader's themes in a movable cell of the window's, and the one chosen, drawn over the scheme and the color
    // settings: its place among them, or their count for the scheme's own colors, and its record read out of the cell
    GuiThemes themes;
    unsigned int theme_chosen;
    GuiTheme theme_shown;
    int system_scale;
    GuiAutohide claim;
    int autohide_moving;
    int buttons_moving;
    GuiStages stages;
    const GuiStage *stage;
    int stages_stale;
    GuiButtons top_menu_buttons;
    GuiButtons controls_buttons;
    GuiButtons tab_buttons;
    GuiWordmark wordmark;
    int motion_system;
    int motion_saved;
    int motion_still;
    int off_screen;
    int lattice_frozen;
    const char *title;
    CliStateEntries entries;
    int editing;
    // whether the scroll eases toward where it goes over frames, and whether the editor claims the window's edges
    // while editing, each the View menu's switch and its setting
    int scroll_smooth;
    int autohide_wanted;
    // whether changed tabs are saved on their own once the edits rest, the rest in milliseconds as the setting gives
    // it, the clock of the last edit in milliseconds, 0 with none standing; and whether the trust question stands in
    // the dialog, its answer taken at the next tick
    int autosave;
    unsigned long long autosave_ms;
    unsigned long long edited_ms;
    int trust_asked;
    // whether a save of the reader's own formats the text first, takes the blanks off its lines' ends and puts a line
    // end after its last line; auto save's writes do none of these
    int format_on_save;
    int trim;
    int final_newline;
    // the macro: whether the keys and texts the view takes are recorded, whether the record plays, and the record, a
    // line each, `key <name>` or `text <bytes>` with the text's backslashes and line breaks escaped; and whether an
    // operator sign goes on the next line, the Edit menu's switch, saved as its setting
    int macro_recording;
    int macro_playing;
    unsigned long long macro_length;
    char macro[GUI_WINDOW_MACRO_BYTES];
    int operator_next_line;
    int closing;
    unsigned long long memory_bytes;
    unsigned long long memory_due_ms;
    int view_shown;
    unsigned int rows;
    unsigned int columns;
    unsigned int bar_shown;
    unsigned int bar_folded;
    int map_known;
    unsigned long long map_first;
    unsigned long long map_last;
    // the key of the view whose language's palette stands as the window's tokens, empty for none
    char tokens_key[CLI_PATH_MAX_LEN];
    unsigned int terminal_columns[2];
    unsigned int terminal_rows[2];
    int terminal_gripped;
    unsigned int terminal_height;
    GuiWindowHit moved;
    int dragging;
    unsigned int terminal_dragged;
    unsigned int clicks;
    unsigned long long double_click_deadline_ms;
    int clicked_x;
    int clicked_y;
    int pending;
    unsigned int pressed;
} GuiWindow;

// the double-click window, the pixels a press may move and still count, the caret's blink, shown for the first half of
// its period and hidden for the second, and the input rest the caret stands shown through after a key before its blink
// begins
#define GUI_DOUBLE_CLICK_MS 500ull
#define GUI_CLICK_ADJACENT 4
#define GUI_CARET_BLINK_PERIOD_MS 1060ull
#define GUI_CARET_INPUT_REST_MS 300ull

// the window begun: its glyphs read, its palette the dark scheme, its parts showing nothing: 1, or 0 with the reason
// written; and ended, the session saved, the modules it began ended and every cell released
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
void gui_window_preferences_input(const GuiWindowInputTypeCfg *args);
unsigned int gui_window_preferences_hit(const GuiWindowInputCfg *args);
void gui_window_preferences_draw(const GuiWindowCfg *args);
// the window started on the tree the command line found, `woken` waking the view's thread from any thread: the settings
// orior's folder saved read, the modules begun, the session, the notices put away and the terminal restored, the
// command the window was started with run, and the first frame owed; and Preferences' window asked closed by the
// system
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
// The parts of the frame that changed drawn again on the raster shown, each part as the frame draws it, every write
// cut to the union of the changes: that union, the rows of which are to be shown again, empty where nothing changed
GuiRect gui_window_damage_draw(const GuiWindowCfg *args);
// what a pixel of the main window and of Preferences' window stands on, as they are laid, written into `hits`
void gui_window_hits(const GuiWindowHitsCfg *args);
void gui_window_preferences_hits(const GuiWindowHitsCfg *args);
// where the pointer on the input stands; and an input in the main window, as its table says
unsigned int gui_window_hit(const GuiWindowInputCfg *args);
void gui_window_input(const GuiWindowInputTypeCfg *args);
// on the window's thread, the modules stepped: the commands come through the door run, the servers' reports and the
// panels' timers taken, the terminal's shells drained, the notices' and the menus' timers taken, lines the walk colored
// taken into the frame, what orior takes in RAM read when its time comes; each that changed owing a frame
void gui_window_step(const GuiWindowCfg *args);
// the tick the window asked for come: the claim's timers run and its parts laid where its slides stand, and the
// ground drawn again where its loop shows a change; and when the window next wants a tick, in milliseconds, 0 for
// never
void gui_window_tick(const GuiWindowCfg *args);
unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args);
// the deadlines the window waits on, each one's name for the telemetry, and the place of the one that set the last
// wait, GUI_WINDOW_WAKES where none did
#define GUI_WINDOW_WAKES 23u
extern const char *const g_gui_window_wake_names[GUI_WINDOW_WAKES];
unsigned int gui_window_timeout_source(void);
// the frame laid for the claim's movement at `time_ms`: while a slide runs the stage standing then, the stages made
// where a slide has been sent since they were; at rest the frame the slides rest at
void gui_window_autohide_layout(const GuiWindowAutohideCfg *args);
// the system's wish for less motion set, 1 where it asks for less: that or View's Reduce Motion, saved in orior's
// folder, stills the lattice, the claim's slides, the name's flight and the caret's blink
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
    unsigned int segment;
};

struct GuiWindowCfg
{
    GuiWindow *window;
    void (*woken)(void);
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

struct GuiWindowInputTypeCfg
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
    GuiRect (*damage_draw)(const GuiWindowCfg *args);
    void (*preferences_close)(const GuiWindowCfg *args);
    void (*step)(const GuiWindowCfg *args);
    void (*tick)(const GuiWindowCfg *args);
    void (*hits)(const GuiWindowHitsCfg *args);
    void (*preferences_hits)(const GuiWindowHitsCfg *args);
} GuiWindowNs;
EMBED_TABLE_LAYOUT(GuiWindowNs, end, preferences_draw, start, draw, flip, caret_draw, busy_draw, damage_draw,
                   preferences_close, step, tick, hits, preferences_hits);
EMBED_TABLE_STORAGE GuiWindowNs gui_window_table EMBED_UNUSED = {
    .end = gui_window_end,
    .preferences_draw = gui_window_preferences_draw,
    .start = gui_window_start,
    .draw = gui_window_draw,
    .flip = gui_window_flip,
    .caret_draw = gui_window_caret_draw,
    .busy_draw = gui_window_busy_draw,
    .damage_draw = gui_window_damage_draw,
    .preferences_close = gui_window_preferences_close,
    .step = gui_window_step,
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
    void (*preferences_input)(const GuiWindowInputTypeCfg *args);
    void (*input)(const GuiWindowInputTypeCfg *args);
} GuiWindowInputTypeNs;
EMBED_TABLE_LAYOUT(GuiWindowInputTypeNs, preferences_input, input);
EMBED_TABLE_STORAGE GuiWindowInputTypeNs gui_window_input_type EMBED_UNUSED = {
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

// The dialog's system window (window_dialog.c), modal over the surface that owns it. A caller asks it with the texts
// and the surface that owns it. Where `held` is set, the door's held command takes the answer, the pressed button's
// label or nothing for a close. Else `dialog_answer` keeps the answer for the caller. The dialog shows only one ask at
// a time, so a second ask while one shows is refused
typedef struct GuiWindowDialogAskCfg GuiWindowDialogAskCfg;
typedef struct GuiWindowDialogWantCfg GuiWindowDialogWantCfg;
int gui_window_dialog_ask(const GuiWindowDialogAskCfg *args);
// the size the dialog's client wants at the main window's scale
void gui_window_dialog_want(const GuiWindowDialogWantCfg *args);
// its client sized at the scale the system draws it at; an input in it; where the pointer on the input stands; what a
// pixel stands on, written into `hits`; its frame drawn into its own raster; and the dialog closed with no answer, as
// the system asks
void gui_window_dialog_resize(const GuiWindowWidthCfg *args);
void gui_window_dialog_input(const GuiWindowInputTypeCfg *args);
unsigned int gui_window_dialog_hit(const GuiWindowInputCfg *args);
void gui_window_dialog_hits(const GuiWindowHitsCfg *args);
void gui_window_dialog_draw(const GuiWindowCfg *args);
void gui_window_dialog_close(const GuiWindowCfg *args);
struct GuiWindowDialogAskCfg
{
    GuiWindow *window;
    unsigned int owner;
    const char *title;
    const char *body;
    const char *const *labels;
    unsigned int count;
    unsigned int preferred;
    int held;
    int error;
};

struct GuiWindowDialogWantCfg
{
    GuiWindow *window;
    unsigned int *width_pointer;
    unsigned int *height_pointer;
};

typedef struct
{
    int (*ask)(const GuiWindowDialogAskCfg *args);
} GuiWindowDialogAskNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogAskNs, ask);
EMBED_TABLE_STORAGE GuiWindowDialogAskNs gui_window_dialog_asking EMBED_UNUSED = {
    .ask = gui_window_dialog_ask,
};

typedef struct
{
    void (*want)(const GuiWindowDialogWantCfg *args);
} GuiWindowDialogWantNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogWantNs, want);
EMBED_TABLE_STORAGE GuiWindowDialogWantNs gui_window_dialog_wanting EMBED_UNUSED = {
    .want = gui_window_dialog_want,
};

typedef struct
{
    void (*resize)(const GuiWindowWidthCfg *args);
} GuiWindowDialogWidthNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogWidthNs, resize);
EMBED_TABLE_STORAGE GuiWindowDialogWidthNs gui_window_dialog_width EMBED_UNUSED = {
    .resize = gui_window_dialog_resize,
};

typedef struct
{
    void (*input)(const GuiWindowInputTypeCfg *args);
} GuiWindowDialogInputTypeNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogInputTypeNs, input);
EMBED_TABLE_STORAGE GuiWindowDialogInputTypeNs gui_window_dialog_input_type EMBED_UNUSED = {
    .input = gui_window_dialog_input,
};

typedef struct
{
    unsigned int (*hit)(const GuiWindowInputCfg *args);
} GuiWindowDialogInputNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogInputNs, hit);
EMBED_TABLE_STORAGE GuiWindowDialogInputNs gui_window_dialog_input_hit EMBED_UNUSED = {
    .hit = gui_window_dialog_hit,
};

typedef struct
{
    void (*hits)(const GuiWindowHitsCfg *args);
} GuiWindowDialogHitsNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogHitsNs, hits);
EMBED_TABLE_STORAGE GuiWindowDialogHitsNs gui_window_dialog_hits_table EMBED_UNUSED = {
    .hits = gui_window_dialog_hits,
};

typedef struct
{
    void (*draw)(const GuiWindowCfg *args);
    void (*close)(const GuiWindowCfg *args);
} GuiWindowDialogNs;
EMBED_TABLE_LAYOUT(GuiWindowDialogNs, draw, close);
EMBED_TABLE_STORAGE GuiWindowDialogNs gui_window_dialog_table EMBED_UNUSED = {
    .draw = gui_window_dialog_draw,
    .close = gui_window_dialog_close,
};

#endif
