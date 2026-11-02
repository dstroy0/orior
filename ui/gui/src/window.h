// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: the window apart from the system it stands on: its frame laid out for its size, its raster drawn with the
// parts, the glyphs and the palette they draw with, the editor showing the open file with the minimap beside it, the
// menu bar over the catalog of commands, and the link to the CLI, each answer and event taken on the window's thread;
// the pointer's place read as the caption, a border, a control or the client; the wheel scrolling the editor; a menu's
// choice run, its keys sent to the editor's view; and the file opened as the launch command or a door command names it
// (window.c)
#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H
typedef struct GuiWindowWokenCfg GuiWindowWokenCfg;
typedef struct GuiWindowWidthCfg GuiWindowWidthCfg;
typedef struct GuiWindowIconCfg GuiWindowIconCfg;
typedef struct GuiWindowTextCfg GuiWindowTextCfg;
typedef struct GuiWindowCfg GuiWindowCfg;
typedef struct GuiWindowClaimCfg GuiWindowClaimCfg;
typedef struct GuiWindowPathCfg GuiWindowPathCfg;
typedef struct GuiWindowInputCfg GuiWindowInputCfg;
typedef struct GuiWindowInputKindCfg GuiWindowInputKindCfg;
typedef struct GuiWindowReducedCfg GuiWindowReducedCfg;
typedef struct GuiWindowShownCfg GuiWindowShownCfg;
typedef struct GuiWindowAtCfg GuiWindowAtCfg;
#include "base.h"
#include "claim.h"
#include "embed_dispatch_layout.h"
#include "framebuffer.h"
#include "ground.h"
#include "input_event.h"
#include "link.h"
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
#include "parts/wordmark.h"
#include "stages.h"

// where the pointer stands: the client, the caption the system drags the window by, the borders and corners the
// system resizes it by, and the three controls
#define GUI_HIT_CLIENT 0u
#define GUI_HIT_CAPTION 1u
#define GUI_HIT_LEFT 2u
#define GUI_HIT_RIGHT 3u
#define GUI_HIT_TOP 4u
#define GUI_HIT_BOTTOM 5u
#define GUI_HIT_TOP_LEFT 6u
#define GUI_HIT_TOP_RIGHT 7u
#define GUI_HIT_BOTTOM_LEFT 8u
#define GUI_HIT_BOTTOM_RIGHT 9u
#define GUI_HIT_MINIMIZE 10u
#define GUI_HIT_MAXIMIZE 11u
#define GUI_HIT_CLOSE 12u
// the resize border's width in the page's pixels
#define GUI_BORDER 5
// The window: its frame; its two framebuffers, the raster the frame is drawn into and the one shown, flipped as a frame
// is presented, the shown one NULL before any; the base every frame starts from and the ground under it, glyphs, icons,
// palette, editor, minimap, menu The window: its frame, raster, the ground every frame starts from, glyphs, icons,
// palette, editor, minimap, menu bar, strip, explorer and link; the panels over the editor, the hover, the completion
// and the signature, with the tickets of their asks in flight, 0 where none is, whether the completion is to be asked
// for as the next frame comes, whether that ask was forced by the keys, whether each panel's ask is to be made again
// with the server once its answer without the server comes, whether the server is to be told the text as the next frame
// comes, an input having changed it, and the cursor's place the signature was last asked at; the problems pane, the
// ticket of the tree's check asked for, and the place the shown file's cursor is to be put at as its next frame comes,
// where one is wanted; when the caret's breath last began, each frame of the view beginning it again, the caret's ink
// in the last frame drawn, and whether its ink alone has changed, which the caret drawn again over the frame pays with
// no frame whole; the busy circle, with whether its stage alone has changed, which the stage blitted over the frame
// pays the same way; Preferences, what its rows stand at, the palette of the window's own they change, and the scale
// the system gave, which the zoom lays the frame at a share of; when the view's rows last moved, 0 once the server was
// told the view they rest at, whether it was told, and the top and rows it was told; the claim and whether its slides
// ran at the last tick, the view told the editor's size as they stop; the controls' tab's shadow, made once it is
// wanted; the wordmark, the tickets of what orior's folder saves as read for it and of the session the system names,
// and the session saved, its size; the catalog of commands the menu bar reads, parsed into a JSON tree of its own once
// it came; the tree's name shown in the bar; the tree's root as the CLI names it, empty where none is open; the files
// open as tabs, how many, the one shown, -1 for none, and the one shown before it, -1 for none; the shown file's path
// and the key it is stored under; whether the window is maximized; the tickets of the calls in flight, the launch
// command, the root, the catalog, a whole path made the tree's, what orior's folder saves, the view's open, the memory
// reading and the map's lines, each 0 where none is, whether the map is wanted again as its answer comes, when the
// memory was last asked and the bytes it answered, and the sequence of the last frame taken; whether the tree's saved
// tabs are back, the session saved again only after, and whether a session for the tree is known, read from the folder
// or made by the tabs' own changes, without which no tabs are saved over whatever the folder holds; the session saved
// again only after; whether the CLI's editor view is open on the shown file, whether the pointer drags in it, and the
// presses counted, when and where the last was; whether a frame is owed; and the control pressed, for the system to
// act on, GUI_HIT_CLIENT where none
typedef struct GuiWindow
{
    GuiFrame frame;
    GuiFramebuffer framebuffer;
    GuiRaster *raster;
    GuiRaster *shown;
    GuiBase base;
    GuiGround ground;
    GuiGlyphs glyphs;
    GuiIcons icons;
    const GuiPalette *palette;
    GuiEditor editor;
    GuiMinimap minimap;
    GuiMenubar menubar;
    GuiStrip strip;
    GuiExplorer explorer;
    GuiLink link;
    GuiHover hover;
    GuiSuggest suggest;
    GuiSignature signature;
    unsigned long long asked_hover;
    unsigned long long asked_complete;
    unsigned long long asked_signature;
    int complete_wanted;
    int complete_forced;
    int complete_again;
    int hover_again;
    int signature_again;
    int told_wanted;
    GuiPlace signature_place;
    GuiProblems problems;
    unsigned long long asked_check;
    GuiPlace place_wanted;
    int place_pending;
    unsigned long long caret_blink_start_ms;
    unsigned int caret_ink;
    int caret_alone;
    GuiBusy busy;
    int busy_alone;
    GuiPreferences preferences;
    GuiPreferenceValues preference_values;
    GuiRaster preferences_raster;
    int preferences_owed;
    int preferences_system_scale;
    GuiPaletteOwn palette_own;
    int system_scale;
    unsigned long long viewport_notify_deadline_ms;
    int view_told;
    unsigned long long view_told_top;
    unsigned int view_told_rows;
    GuiClaim claim;
    int claim_moving;
    GuiStages stages;
    const GuiStage *stage;
    int stages_stale;
    GuiMask controls_shadow;
    GuiWordmark wordmark;
    unsigned long long asked_mark_session;
    int motion_system;
    int motion_saved;
    int motion_still;
    int off_screen;
    CliJson catalog;
    int cataloged;
    char title[CLI_NAME_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    GuiTab tab[GUI_TABS_MOST];
    unsigned int tabs;
    int active;
    int last_active;
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    int maximized;
    unsigned long long asked_launch;
    unsigned long long asked_root;
    unsigned long long asked_catalog;
    unsigned long long asked_relative;
    unsigned long long asked_state;
    unsigned long long asked_view;
    unsigned long long asked_memory;
    unsigned long long asked_map;
    int map_wanted;
    unsigned long long memory_poll_deadline_ms;
    unsigned long long memory_bytes;
    unsigned long long frame_seq;
    int restored;
    int session_known;
    int viewed;
    int dragging;
    unsigned int clicks;
    unsigned long long double_click_deadline_ms;
    int clicked_x;
    int clicked_y;
    int owed;
    unsigned int pressed;
} GuiWindow;

// the milliseconds within which presses count up, the pixels a press may move and still count, the milliseconds
// between readings of the memory, the milliseconds the view's rows rest before the server is told the view, its
// slice cut for it, and the caret's breath: its period; each slide, down from full ink and up again, which the caret
// eases through on a cosine, the rests at full and at dim between them taking what the period leaves; and the dim
// rest's ink, 0 to 255, where the caret rests before it rises
#define GUI_DOUBLE_CLICK_MS 500ull
#define GUI_CLICK_NEAR 4
#define GUI_MEMORY_POLL_MS 2000ull
#define GUI_VIEWPORT_DEBOUNCE_MS 250ull
#define GUI_CARET_BLINK_PERIOD_MS 1500ull
// the entry orior's folder saves View's Reduce Motion under
#define GUI_WINDOW_REDUCE_KEY "orior.reduce-motion"
// the keys Preferences saves under in orior's folder, a color's key its name after the last; and the scheme's own
// opacities
#define GUI_WINDOW_ZOOM_KEY "orior.zoom"
#define GUI_WINDOW_BAR_KEY "orior.opacity.bar"
#define GUI_WINDOW_EDITOR_KEY "orior.opacity.editor"
#define GUI_WINDOW_COLOR_KEY "orior.color."
#define GUI_WINDOW_BAR_OPACITY 30u
#define GUI_WINDOW_EDITOR_OPACITY 80u
#define GUI_CARET_FADE_MS 300ull
#define GUI_CARET_DIM 64u

// the window begun: its glyphs read, its palette the dark scheme, its editor empty and its link's workers started, the
// window's thread woken through `woken` with `context`: 1, or 0 with why given; and ended, every cell given back
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
// the first calls made as the window opens: the launch command taken and the tree's root asked
void gui_window_start(const GuiWindowCfg *args);
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
// where the pointer on the input stands; and an input in the main window, as its kind says
unsigned int gui_window_hit(const GuiWindowInputCfg *args);
void gui_window_input(const GuiWindowInputKindCfg *args);
// a menu's choice run: an item with keys runs as those keys pressed, the editor's view taking them; a line edit with
// none runs by its name, the view taking that; `exit` closes the window as the close control does; `close-editor`
// closes the tab shown, `close-all` every tab, and `last-editor` shows the tab shown before
void gui_window_choose(const GuiWindowCfg *args);
// the file at `path` opened through the CLI as a tab, shown in the editor as its answers come, or shown where it is
// open already; the tab `at` shown; and the tab `at` closed, its view and its branch let go, the tab beside it shown
void gui_window_file_open(const GuiWindowPathCfg *args);
void gui_window_tab_show(const GuiWindowAtCfg *args);
void gui_window_tab_close(const GuiWindowAtCfg *args);
// `length` bytes of a text shown whole in the editor
int gui_window_text_put(const GuiWindowTextCfg *args);
// every answer and event waiting taken, on the window's thread
void gui_window_drain(const GuiWindowCfg *args);
// the tick the window asked for come: the claim's timers run and its parts laid where its slides stand, and the
// ground drawn again where its loop shows a change; and when the window next wants a tick, in milliseconds, 0 for
// never
void gui_window_tick(const GuiWindowCfg *args);
unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args);
// the frame laid for the claim's movement at `now`: while a slide runs the stage standing then, the stages made where a
// slide has been sent since they were; at rest the frame the slides rest at
void gui_window_claim_layout(const GuiWindowClaimCfg *args);
// the system's wish for less motion told, 1 where it asks for less: that or View's Reduce Motion, saved in orior's
// folder, stills the lattice, the claim's slides, the name's flight and the caret's breath
void gui_window_motion_tell(const GuiWindowReducedCfg *args);
// the window gone off the screen, `shown` 0, its ground's clock standing still, or back on it, 1, a frame owed
void gui_window_show(const GuiWindowShownCfg *args);
// the window's icon, the wordmark's eye in the text's color, made into `icon` at `size` pixels a side with its own
// alpha, for the system to take: 1, or 0 where the eye could not be drawn or the raster made; given back as a raster
int gui_window_icon(const GuiWindowIconCfg *args);
struct GuiWindowWokenCfg
{
    GuiWindow *window;
    GuiLinkWoken woken;
    void *context;
    VerbaLinea *err_msg;
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

struct GuiWindowTextCfg
{
    GuiWindow *window;
    const char *text;
    unsigned long long length;
};

struct GuiWindowCfg
{
    GuiWindow *window;
    const GuiChoice *choice;
};

struct GuiWindowClaimCfg
{
    GuiWindow *window;
    unsigned long long time_ms;
};

struct GuiWindowPathCfg
{
    GuiWindow *window;
    const char *path;
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

struct GuiWindowShownCfg
{
    GuiWindow *window;
    int shown;
};

struct GuiWindowAtCfg
{
    GuiWindow *window;
    unsigned int at;
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
    void (*choose)(const GuiWindowCfg *args);
    void (*drain)(const GuiWindowCfg *args);
    void (*tick)(const GuiWindowCfg *args);
} GuiWindowNs;
EMBED_TABLE_LAYOUT(GuiWindowNs, end, preferences_draw, start, draw, flip, caret_draw, busy_draw, choose, drain, tick);
EMBED_TABLE_STORAGE GuiWindowNs gui_window_table EMBED_UNUSED = {
    .end = gui_window_end,
    .preferences_draw = gui_window_preferences_draw,
    .start = gui_window_start,
    .draw = gui_window_draw,
    .flip = gui_window_flip,
    .caret_draw = gui_window_caret_draw,
    .busy_draw = gui_window_busy_draw,
    .choose = gui_window_choose,
    .drain = gui_window_drain,
    .tick = gui_window_tick,
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
    void (*file_open)(const GuiWindowPathCfg *args);
} GuiWindowPathNs;
EMBED_TABLE_LAYOUT(GuiWindowPathNs, file_open);
EMBED_TABLE_STORAGE GuiWindowPathNs gui_window_path EMBED_UNUSED = {
    .file_open = gui_window_file_open,
};

typedef struct
{
    void (*tab_show)(const GuiWindowAtCfg *args);
    void (*tab_close)(const GuiWindowAtCfg *args);
} GuiWindowAtNs;
EMBED_TABLE_LAYOUT(GuiWindowAtNs, tab_show, tab_close);
EMBED_TABLE_STORAGE GuiWindowAtNs gui_window_at EMBED_UNUSED = {
    .tab_show = gui_window_tab_show,
    .tab_close = gui_window_tab_close,
};

typedef struct
{
    int (*text_put)(const GuiWindowTextCfg *args);
} GuiWindowTextNs;
EMBED_TABLE_LAYOUT(GuiWindowTextNs, text_put);
EMBED_TABLE_STORAGE GuiWindowTextNs gui_window_text_table EMBED_UNUSED = {
    .text_put = gui_window_text_put,
};

typedef struct
{
    void (*claim_layout)(const GuiWindowClaimCfg *args);
} GuiWindowClaimNs;
EMBED_TABLE_LAYOUT(GuiWindowClaimNs, claim_layout);
EMBED_TABLE_STORAGE GuiWindowClaimNs gui_window_claim EMBED_UNUSED = {
    .claim_layout = gui_window_claim_layout,
};

typedef struct
{
    void (*motion_tell)(const GuiWindowReducedCfg *args);
} GuiWindowReducedNs;
EMBED_TABLE_LAYOUT(GuiWindowReducedNs, motion_tell);
EMBED_TABLE_STORAGE GuiWindowReducedNs gui_window_reduced EMBED_UNUSED = {
    .motion_tell = gui_window_motion_tell,
};

typedef struct
{
    void (*show)(const GuiWindowShownCfg *args);
} CliWindowShownNs;
EMBED_TABLE_LAYOUT(CliWindowShownNs, show);
EMBED_TABLE_STORAGE CliWindowShownNs cli_window_shown EMBED_UNUSED = {
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
