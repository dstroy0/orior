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
#include "claim.h"
#include "ground.h"
#include "link.h"
#include "parts/editor.h"
#include "parts/explorer.h"
#include "parts/hover.h"
#include "parts/menubar.h"
#include "parts/minimap.h"
#include "parts/problems.h"
#include "parts/signature.h"
#include "parts/statusbar.h"
#include "parts/strip.h"
#include "parts/suggest.h"
#include "parts/tabs.h"

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

// The window: its frame, raster, the ground every frame starts from, glyphs, icons, palette, editor, minimap, menu
// bar, strip, explorer and link; the
// panels over the editor, the hover, the completion and the signature, with the tickets of their asks in flight, 0
// where none is, whether the completion is to be asked for as the next frame comes, whether that ask was forced by
// the keys, whether each panel's ask is to be made again with the server once its answer without the server comes,
// whether the server is to be told the text as the next frame comes, an input having changed it, and the cursor's
// place the signature was last asked at; the problems pane, the ticket of the tree's check asked for, and the place
// the shown file's cursor is to be put at as its next frame comes, where one is wanted; when the caret's blink last
// began, each frame of the view beginning it again, and whether the caret was hidden in the last frame drawn; when the view's rows last
// moved, 0 once the
// server was told the view they rest at, whether it was told, and the top and rows it was told; the claim and
// whether its slides ran at the last tick, the view told the editor's size as they stop; the
// catalog of commands the menu bar reads, parsed into a JSON tree of its own once it came; the tree's name shown in the bar;
// the tree's root as the CLI names it, empty where none is open; the files open as tabs, how many, the one shown, -1
// for none, and the one shown before it, -1 for none; the shown file's path and the key it is stored under; whether
// the window is maximized; the tickets of the calls in flight, the launch command, the root, the catalog, a whole path
// made the tree's, what orior's folder keeps, the view's open, the memory reading and the map's lines, each 0 where
// none is, whether the map is wanted again as its answer comes, when the memory was last asked and the bytes it
// answered, and the sequence of the last frame taken; whether the tree's kept tabs are back, the session kept again
// only after, and whether a session for the tree is known, read from the folder or made by the tabs' own changes,
// without which no tabs are kept over whatever the folder holds; the session kept again
// only after; whether the CLI's editor view is open on the shown file, whether the pointer drags in it, and the
// presses counted, when and where the last was; whether a frame is owed; and the control pressed, for the system to
// act on, GUI_HIT_CLIENT where none
typedef struct
{
    GuiFrame frame;
    GuiRaster raster;
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
    unsigned long long caret_since_ms;
    int caret_hidden;
    unsigned long long view_moved_ms;
    int view_told;
    unsigned long long view_told_top;
    unsigned int view_told_rows;
    GuiClaim claim;
    int claim_moving;
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
    unsigned long long memory_ms;
    unsigned long long memory_bytes;
    unsigned long long frame_seq;
    int restored;
    int session_known;
    int viewed;
    int dragging;
    unsigned int clicks;
    unsigned long long clicked_ms;
    int clicked_x;
    int clicked_y;
    int owed;
    unsigned int pressed;
} GuiWindow;

// the milliseconds within which presses count up, the pixels a press may move and still count, the milliseconds
// between readings of the memory, the milliseconds the view's rows rest before the server is told the view, its
// slice cut for it, and the caret's blink, shown for the first half of it and hidden for the second, as the page's
// ed-blink runs
#define GUI_CLICK_WITHIN 500ull
#define GUI_CLICK_NEAR 4
#define GUI_MEMORY_EVERY 2000ull
#define GUI_VIEW_RESTS 250ull
#define GUI_CARET_BLINK 1060ull

// the window begun: its glyphs read, its palette the dark scheme, its editor empty and its link's workers started, the
// window's thread woken through `woken` with `context`: 1, or 0 with why given; and ended, every cell given back
int gui_window_begun(GuiWindow *window, GuiLinkWoken woken, void *context, VerbaLinea *err_msg);
void gui_window_ended(GuiWindow *window);
// the window sized `width` by `height` at `scale`: its frame laid out and its raster made anew: 1, or 0 where the
// raster's cell could not be taken
int gui_window_sized(GuiWindow *window, unsigned int width, unsigned int height, float scale);
// the first calls made as the window opens: the launch command taken and the tree's root asked
void gui_window_started(GuiWindow *window);
// the frame drawn into the raster, the frame owed no more
void gui_window_drawn(GuiWindow *window);
// where the pointer at x, y stands
unsigned int gui_window_hit(const GuiWindow *window, int x, int y);
// the wheel turned `rows` rows, down where positive, with the pointer at x, y, over the editor or the map; a button
// pressed at x, y with the modifier keys `mods` down, as system.h names them; the pointer moved to x, y; the button let go;
// a key pressed, named as the page's editor names it; and `size` bytes of text typed
void gui_window_wheel(GuiWindow *window, int x, int y, int rows);
void gui_window_pressed(GuiWindow *window, int x, int y, unsigned int mods);
void gui_window_moved(GuiWindow *window, int x, int y);
void gui_window_released(GuiWindow *window, int x, int y);
void gui_window_key(GuiWindow *window, const char *name);
void gui_window_text(GuiWindow *window, const char *text, unsigned long long size);
// the file at `path` opened through the CLI as a tab, shown in the editor as its answers come, or shown where it is
// open already; the tab `at` shown; and the tab `at` closed, its view and its branch let go, the tab beside it shown
void gui_window_file_opened(GuiWindow *window, const char *path);
void gui_window_tab_shown(GuiWindow *window, unsigned int at);
void gui_window_tab_closed(GuiWindow *window, unsigned int at);
// `length` bytes of a text shown whole in the editor
int gui_window_text_put(GuiWindow *window, const char *text, unsigned long long length);
// every answer and event waiting taken, on the window's thread
void gui_window_drained(GuiWindow *window);
// the tick the window asked for come: the claim's timers run and its parts laid where its slides stand, and the
// ground drawn again where its loop shows a change; and when the window next wants a tick, in milliseconds, 0 for
// never
void gui_window_ticked(GuiWindow *window);
unsigned long long gui_window_due(const GuiWindow *window);
// the window gone off the screen, `shown` 0, its ground's clock standing still, or back on it, 1, a frame owed
void gui_window_shown(GuiWindow *window, int shown);
// the window's icon, the wordmark's eye in the text's color, made into `icon` at `size` pixels a side with its own
// alpha, for the system to take: 1, or 0 where the eye could not be drawn or the raster made; given back as a raster
int gui_window_icon(GuiWindow *window, GuiRaster *icon, unsigned int size);
#endif
