// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sides_test.c: the side pane's check: a press shows a tool's pane and a press on it shown collapses it; the pane
// collapses a rest after the pointer leaves it and not while the pointer stands on the side_menu or a menu is open; the
// pointer resting on the edge brings it back, a tremble off the edge within the grace still resting there and a leave
// past it ending the rest; and a rest on another side tool's icon shows its pane as a preview, which the pointer
// leaving the side_menu and the pane takes back and a press on the icon ends with its pane standing; and the slide, a
// pane coming in from all its way gone and one collapsed going over its slide on the ease, its width standing until the
// slide ends
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/sides.h"
#include "../cli/cli_check.h"

// the place among the tools of the side tool `name`
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
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    GuiSideMenu side_menu;
    EMBED_CALL(gui_side_menu_table.init, GuiSideMenuInitCfg, .side_menu = &side_menu);
    GuiSides sides;
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &sides);
    const unsigned int explorer = tool_at("explorer");
    const unsigned int search = tool_at("search");
    const int pane_x = frame.side.x + (frame.side.width / 2);
    const int pane_y = frame.side.y + (frame.side.height / 2);
    const int editor_x = frame.editor.x + (frame.editor.width / 2);
    const int editor_y = frame.editor.y + (frame.editor.height / 2);
    const int side_menu_x = frame.side_menu.x + (frame.side_menu.width / 2);
    const int side_menu_y = frame.side_menu.y + frame.side_menu.height - 4;
    const int edge_x = frame.inside.x + 2;

    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .at = explorer, .time_ms = 0ULL);
    cli_check((side_menu.displayed == (int)explorer) && !side_menu.collapsed, "a press on a tool's icon shows its pane");
    cli_check(sides.collapse_ms == 0ULL, "the pane shown by a press stands with no wait");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = side_menu_x,
                     .y = side_menu_y, .time_ms = 10ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 20ULL);
    cli_check(sides.collapse_ms == 20ULL + GUI_SIDES_REST_MS, "the pointer leaving the side_menu for the editor begins the pane's rest");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = pane_x,
                     .y = pane_y, .time_ms = 100ULL);
    cli_check(sides.collapse_ms == 0ULL, "the pointer back on the pane ends its rest");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 200ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 899ULL) &&
                  !side_menu.collapsed,
              "the pane stands until its rest after the pointer leaves it");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 900ULL) &&
                  side_menu.collapsed && (side_menu.displayed == (int)explorer),
              "and collapses at its rest, its tool still the side_menu's");

    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 1000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 1249ULL),
              "the pointer on the edge shows nothing before its rest");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 1250ULL) &&
                  !side_menu.collapsed,
              "its rest on the edge brings the pane back");
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 1999ULL) &&
                  !side_menu.collapsed && (sides.collapse_ms == 0ULL),
              "the pointer resting where the pane came stands on it, with no wait");

    side_menu.collapsed = 1;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 2000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 2100ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 2150ULL);
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 2250ULL) &&
                  !side_menu.collapsed,
              "a pointer off the edge and back within the grace still rests there");

    side_menu.collapsed = 1;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = edge_x,
                     .y = pane_y, .time_ms = 3000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 3050ULL);
    (void)EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 3170ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 3250ULL) &&
                  side_menu.collapsed,
              "a pointer off the edge past the grace ends its rest");

    side_menu.collapsed = 0;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = pane_x,
                     .y = pane_y, .time_ms = 4000ULL);
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = side_menu_x,
                     .y = side_menu_y, .time_ms = 4100ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 4800ULL) &&
                  !side_menu.collapsed,
              "the pointer on the side_menu leaves the pane standing");
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 5000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .menu_open = 1,
                          .time_ms = 5700ULL) &&
                  !side_menu.collapsed,
              "a menu open leaves the pane standing");

    const GuiRect button = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = &frame, .at = search);
    side_menu.pointed = (int)search;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame,
                     .x = button.x + 2, .y = button.y + 2, .time_ms = 6000ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 6349ULL) &&
                  (side_menu.displayed == (int)explorer),
              "the pointer on another tool's icon shows nothing before its rest");
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 6350ULL) &&
                  (side_menu.displayed == (int)search) && !side_menu.collapsed,
              "its rest on the icon shows the tool's pane as a preview");
    side_menu.pointed = GUI_SIDE_MENU_NONE;
    cli_check(!EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                          .y = editor_y, .time_ms = 6500ULL) &&
                  (side_menu.displayed == (int)search) && !side_menu.collapsed,
              "the pointer leaving the side_menu and the pane leaves the preview standing through the rest");
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 7199ULL) &&
                  (side_menu.displayed == (int)search),
              "a tick inside the rest takes nothing back");
    side_menu.pointed = (int)search;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame,
                     .x = button.x + 2, .y = button.y + 2, .time_ms = 6900ULL);
    cli_check(!EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 7300ULL) &&
                  (side_menu.displayed == (int)search) && !side_menu.collapsed,
              "the pointer back on the side_menu inside the rest keeps the preview");
    side_menu.pointed = GUI_SIDE_MENU_NONE;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame, .x = editor_x,
                     .y = editor_y, .time_ms = 7400ULL);
    cli_check(EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 8100ULL) &&
                  (side_menu.displayed == (int)explorer),
              "the rest off the side_menu and the pane reached takes the preview back");

    side_menu.pointed = (int)search;
    (void)EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame,
                     .x = button.x + 2, .y = button.y + 2, .time_ms = 9000ULL);
    (void)EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .time_ms = 9350ULL);
    cli_check(EMBED_CALL(gui_sides.press, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame,
                         .x = button.x + 2, .y = button.y + 2),
              "a press on the icon the preview shows is the preview's");
    side_menu.pointed = GUI_SIDE_MENU_NONE;
    cli_check(!EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .frame = &frame,
                          .x = editor_x, .y = editor_y, .time_ms = 9500ULL) &&
                  (side_menu.displayed == (int)search),
              "and the pane stands as the pointer leaves");

    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &sides, .side_menu = &side_menu, .at = search, .time_ms = 9800ULL);
    cli_check(side_menu.collapsed && (side_menu.displayed == (int)search), "a press on the icon of the pane shown collapses it");
    cli_check(EMBED_CALL(gui_sides.next_deadline_ms, GuiSidesCfg, .sides_read = &sides) == 0ULL,
              "with the pane collapsed by a press and the pointer at rest, no wait stands");

    // the slide: a pane where none showed comes in from all its way gone, and one collapsed goes over the slide, its
    // width standing until the slide ends; with motion still it stands where it goes at once
    GuiSideMenu sliding;
    EMBED_CALL(gui_side_menu_table.init, GuiSideMenuInitCfg, .side_menu = &sliding);
    GuiSides slid;
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &slid);
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .side_menu = &sliding, .at = explorer, .time_ms = 10000ULL);
    cli_check((EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 10000ULL) == GUI_FIXED_ONE) &&
                  (EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 10220ULL) == 0) &&
                  EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .side_menu_read = &sliding,
                             .time_ms = 10000ULL),
              "a pane shown where none showed takes its width at once and comes in over its slide");
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .side_menu = &sliding, .at = explorer, .time_ms = 11000ULL);
    const int quarter = EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11055ULL);
    const int halfway = EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11120ULL);
    cli_check((EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11000ULL) == 0) &&
                  (quarter > 0) && (quarter < GUI_FIXED_ONE) && (halfway > GUI_FIXED_ONE) &&
                  (halfway < GUI_FIXED_ONE + (GUI_FIXED_ONE / 10)) &&
                  (EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 11220ULL) == GUI_FIXED_ONE),
              "a pane collapsed goes over its slide as a bead: on its way at a quarter, a little past its end at the "
              "middle, and at its end as the slide ends");
    cli_check(EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .side_menu_read = &sliding,
                         .time_ms = 11219ULL) &&
                  !EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &slid, .side_menu_read = &sliding,
                              .time_ms = 11220ULL),
              "and its width stands until its slide ends");
    EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &slid, .side_menu = &sliding, .at = explorer, .still = 1,
               .time_ms = 12000ULL);
    cli_check(EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &slid, .time_ms = 12000ULL) == 0,
              "with motion still the pane stands shown at once");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
