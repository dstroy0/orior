// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system.h: the face of the system the window stands on, the only part of the window written twice, once a system: the
// monitors' areas; a window opened undecorated at a place, its events handed to the window as they come, its pixels
// presented from a raster, its thread woken from another, its place read, its state changed and its closing; and the
// scale the system draws it at (system_windows.c, system_x11.c)
#ifndef GUI_SYSTEM_H
#define GUI_SYSTEM_H
#include "../../cli/src/window/window.h"
#include "raster.h"

// what the window does to itself through the system: minimized, maximized or restored, and closed
#define GUI_ACT_MINIMIZE 0u
#define GUI_ACT_MAXIMIZE 1u
#define GUI_ACT_CLOSE 2u
// the pointer's events: moved, a button pressed, a button let go; and the modifier keys down, as bits
#define GUI_POINTER_MOVED 0u
#define GUI_POINTER_DOWN 1u
#define GUI_POINTER_UP 2u
#define GUI_MOD_SHIFT 1u
#define GUI_MOD_CONTROL 2u
#define GUI_MOD_ALT 4u

// A system window, as its body defines it
typedef struct GuiSystem GuiSystem;

// Where the system hands the window its events, each with the context given at the opening: the client sized, with
// the scale the system draws at; the pointer at x, y of the client, the button 0 for the left, with the modifier keys
// down; the wheel turned `rows` rows, down where positive; a key pressed, named as the page's editor names a key,
// `Mod+Shift+Left`, Mod the control key, Alt and Shift before the key's own name; `size` bytes of text typed as
// UTF-8; the thread woken by another; the system asking to close; where the pointer at x, y stands, as window.h names
// the places, for the system's own drag and resize; the queue empty, where a frame owed is drawn and presented; the
// tick the window asked for come; and the window gone off the screen, minimized or unmapped, `shown` 0, or back on
// it, 1
typedef struct
{
    void (*sized)(void *context, unsigned int width, unsigned int height, float scale);
    void (*pointer)(void *context, unsigned int kind, int x, int y, unsigned int button, unsigned int mods);
    void (*wheel)(void *context, int x, int y, int rows);
    void (*key)(void *context, const char *name);
    void (*text)(void *context, const char *text, unsigned long long size);
    void (*woken)(void *context);
    void (*closing)(void *context);
    unsigned int (*hit)(void *context, int x, int y);
    void (*idle)(void *context);
    void (*ticked)(void *context);
    void (*shown)(void *context, int shown);
} GuiSystemEvents;

// the monitors' areas into `areas`, sized for `most`, the system's main one first: their count
unsigned int gui_system_areas(CliWindowArea *areas, unsigned int most);
// a window opened undecorated at `place`, titled `title`, its events handed as `events` says with `context`: the
// window, or NULL with why given; the loop run until the window closes; and the window closed
GuiSystem *gui_system_opened(const CliWindowPlace *place, const char *title, const GuiSystemEvents *events,
                             void *context, VerbaLinea *err_msg);
void gui_system_run(GuiSystem *system);
void gui_system_closed(GuiSystem *system);
// every event waiting taken and handed on, the loop not run, for a check that posts events to the window; and the
// system's own handle of the window, for a check to post them to
void gui_system_pumped(GuiSystem *system);
void *gui_system_native(const GuiSystem *system);
// `bytes` of pages from the system's own page allocator, zero, one allocation the system's surfaces can read from
// where it lets them, NULL where it gives none; and pages so taken given back, every surface the system made over
// them let go first
void *gui_system_pages(unsigned long long bytes);
void gui_system_pages_given(void *pages, unsigned long long bytes);
// `raster` shown in the client, and shown again as the system asks for the client to be painted: where its pixels
// lie in pages the system gave and the system can read them where they lie, copied from there into its own surface by
// its graphics device and flipped onto the screen, else written through the system's drawing calls
void gui_system_present(GuiSystem *system, const GuiRaster *raster);
// the rows of `raster` that `part` spans shown again in the client, the rest standing as last shown
void gui_system_present_part(GuiSystem *system, const GuiRaster *raster, GuiRect part);
// the window's thread woken, from any thread
void gui_system_wake(GuiSystem *system);
// a tick asked for `after` milliseconds from now, on the window's thread, one at a time, the last asked the one that
// comes, and no sooner than the display's next refresh after the last frame presented; 0 asks for none
void gui_system_tick(GuiSystem *system, unsigned long long after);
// the window minimized, maximized or restored, or asked to close
void gui_system_act(GuiSystem *system, unsigned int act);
// the window's icon given to the system as `title_icon` and `taskbar_icon`, each a raster of the icon's pixels with
// their own alpha, straight, the system copying them: the title's and the taskbar's icon on Windows, _NET_WM_ICON on
// X11
void gui_system_icon(GuiSystem *system, const GuiRaster *title_icon, const GuiRaster *taskbar_icon);
// where the window stands, in the system's own pixels, and whether it fills its monitor; and the scale the system
// draws it at, 1 at 96 dots an inch
void gui_system_placed(const GuiSystem *system, CliWindowPlace *place);
float gui_system_scale(const GuiSystem *system);
#endif
