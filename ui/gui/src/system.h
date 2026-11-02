// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system.h: the face of the system the window stands on, the only part of the window written twice, once a system: the
// monitors' areas; a window opened undecorated at a place, its events handed to the window as they come, its pixels
// presented from a raster, its thread woken from another, its place read, its state changed and its closing; and the
// scale the system draws it at (system_windows.c, system_x11.c)
#ifndef GUI_SYSTEM_H
#define GUI_SYSTEM_H
typedef struct GuiSystemPlaceTitleCfg GuiSystemPlaceTitleCfg;
typedef struct GuiSystemTitleIconCfg GuiSystemTitleIconCfg;
typedef struct GuiSystemRasterCfg GuiSystemRasterCfg;
typedef struct GuiSystemCfg GuiSystemCfg;
typedef struct GuiSystemAreasCfg GuiSystemAreasCfg;
typedef struct GuiSystemWidthCfg GuiSystemWidthCfg;
typedef struct GuiSystemBytesCfg GuiSystemBytesCfg;
typedef struct GuiSystemPlaceCfg GuiSystemPlaceCfg;
typedef struct GuiSystemAfterCfg GuiSystemAfterCfg;
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/window/window.h"
#include "embed_dispatch_layout.h"
#include "input_event.h"
#include "raster.h"

// what the window does to itself through the system: minimized, maximized or restored, and closed
#define GUI_ACT_MINIMIZE 0u
#define GUI_ACT_MAXIMIZE 1u
#define GUI_ACT_CLOSE 2u

// A system window, as its body defines it
typedef struct GuiSystem GuiSystem;

typedef struct
{
    void *context;
    const GuiInput *input;
} GuiEachInputCfg;

// Where the system hands the window its events, each with the context given at the opening: the client sized, with
// the scale the system draws at; an input; the thread woken by another; the system asking to close; where the
// pointer on the input stands, as window.h names the places, for the system's own drag and resize; the queue empty,
// where a frame owed is drawn and presented; the tick the window asked for come; and the window gone off the screen,
// minimized or unmapped, `shown` 0, or back on it, 1
typedef struct
{
    void (*sized)(const GuiEachWidthHeightScaleCfg *args);
    void (*input)(const GuiEachInputCfg *args);
    void (*woken)(void *context);
    void (*closing)(void *context);
    unsigned int (*hit)(const GuiEachInputCfg *args);
    void (*idle)(void *context);
    void (*ticked)(void *context);
    void (*shown)(const GuiEachShownCfg *args);
    void (*motion)(void *context);
} GuiSystemEvents;

// the monitors' areas into `areas`, sized for `most`, the system's main one first: their count
unsigned int gui_system_areas(const GuiSystemAreasCfg *args);
// the whole size of the monitor with the most pixels, the part a taskbar covers with the rest, where a window filling
// it stands: 0 by 0 where there is no display
void gui_system_display_most(const GuiSystemWidthCfg *args);
// a window opened undecorated at `place`, titled `title`, its events handed as `events` says with `context`: the
// window, or NULL with why given; the loop run until the window closes; the window closed, which the loop may do from
// inside an event it hands on, the system's record saved for the loop to read; and the record given back once the
// loop has returned
GuiSystem *gui_system_open(const GuiSystemPlaceTitleCfg *args);
void gui_system_run(const GuiSystemCfg *args);
void gui_system_close(const GuiSystemCfg *args);
void gui_system_free(const GuiSystemCfg *args);
// a second window opened undecorated at `place`, which goes anywhere on any monitor and stands in front of `owner`: its
// events handed as `events` says with `context`, on the owner's loop and the owner's thread, its pixels written through
// the system's drawing calls; the window, or NULL with why given. It is closed and given as any other, before its owner
// is, and closing it leaves the owner and its loop running
GuiSystem *gui_system_owned_open(const GuiSystemPlaceTitleCfg *args);
// every event waiting taken and handed on, the loop not run, for a check that posts events to the window; and the
// system's own handle of the window, for a check to post them to
void gui_system_pumped(const GuiSystemCfg *args);
void *gui_system_native(const GuiSystemPlaceCfg *args);
// `bytes` of pages from the system's own page allocator, zero, one allocation the system's surfaces can read from
// where it lets them, NULL where it gives none; and pages so taken given back, every surface the system made over
// them let go first
void *gui_system_pages(const GuiSystemBytesCfg *args);
void gui_system_pages_free(const GuiSystemBytesCfg *args);
// `raster` shown in the client, and shown again as the system asks for the client to be painted: where its pixels
// lie in pages the system gave and the system can read them where they lie, copied from there into its own surface by
// its graphics device and flipped onto the screen, else written through the system's drawing calls
void gui_system_present(const GuiSystemRasterCfg *args);
// the rows of `raster` that `part` spans shown again in the client, the rest standing as last shown
void gui_system_present_part(const GuiSystemRasterCfg *args);
// the window's thread woken, from any thread
void gui_system_wake(const GuiSystemCfg *args);
// a tick asked for `after` milliseconds from now, on the window's thread, one at a time, the last asked the one that
// comes, and no sooner than the display's next refresh after the last frame presented; 0 asks for none
void gui_system_tick(const GuiSystemAfterCfg *args);
// the window minimized, maximized or restored, or asked to close
void gui_system_act(const GuiSystemCfg *args);
// the window's icon given to the system as `title_icon` and `taskbar_icon`, each a raster of the icon's pixels with
// their own alpha, straight, the system copying them: the title's and the taskbar's icon on Windows, _NET_WM_ICON on
// X11
void gui_system_icon(const GuiSystemTitleIconCfg *args);
// where the window stands, in the system's own pixels, and whether it fills its monitor; and the scale the system
// draws it at, 1 at 96 dots an inch
void gui_system_on_place(const GuiSystemPlaceCfg *args);
int gui_system_scale(const GuiSystemPlaceCfg *args);
// whether the system asks for less motion: 1 where it does; a change of it comes as the `motion` event
int gui_system_motion_reduce(const GuiSystemPlaceCfg *args);
struct GuiSystemPlaceTitleCfg
{
    const CliWindowPlace *place;
    const char *title;
    const GuiSystemEvents *events;
    void *context;
    VerbaLinea *err_msg;
    GuiSystem *owner;
};

struct GuiSystemTitleIconCfg
{
    GuiSystem *system;
    const GuiRaster *title_icon;
    const GuiRaster *taskbar_icon;
};

struct GuiSystemRasterCfg
{
    GuiSystem *system;
    const GuiRaster *raster;
    GuiRect part;
};

struct GuiSystemCfg
{
    GuiSystem *system;
    unsigned int act;
};

struct GuiSystemAreasCfg
{
    CliWindowArea *areas;
    unsigned int most;
};

struct GuiSystemWidthCfg
{
    unsigned int *width;
    unsigned int *height;
};

struct GuiSystemBytesCfg
{
    unsigned long long bytes;
    void *pages;
};

struct GuiSystemPlaceCfg
{
    const GuiSystem *system_read;
    CliWindowPlace *place_out;
};

struct GuiSystemAfterCfg
{
    GuiSystem *system;
    unsigned long long after;
};

typedef struct
{
    unsigned int (*areas)(const GuiSystemAreasCfg *args);
} CliSystemAreasNs;
EMBED_TABLE_LAYOUT(CliSystemAreasNs, areas);
EMBED_TABLE_STORAGE CliSystemAreasNs cli_system_areas EMBED_UNUSED = {
    .areas = gui_system_areas,
};

typedef struct
{
    void (*display_most)(const GuiSystemWidthCfg *args);
} GuiSystemWidthNs;
EMBED_TABLE_LAYOUT(GuiSystemWidthNs, display_most);
EMBED_TABLE_STORAGE GuiSystemWidthNs gui_system_width EMBED_UNUSED = {
    .display_most = gui_system_display_most,
};

typedef struct
{
    GuiSystem *(*open)(const GuiSystemPlaceTitleCfg *args);
    GuiSystem *(*owned_open)(const GuiSystemPlaceTitleCfg *args);
} GuiSystemPlaceTitleNs;
EMBED_TABLE_LAYOUT(GuiSystemPlaceTitleNs, open, owned_open);
EMBED_TABLE_STORAGE GuiSystemPlaceTitleNs gui_system_place_title EMBED_UNUSED = {
    .open = gui_system_open,
    .owned_open = gui_system_owned_open,
};

typedef struct
{
    void (*run)(const GuiSystemCfg *args);
    void (*close)(const GuiSystemCfg *args);
    void (*free)(const GuiSystemCfg *args);
    void (*pumped)(const GuiSystemCfg *args);
    void (*wake)(const GuiSystemCfg *args);
    void (*act)(const GuiSystemCfg *args);
} GuiSystemNs;
EMBED_TABLE_LAYOUT(GuiSystemNs, run, close, free, pumped, wake, act);
EMBED_TABLE_STORAGE GuiSystemNs gui_system_table EMBED_UNUSED = {
    .run = gui_system_run,
    .close = gui_system_close,
    .free = gui_system_free,
    .pumped = gui_system_pumped,
    .wake = gui_system_wake,
    .act = gui_system_act,
};

typedef struct
{
    void *(*native)(const GuiSystemPlaceCfg *args);
    void (*place)(const GuiSystemPlaceCfg *args);
    int (*scale)(const GuiSystemPlaceCfg *args);
    int (*motion_reduce)(const GuiSystemPlaceCfg *args);
} GuiSystemPlaceNs;
EMBED_TABLE_LAYOUT(GuiSystemPlaceNs, native, place, scale, motion_reduce);
EMBED_TABLE_STORAGE GuiSystemPlaceNs gui_system_place EMBED_UNUSED = {
    .native = gui_system_native,
    .place = gui_system_on_place,
    .scale = gui_system_scale,
    .motion_reduce = gui_system_motion_reduce,
};

typedef struct
{
    void *(*pages)(const GuiSystemBytesCfg *args);
    void (*pages_free)(const GuiSystemBytesCfg *args);
} GuiSystemBytesNs;
EMBED_TABLE_LAYOUT(GuiSystemBytesNs, pages, pages_free);
EMBED_TABLE_STORAGE GuiSystemBytesNs gui_system_bytes EMBED_UNUSED = {
    .pages = gui_system_pages,
    .pages_free = gui_system_pages_free,
};

typedef struct
{
    void (*present)(const GuiSystemRasterCfg *args);
    void (*present_part)(const GuiSystemRasterCfg *args);
} GuiSystemRasterNs;
EMBED_TABLE_LAYOUT(GuiSystemRasterNs, present, present_part);
EMBED_TABLE_STORAGE GuiSystemRasterNs gui_system_raster EMBED_UNUSED = {
    .present = gui_system_present,
    .present_part = gui_system_present_part,
};

typedef struct
{
    void (*tick)(const GuiSystemAfterCfg *args);
} GuiSystemAfterNs;
EMBED_TABLE_LAYOUT(GuiSystemAfterNs, tick);
EMBED_TABLE_STORAGE GuiSystemAfterNs gui_system_after EMBED_UNUSED = {
    .tick = gui_system_tick,
};

typedef struct
{
    void (*icon)(const GuiSystemTitleIconCfg *args);
} GuiSystemTitleIconNs;
EMBED_TABLE_LAYOUT(GuiSystemTitleIconNs, icon);
EMBED_TABLE_STORAGE GuiSystemTitleIconNs gui_system_title_icon EMBED_UNUSED = {
    .icon = gui_system_icon,
};

#endif
