// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// statusbar_test.c: the status bar's check: the memory reading written in megabytes, the cursor as line and column
// from one, the selection as lines or letters, the indentation as tabs or spaces; the bar drawn with the file's
// crumbs at its left and the editor's line at its right, the lock at the end, bare where no file is open; and the
// notice after the crumbs with Silence beside it, a failure's in the fail color, one too long cut short within half the
// window, Focus in the signal on the right, and a press on each of them found where it is drawn
#include "../../../../../ui/cli/src/view/base.h"
#include "../../../../../ui/cli/src/view/parts/crumbs.h"
#include "../../../../../ui/cli/src/view/parts/statusbar.h"
#include "../../../../../ui/cli/src/view/view.h"
#include "../cli/cli_check.h"

// whether any pixel of `rect` is not `background_fill`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor background_fill)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != background_fill)
            {
                return 1;
            }
        }
    }
    return 0;
}

// whether every pixel of `rect` is the pane alone, as `base`, the frame's base drawn by itself, laid it
static int bare(const GuiRaster *raster, GuiRect rect, const GuiRaster *base)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != base->pixels[((unsigned int)y * base->width) + (unsigned int)x])
            {
                return 0;
            }
        }
    }
    return 1;
}

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

static int same(const char *text, const char *required)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = text, .other = required, .cap = 64ull});
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .err_msg = &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    char text[64];
    EMBED_CALL(gui_statusbar_bytes.status_memory_text, GuiStatusbarBytesCfg, .bytes = 12897484ull, .text = text, .size = sizeof(text));
    cli_check(same(text, "12.3 MB"), "a reading under a hundred megabytes is written to a tenth");
    EMBED_CALL(gui_statusbar_bytes.status_memory_text, GuiStatusbarBytesCfg, .bytes = 157286400ull, .text = text, .size = sizeof(text));
    cli_check(same(text, "150 MB"), "a reading past a hundred is written whole");
    GuiStatus status;
    memor.set(&(MemoriaCfg){.dst = &status, .val = 0, .bytes = sizeof(status)});
    status.path = "ui/gui/src/interface.c";
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = NULL, .path = status.path, .line = 0ULL);
    status.has_cursor = 1;
    status.line = 11ull;
    status.col = 2ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    status.indent_tabs = -1;
    EMBED_CALL(gui_statusbar.status_cursor_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "12:3"), "the cursor is written as its line and column from one");
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, ""), "no selection is written as nothing");
    status.anchor_col = 7ull;
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "5 selected"), "a selection within a line is written as its letters");
    status.anchor_line = 9ull;
    EMBED_CALL(gui_statusbar.status_selection_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "3 lines selected"), "a selection across lines is written as its lines");
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "4 spaces"), "an indentation nothing set is four spaces");
    status.indent_tabs = 1;
    status.indent_size = 4u;
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "Tab 4"), "an indentation of tabs is written as a tab and its step");
    status.indent_tabs = 0;
    status.indent_size = 2u;
    EMBED_CALL(gui_statusbar.status_indent_text, GuiStatusbarCfg, .status = &status, .text = text, .size = sizeof(text));
    cli_check(same(text, "2 spaces"), "an indentation of spaces is written as its step");

    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 900u, .height = 300u, .scale = 100);
    GuiRaster raster;
    GuiRaster bare_base;
    GuiBase base;
    cli_check(EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 900u, .height = 300u) && EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &bare_base, .width = 900u, .height = 300u) && EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &base, .width = 900u, .height = 300u),
              "a raster for the bar, one for the base alone and the base are made");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    status.memory = 157286400ull;
    status.anchor_line = 11ull;
    status.anchor_col = 2ull;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status);
    const GuiColor tint = raster.pixels[((unsigned int)(frame.status.y + 2) * 900u) + 30u];
    cli_check(tint != palette->background_fill, "the bar is tinted over the ground");
    cli_check(painted(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, tint), "the file's crumbs are drawn at the bar's left");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14, frame.status.y + 6, 14, 16}, tint), "the lock is drawn at the bar's right end");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 60, frame.status.y + 6, 60, 16}, tint), "the indentation is drawn before the lock");
    cli_check(bare(&raster, (GuiRect){400, frame.status.y + 3, 100, 20}, &bare_base), "the bar's middle is bare");

    // the claim holding with the bar come back, the crumbs stand at the bar's left with no strip's capacity before them
    GuiAutohideNumbers numbers = frame.claim;
    numbers.claim = GUI_FIXED_ONE;
    numbers.reveal_bottom = GUI_FIXED_ONE;
    numbers.inset_bottom = GUI_FIXED_ONE;
    GuiFrame autohidden = frame;
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &autohidden, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &autohidden, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &autohidden, .status = &status);
    cli_check(!bare(&raster, (GuiRect){autohidden.status.x + 8, autohidden.status.y + 6, 160, 16}, &bare_base), "the crumbs stand at the bar's left while the claim holds");
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &base, .frame = &frame, .palette = palette, .controls_steady = 0);
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &bare_base, .background_fill = NULL, .color = palette->background_fill);

    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    status.path = "";
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = NULL, .path = status.path, .line = 0ULL);
    status.has_cursor = 0;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status);
    cli_check(bare(&raster, (GuiRect){frame.strip.width + 16, frame.status.y + 6, 120, 16}, &bare_base), "with no file open the left is bare");
    cli_check(painted(&raster, (GuiRect){900 - 16 - 30, frame.status.y + 6, 30, 16}, tint), "the memory reading ends at the bar's right padding");
    cli_check(bare(&raster, (GuiRect){900 - 16 - 16 - 14 - 18 - 200, frame.status.y + 6, 180, 16}, &bare_base), "and the editor's line is away");

    // a notice a gap after the crumbs, Silence beside it, each pressed where it is drawn
    status.path = "ui/gui/src/interface.c";
    EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = NULL, .path = status.path, .line = 0ULL);
    status.notice = "Formatted 3 places.";
    GuiStatusPlaces places;
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status, .places = &places);
    cli_check((places.notice.width > 0) && (places.notice.x > frame.strip.width + 16 + 120), "the notice stands after the crumbs");
    cli_check(painted(&raster, places.notice, tint), "the notice is drawn");
    cli_check((places.silence.width > 0) && (places.silence.x > places.notice.x + places.notice.width), "Silence stands after the notice");
    cli_check(painted(&raster, places.silence, tint), "Silence is drawn");
    cli_check(!colored(&raster, places.notice, palette->fail), "a notice that is no failure's is not in the fail color");
    const int middle_y = frame.status.y + (frame.status.height / 2);
    cli_check(EMBED_CALL(gui_statusbar_x.hit, GuiStatusbarXCfg, .status = &status, .glyphs = &glyphs, .icons = &icons, .frame = &frame, .x = places.notice.x + 4, .y = middle_y) == CLI_HIT_STATUS_NOTICE, "a press on the notice is the notice's");
    cli_check(EMBED_CALL(gui_statusbar_x.hit, GuiStatusbarXCfg, .status = &status, .glyphs = &glyphs, .icons = &icons, .frame = &frame, .x = places.silence.x + 4, .y = middle_y) == CLI_HIT_STATUS_SILENCE, "a press on Silence is Silence's");
    cli_check(EMBED_CALL(gui_statusbar_x.hit, GuiStatusbarXCfg, .status = &status, .glyphs = &glyphs, .icons = &icons, .frame = &frame, .x = frame.strip.width + 20, .y = middle_y) == CLI_HIT_NONE, "a press on the crumbs is none of these");
    // a failure's notice in the fail color
    status.notice_failed = 1;
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status, .places = &places);
    cli_check(colored(&raster, places.notice, palette->fail), "a failure's notice is in the fail color");
    status.notice_failed = 0;
    // a notice longer than half the window cut short within it
    char longest[400];
    memor.set(&(MemoriaCfg){.dst = longest, .val = 'w', .bytes = sizeof(longest) - 1u});
    longest[sizeof(longest) - 1u] = '\0';
    status.notice = longest;
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = NULL, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status, .places = &places);
    cli_check((places.notice.width > 100) && (places.notice.width <= 450), "a notice past half the window is cut short within it");
    cli_check(places.silence.x + places.silence.width <= 900, "Silence still stands inside the bar");
    status.notice = "";
    // Focus with how many wait, in the signal, on the right
    status.focus = 1;
    status.focus_waiting = 2u;
    EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &base, .raster = &raster, .background_fill = NULL, .color = palette->background_fill);
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = &raster, .glyphs = &glyphs, .icons = &icons, .palette = palette, .frame = &frame, .status = &status, .places = &places);
    cli_check((places.focus.width > 0) && (places.focus.x > 450), "Focus stands on the right");
    cli_check(colored(&raster, places.focus, palette->signal), "Focus is in the signal");
    cli_check(EMBED_CALL(gui_statusbar_x.hit, GuiStatusbarXCfg, .status = &status, .glyphs = &glyphs, .icons = &icons, .frame = &frame, .x = places.focus.x + 4, .y = middle_y) == CLI_HIT_STATUS_FOCUS, "a press on Focus is Focus's");
    cli_check(places.notice.width == 0, "with no notice none stands");
    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    g_cli_guard->persistent_buf_release(bare_base.pixels, g_gui_raster_owner);
    bare_base.pixels = NULL;
    bare_base.width = 0u;
    bare_base.height = 0u;
    g_cli_guard->persistent_buf_release(base.tile_red_blue, g_gui_base_owner);
    base.tile_red_blue = NULL;
    base.tile_green = NULL;
    base.tile_remainder = NULL;
    base.width = 0u;
    base.height = 0u;
    base.laid = 0;
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        if (icons.entry[at].used)
        {
            g_cli_guard->persistent_buf_release(icons.entry[at].mask_bits.cover, g_gui_icons_owner);
            icons.entry[at].mask_bits.cover = NULL;
            icons.entry[at].used = 0;
        }
    }
    icons.count = 0u;
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    return cli_on_check();
}
