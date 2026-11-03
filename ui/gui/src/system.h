// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system.h: the face of the system the window stands on, the only part of the program written twice, once a system: the
// monitors' areas; a window opened undecorated at a place, its events handed on as they come, its pixels presented from
// a raster it is handed, its thread woken from another, its place read, its state changed and its closing; and the
// scale the system draws it at. Nothing here takes memory: each window's record is the body's own (system_windows.c,
// system_x11.c)
#ifndef GUI_SYSTEM_H
#define GUI_SYSTEM_H
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/controller/controller.h"
#include "../../cli/src/view/view.h"
#include "embed_dispatch_layout.h"
#include "input_event.h"

typedef struct GuiSystemPositionTitleCfg GuiSystemPositionTitleCfg;
typedef struct GuiSystemTitleIconCfg GuiSystemTitleIconCfg;
typedef struct GuiSystemRasterCfg GuiSystemRasterCfg;
typedef struct GuiSystemCfg GuiSystemCfg;
typedef struct GuiSystemAreasCfg GuiSystemAreasCfg;
typedef struct GuiSystemWidthCfg GuiSystemWidthCfg;
typedef struct GuiSystemPositionCfg GuiSystemPositionCfg;

// A system window, as its body defines it
typedef struct GuiSystem GuiSystem;

typedef struct
{
    void *context;
    const GuiInput *input;
} GuiEachInputCfg;

// Where the system hands its events, each with the context given at the opening: the client sized, with the scale the
// system draws at and the milliseconds of one refresh of its monitor; an input; the thread woken by another; the system
// asking to close; where the pointer on the input stands, as view.h names the places, for the system's own drag and
// resize; the window gone off the screen, minimized or unmapped, `displayed` 0, or back on it, 1; and the system's wish
// for less motion changed
typedef struct
{
    void (*sized)(const GuiEachWidthHeightScaleCfg *args);
    void (*input)(const GuiEachInputCfg *args);
    void (*woken)(void *context);
    void (*closing)(void *context);
    unsigned int (*hit)(const GuiEachInputCfg *args);
    void (*displayed)(const GuiEachDisplayedCfg *args);
    void (*motion)(void *context);
} GuiSystemEvents;

// the monitors' areas into `areas`, sized for `most`, the system's main one first: their count
unsigned int gui_system_areas(const GuiSystemAreasCfg *args);
// the whole size of the monitor with the most pixels, the part a taskbar covers with the rest, where a window filling
// it stands: 0 by 0 where there is no display
void gui_system_display_max(const GuiSystemWidthCfg *args);
// a window opened undecorated at `place`, titled `title`, its events handed as `events` says with `context`: the
// window, or NULL with why given; the loop run until the window closes; the window closed, which the loop may do from
// inside an event it hands on, the system's record saved for the loop to read; and the record given back once the
// loop has returned
GuiSystem *gui_system_open(const GuiSystemPositionTitleCfg *args);
void gui_system_run(const GuiSystemCfg *args);
void gui_system_close(const GuiSystemCfg *args);
void gui_system_free(const GuiSystemCfg *args);
// a second window opened undecorated at `place`, which goes anywhere on any monitor and stands in front of `owner`: its
// events handed as `events` says with `context`, on the owner's loop and the owner's thread, its pixels written through
// the system's drawing calls; the window, or NULL with why given. It is closed and given as any other, before its owner
// is, and closing it leaves the owner and its loop running
GuiSystem *gui_system_owner_open(const GuiSystemPositionTitleCfg *args);
// every event waiting taken and handed on, the loop not run, for a check that posts events to the window; and the
// system's own handle of the window, for a check to post them to
void gui_system_pumped(const GuiSystemCfg *args);
void *gui_system_native(const GuiSystemPositionCfg *args);
// `raster` shown in the client, its description copied, and shown again as the system asks for the client to be
// painted: where its pixels
// lie in pages the system gave and the system can read them where they lie, copied from there into its own surface by
// its graphics device and flipped onto the screen, else written through the system's drawing calls
void gui_system_present(const GuiSystemRasterCfg *args);
// the rows of `raster` that `part` spans shown again in the client, the rest standing as last shown
void gui_system_present_part(const GuiSystemRasterCfg *args);
// the window's thread woken, from any thread
void gui_system_wake(const GuiSystemCfg *args);
// the window minimized, maximized or restored, or asked to close
void gui_system_act(const GuiSystemCfg *args);
// the window's icon given to the system as `title_icon` and `taskbar_icon`, each a raster of the icon's pixels with
// their own alpha, straight, the system copying them: the title's and the taskbar's icon on Windows, _NET_WM_ICON on
// X11
void gui_system_icon(const GuiSystemTitleIconCfg *args);
// where the window stands, in the system's own pixels, and whether it fills its monitor; and the scale the system
// draws it at, 1 at 96 dots an inch
void gui_system_on_position(const GuiSystemPositionCfg *args);
int gui_system_scale(const GuiSystemPositionCfg *args);
// whether the system asks for less motion: 1 where it does; a change of it comes as the `motion` event
int gui_system_motion_reduce(const GuiSystemPositionCfg *args);
struct GuiSystemPositionTitleCfg
{
    const CliWindowPosition *place;
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
    unsigned int *count_out;
};

struct GuiSystemWidthCfg
{
    unsigned int *width;
    unsigned int *height;
};

struct GuiSystemPositionCfg
{
    const GuiSystem *system_read;
    CliWindowPosition *position_out;
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
    void (*display_max)(const GuiSystemWidthCfg *args);
} GuiSystemWidthNs;
EMBED_TABLE_LAYOUT(GuiSystemWidthNs, display_max);
EMBED_TABLE_STORAGE GuiSystemWidthNs gui_system_width EMBED_UNUSED = {
    .display_max = gui_system_display_max,
};

typedef struct
{
    GuiSystem *(*open)(const GuiSystemPositionTitleCfg *args);
    GuiSystem *(*owner_open)(const GuiSystemPositionTitleCfg *args);
} GuiSystemPositionTitleNs;
EMBED_TABLE_LAYOUT(GuiSystemPositionTitleNs, open, owner_open);
EMBED_TABLE_STORAGE GuiSystemPositionTitleNs gui_system_position_title EMBED_UNUSED = {
    .open = gui_system_open,
    .owner_open = gui_system_owner_open,
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
    void *(*native)(const GuiSystemPositionCfg *args);
    void (*place)(const GuiSystemPositionCfg *args);
    int (*scale)(const GuiSystemPositionCfg *args);
    int (*motion_reduce)(const GuiSystemPositionCfg *args);
} GuiSystemPositionNs;
EMBED_TABLE_LAYOUT(GuiSystemPositionNs, native, place, scale, motion_reduce);
EMBED_TABLE_STORAGE GuiSystemPositionNs gui_system_position EMBED_UNUSED = {
    .native = gui_system_native,
    .place = gui_system_on_position,
    .scale = gui_system_scale,
    .motion_reduce = gui_system_motion_reduce,
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
    void (*icon)(const GuiSystemTitleIconCfg *args);
} GuiSystemTitleIconNs;
EMBED_TABLE_LAYOUT(GuiSystemTitleIconNs, icon);
EMBED_TABLE_STORAGE GuiSystemTitleIconNs gui_system_title_icon EMBED_UNUSED = {
    .icon = gui_system_icon,
};

#endif
