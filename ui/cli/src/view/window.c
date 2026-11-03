// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the view of what the controller publishes, on the view's thread. It finds what a pixel stands on and writes
// that as an input, takes the newest snapshot of each part as it is woken, and draws them; it asks nothing and decides
// nothing, and every command is the controller's, whoever pressed what. What is its own is what moves on
// the screen and nothing else: the claim's slides, the name's flight, the circle's stages and the caret's breath, the
// rows' scroll offsets and the marks the pointer leaves on them
#include "window.h"
#include "../command_line/callback_cfg.h"

#include "../text/text.h"
#include "parts/chrome.h"

#include <math.h>

int gui_window_begin(const GuiWindowWokenCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    memor.set(&(MemoriaCfg){.dst = args->window, .val = 0, .bytes = sizeof(*args->window)});
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &args->window->palette_own, .scheme = GUI_SCHEME_DARK);
    args->window->palette = &args->window->palette_own.colors;
    EMBED_CALL(gui_preferences_size.init, GuiPreferencesSizeCfg, .preferences = &args->window->preferences);
    args->window->preference_values.zoom_step = GUI_PREFERENCE_ZOOM_DEFAULT_STEP;
    args->window->raster = &args->window->framebuffer.raster[0];
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &args->window->editor);
    EMBED_CALL(gui_menubar.init, GuiMenubarCfg, .menubar = &args->window->menubar);
    EMBED_CALL(gui_strip_table.init, GuiStripInitCfg, .strip = &args->window->strip);
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &args->window->icons);
    EMBED_CALL(cli_explorer.init, GuiExplorerCfg, .explorer = &args->window->explorer);
    EMBED_CALL(cli_autohide_still.init, GuiAutohideStillCfg, .claim = &args->window->claim);
    EMBED_CALL(cli_wordmark_still.init, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    EMBED_CALL(cli_busy_still.init, GuiBusyStillCfg, .busy = &args->window->busy);
    EMBED_CALL(gui_hover.init, GuiHoverCfg, .hover = &args->window->hover);
    EMBED_CALL(gui_signature.init, GuiSignatureCfg, .signature = &args->window->signature);
    EMBED_CALL(gui_suggest.init, GuiSuggestCfg, .suggest = &args->window->suggest);
    EMBED_CALL(cli_problems.init, GuiProblemsCfg, .problems_out = &args->window->problems);
    if (!EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &args->window->background_fill))
    {
        mmgr_line_put(err_msg, "the lattice has no slot left for the ground");
        return 0;
    }
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &args->window->glyphs, .err_msg = err_msg))
    {
        return 0;
    }
    args->window->pressed = GUI_HIT_CLIENT;
    args->window->display_width = args->display_width;
    args->window->display_height = args->display_height;
    return 1;
}

typedef struct
{
    GuiWindow *window;
    const GuiWindow *window_read;
    int on;
    int x;
    int y;
    const char *key;
    const char *value;
    unsigned int percent;
    const GuiPreferenceChange *change;
    GuiRect caret;
    unsigned long long through;
    unsigned int from;
    unsigned int to;
    CliInput *input;
    const char *bytes;
    unsigned long long length;
} GuiWindowCtx;

void gui_window_end(const GuiWindowCfg *args)
{
    // every snapshot goes with the controller: nothing of them is read from here on
    args->window->editor.frame = NULL;
    args->window->editor.names = NULL;
    args->window->minimap.lines = NULL;
    args->window->hover.panel = NULL;
    args->window->suggest.panel = NULL;
    args->window->signature.panel = NULL;
    args->window->menubar.menus = NULL;
    args->window->explorer.rows = NULL;
    args->window->problems.problems = NULL;
    args->window->tabs = NULL;
    args->window->status = NULL;
    cli_controller_end();
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &args->window->glyphs);
    EMBED_CALL(cli_icons.free, GuiIconsCfg, .icons = &args->window->icons);
    EMBED_CALL(cli_wordmark_still.free, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    EMBED_CALL(cli_busy_still.free, GuiBusyStillCfg, .busy = &args->window->busy);
    EMBED_CALL(gui_chrome_raster.controls_shadow_free, GuiChromeRasterCfg, .shadow = &args->window->controls_shadow);
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &args->window->background_fill);
    EMBED_CALL(gui_stages_autohide.free, GuiStagesAutohideCfg, .stages = &args->window->stages);
    args->window->stage = NULL;
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &args->window->preferences_raster);
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &args->window->base);
    EMBED_CALL(gui_framebuffer_max_width.free, GuiFramebufferMaxWidthCfg, .framebuffer = &args->window->framebuffer);
}

// whether the strip's tool shown is the one named `key`
EMBED_INLINE int window_tool_by_name(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return (args->window_read->strip.displayed >= 0) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_tools[args->window_read->strip.displayed].name, .other = args->key, .cap = 32ULL});
}

// the rows the editor shows, and the columns
EMBED_INLINE unsigned int window_rows(const GuiWindowCtx *args)
{
    const int line_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE,
                                       .scale = args->window_read->frame.scale);
    const int rows = (line_height > 0) ? args->window_read->frame.editor.height / line_height : 0;
    // rows are counted where there is capacity for them
    return (rows > 0) ? (unsigned int)rows : 0u;
}

EMBED_INLINE unsigned int window_columns(const GuiWindowCtx *args)
{
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = args->window->frame.scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs, .size = size);
    const int gutter =
        EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg, .editor_read = &args->window->editor,
                   .cell = cell, .scale = args->window->frame.scale);
    const float pixel_width = (float)(args->window->frame.editor.width - gutter);
    // columns are counted where there is capacity for them
    return ((cell > 0.0f) && (pixel_width > 0.0f)) ? (unsigned int)(pixel_width / cell) : 0u;
}

// the lines the editor shows, with the fraction of the last
EMBED_INLINE float window_on_screen(const GuiWindowCtx *args)
{
    const int line_height = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE,
                                       .scale = args->window_read->frame.scale);
    return (line_height > 0) ? (float)args->window_read->frame.editor.height / (float)line_height : 0.0f;
}

// the controller told the columns and rows of each of the terminal's screens shown, where they moved; a screen no
// longer shown said again as it shows
EMBED_INLINE void window_terminal_resize(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    for (unsigned int view = 0u; view < 2u; view += 1u)
    {
        unsigned int columns = 0u;
        unsigned int rows = 0u;
        const int shown =
            EMBED_CALL(gui_terminal.size, GuiTerminalCfg, .terminal_read = &window->terminal, .frame = &window->frame,
                       .glyphs = &window->glyphs, .view = view, .columns = &columns, .rows = &rows);
        if (shown && ((columns != window->terminal_columns_sent[view]) || (rows != window->terminal_rows_sent[view])))
        {
            const CliInput sized = {.kind = CLI_INPUT_TERMINAL_SIZE, .index = view, .rows = rows, .columns = columns};
            cli_controller_input(&sized, NULL);
        }
        window->terminal_columns_sent[view] = columns;
        window->terminal_rows_sent[view] = rows;
    }
}

// the controller told the editor's size and how many titles the bar shows, where either moved, and the terminal's
// screens' sizes
EMBED_INLINE void window_view_resize(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    EMBED_CALL(window_terminal_resize, GuiWindowCtx, .window = window);
    const unsigned int rows = EMBED_CALL(window_rows, GuiWindowCtx, .window_read = window);
    const unsigned int columns = window_columns(args);
    const unsigned int titles = (window->menubar.menus != NULL) ? window->menubar.menus->titles : 0u;
    const unsigned int shown = (window->menubar.folded_from < titles) ? window->menubar.folded_from : titles;
    const unsigned int folded = window->menubar.folded_from < titles;
    if ((rows == window->sent_rows) && (columns == window->sent_columns) && (shown == window->sent_shown) &&
        (folded == window->sent_folded))
    {
        return;
    }
    window->sent_rows = rows;
    window->sent_columns = columns;
    window->sent_shown = shown;
    window->sent_folded = folded;
    const CliInput sized = {.kind = CLI_INPUT_SIZE, .rows = rows, .columns = columns, .index = shown, .level = folded};
    cli_controller_input(&sized, NULL);
}

// the lines the map placed beside the view stands for told to the controller, where they moved
EMBED_INLINE void window_map_span(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if ((window->editor.frame == NULL) ||
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = window->frame.map))
    {
        return;
    }
    EMBED_CALL(gui_minimap_position.place, GuiMinimapPositionCfg, .map = &window->minimap, .place = window->frame.map,
               .scale = window->frame.scale, .editor = &window->editor,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = window));
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    if (!EMBED_CALL(gui_minimap_first.span, GuiMinimapFirstCfg, .map_read = &window->minimap, .first = &first,
                    .last = &last) ||
        (window->map_sent && (window->map_sent_first == first) && (window->map_sent_last == last)))
    {
        return;
    }
    window->map_sent = 1;
    window->map_sent_first = first;
    window->map_sent_last = last;
    const CliInput span = {.kind = CLI_INPUT_MAP_SPAN, .row = first, .column = last};
    cli_controller_input(&span, NULL);
}

void gui_window_autohide_layout(const GuiWindowAutohideCfg *args)
{
    args->window->pending = 1;
    if (EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &args->window->claim,
                   .time_ms = args->time_ms))
    {
        if ((args->window->stages.stage == NULL) || (args->window->stages.created_for != args->window->claim.sent) ||
            args->window->stages_stale)
        {
            if (args->window->stage != NULL)
            {
                EMBED_CALL(gui_stages_table.stage_autohide_alloc, GuiStagesCfg, .stage = args->window->stage,
                           .claim = &args->window->claim);
            }
            args->window->stage = NULL;
            args->window->stages_stale = 0;
            (void)EMBED_CALL(gui_stages_autohide.init, GuiStagesAutohideCfg, .stages = &args->window->stages,
                             .claim = &args->window->claim, .frame = &args->window->frame, .base = &args->window->base,
                             .palette = args->window->palette, .menubar = &args->window->menubar,
                             .glyphs = &args->window->glyphs, .time_ms = args->time_ms);
        }
        const GuiStage *const stage = EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg,
                                                 .stages_read = &args->window->stages, .time_ms = args->time_ms);
        if (stage != NULL)
        {
            args->window->stage = stage;
            args->window->frame = stage->frame;
            EMBED_CALL(gui_menubar_frame.titles_alloc, GuiMenubarFrameCfg, .menubar = &args->window->menubar,
                       .glyphs = &args->window->glyphs, .title = stage->title, .count = stage->titles,
                       .folded_from = stage->folded_from, .frame = &args->window->frame);
            return;
        }
    }
    // at rest, or the stages' cell not taken: the frame the numbers lay now, and no stage left standing
    args->window->stage = NULL;
    args->window->stages_stale = 0;
    EMBED_CALL(gui_stages_autohide.free, GuiStagesAutohideCfg, .stages = &args->window->stages);
    GuiAutohideNumbers numbers;
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &args->window->claim, .time_ms = args->time_ms,
               .numbers = &numbers);
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &args->window->frame, .numbers = &numbers);
    EMBED_CALL(gui_menubar_bar.titles_layout, GuiMenubarBarCfg, .menubar = &args->window->menubar,
               .glyphs = &args->window->glyphs, .frame = &args->window->frame);
}

EMBED_INLINE void window_autohide_layout(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    EMBED_CALL(gui_window_autohide.autohide_layout, GuiWindowAutohideCfg, .window = window,
               .time_ms = cli_clock_milliseconds());
}

// the claim made, as the editor takes the keys, or let go, as they leave it
EMBED_INLINE void window_autohide(const GuiWindowCtx *args)
{
    int on = args->on;
    if (on == args->window->claim.autohidden)
    {
        return;
    }
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &args->window->claim, .on = on);
    args->window->autohide_moving = 1;
    window_autohide_layout(args);
}

// whether the pointer at x, y stands over the part the claim shows
EMBED_INLINE int window_over_parts(const GuiWindowCtx *args)
{
    int x = args->x;
    int y = args->y;
    const GuiFrame *const frame = &args->window_read->frame;
    const unsigned int displayed = args->window_read->claim.displayed;
    if (displayed == GUI_AUTOHIDE_TOP)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->bar_band,
                                             .other = frame->bar),
                          .x = x, .y = y) ||
               EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->tabs, .x = x, .y = y) ||
               EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->controls_pane, .x = x, .y = y);
    }
    if (displayed == GUI_AUTOHIDE_BOTTOM)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->status_band,
                                             .other = frame->status),
                          .x = x, .y = y);
    }
    if (displayed == GUI_AUTOHIDE_LEFT)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_band,
                                             .other = frame->strip),
                          .x = x, .y = y);
    }
    if (displayed == GUI_AUTOHIDE_RIGHT)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->map, .x = x, .y = y);
    }
    if (displayed == GUI_AUTOHIDE_CONTROLS)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->controls_pane, .x = x, .y = y);
    }
    return 0;
}

// whether the strip's tool shown has the side pane as its window
EMBED_INLINE int window_side_show(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return (args->window_read->strip.displayed >= 0) && g_cli_tools[args->window_read->strip.displayed].side;
}

// the work's inside laid again with the terminal's panel at its foot as the panel stands, the controller told the
// editor's and the screens' sizes
EMBED_INLINE void window_terminal(const GuiWindowCtx *args)
{
    const CliTerminalPanel *const panel = args->window->terminal.panel;
    // a panel's height in the page's pixels is far inside what an int represents
    const int height = ((panel != NULL) && panel->open) ? (int)panel->height : 0;
    args->window->strip.terminal_open = height != 0;
    for (unsigned int at = 0u; at < g_cli_tools_count; at += 1u)
    {
        args->window->strip.terminal_at =
            cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = "terminal", .cap = 16ULL})
                ? at
                : args->window->strip.terminal_at;
    }
    if (height != args->window->frame.terminal_height)
    {
        EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &args->window->frame, .height = height);
        if (args->window->stage != NULL)
        {
            args->window->stages_stale = 1;
            window_autohide_layout(args);
        }
    }
    args->window->pending = 1;
    window_view_resize(args);
}

// the work's inside laid again as the strip's tool shown has a side or not, the controller told the editor's size
EMBED_INLINE void window_side(const GuiWindowCtx *args)
{
    const int side = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &args->window->frame, .displayed = side);
    args->window->pending = 1;
    // a movement under way runs on with the side as it now stands
    if (args->window->stage != NULL)
    {
        args->window->stages_stale = 1;
        window_autohide_layout(args);
    }
    window_view_resize(args);
}

// the menus open closed, as menu.js closes them on a press or the wheel away from them, the window resized or its keys
// gone to another window
EMBED_INLINE void window_menus_close(const GuiWindowCtx *args)
{
    const CliMenus *const menus = args->window_read->menubar.menus;
    if ((menus != NULL) && (menus->levels != 0u))
    {
        const CliInput closing = {.kind = CLI_INPUT_MENU_AWAY};
        cli_controller_input(&closing, NULL);
    }
}

int gui_window_resize(const GuiWindowWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    if ((width != args->window->frame.width) || (height != args->window->frame.height))
    {
        EMBED_CALL(window_menus_close, GuiWindowCtx, .window_read = args->window);
    }
    int system_scale = args->system_scale;
    // the frame is laid at the zoom's share of the scale the system gives
    args->window->system_scale = system_scale;
    // a percent of a percent is far inside what an int represents
    const int scale =
        (system_scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                        .step = args->window->preference_values.zoom_step)) /
        GUI_SCALE_ONE;
    // every take the size needs is made first, with nothing of the window changed: the rasters at the first size with
    // capacity for the largest monitor, and the base's tile tables where it has none yet; none of them from the shared
    // cells at any size after the first. A window past the rasters' capacity, or a take refused, saves the size it has,
    // whole
    if ((args->window->framebuffer.pages == NULL) &&
        !EMBED_CALL(gui_framebuffer_max_width.init, GuiFramebufferMaxWidthCfg,
                    .framebuffer = &args->window->framebuffer, .width = width, .height = height,
                    .max_width = args->window->display_width, .max_height = args->window->display_height))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_framebuffer.fits, GuiFramebufferCfg, .framebuffer_read = &args->window->framebuffer,
                    .width = width, .height = height))
    {
        return 0;
    }
    if ((args->window->base.tile_red_blue == NULL) &&
        !EMBED_CALL(gui_base_width.init, GuiBaseWidthCfg, .base = &args->window->base,
                    .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width), .height = height))
    {
        return 0;
    }
    // then the size taken in one step: the rasters, the base's map and the ground at the size, and the frame laid for
    // it
    (void)EMBED_CALL(gui_framebuffer_max_width.fit, GuiFramebufferMaxWidthCfg,
                     .framebuffer = &args->window->framebuffer, .width = width, .height = height);
    args->window->displayed = NULL;
    EMBED_CALL(gui_base_width.resize, GuiBaseWidthCfg, .base = &args->window->base,
               .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width), .height = height);
    const int laid = EMBED_CALL(gui_background_raster.layout, GuiBackgroundRasterCfg,
                                .background_fill = &args->window->background_fill,
                                .raster = &args->window->framebuffer.raster[GUI_FRAMEBUFFER_BACKGROUND], .width = width,
                                .height = height, .scale = scale, .palette = args->window->palette);
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &args->window->frame, .width = width,
               .height = height, .scale = scale);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &args->window->frame,
               .displayed = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window));
    args->window->stages_stale = 1;
    EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = args->window);
    EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
    return laid;
}

int gui_window_icon(const GuiWindowIconCfg *args)
{
    unsigned int size = args->size;
    // the eye's line at the size, no thinner than a pixel
    const float stroke = GUI_LOGO_STROKE * (float)size / GUI_LOGO_SIDE;
    const GuiMask *const logo = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &args->window->icons, .name = "eye",
                                           .size = size, .stroke = (stroke < 1.0f) ? 1.0f : stroke);
    if ((logo == NULL) || (logo->cover == NULL) ||
        !EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = args->icon, .width = size, .height = size))
    {
        return 0;
    }
    EMBED_CALL(gui_raster_xy_color.stencil, GuiRasterXYColorCfg, .raster = args->icon, .mask_bits = logo,
               .color = args->window->palette->text);
    return 1;
}

// less motion as the system's wish and the saved setting now ask: the claim's slides standing where they are bound, the
// name home with no flight and the lattice saved still, and a frame owed
EMBED_INLINE void window_motion_layout(const GuiWindowCtx *args)
{
    args->window->motion_still = args->window->motion_system || args->window->motion_saved;
    EMBED_CALL(cli_autohide_still.still, GuiAutohideStillCfg, .claim = &args->window->claim,
               .still = args->window->motion_still);
    EMBED_CALL(cli_wordmark_still.still, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark,
               .still = args->window->motion_still);
    EMBED_CALL(cli_busy_still.still, GuiBusyStillCfg, .busy = &args->window->busy, .still = args->window->motion_still);
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->background_fill,
               .paused = args->window->motion_still || args->window->off_screen);
    args->window->pending = 1;
    if (args->window->stage != NULL)
    {
        window_autohide_layout(args);
    }
}

void gui_window_motion_tell(const GuiWindowReducedCfg *args)
{
    args->window->motion_system = args->reduced;
    EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = args->window);
    // the controller marks Reduce Motion by the system's wish as well as the saved setting
    const CliInput told = {.kind = CLI_INPUT_MOTION_SYSTEM, .index = args->reduced ? 1u : 0u};
    cli_controller_input(&told, NULL);
}

void gui_window_show(const GuiWindowDisplayedCfg *args)
{
    args->window->off_screen = !args->displayed;
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->background_fill,
               .paused = !args->displayed || args->window->motion_still);
    args->window->pending = args->displayed || args->window->pending;
}

// the milliseconds each rest of the caret's breath takes, at full ink and at dim ink alike
#define GUI_CARET_STEADY_MS ((GUI_CARET_BLINK_PERIOD_MS - (2ULL * GUI_CARET_FADE_MS)) / 2ULL)

// the ink of a slide `through` its milliseconds, from `from` to `to`, eased on a cosine: still at both ends, moving
// fastest in the middle
EMBED_INLINE unsigned int window_caret_slid(const GuiWindowCtx *args)
{
    // the share of the slide passed, 0 to 1, is a double's
    const double eased =
        (1.0 - cos(3.14159265358979323846 * ((double)args->through / (double)GUI_CARET_FADE_MS))) / 2.0;
    // an ink between two bytes, rounded, is a byte
    return (unsigned int)((double)args->from + (((double)args->to - (double)args->from) * eased) + 0.5);
}

// the caret's breath, its ink at each stage of it, made once: full through the first rest, sliding down, dim through
// the second rest, and sliding up again
#define GUI_CARET_BLINK_STEP_MS 16ULL
static unsigned char s_caret_color[(GUI_CARET_BLINK_PERIOD_MS / GUI_CARET_BLINK_STEP_MS) + 1ULL];
static int s_caret_color_created;

static void window_caret_init(void)
{
    for (unsigned long long stage = 0ULL; stage <= GUI_CARET_BLINK_PERIOD_MS / GUI_CARET_BLINK_STEP_MS; stage += 1ULL)
    {
        const unsigned long long at = stage * GUI_CARET_BLINK_STEP_MS;
        unsigned int text_color = 255u;
        if ((at >= GUI_CARET_STEADY_MS) && (at < GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS))
        {
            text_color = EMBED_CALL(window_caret_slid, GuiWindowCtx, .through = at - GUI_CARET_STEADY_MS, .from = 255u,
                                    .to = GUI_CARET_DIM);
        }
        else if ((at >= GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS) &&
                 (at < (2ULL * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS))
        {
            text_color = GUI_CARET_DIM;
        }
        else if (at >= (2ULL * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS)
        {
            text_color = EMBED_CALL(window_caret_slid, GuiWindowCtx,
                                    .through = at - (2ULL * GUI_CARET_STEADY_MS) - GUI_CARET_FADE_MS,
                                    .from = GUI_CARET_DIM, .to = 255u);
        }
        // an ink is a byte
        s_caret_color[stage] = (unsigned char)text_color;
    }
    s_caret_color_created = 1;
}

// the caret's ink, 0 to 255, where its breath stands since it began; full while no cursor stands or less
// motion is asked for
EMBED_INLINE unsigned int window_caret_color(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (!(args->window_read->editor.frame != NULL) || args->window_read->motion_still)
    {
        return 255u;
    }
    if (!s_caret_color_created)
    {
        window_caret_init();
    }
    return s_caret_color[((now_ms - args->window_read->caret_blink_start_ms) % GUI_CARET_BLINK_PERIOD_MS) /
                         GUI_CARET_BLINK_STEP_MS];
}

// the deadline the caret's ink next changes at: the steady time's end while it is steady, else the next millisecond,
// which the system holds to the display's refresh; 0 where no cursor stands
EMBED_INLINE unsigned long long window_caret_deadline_ms(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (!(args->window_read->editor.frame != NULL) || args->window_read->motion_still)
    {
        return 0ULL;
    }
    const unsigned long long phase_ms = (now_ms - args->window_read->caret_blink_start_ms) % GUI_CARET_BLINK_PERIOD_MS;
    if (phase_ms < GUI_CARET_STEADY_MS)
    {
        return now_ms + GUI_CARET_STEADY_MS - phase_ms;
    }
    const unsigned long long dim_end_ms = (2ULL * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS;
    if ((phase_ms >= GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS) && (phase_ms < dim_end_ms))
    {
        return now_ms + dim_end_ms - phase_ms;
    }
    return now_ms + 1ULL;
}

void gui_window_tick(const GuiWindowCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    (void)EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &args->window->claim);
    const int moving =
        EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &args->window->claim, .time_ms = now_ms);
    // the frame laid for the claim's numbers while a slide moves and as it comes to rest; a tick with the claim at
    // rest owes nothing for it
    if (moving || args->window->autohide_moving)
    {
        EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = args->window);
    }
    if (args->window->autohide_moving && !moving)
    {
        // the slides at rest, the controller is told the editor's size and the map its span
        EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
        EMBED_CALL(window_map_span, GuiWindowCtx, .window = args->window);
    }
    args->window->autohide_moving = moving;
    args->window->pending =
        EMBED_CALL(gui_wordmark_frame.tick, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark,
                   .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette,
                   .frame = &args->window->frame) ||
        args->window->pending;
    // the ground where its loop shows a change
    args->window->pending =
        EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg,
                   .background_fill = &args->window->background_fill, .palette = args->window->palette) ||
        args->window->pending;
    // the busy circle coming or going owes a frame whole
    args->window->pending =
        EMBED_CALL(gui_busy_running.tick, GuiBusyRunningCfg, .busy = &args->window->busy) || args->window->pending;
    // the caret where its ink has changed and the circle where its stage has: where nothing else owes a frame, each
    // drawn again pays it alone, and a frame owed after this tick is still paid whole; no caret stands while the view
    // is not drawn, and a caret under the circle owes the frame whole, neither knowing what the other has laid
    const int caret_changed =
        !args->window->wordmark.exists &&
        (EMBED_CALL(window_caret_color, GuiWindowCtx, .window_read = args->window) != args->window->caret_color);
    const int busy_stepped =
        args->window->busy.under_saved && !args->window->off_screen &&
        (EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .busy_read = &args->window->busy) != args->window->busy.stage_drawn);
    const int crossed =
        args->window->busy.under_saved && args->window->editor.caret_saved &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window->busy.under_rect, .other = args->window->editor.caret_rect));
    const int alone = !args->window->pending && !moving && !crossed;
    args->window->caret_alone = caret_changed && alone;
    args->window->busy_alone = busy_stepped && alone;
    args->window->pending = args->window->pending || ((caret_changed || busy_stepped) && !alone);
}

// whether a panel or a menu lies over `caret`, which saves the caret from being drawn over them
EMBED_INLINE int window_caret_cover(const GuiWindowCtx *args)
{
    if ((args->window_read->hover.panel != NULL) &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->hover.rect, .other = args->caret)))
    {
        return 1;
    }
    if ((args->window_read->suggest.panel != NULL) &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->suggest.rect, .other = args->caret)))
    {
        return 1;
    }
    if ((args->window_read->signature.panel != NULL) &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->signature.rect, .other = args->caret)))
    {
        return 1;
    }
    const unsigned int levels =
        (args->window_read->menubar.menus != NULL) ? args->window_read->menubar.menus->levels : 0u;
    for (unsigned int level = 0u; (level < levels) && (level < GUI_MENU_LEVELS); level += 1u)
    {
        if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                        .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                           .one = args->window_read->menubar.menu_rect[level], .other = args->caret)))
        {
            return 1;
        }
    }
    return 0;
}

void gui_window_flip(const GuiWindowCfg *args)
{
    args->window->displayed = args->window->raster;
    args->window->raster = EMBED_CALL(gui_framebuffer_max_width.flip, GuiFramebufferMaxWidthCfg,
                                      .framebuffer = &args->window->framebuffer);
}

GuiRect gui_window_caret_draw(const GuiWindowCfg *args)
{
    args->window->caret_alone = 0;
    args->window->caret_color = EMBED_CALL(window_caret_color, GuiWindowCtx, .window_read = args->window);
    args->window->editor.caret_color = args->window->caret_color;
    return EMBED_CALL(gui_editor_raster.caret_redrawn, GuiEditorRasterCfg, .editor = &args->window->editor,
                      .raster = (args->window->displayed != NULL) ? args->window->displayed : args->window->raster,
                      .palette = args->window->palette);
}

GuiRect gui_window_busy_draw(const GuiWindowCfg *args)
{
    args->window->busy_alone = 0;
    return EMBED_CALL(gui_busy_frame.redrawn, GuiBusyFrameCfg, .busy = &args->window->busy,
                      .raster = (args->window->displayed != NULL) ? args->window->displayed : args->window->raster);
}

unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args)
{
    const GuiWindow *const window = args->window_read;
    const unsigned long long deadlines[] = {
        EMBED_CALL(gui_autohide.next_deadline_ms, GuiAutohideCfg, .autohide_read = &window->claim),
        EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg,
                   .background_fill = &window->background_fill),
        EMBED_CALL(gui_wordmark_read.next_deadline_ms, GuiWordmarkReadCfg, .wordmark_read = &window->wordmark),
        EMBED_CALL(window_caret_deadline_ms, GuiWindowCtx, .window_read = window),
        // the circle's clock stops while the window is off screen, a publication waking it as any does
        window->off_screen ? 0ULL : EMBED_CALL(gui_busy.next_deadline_ms, GuiBusyCfg, .busy_read = &window->busy),
    };
    unsigned long long earliest = 0ULL;
    for (unsigned int index = 0u; index < (sizeof(deadlines) / sizeof(deadlines[0])); index += 1u)
    {
        if ((deadlines[index] != 0ULL) && ((earliest == 0ULL) || (deadlines[index] < earliest)))
        {
            earliest = deadlines[index];
        }
    }
    if (earliest == 0ULL)
    {
        return 0ULL;
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    return (earliest > now_ms) ? earliest - now_ms : 1ULL;
}

// a setting written to the controller, `key` then a 0 then `value`
EMBED_INLINE void window_setting_send(const GuiWindowCtx *args)
{
    char text[GUI_PREFERENCES_TEXT * 3u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, args->key);
    mmgr_line_ch(&line, '\0');
    mmgr_line_put(&line, args->value);
    mmgr_line_ch(&line, '\0');
    if (mmgr_line_ok(&line))
    {
        // a key and a value are far inside what an unsigned int counts
        const CliInput setting = {.kind = CLI_INPUT_SETTING, .length = (unsigned int)line.at};
        cli_controller_input(&setting, text);
    }
}

// a percent written to the controller as a setting
EMBED_INLINE void window_percent_send(const GuiWindowCtx *args)
{
    char text[8];
    VerbaLinea value = {text, sizeof(text), 0ULL};
    mmgr_line_decimal(&value, args->percent, 1u);
    (void)mmgr_line_finish(&value);
    EMBED_CALL(window_setting_send, GuiWindowCtx, .key = args->key, .value = text);
}

// a text written to the controller as an input of `kind` with its bytes
EMBED_INLINE void window_text_send(const GuiWindowCtx *args)
{
    // a name or a typing is far inside what an unsigned int counts
    const CliInput sent = {.kind = args->input->kind, .hit = args->input->hit, .length = (unsigned int)args->length};
    cli_controller_input(&sent, args->bytes);
}

// the frame laid again at the zoom and the palette, the system's scale and the window's size standing: a color or a
// zoom reaches the ground, the frame and the view through it
EMBED_INLINE void window_preferences_layout(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = window, .width = window->frame.width,
                     .height = window->frame.height, .system_scale = window->system_scale);
    window->pending = 1;
    if (window->preferences_raster.pixels != NULL)
    {
        EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = window,
                   .width = window->preferences.width, .height = window->preferences.height,
                   .system_scale = window->preferences_system_scale);
    }
}

// a row of Settings moved: what it now stands at written to the controller, which publishes it back
EMBED_INLINE void window_preference_step(const GuiWindowCtx *args)
{
    const GuiWindow *window = args->window;
    const GuiPreferenceValues *const values = &window->preference_values;
    if (args->change->at == GUI_PREFERENCE_ZOOM)
    {
        const unsigned int step =
            EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg, .step = values->zoom_step,
                       .by = args->change->by, .round = args->change->round);
        EMBED_CALL(window_percent_send, GuiWindowCtx, .key = "zoom",
                   .percent =
                       EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg, .step = step));
    }
    else if (args->change->at == GUI_PREFERENCE_REDUCE)
    {
        // chosen, it asks for the opposite of what the window now does
        EMBED_CALL(window_setting_send, GuiWindowCtx, .key = "reduce-motion",
                   .value = window->motion_still ? "false" : "true");
    }
    else
    {
        const int bar = args->change->at == GUI_PREFERENCE_BAR;
        EMBED_CALL(window_percent_send, GuiWindowCtx, .key = bar ? "opacity.bar" : "opacity.editor",
                   .percent = EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg,
                                         .percent = bar ? values->bar_percent : values->editor_percent,
                                         .by = args->change->by, .round = args->change->round));
    }
}

// what a key or a press changed in Preferences written to the controller
EMBED_INLINE void window_preference_change(const GuiWindowCtx *args)
{
    args->window->preferences_pending = 1;
    if (args->change->kind == GUI_PREFERENCE_CLOSED)
    {
        const CliInput shut = {.kind = CLI_INPUT_COMMAND};
        EMBED_CALL(window_text_send, GuiWindowCtx, .input = (CliInput *)&shut, .bytes = "preferences",
                   .length = sizeof("preferences"));
    }
    else if (args->change->kind == GUI_PREFERENCE_STEPPED)
    {
        window_preference_step(args);
    }
    else if (args->change->kind == GUI_PREFERENCE_COLORED)
    {
        char name[GUI_PREFERENCES_TEXT];
        char key[GUI_PREFERENCES_TEXT + 16u];
        char value[GUI_PREFERENCES_TEXT];
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = args->window->palette, .at = args->change->at,
                   .name = name, .size = sizeof(name));
        VerbaLinea line = {key, sizeof(key), 0ULL};
        mmgr_line_put(&line, "color.");
        mmgr_line_put(&line, name);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = args->change->color,
                   .text = value, .size = sizeof(value));
        EMBED_CALL(window_setting_send, GuiWindowCtx, .key = key, .value = value);
    }
}

// the settings as published taken: the zoom, the opacities, less motion and the colors that differ from those standing,
// the frame laid again only for a zoom or a color that differs, laying it moving what a menu stands on
EMBED_INLINE void window_settings_take(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const CliSettings *const settings = window->settings;
    if (settings == NULL)
    {
        return;
    }
    GuiPreferenceValues *const values = &window->preference_values;
    const unsigned int zoom_before = values->zoom_step;
    (void)EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = settings->zoom_percent,
                     .step_pointer = &values->zoom_step);
    values->bar_percent = settings->bar_percent;
    values->editor_percent = settings->editor_percent;
    window->motion_saved = settings->motion_saved;
    int colored = 0;
    const unsigned int colors =
        EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = window->palette);
    for (unsigned int each = 0u; each < settings->colors; each += 1u)
    {
        for (unsigned int at = 0u; at < colors; at += 1u)
        {
            char name[GUI_PREFERENCES_TEXT];
            EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = window->palette, .at = at, .name = name,
                       .size = sizeof(name));
            if (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = settings->color[each].name, .cap = sizeof(name)}))
            {
                continue;
            }
            const char *const text = settings->color[each].value;
            const GuiColor current =
                EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = window->palette, .at = at);
            GuiColor color = 0u;
            if (EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = text,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = GUI_PREFERENCES_TEXT}),
                           .current = current, .color_pointer = &color) &&
                (color != current))
            {
                EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &window->palette_own, .at = at,
                           .color = color);
                colored = 1;
            }
            break;
        }
    }
    EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &window->palette_own,
               .bar_percent = values->bar_percent, .editor_percent = values->editor_percent);
    EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = window);
    window->preferences_pending = 1;
    if ((window->frame.width != 0u) && (colored || (values->zoom_step != zoom_before)))
    {
        window_preferences_layout(args);
    }
    window->pending = 1;
}

void gui_window_preferences_resize(const GuiWindowWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    args->window->preferences_system_scale = args->system_scale;
    const int scale =
        (args->system_scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                              .step = args->window->preference_values.zoom_step)) /
        GUI_SCALE_ONE;
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &args->window->preferences,
               .width = width, .height = height, .scale = scale);
    args->window->preferences_pending = 1;
    // the raster's pixels stand for as long as the view does, since the system shows them again as it is asked to
    // paint: made once, with capacity for the largest monitor, and a size past that saves the size it has
    if (args->window->preferences_raster.pixels == NULL)
    {
        const unsigned int most_width = (width > args->window->display_width) ? width : args->window->display_width;
        const unsigned int most_height =
            (height > args->window->display_height) ? height : args->window->display_height;
        if (!EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &args->window->preferences_raster,
                        .width = most_width, .height = most_height))
        {
            return;
        }
        args->window->preferences_capacity = (unsigned long long)most_width * most_height;
    }
    if (((unsigned long long)width * height) > args->window->preferences_capacity)
    {
        return;
    }
    args->window->preferences_raster.width = width;
    args->window->preferences_raster.height = height;
}

static void window_preferences_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    if (args->input->button != GUI_BUTTON_LEFT)
    {
        return;
    }
    GuiPreferenceChange change;
    (void)EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg,
                     .preferences = &window->preferences, .palette = window->palette, .x = args->input->x,
                     .y = args->input->y, .change = &change);
    EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
}

static void window_preferences_move(const GuiInputHandlerCfg *args)
{
    args->window->preferences_pending =
        EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg,
                   .preferences = &args->window->preferences, .palette = args->window->palette, .x = args->input->x,
                   .y = args->input->y) ||
        args->window->preferences_pending;
}

// the pointer gone from its window: nothing is under it
static void window_preferences_leave(const GuiInputHandlerCfg *args)
{
    EMBED_CALL(window_preferences_move, GuiInputHandlerCfg, .window = args->window, .input = GUI_MOVED_INPUT(-1, -1));
}

static void window_preferences_wheel(const GuiInputHandlerCfg *args)
{
    args->window->preferences_pending =
        EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &args->window->preferences,
                   .palette = args->window->palette, .rows = args->input->rows) ||
        args->window->preferences_pending;
}

static void window_preferences_key(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // the command's own keys close it from its own window as they open it from the main one
    const CliInput keyed = {.kind = CLI_INPUT_PREFERENCES_KEY};
    EMBED_CALL(window_text_send, GuiWindowCtx, .input = (CliInput *)&keyed, .bytes = args->input->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->input->name, .cap = 64ULL}) + 1ULL);
    GuiPreferenceChange change;
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &window->preferences,
               .palette = window->palette, .name = args->input->name, .change = &change);
    EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
}

static void window_preferences_text(const GuiInputHandlerCfg *args)
{
    args->window->preferences_pending =
        EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &args->window->preferences,
                   .text = args->input->text, .size = args->input->size) ||
        args->window->preferences_pending;
}

void gui_window_preferences_hits(const GuiWindowHitsCfg *args)
{
    const GuiPreferences *const preferences = &args->window->preferences;
    CliViewHits *const hits = args->hits;
    hits->width = preferences->width;
    hits->height = preferences->height;
    hits->border =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BORDER, .scale = preferences->scale);
    hits->minimize = (GuiRect){0, 0, 0, 0};
    hits->maximize = (GuiRect){0, 0, 0, 0};
    hits->close = (GuiRect){0, 0, 0, 0};
    hits->client[0] = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = preferences);
    hits->clients = 1u;
    // its head moves its window; a client's sides are far inside what an int represents
    hits->caption = (GuiRect){0, 0, (int)preferences->width,
                              EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                         .scale = preferences->scale)};
}

unsigned int gui_window_preferences_hit(const GuiWindowInputCfg *args)
{
    GuiRect client[GUI_WINDOW_HIT_CLIENTS];
    CliViewHits hits = {.client = client};
    EMBED_CALL(gui_window_table.preferences_hits, GuiWindowHitsCfg, .window = args->window_read, .hits = &hits);
    return cli_view_hit_of(&hits, args->input->x, args->input->y);
}

void gui_window_preferences_draw(const GuiWindowCfg *args)
{
    args->window->preferences_pending = 0;
    if (args->window->preferences_raster.pixels == NULL)
    {
        return;
    }
    args->window->preference_values.reduced = args->window->motion_still;
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg,
               .preferences_read = &args->window->preferences, .raster = &args->window->preferences_raster,
               .glyphs = &args->window->glyphs, .palette = args->window->palette,
               .values = &args->window->preference_values);
}

void gui_window_preferences_close(const GuiWindowCfg *args)
{
    (void)args;
    const CliInput shut = {.kind = CLI_INPUT_COMMAND};
    EMBED_CALL(window_text_send, GuiWindowCtx, .input = (CliInput *)&shut, .bytes = "preferences",
               .length = sizeof("preferences"));
}

void gui_window_start(const GuiWindowCfg *args)
{
    args->window->pending = 1;
}

// the newest of each part taken, each that changed owing a frame and moving what stands on it; nothing is released
// here, the controller reading from the ring what the view has moved past
void gui_window_drain(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    const CliViewFrame *const frame = (const CliViewFrame *)cli_controller_latest(CLI_SNAPSHOT_FRAME);
    const CliDocumentMap *const map = (const CliDocumentMap *)cli_controller_latest(CLI_SNAPSHOT_MAP);
    const CliHoverPanel *const hover = (const CliHoverPanel *)cli_controller_latest(CLI_SNAPSHOT_HOVER);
    const CliCompletionPanel *const completions =
        (const CliCompletionPanel *)cli_controller_latest(CLI_SNAPSHOT_COMPLETIONS);
    const CliSignaturePanel *const signature = (const CliSignaturePanel *)cli_controller_latest(CLI_SNAPSHOT_SIGNATURE);
    const CliMenus *const menus = (const CliMenus *)cli_controller_latest(CLI_SNAPSHOT_MENUS);
    const CliTabs *const tabs = (const CliTabs *)cli_controller_latest(CLI_SNAPSHOT_TABS);
    const CliExplorerRows *const rows = (const CliExplorerRows *)cli_controller_latest(CLI_SNAPSHOT_EXPLORER);
    const CliProblems *const problems = (const CliProblems *)cli_controller_latest(CLI_SNAPSHOT_PROBLEMS);
    const CliStatus *const status = (const CliStatus *)cli_controller_latest(CLI_SNAPSHOT_STATUS);
    const CliSettings *const settings = (const CliSettings *)cli_controller_latest(CLI_SNAPSHOT_SETTINGS);
    const CliDocumentNames *const names = (const CliDocumentNames *)cli_controller_latest(CLI_SNAPSHOT_NAMES);
    const CliTerminalPanel *const terminal = (const CliTerminalPanel *)cli_controller_latest(CLI_SNAPSHOT_TERMINAL);
    if (terminal != window->terminal.panel)
    {
        window->terminal.panel = terminal;
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
    }
    const int status_changed = status != window->status;
    if (status_changed)
    {
        window->status = status;
        window->pending = 1;
    }
    if ((status != NULL) && status_changed && (status->shown != window->shown))
    {
        // another file shown: the map stands anew beside it
        window->shown = status->shown;
        window->map_sent = 0;
        EMBED_CALL(gui_minimap.clear, GuiMinimapCfg, .map = &window->minimap);
    }
    if (frame != window->editor.frame)
    {
        window->editor.frame = frame;
        // the caret's breath begins again with every frame, which saves the caret at full ink while the keys go
        window->caret_blink_start_ms = cli_clock_milliseconds();
        window->pending = 1;
        EMBED_CALL(window_view_resize, GuiWindowCtx, .window = window);
        EMBED_CALL(window_map_span, GuiWindowCtx, .window = window);
    }
    window->pending = window->pending || (names != window->editor.names) || (map != window->minimap.lines) ||
                      (hover != window->hover.panel) || (completions != window->suggest.panel) ||
                      (signature != window->signature.panel) || (tabs != window->tabs) ||
                      (rows != window->explorer.rows) || (problems != window->problems.problems);
    window->editor.names = names;
    window->minimap.lines = map;
    window->hover.panel = hover;
    window->suggest.panel = completions;
    window->signature.panel = signature;
    window->tabs = tabs;
    window->explorer.rows = rows;
    window->problems.problems = problems;
    if (menus != window->menubar.menus)
    {
        const int first = window->menubar.menus == NULL;
        window->menubar.menus = menus;
        window->pending = 1;
        if (first)
        {
            EMBED_CALL(gui_menubar_bar.titles_layout, GuiMenubarBarCfg, .menubar = &window->menubar,
                       .glyphs = &window->glyphs, .frame = &window->frame);
            window->stages_stale = window->stage != NULL;
            EMBED_CALL(window_view_resize, GuiWindowCtx, .window = window);
        }
        else
        {
            EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &window->menubar,
                       .glyphs = &window->glyphs, .frame = &window->frame);
        }
    }
    if (settings != window->settings)
    {
        window->settings = settings;
        EMBED_CALL(window_settings_take, GuiWindowCtx, .window = window);
    }
    if ((status != NULL) && status_changed)
    {
        if (status->tool != window->strip.displayed)
        {
            window->strip.displayed = status->tool;
            EMBED_CALL(window_side, GuiWindowCtx, .window = window);
        }
        window->strip.problems_marked = status->problems_marked;
        EMBED_CALL(window_autohide, GuiWindowCtx, .window = window, .on = status->editing && status->viewed);
        EMBED_CALL(gui_busy_running.work, GuiBusyRunningCfg, .busy = &window->busy, .running = status->busy);
        if (status->preferences_displayed != window->preferences_status)
        {
            window->preferences_status = status->preferences_displayed;
            window->preferences.displayed = status->preferences_displayed;
            window->preferences.entering = 0;
            window->preferences_pending = 1;
        }
        if (status->wordmark != window->wordmark_status)
        {
            window->wordmark_status = status->wordmark;
            if (status->wordmark == CLI_WORDMARK_STAND)
            {
                EMBED_CALL(gui_wordmark_frame.stand, GuiWordmarkFrameCfg, .wordmark = &window->wordmark);
            }
            else if (status->wordmark == CLI_WORDMARK_HOME)
            {
                EMBED_CALL(cli_wordmark_still.land, GuiWordmarkStillCfg, .wordmark = &window->wordmark);
            }
        }
        if (status->tree_read && window->wordmark.exists && !window->wordmark.tree_read)
        {
            EMBED_CALL(cli_wordmark_still.tree_read, GuiWordmarkStillCfg, .wordmark = &window->wordmark);
        }
    }
}

// the panels over the editor drawn inside the work's inside: the hover under its word, nothing where the word's line
// has left the rows; the completion under the prefix's first cell; and the signature over the cursor's row
EMBED_INLINE void window_panels_draw(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = frame->scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs, .size = size);
    GuiRect anchor;
    const CliHoverPanel *const hover = args->window->hover.panel;
    args->window->hover.rect = (GuiRect){0, 0, 0, 0};
    if ((hover != NULL) && EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg,
                                      .editor_read = &args->window->editor, .place = frame->editor, .cell = cell,
                                      .scale = frame->scale, .line = hover->line, .col = hover->from, .rect = &anchor))
    {
        EMBED_CALL(gui_hover_raster.draw, GuiHoverRasterCfg, .hover = &args->window->hover,
                   .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette,
                   .anchor = anchor, .bounds = frame->inside, .scale = frame->scale);
    }
    const CliCompletionPanel *const completions = args->window->suggest.panel;
    args->window->suggest.rect = (GuiRect){0, 0, 0, 0};
    args->window->suggest.list = (GuiRect){0, 0, 0, 0};
    if ((completions != NULL) &&
        EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg,
                   .editor_read = &args->window->editor, .place = frame->editor, .cell = cell, .scale = frame->scale,
                   .line = completions->line, .col = completions->from, .rect = &anchor))
    {
        EMBED_CALL(gui_suggest_raster.draw, GuiSuggestRasterCfg, .suggest = &args->window->suggest,
                   .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette,
                   .anchor = anchor, .bounds = frame->inside, .scale = frame->scale);
    }
    args->window->signature.rect = (GuiRect){0, 0, 0, 0};
    if ((args->window->signature.panel != NULL) && (args->window->editor.frame != NULL) &&
        EMBED_CALL(gui_editor_position_cell.position_rect, GuiEditorPositionCellCfg,
                   .editor_read = &args->window->editor, .place = frame->editor, .cell = cell, .scale = frame->scale,
                   .line = args->window->editor.frame->cursor.line, .col = args->window->editor.frame->cursor.col,
                   .rect = &anchor))
    {
        EMBED_CALL(gui_signature_raster.draw, GuiSignatureRasterCfg, .signature = &args->window->signature,
                   .raster = args->window->raster, .glyphs = &args->window->glyphs, .palette = args->window->palette,
                   .anchor = anchor, .bounds = frame->inside, .scale = frame->scale);
    }
}

// the view drawn: the tabs row, the editor with the map beside it, and the side where one is shown, each on the ground
// the base laid it, and the panels over them
EMBED_INLINE void window_view_draw(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    const CliTabs *const tabs = args->window->tabs;
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame, .tab = (tabs != NULL) ? tabs->row : NULL,
               .count = (tabs != NULL) ? tabs->count : 0u, .active = (tabs != NULL) ? tabs->active : -1);
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &args->window->editor,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .place = frame->editor,
               .scale = frame->scale);
    // the shown file's diagnostics underlined on the rows drawn
    const unsigned int code_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = GUI_CODE_SIZE, .scale = frame->scale);
    EMBED_CALL(gui_problems_path_raster.underlined, GuiProblemsPathRasterCfg, .problems = &args->window->problems,
               .editor = &args->window->editor, .raster = raster, .palette = palette, .place = frame->editor,
               .cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs,
                                  .size = code_size),
               .scale = frame->scale);
    EMBED_CALL(gui_minimap_position.draw, GuiMinimapPositionCfg, .map = &args->window->minimap, .raster = raster,
               .palette = palette, .editor = &args->window->editor, .place = frame->map, .scale = frame->scale,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = args->window));
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->side) &&
        (args->window->strip.displayed >= 0))
    {
        // the tool shown is among the tools
        const CliTool *const tool = &g_cli_tools[args->window->strip.displayed];
        const CliStatus *const standing = args->window->status;
        EMBED_CALL(gui_explorer_frame_xy.draw, GuiExplorerFrameXYCfg, .explorer_read = &args->window->explorer,
                   .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette,
                   .frame = frame, .title = tool->label,
                   .folders = cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = "explorer", .cap = 32ULL}),
                   .tree = ((standing != NULL) && (standing->title != NULL)) ? standing->title : "");
        if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = args->window, .key = "problems"))
        {
            EMBED_CALL(gui_problems_row_frame.draw, GuiProblemsRowFrameCfg, .problems = &args->window->problems,
                       .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
        }
    }
    EMBED_CALL(gui_terminal.draw, GuiTerminalCfg, .terminal_read = &args->window->terminal, .frame = frame,
               .glyphs = &args->window->glyphs, .raster = raster, .palette = palette);
    window_panels_draw(args);
}

void gui_window_draw(const GuiWindowCfg *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    args->window->pending = 0;
    args->window->caret_alone = 0;
    args->window->busy_alone = 0;
    if (raster->pixels == NULL)
    {
        return;
    }
    // the base: the lattice as it stands, where its raster is the window's size, else the deep color, with the
    // glass sheet about the work, the work's parts' grounds and the panes that have slid in laid over it in one
    // pass through the map; the controls' pane is the base's once it stands; while the name's pane stands the view is
    // not drawn and the lattice shows through the work
    const GuiRaster *background_fill = &args->window->background_fill.raster;
    if ((background_fill->pixels == NULL) || (background_fill->width != raster->width) ||
        (background_fill->height != raster->height))
    {
        background_fill = NULL;
    }
    const int view_displayed = !args->window->wordmark.exists;
    // a stage of the claim's movement brings its map and its holes; only a frame at rest lays them
    const GuiStage *const stage = view_displayed ? args->window->stage : NULL;
    if (stage != NULL)
    {
        EMBED_CALL(gui_base_frame.paint, GuiBaseFrameCfg, .base = &args->window->base, .palette = palette);
        EMBED_CALL(gui_base_raster.map_draw, GuiBaseRasterCfg, .base_read = &args->window->base, .map = &stage->map,
                   .raster = raster, .background_fill = background_fill, .color = palette->deep);
    }
    else
    {
        const GuiFrame *based = frame;
        GuiFrame bare;
        if (!view_displayed)
        {
            bare = *frame;
            bare.side = (GuiRect){0, 0, 0, 0};
            bare.tabs = (GuiRect){0, 0, 0, 0};
            bare.editor = (GuiRect){0, 0, 0, 0};
            bare.map = (GuiRect){0, 0, 0, 0};
            based = &bare;
        }
        (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &args->window->base, .frame = based,
                         .palette = palette, .controls_steady = frame->controls_pane_share >= GUI_FIXED_ONE);
        EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &args->window->base, .raster = raster,
                   .background_fill = background_fill, .color = palette->deep);
    }
    // the table the panes lie on: the lattice with the sheet's tint and grain; each pane's corners rounded on it
    const GuiUnder table = EMBED_CALL(gui_glass.table, GuiGlassCfg, .background_fill = background_fill,
                                      .color = palette->deep, .glass = palette->glass);
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = palette->glass);
    EMBED_CALL(
        gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
        .background_fill = table,
        .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->bar_band, .other = frame->bar),
        .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = table,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_band,
                                  .other = frame->strip),
               .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_background.pane, GuiRasterRectRadiusBackgroundCfg, .raster = raster,
               .background_fill = table,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->status_band,
                                  .other = frame->status),
               .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    // the work's outline: the lines it stands in, as the page draws them, the signal along its head, a thin line of
    // it down its left, the link darkened down its right and along its foot, bent round its corners and round the
    // holes the parts shown over the work make in it while the claim holds
    const GuiColor right_line =
        GUI_RGB((GUI_RED_OF(palette->link) * 65u) / 100u, (GUI_GREEN_OF(palette->link) * 65u) / 100u,
                (GUI_BLUE_OF(palette->link) * 65u) / 100u);
    const GuiOutline lines = {
        palette->signal,
        right_line,
        right_line,
        (palette->signal & 0x00ffffffu) | (0x59u << 24u),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_TOP, .scale = frame->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_RIGHT, .scale = frame->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_BOTTOM, .scale = frame->scale),
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_WORK_PAD_LEFT, .scale = frame->scale)};
    const GuiUnder clear = {background_fill, 0u, NULL, 0u, palette->deep};
    GuiRect holes_laid[4];
    const unsigned int count =
        (stage != NULL) ? stage->hole_count
                        : EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = frame, .holes = holes_laid);
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = raster, .work = frame->work,
               .inside = frame->inside, .radius = frame->radius, .lines = &lines,
               .holes = (stage != NULL) ? stage->holes : holes_laid, .count = count, .table = table, .clear = clear);
    // the work's inside: the tabs row, the editor with the map beside it, and the side where one is shown, each on
    // the ground the base laid it
    if (view_displayed)
    {
        EMBED_CALL(window_view_draw, GuiWindowCtx, .window = args->window);
    }
    // the parts about the work, each on its pane: the strip, the bar with the menu bar's titles and the controls,
    // and the status bar
    EMBED_CALL(gui_strip_frame_raster.draw, GuiStripFrameRasterCfg, .raster = raster, .icons = &args->window->icons,
               .palette = palette, .frame = frame, .strip_read = &args->window->strip);
    EMBED_CALL(gui_chrome_raster.bar_draw, GuiChromeRasterCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame,
               .title = ((args->window->status != NULL) && (args->window->status->title != NULL))
                            ? args->window->status->title
                            : "",
               .table = table, .shadow = &args->window->controls_shadow);
    EMBED_CALL(gui_menubar_glyphs_frame.titles_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &args->window->menubar,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
    const CliViewFrame *const shown = args->window->editor.frame;
    const CliStatus *const standing = args->window->status;
    const CliPosition none = {0ULL, 0ULL};
    const CliPosition cursor = (shown != NULL) ? shown->cursor : none;
    const CliPosition anchor = (shown != NULL) ? shown->anchor : none;
    const GuiStatus status = {(standing != NULL) ? standing->path : "",
                              shown != NULL,
                              cursor.line,
                              cursor.col,
                              anchor.line,
                              anchor.col,
                              (standing != NULL) && standing->crlf,
                              (shown != NULL) ? shown->indent_tabs : -1,
                              (shown != NULL) ? shown->indent_size : 4u,
                              (standing != NULL) ? standing->memory_bytes : 0ULL};
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .icons = &args->window->icons, .palette = palette, .frame = frame, .status = &status);
    // the large name as it stands or goes, the funnel over the whole window, and the eye over the bar and the strip
    EMBED_CALL(gui_wordmark_frame.draw, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark, .raster = raster,
               .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette, .frame = frame);
    EMBED_CALL(gui_wordmark_frame.logo_draw, GuiWordmarkFrameCfg, .wordmark_read = &args->window->wordmark,
               .raster = raster, .icons = &args->window->icons, .palette = palette, .frame = frame);
    // the menus open go over every part
    EMBED_CALL(gui_menubar_glyphs_frame.menus_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &args->window->menubar,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
    // the caret last, at the ink its breath stands at, where no panel or menu lies over it: drawn over the frame
    // whole, its breath's steps draw it again alone; none stands while the view is not drawn
    args->window->caret_color = EMBED_CALL(window_caret_color, GuiWindowCtx, .window_read = args->window);
    args->window->editor.caret_color = args->window->caret_color;
    const GuiRect caret =
        EMBED_CALL(gui_editor_position.caret_rect, GuiEditorPositionCfg, .editor_read = &args->window->editor,
                   .glyphs = &args->window->glyphs, .place = frame->editor, .scale = frame->scale);
    if (view_displayed && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = caret) &&
        !EMBED_CALL(window_caret_cover, GuiWindowCtx, .window_read = args->window, .caret = caret))
    {
        (void)EMBED_CALL(gui_editor_raster.caret_draw, GuiEditorRasterCfg, .editor = &args->window->editor,
                         .raster = raster, .palette = palette, .caret = caret);
    }
    else
    {
        args->window->editor.caret_saved = 0;
    }
    // the busy circle over all of it, the pixels under it saved for its stages' steps
    EMBED_CALL(gui_busy_frame.draw, GuiBusyFrameCfg, .busy = &args->window->busy, .raster = raster, .palette = palette,
               .frame = frame);
}

void gui_window_hits(const GuiWindowHitsCfg *args)
{
    const GuiWindow *const window = args->window;
    const GuiFrame *const frame = &window->frame;
    CliViewHits *const hits = args->hits;
    hits->width = frame->width;
    hits->height = frame->height;
    hits->border = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BORDER, .scale = frame->scale);
    hits->minimize = EMBED_CALL(gui_chrome.control_rect, GuiChromeCfg, .frame = frame, .which = GUI_CONTROL_MINIMIZE);
    hits->maximize = EMBED_CALL(gui_chrome.control_rect, GuiChromeCfg, .frame = frame, .which = GUI_CONTROL_MAXIMIZE);
    hits->close = EMBED_CALL(gui_chrome.control_rect, GuiChromeCfg, .frame = frame, .which = GUI_CONTROL_CLOSE);
    // the menu bar's titles and its open menus are the client's, not the caption's, and the eye in the bar is too
    unsigned int clients = 0u;
    for (unsigned int at = 0u; at < window->menubar.titles; at += 1u)
    {
        hits->client[clients] = window->menubar.title[at].rect;
        clients += 1u;
    }
    const unsigned int levels = (window->menubar.menus != NULL) ? window->menubar.menus->levels : 0u;
    for (unsigned int level = 0u; (level < levels) && (level < GUI_MENU_LEVELS); level += 1u)
    {
        hits->client[clients] = window->menubar.menu_rect[level];
        clients += 1u;
    }
    if (window->wordmark.home)
    {
        hits->client[clients] = EMBED_CALL(gui_wordmark_frame.logo_square, GuiWordmarkFrameCfg, .frame = frame);
        clients += 1u;
    }
    hits->clients = clients;
    hits->caption = frame->bar;
}

unsigned int gui_window_hit(const GuiWindowInputCfg *args)
{
    GuiRect client[GUI_WINDOW_HIT_CLIENTS];
    CliViewHits hits = {.client = client};
    EMBED_CALL(gui_window_table.hits, GuiWindowHitsCfg, .window = args->window_read, .hits = &hits);
    return cli_view_hit_of(&hits, args->input->x, args->input->y);
}

// what the pixel at x, y stands on written into `input`: its hit, and its place among the rows, or its row and cell in
// the editor
EMBED_INLINE void window_hit_read(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    CliInput *const input = args->input;
    const int x = args->x;
    const int y = args->y;
    input->hit = CLI_HIT_NONE;
    if (EMBED_CALL(gui_suggest_x.row_at, GuiSuggestXCfg, .suggest_read = &window->suggest, .x = x, .y = y,
                   .at = &input->index))
    {
        input->hit = CLI_HIT_SUGGEST_ROW;
        return;
    }
    if (EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &window->suggest, .x = x, .y = y))
    {
        input->hit = CLI_HIT_SUGGEST_LIST;
        return;
    }
    if (EMBED_CALL(gui_hover_x.holds, GuiHoverXCfg, .hover_read = &window->hover, .x = x, .y = y))
    {
        input->hit = CLI_HIT_HOVER_BOX;
        return;
    }
    if (EMBED_CALL(gui_signature_x.holds, GuiSignatureXCfg, .signature_read = &window->signature, .x = x, .y = y))
    {
        input->hit = CLI_HIT_SIGNATURE_BOX;
        return;
    }
    input->hit =
        EMBED_CALL(gui_menubar_x.hit, GuiMenubarXCfg, .menubar_read = &window->menubar, .scale = window->frame.scale,
                   .x = x, .y = y, .level = &input->level, .index = &input->index);
    if (input->hit != CLI_HIT_NONE)
    {
        return;
    }
    if (EMBED_CALL(gui_wordmark_frame.holds, GuiWordmarkFrameCfg, .wordmark_read = &window->wordmark,
                   .frame = &window->frame, .x = x, .y = y))
    {
        input->hit = CLI_HIT_WORDMARK;
        return;
    }
    // the rest of the name's pane takes a press to no view
    if (window->wordmark.exists &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.work, .x = x, .y = y))
    {
        return;
    }
    if (EMBED_CALL(gui_strip_frame.hit, GuiStripFrameCfg, .frame = &window->frame, .x = x, .y = y,
                   .at_pointer = &input->index))
    {
        input->hit = CLI_HIT_TOOL;
        return;
    }
    // the terminal's panel takes every press on it, those on none of its parts to nothing
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.terminal, .x = x, .y = y))
    {
        input->hit = EMBED_CALL(gui_terminal.hit, GuiTerminalCfg, .terminal_read = &window->terminal,
                                .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .x = x, .y = y,
                                .index = &input->index, .row = &input->row, .column = &input->column);
        return;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = x, .y = y))
    {
        if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .key = "problems"))
        {
            if (EMBED_CALL(gui_problems_frame.head_holds, GuiProblemsFrameCfg, .frame = &window->frame, .x = x, .y = y))
            {
                input->hit = CLI_HIT_PROBLEMS_CHECK;
            }
            else if (EMBED_CALL(gui_problems_row_frame.hit, GuiProblemsRowFrameCfg, .problems = &window->problems,
                                .frame = &window->frame, .x = x, .y = y, .row_pointer = &input->index))
            {
                input->hit = CLI_HIT_PROBLEM_ROW;
            }
        }
        else if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .key = "explorer") &&
                 EMBED_CALL(gui_explorer_frame_xy.hit, GuiExplorerFrameXYCfg, .explorer_read = &window->explorer,
                            .frame = &window->frame, .x = x, .y = y, .row_pointer = &input->index))
        {
            input->hit = CLI_HIT_EXPLORER_ROW;
        }
        return;
    }
    int closing = 0;
    const CliTabs *const tabs = window->tabs;
    if ((tabs != NULL) &&
        EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = (GuiGlyphs *)&window->glyphs,
                   .frame = &window->frame, .tab = tabs->row, .count = tabs->count, .x = x, .y = y,
                   .at_pointer = &input->index, .closing = &closing))
    {
        input->hit = closing ? CLI_HIT_TAB_CLOSE : CLI_HIT_TAB;
        return;
    }
    if (window->editor.frame == NULL)
    {
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = window->frame.scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg,
                                  .glyphs = (GuiGlyphs *)&window->glyphs, .size = size);
    if (EMBED_CALL(gui_editor_position_cell_scale.cell_at, GuiEditorPositionCellScaleCfg,
                   .editor_read = &window->editor, .place = window->frame.editor, .cell = cell,
                   .scale = window->frame.scale, .x = x, .y = y, .row = &input->row, .column = &input->column))
    {
        input->hit = CLI_HIT_EDITOR;
    }
    else if (EMBED_CALL(gui_editor_position_cell_scale.row_at, GuiEditorPositionCellScaleCfg,
                        .editor_read = &window->editor, .place = window->frame.editor, .cell = cell,
                        .scale = window->frame.scale, .x = x, .y = y, .row = &input->row))
    {
        input->hit = CLI_HIT_GUTTER;
    }
}

// the mods the system names, as the controller's bits
EMBED_INLINE unsigned int window_mods(const GuiInput *input)
{
    return ((input->mods & GUI_MOD_SHIFT) != 0u) ? CLI_INPUT_MOD_SHIFT : 0u;
}

// the right button pressed or the wheel turned at x, y: away from the menus open it closes them, as menu.js closes them
// on any press or wheel outside them, and on a menu or on the bar's titles while the bar's menu is open it does nothing
EMBED_INLINE void window_menus_away(const GuiWindowCtx *args)
{
    const CliMenus *const menus = args->window_read->menubar.menus;
    if ((menus == NULL) || (menus->levels == 0u))
    {
        return;
    }
    CliInput pressed = {.kind = CLI_INPUT_MENU_AWAY};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window_read, .x = args->x, .y = args->y,
               .input = &pressed);
    if ((pressed.hit != CLI_HIT_MENU_ROW) && ((pressed.hit != CLI_HIT_MENU_TITLE) || (menus->open < 0)))
    {
        EMBED_CALL(window_menus_close, GuiWindowCtx, .window_read = args->window_read);
    }
}

static void window_wheel(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // while the name's pane stands no view takes the wheel
    if (window->wordmark.exists)
    {
        return;
    }
    EMBED_CALL(window_menus_away, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y);
    // the wheel over the side scrolls the rows, which is the window's own; over the completion's list its rows; over
    // the editor or the map, the text
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = args->input->x,
                   .y = args->input->y))
    {
        const int moved =
            EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .key = "problems")
                ? EMBED_CALL(gui_problems_row_frame.wheel, GuiProblemsRowFrameCfg, .problems_out = &window->problems,
                             .frame = &window->frame, .rows = args->input->rows)
                : EMBED_CALL(gui_explorer_frame.wheel, GuiExplorerFrameCfg, .explorer = &window->explorer,
                             .frame = &window->frame, .rows = args->input->rows);
        window->pending = moved || window->pending;
        return;
    }
    if (EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &window->suggest, .x = args->input->x,
                   .y = args->input->y))
    {
        const CliInput wheel = {.kind = CLI_INPUT_WHEEL, .hit = CLI_HIT_SUGGEST_LIST, .rows_moved = args->input->rows};
        cli_controller_input(&wheel, NULL);
        return;
    }
    // over a terminal's pane, its view
    unsigned int pane = 0u;
    if (EMBED_CALL(gui_terminal.hit, GuiTerminalCfg, .terminal_read = &window->terminal, .frame = &window->frame,
                   .glyphs = (GuiGlyphs *)&window->glyphs, .x = args->input->x, .y = args->input->y,
                   .index = &pane) == CLI_HIT_TERMINAL)
    {
        const CliInput wheel = {
            .kind = CLI_INPUT_WHEEL, .hit = CLI_HIT_TERMINAL, .index = pane, .rows_moved = args->input->rows};
        cli_controller_input(&wheel, NULL);
        return;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.editor, .x = args->input->x,
                   .y = args->input->y) ||
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.map, .x = args->input->x,
                   .y = args->input->y))
    {
        const CliInput wheel = {.kind = CLI_INPUT_WHEEL, .hit = CLI_HIT_EDITOR, .rows_moved = args->input->rows};
        cli_controller_input(&wheel, NULL);
    }
}

// a press of the middle button: on a terminal's tab it ends the tab's shell and closes the tab, as its × does, and
// elsewhere it does nothing
EMBED_INLINE void window_middle_press(const GuiWindowCtx *args)
{
    CliInput pressed = {.kind = CLI_INPUT_POINTER_DOWN};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window_read, .x = args->x, .y = args->y,
               .input = &pressed);
    if (pressed.hit == CLI_HIT_TERMINAL_TAB)
    {
        pressed.hit = CLI_HIT_TERMINAL_TAB_CLOSE;
        cli_controller_input(&pressed, NULL);
    }
}

// whether the pixel at x, y stands on the terminal's top edge, which drags its height: the panel's top line and the
// grip's reach either side of it
EMBED_INLINE int window_terminal_grip(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window_read->frame;
    const int reach =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TERMINAL_GRIP, .scale = frame->scale);
    return !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->terminal) &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                      .rect = (GuiRect){frame->terminal.x, frame->terminal.y - reach, frame->terminal.width, 2 * reach},
                      .x = args->x, .y = args->y);
}

// the terminal's height for its top edge dragged to y, in the page's pixels, between its least and its most as the
// frame lays it, told the controller where it moved
EMBED_INLINE void window_terminal_drag(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiFrame *const frame = &window->frame;
    const int foot = frame->terminal.y + frame->terminal.height;
    const int scale = (frame->scale > 0) ? frame->scale : GUI_SCALE_ONE;
    // a height in the frame's pixels comes back to the page's at the scale
    const int asked = ((foot - args->y) * GUI_SCALE_ONE) / scale;
    // the window's height in the page's pixels, of which the panel takes no more than its share and what leaves the
    // views their least
    const int window_height = ((int)frame->height * GUI_SCALE_ONE) / scale;
    const int share = (window_height * GUI_TERMINAL_MOST_PERCENT) / 100;
    const int leaves = window_height - GUI_TERMINAL_SHELL_LEAST;
    const int most = (share < leaves) ? share : leaves;
    const int bounded = (asked < most) ? asked : most;
    const int height = (bounded > GUI_TERMINAL_LEAST) ? bounded : GUI_TERMINAL_LEAST;
    // the height is not negative
    if ((unsigned int)height != window->terminal_height_sent)
    {
        window->terminal_height_sent = (unsigned int)height;
        const CliInput sized = {.kind = CLI_INPUT_TERMINAL_HEIGHT, .rows = (unsigned int)height};
        cli_controller_input(&sized, NULL);
    }
}

static void window_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    if (input->button == GUI_BUTTON_MIDDLE)
    {
        EMBED_CALL(window_middle_press, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y);
        return;
    }
    if (input->button == GUI_BUTTON_RIGHT)
    {
        EMBED_CALL(window_menus_away, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y);
        return;
    }
    if (EMBED_CALL(window_terminal_grip, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y))
    {
        window->terminal_gripped = 1;
        window->terminal_height_sent = 0u;
        return;
    }
    const unsigned int hit = EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input);
    window->pressed =
        ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE)) ? hit : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
    CliInput pressed = {.kind = CLI_INPUT_POINTER_DOWN, .mods = window_mods(input)};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y, .input = &pressed);
    // a press on the map goes to that part of the text and may begin the slider's drag, the top worked out from what
    // the map shows
    unsigned long long top = 0ULL;
    if ((pressed.hit == CLI_HIT_NONE) &&
        EMBED_CALL(gui_minimap_position_scale.press, GuiMinimapPositionScaleCfg, .map = &window->minimap,
                   .place = window->frame.map, .scale = window->frame.scale, .editor = &window->editor,
                   .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = window), .x = input->x,
                   .y = input->y, .top = &top))
    {
        pressed.hit = CLI_HIT_MAP;
        pressed.row = top;
    }
    // presses in a row within the time and near the place count up
    const unsigned long long now_ms = cli_clock_milliseconds();
    const int adjacent = ((input->x - window->clicked_x) <= GUI_CLICK_ADJACENT) &&
                         ((window->clicked_x - input->x) <= GUI_CLICK_ADJACENT) &&
                         ((input->y - window->clicked_y) <= GUI_CLICK_ADJACENT) &&
                         ((window->clicked_y - input->y) <= GUI_CLICK_ADJACENT);
    window->clicks = (now_ms <= window->double_click_deadline_ms) && adjacent ? window->clicks + 1u : 1u;
    window->double_click_deadline_ms = now_ms + GUI_DOUBLE_CLICK_MS;
    window->clicked_x = input->x;
    window->clicked_y = input->y;
    pressed.clicks = window->clicks;
    window->dragging = (pressed.hit == CLI_HIT_EDITOR) || (pressed.hit == CLI_HIT_GUTTER);
    window->terminal_dragged = (pressed.hit == CLI_HIT_TERMINAL) ? (pressed.index + 1u) : 0u;
    window->moved_sent = pressed;
    cli_controller_input(&pressed, NULL);
}

static void window_move(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    if (window->terminal_gripped)
    {
        EMBED_CALL(window_terminal_drag, GuiWindowCtx, .window = window, .y = args->input->y);
        return;
    }
    // a drag that began in a terminal's pane goes on choosing in it, the pointer clamped to its rows and columns
    if (window->terminal_dragged != 0u)
    {
        CliInput moved = {.kind = CLI_INPUT_POINTER_MOVED,
                          .hit = CLI_HIT_TERMINAL,
                          .index = window->terminal_dragged - 1u,
                          .clicks = window->clicks};
        if (EMBED_CALL(gui_terminal.cell_at, GuiTerminalCfg, .terminal_read = &window->terminal,
                       .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .view = moved.index,
                       .x = args->input->x, .y = args->input->y, .row = &moved.row, .column = &moved.column) &&
            ((moved.row != window->moved_sent.row) || (moved.column != window->moved_sent.column)))
        {
            window->moved_sent = moved;
            cli_controller_input(&moved, NULL);
        }
        return;
    }
    if (window->claim.autohidden)
    {
        const GuiFrame *const frame = &window->frame;
        const unsigned long long now_ms = cli_clock_milliseconds();
        const int adjacent_before = window->claim.adjacent;
        EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &window->claim, .x = args->input->x,
                   .y = args->input->y,
                   .zone =
                       EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = frame->width,
                                  .height = frame->height, .scale = frame->scale,
                                  .controls_width = frame->controls.width, .x = args->input->x, .y = args->input->y),
                   .over_parts = EMBED_CALL(window_over_parts, GuiWindowCtx, .window_read = window, .x = args->input->x,
                                            .y = args->input->y));
        if ((window->claim.adjacent != adjacent_before) ||
            EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &window->claim, .time_ms = now_ms))
        {
            window->autohide_moving = 1;
            EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = window);
        }
    }
    // the marks the pointer leaves on what it stands over are the window's own
    window->pending =
        EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &window->menubar,
                   .glyphs = &window->glyphs, .frame = &window->frame, .x = args->input->x, .y = args->input->y) ||
        window->pending;
    window->pending = EMBED_CALL(gui_strip_frame_x.move, GuiStripFrameXCfg, .strip = &window->strip,
                                 .frame = &window->frame, .x = args->input->x, .y = args->input->y) ||
                      window->pending;
    window->pending = (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .key = "problems")
                           ? EMBED_CALL(gui_problems_frame.move, GuiProblemsFrameCfg, .problems_out = &window->problems,
                                        .frame = &window->frame, .x = args->input->x, .y = args->input->y)
                           : EMBED_CALL(gui_explorer_frame_x.move, GuiExplorerFrameXCfg, .explorer = &window->explorer,
                                        .frame = &window->frame, .x = args->input->x, .y = args->input->y)) ||
                      window->pending;
    // the slider dragged goes to the part of the text it stands over, as a press on the map does
    unsigned long long top = 0ULL;
    if (EMBED_CALL(gui_minimap_position_scale.drag, GuiMinimapPositionScaleCfg, .map = &window->minimap,
                   .y = args->input->y, .top = &top))
    {
        if ((window->editor.frame != NULL) && (top != window->editor.frame->top))
        {
            const CliInput dragged = {.kind = CLI_INPUT_POINTER_DOWN, .hit = CLI_HIT_MAP, .row = top};
            cli_controller_input(&dragged, NULL);
        }
        return;
    }
    CliInput moved = {.kind = CLI_INPUT_POINTER_MOVED, .clicks = window->clicks};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
               .input = &moved);
    // a pointer that moved on no cell and no row says nothing new
    if ((moved.hit == window->moved_sent.hit) && (moved.level == window->moved_sent.level) &&
        (moved.index == window->moved_sent.index) && (moved.row == window->moved_sent.row) &&
        (moved.column == window->moved_sent.column))
    {
        return;
    }
    window->moved_sent = moved;
    cli_controller_input(&moved, NULL);
}

static void window_leave(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const int adjacent_before = window->claim.adjacent;
    EMBED_CALL(cli_autohide_still.leave, GuiAutohideStillCfg, .claim = &window->claim);
    if (window->claim.adjacent != adjacent_before)
    {
        window->autohide_moving = 1;
        EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = window);
    }
}

static void window_release(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    window->terminal_gripped = 0;
    if (window->terminal_dragged != 0u)
    {
        CliInput released = {.kind = CLI_INPUT_POINTER_UP,
                             .hit = CLI_HIT_TERMINAL,
                             .index = window->terminal_dragged - 1u,
                             .clicks = window->clicks};
        (void)EMBED_CALL(gui_terminal.cell_at, GuiTerminalCfg, .terminal_read = &window->terminal,
                         .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .view = released.index,
                         .x = args->input->x, .y = args->input->y, .row = &released.row, .column = &released.column);
        window->terminal_dragged = 0u;
        cli_controller_input(&released, NULL);
    }
    if (window->minimap.dragging)
    {
        EMBED_CALL(gui_minimap.release, GuiMinimapCfg, .map = &window->minimap);
        window->pending = 1;
    }
    if (window->dragging)
    {
        window->dragging = 0;
        CliInput released = {.kind = CLI_INPUT_POINTER_UP, .clicks = window->clicks};
        EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
                   .input = &released);
        cli_controller_input(&released, NULL);
    }
}

static void window_key(const GuiInputHandlerCfg *args)
{
    const GuiWindow *window = args->window;
    const CliInput keyed = {.kind = CLI_INPUT_KEY, .hit = window->wordmark.exists ? CLI_HIT_WORDMARK : CLI_HIT_NONE};
    EMBED_CALL(window_text_send, GuiWindowCtx, .input = (CliInput *)&keyed, .bytes = args->input->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->input->name, .cap = 64ULL}) + 1ULL);
}

// the keys gone to another window: the menus open closed
static void window_blur(const GuiInputHandlerCfg *args)
{
    EMBED_CALL(window_menus_close, GuiWindowCtx, .window_read = args->window);
}

// the system's gesture for a menu: the menu of what stands at the pointer asked for there
static void window_menu(const GuiInputHandlerCfg *args)
{
    CliInput asked = {
        .kind = CLI_INPUT_MENU, .mods = window_mods(args->input), .x = args->input->x, .y = args->input->y};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window, .x = args->input->x, .y = args->input->y,
               .input = &asked);
    cli_controller_input(&asked, NULL);
}

static void window_text(const GuiInputHandlerCfg *args)
{
    // while the name's pane stands no view takes the text
    if (args->window->wordmark.exists)
    {
        return;
    }
    const CliInput typed = {.kind = CLI_INPUT_TEXT};
    EMBED_CALL(window_text_send, GuiWindowCtx, .input = (CliInput *)&typed, .bytes = args->input->text,
               .length = args->input->size);
}

// what answers each kind of input, in the main window and in Preferences' window
static const GuiInputHandlers s_window_handlers = {{[GUI_INPUT_MOVED] = window_move,
                                                    [GUI_INPUT_DOWN] = window_press,
                                                    [GUI_INPUT_UP] = window_release,
                                                    [GUI_INPUT_LEFT] = window_leave,
                                                    [GUI_INPUT_WHEEL] = window_wheel,
                                                    [GUI_INPUT_KEY] = window_key,
                                                    [GUI_INPUT_TEXT] = window_text,
                                                    [GUI_INPUT_MENU] = window_menu,
                                                    [GUI_INPUT_BLUR] = window_blur}};
static const GuiInputHandlers s_preferences_handlers = {{[GUI_INPUT_MOVED] = window_preferences_move,
                                                         [GUI_INPUT_DOWN] = window_preferences_press,
                                                         [GUI_INPUT_LEFT] = window_preferences_leave,
                                                         [GUI_INPUT_WHEEL] = window_preferences_wheel,
                                                         [GUI_INPUT_KEY] = window_preferences_key,
                                                         [GUI_INPUT_TEXT] = window_preferences_text}};

void gui_window_input(const GuiWindowInputKindCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &s_window_handlers, .window = window,
               .input = input);
}

void gui_window_preferences_input(const GuiWindowInputKindCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &s_preferences_handlers, .window = window,
               .input = input);
}
