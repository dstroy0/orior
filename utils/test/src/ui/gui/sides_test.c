// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides_test.c: the side pane's check: a press shows a tool's pane and a press on it shown collapses it; the pane
// collapses a rest after the pointer leaves it and not while the pointer stands on the strip or a menu is open; the
// pointer resting on the edge brings it back, a tremble off the edge within the grace still resting there and a leave
// past it ending the rest; and a rest on another side tool's icon shows its pane as a preview, which the pointer
// leaving the strip and the pane takes back and a press on the icon ends with its pane standing; and the slide, a
// pane coming in from all its way gone and one collapsed going over its slide on the ease, its width standing until the
// slide ends
#include "../../../../../ui/cli/src/view/sides.h"
#include "../cli/cli_check.h"

// the place among the tools of the side tool `name`
static unsigned int tool_at(const char *name)
{
    for (unsigned int at = 0u; at < g_cli_tools_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = name, .cap = 32ULL}))
        {
            return at;
        }
    }
    return 0u;
}

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    GuiStrip strip;
    EMBED_CALL(gui_strip_table.init, GuiStripInitCfg, .strip = &strip);
    GuiSides sides;
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &sides);
    const unsigned int explorer = tool_at("explorer");
    const unsigned int search = tool_at("search");
    const int pane_x = frame.side.x + (frame.side.width / 2);
    const int pane_y = frame.side.y + (frame.side.height / 2);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);
    const int strip_x = frame.strip.x + (frame.strip.width / 2);
    const int strip_y = frame.strip.y + frame.strip.height - 4;
    const int edge_x = frame.inside.x + 2;

    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &sides, .strip = &strip, .at = explorer, .time_ms = 0ULL);
    cli_check((strip.displayed == (int)explorer) && !strip.collapsed, "a press on a tool's icon shows its pane");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = pane_x,
                     .y = pane_y, .time_ms = 100ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 200ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 899ULL) &&
                  !strip.collapsed,
              "the pane stands until its rest after the pointer leaves it");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 900ULL) &&
                  strip.collapsed && (strip.displayed == (int)explorer),
              "and collapses at its rest, its tool still the strip's");

    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 1000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 1249ULL),
              "the pointer on the edge shows nothing before its rest");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 1250ULL) &&
                  !strip.collapsed,
              "its rest on the edge brings the pane back");

    strip.collapsed = 1;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 2000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 2100ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 2150ULL);
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 2250ULL) &&
                  !strip.collapsed,
              "a pointer off the edge and back within the grace still rests there");

    strip.collapsed = 1;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 3000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 3050ULL);
    (void)EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 3170ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 3250ULL) &&
                  strip.collapsed,
              "a pointer off the edge past the grace ends its rest");

    strip.collapsed = 0;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = pane_x,
                     .y = pane_y, .time_ms = 4000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = strip_x,
                     .y = strip_y, .time_ms = 4100ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 4800ULL) &&
                  !strip.collapsed,
              "the pointer on the strip leaves the pane standing");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 5000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .menu_open = 1,
                          .time_ms = 5700ULL) &&
                  !strip.collapsed,
              "a menu open leaves the pane standing");

    const GuiRect button = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &frame, .at = search);
    strip.pointed = (int)search;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame,
                     .x = button.x + 2, .y = button.y + 2, .time_ms = 6000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 6349ULL) &&
                  (strip.displayed == (int)explorer),
              "the pointer on another tool's icon shows nothing before its rest");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 6350ULL) &&
                  (strip.displayed == (int)search) && !strip.collapsed,
              "its rest on the icon shows the tool's pane as a preview");
    strip.pointed = GUI_STRIP_NONE;
    cli_check(EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame, .x = editor_x,
                         .y = editor_y, .time_ms = 6500ULL) &&
                  (strip.displayed == (int)explorer),
              "the pointer leaving the strip and the pane takes the preview back");

    strip.pointed = (int)search;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame,
                     .x = button.x + 2, .y = button.y + 2, .time_ms = 7000ULL);
    (void)EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .strip = &strip, .time_ms = 7350ULL);
    cli_check(EMBED_CALL(gui_sides.press, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame,
                         .x = button.x + 2, .y = button.y + 2),
              "a press on the icon the preview shows is the preview's");
    strip.pointed = GUI_STRIP_NONE;
    cli_check(!EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .strip = &strip, .frame = &frame,
                          .x = editor_x, .y = editor_y, .time_ms = 7500ULL) &&
                  (strip.displayed == (int)search),
              "and the pane stands as the pointer leaves");

    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &sides, .strip = &strip, .at = search, .time_ms = 8000ULL);
    cli_check(strip.collapsed && (strip.displayed == (int)search), "a press on the icon of the pane shown collapses it");
    cli_check(EMBED_CALL(gui_sides.next_deadline_ms, GuiSidesCfg, .sides_read = &sides) == 0ULL,
              "with the pane collapsed by a press and the pointer at rest, no wait stands");

    // the slide: a pane where none showed comes in from all its way gone, and one collapsed goes over the slide, its
    // width standing until the slide ends; with motion still it stands where it goes at once
    GuiStrip sliding;
    EMBED_CALL(gui_strip_table.init, GuiStripInitCfg, .strip = &sliding);
    GuiSides slid;
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &slid);
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .strip = &sliding, .at = explorer, .time_ms = 10000ULL);
    cli_check((EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 10000ULL) == GUI_FIXED_ONE) &&
                  (EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 10220ULL) == 0) &&
                  EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .strip_read = &sliding,
                             .time_ms = 10000ULL),
              "a pane shown where none showed takes its width at once and comes in over its slide");
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .strip = &sliding, .at = explorer, .time_ms = 11000ULL);
    const int halfway = EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11110ULL);
    cli_check((EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11000ULL) == 0) &&
                  (halfway > 0) && (halfway < GUI_FIXED_ONE) &&
                  (EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11220ULL) == GUI_FIXED_ONE),
              "a pane collapsed goes over its slide on the ease");
    cli_check(EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .strip_read = &sliding,
                         .time_ms = 11219ULL) &&
                  !EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .strip_read = &sliding,
                              .time_ms = 11220ULL),
              "and its width stands until its slide ends");
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .strip = &sliding, .at = explorer, .still = 1,
               .time_ms = 12000ULL);
    cli_check(EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 12000ULL) == 0,
              "with motion still the pane stands shown at once");
    return cli_on_check();
}
