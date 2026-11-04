// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.h: the view's thread, which is the command line's. The window is a pump and a blit: its thread hands every event
// the system gives it to the view's ring, does each order the view writes back, and shows the rasters the orders hand
// it; it draws nothing, computes nothing and asks for no memory. All that is drawn is drawn on this thread, in memory
// the command line owns. Each ring has one writer and one reader, and the map of what a pixel stands on is handed over
// in three buffers, one of them the writer's, one the reader's and one on its way, so neither side waits for the other
// (view.c)
#ifndef CLI_VIEW_H
#define CLI_VIEW_H
#include "../../../gui/src/input_event.h"
#include "../command_line/callback_cfg.h"
#include "embed_atomic.h"
#include "memoria_anularis/memoria_anularis.h"
#include "raster.h"

// what a pixel of the main window stands on, as the window lays it; a key sent while the name's pane stands names the
// wordmark, no view taking it. In the terminal: its pane, a tab, a tab's ×, New Terminal, Split, and the panel on none
// of these. On the status bar: the notice it shows, the Silence beside it, and Focus
#define CLI_HIT_NONE 0u
#define CLI_HIT_MENU_TITLE 1u
#define CLI_HIT_MENU_ROW 2u
#define CLI_HIT_TAB 3u
#define CLI_HIT_TAB_CLOSE 4u
#define CLI_HIT_TOOL 5u
#define CLI_HIT_EXPLORER_ROW 6u
#define CLI_HIT_PROBLEM_ROW 7u
#define CLI_HIT_PROBLEMS_CHECK 8u
#define CLI_HIT_SUGGEST_ROW 9u
#define CLI_HIT_MAP 10u
#define CLI_HIT_EDITOR 11u
#define CLI_HIT_GUTTER 12u
#define CLI_HIT_WORDMARK 13u
#define CLI_HIT_HOVER_BOX 14u
#define CLI_HIT_SIGNATURE_BOX 15u
#define CLI_HIT_SUGGEST_LIST 16u
#define CLI_HIT_TERMINAL 17u
#define CLI_HIT_TERMINAL_TAB 18u
#define CLI_HIT_TERMINAL_TAB_CLOSE 19u
#define CLI_HIT_TERMINAL_NEW 20u
#define CLI_HIT_TERMINAL_SPLIT 21u
#define CLI_HIT_TERMINAL_PANEL 22u
#define CLI_HIT_STATUS_NOTICE 23u
#define CLI_HIT_STATUS_SILENCE 24u
#define CLI_HIT_STATUS_FOCUS 25u

// A place on the screen in the system's own pixels, and whether the window filled its monitor there
typedef struct
{
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int maximized;
} CliWindowPosition;

// A monitor's area, in the system's own pixels
typedef struct
{
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} CliWindowArea;

// the least size the window is dragged to, and a place saved is read back at, in pixels at a scale of one
#define CLI_POSITION_MIN_WIDTH 960u
#define CLI_POSITION_MIN_HEIGHT 600u

// the window's two surfaces
#define CLI_VIEW_MAIN 0u
#define CLI_VIEW_PREFERENCES 1u
#define CLI_VIEW_SURFACES 2u

// where the pointer stands: the client, the caption the system drags the window by, the borders and corners the system
// resizes it by, and the three controls
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

// what the window does to itself through the system: minimized, maximized or restored, and closed
#define GUI_ACT_MINIMIZE 0u
#define GUI_ACT_MAXIMIZE 1u
#define GUI_ACT_CLOSE 2u

// the icon's sides in pixels, the small one the title's and the large one the taskbar's and the switcher's
#define GUI_ICON_SMALL 32u
#define GUI_ICON_LARGE 128u

// what the system's thread hands the view: a surface's client sized, with the scale the system draws it at and the
// milliseconds of one refresh of its monitor; an input; Preferences' window asked closed by the system; the window gone
// off the screen or back on it, `flag` 1 on; the system's wish for less motion, `flag` 1 for less; a raster the view
// ordered shown; and Preferences' window refused by the system
#define CLI_VIEW_EVENT_SIZED 0u
#define CLI_VIEW_EVENT_INPUT 1u
#define CLI_VIEW_EVENT_CLOSING 2u
#define CLI_VIEW_EVENT_DISPLAYED 3u
#define CLI_VIEW_EVENT_MOTION 4u
#define CLI_VIEW_EVENT_PRESENTED 5u
#define CLI_VIEW_EVENT_REFUSED 6u

typedef struct
{
    unsigned int kind;
    unsigned int surface;
    unsigned int width;
    unsigned int height;
    int scale;
    unsigned int frame_ms;
    int flag;
    GuiInput input;
} CliViewEvent;

// what the view orders of the system's thread: a raster shown on a surface, whole or the rows of `part`; the window
// minimized, maximized or closed; Preferences' window opened at `width` by `height` over the main one, or closed; and
// the two icons given to the system, which copies them
#define CLI_VIEW_ORDER_PRESENT 0u
#define CLI_VIEW_ORDER_PRESENT_PART 1u
#define CLI_VIEW_ORDER_ACT 2u
#define CLI_VIEW_ORDER_PREFERENCES_OPEN 3u
#define CLI_VIEW_ORDER_PREFERENCES_CLOSE 4u
#define CLI_VIEW_ORDER_ICON 5u

typedef struct
{
    unsigned int kind;
    unsigned int surface;
    unsigned int act;
    unsigned int width;
    unsigned int height;
    GuiRect part;
    GuiRaster raster;
    GuiRaster raster_large;
} CliViewOrder;

// What a pixel of a surface stands on, as the view last laid it: the client's size and the resize border's width; the
// three controls, empty on a surface with none; the rectangles the caption gives way to; and the caption. A surface of
// no width is the client everywhere
typedef struct
{
    unsigned int width;
    unsigned int height;
    int border;
    GuiRect minimize;
    GuiRect maximize;
    GuiRect close;
    GuiRect *client;
    unsigned int clients;
    GuiRect caption;
} CliViewHits;

// What wakes the system's thread, from any thread
typedef void (*CliViewWoken)(void *context);

// The view begun: the system's largest monitor's size for its framebuffers, and what wakes the system's thread
typedef struct
{
    CliViewWoken woken;
    void *context;
    unsigned int display_width;
    unsigned int display_height;
    VerbaLinea *err_msg;
} CliViewBeginCfg;

// the view's window and the tree begun, the window started on the tree and the view's thread started: 1, or 0 with the
// reason written and nothing left standing; and ended, the thread let end, the tree's servers and processes ended and
// every cell of the view released, after the system's window has closed
int cli_view_begin(const CliViewBeginCfg *args);
void cli_view_end(void);
// from the system's thread alone: one event written, its input's name or text with it, dropped where the ring cannot
// hold it; the next order taken: 1, or 0 where none waits; and where a pixel of `surface` stands as the view last laid
// it, one of the places above
void cli_view_event(const CliViewEvent *event);
int cli_view_order(CliViewOrder *order);
unsigned int cli_view_hit(unsigned int surface, int x, int y);
// where a pixel stands on `hits`, on any thread
unsigned int cli_view_hit_of(const CliViewHits *hits, int x, int y);
// the window's place for each tree, saved in the file `places` in orior's home as the window closes, one record for
// each tree (place.c): the place saved for the tree `root`, fitted to the monitors `areas`, `count` of them, the
// system's main one first, into `place`: 1, or 0 where none is saved, it is below the least size or there is no
// monitor; and `place` saved for `root`, beside the places of the other trees, nothing saved where `root` is NULL
int cli_view_place_saved(const char *root, const CliWindowArea *areas, unsigned int count, CliWindowPosition *place);
void cli_view_place_store(const char *root, const CliWindowPosition *place);

#endif
