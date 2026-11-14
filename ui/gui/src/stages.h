// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages.h: the window's movement positioned once, the claim's slides, the side pane's and the terminal panel's arrival. As
// a slide is sent every stage from now until the slides rest is positioned in one cell: the frame, the holes in the work, the
// base's map and the menu bar's titles of that stage. A frame of the movement reads its stage by index and computes
// nothing, the stage's map blitting the base and the raster's coverage tables rounding the corners; the cell goes back
// as the slides rest (stages.c)
#ifndef GUI_STAGES_H
#define GUI_STAGES_H
#include "../../../includes/window/base.h"
#include "../../../includes/window/claim.h"
#include "../../../includes/window/docks.h"
#include "../../../includes/window/sides.h"
#include "embed_dispatch_layout.h"
#include "menubar.h"

typedef struct GuiStagesAutohideCfg GuiStagesAutohideCfg;
typedef struct GuiStagesCfg GuiStagesCfg;
typedef struct GuiStagesReadCfg GuiStagesReadCfg;

// the milliseconds a stage is, a frame at 120 frames a second; a slower display reads every other stage
#define GUI_STAGE_MS 8ull

// A stage: its frame; the holes the parts make in the work; its map; and its titles, in the stages' cell
typedef struct
{
    GuiFrame frame;
    GuiRect holes[4];
    unsigned int hole_count;
    GuiBaseMap map;
    const GuiTitle *title;
    unsigned int titles;
    unsigned int folded_from;
} GuiStage;

// The stages: the cell, holding the stages, their map words, their runs and their titles; how many; when the first
// is and when the last does; and the claim's count of sends they were made for
typedef struct
{
    GuiStage *stage;
    unsigned int count;
    unsigned long long start_ms;
    unsigned long long end_ms;
    unsigned int created_for;
} GuiStages;

// every stage positioned from `time_ms` until the claim's slides, the side pane's slide and the terminal panel's arrival
// end, each stage's frame `frame` as the claim's numbers position it at that stage's moment with the side as `sides_read`
// slides it for the side_menu's tool `side_menu_read` and the panel as `docks_read` brings it in, in `palette`'s
// colors, the cell taken exact: 1, or 0 with none where the cell could not be taken. The claim's slats' shares come out
// as they are at the last stage, and `base` and `menubar` as they were positioned last; taking the first stage puts the
// menu bar back
int gui_stages_init(const GuiStagesAutohideCfg *args);
// the stage located at `time_ms`, the last from when the slides end, NULL where none is made; and the cell given back
const GuiStage *gui_stages_at(const GuiStagesReadCfg *args);
// the claim's slats' shares put back to where `stage` is, as a slide is sent again from there
void gui_stage_autohide_alloc(const GuiStagesCfg *args);
struct GuiStagesAutohideCfg
{
    GuiStages *stages;
    GuiAutohide *claim;
    const GuiSides *sides_read;
    const GuiSideMenu *side_menu_read;
    const GuiDocks *docks_read;
    const GuiFrame *frame;
    GuiBase *base;
    const GuiPalette *palette;
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    unsigned long long time_ms;
};

struct GuiStagesCfg
{
    GuiAutohide *claim;
    const GuiStage *stage;
};

struct GuiStagesReadCfg
{
    unsigned long long time_ms;
    const GuiStages *stages_read;
};

typedef struct
{
    int (*init)(const GuiStagesAutohideCfg *args);
} GuiStagesAutohideNs;
EMBED_TABLE_LAYOUT(GuiStagesAutohideNs, init);
EMBED_TABLE_STORAGE GuiStagesAutohideNs gui_stages_autohide EMBED_UNUSED = {
    .init = gui_stages_init,
};

typedef struct
{
    const GuiStage *(*at)(const GuiStagesReadCfg *args);
} GuiStagesReadNs;
EMBED_TABLE_LAYOUT(GuiStagesReadNs, at);
EMBED_TABLE_STORAGE GuiStagesReadNs gui_stages_read EMBED_UNUSED = {
    .at = gui_stages_at,
};

typedef struct
{
    void (*stage_autohide_alloc)(const GuiStagesCfg *args);
} GuiStagesNs;
EMBED_TABLE_LAYOUT(GuiStagesNs, stage_autohide_alloc);
EMBED_TABLE_STORAGE GuiStagesNs gui_stages_table EMBED_UNUSED = {
    .stage_autohide_alloc = gui_stage_autohide_alloc,
};

#endif
