// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lights_test.c: the window's lights' check: the key light hangs over the wordmark's eye at the top menu's head,
// above the glass at full strength, and the fill light stands beyond the window's bottom right corner, weaker
#include "../../../../../includes/window/lights.h"
#include "../cli/cli_check.h"

#define LIGHTS_TEST_WIDTH 1440u
#define LIGHTS_TEST_HEIGHT 920u

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    GuiLight lights[GUI_LIGHTS_MAX];
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = LIGHTS_TEST_WIDTH,
               .height = LIGHTS_TEST_HEIGHT, .scale = 100);
    const unsigned int light_count = EMBED_CALL(gui_lights_table.lights, GuiLightsCfg, .lights = lights, .frame = &frame);
    cli_check(light_count == 2u, "the window has a key light and a fill light");
    cli_check((lights[0].x > 0.0) && (lights[0].x < (double)frame.side_menu.width) && (lights[0].y > 0.0) &&
                  (lights[0].y < (double)frame.top_menu.height),
              "the key light hangs over the wordmark's eye at the top menu's head");
    cli_check((lights[0].z > 0.0) && (lights[0].strength == 1.0), "above the glass at full strength");
    cli_check((lights[1].x > (double)LIGHTS_TEST_WIDTH) && (lights[1].y > (double)LIGHTS_TEST_HEIGHT),
              "the fill light stands beyond the bottom right");
    cli_check((lights[1].z > 0.0) && (lights[1].strength < lights[0].strength), "above the glass, weaker");
    return cli_on_check();
}
