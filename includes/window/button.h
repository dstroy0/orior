// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// button.h: a group of buttons as beads of water on the glass, the side menu's tools, the top menu's tools or the
// window controls: their beads' atlas over a segment of the block's bead part, baked in the palette's bead colors
// under the window's lights; each button's press, a slide the frame reads the bead's frame from; and the button held
// pressed. A press sinks and flattens the button's own bead over GUI_BUTTONS_PRESS_MS, and the release springs it back
// with a wobble over GUI_BUTTONS_RELEASE_MS (button.c)
#ifndef GUI_WINDOW_BUTTON_H
#define GUI_WINDOW_BUTTON_H
#include "claim.h"
#include "palette.h"
#include "../glass/bead.h"
#include "embed_dispatch_layout.h"

typedef struct GuiButtonsCfg GuiButtonsCfg;
typedef struct GuiButtonsBakeCfg GuiButtonsBakeCfg;
typedef struct GuiButtonsMoveCfg GuiButtonsMoveCfg;
typedef struct GuiButtonsDrawCfg GuiButtonsDrawCfg;

// the press and the release, in milliseconds
#define GUI_BUTTONS_PRESS_MS 90ULL
#define GUI_BUTTONS_RELEASE_MS 320ULL

// The buttons: the atlas of their beads. The press of each button, GUI_FIXED_ONE at rest to twice that pressed, below
// rest in the wobble. Then the button that stays pressed, GUI_BEAD_TOOLS_MAX for none
typedef struct
{
    GuiBeadAtlas atlas;
    GuiSlide press[GUI_BEAD_TOOLS_MAX];
    unsigned int pressed;
} GuiButtons;

// buttons at rest with nothing baked
void gui_buttons_init(const GuiButtonsCfg *args);
// the bytes that the atlas of the beads takes for `tools` buttons `width` by `height` pixels at max. The margin `pad`
// goes round each, with the reach `reach`. The result is 0 where the bead refuses the sizes.
//
// Then the atlas arranged for them, over `pixels`, which the caller passes with the bytes in it:
//   - their beads are of `shape`, with the corner `radius`.
//   - the top lefts of their cells in the window are `cells`, with the own width and height of each button, 0 for the
//     max.
//   - `light_count` lights of `lights` light them in the bead colors of the palette.
//
// The result is 1, or 0 where there are no pixels or the bead refuses the sizes. A button whose frames had a bake for
// the same sizes, shape, place, lights and colors keeps them
unsigned long long gui_buttons_bytes(const GuiButtonsBakeCfg *args);
int gui_buttons_bake(const GuiButtonsBakeCfg *args);
// the button `tool` pressed at `time_ms`, and it stays pressed. Then the pressed button let go at `time_ms`, which
// springs its bead back with a wobble
void gui_buttons_press(const GuiButtonsMoveCfg *args);
void gui_buttons_release(const GuiButtonsMoveCfg *args);
// when the buttons' presses end at `time_ms`, 0 where none runs
unsigned long long gui_buttons_end_ms(const GuiButtonsCfg *args);
// the bead of the button `tool`, drawn as its press is at `time_ms`, with the top left of its cell at x, y, inside
// `clip`. The draw takes `fade` of 255 off its opacity, 0 for the bead whole
void gui_buttons_draw(const GuiButtonsDrawCfg *args);

struct GuiButtonsCfg
{
    GuiButtons *buttons;
    const GuiButtons *buttons_read;
    unsigned long long time_ms;
};
struct GuiButtonsBakeCfg
{
    GuiButtons *buttons;
    const GuiPalette *palette;
    unsigned int width;
    unsigned int height;
    unsigned int pad;
    unsigned int reach;
    unsigned int shape;
    unsigned int radius;
    const GuiRect *cells;
    unsigned int tools;
    const GuiLight *lights;
    unsigned int light_count;
    GuiColor *pixels;
};
struct GuiButtonsMoveCfg
{
    GuiButtons *buttons;
    unsigned int tool;
    unsigned long long time_ms;
};
struct GuiButtonsDrawCfg
{
    GuiButtons *buttons;
    GuiRaster *raster;
    unsigned int tool;
    int x;
    int y;
    unsigned int fade;
    unsigned long long time_ms;
    GuiRect clip;
};
typedef struct
{
    void (*init)(const GuiButtonsCfg *args);
    unsigned long long (*end_ms)(const GuiButtonsCfg *args);
} GuiButtonsNs;
EMBED_TABLE_LAYOUT(GuiButtonsNs, init, end_ms);
EMBED_TABLE_STORAGE GuiButtonsNs gui_buttons EMBED_UNUSED = {
    .init = gui_buttons_init,
    .end_ms = gui_buttons_end_ms,
};
typedef struct
{
    unsigned long long (*bytes)(const GuiButtonsBakeCfg *args);
    int (*bake)(const GuiButtonsBakeCfg *args);
} GuiButtonsBakeNs;
EMBED_TABLE_LAYOUT(GuiButtonsBakeNs, bytes, bake);
EMBED_TABLE_STORAGE GuiButtonsBakeNs gui_buttons_bake_table EMBED_UNUSED = {
    .bytes = gui_buttons_bytes,
    .bake = gui_buttons_bake,
};
typedef struct
{
    void (*press)(const GuiButtonsMoveCfg *args);
    void (*release)(const GuiButtonsMoveCfg *args);
} GuiButtonsMoveNs;
EMBED_TABLE_LAYOUT(GuiButtonsMoveNs, press, release);
EMBED_TABLE_STORAGE GuiButtonsMoveNs gui_buttons_move EMBED_UNUSED = {
    .press = gui_buttons_press,
    .release = gui_buttons_release,
};
typedef struct
{
    void (*draw)(const GuiButtonsDrawCfg *args);
} GuiButtonsDrawNs;
EMBED_TABLE_LAYOUT(GuiButtonsDrawNs, draw);
EMBED_TABLE_STORAGE GuiButtonsDrawNs gui_buttons_draw_table EMBED_UNUSED = {
    .draw = gui_buttons_draw,
};
#endif
