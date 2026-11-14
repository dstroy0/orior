// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// docks.h: a tool window coming in from the edge it docks at as it shows, as the page's do: the terminal's panel
// starts GUI_TERMINAL_ARRIVES page's pixels below its place at the work's foot and comes up to it over
// GUI_DOCKS_ARRIVING_MS on the page's ease-out, its opacity rising from none to whole with it. A panel located as the
// window starts is at once, and so does one displayed while less motion is asked for. The frame positions the panel where
// it is for the share of its way left, and the stages position every moment of it with the claim's slides (docks.c)
#ifndef GUI_DOCKS_H
#define GUI_DOCKS_H
#include "claim.h"
#include "embed_dispatch_layout.h"
#include "frame.h"

typedef struct GuiDocksCfg GuiDocksCfg;

// how long a panel's arrival takes, in milliseconds
#define GUI_DOCKS_ARRIVING_MS 140ULL

// The docks: the panel's arrival, a slide of the way it has left, from all of it to none
typedef struct
{
    GuiSlide arrive;
} GuiDocks;

// no arrival
void gui_docks_init(const GuiDocksCfg *args);
// the panel displayed at `time_ms`: its arrival begun, or none where motion is `still`
void gui_docks_show(const GuiDocksCfg *args);
// the share of its way the panel has left at `time_ms`, GUI_FIXED_ONE as it begins and 0 arrived; and when the arrival
// ends, 0 where none runs at `time_ms`
int gui_docks_away(const GuiDocksCfg *args);
unsigned long long gui_docks_arrive_end_ms(const GuiDocksCfg *args);
struct GuiDocksCfg
{
    GuiDocks *docks;
    const GuiDocks *docks_read;
    int still;
    unsigned long long time_ms;
};

typedef struct
{
    void (*init)(const GuiDocksCfg *args);
    void (*show)(const GuiDocksCfg *args);
    int (*away)(const GuiDocksCfg *args);
    unsigned long long (*arrive_end_ms)(const GuiDocksCfg *args);
} GuiDocksNs;
EMBED_TABLE_LAYOUT(GuiDocksNs, init, show, away, arrive_end_ms);
EMBED_TABLE_STORAGE GuiDocksNs gui_docks EMBED_UNUSED = {
    .init = gui_docks_init,
    .show = gui_docks_show,
    .away = gui_docks_away,
    .arrive_end_ms = gui_docks_arrive_end_ms,
};

#endif
