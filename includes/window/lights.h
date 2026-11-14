// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lights.h: the window's lights, the places every lit thing in the window reflects: the key light over the wordmark's
// eye at the top menu's head, and a dim fill light beyond the window's bottom right corner (lights.c)
#ifndef GUI_WINDOW_LIGHTS_H
#define GUI_WINDOW_LIGHTS_H
#include "frame.h"
#include "../glass/bead.h"
#include "embed_dispatch_layout.h"

typedef struct GuiLightsCfg GuiLightsCfg;

// The window's lights, into `lights`, GUI_LIGHTS_MAX of them: how many. The key light hangs over the wordmark's eye,
// half the window's larger side above the glass, so every reflection points to that corner and a bead pressed sinks
// away from it. The fill light stands beyond the window's bottom right corner, so the far rims are not black. The
// placed lights are the same over a window `width` by `height` with the key light over `key_x`, `key_y`, for a
// window of its own with no frame
unsigned int gui_lights(const GuiLightsCfg *args);
unsigned int gui_lights_placed(const GuiLightsCfg *args);

struct GuiLightsCfg
{
    GuiLight *lights;
    const GuiFrame *frame;
    unsigned int width;
    unsigned int height;
    double key_x;
    double key_y;
};

typedef struct
{
    unsigned int (*lights)(const GuiLightsCfg *args);
    unsigned int (*placed)(const GuiLightsCfg *args);
} GuiLightsNs;
EMBED_TABLE_LAYOUT(GuiLightsNs, lights, placed);
EMBED_TABLE_STORAGE GuiLightsNs gui_lights_table EMBED_UNUSED = {
    .lights = gui_lights,
    .placed = gui_lights_placed,
};

#endif
