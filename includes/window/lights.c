// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lights.c: the window's lights placed from the frame
#include "lights.h"

// The key light's height above the glass as a share of the window's larger side: near enough that the reflection
// sits elsewhere on each button, as one light over the pane does. The fill light's place past the bottom right
// corner as shares of the width and height, its height, and its strength
#define LIGHTS_KEY_Z 0.5
#define LIGHTS_FILL_X 1.2
#define LIGHTS_FILL_Y 1.2
#define LIGHTS_FILL_Z 0.9
#define LIGHTS_FILL_STRENGTH 0.15

unsigned int gui_lights_placed(const GuiLightsCfg *args)
{
    const double width = (double)args->width;
    const double height = (double)args->height;
    const double larger = (width > height) ? width : height;
    args->lights[0] = (GuiLight){args->key_x, args->key_y, LIGHTS_KEY_Z * larger, 1.0};
    args->lights[1] =
        (GuiLight){LIGHTS_FILL_X * width, LIGHTS_FILL_Y * height, LIGHTS_FILL_Z * larger, LIGHTS_FILL_STRENGTH};
    return 2u;
}

unsigned int gui_lights(const GuiLightsCfg *args)
{
    const GuiFrame *const frame = args->frame;
    // the key light over the wordmark's eye: the middle of the side menu's width across and of the top menu's height
    // down
    return EMBED_CALL(gui_lights_placed, GuiLightsCfg, .lights = args->lights, .width = frame->width,
                      .height = frame->height, .key_x = (double)frame->side_menu.width / 2.0,
                      .key_y = (double)frame->top_menu.height / 2.0);
}
