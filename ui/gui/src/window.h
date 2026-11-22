// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: the window on the view's thread (thread.c): its frame arranged for its size, its raster drawn with the
// parts, the glyphs and the palette they draw with, each part reading the command line's modules where their state
// is; the pointer's place read as the caption, a border, a control or the client, and each input passed straight to
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
#include "../../../includes/window/pointer.h"
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
#include "rename.h"
#include "signature.h"
#include "statusbar.h"
#include "suggest.h"
#include "tabs.h"
#include "terminal.h"
#include "themes.h"

// the rest after the last edit before auto save writes the changed tabs, in milliseconds, while no setting gives one
#define GUI_WINDOW_AUTOSAVE_DEFAULT_MS 1000ULL
// the max bytes a macro's record holds
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
typedef struct GuiWindowPointerCfg GuiWindowPointerCfg;
typedef struct GuiWindowDisplayedCfg GuiWindowDisplayedCfg;

// The resize border's width in the page's pixels. Then the rectangles a map of what a pixel is on holds: each title,
// each open menu and the eye. Then the milliseconds of one refresh of a monitor before the system sends its own. Last,
// the
#define GUI_BORDER 5
#define GUI_WINDOW_HIT_CLIENTS (GUI_MENUBAR_TITLES + GUI_MENU_LEVELS + 1u + GUI_TOP_MENU_TOOLS)
#define GUI_VIEW_FRAME_MS 17u
// The least milliseconds between two releases of the cellblock's free pages to the system, on ticks that run for other
// reasons. Then the bytes of the RAM readout's last digit, a tenth of a megabyte
#define GUI_MEMORY_MS 2000ULL
#define GUI_MEMORY_TENTH (1024ULL * 1024ULL / 10ULL)
// the growth of the working set after the last release that starts the next release: the pool's least released span
#define GUI_MEMORY_GROWN (64ULL << 10u)
// the inputs the window's own pointer drives that wait for the view's thread at once: a drag's press, its places and
// its release
#define GUI_WINDOW_DRIVEN 64u
// The hands: the pointers beside the window's own, each driven through the door under a name. Each has a surface of
// the system's own and a mark. A hand's mark is the palette's mark color at the hand's place in the ring. Its picture
// is the pointer's sprite baked in that color. The hands and the window's own pointer take the system's surfaces, one
// each
#define GUI_WINDOW_HANDS 8u
#define GUI_WINDOW_HAND_NAME 64u
// A glide's speed in the client's pixels a second, if the pointer command gives none, and the longest time a frame
// counts toward it. A stall moves the pointer no further than that.
#define GUI_GLIDE_SPEED 1600u
#define GUI_GLIDE_STALL_MS 100ULL
_Static_assert(GUI_WINDOW_HANDS <= GUI_MARKS, "each hand has a mark color of the palette's ring");

// A hand: its name, whether it is in use, and where its hot point is in the client's pixels. Then its shape, whether
// it is on the window, and its picture as baked
typedef struct
{
    char name[GUI_WINDOW_HAND_NAME];
    int used;
    int x;
    int y;
    unsigned int shape;
    int shown;
    GuiColor picture[GUI_POINTER_PICTURE_MAX];
} GuiHand;

// A glide is a pointer's move to a control at a speed. The record holds whether one is under way, and the client's
// pixel it goes to. Then its speed in pixels a second, the moment of its last step, and where the pointer is now.
typedef struct
{
    int active;
    int to_x;
    int to_y;
    unsigned int speed;
    unsigned long long last_ms;
    float x;
    float y;
} GuiGlide;

// The editor's scroll: the rows one notch of the wheel moves. Then the milliseconds in which an eased scroll covers all
// but a share of 1/e of its way. Then the max milliseconds one step counts, and a late frame does not jump. Last, the
// part of a pixel within which a scroll stops
#define GUI_SCROLL_NOTCH_ROWS 3
#define GUI_SCROLL_EASE_MS 70.0
#define GUI_SCROLL_STEP_MAX_MS 40ULL
#define GUI_SCROLL_STILL 0.25
// The autoscroll a middle press in the text starts. The pixels either side of the press point scroll nothing, down
// and across apart. Past them, each pixel of the pointer's distance moves the scroll this many pixels a second
#define GUI_AUTOSCROLL_REST_PIXELS 8
#define GUI_AUTOSCROLL_PIXELS_A_SECOND 8.0

// What a pixel of the main window is on, a CLI_HIT_*, with the level and the place among its rows of what it is on.
// Then the row and the cell it is on in the editor or a terminal's pane, and the pixel itself
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

// The window. It holds its frame, and its two framebuffers: the raster the frame draws into, and the one on display.
// They flip as a frame goes up, and the one on display is NULL before any. Then the base every frame starts from and
// the ground under it, the glyphs, the icons and the palette.
//
// Then the parts and the side pane's waits, and the editor and the terminal's panes. Each reads its view's frame for
// one call of the window, and the window gives the frame back as that call returns. Then the editor's token table and
// map, and the hover and the completion as the panels last had them.
//
// Then when the caret's blink begins, as each frame of the view puts it an input rest later. Then the caret's ink in
// the last frame drawn, and whether its ink alone changed. A draw of the caret again over the frame pays for that,
// with no frame whole. Then the busy circle, with whether its stage alone changed.
//
// Then the pointer drawn on the main window, with whether its move alone owes a draw. Then the pointers drawn on
// Preferences' window and the dialog's. Then Preferences, what its rows are at, and the palette of the window's own
// they change. Then the scale the system gave, and the zoom lays the frame out at a share of it.
//
// Then the claim, and whether its slides ran at the last tick. Then the controls' tab's shadow, the wordmark, and less
// motion as the system and the settings ask it. Then the tree's name, the last part of its folder. Then whether the
// editor takes the keys, and whether the window must close. Then what orior takes in RAM, and the time of its next
// read.
//
// Then the view on display at the last frame take, -1 for none. Then the editor's rows and columns, the bar's titles on
// display, whether the rest fold, and the lines of the map. Each is as the window last passed it on. Then the
// terminal's two panes' columns and rows, and its height, as the window last passed them on.
//
// Then what the pointer last moved onto, and whether the pointer drags the terminal's top edge or drags in the editor.
// Then the terminal's pane it drags in, one past it, 0 for none. Then the presses counted, with when and where the
// last was. Then whether the window owes a frame. Last, the control pressed for the system to act on, GUI_HIT_CLIENT
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
    GuiSideMenu side_menu;
    unsigned int top_menu_tool_pointed;
    GuiSides sides;
    GuiDocks docks;
    GuiPane panes[GUI_PANE_TOOLS];
    GuiHover hover;
    GuiSuggest suggest;
    GuiSignature signature;
    GuiRename rename;
    GuiTip tip;
    GuiTerminal terminal;
    CliHoverPanel hover_panel;
    CliCompletionPanel completion_panel;
    unsigned long long caret_blink_start_ms;
    unsigned int caret_color;
    int caret_alone;
    GuiBusy busy;
    int busy_alone;
    GuiPointer pointer;
    // The mouse's raw input and the pointer's lock as the door last asked for them, each ordered of the system by the
    // view's thread. Then the mouse's relative motion added up since the last read, and a read of it the door holds
    int raw_wanted;
    int lock_wanted;
    long long relative_x;
    long long relative_y;
    int relative_read_held;
    // A sessions command the door keeps while the sessions module gathers its answer. Then a door macro command the
    // door keeps while a macro's steps go out
    int sessions_held;
    int macro_held;
    GuiPointer preferences_pointer;
    GuiPointer dialog_pointer;
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
    // displayed when the tabs row last moved to show it
    GuiScrollbar editor_down;
    GuiScrollbar editor_across;
    GuiScrollbar tabs_bar;
    unsigned int editor_bar_dragged;
    int editor_bar_grab;
    int tabs_displayed_seen;
    // The editor's scroll in pixels down and across the file's grid: where it is and where it goes. Each frame moves it
    // a share of the way to where it goes. Then the top line and the first cell column the window last sent the view.
    // Then the clock of the last step in milliseconds, and whether the scroll moves.
    //
    // Down, the scroll counts pixels from any start, and the view gets the whole rows it moves, as a wheel moves them
    // past the folds. Across, it counts pixels from the grid's first cell column. Last, the line a thumb's drag puts
    // the view at, applied at the next frame, ~0 for none
    double scroll_down;
    double scroll_down_to;
    double scroll_across;
    double scroll_across_to;
    unsigned long long scroll_top_sent;
    unsigned long long scroll_left_sent;
    unsigned long long scroll_stepped_ms;
    int scroll_moving;
    unsigned long long scroll_drag_top;
    // The autoscroll: whether it runs, the press point, and the pointer's place as it last moved. Then whether the
    // pointer moved past the rest pixels while it ran, and the clock of its last step in milliseconds. It runs from a
    // middle press in the text to the next press. It also ends where the middle button goes up after such a move. A
    // drag ends with its button, and a click leaves the scroll to run. It glides down and across each by the pointer's
    // distance that way
    int autoscroll;
    int autoscroll_x;
    int autoscroll_y;
    int autoscroll_pointer_x;
    int autoscroll_pointer_y;
    int autoscroll_dragged;
    unsigned long long autoscroll_stepped_ms;
    // the union of the parts of the frame that changed since it was last displayed, empty for none; a frame owed whole,
    // `pending`, draws it all
    GuiRect damage;
    // The base the last whole frame positioned, the window's size, and whether it holds that base. A frame of changes
    // alone copies its part of the base from here, since the lattice and the grounds change only with a whole frame
    GuiRaster base_retained;
    int base_retained_whole;
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
    // The palette Preferences shows while the reader chose a language plugin's: the window's colors with that plugin's
    // tokens. Then the id of the plugin whose tokens the window's palette holds, empty where it holds none
    GuiPaletteOwn palette_choice_own;
    char tokens_id[CLI_NAME_MAX_LEN];
    // The reader's themes in a movable cell of the window's, and the one chosen, drawn over the scheme and the color
    // settings. Then its place among them, or their count for the scheme's own colors, and its record read out of the
    // cell
    GuiThemes themes;
    unsigned int theme_chosen;
    GuiTheme theme_displayed;
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
    int editing;
    // whether another window of the system has the keys: the caret stands still and sets no deadline while it does
    int blurred;
    // Whether the scroll eases toward where it goes over frames, and whether the editor claims the window's edges
    // during an edit. Each is the View menu's switch and its setting
    int scroll_smooth;
    int autohide_wanted;
    // Whether the window saves changed tabs on its own once the edits rest, and the rest in milliseconds as the setting
    // gives it. Then the clock of the last edit in milliseconds, 0 with none. Last, whether the trust question is in
    // the dialog, its answer taken at the next tick
    int autosave;
    unsigned long long autosave_ms;
    unsigned long long edited_ms;
    int trust_asked;
    // whether a script plugin's question of a right is in the dialog, its answer taken at the next tick
    int rights_asked;
    // Whether a save of the reader's own formats the text first, and takes the blanks off its lines' ends. Then
    // whether it puts a line end after its last line. Auto save's writes do none of these
    int format_on_save;
    int trim;
    int final_newline;
    // The macro: whether it records the keys and texts the view takes, and whether the record plays. Then the record,
    // a line each, `key <name>` or `text <bytes>`, with the text's backslashes and line breaks escaped. Last, whether
    // an operator sign goes on the next line, the Edit menu's switch, saved as its setting
    int macro_recording;
    int macro_playing;
    unsigned long long macro_length;
    char macro[GUI_WINDOW_MACRO_BYTES];
    int operator_next_line;
    int closing;
    unsigned long long memory_bytes;
    unsigned long long memory_due_ms;
    // the half blink of the state dots at the last step. A dot that blinks draws again at the next half
    unsigned long long dots_blink;
    // the least working set after the last release of the cellblock's free pages, and when the last input came
    unsigned long long memory_released_bytes;
    unsigned long long input_ms;
    int view_displayed;
    unsigned int rows;
    unsigned int columns;
    unsigned int bar_displayed;
    unsigned int bar_folded;
    int map_known;
    unsigned long long map_first;
    unsigned long long map_last;
    // The text and the colors serial the names and the map were last read at, and whether the last view take read them
    // again
    char colors_key[CLI_NAME_MAX_LEN];
    unsigned int colors_serial;
    int recolored;
    // the key of the view whose language's palette is as the window's tokens, empty for none
    char tokens_key[CLI_PATH_MAX_LEN];
    unsigned int terminal_columns[2];
    unsigned int terminal_rows[2];
    int terminal_gripped;
    unsigned int terminal_height;
    GuiWindowHit moved;
    int dragging;
    // the table column line gripped, counted from one, 0 for none: the drag sets the width of the column before it
    unsigned int column_drag;
    // Whether the window's own pointer is on. A test moves it through the interface's pointer command, and the window
    // does not read the system's leaves while it is on
    int pointer_own;
    GuiHand hands[GUI_WINDOW_HANDS];
    // The glide of the window's own pointer is at 0, and each hand's glide follows it. A glide is a move to a
    // control at a speed, one step a frame.
    GuiGlide glide[GUI_WINDOW_HANDS + 1u];
    // the inputs the window's own pointer and the hands drive, in their order, that the view's thread takes as the
    // system's
    GuiInput driven[GUI_WINDOW_DRIVEN];
    unsigned int driven_count;
    unsigned int terminal_dragged;
    unsigned int clicks;
    unsigned long long double_click_deadline_ms;
    int clicked_x;
    int clicked_y;
    int pending;
    // Whether the window writes its telemetry lines to the error stream. They are the cells' log on the memory tick,
    // the frames' line, the walk's line a step and a frame take's line. They are off until the interface's
    // `telemetry on`
    int telemetry;
    unsigned int pressed;
} GuiWindow;

// The double-click window and the pixels a press may move and still count. The caret's blink shows the caret for the
// first half of its period and hides it for the second. After a key, the caret shows through the input rest before
// its blink begins.
#define GUI_DOUBLE_CLICK_MS 500ULL
#define GUI_CLICK_ADJACENT 4
#define GUI_CARET_BLINK_PERIOD_MS 1060ULL
#define GUI_CARET_INPUT_REST_MS 300ULL

// The window begun: its glyphs read, its palette the dark scheme, and its parts empty. Returns 1, or 0 with the reason
// written. Then the window ended: the session saved, the modules it began ended, and every cell released
int gui_window_begin(const GuiWindowWokenCfg *args);
void gui_window_end(const GuiWindowCfg *args);
// the window sized `width` by `height` at `scale`, its frame laid out and its raster made anew: 1, or 0 where no cell
// for the raster is there
int gui_window_resize(const GuiWindowWidthCfg *args);
// Preferences' system window, which the program opens and closes as `preferences.displayed` says. Its client takes the
// size at the scale the system draws it at, with the zoom over that. Then an input in it, and none of them are the
// main window's, nor the other way. Then where the pointer on the input is, as the names above give the places. Last,
// its frame drawn into its own raster, and the frame owed no more
void gui_window_preferences_resize(const GuiWindowWidthCfg *args);
void gui_window_preferences_input(const GuiWindowInputTypeCfg *args);
unsigned int gui_window_preferences_hit(const GuiWindowInputCfg *args);
// The pointer of Preferences' window or the dialog's put at the input's place, in the shape of the border `hit` names
// and the arrow elsewhere. Where the input is a leave, it goes off the window. Returns 1 where it changed, which owes
// its frame
int gui_window_pointer_surface(const GuiWindowPointerCfg *args);
void gui_window_preferences_draw(const GuiWindowCfg *args);
// The window started on the tree the command line found, and `woken` wakes the view's thread from any thread. The
// settings in orior's folder come in, and the modules begin. The session, the notices put away and the terminal come
// back. The command the window started with runs, and the window owes the first frame. Then Preferences' window, which
// the system asks to close
void gui_window_start(const GuiWindowCfg *args);
void gui_window_preferences_close(const GuiWindowCfg *args);
// The frame drawn into the raster not on display, and the frame owed no more. Then the two flip once that frame goes
// up: the raster drawn goes on display, and the next draw goes into the other
void gui_window_draw(const GuiWindowCfg *args);
void gui_window_flip(const GuiWindowCfg *args);
// The caret drawn again on the raster on display, over the frame last drawn, at the ink its breath is at now. That
// pays the frame owed for its ink alone. Returns the caret's rectangle, whose rows go on display again, empty where no
// caret is. Then the busy circle's stage at now blitted again on the raster on display, which pays its step. It returns
// its rectangle the same way
GuiRect gui_window_caret_draw(const GuiWindowCfg *args);
GuiRect gui_window_busy_draw(const GuiWindowCfg *args);
// The parts of the frame that changed drawn again on the raster on display, each part as the frame draws it. Every
// write stays inside the union of the changes. Returns that union, whose rows go on display again, empty where nothing
// changed
GuiRect gui_window_damage_draw(const GuiWindowCfg *args);
// what a pixel of the main window and of Preferences' window is on, as their layout puts them, written into `hits`
void gui_window_hits(const GuiWindowHitsCfg *args);
void gui_window_preferences_hits(const GuiWindowHitsCfg *args);
// where the pointer on the input is; and an input in the main window, as its table says
unsigned int gui_window_hit(const GuiWindowInputCfg *args);
void gui_window_input(const GuiWindowInputTypeCfg *args);
// On the window's thread, the modules stepped. The commands that came through the interface run, and the servers'
// reports and the panels' timers come in. The terminal's shells drain, and the notices' and the menus' timers come in.
// Lines the walk colored go into the frame, and the window reads what orior takes in RAM when its time comes. Each that
// changed owes a frame
void gui_window_step(const GuiWindowCfg *args);
// The tick the window asked for comes. The claim's timers run, and its parts go where its slides are. The ground draws
// again where its loop shows a change. Then when the window next wants a tick, in milliseconds, 0 for never
void gui_window_tick(const GuiWindowCfg *args);
unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args);
// The deadlines the window waits on, each one's name for the telemetry, and the place of the one that set the last
// wait. It is GUI_WINDOW_WAKES where none did
#define GUI_WINDOW_WAKES 23u
extern const char *const g_gui_window_wake_names[GUI_WINDOW_WAKES];
unsigned int gui_window_timeout_source(void);
// The frame laid out for the claim's movement at `time_ms`. While a slide runs, it takes the stage of that moment, and
// the stages come anew where a slide started since they came. At rest it is the frame the slides rest at
void gui_window_autohide_layout(const GuiWindowAutohideCfg *args);
// The system's wish for less motion set, 1 where it asks for less. That wish, or View's Reduce Motion saved in orior's
// folder, stills the lattice, the claim's slides, the name's flight and the caret's blink
void gui_window_motion_wish(const GuiWindowReducedCfg *args);
// the window gone off the screen, `displayed` 0, its ground's clock stationary, or back on it, 1, a frame owed
void gui_window_show(const GuiWindowDisplayedCfg *args);
// The window's icon, the wordmark's eye in the text's color, made into `icon` at `size` pixels a side with its own
// alpha, for the system to take. Returns 1, or 0 where the draw of the eye or the raster fails. It goes back as a
// raster
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

struct GuiWindowPointerCfg
{
    GuiPointer *pointer;
    const GuiInput *input;
    unsigned int hit;
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
EMBED_TABLE_STORAGE CliWindowDisplayedNs cli_window_displayed EMBED_UNUSED = {
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

// The dialog's system window (window_dialog.c) is modal over the surface that owns it. A caller asks it with the
// texts and the surface that owns it. Where `held` is 1, the open interface command takes the answer. The answer is
// the pressed button's label, or nothing for a close. Else `dialog_answer` keeps the answer for the caller. The
// dialog shows one ask at a time and refuses a second ask while one shows.
typedef struct GuiWindowDialogAskCfg GuiWindowDialogAskCfg;
typedef struct GuiWindowDialogWantCfg GuiWindowDialogWantCfg;
int gui_window_dialog_ask(const GuiWindowDialogAskCfg *args);
// the size the dialog's client wants at the main window's scale
void gui_window_dialog_want(const GuiWindowDialogWantCfg *args);
// Its client takes the size at the scale the system draws it at. Then an input in it, and where the pointer on the
// input is. Then what a pixel is on, written into `hits`, and its frame drawn into its own raster. Last, the dialog
// closed with no answer, as the system asks
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

typedef struct
{
    int (*surface)(const GuiWindowPointerCfg *args);
} GuiWindowPointerNs;
EMBED_TABLE_LAYOUT(GuiWindowPointerNs, surface);
EMBED_TABLE_STORAGE GuiWindowPointerNs gui_window_pointer_table EMBED_UNUSED = {
    .surface = gui_window_pointer_surface,
};

#endif
