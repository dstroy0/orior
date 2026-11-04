// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the window on the view's thread. It finds what a pixel stands on and hands each input straight to the
// module it is for: the menus and the notices of the command line, the views of the editor and their files, the
// explorer's listing, the servers' panels and problems, and the terminal's tabs; each part draws what its module
// stores, read where it stands. What is the window's own is what moves on the screen, the strip's tool shown, whether
// the editor takes the keys, the settings it draws with and the rows' scroll offsets and the marks the pointer leaves
// on them
#include "window.h"
#include "../command_line/callback_cfg.h"

#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../machine/state.h"
#include "../text/text.h"
#include "parts/chrome.h"

// orior's repository, a press on the name opening it; the keys of orior's folder the settings, the session's wordmark
// and the strip's group stand under; and two starts of a session, the seconds their boot times may read apart, and the
// most bytes of a log in
#define GUI_WINDOW_REPOSITORY "https://github.com/dstroy0/orior"
#define GUI_WINDOW_REDUCE_KEY "reduce-motion"
#define GUI_WINDOW_ZOOM_KEY "zoom"
#define GUI_WINDOW_BAR_KEY "opacity.bar"
#define GUI_WINDOW_EDITOR_KEY "opacity.editor"
#define GUI_WINDOW_COLOR_KEY "color."
#define GUI_WINDOW_WORDMARK_KEY "wordmark.session"
#define GUI_WINDOW_GROUP_KEY "panes.group"
#define GUI_WINDOW_SESSION_SECONDS 60ULL
#define GUI_WINDOW_LOGON_MAX_LEN 128u
// the scheme's opacities of the bar and the editor, in percent, where none is saved
#define GUI_WINDOW_BAR_PERCENT 30u
#define GUI_WINDOW_EDITOR_PERCENT 80u

// the commands the window has no keys for that the view runs by name, and the commands that run here whatever else
// their keys might mean, the terminal's among them
static const char *const s_view_commands[] = {"sort-ascending", "sort-descending", "unique-lines",
                                              "upper-case",     "lower-case",      "title-case"};
static const char *const s_own_commands[] = {
    "exit",           "close-editor", "close-all", "last-editor", "reduce-motion", "preferences",   "terminal-view",
    "toggle",         "new",          "split",     "clear",       "kill",          "terminal-copy", "terminal-paste",
    "terminal-close", "save",         "save-all"};

// the window, which the menus' answers and the walk's threads reach; and whether the walk colored a line since the
// window last took the frame, set on the walk's threads, and what wakes the view's thread
static GuiWindow *s_window;
static embed_atomic_u32 s_colored;
static void (*s_woken)(void);

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
    EMBED_CALL(gui_sides.init, GuiSidesCfg, .sides = &args->window->sides);
    EMBED_CALL(gui_docks.init, GuiDocksCfg, .docks = &args->window->docks);
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
    GuiWindowHit *hit;
    const char *name;
    const char *const *words;
    unsigned int count;
    unsigned int at;
    const CliPosition *place;
    const CliStateEntries *entries;
    unsigned int *percent_out;
    int edited;
    const CliViewInput *view_input;
    int starting;
} GuiWindowCtx;

void gui_window_end(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    cli_view_session_save(cli_command_line_root());
    cli_terminal_tabs_end();
    cli_servers_problems_end();
    cli_servers_panels_end();
    cli_files_explorer_end();
    // the notices read the entries in place, so they let go before the entries do
    cli_notices_end();
    g_cli_cells->persistent_buf_release(window->entries, &g_cli_view_cells);
    window->entries = NULL;
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = NULL, .threaded = 0);
    // what the window took of the views goes back to them: nothing of it is read from here on
    cli_view_frame_release((CliViewFrame *)window->editor.frame);
    g_cli_cells->persistent_buf_release((void *)window->editor.names, &g_cli_view_cells);
    cli_view_map_release((CliDocumentMap *)window->minimap.lines);
    cli_view_frame_release((CliViewFrame *)window->terminal.frame[0]);
    cli_view_frame_release((CliViewFrame *)window->terminal.frame[1]);
    g_cli_cells->persistent_buf_release((void *)window->terminal.names[0], &g_cli_view_cells);
    g_cli_cells->persistent_buf_release((void *)window->terminal.names[1], &g_cli_view_cells);
    window->editor.frame = NULL;
    window->editor.names = NULL;
    window->minimap.lines = NULL;
    window->terminal.frame[0] = NULL;
    window->terminal.frame[1] = NULL;
    window->terminal.names[0] = NULL;
    window->terminal.names[1] = NULL;
    window->hover.panel = NULL;
    window->suggest.panel = NULL;
    window->signature.panel = NULL;
    s_window = NULL;
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

// the key of the view shown, NULL for none
EMBED_INLINE const char *window_shown_key(const GuiWindowCtx *args)
{
    (void)args;
    const int shown = cli_view_shown();
    // the view shown is among the views
    return (shown >= 0) ? cli_view_key((unsigned int)shown) : NULL;
}

// the terminal read again of its tabs: the panel, its panes, and each pane's frame of its tab's text and the table of
// its runs' tokens, taken anew, those it had released
EMBED_INLINE void window_terminal_take(const GuiWindowCtx *args)
{
    GuiTerminal *const terminal = &args->window->terminal;
    terminal->panel = cli_terminal_tabs_panel();
    for (unsigned int view = 0u; view < 2u; view += 1u)
    {
        const int shown = cli_terminal_tabs_view(view, &terminal->view[view]);
        const char *const key = shown ? terminal->view[view].key : NULL;
        cli_view_frame_release((CliViewFrame *)terminal->frame[view]);
        terminal->frame[view] = (key != NULL) ? cli_view_frame_alloc(key) : NULL;
        g_cli_cells->persistent_buf_release((void *)terminal->names[view], &g_cli_view_cells);
        terminal->names[view] = (key != NULL) ? EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg,
                                                           .key = key, .owner = &g_cli_view_cells)
                                              : NULL;
    }
    args->window->pending = 1;
}

// the columns and rows of each of the terminal's screens shown handed to its tabs, where they moved; a screen no longer
// shown handed them again as it shows
EMBED_INLINE void window_terminal_resize(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    int sized = 0;
    for (unsigned int view = 0u; view < 2u; view += 1u)
    {
        unsigned int columns = 0u;
        unsigned int rows = 0u;
        const int shown =
            EMBED_CALL(gui_terminal.size, GuiTerminalCfg, .terminal_read = &window->terminal, .frame = &window->frame,
                       .glyphs = &window->glyphs, .view = view, .columns = &columns, .rows = &rows);
        if (shown && ((columns != window->terminal_columns[view]) || (rows != window->terminal_rows[view])))
        {
            cli_terminal_tabs_size(view, columns, rows);
            sized = 1;
        }
        window->terminal_columns[view] = columns;
        window->terminal_rows[view] = rows;
    }
    if (sized)
    {
        EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
    }
}

// the shown view's frame, names and map taken anew of it, those it had released; `edited`
// where the text changed, which the map's lines no longer show
EMBED_INLINE void window_view_take(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    const int shown = cli_view_shown();
    if (shown != window->view_shown)
    {
        // another file shown: the map stands anew beside it, at the editor's size
        window->view_shown = shown;
        window->map_known = 0;
        EMBED_CALL(gui_minimap.clear, GuiMinimapCfg, .map = &window->minimap);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = window->rows, .columns = window->columns};
        if (key != NULL)
        {
            (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized,
                             .err_msg = &err_msg);
        }
    }
    cli_view_frame_release((CliViewFrame *)window->editor.frame);
    window->editor.frame = (key != NULL) ? cli_view_frame_alloc(key) : NULL;
    g_cli_cells->persistent_buf_release((void *)window->editor.names, &g_cli_view_cells);
    window->editor.names = (key != NULL) ? EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = key,
                                                      .owner = &g_cli_view_cells)
                                         : NULL;
    if (args->edited || (window->minimap.lines == NULL) || (key == NULL))
    {
        cli_view_map_release((CliDocumentMap *)window->minimap.lines);
        window->minimap.lines =
            ((key != NULL) && window->map_known) ? cli_view_map_alloc(key, window->map_first, window->map_last) : NULL;
    }
    // every frame the view answers shows the caret through the input rest, its blink beginning after it, so the caret
    // stands shown while the keys go
    window->caret_blink_start_ms = cli_clock_milliseconds() + GUI_CARET_INPUT_REST_MS;
    window->pending = 1;
}

// the editor's size handed to the view shown and how many titles the bar shows handed to the menus, where either moved,
// and the terminal's screens' sizes
EMBED_INLINE void window_view_resize(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    EMBED_CALL(window_terminal_resize, GuiWindowCtx, .window = window);
    const unsigned int rows = EMBED_CALL(window_rows, GuiWindowCtx, .window_read = window);
    const unsigned int columns = window_columns(args);
    const unsigned int titles = cli_menu_titles();
    const unsigned int shown = (window->menubar.folded_from < titles) ? window->menubar.folded_from : titles;
    const unsigned int folded = window->menubar.folded_from < titles;
    cli_menus_bar(shown, (int)folded);
    if ((rows == window->rows) && (columns == window->columns))
    {
        return;
    }
    window->rows = rows;
    window->columns = columns;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = rows, .columns = columns};
    if ((key != NULL) &&
        EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized, .err_msg = &err_msg))
    {
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0);
    }
}

// the lines the map placed beside the view stands for taken of the view, where they moved
EMBED_INLINE void window_map_span(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if ((window->editor.frame == NULL) ||
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = window->frame.map))
    {
        return;
    }
    EMBED_CALL(gui_minimap_position.place, GuiMinimapPositionCfg, .map = &window->minimap,
               .place = window->frame.map_whole, .clip = window->frame.map, .scale = window->frame.scale,
               .editor = &window->editor,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = window));
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    if (!EMBED_CALL(gui_minimap_first.span, GuiMinimapFirstCfg, .map_read = &window->minimap, .first = &first,
                    .last = &last) ||
        (window->map_known && (window->map_first == first) && (window->map_last == last)))
    {
        return;
    }
    window->map_known = 1;
    window->map_first = first;
    window->map_last = last;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    cli_view_map_release((CliDocumentMap *)window->minimap.lines);
    window->minimap.lines = (key != NULL) ? cli_view_map_alloc(key, first, last) : NULL;
    window->pending = 1;
}

void gui_window_autohide_layout(const GuiWindowAutohideCfg *args)
{
    args->window->pending = 1;
    // the claim's slides, the side pane's and the terminal panel's arrival run through the stages, each stage's frame
    // laid with all of them at its moment
    if (EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &args->window->claim,
                   .time_ms = args->time_ms) ||
        (EMBED_CALL(gui_sides.slide_end_ms, GuiSidesCfg, .sides_read = &args->window->sides,
                    .time_ms = args->time_ms) != 0ULL) ||
        (EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = &args->window->docks,
                    .time_ms = args->time_ms) != 0ULL))
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
                             .claim = &args->window->claim, .sides_read = &args->window->sides,
                             .strip_read = &args->window->strip, .docks_read = &args->window->docks,
                             .frame = &args->window->frame, .base = &args->window->base,
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
    // at rest, or the stages' cell not taken: the frame the numbers, the side's slide and the panel's arrival lay now,
    // and no stage left standing
    args->window->stage = NULL;
    args->window->stages_stale = 0;
    EMBED_CALL(gui_stages_autohide.free, GuiStagesAutohideCfg, .stages = &args->window->stages);
    args->window->frame.side_displayed = EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &args->window->sides,
                                                    .strip_read = &args->window->strip, .time_ms = args->time_ms);
    args->window->frame.side_away =
        EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &args->window->sides, .time_ms = args->time_ms);
    args->window->frame.terminal_away =
        EMBED_CALL(gui_docks.away, GuiDocksCfg, .docks_read = &args->window->docks, .time_ms = args->time_ms);
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

// whether the side stands in the work now: the strip's tool shown has the side pane as its window and the pane has not
// collapsed, or its width stands as it slides away
EMBED_INLINE int window_side_show(const GuiWindowCtx *args)
{
    return EMBED_CALL(gui_sides.stands, GuiSidesCfg, .sides_read = &args->window_read->sides,
                      .strip_read = &args->window_read->strip, .time_ms = cli_clock_milliseconds());
}

// the terminal read again of its tabs and the work's inside laid again with its panel at its foot as the panel stands,
// a panel opened coming in from the foot unless the window is `starting`, the editor's and the screens' sizes handed
// on where they moved
EMBED_INLINE void window_terminal(const GuiWindowCtx *args)
{
    EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = args->window);
    const CliTerminalPanel *const panel = &args->window->terminal.panel;
    // a panel's height in the page's pixels is far inside what an int represents
    const int height = panel->open ? (int)panel->height : 0;
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
        const unsigned long long now_ms = cli_clock_milliseconds();
        if ((args->window->frame.terminal_height == 0) && !args->starting)
        {
            EMBED_CALL(gui_docks.show, GuiDocksCfg, .docks = &args->window->docks, .still = args->window->motion_still,
                       .time_ms = now_ms);
        }
        EMBED_CALL(gui_frame_displayed.terminal, GuiFrameDisplayedCfg, .frame = &args->window->frame, .height = height,
                   .away =
                       EMBED_CALL(gui_docks.away, GuiDocksCfg, .docks_read = &args->window->docks, .time_ms = now_ms));
        // a movement under way, or the arrival begun, runs on with the panel as it now stands
        if ((args->window->stage != NULL) ||
            (EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = &args->window->docks, .time_ms = now_ms) !=
             0ULL))
        {
            args->window->stages_stale = 1;
            window_autohide_layout(args);
        }
    }
    args->window->pending = 1;
    window_view_resize(args);
}

// the work's inside laid again as the strip's tool shown has a side or not, the editor's size handed on where it moved
EMBED_INLINE void window_side(const GuiWindowCtx *args)
{
    const int side = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &args->window->frame, .displayed = side,
               .away = EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &args->window->sides,
                                  .time_ms = cli_clock_milliseconds()));
    args->window->pending = 1;
    // a movement under way runs on with the side as it now stands
    if (args->window->stage != NULL)
    {
        args->window->stages_stale = 1;
        window_autohide_layout(args);
    }
    window_view_resize(args);
}

// the side laid again as the pane's waits have shown or collapsed it or a preview has changed its tool, the explorer
// listed as it shows
EMBED_INLINE void window_sides_laid(const GuiWindowCtx *args)
{
    if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = args->window, .key = "explorer"))
    {
        cli_files_explorer_show(cli_command_line_root());
    }
    EMBED_CALL(window_side, GuiWindowCtx, .window = args->window);
}

// the menus open closed, as menu.js closes them on a press or the wheel away from them, the window resized or its keys
// gone to another window
EMBED_INLINE void window_menus_close(const GuiWindowCtx *args)
{
    if (g_cli_menus_open.levels != 0u)
    {
        cli_menus_close(0u);
        args->window->pending = 1;
    }
}

int gui_window_resize(const GuiWindowWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    if ((width != args->window->frame.width) || (height != args->window->frame.height))
    {
        EMBED_CALL(window_menus_close, GuiWindowCtx, .window = args->window);
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
               .displayed = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window),
               .away = EMBED_CALL(gui_sides.away, GuiSidesCfg, .sides_read = &args->window->sides,
                                  .time_ms = cli_clock_milliseconds()));
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

void gui_window_motion_wish(const GuiWindowReducedCfg *args)
{
    args->window->motion_system = args->reduced;
    EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = args->window);
}

void gui_window_show(const GuiWindowDisplayedCfg *args)
{
    args->window->off_screen = !args->displayed;
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->background_fill,
               .paused = !args->displayed || args->window->motion_still);
    args->window->pending = args->displayed || args->window->pending;
}

// the caret's ink, 0 or 255: hidden while the editor does not have the keys; else shown through the input rest before
// its blink begins, then shown for the first half of each blink and hidden for the second; shown while no cursor
// stands or less motion is asked for
EMBED_INLINE unsigned int window_caret_color(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long begun_ms = args->window_read->caret_blink_start_ms;
    if (!args->window_read->editing)
    {
        return 0u;
    }
    if (!(args->window_read->editor.frame != NULL) || args->window_read->motion_still || (now_ms < begun_ms))
    {
        return 255u;
    }
    return (((now_ms - begun_ms) % GUI_CARET_BLINK_PERIOD_MS) < (GUI_CARET_BLINK_PERIOD_MS / 2ULL)) ? 255u : 0u;
}

// the deadline the caret's ink next changes at: the end of the first half of its blink while the input rest runs, else
// the end of the half of the blink it stands in; 0 where the editor does not have the keys, no cursor stands or less
// motion is asked for
EMBED_INLINE unsigned long long window_caret_deadline_ms(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long begun_ms = args->window_read->caret_blink_start_ms;
    const unsigned long long half_ms = GUI_CARET_BLINK_PERIOD_MS / 2ULL;
    if (!args->window_read->editing || !(args->window_read->editor.frame != NULL) || args->window_read->motion_still)
    {
        return 0ULL;
    }
    if (now_ms < begun_ms)
    {
        return begun_ms + half_ms;
    }
    return now_ms + half_ms - ((now_ms - begun_ms) % half_ms);
}

void gui_window_tick(const GuiWindowCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    (void)EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &args->window->claim);
    const int moving =
        EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &args->window->claim, .time_ms = now_ms) ||
        (EMBED_CALL(gui_sides.slide_end_ms, GuiSidesCfg, .sides_read = &args->window->sides, .time_ms = now_ms) !=
         0ULL) ||
        (EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = &args->window->docks, .time_ms = now_ms) !=
         0ULL);
    // the frame laid for the claim's numbers, the side's slide and the panel's arrival while any of them moves and as
    // they come to rest; a tick with all at rest owes nothing for them
    if (moving || args->window->autohide_moving)
    {
        EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = args->window);
    }
    if (args->window->autohide_moving && !moving)
    {
        // the slides at rest, the editor's size and the map's span handed on where they moved
        EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
        EMBED_CALL(window_map_span, GuiWindowCtx, .window = args->window);
    }
    args->window->autohide_moving = moving;
    // the side pane's waits reached: the pane collapsed or shown again, or a tool's pane shown as a preview
    if (EMBED_CALL(gui_sides.tick, GuiSidesCfg, .sides = &args->window->sides, .strip = &args->window->strip,
                   .still = args->window->motion_still, .menu_open = g_cli_menus_open.levels > 0u, .time_ms = now_ms))
    {
        EMBED_CALL(window_sides_laid, GuiWindowCtx, .window = args->window);
    }
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
    const unsigned int levels = g_cli_menus_open.levels;
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
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long deadlines[] = {
        EMBED_CALL(gui_autohide.next_deadline_ms, GuiAutohideCfg, .autohide_read = &window->claim),
        // the side pane's slide and the terminal panel's arrival read their next stage a stage on
        ((EMBED_CALL(gui_sides.slide_end_ms, GuiSidesCfg, .sides_read = &window->sides, .time_ms = now_ms) != 0ULL) ||
         (EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = &window->docks, .time_ms = now_ms) != 0ULL))
            ? now_ms + GUI_STAGE_MS
            : 0ULL,
        EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg,
                   .background_fill = &window->background_fill),
        EMBED_CALL(gui_wordmark_read.next_deadline_ms, GuiWordmarkReadCfg, .wordmark_read = &window->wordmark),
        EMBED_CALL(window_caret_deadline_ms, GuiWindowCtx, .window_read = window),
        EMBED_CALL(gui_sides.next_deadline_ms, GuiSidesCfg, .sides_read = &window->sides),
        // the circle's clock stops while the window is off screen, a wake from a module's thread waking it as any does
        window->off_screen ? 0ULL : EMBED_CALL(gui_busy.next_deadline_ms, GuiBusyCfg, .busy_read = &window->busy),
        cli_servers_panels_due_ms(),
        cli_terminal_tabs_next_ms(),
        cli_notices_due_ms(),
        g_cli_menus_open.rest_due_ms,
        window->memory_due_ms,
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
    return (earliest > now_ms) ? earliest - now_ms : 1ULL;
}

// the setting `key` saved as `value` in orior's folder
EMBED_INLINE void window_setting_save(const GuiWindowCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = args->key, .value = args->value, .err_msg = &err_msg});
    }
}

// a percent saved in orior's folder as the setting `key`
EMBED_INLINE void window_percent_save(const GuiWindowCtx *args)
{
    char text[8];
    VerbaLinea value = {text, sizeof(text), 0ULL};
    mmgr_line_decimal(&value, args->percent, 1u);
    (void)mmgr_line_finish(&value);
    EMBED_CALL(window_setting_save, GuiWindowCtx, .key = args->key, .value = text);
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

// the settings as they now stand drawn with: the opacities, less motion, Preferences owing a frame, and the frame laid
// again where `on` is 1, a zoom or a color having changed, laying it moving what a menu stands on
EMBED_INLINE void window_settings_layout(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiPreferenceValues *const values = &window->preference_values;
    EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &window->palette_own,
               .bar_percent = values->bar_percent, .editor_percent = values->editor_percent);
    EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = window);
    window->preferences_pending = 1;
    if ((window->frame.width != 0u) && args->on)
    {
        window_preferences_layout(args);
    }
    window->pending = 1;
}

// Preferences' window opened where it is closed and closed where it is open
EMBED_INLINE void window_preferences_toggle(const GuiWindowCtx *args)
{
    args->window->preferences.displayed = !args->window->preferences.displayed;
    args->window->preferences.entering = 0;
    args->window->preferences_pending = 1;
}

// a row of Settings moved: what it now stands at taken and saved in orior's folder
EMBED_INLINE void window_preference_step(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (args->change->at == GUI_PREFERENCE_ZOOM)
    {
        const unsigned int step =
            EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg, .step = values->zoom_step,
                       .by = args->change->by, .round = args->change->round);
        const int zoomed = step != values->zoom_step;
        values->zoom_step = step;
        EMBED_CALL(window_percent_save, GuiWindowCtx, .key = GUI_WINDOW_ZOOM_KEY,
                   .percent =
                       EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg, .step = step));
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = zoomed);
    }
    else if (args->change->at == GUI_PREFERENCE_REDUCE)
    {
        // chosen, it asks for the opposite of what the window now does
        window->motion_saved = !window->motion_still;
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = GUI_WINDOW_REDUCE_KEY,
                   .value = window->motion_saved ? "true" : "false");
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 0);
    }
    else
    {
        const int bar = args->change->at == GUI_PREFERENCE_BAR;
        unsigned int *const percent = bar ? &values->bar_percent : &values->editor_percent;
        *percent = EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg,
                              .percent = *percent, .by = args->change->by, .round = args->change->round);
        EMBED_CALL(window_percent_save, GuiWindowCtx, .key = bar ? GUI_WINDOW_BAR_KEY : GUI_WINDOW_EDITOR_KEY,
                   .percent = *percent);
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 0);
    }
}

// what a key or a press changed in Preferences taken and saved in orior's folder
EMBED_INLINE void window_preference_change(const GuiWindowCtx *args)
{
    args->window->preferences_pending = 1;
    if (args->change->kind == GUI_PREFERENCE_CLOSED)
    {
        EMBED_CALL(window_preferences_toggle, GuiWindowCtx, .window = args->window);
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
        mmgr_line_put(&line, GUI_WINDOW_COLOR_KEY);
        mmgr_line_put(&line, name);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = args->change->color,
                   .text = value, .size = sizeof(value));
        EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &args->window->palette_own,
                   .at = args->change->at, .color = args->change->color);
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = key, .value = value);
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = args->window, .on = 1);
    }
}

// the percent the text `value` writes into `percent_out`, digits alone and no more than three, where it does
EMBED_INLINE void window_percent_read(const GuiWindowCtx *args)
{
    if (args->value == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = 8ULL});
    unsigned long long end = 0ULL;
    const unsigned long long number = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->value,
                                                 .at = 0ULL, .size = size, .end = &end);
    if ((size != 0ULL) && (size <= 3ULL) && (end == size))
    {
        // no more than three digits, far inside what an unsigned int represents
        *args->percent_out = (unsigned int)number;
    }
}

// the settings orior's folder saved read from `entries`: the zoom, the opacities, less motion and each color a
// setting of `color.` and the palette's name sets
EMBED_INLINE void window_settings_read(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    unsigned int zoom_percent = EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                           .step = GUI_PREFERENCE_ZOOM_DEFAULT_STEP);
    EMBED_CALL(window_percent_read, GuiWindowCtx, .value = cli_state_entry_value(args->entries, GUI_WINDOW_ZOOM_KEY),
               .percent_out = &zoom_percent);
    (void)EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = zoom_percent,
                     .step_pointer = &values->zoom_step);
    values->bar_percent = GUI_WINDOW_BAR_PERCENT;
    values->editor_percent = GUI_WINDOW_EDITOR_PERCENT;
    EMBED_CALL(window_percent_read, GuiWindowCtx, .value = cli_state_entry_value(args->entries, GUI_WINDOW_BAR_KEY),
               .percent_out = &values->bar_percent);
    EMBED_CALL(window_percent_read, GuiWindowCtx, .value = cli_state_entry_value(args->entries, GUI_WINDOW_EDITOR_KEY),
               .percent_out = &values->editor_percent);
    const char *const reduce = cli_state_entry_value(args->entries, GUI_WINDOW_REDUCE_KEY);
    window->motion_saved =
        (reduce != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = reduce, .other = "true", .cap = 8ULL});
    const unsigned int colors =
        EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = window->palette);
    for (unsigned int each = 0u; (args->entries != NULL) && (each < args->entries->count); each += 1u)
    {
        const char *const entry = args->entries->entry[each].key;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = entry, .other = GUI_WINDOW_COLOR_KEY, .cap = 64ULL}))
        {
            continue;
        }
        for (unsigned int at = 0u; at < colors; at += 1u)
        {
            char name[GUI_PREFERENCES_TEXT];
            EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = window->palette, .at = at, .name = name,
                       .size = sizeof(name));
            if (!cellul.eq(&(CatenaFinitaCfg){
                    .src = name, .other = entry + sizeof(GUI_WINDOW_COLOR_KEY) - 1u, .cap = sizeof(name)}))
            {
                continue;
            }
            const char *const text = args->entries->entry[each].value;
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
            }
            break;
        }
    }
    EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 1);
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
    CliItem item;
    if (cli_menus_keyed(args->input->name, &item) &&
        cellul.eq(&(CatenaFinitaCfg){.src = item.command, .other = "preferences", .cap = CLI_LABEL_MAX_LEN}))
    {
        EMBED_CALL(window_preferences_toggle, GuiWindowCtx, .window = window);
        return;
    }
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
    if (args->window->preferences.displayed)
    {
        EMBED_CALL(window_preferences_toggle, GuiWindowCtx, .window = args->window);
    }
}

// the panels read again of the servers' panels: the hover and the completion where they stand, and the signature
EMBED_INLINE void window_panels_take(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    window->hover.panel = cli_servers_panels_hover(&window->hover_panel) ? &window->hover_panel : NULL;
    window->suggest.panel = cli_servers_panels_completion(&window->completion_panel) ? &window->completion_panel : NULL;
    window->signature.panel = cli_servers_panels_signature();
    window->pending = 1;
}

// every panel over the editor hidden
EMBED_INLINE void window_panels_hide(const GuiWindowCtx *args)
{
    cli_servers_panels_hide();
    EMBED_CALL(window_panels_take, GuiWindowCtx, .window = args->window);
}

// whether the editor takes the keys set as `on` asks, never while no view is shown, the claim taken or let go with it
EMBED_INLINE void window_editing(const GuiWindowCtx *args)
{
    args->window->editing = args->on && (cli_view_shown() >= 0);
    EMBED_CALL(window_autohide, GuiWindowCtx, .window = args->window, .on = args->window->editing);
    args->window->pending = 1;
}

// the menus open laid again where they now stand, a frame owed
EMBED_INLINE void window_menus_laid(const GuiWindowCtx *args)
{
    EMBED_CALL(gui_menubar_bar.menus_layout, GuiMenubarBarCfg, .menubar = &args->window->menubar,
               .glyphs = &args->window->glyphs, .frame = &args->window->frame);
    args->window->pending = 1;
}

// the session of the views saved in orior's folder, and the strip's group where the tool shown has the side
EMBED_INLINE void window_session_save(const GuiWindowCtx *args)
{
    cli_view_session_save(cli_command_line_root());
    const int tool = args->window->strip.displayed;
    // the tool shown is among the tools
    if ((tool >= 0) && g_cli_tools[tool].side)
    {
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = GUI_WINDOW_GROUP_KEY, .value = g_cli_tools[tool].name);
    }
}

// another view shown, or none: the panels hidden, its frame taken, the editor let go of the keys where none is, the
// session saved
EMBED_INLINE void window_shown(const GuiWindowCtx *args)
{
    EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = args->window);
    EMBED_CALL(window_view_take, GuiWindowCtx, .window = args->window, .edited = 1);
    if (cli_view_shown() < 0)
    {
        EMBED_CALL(window_editing, GuiWindowCtx, .window = args->window, .on = 0);
    }
    EMBED_CALL(window_session_save, GuiWindowCtx, .window = args->window);
}

// `view_input` handed to the view shown, its frame taken anew; `edited` where it may have edited the text
EMBED_INLINE void window_view_input(const GuiWindowCtx *args)
{
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    if (key == NULL)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = args->view_input,
                     .err_msg = &err_msg);
    EMBED_CALL(window_view_take, GuiWindowCtx, .window = args->window, .edited = args->edited);
}

// the view at `at` shown, the one shown before it saved
EMBED_INLINE void window_tab_show(const GuiWindowCtx *args)
{
    if (args->at < cli_view_count())
    {
        cli_view_show(args->at);
        EMBED_CALL(window_shown, GuiWindowCtx, .window = args->window);
    }
}

// the view at `at` closed and its text let go, a view with changes not saved closing on a second close, as tabs.js
// closes one; the view after it shown where it was the one shown
EMBED_INLINE void window_tab_close(const GuiWindowCtx *args)
{
    const char *const key = cli_view_key(args->at);
    if (key == NULL)
    {
        return;
    }
    if (EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = key) && !cli_view_closing(args->at))
    {
        cli_view_closing_set(args->at, 1);
        args->window->pending = 1;
        return;
    }
    cli_view_file_close(args->at);
    EMBED_CALL(window_shown, GuiWindowCtx, .window = args->window);
}

// every view closed once, the last first, so those before it stand where they stood: a changed one stands, its close
// waiting on a second
EMBED_INLINE void window_tabs_close(const GuiWindowCtx *args)
{
    for (unsigned int at = cli_view_count(); at > 0u; at -= 1u)
    {
        EMBED_CALL(window_tab_close, GuiWindowCtx, .window = args->window, .at = at - 1u);
    }
}

// the view at `at` saved to its file as the tree writes every file, its close no longer waiting: the save a break for
// the notices waiting, or a write that failed posted as a failure's notice
EMBED_INLINE void window_tab_save(const GuiWindowCtx *args)
{
    const char *const key = cli_view_key(args->at);
    const char *const root = cli_command_line_root();
    if ((key == NULL) || (root == NULL))
    {
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = cli_view_path_at(args->at),
               .joined = full);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    cli_view_closing_set(args->at, 0);
    if (EMBED_CALL(cli_view_save_table.save, CliViewSaveCfg, .key = key, .full = full, .err_msg = &err_msg))
    {
        cli_notices_break();
    }
    else
    {
        cli_notice_post((mmgr_line_finish(&err_msg) != 0ULL) ? why : "The file could not be written.", 1);
    }
    args->window->pending = 1;
}

// every view with changes not saved saved
EMBED_INLINE void window_tabs_save(const GuiWindowCtx *args)
{
    const unsigned int count = cli_view_count();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = cli_view_key(at)))
        {
            EMBED_CALL(window_tab_save, GuiWindowCtx, .window = args->window, .at = at);
        }
    }
}

// the file at `name` opened as a view and shown, its cursor at `place` where it is not NULL, and else where the path's
// line and column put it; a file refused posted as a failure's notice
EMBED_INLINE void window_file_open(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (cli_view_file_open(cli_command_line_root(), args->name, window->rows, window->columns, args->place, &err_msg) <
        0)
    {
        cli_notice_post((mmgr_line_finish(&err_msg) != 0ULL) ? why : "The file could not be opened.", 1);
        return;
    }
    EMBED_CALL(window_shown, GuiWindowCtx, .window = window);
}

// the tool `at` of the strip shown, or hidden where it shows; the terminal's panel opened or closed beside whatever
// tool shows; the explorer listed as it shows
EMBED_INLINE void window_tool_toggle(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if ((args->at >= g_cli_tools_count) || g_cli_tools[args->at].menu)
    {
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[args->at].name, .other = "terminal", .cap = 16ULL}))
    {
        (void)cli_terminal_tabs_command("toggle", NULL, 0u);
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
        return;
    }
    // a side tool's pane shown, or collapsed where it shows; another tool's window shown or hidden; the tools are
    // fewer than an int counts
    if (g_cli_tools[args->at].side)
    {
        EMBED_CALL(gui_sides.toggle, GuiSidesCfg, .sides = &window->sides, .strip = &window->strip, .at = args->at,
                   .still = window->motion_still, .time_ms = cli_clock_milliseconds());
    }
    else
    {
        window->strip.displayed = (window->strip.displayed == (int)args->at) ? -1 : (int)args->at;
        window->strip.collapsed = 0;
    }
    EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
    EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
    if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .key = "explorer"))
    {
        cli_files_explorer_show(cli_command_line_root());
    }
    EMBED_CALL(window_session_save, GuiWindowCtx, .window = window);
    EMBED_CALL(window_side, GuiWindowCtx, .window = window);
}

// whether `name` is among `count` names
EMBED_INLINE int window_name_among(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->words[at], .cap = CLI_LABEL_MAX_LEN}))
        {
            return 1;
        }
    }
    return 0;
}

// whether `name` is `key`
EMBED_INLINE int window_is(const GuiWindowCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->key, .cap = CLI_LABEL_MAX_LEN});
}

// the menus' answer to whether an item cannot be chosen: it needs a file open while none is, text chosen in the
// terminal while none is, the view shown changed while it is not or any view changed while none is, or it is never
// chosen
static int window_item_disabled(const CliItem *item)
{
    const char *const needs = item->needs;
    const int needs_editor = EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "editor") ||
                             EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "tab");
    const int needs_chosen = EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "terminal-chosen");
    const int needs_shown_changed = EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "changed-active");
    const int needs_changed = EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "changed");
    const int never = EMBED_CALL(window_is, GuiWindowCtx, .name = needs, .key = "never");
    int changed = 0;
    const unsigned int count = cli_view_count();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        changed = changed || EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = cli_view_key(at));
    }
    const char *const shown = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    const int shown_changed = (shown != NULL) && EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = shown);
    return never || (needs_editor && (count == 0u)) || (needs_chosen && !cli_terminal_tabs_checked(needs)) ||
           (needs_shown_changed && !shown_changed) || (needs_changed && !changed);
}

// the menus' answer to the state an item shows: Reduce Motion by the saved setting or the system's wish, Focus while
// it stops the notices, and the terminal's own
static int window_item_checked(const CliItem *item)
{
    const char *const checks = item->checks;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = checks, .key = GUI_WINDOW_REDUCE_KEY))
    {
        return (s_window != NULL) && (s_window->motion_saved || s_window->motion_system);
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = checks, .key = "focus"))
    {
        return cli_notices_focused();
    }
    return cli_terminal_tabs_checked(checks);
}

static void window_key_take(const GuiWindowCtx *args);

// the command `name` run with its `count` `words`, its keys `value`, whoever asked for it
EMBED_INLINE void window_command_run(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const name = args->name;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "exit"))
    {
        window->closing = 1;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preferences"))
    {
        EMBED_CALL(window_preferences_toggle, GuiWindowCtx, .window = window);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "close-editor"))
    {
        // the view shown is among the views
        const int shown = cli_view_shown();
        if (shown >= 0)
        {
            EMBED_CALL(window_tab_close, GuiWindowCtx, .window = window, .at = (unsigned int)shown);
        }
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "close-all"))
    {
        EMBED_CALL(window_tabs_close, GuiWindowCtx, .window = window);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "last-editor"))
    {
        // the view shown before is among the views
        const int last = cli_view_last();
        if (last >= 0)
        {
            EMBED_CALL(window_tab_show, GuiWindowCtx, .window = window, .at = (unsigned int)last);
        }
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "save"))
    {
        // the view shown is among the views
        const int shown = cli_view_shown();
        if (shown >= 0)
        {
            EMBED_CALL(window_tab_save, GuiWindowCtx, .window = window, .at = (unsigned int)shown);
        }
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "save-all"))
    {
        EMBED_CALL(window_tabs_save, GuiWindowCtx, .window = window);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "focus"))
    {
        cli_notices_focus(!cli_notices_focused());
        window->pending = 1;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "notice-show-again"))
    {
        if (args->count != 0u)
        {
            cli_notices_show_again(args->words[0]);
        }
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = GUI_WINDOW_REDUCE_KEY))
    {
        // on or off by its word, else turned from what shows, the saved setting or the system's wish
        const char *const word = (args->count != 0u) ? args->words[0] : "";
        const int on = EMBED_CALL(window_is, GuiWindowCtx, .name = word, .key = "on");
        const int off = EMBED_CALL(window_is, GuiWindowCtx, .name = word, .key = "off");
        window->motion_saved = on || (!off && !(window->motion_saved || window->motion_system));
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = GUI_WINDOW_REDUCE_KEY,
                   .value = window->motion_saved ? "true" : "false");
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 0);
    }
    else if (cli_terminal_tabs_command(name, args->words, args->count))
    {
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
    }
    else if ((args->value != NULL) && (args->value[0] != '\0'))
    {
        EMBED_CALL(window_key_take, GuiWindowCtx, .window = window, .name = args->value, .on = 0);
    }
    else if (EMBED_CALL(window_name_among, GuiWindowCtx, .name = name, .words = s_view_commands,
                        .count = sizeof(s_view_commands) / sizeof(s_view_commands[0])))
    {
        EMBED_CALL(window_key_take, GuiWindowCtx, .window = window, .name = name, .on = 0);
    }
}

// a command the command line asks for, through the door or as the window started: the file a path names opened, or a
// command of the menus, the window's or the view's run: 1, or 0 where it is none of them
static int window_command_named(const char *command, const char *const *words, unsigned int count)
{
    GuiWindow *const window = s_window;
    if (window == NULL)
    {
        return 0;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = command, .key = "file"))
    {
        if (count != 0u)
        {
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = words[0], .place = NULL);
        }
        return count != 0u;
    }
    CliItem item;
    const int keyed = cli_menus_named(command, &item);
    const int known = keyed ||
                      EMBED_CALL(window_name_among, GuiWindowCtx, .name = command, .words = s_own_commands,
                                 .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])) ||
                      EMBED_CALL(window_name_among, GuiWindowCtx, .name = command, .words = s_view_commands,
                                 .count = sizeof(s_view_commands) / sizeof(s_view_commands[0]));
    if (known)
    {
        EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = command,
                   .value = keyed ? item.keys : NULL, .words = words, .count = count);
    }
    return known;
}

// the tool whose keys are the key `name`, -1 for none
EMBED_INLINE int window_tool_key(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_tools_count; at += 1u)
    {
        const char *const keys = g_cli_tools[at].keys;
        if ((keys[0] != '\0') && cli_menus_keys_name(keys, args->name))
        {
            // the tools are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the key `name` taken, `on` where the name's pane stands and no view takes it: the menu key opens the terminal's menu
// at its key field while the panel takes the keys; the panel takes them ahead of the window while no menu is open; the
// menus', a command's own, a tool's, the panels' and else the view's
static void window_key_take(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const name = args->name;
    if ((g_cli_menus_open.levels == 0u) && !args->on &&
        EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "ContextMenu") &&
        cli_terminal_tabs_checked("terminal-keys"))
    {
        cli_menus_own_open(cli_terminal_tabs_menu(), CLI_TERMINAL_MENU_ROWS, CLI_MENU_PLACE_TERMINAL_KEYS, 0, 0);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        return;
    }
    if ((g_cli_menus_open.levels == 0u) && !args->on && cli_terminal_tabs_key(name))
    {
        EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
        return;
    }
    CliItem chosen;
    int chose = 0;
    if (cli_menus_key(name, &chosen, &chose))
    {
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        if (chose)
        {
            EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = chosen.command,
                       .value = chosen.keys);
        }
        return;
    }
    if (args->on)
    {
        return;
    }
    CliItem item;
    if (cli_menus_keyed(name, &item) &&
        EMBED_CALL(window_name_among, GuiWindowCtx, .name = item.command, .words = s_own_commands,
                   .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])))
    {
        EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = item.command, .value = item.keys);
        return;
    }
    const int tool = EMBED_CALL(window_tool_key, GuiWindowCtx, .name = name);
    if (tool >= 0)
    {
        // the tool is among the tools
        EMBED_CALL(window_tool_toggle, GuiWindowCtx, .window = window, .at = (unsigned int)tool);
        return;
    }
    if (cli_servers_panels_key(name))
    {
        EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
        return;
    }
    const CliViewInput keyed = {.kind = CLI_VIEW_KEY, .name = name};
    EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &keyed, .edited = 1);
}

// from any of the walk's threads: the walk colored lines, the frame, the map and the names taken again as the view's
// thread wakes
static void window_colored(void)
{
    EMBED_ATOMIC_STORE(&s_colored, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

// what the name does in this session: it stands on the first start in it, and goes home on any start after; the
// session saved is `seconds logon`, the boot time reading a few seconds either way from one start to the next
EMBED_INLINE void window_wordmark_read(const GuiWindowCtx *args)
{
    char logon[GUI_WINDOW_LOGON_MAX_LEN];
    EMBED_CALL(cli_capacity_logon_internal.logon_by_name, CliCapacityLogonCtx, .logon = logon,
               .capacity_bytes = sizeof(logon));
    const unsigned long long booted = cli_booted_seconds();
    const char *const saved = cli_state_entry_value(args->entries, GUI_WINDOW_WORDMARK_KEY);
    int same = 0;
    if (saved != NULL)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL});
        unsigned long long end = 0ULL;
        const unsigned long long then =
            EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = saved, .at = 0ULL, .size = size, .end = &end);
        const unsigned long long apart = (then > booted) ? (then - booted) : (booted - then);
        same = (end < size) && (saved[end] == ' ') && (apart < GUI_WINDOW_SESSION_SECONDS) &&
               cellul.eq(&(CatenaFinitaCfg){.src = saved + end + 1ULL, .other = logon, .cap = sizeof(logon)});
    }
    if (same)
    {
        EMBED_CALL(cli_wordmark_still.land, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    }
    else
    {
        EMBED_CALL(gui_wordmark_frame.stand, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark);
    }
    char text[GUI_WINDOW_LOGON_MAX_LEN + 32u];
    VerbaLinea now = {text, sizeof(text), 0ULL};
    mmgr_line_decimal(&now, booted, 1u);
    mmgr_line_ch(&now, ' ');
    mmgr_line_put(&now, logon);
    if (mmgr_line_finish(&now) != 0ULL)
    {
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = GUI_WINDOW_WORDMARK_KEY, .value = text);
    }
}

// the strip's group saved shown, the tool of that name where its window is the side
EMBED_INLINE void window_group_read(const GuiWindowCtx *args)
{
    const char *const group = cli_state_entry_value(args->entries, GUI_WINDOW_GROUP_KEY);
    for (unsigned int at = 0u; (group != NULL) && (at < g_cli_tools_count); at += 1u)
    {
        if (g_cli_tools[at].side &&
            cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = group, .cap = 32ULL}))
        {
            // the tools are fewer than an int counts
            args->window->strip.displayed = (int)at;
        }
    }
    if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = args->window, .key = "explorer"))
    {
        cli_files_explorer_show(cli_command_line_root());
    }
}

void gui_window_start(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    s_window = window;
    s_woken = args->woken;
    window->view_shown = -1;
    const char *const root = cli_command_line_root();
    window->title = cli_command_line_tree_name();
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = window_colored, .threaded = 1);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg);
    g_cli_menus_open.disabled = window_item_disabled;
    g_cli_menus_open.checked = window_item_checked;
    cli_servers_problems_begin(args->woken);
    cli_servers_panels_begin(root);
    char folder[CLI_PATH_MAX_LEN];
    // the entries stand until the window ends, read in place by the modules that restore from them
    CliStateEntries *const entries =
        EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder)
            ? cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = &g_cli_view_cells})
            : NULL;
    window->entries = entries;
    EMBED_CALL(window_settings_read, GuiWindowCtx, .window = window, .entries = entries);
    EMBED_CALL(window_wordmark_read, GuiWindowCtx, .window = window, .entries = entries);
    cli_notices_restore(entries);
    cli_terminal_tabs_restore(entries, root);
    EMBED_CALL(window_terminal, GuiWindowCtx, .window = window, .starting = 1);
    if (root != NULL)
    {
        EMBED_CALL(window_group_read, GuiWindowCtx, .window = window, .entries = entries);
        cli_view_session_restore(root, entries, window->rows, window->columns);
    }
    // the command the window was started with run as the menus run it
    const CliOutcome *const launch = cli_command_line_launched();
    if (launch->command[0] != '\0')
    {
        const char *words[CLI_LAUNCH_ARGS];
        for (unsigned int at = 0u; at < launch->args; at += 1u)
        {
            words[at] = EMBED_CALL(cli_command_line_table.launch_arg, CliCommandLineCfg, .launch = launch, .at = at);
        }
        (void)window_command_named(launch->command, words, launch->args);
    }
    EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
    EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
    // the pane in sight collapses a rest after the window begins, the pointer never coming to it
    EMBED_CALL(gui_sides.settle, GuiSidesCfg, .sides = &window->sides, .time_ms = cli_clock_milliseconds());
    if (window->wordmark.exists && !window->wordmark.tree_read)
    {
        EMBED_CALL(cli_wordmark_still.tree_read, GuiWordmarkStillCfg, .wordmark = &window->wordmark);
    }
    window->pending = 1;
}

void gui_window_step(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    window->pending = cli_reach_take(window_command_named) || window->pending;
    if (cli_servers_problems_step())
    {
        window->strip.problems_marked = cli_servers_problems_counts().errors != 0u;
        window->pending = 1;
    }
    if (cli_servers_panels_step())
    {
        EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
    }
    if (cli_terminal_tabs_step())
    {
        EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
    }
    window->pending = cli_notices_step() || window->pending;
    if (cli_menus_rest())
    {
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
    }
    if (EMBED_ATOMIC_LOAD(&s_colored) != 0u)
    {
        EMBED_ATOMIC_STORE(&s_colored, 0u);
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (now_ms >= window->memory_due_ms)
    {
        CliProcessMemory measured;
        window->memory_bytes =
            EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx, .pid = cli_process_own(), .memory = &measured)
                ? measured.working
                : 0ULL;
        window->memory_due_ms = now_ms + GUI_MEMORY_MS;
        window->pending = 1;
    }
    EMBED_CALL(gui_busy_running.work, GuiBusyRunningCfg, .busy = &window->busy,
               .running = cli_servers_problems_checking());
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

// what the status bar shows, from the view shown, the editor's frame as last taken, the notices and the window's own
EMBED_INLINE GuiStatus window_status_read(const GuiWindowCtx *args)
{
    const CliViewFrame *const shown = args->window_read->editor.frame;
    const int at = cli_view_shown();
    // the view shown is among the views
    const char *const path = (at >= 0) ? cli_view_path_at((unsigned int)at) : NULL;
    const char *const key = (at >= 0) ? cli_view_key((unsigned int)at) : NULL;
    const char *const notice = cli_notice_shown();
    const CliPosition none = {0ULL, 0ULL};
    const CliPosition cursor = (shown != NULL) ? shown->cursor : none;
    const CliPosition anchor = (shown != NULL) ? shown->anchor : none;
    return (GuiStatus){(path != NULL) ? path : "",
                       shown != NULL,
                       cursor.line,
                       cursor.col,
                       anchor.line,
                       anchor.col,
                       (key != NULL) && EMBED_CALL(cli_document.crlf, CliDocumentCfg, .key = key),
                       (shown != NULL) ? shown->indent_tabs : -1,
                       (shown != NULL) ? shown->indent_size : 4u,
                       args->window_read->memory_bytes,
                       (notice != NULL) ? notice : "",
                       cli_notice_failed(),
                       cli_notices_focused(),
                       cli_notices_waiting()};
}

// the view drawn: the tabs row, the editor with the map beside it, and the side where one is shown, each on the ground
// the base laid it, and the panels over them
EMBED_INLINE void window_view_draw(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame);
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
               .palette = palette, .editor = &args->window->editor, .place = frame->map_whole, .clip = frame->map,
               .scale = frame->scale,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = args->window));
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->side) &&
        (args->window->strip.displayed >= 0))
    {
        // the tool shown is among the tools
        const CliTool *const tool = &g_cli_tools[args->window->strip.displayed];
        // the pane drawn where it stands as it slides, the side its rectangle moved by its way gone
        GuiFrame slid = *frame;
        slid.side = frame->side_pane;
        EMBED_CALL(gui_explorer_frame_xy.draw, GuiExplorerFrameXYCfg, .explorer_read = &args->window->explorer,
                   .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette,
                   .frame = &slid, .title = tool->label,
                   .folders = cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = "explorer", .cap = 32ULL}),
                   .tree = (args->window->title != NULL) ? args->window->title : "");
        if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = args->window, .key = "problems"))
        {
            EMBED_CALL(gui_problems_row_frame.draw, GuiProblemsRowFrameCfg, .problems = &args->window->problems,
                       .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = &slid);
        }
    }
    // the panel drawn where it stands as it arrives, its rectangle and its views' moved down by its way left
    GuiFrame arriving = *frame;
    arriving.terminal = frame->terminal_drawn;
    arriving.terminal_views = frame->terminal_views_drawn;
    EMBED_CALL(gui_terminal.draw, GuiTerminalCfg, .terminal_read = &args->window->terminal, .frame = &arriving,
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
               .palette = palette, .frame = frame, .title = (args->window->title != NULL) ? args->window->title : "",
               .table = table, .shadow = &args->window->controls_shadow);
    EMBED_CALL(gui_menubar_glyphs_frame.titles_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &args->window->menubar,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
    const GuiStatus status = EMBED_CALL(window_status_read, GuiWindowCtx, .window_read = args->window);
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
    // the caret last, at the ink its blink stands at, where no panel or menu lies over it: drawn over the frame
    // whole, its blink's flips draw it again alone; none stands while the view is not drawn
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
    const unsigned int levels = g_cli_menus_open.levels;
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

// what the pixel at x, y stands on written into `hit`: its hit, and its place among the rows, or its row and cell in
// the editor
EMBED_INLINE void window_hit_read(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    GuiWindowHit *const input = args->hit;
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
    // the status bar takes every press on it, those on none of its pressable items to nothing
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.status, .x = x, .y = y))
    {
        const GuiStatus status = EMBED_CALL(window_status_read, GuiWindowCtx, .window_read = window);
        input->hit =
            EMBED_CALL(gui_statusbar_x.hit, GuiStatusbarXCfg, .status = &status, .glyphs = (GuiGlyphs *)&window->glyphs,
                       .icons = (GuiIcons *)&window->icons, .frame = &window->frame, .x = x, .y = y);
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
    if (EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = (GuiGlyphs *)&window->glyphs,
                   .frame = &window->frame, .x = x, .y = y, .at_pointer = &input->index, .closing = &closing))
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

// whether Shift is down on the input
EMBED_INLINE int window_shift(const GuiInput *input)
{
    return (input->mods & GUI_MOD_SHIFT) != 0u;
}

// the right button pressed or the wheel turned at x, y: away from the menus open it closes them, as menu.js closes them
// on any press or wheel outside them, and on a menu or on the bar's titles while the bar's menu is open it does nothing
EMBED_INLINE void window_menus_away(const GuiWindowCtx *args)
{
    if (g_cli_menus_open.levels == 0u)
    {
        return;
    }
    GuiWindowHit pressed = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window, .x = args->x, .y = args->y, .hit = &pressed);
    if ((pressed.hit != CLI_HIT_MENU_ROW) && ((pressed.hit != CLI_HIT_MENU_TITLE) || (g_cli_menus_open.open < 0)))
    {
        EMBED_CALL(window_menus_close, GuiWindowCtx, .window = args->window);
    }
}

// the press a terminal's part names, its place among the terminal's hits, which stand in the order the window's do;
// 0 for a press on none of the terminal's parts
EMBED_INLINE unsigned int window_terminal_hit(const GuiWindowCtx *args)
{
    const unsigned int hit = args->hit->hit;
    return ((hit >= CLI_HIT_TERMINAL) && (hit <= CLI_HIT_TERMINAL_PANEL))
               ? (CLI_TERMINAL_HIT_PANE + (hit - CLI_HIT_TERMINAL))
               : 0u;
}

// the view shown scrolled to put the line `row` of `hit` at its top
EMBED_INLINE void window_view_scroll(const GuiWindowCtx *args)
{
    const CliViewInput scrolled = {.kind = CLI_VIEW_SCROLL, .top = args->hit->row};
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .view_input = &scrolled, .edited = 0);
}

// a press, on what the window's layout found under it at `hit`, `on` where Shift is down: the menus', the panels', the
// status bar's, the terminal's, and else the part's it stands on
EMBED_INLINE void window_pressed(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiWindowHit *const hit = args->hit;
    cli_notices_pointer();
    if (hit->hit == CLI_HIT_MENU_TITLE)
    {
        cli_menus_title_press(hit->index);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        return;
    }
    if (hit->hit == CLI_HIT_MENU_ROW)
    {
        CliItem chosen;
        const int chose =
            (hit->level < g_cli_menus_open.levels) && cli_menus_row_press(hit->level, hit->index, &chosen);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        if (chose)
        {
            EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = chosen.command,
                       .value = chosen.keys);
        }
        return;
    }
    // a press on neither closes every menu open
    EMBED_CALL(window_menus_close, GuiWindowCtx, .window = window);
    switch (hit->hit)
    {
    case CLI_HIT_SUGGEST_LIST:
    case CLI_HIT_HOVER_BOX:
    case CLI_HIT_SIGNATURE_BOX:
        return;
    case CLI_HIT_SUGGEST_ROW:
        cli_servers_panels_accept(hit->index);
        EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
        return;
    case CLI_HIT_STATUS_NOTICE:
    case CLI_HIT_STATUS_SILENCE:
    case CLI_HIT_STATUS_FOCUS:
        cli_notices_press(CLI_NOTICES_PRESS_NOTICE + (hit->hit - CLI_HIT_STATUS_NOTICE));
        window->pending = 1;
        return;
    default:
        break;
    }
    EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
    // a press on the terminal is the terminal's; one elsewhere takes the keys from it
    const unsigned int terminal_hit = EMBED_CALL(window_terminal_hit, GuiWindowCtx, .hit = (GuiWindowHit *)hit);
    if ((terminal_hit != 0u) || window->terminal.panel.keys)
    {
        const CliTerminalTabsPressCfg pressed = {terminal_hit,   hit->index, hit->row, hit->column,
                                                 window->clicks, args->on,   0};
        cli_terminal_tabs_press(&pressed);
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
    }
    if (terminal_hit != 0u)
    {
        return;
    }
    switch (hit->hit)
    {
    case CLI_HIT_WORDMARK: {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_report_err_msg.open_page, CliReportErrMsgCfg, .url = GUI_WINDOW_REPOSITORY,
                         .err_msg = &err_msg);
        break;
    }
    case CLI_HIT_TOOL:
        EMBED_CALL(window_tool_toggle, GuiWindowCtx, .window = window, .at = hit->index);
        break;
    case CLI_HIT_PROBLEMS_CHECK:
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        cli_servers_problems_check(cli_command_line_root());
        window->pending = 1;
        break;
    case CLI_HIT_PROBLEM_ROW: {
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        CliProblemRow row;
        if (cli_servers_problems_press(hit->index, &row))
        {
            const CliPosition place = {row.line, row.col};
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = row.path, .place = &place);
        }
        window->pending = 1;
        break;
    }
    case CLI_HIT_EXPLORER_ROW: {
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        CliExplorerRow row;
        const int listed = cli_files_explorer_row(hit->index, &row);
        if (!cli_files_explorer_press(hit->index) && listed && !row.dir)
        {
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = row.path, .place = NULL);
        }
        window->pending = 1;
        break;
    }
    case CLI_HIT_TAB:
        EMBED_CALL(window_tab_show, GuiWindowCtx, .window = window, .at = hit->index);
        break;
    case CLI_HIT_TAB_CLOSE:
        EMBED_CALL(window_tab_close, GuiWindowCtx, .window = window, .at = hit->index);
        break;
    case CLI_HIT_MAP:
        EMBED_CALL(window_view_scroll, GuiWindowCtx, .window = window, .hit = (GuiWindowHit *)hit);
        break;
    case CLI_HIT_EDITOR:
    case CLI_HIT_GUTTER: {
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
        const CliViewInput pointed = {.kind = CLI_VIEW_POINTER,
                                      .pointer = CLI_VIEW_POINTER_DOWN,
                                      .row = hit->row,
                                      .column = hit->column,
                                      .clicks = window->clicks,
                                      .shift = args->on,
                                      .gutter = hit->hit == CLI_HIT_GUTTER};
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &pointed, .edited = 0);
        break;
    }
    default:
        break;
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
    EMBED_CALL(window_menus_away, GuiWindowCtx, .window = window, .x = args->input->x, .y = args->input->y);
    // over an open menu the wheel scrolls its rows and nothing under it
    if (EMBED_CALL(gui_menubar_glyphs_frame_x.wheel, GuiMenubarGlyphsFrameXCfg, .menubar = &window->menubar,
                   .glyphs = &window->glyphs, .frame = &window->frame, .x = args->input->x, .y = args->input->y,
                   .rows = args->input->rows))
    {
        window->pending = 1;
        return;
    }
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
        cli_servers_panels_wheel(args->input->rows);
        EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
        return;
    }
    // over a terminal's pane, its view
    unsigned int pane = 0u;
    if (EMBED_CALL(gui_terminal.hit, GuiTerminalCfg, .terminal_read = &window->terminal, .frame = &window->frame,
                   .glyphs = (GuiGlyphs *)&window->glyphs, .x = args->input->x, .y = args->input->y,
                   .index = &pane) == CLI_HIT_TERMINAL)
    {
        cli_terminal_tabs_wheel(pane, args->input->rows);
        EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
        return;
    }
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    if ((key != NULL) && (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.editor,
                                     .x = args->input->x, .y = args->input->y) ||
                          EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.map,
                                     .x = args->input->x, .y = args->input->y)))
    {
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
        const CliViewInput wheeled = {
            .kind = CLI_VIEW_WHEEL, .wheel_rows = args->input->rows, .top = CLI_VIEW_WHEEL_UNBOUND};
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &wheeled, .edited = 0);
    }
}

// a press of the middle button: on an editor's tab it closes the tab, and on a terminal's tab it ends the tab's shell
// and closes the tab, each as its × does; elsewhere it does nothing
EMBED_INLINE void window_middle_press(const GuiWindowCtx *args)
{
    GuiWindowHit pressed = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window, .x = args->x, .y = args->y, .hit = &pressed);
    if ((pressed.hit == CLI_HIT_TAB) || (pressed.hit == CLI_HIT_TAB_CLOSE))
    {
        pressed.hit = CLI_HIT_TAB_CLOSE;
        EMBED_CALL(window_pressed, GuiWindowCtx, .window = args->window, .hit = &pressed, .on = 0);
    }
    else if (pressed.hit == CLI_HIT_TERMINAL_TAB)
    {
        pressed.hit = CLI_HIT_TERMINAL_TAB_CLOSE;
        EMBED_CALL(window_pressed, GuiWindowCtx, .window = args->window, .hit = &pressed, .on = 0);
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
// frame lays it, handed to the terminal where it moved
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
    if ((unsigned int)height != window->terminal_height)
    {
        window->terminal_height = (unsigned int)height;
        cli_terminal_tabs_height((unsigned int)height);
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
    }
}

static void window_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    if (input->button == GUI_BUTTON_MIDDLE)
    {
        EMBED_CALL(window_middle_press, GuiWindowCtx, .window = window, .x = input->x, .y = input->y);
        return;
    }
    if (input->button == GUI_BUTTON_RIGHT)
    {
        EMBED_CALL(window_menus_away, GuiWindowCtx, .window = window, .x = input->x, .y = input->y);
        return;
    }
    if (EMBED_CALL(window_terminal_grip, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y))
    {
        window->terminal_gripped = 1;
        window->terminal_height = 0u;
        return;
    }
    const unsigned int hit = EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input);
    window->pressed =
        ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE)) ? hit : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
    GuiWindowHit pressed = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y, .hit = &pressed);
    // a press on the map goes to that part of the text and may begin the slider's drag, the top worked out from what
    // the map shows
    unsigned long long top = 0ULL;
    if ((pressed.hit == CLI_HIT_NONE) &&
        EMBED_CALL(gui_minimap_position_scale.press, GuiMinimapPositionScaleCfg, .map = &window->minimap,
                   .place = window->frame.map_whole, .clip = window->frame.map, .scale = window->frame.scale,
                   .editor = &window->editor,
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
    window->dragging = (pressed.hit == CLI_HIT_EDITOR) || (pressed.hit == CLI_HIT_GUTTER);
    window->terminal_dragged = (pressed.hit == CLI_HIT_TERMINAL) ? (pressed.index + 1u) : 0u;
    window->moved = pressed;
    // a press on the icon a preview shows ends the preview with its pane standing and does nothing more
    if (EMBED_CALL(gui_sides.press, GuiSidesCfg, .sides = &window->sides, .strip = &window->strip,
                   .frame = &window->frame, .x = input->x, .y = input->y))
    {
        return;
    }
    EMBED_CALL(window_pressed, GuiWindowCtx, .window = window, .hit = &pressed, .on = window_shift(input));
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
        GuiWindowHit moved = {CLI_HIT_TERMINAL, 0u, window->terminal_dragged - 1u, 0ULL, 0ULL};
        if (EMBED_CALL(gui_terminal.cell_at, GuiTerminalCfg, .terminal_read = &window->terminal,
                       .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .view = moved.index,
                       .x = args->input->x, .y = args->input->y, .row = &moved.row, .column = &moved.column) &&
            ((moved.row != window->moved.row) || (moved.column != window->moved.column)))
        {
            window->moved = moved;
            const CliTerminalTabsPressCfg pointed = {CLI_TERMINAL_HIT_PANE, moved.index, moved.row, moved.column,
                                                     window->clicks,        0,           0};
            if (cli_terminal_tabs_pointer(&pointed))
            {
                EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
            }
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
    // the side pane's waits begun and ended by where the pointer stands, and a preview it has left taken back
    if (EMBED_CALL(gui_sides.move, GuiSidesCfg, .sides = &window->sides, .strip = &window->strip,
                   .still = window->motion_still, .frame = &window->frame, .x = args->input->x, .y = args->input->y,
                   .time_ms = cli_clock_milliseconds()))
    {
        EMBED_CALL(window_sides_laid, GuiWindowCtx, .window = window);
    }
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
            GuiWindowHit dragged = {CLI_HIT_MAP, 0u, 0u, top, 0ULL};
            EMBED_CALL(window_view_scroll, GuiWindowCtx, .window = window, .hit = &dragged);
        }
        return;
    }
    GuiWindowHit moved = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
               .hit = &moved);
    // a pointer that moved on no cell and no row brings nothing new
    if ((moved.hit == window->moved.hit) && (moved.level == window->moved.level) &&
        (moved.index == window->moved.index) && (moved.row == window->moved.row) &&
        (moved.column == window->moved.column))
    {
        return;
    }
    window->moved = moved;
    // the menus mark the title or the row the pointer moved onto
    if (moved.hit == CLI_HIT_MENU_TITLE)
    {
        cli_menus_title_move(moved.index);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
    }
    else if (moved.hit == CLI_HIT_MENU_ROW)
    {
        cli_menus_row_move(moved.level, moved.index);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
    }
    // a drag in the editor chooses in it; else the panels follow the pointer
    if (window->dragging)
    {
        const CliViewInput pointed = {.kind = CLI_VIEW_POINTER,
                                      .pointer = CLI_VIEW_POINTER_MOVED,
                                      .row = moved.row,
                                      .column = moved.column,
                                      .clicks = window->clicks,
                                      .gutter = moved.hit == CLI_HIT_GUTTER};
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &pointed, .edited = 0);
        return;
    }
    const CliServersPanelsPointerCfg pointer = {moved.hit == CLI_HIT_EDITOR, moved.row, moved.column,
                                                moved.hit == CLI_HIT_HOVER_BOX};
    cli_servers_panels_pointer(&pointer);
    EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
}

static void window_leave(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const int adjacent_before = window->claim.adjacent;
    EMBED_CALL(cli_autohide_still.leave, GuiAutohideStillCfg, .claim = &window->claim);
    EMBED_CALL(gui_sides.leave, GuiSidesCfg, .sides = &window->sides, .time_ms = cli_clock_milliseconds());
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
        GuiWindowHit released = {CLI_HIT_TERMINAL, 0u, window->terminal_dragged - 1u, 0ULL, 0ULL};
        (void)EMBED_CALL(gui_terminal.cell_at, GuiTerminalCfg, .terminal_read = &window->terminal,
                         .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .view = released.index,
                         .x = args->input->x, .y = args->input->y, .row = &released.row, .column = &released.column);
        window->terminal_dragged = 0u;
        const CliTerminalTabsPressCfg pointed = {
            CLI_TERMINAL_HIT_PANE, released.index, released.row, released.column, window->clicks, 0, 1};
        if (cli_terminal_tabs_pointer(&pointed))
        {
            EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
        }
    }
    if (window->minimap.dragging)
    {
        EMBED_CALL(gui_minimap.release, GuiMinimapCfg, .map = &window->minimap);
        window->pending = 1;
    }
    if (window->dragging)
    {
        window->dragging = 0;
        GuiWindowHit released = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
        EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
                   .hit = &released);
        const CliViewInput pointed = {.kind = CLI_VIEW_POINTER,
                                      .pointer = CLI_VIEW_POINTER_UP,
                                      .row = released.row,
                                      .column = released.column,
                                      .clicks = window->clicks,
                                      .gutter = released.hit == CLI_HIT_GUTTER};
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &pointed, .edited = 0);
    }
}

static void window_key(const GuiInputHandlerCfg *args)
{
    GuiWindow *const window = args->window;
    cli_notices_key(args->input->name, window->editing);
    EMBED_CALL(window_key_take, GuiWindowCtx, .window = window, .name = args->input->name,
               .on = window->wordmark.exists);
}

// the keys gone to another window: the menus open closed
static void window_blur(const GuiInputHandlerCfg *args)
{
    EMBED_CALL(window_menus_close, GuiWindowCtx, .window = args->window);
}

// the system's gesture for a menu: the menu of what stands at the pointer asked for there, none on a menu open, the
// menus open closed first; the terminal's panel answers for every place on it
static void window_menu(const GuiInputHandlerCfg *args)
{
    GuiWindow *const window = args->window;
    GuiWindowHit asked = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
               .hit = &asked);
    if (asked.hit == CLI_HIT_MENU_ROW)
    {
        return;
    }
    EMBED_CALL(window_menus_close, GuiWindowCtx, .window = window);
    if ((asked.hit >= CLI_HIT_TERMINAL) && (asked.hit <= CLI_HIT_TERMINAL_PANEL))
    {
        cli_menus_own_open(cli_terminal_tabs_menu(), CLI_TERMINAL_MENU_ROWS, CLI_MENU_PLACE_POINT, args->input->x,
                           args->input->y);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
    }
}

static void window_text(const GuiInputHandlerCfg *args)
{
    GuiWindow *const window = args->window;
    cli_notices_input();
    // while the name's pane stands no view takes the text
    if (window->wordmark.exists)
    {
        return;
    }
    if (cli_terminal_tabs_text(args->input->text, args->input->size))
    {
        EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = window);
        return;
    }
    const CliViewInput typed = {.kind = CLI_VIEW_TEXT, .text = args->input->text, .length = args->input->size};
    EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
    cli_servers_panels_text(args->input->text, args->input->size);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &typed, .edited = 1);
    EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
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
