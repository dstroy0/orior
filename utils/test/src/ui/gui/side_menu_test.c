// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// side_menu_test.c: the side_menu's check: a side_menu starts with no tool pointed and its beads at rest; the pointer on a tool's
// button points it and off the side_menu points none; the pointer leaving the window points none; a press on a tool
// flattens that tool's bead alone, a press on another lets the first go, and the release springs the pressed one
// back; a press past the tools does nothing; and the side_menu drawn lays the atlas out for every tool at its place and
// bakes every tool's frames
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/side_menu.h"
#include "../cli/cli_check.h"

// the place among the tools of the tool `name`
static unsigned int tool_at(const char *name)
{
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_gui_tools[at].name, .other = name, .cap = 32ULL}))
        {
            return at;
        }
    }
    return 0u;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    GuiSideMenu side_menu;
    EMBED_CALL(gui_side_menu_table.init, GuiSideMenuInitCfg, .side_menu = &side_menu);
    cli_check((side_menu.pointed == GUI_SIDE_MENU_NONE) && (side_menu.buttons.pressed == GUI_BEAD_TOOLS_MAX) &&
                  (EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &side_menu.buttons, .time_ms = 0ULL) == 0ULL),
              "a side_menu starts with no tool pointed and its beads at rest");
    const unsigned int explorer = tool_at("explorer");
    const unsigned int search = tool_at("search");
    const GuiRect explorer_button = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = &frame, .at = explorer);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);

    cli_check(EMBED_CALL(gui_side_menu_frame_x.move, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame,
                         .x = explorer_button.x + 2, .y = explorer_button.y + 2) &&
                  (side_menu.pointed == (int)explorer),
              "the pointer on a tool's button points it");
    cli_check(EMBED_CALL(gui_side_menu_frame_x.move, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                         .y = editor_y) &&
                  (side_menu.pointed == GUI_SIDE_MENU_NONE),
              "the pointer off the side_menu points none");
    (void)EMBED_CALL(gui_side_menu_frame_x.move, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame,
                     .x = explorer_button.x + 2, .y = explorer_button.y + 2);
    EMBED_CALL(gui_side_menu_frame_x.leave, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame);
    cli_check(side_menu.pointed == GUI_SIDE_MENU_NONE, "the pointer leaving the window points none");

    EMBED_CALL(gui_side_menu_frame_x.press, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .at = (int)explorer,
               .time_ms = 1000ULL);
    cli_check((side_menu.buttons.pressed == explorer) && (side_menu.buttons.press[explorer].to == 2 * GUI_FIXED_ONE) &&
                  (side_menu.buttons.press[search].to == GUI_FIXED_ONE),
              "a press on a tool flattens that tool's bead alone");
    EMBED_CALL(gui_side_menu_frame_x.press, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .at = (int)search,
               .time_ms = 1050ULL);
    cli_check((side_menu.buttons.pressed == search) && (side_menu.buttons.press[explorer].to == GUI_FIXED_ONE) &&
                  (side_menu.buttons.press[search].to == 2 * GUI_FIXED_ONE),
              "a press on another tool lets the first go");
    EMBED_CALL(gui_side_menu_frame_x.release, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .time_ms = 1100ULL);
    cli_check((side_menu.buttons.pressed == GUI_BEAD_TOOLS_MAX) && (side_menu.buttons.press[search].to == GUI_FIXED_ONE) &&
                  (side_menu.buttons.press[search].curve == GUI_CURVE_BEAD),
              "the release springs the pressed bead back");
    cli_check(EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &side_menu.buttons, .time_ms = 1100ULL) == 0ULL,
              "the beads' end is 0 before the atlas is laid out, the side_menu not yet drawn");
    EMBED_CALL(gui_side_menu_frame_x.press, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .at = GUI_SIDE_MENU_NONE,
               .time_ms = 1200ULL);
    cli_check(side_menu.buttons.pressed == GUI_BEAD_TOOLS_MAX, "a press past the tools does nothing");

    // the side_menu drawn lays the atlas out for every tool at its place and bakes every tool's frames
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiIcons icons;
    EMBED_CALL(gui_icons.init, GuiIconsCfg, .icons = &icons);
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u),
              "a raster the frame's size is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 1440, 920},
               .color = palette->background_fill);
    EMBED_CALL(gui_side_menu_frame_raster.draw, GuiSideMenuFrameRasterCfg, .raster = &raster, .icons = &icons, .palette = palette,
               .frame = &frame, .side_menu_read = &side_menu, .buttons = &side_menu.buttons, .time_ms = 3000ULL);
    const int pad = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BEAD_PAD, .scale = frame.scale);
    cli_check(side_menu.buttons.atlas.made && (side_menu.buttons.atlas.tools == g_gui_tools_count) &&
                  (side_menu.buttons.atlas.width == (unsigned int)GUI_SIDE_MENU_BUTTON) &&
                  (side_menu.buttons.atlas.origin[explorer].y == explorer_button.y - pad),
              "the side_menu drawn lays the atlas out for every tool at its place");
    cli_check(side_menu.buttons.atlas.baked == (1u << g_gui_tools_count) - 1u, "and bakes every tool's frames");
    EMBED_CALL(gui_side_menu_frame_x.press, GuiSideMenuFrameXCfg, .side_menu = &side_menu, .frame = &frame, .at = (int)search,
               .time_ms = 4000ULL);
    cli_check(EMBED_CALL(gui_buttons.end_ms, GuiButtonsCfg, .buttons_read = &side_menu.buttons, .time_ms = 4000ULL) == 4000ULL + GUI_BUTTONS_PRESS_MS,
              "a press on a drawn side_menu runs its bead to the press's end");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
