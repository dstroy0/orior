// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip_test.c: the strip's check: its tools stand as buttons down the strip, the top group from its head and the
// foot group up from its foot; a pixel on a button reads as its tool; the pointer moving onto one marks it; a press
// shows a tool's window, a press on another shows that one instead and a press on the one shown closes it; a tool's
// keys do the same; and the strip drawn shows each icon, the one shown on its ground
#include "../../../../../ui/gui/src/parts/strip.h"
#include "../../../../../ui/gui/src/base.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` is not `ground`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

static GuiColor pixel(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame;
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 800u, .height = 600u, .scale = 100);
    GuiStrip strip;
    EMBED_CALL(gui_strip_name.init, GuiStripNameCfg, .strip = &strip);
    cli_check((strip.pointed == GUI_STRIP_NONE) && (strip.shown == GUI_STRIP_NONE), "a strip made points at and shows no tool");
    cli_check(gui_strip_tools() == 16u, "the strip has the page's sixteen tools");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = 0u)->name, .other = "explorer", .cap = 32ull}) &&
                  (EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = 0u)->group == GUI_STRIP_TOP) && (EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = 5u)->group == GUI_STRIP_FOOT) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = 15u)->name, .other = "runs", .cap = 32ull}),
              "the explorer leads the top group and the runs end the foot group");

    const GuiRect first = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 0u);
    const GuiRect second = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 1u);
    const GuiRect last = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 15u);
    const GuiRect before_last = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 14u);
    cli_check((first.x == frame.strip.x + 3) && (first.y == frame.strip.y + 6) && (first.width == 38) && (first.height == 38),
              "the first button stands at the strip's head, in the middle of its width");
    cli_check(second.y == first.y + 38 + 4, "the buttons of a group stand a gap apart");
    cli_check((last.y + last.height == frame.strip.y + frame.strip.height - 6) && (before_last.y == last.y - 38 - 4),
              "the foot group ends at the strip's foot");

    unsigned int at = 9u;
    cli_check(EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = &frame, .x = first.x + 10, .y = first.y + 10, .at_pointer = &at) && (at == 0u), "a pixel on a button reads as its tool");
    cli_check(EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = &frame, .x = last.x + 10, .y = last.y + 10, .at_pointer = &at) && (at == 15u), "a pixel on the last button reads as the runs");
    cli_check(!EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = &frame, .x = first.x + 10, .y = second.y - 2, .at_pointer = &at), "a pixel in the gap between buttons is no tool's");
    cli_check(!EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = &frame, .x = frame.editor.x + 10, .y = first.y + 10, .at_pointer = &at), "a pixel in the editor is no tool's");

    cli_check(EMBED_CALL(gui_strip_frame_x.move, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = first.x + 10, .y = first.y + 10) && (strip.pointed == 0), "the pointer moving onto a button marks it");
    cli_check(!EMBED_CALL(gui_strip_frame_x.move, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = first.x + 12, .y = first.y + 12), "moving within the button changes nothing");
    cli_check(EMBED_CALL(gui_strip_frame_x.move, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = frame.editor.x + 10, .y = first.y + 10) && (strip.pointed == GUI_STRIP_NONE),
              "the pointer leaving the strip unmarks it");

    cli_check(EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = first.x + 10, .y = first.y + 10) && (strip.shown == 0), "a press on a tool shows its window");
    cli_check(EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = second.x + 10, .y = second.y + 10) && (strip.shown == 1), "a press on another shows that one instead");
    cli_check(EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = second.x + 10, .y = second.y + 10) && (strip.shown == GUI_STRIP_NONE),
              "a press on the one shown closes it");
    cli_check(EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = first.x + 10, .y = second.y - 2) && (strip.shown == GUI_STRIP_NONE),
              "a press in the strip off its buttons is the strip's and changes nothing");
    cli_check(!EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = frame.editor.x + 10, .y = first.y + 10), "a press in the editor is not the strip's");
    const GuiRect more = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 4u);
    cli_check(EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &strip, .frame = &frame, .x = more.x + 10, .y = more.y + 10) && (strip.shown == GUI_STRIP_NONE),
              "a press on the more tool shows no window of its own");

    cli_check(EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "Mod+Shift+E") && (strip.shown == 0), "the explorer's keys show it");
    cli_check(EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "Alt+5") && (strip.shown == 6), "the debug's keys show it instead");
    cli_check(EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "Mod+`") && (strip.shown == 7), "the terminal's keys show it instead");
    cli_check(EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "Mod+`") && (strip.shown == GUI_STRIP_NONE), "the keys of the one shown close it");
    cli_check(!EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "Mod+Q") && !EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &strip, .name = "E"), "keys no tool has are not the strip's");

    // the strip is drawn on the pane the frame's base lays; a raster of the base alone is what the strip leaves bare
    GuiRaster raster;
    GuiRaster bare_base;
    GuiBase base;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 800u, .height = 600u) && EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &bare_base, .width = 800u, .height = 600u) && EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 800u, .height = 600u),
              "a raster for the strip, one for the base alone and the base are made");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .ground = NULL, .color = palette->ground);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .ground = NULL, .color = palette->ground);
    strip.shown = 1;
    strip.pointed = 2;
    EMBED_CALL(gui_strip_frame_raster.draw, GuiStripFrameRasterCfg, .raster = &raster, .icons = &icons, .palette = palette, .frame = &frame, .strip_read = &strip);
    const GuiRect third = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = 2u);
    const GuiColor plain = pixel(&raster, first.x + 1, first.y + 19);
    cli_check(plain != palette->ground, "the strip is tinted over the ground");
    cli_check(pixel(&raster, second.x + 1, second.y + 19) != plain, "the tool shown stands on its own ground");
    cli_check((pixel(&raster, third.x + 1, third.y + 19) != plain) && (pixel(&raster, third.x + 1, third.y + 19) != pixel(&raster, second.x + 1, second.y + 19)),
              "the tool pointed stands on the pointed ground");
    cli_check(painted(&raster, (GuiRect){first.x + 5, first.y + 5, 28, 28}, plain), "a tool's icon is drawn in its button");
    cli_check(painted(&raster, (GuiRect){last.x + 5, last.y + 5, 28, 28}, plain), "the last tool's icon is drawn in its button");
    // the gap between buttons is the pane alone, as the base laid it
    int bare = 1;
    for (int y = second.y - 4; y < second.y; y += 1)
    {
        for (int x = first.x; x < first.x + 38; x += 1)
        {
            bare = bare && (pixel(&raster, x, y) == pixel(&bare_base, x, y));
        }
    }
    cli_check(bare, "the gap between buttons is bare, the pane alone");
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &base);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &bare_base);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &raster);
    EMBED_CALL(cli_icons.free, GuiIconsCfg, .icons = &icons);
    return cli_on_check();
}
