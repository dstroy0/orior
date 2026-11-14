// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tabs_test.c: the tabs row's check: a tab whose text has changes not saved is wider by the mark and the gap after it,
// the mark drawn before its name in the hover's color, and a tab whose close waits on a second shows ×? for its close
// mark, which is wider and still at the tab's right
#include "../../../../../includes/window/block.h"
#include "../../../../../ui/gui/src/tabs.h"
#include "../cli/cli_check.h"

#include <math.h>

// whether any pixel of `rect` is `color`
static int colored(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] == color)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    const GuiPalette *const palette = EMBED_CALL(gui_palette_scheme.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    cli_check(frame.tabs.width > 0, "the frame positions a tabs row");
    // the same file's view as it opened, changed, and changed with its close waiting
    static const char *const s_keys[3] = {"check:plain", "check:changed", "check:waiting"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        CliShape shape;
        cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_keys[at], .text = "int a;\n", .size = 7ull, .shape = &shape) && EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_keys[at], .path = "src/thread.c"),
                  "a view is opened on the file");
    }
    EMBED_CALL(cli_view_table.unsaved, CliViewCfg, .key = s_keys[1]);
    EMBED_CALL(cli_view_table.unsaved, CliViewCfg, .key = s_keys[2]);
    cli_view_closing_set(2u, 1);
    const GuiRect plain = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 0u);
    const GuiRect changed = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 1u);
    const GuiRect waiting = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 2u);
    const float mark = EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = GUI_TAB_SIZE, .text = "\xe2\x97\x8f", .length = 3u);
    cli_check(mark > 0.0f, "the code face, or the face it falls back on, draws the mark");
    cli_check(changed.width - plain.width == (int)roundf(mark) + GUI_TAB_GAP, "a changed tab is wider by the mark and the gap after it");
    cli_check(changed.x == plain.x + plain.width, "the tabs are in a row");
    const GuiRect plain_close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 1u);
    const GuiRect waiting_close = EMBED_CALL(gui_tabs_glyphs.tab_close_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 2u);
    cli_check(waiting_close.width > plain_close.width, "the close mark waiting on a second, ×?, is wider than ×");
    cli_check((waiting.width - changed.width) == (waiting_close.width - plain_close.width), "the tab grows by what its close mark grows");
    cli_check(waiting_close.x + waiting_close.width + GUI_TAB_PAD_RIGHT == waiting.x + waiting.width, "the close mark is at the tab's right, its padding after it");

    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 1440u, .height = 920u), "the raster is taken");
    (void)EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .glyphs = &glyphs, .frame = &frame, .raster = &raster, .palette = palette);
    const int left = GUI_TAB_PAD_LEFT;
    const GuiRect plain_mark = {plain.x + left, plain.y, (int)roundf(mark), plain.height};
    const GuiRect changed_mark = {changed.x + left, changed.y, (int)roundf(mark), changed.height};
    cli_check(colored(&raster, changed_mark, palette->hover), "the mark is before a changed tab's name in the hover's color");
    cli_check(!colored(&raster, plain_mark, palette->hover), "a tab as it opened has no mark");
    // the tabs are in one side_menu, and the row is a window on it that the slide moves left over the side_menu
    const int whole = EMBED_CALL(gui_tabs_glyphs.whole, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame);
    cli_check(whole == (waiting.x + waiting.width) - frame.tabs.x, "the side_menu is as wide as the tabs from the row's left edge");
    frame.tabs_left = 30;
    const GuiRect slid = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 1u);
    cli_check((slid.x == changed.x - 30) && (slid.width == changed.width), "a slide moves each tab left by its pixels and retains its width");
    cli_check(EMBED_CALL(gui_tabs_glyphs.whole, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame) == whole, "a slide retains the side_menu's width");
    unsigned int hit_at = 9u;
    int hit_closing = 0;
    cli_check(EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &glyphs, .frame = &frame, .x = slid.x + 2, .y = slid.y + 2, .at_pointer = &hit_at, .closing = &hit_closing) && (hit_at == 1u),
              "a press finds the tab where the slide put it");
    frame.tabs_left = 0;
    // halfway through the claim the row is whole, risen by half out of the work's head, each tab with it
    GuiAutohideNumbers numbers = frame.claim;
    numbers.claim = GUI_FIXED_ONE / 2;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &frame, .numbers = &numbers);
    const GuiRect risen = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &glyphs, .frame = &frame, .at = 0u);
    cli_check((risen.y == frame.tabs_row.y + GUI_TAB_CLEAR) && (risen.height == frame.tabs_row.height - GUI_TAB_CLEAR - 1) && (risen.y < frame.tabs.y),
              "halfway through the claim a tab is whole in the row risen out of the work's head");
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
