// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// strip_test.c: the strip's check: its tools stand as buttons down the strip, the top group from its head and the
// foot group up from its foot; a pixel on a button reads as its tool; the pointer moving onto one marks it; a press
// shows a tool's window, a press on another shows that one instead and a press on the one shown closes it; a tool's
// keys do the same; and the strip drawn shows each icon, the one shown on its ground
#include "../../../../../ui/gui/src/parts/strip.h"
#include "../../../../../ui/gui/src/parts/glass.h"
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
    (void)cli_cells_made(CLI_CELLS_POOL);
    GuiIcons icons;
    gui_icons_made(&icons);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 600u, 1.0f);
    GuiStrip strip;
    gui_strip_made(&strip);
    cli_check((strip.pointed == GUI_STRIP_NONE) && (strip.shown == GUI_STRIP_NONE), "a strip made points at and shows no tool");
    cli_check(gui_strip_tools() == 16u, "the strip has the page's sixteen tools");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = gui_strip_tool(0u)->name, .other = "explorer", .cap = 32ull}) &&
                  (gui_strip_tool(0u)->group == GUI_STRIP_TOP) && (gui_strip_tool(5u)->group == GUI_STRIP_FOOT) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = gui_strip_tool(15u)->name, .other = "runs", .cap = 32ull}),
              "the explorer leads the top group and the runs end the foot group");

    const GuiRect first = gui_strip_button(&frame, 0u);
    const GuiRect second = gui_strip_button(&frame, 1u);
    const GuiRect last = gui_strip_button(&frame, 15u);
    const GuiRect before_last = gui_strip_button(&frame, 14u);
    cli_check((first.x == frame.strip.x + 3) && (first.y == frame.strip.y + 6) && (first.width == 38) && (first.height == 38),
              "the first button stands at the strip's head, in the middle of its width");
    cli_check(second.y == first.y + 38 + 4, "the buttons of a group stand a gap apart");
    cli_check((last.y + last.height == frame.strip.y + frame.strip.height - 6) && (before_last.y == last.y - 38 - 4),
              "the foot group ends at the strip's foot");

    unsigned int at = 9u;
    cli_check(gui_strip_hit(&frame, first.x + 10, first.y + 10, &at) && (at == 0u), "a pixel on a button reads as its tool");
    cli_check(gui_strip_hit(&frame, last.x + 10, last.y + 10, &at) && (at == 15u), "a pixel on the last button reads as the runs");
    cli_check(!gui_strip_hit(&frame, first.x + 10, second.y - 2, &at), "a pixel in the gap between buttons is no tool's");
    cli_check(!gui_strip_hit(&frame, frame.editor.x + 10, first.y + 10, &at), "a pixel in the editor is no tool's");

    cli_check(gui_strip_moved(&strip, &frame, first.x + 10, first.y + 10) && (strip.pointed == 0), "the pointer moving onto a button marks it");
    cli_check(!gui_strip_moved(&strip, &frame, first.x + 12, first.y + 12), "moving within the button changes nothing");
    cli_check(gui_strip_moved(&strip, &frame, frame.editor.x + 10, first.y + 10) && (strip.pointed == GUI_STRIP_NONE),
              "the pointer leaving the strip unmarks it");

    cli_check(gui_strip_pressed(&strip, &frame, first.x + 10, first.y + 10) && (strip.shown == 0), "a press on a tool shows its window");
    cli_check(gui_strip_pressed(&strip, &frame, second.x + 10, second.y + 10) && (strip.shown == 1), "a press on another shows that one instead");
    cli_check(gui_strip_pressed(&strip, &frame, second.x + 10, second.y + 10) && (strip.shown == GUI_STRIP_NONE),
              "a press on the one shown closes it");
    cli_check(gui_strip_pressed(&strip, &frame, first.x + 10, second.y - 2) && (strip.shown == GUI_STRIP_NONE),
              "a press in the strip off its buttons is the strip's and changes nothing");
    cli_check(!gui_strip_pressed(&strip, &frame, frame.editor.x + 10, first.y + 10), "a press in the editor is not the strip's");
    const GuiRect more = gui_strip_button(&frame, 4u);
    cli_check(gui_strip_pressed(&strip, &frame, more.x + 10, more.y + 10) && (strip.shown == GUI_STRIP_NONE),
              "a press on the more tool shows no window of its own");

    cli_check(gui_strip_keyed(&strip, "Mod+Shift+E") && (strip.shown == 0), "the explorer's keys show it");
    cli_check(gui_strip_keyed(&strip, "Alt+5") && (strip.shown == 6), "the debug's keys show it instead");
    cli_check(gui_strip_keyed(&strip, "Mod+`") && (strip.shown == 7), "the terminal's keys show it instead");
    cli_check(gui_strip_keyed(&strip, "Mod+`") && (strip.shown == GUI_STRIP_NONE), "the keys of the one shown close it");
    cli_check(!gui_strip_keyed(&strip, "Mod+Q") && !gui_strip_keyed(&strip, "E"), "keys no tool has are not the strip's");

    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 800u, 600u), "a raster for the strip is made");
    gui_raster_filled(&raster, (GuiRect){0, 0, 800, 600}, palette->ground);
    strip.shown = 1;
    strip.pointed = 2;
    gui_strip_drawn(&raster, &icons, palette, &frame, &strip);
    const GuiRect third = gui_strip_button(&frame, 2u);
    const GuiColor plain = pixel(&raster, first.x + 1, first.y + 19);
    cli_check(plain != palette->ground, "the strip is tinted over the ground");
    cli_check(pixel(&raster, second.x + 1, second.y + 19) != plain, "the tool shown stands on its own ground");
    cli_check((pixel(&raster, third.x + 1, third.y + 19) != plain) && (pixel(&raster, third.x + 1, third.y + 19) != pixel(&raster, second.x + 1, second.y + 19)),
              "the tool pointed stands on the pointed ground");
    cli_check(painted(&raster, (GuiRect){first.x + 5, first.y + 5, 28, 28}, plain), "a tool's icon is drawn in its button");
    cli_check(painted(&raster, (GuiRect){last.x + 5, last.y + 5, 28, 28}, plain), "the last tool's icon is drawn in its button");
    // the gap between buttons is the glass alone: the tint over the ground, grained as the window grains it
    int bare = 1;
    for (int y = second.y - 4; y < second.y; y += 1)
    {
        for (int x = first.x; x < first.x + 38; x += 1)
        {
            const unsigned int grain = gui_grain()[(((unsigned int)y % GUI_GRAIN_TILE) * GUI_GRAIN_TILE) + ((unsigned int)x % GUI_GRAIN_TILE)];
            const unsigned int share = GUI_ALPHA_OF(palette->glass);
            const GuiColor tinted = GUI_RGB(((GUI_RED_OF(palette->glass) * share) + (GUI_RED_OF(palette->ground) * (255u - share)) + 127u) / 255u,
                                            ((GUI_GREEN_OF(palette->glass) * share) + (GUI_GREEN_OF(palette->ground) * (255u - share)) + 127u) / 255u,
                                            ((GUI_BLUE_OF(palette->glass) * share) + (GUI_BLUE_OF(palette->ground) * (255u - share)) + 127u) / 255u);
            const GuiColor grained = GUI_RGB(((0xffu * grain) + (GUI_RED_OF(tinted) * (255u - grain)) + 127u) / 255u,
                                             ((0xffu * grain) + (GUI_GREEN_OF(tinted) * (255u - grain)) + 127u) / 255u,
                                             ((0xffu * grain) + (GUI_BLUE_OF(tinted) * (255u - grain)) + 127u) / 255u);
            bare = bare && (pixel(&raster, x, y) == grained);
        }
    }
    cli_check(bare, "the gap between buttons is bare, the glass alone");
    gui_raster_given(&raster);
    gui_icons_given(&icons);
    return cli_checked();
}
