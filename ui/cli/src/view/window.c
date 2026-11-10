// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the window on the view's thread. It finds what a pixel stands on and hands each input straight to the
// module it is for: the menus and the notices of the command line, the views of the editor and their files, the
// explorer's listing, the servers' panels and problems, and the terminal's tabs; each part draws what its module
// stores, read where it stands. What is the window's own is what moves on the screen, the strip's tool shown, whether
// the editor takes the keys, the settings it draws with and the rows' scroll offsets and the marks the pointer leaves
// on them
#include "window.h"
#include "../code/crumbs.h"
#include "../code/diff_view.h"
#include "../code/git.h"
#include "../command_line/callback_cfg.h"

#include "../command_line/command_line.h"
#include "../command_line/pool.h"
#include "../comms/comms.h"
#include "../debug/debug_pane.h"
#include "../editor/files.h"
#include "../editor/hunks.h"
#include "../editor/view_edits.h"
#include "../machine/reach.h"
#include "../machine/report.h"
#include "../machine/settings.h"
#include "../machine/state.h"
#include "../plugins/plugins.h"
#include "../preview/preview.h"
#include "../runs/job_runs.h"
#include "../runs/qry.h"
#include "../tests/testing.h"
#include "../text/text.h"
#include "parts/change_peek.h"
#include "parts/changes.h"
#include "parts/chrome.h"
#include "parts/comms.h"
#include "parts/crumbs.h"
#include "parts/diffview.h"
#include "parts/git.h"
#include "parts/gutter.h"
#include "parts/preview.h"
#include "parts/run.h"
#include "parts/runs.h"

#include <math.h>

// orior's repository, a press on the name opening it; the keys of orior's folder the settings, the session's wordmark
// and the strip's group stand under; and two starts of a session, the seconds their boot times may read apart, and the
// most bytes of a log in
#define GUI_WINDOW_REPOSITORY "https://github.com/dstroy0/orior"
#define GUI_WINDOW_REDUCE_KEY "reduce-motion"
#define GUI_WINDOW_AUTHORS_KEY "git-authors"
#define GUI_WINDOW_ZOOM_KEY "zoom"
#define GUI_WINDOW_BAR_KEY "opacity.bar"
#define GUI_WINDOW_EDITOR_KEY "opacity.editor"
#define GUI_WINDOW_COLOR_KEY "color."
#define GUI_WINDOW_KEYS_KEY "keys."
#define GUI_WINDOW_WORDMARK_KEY "wordmark.session"
#define GUI_WINDOW_GROUP_KEY "panes.group"
#define GUI_WINDOW_SESSION_SECONDS 60ULL
#define GUI_WINDOW_LOGON_MAX_LEN 128u
#define GUI_WINDOW_MESSAGE_MAX 8192u
// the scheme's opacities of the bar and the editor, in percent, where none is saved
#define GUI_WINDOW_BAR_PERCENT 30u
#define GUI_WINDOW_EDITOR_PERCENT 80u
// the most items one node of the Preferences menu lists
#define GUI_WINDOW_PREFERENCE_ITEMS 128u

// the commands the window has no keys for that the view runs by name, and the commands that run here whatever else
// their keys might mean, the terminal's among them
static const char *const s_view_commands[] = {"sort-ascending", "sort-descending", "unique-lines",
                                              "upper-case",     "lower-case",      "title-case"};
static const char *const s_own_commands[] = {"exit",
                                             "close-editor",
                                             "close-all",
                                             "last-editor",
                                             "reduce-motion",
                                             "preferences",
                                             "terminal-view",
                                             "toggle",
                                             "new",
                                             "split",
                                             "clear",
                                             "kill",
                                             "terminal-copy",
                                             "terminal-paste",
                                             "terminal-close",
                                             "save",
                                             "save-all",
                                             "git-authors",
                                             "file-open",
                                             "file-copy-path",
                                             "explorer-refresh",
                                             "explorer-collapse",
                                             "tab-close",
                                             "tab-close-others",
                                             "commit-files",
                                             "commit-copy-id",
                                             "commit-copy-message",
                                             "symbol-go",
                                             "breadcrumbs",
                                             "change-peek",
                                             "next-change",
                                             "previous-change"};

// the window, which the menus' answers and the walk's threads reach; and whether the walk colored a line since the
// window last took the frame, set on the walk's threads, and what wakes the view's thread
static GuiWindow *s_window;
static embed_atomic_u32 s_colored;
static void (*s_woken)(void);
// the view standing over the editor in its place, none while its table is NULL
static GuiWork s_work;

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
    for (unsigned int at = 0u; at < GUI_PANE_TOOLS; at += 1u)
    {
        EMBED_CALL(gui_pane.init, GuiPaneCfg, .pane = &args->window->panes[at]);
    }
    EMBED_CALL(cli_autohide_still.init, GuiAutohideStillCfg, .claim = &args->window->claim);
    EMBED_CALL(cli_wordmark_still.init, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    EMBED_CALL(cli_busy_still.init, GuiBusyStillCfg, .busy = &args->window->busy);
    EMBED_CALL(gui_hover.init, GuiHoverCfg, .hover = &args->window->hover);
    EMBED_CALL(gui_signature.init, GuiSignatureCfg, .signature = &args->window->signature);
    EMBED_CALL(gui_suggest.init, GuiSuggestCfg, .suggest = &args->window->suggest);
    if (!EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &args->window->background_fill) ||
        !EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &args->window->preferences_ground) ||
        !EMBED_CALL(gui_background.init, GuiBackgroundCfg, .background_fill = &args->window->dialog_ground))
    {
        mmgr_line_put(err_msg, "the lattice has no slot left for the ground");
        return 0;
    }
    // Preferences' lattice and the dialog's stand still until their windows show
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->preferences_ground,
               .paused = 1);
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->dialog_ground, .paused = 1);
    EMBED_CALL(gui_dialog.init, GuiDialogCfg, .dialog = &args->window->dialog);
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &args->window->glyphs, .err_msg = err_msg))
    {
        return 0;
    }
    args->window->pressed = GUI_HIT_CLIENT;
    // no thumb's drag waits for a frame, and the first frame the view hands on sets where the scroll stands
    args->window->scroll_drag_top = ~0ULL;
    args->window->scroll_top_sent = ~0ULL;
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
    unsigned int event;
    const CliItem *item;
    const GuiPaneTable *table_read;
    char *value_out;
    unsigned int section;
    unsigned int step;
    unsigned long long row;
    long long pixels;
    double down;
    double across;
    int *line_out;
    float *cell_out;
    unsigned long long *total_out;
    unsigned long long *shown_out;
    unsigned long long *first_out;
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
    g_cli_guard->persistent_buf_release(window->entries, g_cli_view_owner);
    window->entries = NULL;
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = NULL, .threaded = 0);
    // what the window took of the views goes back to them: nothing of it is read from here on
    cli_view_frame_release((CliViewFrame *)window->editor.frame);
    g_cli_guard->persistent_buf_release((void *)window->editor.names, g_cli_view_owner);
    cli_view_map_release((CliDocumentMap *)window->minimap.lines);
    cli_view_frame_release((CliViewFrame *)window->terminal.frame[0]);
    cli_view_frame_release((CliViewFrame *)window->terminal.frame[1]);
    g_cli_guard->persistent_buf_release((void *)window->terminal.names[0], g_cli_view_owner);
    g_cli_guard->persistent_buf_release((void *)window->terminal.names[1], g_cli_view_owner);
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
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        if (args->window->icons.entry[at].used)
        {
            g_cli_guard->persistent_buf_release(args->window->icons.entry[at].mask_bits.cover, g_gui_icons_owner);
            args->window->icons.entry[at].mask_bits.cover = NULL;
            args->window->icons.entry[at].used = 0;
        }
    }
    args->window->icons.count = 0u;
    g_cli_guard->persistent_buf_release(args->window->wordmark.flight_first, g_gui_owner);
    args->window->wordmark.flight_first = NULL;
    args->window->wordmark.flight_span = NULL;
    args->window->wordmark.flight_cover = NULL;
    g_cli_guard->persistent_buf_release(args->window->wordmark.row_sum, g_gui_owner);
    args->window->wordmark.row_sum = NULL;
    args->window->wordmark.text_color = NULL;
    args->window->wordmark.row = NULL;
    args->window->wordmark.name.cover = NULL;
    g_cli_guard->persistent_buf_release((&args->window->wordmark.under)->pixels, g_gui_raster_owner);
    (&args->window->wordmark.under)->pixels = NULL;
    (&args->window->wordmark.under)->width = 0u;
    (&args->window->wordmark.under)->height = 0u;
    args->window->wordmark.created_em = 0u;
    args->window->wordmark.laid_width = 0u;
    g_cli_guard->persistent_buf_release(args->window->busy.cell, g_gui_owner);
    args->window->busy.cell = NULL;
    args->window->busy.cell_bytes = 0ULL;
    args->window->busy.under_saved = 0;
    args->window->busy.displayed = 0;
    g_cli_guard->persistent_buf_release(args->window->controls_shadow.cover, g_gui_owner);
    args->window->controls_shadow.cover = NULL;
    args->window->controls_shadow.width = 0u;
    args->window->controls_shadow.height = 0u;
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &args->window->background_fill);
    g_cli_guard->persistent_buf_release(args->window->stages.stage, g_gui_owner);
    args->window->stages.stage = NULL;
    args->window->stages.count = 0u;
    args->window->stage = NULL;
    g_cli_guard->persistent_buf_release(args->window->base_kept.pixels, g_gui_raster_owner);
    args->window->base_kept.pixels = NULL;
    args->window->base_kept_whole = 0;
    g_cli_guard->persistent_buf_release(args->window->preferences_raster.pixels, g_gui_raster_owner);
    args->window->preferences_raster.pixels = NULL;
    args->window->preferences_raster.width = 0u;
    args->window->preferences_raster.height = 0u;
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &args->window->preferences_ground);
    g_cli_guard->persistent_buf_release(args->window->preferences_ground_raster.pixels, g_gui_raster_owner);
    args->window->preferences_ground_raster.pixels = NULL;
    args->window->preferences_ground_raster.width = 0u;
    args->window->preferences_ground_raster.height = 0u;
    g_cli_guard->persistent_buf_release(args->window->dialog_raster.pixels, g_gui_raster_owner);
    args->window->dialog_raster.pixels = NULL;
    args->window->dialog_raster.width = 0u;
    args->window->dialog_raster.height = 0u;
    EMBED_CALL(gui_background.free, GuiBackgroundCfg, .background_fill = &args->window->dialog_ground);
    g_cli_guard->persistent_buf_release(args->window->dialog_ground_raster.pixels, g_gui_raster_owner);
    args->window->dialog_ground_raster.pixels = NULL;
    args->window->dialog_ground_raster.width = 0u;
    args->window->dialog_ground_raster.height = 0u;
    g_cli_guard->persistent_buf_release(args->window->base.tile_red_blue, g_gui_base_owner);
    args->window->base.tile_red_blue = NULL;
    args->window->base.tile_green = NULL;
    args->window->base.tile_remainder = NULL;
    args->window->base.width = 0u;
    args->window->base.height = 0u;
    args->window->base.laid = 0;
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

// the tool whose pane the side shows, NULL where none; its table of pane, NULL where it has none; and the pane's own
// state, a state a tool
EMBED_INLINE const CliTool *window_tool(const GuiWindowCtx *args)
{
    // the tool shown is among the tools, which are fewer than the pane states
    return (args->window_read->strip.displayed >= 0) ? &g_cli_tools[args->window_read->strip.displayed] : NULL;
}

EMBED_INLINE const GuiPaneTable *window_pane_table(const GuiWindowCtx *args)
{
    const CliTool *const tool = window_tool(args);
    return (tool != NULL) ? EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = tool->name) : NULL;
}

EMBED_INLINE GuiPane *window_pane(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return &args->window
                ->panes[(args->window->strip.displayed >= 0) ? (unsigned int)args->window->strip.displayed : 0u];
}

EMBED_INLINE const GuiPane *window_pane_read(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return &args->window_read
                ->panes[(args->window_read->strip.displayed >= 0) ? (unsigned int)args->window_read->strip.displayed
                                                                  : 0u];
}

// the footer tool whose pane stands in the panel at the work's foot, NULL where none does; its foot table; and the
// pane's own state, a state a tool
EMBED_INLINE const GuiPaneTable *window_foot_table(const GuiWindowCtx *args)
{
    const int at = args->window_read->strip.foot;
    // the tool is among the tools
    const GuiPaneTable *const table =
        (at >= 0) ? EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = g_cli_tools[at].name) : NULL;
    return ((table != NULL) && table->foot) ? table : NULL;
}

EMBED_INLINE GuiPane *window_foot_pane(const GuiWindowCtx *args)
{
    // the tool is among the tools
    return &args->window->panes[(args->window->strip.foot >= 0) ? (unsigned int)args->window->strip.foot : 0u];
}

EMBED_INLINE const GuiPane *window_foot_pane_read(const GuiWindowCtx *args)
{
    // the tool is among the tools
    return &args->window_read
                ->panes[(args->window_read->strip.foot >= 0) ? (unsigned int)args->window_read->strip.foot : 0u];
}

// whether the pane of `table_read` is in sight: its tool's window the side's and the side not collapsed, or its tool's
// pane in the panel at the foot
EMBED_INLINE int window_table_shown(const GuiWindowCtx *args)
{
    return ((window_pane_table(args) == args->table_read) && !args->window_read->strip.collapsed) ||
           (window_foot_table(args) == args->table_read);
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
        g_cli_guard->persistent_buf_release((void *)terminal->names[view], g_cli_view_owner);
        terminal->names[view] = (key != NULL) ? EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg,
                                                           .key = key, .owner = g_cli_view_owner)
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

// The shown pane's data read anew on `event`, one of the pane's events, for the tree and the view shown. A pane's table
// reads on the events that change its data and no other. With `on` 1 only the tokens changed and the text did not, so
// the changes against the last commit stay as they were read
EMBED_INLINE void window_pane_read_event(const GuiWindowCtx *args)
{
    const GuiPaneTable *const tables[2] = {EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = args->window),
                                           EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = args->window)};
    const int shown = cli_view_shown();
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (tables[at] == NULL)
        {
            continue;
        }
        // the view shown is among the views
        EMBED_CALL(gui_pane.read, GuiPaneCfg, .table = tables[at], .event = args->event,
                   .root = cli_command_line_root(), .key = (shown >= 0) ? cli_view_key((unsigned int)shown) : NULL,
                   .path = (shown >= 0) ? cli_view_path_at((unsigned int)shown) : NULL);
    }
    // the view shown or saved: its changes against the last commit, which the gutter marks, and its crumbs at the
    // cursor
    if ((args->event == GUI_PANE_VIEW) || (args->event == GUI_PANE_SAVED))
    {
        // the view shown is among the views
        const char *const key = (shown >= 0) ? cli_view_key((unsigned int)shown) : NULL;
        const char *const path = (shown >= 0) ? cli_view_path_at((unsigned int)shown) : NULL;
        CliPosition cursor = {0ULL, 0ULL};
        if (key != NULL)
        {
            (void)cli_view_cursor(&(CliViewCursorCfg){.key = key, .cursor = &cursor, .anchor = NULL});
        }
        if (!args->on)
        {
            EMBED_CALL(cli_hunks.read_later, CliHunksCfg, .key = key, .path = path, .root = cli_command_line_root());
        }
        EMBED_CALL(cli_crumbs.read, CliCrumbsCfg, .key = key, .path = path, .line = cursor.line);
    }
}

// every view with changes not saved saved, as a pane's action asks before it runs the tree's code
static void window_save_all(void *context);
static void window_file_reread(void *context, const char *path);

EMBED_INLINE void window_scroll_damage(const GuiWindowCtx *args);

// a line's height and a cell's width of the editor's code, in pixels at the frame's scale, into `line_out` and
// `cell_out`
EMBED_INLINE void window_scroll_units(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = window->frame.scale);
    *args->line_out =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = window->frame.scale);
    *args->cell_out = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg,
                                 .glyphs = (GuiGlyphs *)&window->glyphs, .size = size);
}

// The scroll put where the frame the view handed on stands, where the view moved on its own: a key, find or the map
// moved its top line or its first cell column. The scroll stands still there, on a whole line and a whole cell
EMBED_INLINE void window_scroll_adopt(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const CliViewFrame *const frame = window->editor.frame;
    if ((frame == NULL) || ((frame->top == window->scroll_top_sent) && (frame->left == window->scroll_left_sent)))
    {
        return;
    }
    int line = 0;
    float cell = 0.0f;
    EMBED_CALL(window_scroll_units, GuiWindowCtx, .window_read = window, .line_out = &line, .cell_out = &cell);
    window->scroll_down = (double)frame->top * (double)line;
    window->scroll_down_to = window->scroll_down;
    window->scroll_across = (double)frame->left * (double)cell;
    window->scroll_across_to = window->scroll_across;
    window->scroll_top_sent = frame->top;
    window->scroll_left_sent = frame->left;
    window->scroll_moving = 0;
    window->editor.scroll_y = 0;
    window->editor.scroll_x = 0.0f;
}

// The shown view's frame, names and map taken anew of it, those it had released, and the shown pane's data where the
// view's change reaches it; `edited` where the text changed, which the map's lines no longer show. A take where the
// view only scrolled, `on` 1, marks the scroll's part of the frame changed, and any other take owes a frame whole
EMBED_INLINE void window_map_span(const GuiWindowCtx *args);

// one color of the shown language's palette taken as a token of the window's palette, its value read as the color
// settings are, the editor's text standing in for an alpha the value leaves out
static void window_token_each(const CliPluginColorCfg *args)
{
    GuiPaletteOwn *const own = (GuiPaletteOwn *)args->context;
    GuiColor color = 0u;
    if (EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = args->value,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = GUI_PREFERENCES_TEXT}),
                   .current = own->colors.editor_text, .color_pointer = &color))
    {
        EMBED_CALL(gui_palette_own_name.token_add, GuiPaletteOwnNameCfg, .own = own, .name = args->name,
                   .color = color);
    }
}

// the palette of the language plugin the view under `key` opens in loaded as the window's tokens, read again only
// as the view shown changes; a view of no language, or none, leaves the window with no tokens
EMBED_INLINE void window_tokens_load(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const key = (args->key != NULL) ? args->key : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = window->tokens_key, .other = key, .cap = sizeof(window->tokens_key)}))
    {
        return;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = window->tokens_key,
                      .src = key,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = sizeof(window->tokens_key)}) + 1ULL});
    EMBED_CALL(gui_palette_own.tokens_clear, GuiPaletteOwnCfg, .own = &window->palette_own);
    window->tokens_id[0] = '\0';
    char path[CLI_PATH_MAX_LEN];
    if ((key[0] == '\0') || !EMBED_CALL(cli_view_path_type.path, CliViewPathTypeCfg, .key = key, .path_out = path) ||
        (path[0] == '\0'))
    {
        return;
    }
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known =
        (languages.pair != NULL) && EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                               .languages_read = &languages, .path = path, .id_out = window->tokens_id);
    g_cli_guard->persistent_buf_release(languages.pair, g_cli_plugins_owner);
    if (known)
    {
        (void)EMBED_CALL(cli_plugins_id_each.palette_each, CliPluginsIdEachCfg, .id = window->tokens_id,
                         .each = window_token_each, .context = &window->palette_own);
    }
    else
    {
        window->tokens_id[0] = '\0';
    }
}

EMBED_INLINE void window_view_take(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    EMBED_CALL(window_tokens_load, GuiWindowCtx, .window = window, .key = key);
    const int shown = cli_view_shown();
    // A view that only scrolled keeps its text, its cursor and its file, so the panes, the changes against the last
    // commit and the crumbs stay as they were read. `on` 1 says the view only scrolled. A view that kept its text and
    // its file keeps its changes against the last commit, so the pane read leaves them as they were read
    if (!args->on || (shown != window->view_shown))
    {
        EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = window, .event = GUI_PANE_VIEW,
                   .on = !args->edited && (shown == window->view_shown));
    }
    if (shown != window->view_shown)
    {
        // another file shown: the map stands anew beside it, at the editor's size
        window->view_shown = shown;
        window->map_known = 0;
        // the scroll takes the place the other file's view stands at
        window->scroll_top_sent = ~0ULL;
        EMBED_CALL(gui_minimap.clear, GuiMinimapCfg, .map = &window->minimap);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = window->rows, .columns = window->columns};
        if (key != NULL)
        {
            (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized,
                             .err_msg = &err_msg);
        }
    }
    cli_view_frame_release((CliViewFrame *)window->editor.frame);
    window->editor.frame = (key != NULL) ? cli_view_frame_alloc(key) : NULL;
    g_cli_guard->persistent_buf_release((void *)window->editor.names, g_cli_view_owner);
    window->editor.names = (key != NULL) ? EMBED_CALL(cli_document_names.names_alloc, CliDocumentNamesCfg, .key = key,
                                                      .owner = g_cli_view_owner)
                                         : NULL;
    if (args->edited || (window->minimap.lines == NULL) || (key == NULL))
    {
        cli_view_map_release((CliDocumentMap *)window->minimap.lines);
        window->minimap.lines =
            ((key != NULL) && window->map_known) ? cli_view_map_alloc(key, window->map_first, window->map_last) : NULL;
    }
    // the map's lines follow the view: a frame that moved the map's first row takes the lines it now stands for
    EMBED_CALL(window_map_span, GuiWindowCtx, .window = window);
    EMBED_CALL(window_scroll_adopt, GuiWindowCtx, .window = window);
    // every frame the view answers shows the caret through the input rest, its blink beginning after it, so the caret
    // stands shown while the keys go
    window->caret_blink_start_ms = cli_clock_milliseconds() + GUI_CARET_INPUT_REST_MS;
    if (args->on)
    {
        EMBED_CALL(window_scroll_damage, GuiWindowCtx, .window = window);
    }
    else
    {
        window->pending = 1;
    }
}

// The rectangle `caret`, an empty one adding nothing, joined to the frame's changes: their union is the smallest
// rectangle that holds both
EMBED_INLINE void window_damage_add(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiRect add = args->caret;
    if ((add.width <= 0) || (add.height <= 0))
    {
        return;
    }
    const GuiRect had = window->damage;
    if ((had.width <= 0) || (had.height <= 0))
    {
        window->damage = add;
        return;
    }
    const int left = (had.x < add.x) ? had.x : add.x;
    const int top = (had.y < add.y) ? had.y : add.y;
    const int right = ((had.x + had.width) > (add.x + add.width)) ? (had.x + had.width) : (add.x + add.width);
    const int foot = ((had.y + had.height) > (add.y + add.height)) ? (had.y + had.height) : (add.y + add.height);
    window->damage = (GuiRect){left, top, right - left, foot - top};
}

// The part of the frame a scroll changes: the editor's text and gutter, its two scrollbars and the map's column
EMBED_INLINE void window_scroll_damage(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiFrame *const frame = &window->frame;
    const GuiRect area = {frame->editor.x, frame->editor.y, frame->editor.width + frame->editor_bar.width,
                          frame->editor.height + frame->editor_bar_across.height};
    EMBED_CALL(window_damage_add, GuiWindowCtx, .window = window, .caret = area);
    EMBED_CALL(window_damage_add, GuiWindowCtx, .window = window, .caret = frame->map);
}

// The scroll sent toward `down` and `across` pixels. Across is held inside the grid, from its first cell column to the
// widest line's last columns filling the view. Down has no bound here: the view stops the rows at the text's ends.
// With `on` 0 the scroll stands there at the next frame, with no ease
EMBED_INLINE void window_scroll_to(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const CliViewFrame *const frame = window->editor.frame;
    int line = 0;
    float cell = 0.0f;
    EMBED_CALL(window_scroll_units, GuiWindowCtx, .window_read = window, .line_out = &line, .cell_out = &cell);
    const unsigned long long widest = (frame != NULL) ? frame->widest : 0ULL;
    const double most_across = (widest > window->columns) ? ((double)(widest - window->columns) * (double)cell) : 0.0;
    window->scroll_down_to = args->down;
    window->scroll_across_to = (args->across < 0.0) ? 0.0 : ((args->across > most_across) ? most_across : args->across);
    if (!args->on)
    {
        window->scroll_down = window->scroll_down_to;
        window->scroll_across = window->scroll_across_to;
    }
    if (!window->scroll_moving)
    {
        window->scroll_stepped_ms = cli_clock_milliseconds();
    }
    window->scroll_moving = 1;
    EMBED_CALL(window_scroll_damage, GuiWindowCtx, .window = window);
}

// One way of the scroll stepped by the share `share` of its distance, and stopped once it stands within
// GUI_SCROLL_STILL of a pixel of where it goes: 1 where it still moves
EMBED_INLINE int window_scroll_toward(double *at, double to, double share)
{
    *at += (to - *at) * share;
    if (fabs(to - *at) < GUI_SCROLL_STILL)
    {
        *at = to;
        return 0;
    }
    return 1;
}

// One step of the autoscroll at a frame. The pointer's distance from the press point, past GUI_AUTOSCROLL_REST_PIXELS,
// sets the speed: GUI_AUTOSCROLL_PIXELS_A_SECOND for each pixel. Below the press point the scroll goes down, above it
// up, by the time since the last step. The scroll's own step eases it there and sends the rows crossed. An autoscroll
// with no editor ends
EMBED_INLINE void window_autoscroll_step(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if (!window->autoscroll)
    {
        return;
    }
    if (window->editor.frame == NULL)
    {
        window->autoscroll = 0;
        return;
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long since_ms = now_ms - window->autoscroll_stepped_ms;
    window->autoscroll_stepped_ms = now_ms;
    const int distance = window->autoscroll_pointer_y - window->autoscroll_y;
    const int past = (distance > GUI_AUTOSCROLL_REST_PIXELS)    ? (distance - GUI_AUTOSCROLL_REST_PIXELS)
                     : (distance < -GUI_AUTOSCROLL_REST_PIXELS) ? (distance + GUI_AUTOSCROLL_REST_PIXELS)
                                                                : 0;
    if (past == 0)
    {
        return;
    }
    const double moved = (double)past * GUI_AUTOSCROLL_PIXELS_A_SECOND * ((double)since_ms / 1000.0);
    const double down = window->scroll_down_to + moved;
    EMBED_CALL(window_scroll_to, GuiWindowCtx, .window = window, .on = 1, .down = (down < 0.0) ? 0.0 : down,
               .across = window->scroll_across_to);
}

// One step of the scroll at a frame. A thumb's drag puts the view at its line and cell column first. Then each way
// moves the share of its distance the time since the last step gives. The view is sent the whole rows the scroll
// crossed down and the cell column it stands at across, and the editor draws the part of a line past them. Where the
// view's top does not move for rows sent, the text ends there, and the scroll stops on that line
EMBED_INLINE void window_scroll_step(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    if (!window->scroll_moving || (key == NULL) || (window->editor.frame == NULL))
    {
        window->scroll_moving = 0;
        return;
    }
    int line = 0;
    float cell = 0.0f;
    EMBED_CALL(window_scroll_units, GuiWindowCtx, .window_read = window, .line_out = &line, .cell_out = &cell);
    if ((line <= 0) || (cell <= 0.0f))
    {
        window->scroll_moving = 0;
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (window->scroll_drag_top != ~0ULL)
    {
        const CliViewInput put = {.type = CLI_VIEW_SCROLL, .top = window->scroll_drag_top};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &put, .err_msg = &err_msg);
        window->scroll_drag_top = ~0ULL;
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0, .on = 1);
        if (window->editor.frame == NULL)
        {
            return;
        }
    }
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long since_ms = now_ms - window->scroll_stepped_ms;
    window->scroll_stepped_ms = now_ms;
    const double span = (double)((since_ms < GUI_SCROLL_STEP_MOST_MS) ? since_ms : GUI_SCROLL_STEP_MOST_MS);
    const double share = 1.0 - exp(-span / GUI_SCROLL_EASE_MS);
    const double rows_before = floor(window->scroll_down / (double)line);
    const int down_moves = window_scroll_toward(&window->scroll_down, window->scroll_down_to, share);
    const int across_moves = window_scroll_toward(&window->scroll_across, window->scroll_across_to, share);
    window->scroll_moving = down_moves || across_moves;
    // the rows crossed are few each frame, far inside what a long long represents
    const long long crossed = (long long)(floor(window->scroll_down / (double)line) - rows_before);
    // the cell column is not negative, the scroll across held at 0 and more
    const unsigned long long left = (unsigned long long)(window->scroll_across / (double)cell);
    if (crossed != 0LL)
    {
        const unsigned long long top_before = cli_view_top(key);
        const CliViewInput rows = {.type = CLI_VIEW_WHEEL, .wheel_rows = crossed, .top = CLI_VIEW_WHEEL_UNBOUND};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &rows, .err_msg = &err_msg);
        if (left != window->scroll_left_sent)
        {
            const CliViewInput slid = {.type = CLI_VIEW_SLIDE, .column = left};
            (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &slid,
                             .err_msg = &err_msg);
        }
        const unsigned long long top_after = cli_view_top(key);
        window->scroll_top_sent = top_after;
        window->scroll_left_sent = left;
        if (top_after == top_before)
        {
            // the text ends this way: the scroll stops on the line it stood at
            window->scroll_down = rows_before * (double)line;
            window->scroll_down_to = window->scroll_down;
            window->scroll_moving = across_moves;
        }
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0, .on = 1);
    }
    else if (left != window->scroll_left_sent)
    {
        const CliViewInput slid = {.type = CLI_VIEW_SLIDE, .column = left};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &slid, .err_msg = &err_msg);
        window->scroll_left_sent = left;
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0, .on = 1);
    }
    // the part of a line and of a cell past the view's top line and first cell column, each less than one of them
    window->editor.scroll_y = (int)(window->scroll_down - (floor(window->scroll_down / (double)line) * (double)line));
    window->editor.scroll_x = (float)(window->scroll_across - ((double)left * (double)cell));
    EMBED_CALL(window_scroll_damage, GuiWindowCtx, .window = window);
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
    const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = rows, .columns = columns};
    if ((key != NULL) &&
        EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized, .err_msg = &err_msg))
    {
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0, .on = 1);
    }
}

// The lines the map placed beside the view stands for taken of the view, where they moved. The map's column stands
// whole, away or shown, and the lines are taken either way, so a map that comes back draws at once
EMBED_INLINE void window_map_span(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if ((window->editor.frame == NULL) || (window->frame.map_whole.height <= 0))
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
    g_cli_guard->persistent_buf_release(args->window->stages.stage, g_gui_owner);
    args->window->stages.stage = NULL;
    args->window->stages.count = 0u;
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
        // the strip, and the side pane while it stands: the pointer works in the pane as it works on the strip
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_band,
                                             .other = frame->strip),
                          .x = x, .y = y) ||
               (frame->side_displayed &&
                EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->side, .x = x, .y = y));
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
// open for the terminal or for a footer tool's pane at the terminal panel's height, a panel opened coming in from the
// foot unless the window is `starting`, the editor's and the screens' sizes handed on where they moved
EMBED_INLINE void window_terminal(const GuiWindowCtx *args)
{
    EMBED_CALL(window_terminal_take, GuiWindowCtx, .window = args->window);
    const CliTerminalPanel *const panel = &args->window->terminal.panel;
    const int foot = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = args->window) != NULL;
    // a panel's height in the page's pixels is far inside what an int represents
    const int height = (panel->open || foot) ? (int)panel->height : 0;
    args->window->strip.terminal_open = panel->open;
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

// the side laid again as the pane's waits have shown or collapsed it or a preview has changed its tool, the shown
// pane's data read as it shows
EMBED_INLINE void window_sides_laid(const GuiWindowCtx *args)
{
    EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = args->window, .event = GUI_PANE_SHOWN);
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
    // the autoscroll moves the scroll first, then the editor's scroll steps once a frame, whatever turned or dragged it
    // since the last
    EMBED_CALL(window_autoscroll_step, GuiWindowCtx, .window = args->window);
    EMBED_CALL(window_scroll_step, GuiWindowCtx, .window = args->window);
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
    // the ground where its loop shows a change, and Preferences' own while its window shows
    args->window->pending =
        EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg,
                   .background_fill = &args->window->background_fill, .palette = args->window->palette) ||
        args->window->pending;
    if (args->window->preferences.displayed)
    {
        args->window->preferences_pending =
            EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg,
                       .background_fill = &args->window->preferences_ground, .palette = args->window->palette) ||
            args->window->preferences_pending;
    }
    if (args->window->dialog.displayed)
    {
        args->window->dialog_pending =
            EMBED_CALL(gui_background_raster.tick, GuiBackgroundRasterCfg,
                       .background_fill = &args->window->dialog_ground, .palette = args->window->palette) ||
            args->window->dialog_pending;
    }
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

// the deadline that set the last wait, its place among gui_window_timeout_ms's deadlines, GUI_WINDOW_WAKES for none
static unsigned int s_timeout_source = GUI_WINDOW_WAKES;

const char *const g_gui_window_wake_names[GUI_WINDOW_WAKES] = {
    "autohide", "scroll", "slide",  "ground", "preferences", "dialog",  "wordmark",
    "caret",    "sides",  "busy",   "panels", "terminal",    "notices", "menus",
    "memory",   "runs",   "search", "git",    "preview",     "comms",   "debug",
};

unsigned int gui_window_timeout_source(void)
{
    return s_timeout_source;
}

unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args)
{
    const GuiWindow *const window = args->window_read;
    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long deadlines[] = {
        EMBED_CALL(gui_autohide.next_deadline_ms, GuiAutohideCfg, .autohide_read = &window->claim),
        // the editor's scroll steps at the next frame while it moves or the autoscroll runs
        (window->scroll_moving || window->autoscroll) ? now_ms + 1ULL : 0ULL,
        // the side pane's slide and the terminal panel's arrival read their next stage a stage on
        ((EMBED_CALL(gui_sides.slide_end_ms, GuiSidesCfg, .sides_read = &window->sides, .time_ms = now_ms) != 0ULL) ||
         (EMBED_CALL(gui_docks.arrive_end_ms, GuiDocksCfg, .docks_read = &window->docks, .time_ms = now_ms) != 0ULL))
            ? now_ms + GUI_STAGE_MS
            : 0ULL,
        EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg,
                   .background_fill = &window->background_fill),
        window->preferences.displayed ? EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg,
                                                   .background_fill = &window->preferences_ground)
                                      : 0ULL,
        window->dialog.displayed ? EMBED_CALL(gui_background_clock.next_deadline_ms, GuiBackgroundClockCfg,
                                              .background_fill = &window->dialog_ground)
                                 : 0ULL,
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
        EMBED_CALL(cli_runs_pane.deadline_ms, CliRunsPaneCfg, .root = cli_command_line_root(),
                   .shown = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                       .table_read = &gui_runs_table)),
        EMBED_CALL(cli_search_pane.deadline_ms, CliSearchPaneCfg, .at = 0u),
        EMBED_CALL(cli_git_log.deadline_ms, CliGitLogCfg, .at = 0u),
        EMBED_CALL(cli_preview.deadline_ms, CliPreviewCfg, .now_ms = now_ms),
        EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg,
                   .shown = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                       .table_read = &gui_comms_table)),
        EMBED_CALL(cli_debug_pane_view.deadline_ms, CliDebugPaneViewCfg, .now_ms = now_ms),
    };
    _Static_assert((sizeof(deadlines) / sizeof(deadlines[0])) == GUI_WINDOW_WAKES,
                   "each deadline of the window needs its name in g_gui_window_wake_names");
    unsigned long long earliest = 0ULL;
    s_timeout_source = GUI_WINDOW_WAKES;
    for (unsigned int index = 0u; index < (sizeof(deadlines) / sizeof(deadlines[0])); index += 1u)
    {
        if ((deadlines[index] != 0ULL) && ((earliest == 0ULL) || (deadlines[index] < earliest)))
        {
            earliest = deadlines[index];
            s_timeout_source = index;
        }
    }
    if (earliest == 0ULL)
    {
        return 0ULL;
    }
    return (earliest > now_ms) ? earliest - now_ms : 1ULL;
}

// whether `name` is `key`
EMBED_INLINE int window_is(const GuiWindowCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->key, .cap = CLI_LABEL_MAX_LEN});
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
// each plugin with a palette listed in Preferences by its id, one the list holds already left out
static void window_palette_id_each(const CliPluginIdCfg *args)
{
    GuiPreferences *const preferences = (GuiPreferences *)args->context;
    for (unsigned int at = 0u; at < preferences->palettes; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = preferences->palette_name[at], .other = args->id, .cap = GUI_PREFERENCES_TEXT}))
        {
            return;
        }
    }
    EMBED_CALL(gui_preferences_size.palette_add, GuiPreferencesSizeCfg, .preferences = preferences, .text = args->id);
}

// the palette Preferences shows: the window's, or the window's colors with the chosen plugin's tokens
EMBED_INLINE const GuiPalette *window_preferences_palette(const GuiWindowCtx *args)
{
    return (args->window->preferences.palette_chosen == 0u) ? args->window->palette
                                                            : &args->window->palette_choice_own.colors;
}

EMBED_INLINE void window_preferences_toggle(const GuiWindowCtx *args)
{
    args->window->preferences.displayed = !args->window->preferences.displayed;
    args->window->preferences.entering = 0;
    args->window->preferences_pending = 1;
    // Preferences opened lists the palettes anew: the window's, then each language plugin's that carries one
    if (args->window->preferences.displayed)
    {
        EMBED_CALL(gui_preferences_size.palettes_clear, GuiPreferencesSizeCfg,
                   .preferences = &args->window->preferences);
        EMBED_CALL(gui_preferences_size.palette_add, GuiPreferencesSizeCfg, .preferences = &args->window->preferences,
                   .text = "Window");
        EMBED_CALL(cli_plugins_each_id.palettes_each, CliPluginsEachIdCfg, .each = window_palette_id_each,
                   .context = &args->window->preferences);
    }
    // Preferences' own lattice runs while its window shows, and its clock stands still while it is closed
    EMBED_CALL(gui_background.pause, GuiBackgroundCfg, .background_fill = &args->window->preferences_ground,
               .paused = !args->window->preferences.displayed);
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

// a percent written into the text `value_out`, a row's value, as a row of Settings says it
EMBED_INLINE void window_percent_text(const GuiWindowCtx *args)
{
    VerbaLinea line = {args->value_out, sizeof(((GuiPreferenceRow *)0)->value), 0ULL};
    mmgr_line_decimal(&line, args->percent, 1u);
    mmgr_line_ch(&line, '%');
    (void)mmgr_line_finish(&line);
}

// The section of Preferences that shows the rows under the catalog's chain `name`: the fonts node's in Fonts, the
// patterns and search nodes' in Files, the sync node's in Carry, and every other row in Settings
EMBED_INLINE unsigned int window_preference_section(const GuiWindowCtx *args)
{
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "fonts"))
    {
        return GUI_PREFERENCES_FONTS;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "patterns") ||
        EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "search"))
    {
        return GUI_PREFERENCES_FILES;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "sync"))
    {
        return GUI_PREFERENCES_CARRY;
    }
    return GUI_PREFERENCES_SETTINGS;
}

// The section of Preferences for the button of the catalog's command `name`, at the top of the Preferences menu: a
// theme's commands in Theme, and the settings files' in In force. GUI_PREFERENCES_SECTIONS where it has no button
EMBED_INLINE unsigned int window_preference_button_section(const GuiWindowCtx *args)
{
    if (cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "theme-", .cap = CLI_LABEL_MAX_LEN}))
    {
        return GUI_PREFERENCES_THEME;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "settings-own") ||
        EMBED_CALL(window_is, GuiWindowCtx, .name = args->name, .key = "settings-tree"))
    {
        return GUI_PREFERENCES_FORCE;
    }
    return GUI_PREFERENCES_SECTIONS;
}

// A row added at the end of the rows: its section, type, label, key and value, and its note. The note holds the word a
// button's command runs with, or where a line's value comes from. 1, or 0 where the rows are full
EMBED_INLINE int window_preference_line_add(const GuiWindowCtx *args)
{
    GuiPreferenceValues *const values = &args->window->preference_values;
    if (values->rows >= GUI_PREFERENCE_ROWS_MOST)
    {
        return 0;
    }
    GuiPreferenceRow *const row = &values->row[values->rows];
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    row->section = args->section;
    row->type = args->at;
    VerbaLinea label = {row->label, sizeof(row->label), 0ULL};
    mmgr_line_put(&label, args->name);
    (void)mmgr_line_finish(&label);
    VerbaLinea key = {row->key, sizeof(row->key), 0ULL};
    mmgr_line_put(&key, args->key);
    (void)mmgr_line_finish(&key);
    VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
    mmgr_line_put(&value, (args->value != NULL) ? args->value : "");
    (void)mmgr_line_finish(&value);
    VerbaLinea note = {row->note, sizeof(row->note), 0ULL};
    mmgr_line_put(&note, (args->value_out != NULL) ? args->value_out : "");
    (void)mmgr_line_finish(&note);
    values->rows += 1u;
    return 1;
}

// The rows of Keys under the catalog's list `at`, the menu's title `name` before each label: a row for each command
// that keys run, its key `keys.` and the command, its value the keys that run it now. A node's rows follow it
EMBED_INLINE void window_preference_keys_under(const GuiWindowCtx *args)
{
    CliItem items[GUI_WINDOW_PREFERENCE_ITEMS];
    const unsigned int count = cli_menu_rows(args->at, items, GUI_WINDOW_PREFERENCE_ITEMS);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliItem *const item = &items[at];
        if ((item->command == NULL) || item->hidden || item->line || item->setting)
        {
            continue;
        }
        if (item->list != 0u)
        {
            EMBED_CALL(window_preference_keys_under, GuiWindowCtx, .window = args->window, .at = item->list,
                       .name = args->name);
            continue;
        }
        char label[GUI_PREFERENCES_TEXT];
        VerbaLinea label_line = {label, sizeof(label), 0ULL};
        mmgr_line_put(&label_line, args->name);
        mmgr_line_put(&label_line, ": ");
        mmgr_line_put(&label_line, (item->label != NULL) ? item->label : item->command);
        (void)mmgr_line_finish(&label_line);
        char key[GUI_PREFERENCES_TEXT];
        VerbaLinea key_line = {key, sizeof(key), 0ULL};
        mmgr_line_put(&key_line, GUI_WINDOW_KEYS_KEY);
        mmgr_line_put(&key_line, item->command);
        (void)mmgr_line_finish(&key_line);
        (void)EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window,
                         .section = GUI_PREFERENCES_KEYS, .at = GUI_PREFERENCE_TYPE_KEY, .name = label, .key = key,
                         .value = (item->keys != NULL) ? item->keys : "");
    }
}

// The rows of Keys: every menu's commands, each menu's in turn
EMBED_INLINE void window_preference_keys_add(const GuiWindowCtx *args)
{
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        EMBED_CALL(window_preference_keys_under, GuiWindowCtx, .window = args->window, .at = cli_menu_list(menu),
                   .name = cli_menu_title_text(menu));
    }
}

// The rows of Notices: a button for each type of notice put away, which shows the type again. A line says so where
// no type is put away
EMBED_INLINE void window_preference_notices_add(const GuiWindowCtx *args)
{
    const char *const away = cli_notices_away();
    const unsigned long long size =
        (away != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = away, .cap = CLI_POOL_BYTES}) : 0ULL;
    int added = 0;
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = away, .at = at, .size = size, .set = "\n");
        if (end > at)
        {
            char type[GUI_PREFERENCES_TEXT];
            VerbaLinea type_line = {type, sizeof(type), 0ULL};
            mmgr_line_put_n(&type_line, away + at, end - at);
            (void)mmgr_line_finish(&type_line);
            added = EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window,
                               .section = GUI_PREFERENCES_NOTICES, .at = GUI_PREFERENCE_TYPE_ACTION, .name = type,
                               .key = "notice-show-again", .value = "Show Again", .value_out = type) ||
                    added;
        }
        at = end + 1ULL;
    }
    if (!added)
    {
        (void)EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window,
                         .section = GUI_PREFERENCES_NOTICES, .at = GUI_PREFERENCE_TYPE_LINE,
                         .name = "No notice is put away", .key = "", .value = "");
    }
}

// The lines of In force: each setting a settings file sets, its value in force, and the file and the part of it the
// value comes from. The reader's own settings files and the tree's are read. A line says so where none sets one
EMBED_INLINE void window_preference_force_add(const GuiWindowCtx *args)
{
    typedef struct
    {
        CliChain reader;
        CliChain tree;
        CliLayers layers;
    } WindowForceRead;
    WindowForceRead *const read = g_cli_guard->temporary_buf_alloc(sizeof(*read), g_cli_settings_owner);
    if (read == NULL)
    {
        return;
    }
    const char *const root = (cli_command_line_root() != NULL) ? cli_command_line_root() : "";
    EMBED_CALL(cli_settings_path_table.reader, CliSettingsPathCfg, .chain = &read->reader);
    EMBED_CALL(cli_settings_path_table.tree, CliSettingsPathCfg, .root = root, .chain = &read->tree);
    EMBED_CALL(cli_settings_root.layers, CliSettingsRootCfg, .reader = &read->reader, .tree = &read->tree, .root = root,
               .layers = &read->layers);
    const CliLayers *const layers = &read->layers;
    int added = 0;
    for (unsigned int at = 0u; (layers->text != NULL) && (at < layers->froms); at += 1u)
    {
        const CliLayerSource *const source = &layers->from[at];
        // the first part of a key's parts is the one in force; the parts under it repeat its key
        if ((at != 0u) && cellul.eq(&(CatenaFinitaCfg){.src = layers->text + layers->from[at - 1u].key,
                                                       .other = layers->text + source->key,
                                                       .cap = CLI_POOL_BYTES}))
        {
            continue;
        }
        char from[GUI_PREFERENCES_TEXT * 2u];
        VerbaLinea from_line = {from, sizeof(from), 0ULL};
        mmgr_line_put(&from_line, layers->text + source->file);
        if (source->has_part)
        {
            mmgr_line_put(&from_line, " for ");
            mmgr_line_put(&from_line, layers->text + source->part);
        }
        (void)mmgr_line_finish(&from_line);
        added = EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window,
                           .section = GUI_PREFERENCES_FORCE, .at = GUI_PREFERENCE_TYPE_LINE,
                           .name = layers->text + source->key, .key = layers->text + source->key,
                           .value = layers->text + source->text, .value_out = from) ||
                added;
    }
    if (!added)
    {
        (void)EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window,
                         .section = GUI_PREFERENCES_FORCE, .at = GUI_PREFERENCE_TYPE_LINE,
                         .name = "No settings file sets a setting", .key = "", .value = "");
    }
    if (read->layers.text != NULL)
    {
        g_cli_guard->persistent_buf_release(read->layers.text, g_cli_settings_owner);
    }
    const CliChain *const chains[2] = {&read->reader, &read->tree};
    for (unsigned int chain = 0u; chain < 2u; chain += 1u)
    {
        if (chains[chain]->json.node != NULL)
        {
            g_cli_guard->persistent_buf_release(
                chains[chain]->json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), chains[chain]->json.node));
        }
    }
    g_cli_guard->persistent_buf_release(read, g_cli_settings_owner);
}

// One row of the section `section` for the catalog's setting `item`, where the chain `name` puts the item in that
// section. Its key is the chain and the item's command, and its value comes from `entries`. The item's arguments give
// its type: the zoom's own key the zoom, `<percent>` a percent, `[on|off]` a switch, `<count>` a count, and any other
// text. The zoom and the two opacities the window draws with read their values from what the window stands at. An
// opacity with no value is 100%, and a switch with no value is off
EMBED_INLINE void window_preference_row_add(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if ((values->rows >= GUI_PREFERENCE_ROWS_MOST) ||
        (EMBED_CALL(window_preference_section, GuiWindowCtx, .name = args->name) != args->section))
    {
        return;
    }
    GuiPreferenceRow *const row = &values->row[values->rows];
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    row->section = args->section;
    VerbaLinea key = {row->key, sizeof(row->key), 0ULL};
    if (args->name[0] != '\0')
    {
        mmgr_line_put(&key, args->name);
        mmgr_line_ch(&key, '.');
    }
    mmgr_line_put(&key, args->item->command);
    VerbaLinea label = {row->label, sizeof(row->label), 0ULL};
    mmgr_line_put(&label, (args->item->label != NULL) ? args->item->label : args->item->command);
    if ((mmgr_line_finish(&key) == 0ULL) || (mmgr_line_finish(&label) == 0ULL))
    {
        return;
    }
    const char *const takes = (args->item->args != NULL) ? args->item->args : "";
    const char *const saved = (args->entries != NULL) ? cli_state_entry_value(args->entries, row->key) : NULL;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_ZOOM_KEY))
    {
        row->type = GUI_PREFERENCE_TYPE_ZOOM;
        EMBED_CALL(window_percent_text, GuiWindowCtx, .value_out = row->value,
                   .percent = EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                         .step = values->zoom_step));
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = takes, .key = "<percent>"))
    {
        row->type = GUI_PREFERENCE_TYPE_PERCENT;
        const int bar = EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_BAR_KEY);
        const int editor = EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_EDITOR_KEY);
        // an opacity that nothing sets stands whole
        unsigned int percent = GUI_PREFERENCE_OPACITY_MAX;
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = saved, .percent_out = &percent);
        if (bar || editor)
        {
            percent = bar ? values->bar_percent : values->editor_percent;
        }
        EMBED_CALL(window_percent_text, GuiWindowCtx, .value_out = row->value, .percent = percent);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = takes, .key = "[on|off]"))
    {
        // a switch the CLI saves as true or false; one that nothing sets is off
        row->type = GUI_PREFERENCE_TYPE_SWITCH;
        const int on = (saved != NULL) && EMBED_CALL(window_is, GuiWindowCtx, .name = saved, .key = "true");
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_put(&value, on ? "on" : "off");
        (void)mmgr_line_finish(&value);
    }
    else
    {
        row->type = EMBED_CALL(window_is, GuiWindowCtx, .name = takes, .key = "<count>") ? GUI_PREFERENCE_TYPE_COUNT
                                                                                         : GUI_PREFERENCE_TYPE_TEXT;
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_put(&value, (saved != NULL) ? saved : "");
        (void)mmgr_line_finish(&value);
    }
    values->rows += 1u;
}

// The rows of the section `section` under the catalog's list `at`, the chain `name` before their keys: each setting a
// row, each command a button, and each node's rows after it under its own chain. A setting of `each` key, as the keys
// of each command, has a section of its own and no row here. A command at the top of the menu is a button where its
// section is one; a command in a node is a button of the node's section
EMBED_INLINE void window_preference_rows_under(const GuiWindowCtx *args)
{
    CliItem items[GUI_WINDOW_PREFERENCE_ITEMS];
    const unsigned int count = cli_menu_rows(args->at, items, GUI_WINDOW_PREFERENCE_ITEMS);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliItem *const item = &items[at];
        if ((item->command == NULL) || item->hidden || item->each)
        {
            continue;
        }
        const unsigned int button_section =
            (args->name[0] == '\0') ? EMBED_CALL(window_preference_button_section, GuiWindowCtx, .name = item->command)
                                    : EMBED_CALL(window_preference_section, GuiWindowCtx, .name = args->name);
        const char *const label = (item->label != NULL) ? item->label : item->command;
        if (item->setting)
        {
            EMBED_CALL(window_preference_row_add, GuiWindowCtx, .window = args->window, .item = item,
                       .name = args->name, .entries = args->entries, .section = args->section);
        }
        else if ((item->list == 0u) && !item->line && (button_section == args->section))
        {
            (void)EMBED_CALL(window_preference_line_add, GuiWindowCtx, .window = args->window, .section = args->section,
                             .at = GUI_PREFERENCE_TYPE_ACTION, .name = label, .key = item->command, .value = label);
        }
        else if ((item->list != 0u) && (args->name[0] == '\0'))
        {
            // a node of the menu, one level down: its chain is its own command
            EMBED_CALL(window_preference_rows_under, GuiWindowCtx, .window = args->window, .at = item->list,
                       .name = item->command, .entries = args->entries, .section = args->section);
        }
    }
}

// Less motion, the window's own switch, added as a row of Settings
EMBED_INLINE void window_preference_reduce_add(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (values->rows < GUI_PREFERENCE_ROWS_MOST)
    {
        GuiPreferenceRow *const row = &values->row[values->rows];
        memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
        VerbaLinea label = {row->label, sizeof(row->label), 0ULL};
        mmgr_line_put(&label, "Reduce Motion");
        (void)mmgr_line_finish(&label);
        VerbaLinea key = {row->key, sizeof(row->key), 0ULL};
        mmgr_line_put(&key, GUI_WINDOW_REDUCE_KEY);
        (void)mmgr_line_finish(&key);
        row->type = GUI_PREFERENCE_TYPE_SWITCH;
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_put(&value, window->motion_saved ? "on" : "off");
        (void)mmgr_line_finish(&value);
        values->rows += 1u;
    }
}

// Every section's rows read again from the catalog's Preferences menu and the settings in `entries`, one section after
// another so that each section's rows stand together. Less motion, the window's own switch, ends Settings. Colors has
// no rows
EMBED_INLINE void window_preference_rows_read(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    GuiPreferences *const preferences = &window->preferences;
    values->rows = 0u;
    for (unsigned int section = 0u; section < GUI_PREFERENCES_SECTIONS; section += 1u)
    {
        preferences->section_first[section] = values->rows;
        for (unsigned int menu = 0u; (section != GUI_PREFERENCES_COLORS) && (menu < cli_menus()); menu += 1u)
        {
            if (EMBED_CALL(window_is, GuiWindowCtx, .name = cli_menu_title_text(menu), .key = "Preferences"))
            {
                EMBED_CALL(window_preference_rows_under, GuiWindowCtx, .window = window, .at = cli_menu_list(menu),
                           .name = "", .entries = args->entries, .section = section);
            }
        }
        if (section == GUI_PREFERENCES_SETTINGS)
        {
            EMBED_CALL(window_preference_reduce_add, GuiWindowCtx, .window = window);
        }
        else if (section == GUI_PREFERENCES_KEYS)
        {
            EMBED_CALL(window_preference_keys_add, GuiWindowCtx, .window = window);
        }
        else if (section == GUI_PREFERENCES_FORCE)
        {
            EMBED_CALL(window_preference_force_add, GuiWindowCtx, .window = window);
        }
        else if (section == GUI_PREFERENCES_NOTICES)
        {
            EMBED_CALL(window_preference_notices_add, GuiWindowCtx, .window = window);
        }
        preferences->section_rows[section] = values->rows - preferences->section_first[section];
    }
    // the part reads each row's type to know how a key or a press changes the row
    for (unsigned int row = 0u; row < values->rows; row += 1u)
    {
        preferences->type[row] = values->row[row].type;
    }
    preferences->first = preferences->section_first[preferences->section];
    preferences->rows = preferences->section_rows[preferences->section];
    preferences->marked = (preferences->marked < preferences->rows) ? preferences->marked : 0u;
}

// the zoom row `at` set to the step `step`: the step saved, the row's value said, and the window laid out again where
// the zoom moved
EMBED_INLINE void window_preference_zoom_set(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    GuiPreferenceRow *const row = &values->row[args->at];
    const int zoomed = args->step != values->zoom_step;
    values->zoom_step = args->step;
    const unsigned int percent =
        EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg, .step = args->step);
    EMBED_CALL(window_percent_save, GuiWindowCtx, .key = GUI_WINDOW_ZOOM_KEY, .percent = percent);
    EMBED_CALL(window_percent_text, GuiWindowCtx, .value_out = row->value, .percent = percent);
    EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = zoomed);
}

// The percent row `at` set to `percent`: the percent saved and the row's value said. The bar's and the editor's
// opacity also move what the window draws
EMBED_INLINE void window_preference_percent_set(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    GuiPreferenceRow *const row = &values->row[args->at];
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_BAR_KEY))
    {
        values->bar_percent = args->percent;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_EDITOR_KEY))
    {
        values->editor_percent = args->percent;
    }
    EMBED_CALL(window_percent_save, GuiWindowCtx, .key = row->key, .percent = args->percent);
    EMBED_CALL(window_percent_text, GuiWindowCtx, .value_out = row->value, .percent = args->percent);
    EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 0);
}

// A slider dragged: the zoom's or the percent's value at the share of its track the pointer stands at, in thousandths.
// The value goes to the nearest step of the zoom, and to the nearest whole percent
EMBED_INLINE void window_preference_slid(const GuiWindowCtx *args)
{
    GuiPreferenceValues *const values = &args->window->preference_values;
    if (args->change->at >= values->rows)
    {
        return;
    }
    const GuiPreferenceRow *const row = &values->row[args->change->at];
    const unsigned int share = (args->change->share > 1000u) ? 1000u : args->change->share;
    if (row->type == GUI_PREFERENCE_TYPE_ZOOM)
    {
        const unsigned int step = ((share * (GUI_PREFERENCE_ZOOM_STEPS - 1u)) + 500u) / 1000u;
        if (step != values->zoom_step)
        {
            EMBED_CALL(window_preference_zoom_set, GuiWindowCtx, .window = args->window, .at = args->change->at,
                       .step = step);
        }
    }
    else if (row->type == GUI_PREFERENCE_TYPE_PERCENT)
    {
        const unsigned int percent = ((share * GUI_PREFERENCE_OPACITY_MAX) + 500u) / 1000u;
        unsigned int standing = 0u;
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = row->value, .percent_out = &standing);
        if (percent != standing)
        {
            EMBED_CALL(window_preference_percent_set, GuiWindowCtx, .window = args->window, .at = args->change->at,
                       .percent = percent);
        }
    }
}

// a row of Settings moved: what it now stands at taken and saved in orior's folder, by the row's type
EMBED_INLINE void window_preference_step(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (args->change->at >= values->rows)
    {
        return;
    }
    GuiPreferenceRow *const row = &values->row[args->change->at];
    if (row->type == GUI_PREFERENCE_TYPE_ZOOM)
    {
        EMBED_CALL(window_preference_zoom_set, GuiWindowCtx, .window = window, .at = args->change->at,
                   .step = EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg,
                                      .step = values->zoom_step, .by = args->change->by, .round = args->change->round));
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = GUI_WINDOW_REDUCE_KEY))
    {
        // chosen, it asks for the opposite of what the window now does
        window->motion_saved = !window->motion_still;
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = GUI_WINDOW_REDUCE_KEY,
                   .value = window->motion_saved ? "true" : "false");
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_put(&value, window->motion_saved ? "on" : "off");
        (void)mmgr_line_finish(&value);
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 0);
    }
    else if (row->type == GUI_PREFERENCE_TYPE_SWITCH)
    {
        // a switch turned over, saved as the CLI saves it
        const int on = !EMBED_CALL(window_is, GuiWindowCtx, .name = row->value, .key = "on");
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = row->key, .value = on ? "true" : "false");
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_put(&value, on ? "on" : "off");
        (void)mmgr_line_finish(&value);
    }
    else if (row->type == GUI_PREFERENCE_TYPE_PERCENT)
    {
        unsigned int standing = 0u;
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = row->value, .percent_out = &standing);
        EMBED_CALL(window_preference_percent_set, GuiWindowCtx, .window = window, .at = args->change->at,
                   .percent = EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg,
                                         .percent = standing, .by = args->change->by, .round = args->change->round));
    }
    else if (row->type == GUI_PREFERENCE_TYPE_COUNT)
    {
        // a count moved one at a time, never below 0
        unsigned long long end = 0ULL;
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = row->value, .cap = sizeof(row->value)});
        unsigned long long count = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = row->value, .at = 0ULL,
                                              .size = size, .end = &end);
        count = (args->change->by < 0) ? ((count == 0ULL) ? 0ULL : count - 1ULL) : count + 1ULL;
        VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
        mmgr_line_decimal(&value, count, 1u);
        (void)mmgr_line_finish(&value);
        EMBED_CALL(window_setting_save, GuiWindowCtx, .key = row->key, .value = row->value);
    }
    else if (row->type == GUI_PREFERENCE_TYPE_TEXT)
    {
        // a row of text begins typing in place, from the value it holds
        GuiPreferences *const preferences = &window->preferences;
        const unsigned long long size =
            cellul.len(&(CatenaFinitaCfg){.src = row->value, .cap = sizeof(preferences->entered) - 1u});
        memor.cpy(&(MemoriaCfg){.dst = preferences->entered, .src = row->value, .bytes = size});
        // a value's bytes are fewer than its field holds, far inside what an unsigned int counts
        preferences->entered_length = (unsigned int)size;
        preferences->entering = 1;
    }
}

// the text typed for the row `at` taken as its value and saved in orior's folder
EMBED_INLINE void window_preference_typed(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (args->change->at >= values->rows)
    {
        return;
    }
    GuiPreferenceRow *const row = &values->row[args->change->at];
    VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
    mmgr_line_put_n(&value, window->preferences.entered, window->preferences.entered_length);
    (void)mmgr_line_finish(&value);
    EMBED_CALL(window_setting_save, GuiWindowCtx, .key = row->key, .value = row->value);
}

EMBED_INLINE int window_command_run(const GuiWindowCtx *args);
EMBED_INLINE void window_editor_bar_numbers(const GuiWindowCtx *args);
EMBED_INLINE void window_editor_bar_layout(const GuiWindowCtx *args);
EMBED_INLINE void window_tabs_bar_numbers(const GuiWindowCtx *args);
EMBED_INLINE void window_tabs_bar_layout(const GuiWindowCtx *args);
EMBED_INLINE void window_tabs_follow(const GuiWindowCtx *args);
EMBED_INLINE void window_tabs_slide(const GuiWindowCtx *args);

// The row `at` taken out of the rows, and the rows after it moved up a place. The section that held it holds one row
// fewer, and each section after it starts a row sooner
EMBED_INLINE void window_preference_row_remove(const GuiWindowCtx *args)
{
    GuiPreferenceValues *const values = &args->window->preference_values;
    GuiPreferences *const preferences = &args->window->preferences;
    if (args->at >= values->rows)
    {
        return;
    }
    const unsigned int section = values->row[args->at].section;
    for (unsigned int row = args->at; row + 1u < values->rows; row += 1u)
    {
        values->row[row] = values->row[row + 1u];
        preferences->type[row] = preferences->type[row + 1u];
    }
    values->rows -= 1u;
    preferences->section_rows[section] -= (preferences->section_rows[section] != 0u) ? 1u : 0u;
    for (unsigned int after = section + 1u; after < GUI_PREFERENCES_SECTIONS; after += 1u)
    {
        preferences->section_first[after] -= (preferences->section_first[after] != 0u) ? 1u : 0u;
    }
    preferences->first = preferences->section_first[preferences->section];
    preferences->rows = preferences->section_rows[preferences->section];
    preferences->marked = (preferences->marked < preferences->rows)
                              ? preferences->marked
                              : ((preferences->rows != 0u) ? preferences->rows - 1u : 0u);
}

// A button pressed: its row's command run, with the row's note as its word where the note holds one. A type of notice
// shown again leaves Notices
EMBED_INLINE void window_preference_acted(const GuiWindowCtx *args)
{
    GuiPreferenceValues *const values = &args->window->preference_values;
    if (args->change->at >= values->rows)
    {
        return;
    }
    const GuiPreferenceRow *const row = &values->row[args->change->at];
    const char *const words[1] = {row->note};
    const unsigned int count = (row->note[0] != '\0') ? 1u : 0u;
    if (!EMBED_CALL(window_command_run, GuiWindowCtx, .window = args->window, .name = row->key, .words = words,
                    .count = count))
    {
        char said[GUI_PREFERENCES_TEXT * 2u];
        VerbaLinea line = {said, sizeof(said), 0ULL};
        mmgr_line_put(&line, "The window has no part for ");
        mmgr_line_put(&line, row->label);
        mmgr_line_put(&line, " yet.");
        (void)mmgr_line_finish(&line);
        cli_notice_post(said, 1);
        return;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = row->key, .key = "notice-show-again"))
    {
        EMBED_CALL(window_preference_row_remove, GuiWindowCtx, .window = args->window, .at = args->change->at);
    }
}

// A row's keys set, as `keyed` is 1, to the keys pressed, or taken off as it is 0. The row's key is `keys.` and the
// command; the keys are saved under it and laid over the command's keys at once
EMBED_INLINE void window_preference_keyed(const GuiWindowCtx *args)
{
    GuiPreferenceValues *const values = &args->window->preference_values;
    if (args->change->at >= values->rows)
    {
        return;
    }
    GuiPreferenceRow *const row = &values->row[args->change->at];
    VerbaLinea value = {row->value, sizeof(row->value), 0ULL};
    if (args->on)
    {
        mmgr_line_put_n(&value, args->window->preferences.entered, args->window->preferences.entered_length);
    }
    (void)mmgr_line_finish(&value);
    EMBED_CALL(window_setting_save, GuiWindowCtx, .key = row->key, .value = row->value);
    cli_commands_keys_own(row->key + sizeof(GUI_WINDOW_KEYS_KEY) - 1u, row->value);
}

// The palette `at` of Preferences' list chosen: the window's shows its own colors from the first; a plugin's shows
// the window's colors with that plugin's tokens, from the first token
EMBED_INLINE void window_palette_choose(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferences *const preferences = &window->preferences;
    if (args->change->at == 0u)
    {
        preferences->color_first = 0u;
        return;
    }
    GuiPaletteOwn *const choice = &window->palette_choice_own;
    choice->colors = window->palette_own.colors;
    choice->colors.token = choice->token;
    EMBED_CALL(gui_palette_own.tokens_clear, GuiPaletteOwnCfg, .own = choice);
    (void)EMBED_CALL(cli_plugins_id_each.palette_each, CliPluginsIdEachCfg,
                     .id = preferences->palette_name[args->change->at], .each = window_token_each, .context = choice);
    preferences->color_first =
        EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = &choice->colors) -
        choice->colors.token_count;
}

// a token of the chosen plugin's palette made the color typed: in the palette Preferences shows, in the plugin's
// plugin.json under the reader's plugins folder, and in the window's own tokens where the shown file is of that plugin
EMBED_INLINE void window_plugin_color_set(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPaletteOwn *const choice = &window->palette_choice_own;
    const GuiPreferences *const preferences = &window->preferences;
    if (args->change->at < preferences->color_first)
    {
        return;
    }
    const unsigned int token = args->change->at - preferences->color_first;
    if (token >= choice->colors.token_count)
    {
        return;
    }
    const char *const id = preferences->palette_name[preferences->palette_chosen];
    const char *const name = choice->token[token].name;
    char value[GUI_PREFERENCES_TEXT];
    EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = choice, .at = args->change->at,
               .color = args->change->color);
    EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = args->change->color, .text = value,
               .size = sizeof(value), .alpha_always = 1);
    (void)EMBED_CALL(cli_plugins_id_name_value.palette_set, CliPluginsIdNameValueCfg, .id = id, .name = name,
                     .value = value);
    if (cellul.eq(&(CatenaFinitaCfg){.src = window->tokens_id, .other = id, .cap = sizeof(window->tokens_id)}))
    {
        for (unsigned int at = 0u; at < window->palette_own.colors.token_count; at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = window->palette_own.token[at].name, .other = name, .cap = GUI_PALETTE_TOKEN_NAME}))
            {
                window->palette_own.token[at].color = args->change->color;
            }
        }
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 1);
    }
    window->preferences_pending = 1;
}

// what a key or a press changed in Preferences taken and saved in orior's folder
EMBED_INLINE void window_preference_change(const GuiWindowCtx *args)
{
    args->window->preferences_pending = 1;
    if (args->change->type == GUI_PREFERENCE_SLID)
    {
        EMBED_CALL(window_preference_slid, GuiWindowCtx, .window = args->window, .change = args->change);
    }
    else if (args->change->type == GUI_PREFERENCE_ACTED)
    {
        EMBED_CALL(window_preference_acted, GuiWindowCtx, .window = args->window, .change = args->change);
    }
    else if ((args->change->type == GUI_PREFERENCE_KEYED) || (args->change->type == GUI_PREFERENCE_UNKEYED))
    {
        EMBED_CALL(window_preference_keyed, GuiWindowCtx, .window = args->window, .change = args->change,
                   .on = args->change->type == GUI_PREFERENCE_KEYED);
    }
    else if (args->change->type == GUI_PREFERENCE_CLOSED)
    {
        // Preferences closed itself: it stays closed, and a toggle would open it again
        args->window->preferences.displayed = 0;
        args->window->preferences.entering = 0;
    }
    else if (args->change->type == GUI_PREFERENCE_STEPPED)
    {
        window_preference_step(args);
    }
    else if (args->change->type == GUI_PREFERENCE_TYPED)
    {
        window_preference_typed(args);
    }
    else if (args->change->type == GUI_PREFERENCE_PALETTE)
    {
        window_palette_choose(args);
    }
    else if ((args->change->type == GUI_PREFERENCE_COLORED) && (args->window->preferences.palette_chosen != 0u))
    {
        window_plugin_color_set(args);
    }
    else if (args->change->type == GUI_PREFERENCE_COLORED)
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

// the color of the palette's whose name follows `color.` in `key` set to the text `value`, where the text reads as a
// color other than the one standing
EMBED_INLINE void window_color_apply(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const unsigned int colors =
        EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = window->palette);
    for (unsigned int at = 0u; at < colors; at += 1u)
    {
        char name[GUI_PREFERENCES_TEXT];
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = window->palette, .at = at, .name = name,
                   .size = sizeof(name));
        if (!cellul.eq(&(CatenaFinitaCfg){
                .src = name, .other = args->key + sizeof(GUI_WINDOW_COLOR_KEY) - 1u, .cap = sizeof(name)}))
        {
            continue;
        }
        const GuiColor current =
            EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = window->palette, .at = at);
        GuiColor color = 0u;
        if (EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = args->value,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = GUI_PREFERENCES_TEXT}),
                       .current = current, .color_pointer = &color) &&
            (color != current))
        {
            EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &window->palette_own, .at = at,
                       .color = color);
        }
        return;
    }
}

// the setting `key` at the text `value` taken into what the window draws with: the zoom, the bar's and the editor's
// opacity, less motion, a color of `color.` and a command's own keys of `keys.`; the key of any other setting is the
// CLI's alone and changes nothing here
EMBED_INLINE void window_setting_apply(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->key, .key = GUI_WINDOW_ZOOM_KEY))
    {
        unsigned int zoom_percent = 0u;
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = args->value, .percent_out = &zoom_percent);
        (void)EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = zoom_percent,
                         .step_pointer = &values->zoom_step);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->key, .key = GUI_WINDOW_BAR_KEY))
    {
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = args->value, .percent_out = &values->bar_percent);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->key, .key = GUI_WINDOW_EDITOR_KEY))
    {
        EMBED_CALL(window_percent_read, GuiWindowCtx, .value = args->value, .percent_out = &values->editor_percent);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = args->key, .key = GUI_WINDOW_REDUCE_KEY))
    {
        window->motion_saved = cellul.eq(&(CatenaFinitaCfg){.src = args->value, .other = "true", .cap = 8ULL});
    }
    else if (cellul.starts(&(CatenaFinitaCfg){.src = args->key, .other = GUI_WINDOW_COLOR_KEY, .cap = 64ULL}))
    {
        EMBED_CALL(window_color_apply, GuiWindowCtx, .window = window, .key = args->key, .value = args->value);
    }
    else if (cellul.starts(&(CatenaFinitaCfg){.src = args->key, .other = GUI_WINDOW_KEYS_KEY, .cap = 64ULL}))
    {
        cli_commands_keys_own(args->key + sizeof(GUI_WINDOW_KEYS_KEY) - 1u, args->value);
    }
}

// the settings orior's folder saved, the CLI's `entries`, taken over the window's defaults one at a time
EMBED_INLINE void window_settings_read(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    values->zoom_step = GUI_PREFERENCE_ZOOM_DEFAULT_STEP;
    values->bar_percent = GUI_WINDOW_BAR_PERCENT;
    values->editor_percent = GUI_WINDOW_EDITOR_PERCENT;
    window->motion_saved = 0;
    for (unsigned int each = 0u; (args->entries != NULL) && (each < args->entries->count); each += 1u)
    {
        EMBED_CALL(window_setting_apply, GuiWindowCtx, .window = window, .key = args->entries->entry[each].key,
                   .value = args->entries->entry[each].value);
    }
    EMBED_CALL(window_preference_rows_read, GuiWindowCtx, .window = window, .entries = args->entries);
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
               .glyphs = &args->window->glyphs, .width = width, .height = height, .scale = scale);
    args->window->preferences_pending = 1;
    // the raster's pixels stand for as long as the view does, since the system shows them again as it is asked to
    // paint: made once, with capacity for the largest monitor, and a size past that saves the size it has
    if (args->window->preferences_raster.pixels == NULL)
    {
        const unsigned int most_width = (width > args->window->display_width) ? width : args->window->display_width;
        const unsigned int most_height =
            (height > args->window->display_height) ? height : args->window->display_height;
        // the raster Preferences draws into, and the one its lattice is drawn into, both at the same capacity
        if (!EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &args->window->preferences_raster,
                        .width = most_width, .height = most_height) ||
            !EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &args->window->preferences_ground_raster,
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
    // Preferences' own lattice laid over its window at its scale, as the main window's is over the main window
    args->window->preferences_ground_raster.width = width;
    args->window->preferences_ground_raster.height = height;
    (void)EMBED_CALL(gui_background_raster.layout, GuiBackgroundRasterCfg,
                     .background_fill = &args->window->preferences_ground,
                     .raster = &args->window->preferences_ground_raster, .width = width, .height = height,
                     .scale = scale, .palette = args->window->palette);
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
                     .preferences = &window->preferences,
                     .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = window),
                     .x = args->input->x, .y = args->input->y, .change = &change);
    EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
}

static void window_preferences_move(const GuiInputHandlerCfg *args)
{
    GuiWindow *const window = args->window;
    if (window->preferences.sliding || window->preferences.scrolling)
    {
        // a slider or the scrollbar's thumb held: the pointer drags it
        GuiPreferenceChange change = {GUI_PREFERENCE_NOTHING, 0u, 0, 0, 0u, 0u};
        if (EMBED_CALL(gui_preferences_palette_change.slide, GuiPreferencesPaletteChangeCfg,
                       .preferences = &window->preferences,
                       .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = window),
                       .x = args->input->x, .y = args->input->y, .change = &change))
        {
            EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
        }
        return;
    }
    args->window->preferences_pending =
        EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg,
                   .preferences = &args->window->preferences,
                   .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = args->window),
                   .x = args->input->x, .y = args->input->y) ||
        args->window->preferences_pending;
}

// the left button let go: a slider or the scrollbar's thumb held is let go
static void window_preferences_release(const GuiInputHandlerCfg *args)
{
    if (args->input->button != GUI_BUTTON_LEFT)
    {
        return;
    }
    GuiPreferenceChange change = {GUI_PREFERENCE_NOTHING, 0u, 0, 0, 0u, 0u};
    args->window->preferences_pending =
        EMBED_CALL(gui_preferences_palette_change.release, GuiPreferencesPaletteChangeCfg,
                   .preferences = &args->window->preferences,
                   .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = args->window),
                   .x = args->input->x, .y = args->input->y, .change = &change) ||
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
                   .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = args->window),
                   .rows = args->input->rows) ||
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
               .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = window),
               .name = args->input->name, .change = &change);
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
    // The close mark and each section's tab take their presses; the rest of the head moves the window. The head runs
    // from the window's top to the foot of the tabs' row
    hits->client[0] = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = preferences);
    hits->clients = 1u;
    GuiRect tab = {0, 0, 0, 0};
    for (unsigned int section = 0u; (section < GUI_PREFERENCES_SECTIONS) && (hits->clients < GUI_WINDOW_HIT_CLIENTS);
         section += 1u)
    {
        tab = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                         .preferences_read = preferences, .at = section);
        hits->client[hits->clients] = tab;
        hits->clients += 1u;
    }
    // a client's sides are far inside what an int represents
    hits->caption = (GuiRect){0, 0, (int)preferences->width, tab.y + tab.height};
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
    // the lattice under the panes where it was laid at the window's size, else none, the deep color standing for it
    const GuiRaster *const ground = &args->window->preferences_ground_raster;
    const int laid = args->window->preferences_ground.laid && (ground->pixels != NULL) &&
                     (ground->width == args->window->preferences_raster.width) &&
                     (ground->height == args->window->preferences_raster.height);
    EMBED_CALL(gui_preferences_palette_values.draw, GuiPreferencesPaletteValuesCfg,
               .preferences_read = &args->window->preferences, .raster = &args->window->preferences_raster,
               .glyphs = &args->window->glyphs,
               .palette = EMBED_CALL(window_preferences_palette, GuiWindowCtx, .window = args->window),
               .values = &args->window->preference_values, .ground = laid ? ground : NULL);
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

// Every panel over the editor hidden. Where no panel shows, nothing changes and no frame is owed
EMBED_INLINE void window_panels_hide(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window;
    if ((window->hover.panel == NULL) && (window->suggest.panel == NULL) && (window->signature.panel == NULL))
    {
        cli_servers_panels_hide();
        return;
    }
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
        EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = args->window, .event = GUI_PANE_SAVED);
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

static void window_save_all(void *context)
{
    EMBED_CALL(window_tabs_save, GuiWindowCtx, .window = (GuiWindow *)context);
}

// the view open on the file at `path` closed and the file opened again, its cursor where it stood, so the view reads
// the file as the disk now has it; the view shown before stays shown
static void window_file_reread(void *context, const char *path)
{
    GuiWindow *const window = (GuiWindow *)context;
    const unsigned int count = cli_view_count();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const open = cli_view_path_at(at);
        if ((open == NULL) || !cellul.eq(&(CatenaFinitaCfg){.src = open, .other = path, .cap = CLI_PATH_MAX_LEN}))
        {
            continue;
        }
        const int shown = cli_view_shown();
        CliPosition cursor = {0ULL, 0ULL};
        CliPosition anchor = {0ULL, 0ULL};
        (void)cli_view_cursor(&(CliViewCursorCfg){.key = cli_view_key(at), .cursor = &cursor, .anchor = &anchor});
        cli_view_file_close(at);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        (void)cli_view_file_open(cli_command_line_root(), path, window->rows, window->columns, &cursor, &err_msg);
        // the view opened again stands last; the one shown before, where it was another, stands one place nearer
        // the front where it stood past the view closed
        if ((shown >= 0) && ((unsigned int)shown != at))
        {
            cli_view_show((shown > (int)at) ? (unsigned int)(shown - 1) : (unsigned int)shown);
        }
        EMBED_CALL(window_shown, GuiWindowCtx, .window = window);
        return;
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

// a settings file opened, each made an empty object where it is not there yet: the tree's `.orior/settings.json` in the
// editor where `on` is 1, else the reader's own in orior's folder, outside every tree, as the system opens it
EMBED_INLINE void window_settings_file_open(const GuiWindowCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    VerbaLinea line = {path, sizeof(path), 0ULL};
    const char *const root = cli_command_line_root();
    if (args->on && (root != NULL))
    {
        mmgr_line_put(&line, root);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, CLI_SETTINGS_TREE);
    }
    else if (!args->on && EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = path))
    {
        line.at = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = sizeof(path)});
        mmgr_line_put(&line, "/settings.json");
    }
    if ((mmgr_line_finish(&line) == 0ULL) || !EMBED_CALL(cli_path_err_msg_type_internal.settings_ensure,
                                                         CliPathErrMsgTypeCtx, .path_out = path, .err_msg = &err_msg))
    {
        cli_notice_post((mmgr_line_finish(&err_msg) != 0ULL) ? why : "The settings file could not be opened.", 1);
        return;
    }
    if (args->on)
    {
        EMBED_CALL(window_file_open, GuiWindowCtx, .window = args->window, .name = path, .place = NULL);
        return;
    }
    if (!EMBED_CALL(cli_path_err_msg_name_internal.home_reveal, CliPathErrMsgNameCtx, .path = path,
                    .err_msg = &err_msg))
    {
        cli_notice_post((mmgr_line_finish(&err_msg) != 0ULL) ? why : "The settings file could not be opened.", 1);
    }
}

// the tool `at` of the strip shown, or hidden where it shows; the terminal's panel opened or closed beside whatever
// side tool shows, the footer tool's pane standing in it leaving; a footer tool's pane shown in the panel at the foot,
// the terminal closed where it was open, or hidden where it shows; the explorer listed as it shows
EMBED_INLINE void window_tool_toggle(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if ((args->at >= g_cli_tools_count) || g_cli_tools[args->at].menu)
    {
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[args->at].name, .other = "terminal", .cap = 16ULL}))
    {
        window->strip.foot = GUI_STRIP_NONE;
        (void)cli_terminal_tabs_command("toggle", NULL, 0u);
        EMBED_CALL(window_terminal, GuiWindowCtx, .window = window);
        return;
    }
    const GuiPaneTable *const table = EMBED_CALL(gui_pane.table_of, GuiPaneCfg, .tool = g_cli_tools[args->at].name);
    if ((table != NULL) && table->foot)
    {
        // the tools are fewer than an int counts
        window->strip.foot = (window->strip.foot == (int)args->at) ? GUI_STRIP_NONE : (int)args->at;
        if (window->terminal.panel.open && (window->strip.foot >= 0))
        {
            (void)cli_terminal_tabs_command("toggle", NULL, 0u);
        }
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
        EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = window, .event = GUI_PANE_SHOWN);
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
    EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = window, .event = GUI_PANE_SHOWN);
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
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = checks, .key = GUI_WINDOW_AUTHORS_KEY))
    {
        return EMBED_CALL(cli_git_log.authors, CliGitLogCfg, .at = 0u);
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = checks, .key = CLI_CRUMBS_KEY))
    {
        return EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u);
    }
    return cli_terminal_tabs_checked(checks);
}

static void window_key_take(const GuiWindowCtx *args);

// the view whose key is `name`: its place, or the views' count where none is
EMBED_INLINE unsigned int window_tab_at(const GuiWindowCtx *args)
{
    const unsigned int count = cli_view_count();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = cli_view_key(at), .other = args->name, .cap = CLI_PATH_MAX_LEN}))
        {
            return at;
        }
    }
    return count;
}

// `text` put on the system's clipboard
EMBED_INLINE void window_clipboard_put(const GuiWindowCtx *args)
{
    (void)cli_clipboard_text_write(&(CliSizeTextCtx){
        .size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}), .text = args->name});
}

// the commands the parts' menus run with their rows' `words`, `count` of them: a path opened, a folder of the tree
// opened or closed; a path, a commit's id or its message put on the clipboard; the explorer listed again or its folders
// closed; the tab of a path closed, or every tab but it; and a commit's files opened or closed: 1, or 0 where `name` is
// none of them
EMBED_INLINE int window_words_run(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const name = args->name;
    const char *const first = (args->count != 0u) ? args->words[0] : "";
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "file-open"))
    {
        if ((args->count != 0u) && !cli_files_explorer_open(first))
        {
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = first, .place = NULL);
        }
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "file-copy-path") ||
        EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "commit-copy-id"))
    {
        if (args->count != 0u)
        {
            EMBED_CALL(window_clipboard_put, GuiWindowCtx, .name = first);
        }
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "commit-copy-message"))
    {
        char text[GUI_WINDOW_MESSAGE_MAX];
        VerbaLinea out = {text, sizeof(text), 0ULL};
        if ((args->count != 0u) &&
            EMBED_CALL(cli_git_log.message, CliGitLogCfg, .name = first, .out = &out,
                       .root = cli_command_line_root()) &&
            mmgr_line_finish(&out))
        {
            EMBED_CALL(window_clipboard_put, GuiWindowCtx, .name = text);
        }
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "explorer-refresh"))
    {
        cli_files_explorer_refresh();
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "explorer-collapse"))
    {
        cli_files_explorer_collapse();
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "tab-close"))
    {
        const unsigned int at = EMBED_CALL(window_tab_at, GuiWindowCtx, .name = first);
        if (at < cli_view_count())
        {
            EMBED_CALL(window_tab_close, GuiWindowCtx, .window = window, .at = at);
        }
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "tab-close-others"))
    {
        // every other view closed once, the last first, a changed one standing for a second close
        for (unsigned int at = cli_view_count(); at > 0u; at -= 1u)
        {
            if (!cellul.eq(&(CatenaFinitaCfg){.src = cli_view_key(at - 1u), .other = first, .cap = CLI_PATH_MAX_LEN}))
            {
                EMBED_CALL(window_tab_close, GuiWindowCtx, .window = window, .at = at - 1u);
            }
        }
        return 1;
    }
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "commit-files"))
    {
        (void)EMBED_CALL(cli_git_log.toggle_id, CliGitLogCfg, .name = first, .root = cli_command_line_root());
        return 1;
    }
    return 0;
}

// a Comms command run by the CLI: the Comms pane shown in the panel at the foot where the command shows a view of it,
// the text it answers put on the clipboard and the file it answers opened: 1, or 0 where `name` is no Comms command
EMBED_INLINE int window_comms_run(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    char clipboard_text[GUI_WINDOW_MESSAGE_MAX];
    char open_text[CLI_PATH_MAX_LEN];
    VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
    VerbaLinea open_out = {open_text, sizeof(open_text), 0ULL};
    int shown = 0;
    if (!EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = args->name, .words = args->words,
                    .count = args->count, .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown))
    {
        return 0;
    }
    for (unsigned int at = 0u; shown && (at < g_cli_tools_count); at += 1u)
    {
        // the tools are fewer than an int counts
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_tools[at].name, .other = "comms", .cap = 16ULL}) &&
            (window->strip.foot != (int)at))
        {
            EMBED_CALL(window_tool_toggle, GuiWindowCtx, .window = window, .at = at);
        }
    }
    if (mmgr_line_finish(&clipboard) != 0ULL)
    {
        EMBED_CALL(window_clipboard_put, GuiWindowCtx, .name = clipboard_text);
    }
    if (mmgr_line_finish(&open_out) != 0ULL)
    {
        EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = open_text, .place = NULL);
    }
    window->pending = 1;
    return 1;
}

// the commands of the view's changes and its crumbs: a declaration's line gone to, its word the line; the crumbs shown
// or not by their word `on` or `off`, else turned; the change at the cursor's line shown; and the cursor put on the
// change after or before it: 1, or 0 where `name` is none of them
EMBED_INLINE int window_widgets_run(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const name = args->name;
    const char *const first = (args->count != 0u) ? args->words[0] : "";
    const unsigned long long line = (window->editor.frame != NULL) ? window->editor.frame->cursor.line : 0ULL;
    const int next = EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "next-change");
    CliPosition place = {0ULL, 0ULL};
    int placed = 0;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "symbol-go"))
    {
        placed = EMBED_CALL(cli_crumbs.go, CliCrumbsCfg, .text = first, .place_out = &place);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "breadcrumbs"))
    {
        const int on = EMBED_CALL(window_is, GuiWindowCtx, .name = first, .key = "on");
        const int off = EMBED_CALL(window_is, GuiWindowCtx, .name = first, .key = "off");
        EMBED_CALL(cli_crumbs.shown_set, CliCrumbsCfg,
                   .on = on || (!off && !EMBED_CALL(cli_crumbs.shown, CliCrumbsCfg, .at = 0u)));
        EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = window, .event = GUI_PANE_VIEW);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "change-peek"))
    {
        (void)EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = line);
    }
    else if (next || EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "previous-change"))
    {
        placed = EMBED_CALL(cli_hunks.step, CliHunksCfg, .line = line, .by = next ? 1 : -1, .line_out = &place.line);
    }
    else
    {
        return 0;
    }
    if (placed)
    {
        const CliViewInput positioned = {.type = CLI_VIEW_POSITION, .row = place.line, .column = place.col};
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &positioned, .edited = 0);
    }
    return 1;
}

// the command `name` run with its `count` `words`, its keys `value`, whoever asked for it
EMBED_INLINE int window_command_run(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const name = args->name;
    // 1 where a branch below runs the command, 0 where none does: the window has no part for it
    int ran = 1;
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "exit"))
    {
        window->closing = 1;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preferences") && (args->count >= 2u))
    {
        // a setting the CLI wrote to settings.json as it sent it here, its key and its text the command's words
        EMBED_CALL(window_setting_apply, GuiWindowCtx, .window = window, .key = args->words[0],
                   .value = args->words[1]);
        EMBED_CALL(window_settings_layout, GuiWindowCtx, .window = window, .on = 1);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preferences"))
    {
        EMBED_CALL(window_preferences_toggle, GuiWindowCtx, .window = window);
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "settings-own") ||
             EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "settings-tree"))
    {
        EMBED_CALL(window_settings_file_open, GuiWindowCtx, .window = window,
                   .on = EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "settings-tree"));
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
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = GUI_WINDOW_AUTHORS_KEY))
    {
        // on or off by its word, else turned from what the Git pane shows
        const char *const word = (args->count != 0u) ? args->words[0] : "";
        const int on = EMBED_CALL(window_is, GuiWindowCtx, .name = word, .key = "on");
        const int off = EMBED_CALL(window_is, GuiWindowCtx, .name = word, .key = "off");
        EMBED_CALL(cli_git_log.authors_set, CliGitLogCfg,
                   .on = on || (!off && !EMBED_CALL(cli_git_log.authors, CliGitLogCfg, .at = 0u)),
                   .now_ms = cli_clock_milliseconds());
        window->pending = 1;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "show-changes"))
    {
        // the file's changes against the last commit side by side over the editor, the shown view's without a word
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (EMBED_CALL(cli_diff_view_source_err_msg.open, CliDiffViewSourceErrMsgCfg, .root = cli_command_line_root(),
                       .source = CLI_DIFF_VIEW_HEAD, .path = (args->count != 0u) ? args->words[0] : NULL,
                       .err_msg = &err_msg))
        {
            EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &s_work, .table = &gui_diffview_table,
                       .frame = &window->frame);
        }
        else if (mmgr_line_finish(&err_msg) != 0ULL)
        {
            cli_notice_post(why, 1);
        }
        window->pending = 1;
    }
    else if (EMBED_CALL(window_comms_run, GuiWindowCtx, .window = window, .name = name, .words = args->words,
                        .count = args->count))
    {
        window->pending = 1;
    }
    else if (EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preview") ||
             EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preview-alone"))
    {
        // the mode its word reads, else the command's own turned on or off; the step lays the frame for it
        EMBED_CALL(cli_preview.mode_set, CliPreviewCfg,
                   .mode = EMBED_CALL(window_is, GuiWindowCtx, .name = name, .key = "preview-alone")
                               ? CLI_PREVIEW_ALONE
                               : CLI_PREVIEW_BESIDE,
                   .word = (args->count != 0u) ? args->words[0] : NULL);
        window->pending = 1;
    }
    else if (EMBED_CALL(window_words_run, GuiWindowCtx, .window = window, .name = name, .words = args->words,
                        .count = args->count))
    {
        window->pending = 1;
    }
    else if (EMBED_CALL(window_widgets_run, GuiWindowCtx, .window = window, .name = name, .words = args->words,
                        .count = args->count))
    {
        window->pending = 1;
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
    else
    {
        ran = 0;
    }
    return ran;
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
    // A question asked in the dialog: its title, its body and one to four buttons, after `--error` where it tells of an
    // error. The dialog is modal over the window that holds the focus. The door holds the command until a person
    // answers, and the answer is the label of the button pressed
    if (EMBED_CALL(window_is, GuiWindowCtx, .name = command, .key = "ask"))
    {
        const int error = (count != 0u) && EMBED_CALL(window_is, GuiWindowCtx, .name = words[0], .key = "--error");
        // the flag word, where it stands, comes before the title
        const unsigned int first = error ? 1u : 0u;
        const unsigned int owner =
            (window->preferences.displayed && window->preferences.focused) ? CLI_VIEW_PREFERENCES : CLI_VIEW_MAIN;
        const int asked =
            (count >= first + 3u) &&
            EMBED_CALL(gui_window_dialog_asking.ask, GuiWindowDialogAskCfg, .window = window, .owner = owner,
                       .title = words[first], .body = words[first + 1u], .labels = words + first + 2u,
                       .count = count - first - 2u, .preferred = 0u, .held = 1, .error = error);
        return asked ? CLI_REACH_HOLD : 0;
    }
    CliItem item;
    const int keyed = cli_menus_named(command, &item);
    const int known = keyed ||
                      EMBED_CALL(window_name_among, GuiWindowCtx, .name = command, .words = s_own_commands,
                                 .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])) ||
                      EMBED_CALL(window_name_among, GuiWindowCtx, .name = command, .words = s_view_commands,
                                 .count = sizeof(s_view_commands) / sizeof(s_view_commands[0]));
    // a command of the menus the window runs no part for is refused, the door answering that it is none of the window's
    return known && EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = command,
                               .value = keyed ? item.keys : NULL, .words = words, .count = count);
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

// a menu's choice: the menus laid again and, `on` where an item was chosen, the `item`'s command run with its keys
EMBED_INLINE void window_menu_chosen(const GuiWindowCtx *args)
{
    EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = args->window);
    if (args->on)
    {
        // a part's own row runs with the word its args carry
        const char *const word = cli_menus_own_word(args->item->args);
        const char *const words[1] = {word};
        (void)EMBED_CALL(window_command_run, GuiWindowCtx, .window = args->window, .name = args->item->command,
                         .value = args->item->keys, .words = words, .count = (word != NULL) ? 1u : 0u);
    }
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
        EMBED_CALL(window_menu_chosen, GuiWindowCtx, .window = window, .on = chose, .item = &chosen);
        return;
    }
    if (args->on)
    {
        return;
    }
    // the view over the editor takes its keys, Escape closing it
    if (EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &s_work, .frame = &window->frame, .glyphs = &window->glyphs,
                   .name = name))
    {
        window->pending = 1;
        return;
    }
    // the shown pane's field, the side's or the foot's, takes the key while it has the keys
    if (EMBED_CALL(gui_pane.key, GuiPaneCfg,
                   .table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window),
                   .pane = EMBED_CALL(window_pane, GuiWindowCtx, .window = window), .name = name,
                   .root = cli_command_line_root(), .save_all = window_save_all, .context = window) ||
        EMBED_CALL(gui_pane.key, GuiPaneCfg,
                   .table = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window),
                   .pane = EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window), .name = name,
                   .root = cli_command_line_root(), .save_all = window_save_all, .context = window))
    {
        window->pending = 1;
        return;
    }
    CliItem item;
    if (cli_menus_keyed(name, &item) &&
        EMBED_CALL(window_name_among, GuiWindowCtx, .name = item.command, .words = s_own_commands,
                   .count = sizeof(s_own_commands) / sizeof(s_own_commands[0])))
    {
        (void)EMBED_CALL(window_command_run, GuiWindowCtx, .window = window, .name = item.command, .value = item.keys);
        return;
    }
    const int tool = EMBED_CALL(window_tool_key, GuiWindowCtx, .name = name);
    if (tool >= 0)
    {
        // the tool is among the tools
        EMBED_CALL(window_tool_toggle, GuiWindowCtx, .window = window, .at = (unsigned int)tool);
        return;
    }
    // the change peek takes Escape while it stands, ahead of the panels
    if (EMBED_CALL(cli_hunks.key, CliHunksCfg, .name = name))
    {
        window->pending = 1;
        return;
    }
    if (cli_servers_panels_key(name))
    {
        EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
        return;
    }
    const CliViewInput keyed = {.type = CLI_VIEW_KEY, .name = name};
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
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_POOL_BYTES});
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
    EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = args->window, .event = GUI_PANE_SHOWN);
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
            ? cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner})
            : NULL;
    window->entries = entries;
    EMBED_CALL(cli_tests_pane.begin, CliTestsPaneCfg, .woken = args->woken, .entries = entries);
    EMBED_CALL(cli_runs_pane.begin, CliRunsPaneCfg, .woken = args->woken);
    EMBED_CALL(cli_search_pane.begin, CliSearchPaneCfg, .woken = args->woken, .entries = entries);
    EMBED_CALL(cli_git_log.begin, CliGitLogCfg, .woken = args->woken, .entries = entries);
    EMBED_CALL(cli_changes.begin, CliGitChangesCfg, .woken = args->woken);
    EMBED_CALL(cli_preview.begin, CliPreviewCfg, .entries = entries);
    EMBED_CALL(cli_crumbs.begin, CliCrumbsCfg, .entries = entries);
    EMBED_CALL(cli_job_runs_woken.begin, CliJobRunsWokenCfg, .woken = args->woken);
    EMBED_CALL(cli_debug_pane_view.begin, CliDebugPaneViewCfg, .root = root, .woken = args->woken);
    EMBED_CALL(cli_comms_pane.begin, CliCommsPaneCfg, .woken = args->woken);
    EMBED_CALL(cli_diff_view_option_on.begin, CliDiffViewOptionOnCfg, .entries = entries);
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
        // the colorer colored more lines: their runs, the map's colors, the crumbs and the outline can change, and the
        // text cannot
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1, .on = 1);
        EMBED_CALL(window_pane_read_event, GuiWindowCtx, .window = window, .event = GUI_PANE_VIEW, .on = 1);
        window->pending = 1;
    }
    // a run of tests reported on its own thread: the pane drawn anew
    window->pending = EMBED_CALL(cli_tests_pane.changed, CliTestsPaneCfg, .at = 0u) || window->pending;
    const unsigned long long now_ms = cli_clock_milliseconds();
    // the query runs listed and the opened live runs read on their beats, the strip's Runs mark following
    if (EMBED_CALL(cli_runs_pane.step, CliRunsPaneCfg, .root = cli_command_line_root(), .now_ms = now_ms,
                   .shown = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                       .table_read = &gui_runs_table)))
    {
        EMBED_CALL(cli_runs_pane.marks, CliRunsPaneCfg, .live_out = &window->strip.runs_live,
                   .panicked_out = &window->strip.runs_panicked);
        window->pending = 1;
    }
    // the Search pane's query searched as its rest passes or as the watch marks what it found stale, and the Git
    // pane's graph and the Changes read again the same way while each is in sight
    window->pending =
        EMBED_CALL(cli_search_pane.step, CliSearchPaneCfg, .root = cli_command_line_root(), .now_ms = now_ms) ||
        window->pending;
    window->pending = EMBED_CALL(cli_git_log.step, CliGitLogCfg, .root = cli_command_line_root(), .now_ms = now_ms,
                                 .on = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                                  .table_read = &gui_git_table)) ||
                      window->pending;
    window->pending = EMBED_CALL(cli_changes.step, CliGitChangesCfg, .root = cli_command_line_root(),
                                 .on = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                                  .table_read = &gui_changes_table)) ||
                      window->pending;
    // what the jobs' runs reported on their threads taken into the Run pane's rows, the pane in the foot following its
    // last row
    if (EMBED_CALL(cli_job_runs_woken.step, CliJobRunsWokenCfg, .woken = NULL))
    {
        if (EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window) == &gui_run_table)
        {
            EMBED_CALL(gui_run_pane.follow, GuiPaneCfg, .table = &gui_run_table,
                       .pane = EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window), .frame = &window->frame);
        }
        window->pending = 1;
    }
    // the Debug pane's events read as its sessions' adapters report them
    window->pending =
        EMBED_CALL(cli_debug_pane_view.step, CliDebugPaneViewCfg, .root = cli_command_line_root(), .now_ms = now_ms) ||
        window->pending;
    // the Comms pane listed again as the comms layer moves while in sight, and a send's answer taken in
    window->pending =
        EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .root = cli_command_line_root(), .now_ms = now_ms,
                   .shown = EMBED_CALL(window_table_shown, GuiWindowCtx, .window_read = window,
                                       .table_read = &gui_comms_table)) ||
        window->pending;
    // the preview's rectangle laid again where its mode moved, the editor's size handed on, and its items stepped for
    // the view shown in it, drawn anew where they moved
    {
        // a mode is one of three
        const int preview_mode = (int)EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = now_ms);
        if (preview_mode != window->frame.preview_mode)
        {
            EMBED_CALL(gui_frame_displayed.preview, GuiFrameDisplayedCfg, .frame = &window->frame,
                       .displayed = preview_mode);
            if (window->stage != NULL)
            {
                window->stages_stale = 1;
                EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = window);
            }
            EMBED_CALL(window_view_resize, GuiWindowCtx, .window = window);
            window->pending = 1;
        }
        window->pending =
            EMBED_CALL(cli_preview.step, CliPreviewCfg, .key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u),
                       .now_ms = now_ms, .width_px = window->frame.preview.width,
                       .height_px = window->frame.preview.height, .scale = window->frame.scale) ||
            window->pending;
    }
    if (now_ms >= window->memory_due_ms)
    {
        CliProcessMemory measured;
        window->memory_bytes =
            EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx, .pid = cli_process_own(), .memory = &measured)
                ? measured.working
                : 0ULL;
        cli_pool_telemetry_log();
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
    EMBED_CALL(window_tabs_follow, GuiWindowCtx, .window = args->window);
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame);
    // the view over the editor drawn in its place, else the editor, the shown file's diagnostics underlined on the rows
    // drawn and the map beside it
    if (EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &s_work))
    {
        EMBED_CALL(gui_work.draw, GuiWorkCfg, .work_read = &s_work, .frame = frame, .raster = raster,
                   .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette);
    }
    else
    {
        // the token names' colors kept for the editor's text and the map, read again only where the names change
        EMBED_CALL(gui_editor_palette.colors_keep, GuiEditorPaletteCfg, .editor = &args->window->editor,
                   .palette = palette);
        EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = &args->window->editor,
                   .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .place = frame->editor,
                   .scale = frame->scale);
        const unsigned int code_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                .pixels = GUI_CODE_SIZE, .scale = frame->scale);
        EMBED_CALL(gui_problems_path_raster.underlined, GuiProblemsPathRasterCfg, .editor = &args->window->editor,
                   .raster = raster, .palette = palette, .place = frame->editor,
                   .cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg,
                                      .glyphs = &args->window->glyphs, .size = code_size),
                   .scale = frame->scale);
        EMBED_CALL(gui_minimap_position.draw, GuiMinimapPositionCfg, .map = &args->window->minimap, .raster = raster,
                   .palette = palette, .editor = &args->window->editor, .place = frame->map_whole, .clip = frame->map,
                   .scale = frame->scale,
                   .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = args->window));
        // the editor's scrollbars, down its right side and across its foot, each where the view stands on the grid
        for (int across = 0; across <= 1; across += 1)
        {
            EMBED_CALL(window_editor_bar_layout, GuiWindowCtx, .window = args->window, .on = across);
            unsigned long long total = 0ULL;
            unsigned long long shown = 0ULL;
            unsigned long long first = 0ULL;
            EMBED_CALL(window_editor_bar_numbers, GuiWindowCtx, .window_read = args->window, .on = across,
                       .total_out = &total, .shown_out = &shown, .first_out = &first);
            EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg,
                       .bar_read = across ? &args->window->editor_across : &args->window->editor_down, .raster = raster,
                       .palette = palette, .total = total, .shown = shown, .first = first,
                       .pointed = args->window->editor_bar_dragged == (across ? 2u : 1u));
        }
    }
    // the preview beside the text or over the editor's rectangle, as its mode lays the frame
    EMBED_CALL(gui_preview.draw, GuiPreviewCfg, .raster = raster, .glyphs = &args->window->glyphs, .palette = palette,
               .frame = frame);
    // The tabs row's scrollbar hangs over the editor's top while the row holds more tabs than it shows. It stands
    // only while the row stands whole
    if (frame->tabs_share >= GUI_FIXED_ONE)
    {
        EMBED_CALL(window_tabs_bar_layout, GuiWindowCtx, .window = args->window);
        unsigned long long whole = 0ULL;
        unsigned long long width = 0ULL;
        unsigned long long left = 0ULL;
        EMBED_CALL(window_tabs_bar_numbers, GuiWindowCtx, .window_read = args->window, .total_out = &whole,
                   .shown_out = &width, .first_out = &left);
        // the band the row grows down by takes the row's own ground, as the page's row does
        if (EMBED_CALL(gui_scrollbar.needed, GuiScrollbarCfg, .total = whole, .shown = width))
        {
            EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                       .rect = args->window->tabs_bar.rect, .color = palette->surface);
        }
        EMBED_CALL(gui_scrollbar.draw, GuiScrollbarCfg, .bar_read = &args->window->tabs_bar, .raster = raster,
                   .palette = palette, .total = whole, .shown = width, .first = left,
                   .pointed = args->window->editor_bar_dragged == 3u);
    }
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->side) &&
        (args->window->strip.displayed >= 0))
    {
        // the tool shown is among the tools; its pane drawn where it stands as it slides, the side its rectangle moved
        // by its way gone
        const CliTool *const tool = EMBED_CALL(window_tool, GuiWindowCtx, .window_read = args->window);
        GuiFrame slid = *frame;
        slid.side = frame->side_pane;
        EMBED_CALL(gui_pane.draw, GuiPaneCfg,
                   .table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = args->window),
                   .pane_read = EMBED_CALL(window_pane, GuiWindowCtx, .window = args->window), .frame = &slid,
                   .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette,
                   .title = tool->label, .tree = (args->window->title != NULL) ? args->window->title : "",
                   .cursor_line =
                       (args->window->editor.frame != NULL) ? args->window->editor.frame->cursor.line : 0ULL);
    }
    // the panel drawn where it stands as it arrives, its rectangle and its views' moved down by its way left: the
    // footer tool's pane where one stands in it, else the terminal
    GuiFrame arriving = *frame;
    arriving.terminal = frame->terminal_drawn;
    arriving.terminal_views = frame->terminal_views_drawn;
    const GuiPaneTable *const foot = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = args->window);
    if (foot != NULL)
    {
        // the footer tool is among the tools
        EMBED_CALL(gui_pane.draw, GuiPaneCfg, .table = foot,
                   .pane_read = EMBED_CALL(window_foot_pane_read, GuiWindowCtx, .window_read = args->window),
                   .frame = &arriving, .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons,
                   .palette = palette, .title = g_cli_tools[args->window->strip.foot].label,
                   .tree = (args->window->title != NULL) ? args->window->title : "",
                   .cursor_line =
                       (args->window->editor.frame != NULL) ? args->window->editor.frame->cursor.line : 0ULL);
    }
    else
    {
        EMBED_CALL(gui_terminal.draw, GuiTerminalCfg, .terminal_read = &args->window->terminal, .frame = &arriving,
                   .glyphs = &args->window->glyphs, .raster = raster, .palette = palette);
    }
    // the change peek over the editor, under the lines of the change the CLI shows
    {
        const unsigned int peek_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                .pixels = GUI_CODE_SIZE, .scale = frame->scale);
        EMBED_CALL(gui_change_peek.draw, GuiChangePeekCfg, .raster = raster, .glyphs = &args->window->glyphs,
                   .icons = &args->window->icons, .palette = palette, .editor_read = &args->window->editor,
                   .place = frame->editor, .bounds = frame->inside,
                   .cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg,
                                      .glyphs = &args->window->glyphs, .size = peek_size),
                   .scale = frame->scale);
    }
    window_panels_draw(args);
}

// The base the frame just laid on the window's raster kept whole, the kept raster taken again at the window's size
// where the size moved. A raster that cannot be taken leaves no base kept, and the frames of changes lay their base
EMBED_INLINE void window_base_keep(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiRaster *const raster = window->raster;
    GuiRaster *const kept = &window->base_kept;
    window->base_kept_whole = 0;
    if ((kept->pixels == NULL) || (kept->width != raster->width) || (kept->height != raster->height))
    {
        g_cli_guard->persistent_buf_release(kept->pixels, g_gui_raster_owner);
        kept->pixels = NULL;
        if (!EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = kept, .width = raster->width,
                        .height = raster->height))
        {
            kept->pixels = NULL;
            return;
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = kept->pixels,
                            .src = raster->pixels,
                            .bytes = (size_t)raster->width * raster->height * sizeof(GuiColor)});
    window->base_kept_whole = 1;
}

// The part `caret` of the kept base copied onto the window's raster, row by row; the part lies on both rasters, which
// are the same size, so its sides are not negative
EMBED_INLINE void window_base_copy(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window;
    const GuiRaster *const kept = &window->base_kept;
    GuiRaster *const raster = window->raster;
    const GuiRect part = args->caret;
    for (int y = part.y; y < part.y + part.height; y += 1)
    {
        const size_t at = ((size_t)y * raster->width) + (size_t)part.x;
        memor.cpy(&(MemoriaCfg){
            .dst = raster->pixels + at, .src = kept->pixels + at, .bytes = (size_t)part.width * sizeof(GuiColor)});
    }
}

void gui_window_draw(const GuiWindowCfg *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    // A frame with a clip draws again only the frame's changes, and the caret and the circle stand as they were
    // outside it
    const GuiRect clip = raster->clip;
    const int clipped = (clip.width > 0) && (clip.height > 0);
    args->window->pending = 0;
    if (!clipped)
    {
        // a whole frame draws every change
        args->window->caret_alone = 0;
        args->window->busy_alone = 0;
        args->window->damage = (GuiRect){0, 0, 0, 0};
    }
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
    GuiRaster *const kept = &args->window->base_kept;
    const int kept_fits = args->window->base_kept_whole && (kept->pixels != NULL) && (kept->width == raster->width) &&
                          (kept->height == raster->height);
    if (clipped && kept_fits)
    {
        // a frame of changes alone copies its part of the base the last whole frame laid
        EMBED_CALL(window_base_copy, GuiWindowCtx, .window = args->window, .caret = clip);
    }
    else if (stage != NULL)
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
    if (!clipped)
    {
        // the base a whole frame laid kept for the frames of changes alone after it
        EMBED_CALL(window_base_keep, GuiWindowCtx, .window = args->window);
    }
    // the table the panes lie on: the lattice with the sheet's tint and grain; each pane's corners rounded on it
    const GuiUnder table = EMBED_CALL(gui_glass.table, GuiGlassCfg, .background_fill = background_fill,
                                      .color = palette->deep, .glass = palette->glass, .grain = palette->grain);
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
    // holes the parts shown over the work make in it while the claim holds; while Preferences' window holds the
    // focus, the signal is on its edge and the work's head and left take the right's line
    const GuiColor right_line = palette->work_line;
    const int work_focused = !(args->window->preferences.displayed && args->window->preferences.focused);
    const GuiOutline lines = {
        work_focused ? palette->signal : right_line,
        right_line,
        right_line,
        work_focused ? palette->work_left : right_line,
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
    // A clipped frame draws the caret only where the clip holds the caret and the caret drawn last. The caret writes
    // its pixels itself and saves those under it, so outside the clip it stands as it was
    const GuiRect caret_in = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = caret, .other = clip);
    const GuiRect drawn = args->window->editor.caret_rect;
    const GuiRect drawn_in = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = drawn, .other = clip);
    const int caret_held = (caret_in.width == caret.width) && (caret_in.height == caret.height);
    const int drawn_held =
        !args->window->editor.caret_saved || ((drawn_in.width == drawn.width) && (drawn_in.height == drawn.height));
    const int caret_reached = !clipped || (caret_held && drawn_held);
    if (caret_reached && view_displayed && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = caret) &&
        !EMBED_CALL(window_caret_cover, GuiWindowCtx, .window_read = args->window, .caret = caret))
    {
        (void)EMBED_CALL(gui_editor_raster.caret_draw, GuiEditorRasterCfg, .editor = &args->window->editor,
                         .raster = raster, .palette = palette, .caret = caret);
        args->window->caret_alone = 0;
    }
    else if (!clipped)
    {
        args->window->editor.caret_saved = 0;
    }
    // The busy circle over all of it, the pixels under it saved for its stages' steps. A clipped frame draws it only
    // where the clip holds it
    if (!clipped || !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                                .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                                   .one = args->window->busy.under_rect, .other = clip)))
    {
        EMBED_CALL(gui_busy_frame.draw, GuiBusyFrameCfg, .busy = &args->window->busy, .raster = raster,
                   .palette = palette, .frame = frame);
    }
}

GuiRect gui_window_damage_draw(const GuiWindowCfg *args)
{
    GuiWindow *const window = args->window;
    const GuiRect damage = window->damage;
    window->damage = (GuiRect){0, 0, 0, 0};
    GuiRaster *const back = window->raster;
    GuiRaster *const shown = (window->displayed != NULL) ? window->displayed : back;
    if (shown->pixels == NULL)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    // a raster's sides are far inside what an int represents
    const GuiRect cut = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = damage,
                                   .other = (GuiRect){0, 0, (int)shown->width, (int)shown->height});
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = cut))
    {
        return cut;
    }
    // the frame drawn as a whole frame is, on the raster shown, every write cut to the changes
    window->raster = shown;
    shown->clip = cut;
    gui_window_draw(args);
    shown->clip = (GuiRect){0, 0, 0, 0};
    window->raster = back;
    return cut;
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
    input->x = x;
    input->y = y;
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
    if (EMBED_CALL(gui_change_peek.hit, GuiChangePeekCfg, .x = x, .y = y, .action_out = &input->index))
    {
        input->hit = CLI_HIT_CHANGE_PEEK;
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
        // a crumb as the bar laid it, off the bar's own items
        if ((input->hit == CLI_HIT_NONE) &&
            EMBED_CALL(gui_crumbs.hit, GuiCrumbsCfg, .x = x, .y = y, .index_out = &input->index))
        {
            input->hit = CLI_HIT_STATUS_CRUMB;
        }
        return;
    }
    // the panel at the foot takes every press on it: the footer tool's pane head's action or row where one stands in
    // it, else the terminal's parts, those on none of them to nothing
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.terminal, .x = x, .y = y))
    {
        const GuiPaneTable *const foot = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window);
        if (foot == NULL)
        {
            input->hit = EMBED_CALL(gui_terminal.hit, GuiTerminalCfg, .terminal_read = &window->terminal,
                                    .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .x = x, .y = y,
                                    .index = &input->index, .row = &input->row, .column = &input->column);
            return;
        }
        const GuiPane *const pane = EMBED_CALL(window_foot_pane_read, GuiWindowCtx, .window_read = window);
        input->index = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = foot, .pane_read = pane,
                                  .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .x = x, .y = y);
        if (input->index != 0u)
        {
            input->hit = CLI_HIT_FOOT_ACTION;
        }
        else if (EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = foot, .pane_read = pane, .frame = &window->frame, .x = x,
                            .y = y, .row_pointer = &input->index))
        {
            input->hit = CLI_HIT_FOOT_ROW;
        }
        return;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = x, .y = y))
    {
        // the pane's head's action where the pixel stands on one, else its row
        const GuiPaneTable *const table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window);
        if (table == NULL)
        {
            return;
        }
        const GuiPane *const pane = EMBED_CALL(window_pane_read, GuiWindowCtx, .window_read = window);
        input->index = EMBED_CALL(gui_pane.head_hit, GuiPaneCfg, .table = table, .pane_read = pane,
                                  .frame = &window->frame, .glyphs = (GuiGlyphs *)&window->glyphs, .x = x, .y = y);
        if (input->index != 0u)
        {
            input->hit = CLI_HIT_PANE_ACTION;
        }
        else if (EMBED_CALL(gui_pane.hit, GuiPaneCfg, .table = table, .pane_read = pane, .frame = &window->frame,
                            .x = x, .y = y, .row_pointer = &input->index))
        {
            input->hit = CLI_HIT_PANE_ROW;
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
    // the view over the editor takes every pixel of the editor and the map while it stands there
    const unsigned int work = EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &s_work, .frame = &window->frame,
                                         .glyphs = (GuiGlyphs *)&window->glyphs, .x = x, .y = y);
    if (work != GUI_WORK_OUTSIDE)
    {
        input->hit = CLI_HIT_WORK;
        input->index = work;
        return;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.preview, .x = x, .y = y))
    {
        input->hit = CLI_HIT_PREVIEW;
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
    // the sticky rows stand over the editor's top, so a press on them is theirs before the rows under them
    if (EMBED_CALL(gui_editor_position_cell_scale.sticky_at, GuiEditorPositionCellScaleCfg,
                   .editor_read = &window->editor, .place = window->frame.editor, .cell = cell,
                   .scale = window->frame.scale, .x = x, .y = y, .row = &input->row))
    {
        input->hit = CLI_HIT_STICKY;
    }
    else if (EMBED_CALL(gui_editor_position_cell_scale.cell_at, GuiEditorPositionCellScaleCfg,
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
    const CliViewInput scrolled = {.type = CLI_VIEW_SCROLL, .top = args->hit->row};
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .view_input = &scrolled, .edited = 0);
}

// the control `at` of the change peek pressed: revert writes the CLI's edits into the view shown as one step and
// closes the peek, the change before or after is shown with the cursor on its first line, and close closes it; a press
// on the box off its controls does nothing
EMBED_INLINE void window_change_peek_pressed(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    window->pending = 1;
    if ((args->at == GUI_CHANGE_PEEK_REVERT) && (key != NULL))
    {
        CliEdit edit;
        unsigned int count = 0u;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const int reverted =
            EMBED_CALL(cli_hunks.revert, CliHunksCfg, .edit_out = &edit, .edit_count = &count, .err_msg = &err_msg) &&
            EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = key, .edit = &edit, .count = count,
                       .err_msg = &err_msg);
        if (!reverted && (mmgr_line_finish(&err_msg) != 0ULL))
        {
            cli_notice_post(why, 1);
        }
        EMBED_CALL(cli_hunks.close, CliHunksCfg, .at = 0u);
        EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 1);
        return;
    }
    if ((args->at == GUI_CHANGE_PEEK_PREVIOUS) || (args->at == GUI_CHANGE_PEEK_NEXT))
    {
        unsigned long long line = 0ULL;
        if (EMBED_CALL(cli_hunks.step, CliHunksCfg, .by = (args->at == GUI_CHANGE_PEEK_NEXT) ? 1 : -1,
                       .line_out = &line))
        {
            const CliViewInput positioned = {.type = CLI_VIEW_POSITION, .row = line, .column = 0ULL};
            EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &positioned, .edited = 0);
        }
        return;
    }
    if (args->at == GUI_CHANGE_PEEK_CLOSE)
    {
        EMBED_CALL(cli_hunks.close, CliHunksCfg, .at = 0u);
    }
}

// a press on the gutter's row `hit` at its marks' column: the change of the row's line shown: 1, or 0 where the press
// stands off the column or the line stands in no change
EMBED_INLINE int window_change_mark_pressed(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = window->frame.scale);
    const float cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg,
                                  .glyphs = (GuiGlyphs *)&window->glyphs, .size = size);
    const int text_x =
        window->frame.editor.x + EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                            .editor_read = &window->editor, .cell = cell, .scale = window->frame.scale);
    unsigned long long line = 0ULL;
    unsigned long long col = 0ULL;
    return (key != NULL) &&
           EMBED_CALL(gui_gutter.mark_hit, GuiGutterCfg, .x = args->hit->x, .text_x = text_x,
                      .scale = window->frame.scale) &&
           EMBED_CALL(cli_view_row.position_at, CliViewRowCfg, .key = key, .row = args->hit->row, .column = 0ULL,
                      .line = &line, .col = &col) &&
           EMBED_CALL(cli_hunks.show, CliHunksCfg, .line = line);
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
        EMBED_CALL(window_menu_chosen, GuiWindowCtx, .window = window, .on = chose, .item = &chosen);
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
    case CLI_HIT_CHANGE_PEEK:
        EMBED_CALL(window_change_peek_pressed, GuiWindowCtx, .window = window, .at = hit->index);
        return;
    case CLI_HIT_STATUS_CRUMB:
        // what stands beside the crumb opened as a menu at the crumb
        if (EMBED_CALL(cli_crumbs.menu, CliCrumbsCfg, .at = hit->index, .root = cli_command_line_root()))
        {
            const GuiRect crumb = EMBED_CALL(gui_crumbs.place, GuiCrumbsCfg, .index = hit->index);
            unsigned int count = 0u;
            const CliItem *const rows = cli_menus_own_rows(&count);
            cli_menus_own_open(rows, count, CLI_MENU_PLACE_POINT, crumb.x, crumb.y);
            EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        }
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
    case CLI_HIT_PANE_ACTION:
    case CLI_HIT_FOOT_ACTION: {
        // the pane head's action run, a run of the tree's code saving every view first
        const int foot = hit->hit == CLI_HIT_FOOT_ACTION;
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        EMBED_CALL(gui_pane.action, GuiPaneCfg,
                   .table = foot ? EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window)
                                 : EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window),
                   .pane = foot ? EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window)
                                : EMBED_CALL(window_pane, GuiWindowCtx, .window = window),
                   .which = hit->index, .root = cli_command_line_root(), .save_all = window_save_all,
                   .file_reread = window_file_reread, .context = window);
        window->pending = 1;
        break;
    }
    case CLI_HIT_PANE_ROW:
    case CLI_HIT_FOOT_ROW: {
        // the pane's row pressed: the file it opens, where it opens one
        const int foot = hit->hit == CLI_HIT_FOOT_ROW;
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 0);
        GuiPaneOpen open = {NULL, {0ULL, 0ULL}, 0};
        if (EMBED_CALL(gui_pane.press, GuiPaneCfg,
                       .table = foot ? EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window)
                                     : EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window),
                       .row = hit->index, .frame = &window->frame,
                       .pane_read = foot ? EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window)
                                         : EMBED_CALL(window_pane, GuiWindowCtx, .window = window),
                       .x = hit->x, .root = cli_command_line_root(), .open_out = &open))
        {
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = open.path,
                       .place = open.placed ? &open.place : NULL);
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
    case CLI_HIT_WORK:
        (void)EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &s_work, .frame = &window->frame,
                         .glyphs = &window->glyphs, .index = hit->index, .x = hit->x, .y = hit->y);
        window->pending = 1;
        break;
    case CLI_HIT_PREVIEW: {
        // a link pressed: a file of the tree opened, or a place of the preview already scrolled to, the view it led
        // taken anew
        CliPreviewOpen open;
        const unsigned int target = EMBED_CALL(gui_preview.press, GuiPreviewCfg, .frame = &window->frame, .x = hit->x,
                                               .y = hit->y, .open_out = &open);
        if (target == CLI_PREVIEW_OPEN_FILE)
        {
            const CliPosition place = {open.line, 0ULL};
            EMBED_CALL(window_file_open, GuiWindowCtx, .window = window, .name = open.path,
                       .place = open.placed ? &place : NULL);
        }
        else if (target == CLI_PREVIEW_OPEN_PLACE)
        {
            EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0);
        }
        window->pending = 1;
        break;
    }
    case CLI_HIT_MAP:
    case CLI_HIT_STICKY:
        // a sticky row pressed puts its line at the view's top
        EMBED_CALL(window_view_scroll, GuiWindowCtx, .window = window, .hit = (GuiWindowHit *)hit);
        break;
    case CLI_HIT_EDITOR:
    case CLI_HIT_GUTTER: {
        EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
        // a press on a row's change mark shows its change
        if ((hit->hit == CLI_HIT_GUTTER) &&
            EMBED_CALL(window_change_mark_pressed, GuiWindowCtx, .window_read = window, .hit = (GuiWindowHit *)hit))
        {
            window->pending = 1;
            break;
        }
        const CliViewInput pointed = {.type = CLI_VIEW_POINTER,
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
    // the wheel over the side, or over the foot panel while a footer tool's pane stands in it, scrolls the rows, which
    // is the window's own; over the completion's list its rows; over the editor or the map, the text
    const GuiPaneTable *const foot = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window);
    if ((foot != NULL) &&
        EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                   .rect = EMBED_CALL(gui_pane.region, GuiPaneCfg, .table = foot, .frame = &window->frame),
                   .x = args->input->x, .y = args->input->y))
    {
        const int moved = EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = foot,
                                     .pane = EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window),
                                     .frame = &window->frame, .rows = args->input->rows);
        window->pending = moved || window->pending;
        return;
    }
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = args->input->x,
                   .y = args->input->y))
    {
        const GuiPaneTable *const table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window);
        const int moved = (table != NULL) && EMBED_CALL(gui_pane.wheel, GuiPaneCfg, .table = table,
                                                        .pane = EMBED_CALL(window_pane, GuiWindowCtx, .window = window),
                                                        .frame = &window->frame, .rows = args->input->rows);
        window->pending = moved || window->pending;
        return;
    }
    // Over the tabs row, the row moves along its strip, GUI_TABS_STEP pixels a row of a notch. A turn toward the
    // reader and the sideways wheel's turn to the right move it to the right
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.tabs_row, .x = args->input->x,
                   .y = args->input->y))
    {
        const long long step = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_STEP,
                                          .scale = window->frame.scale);
        const long long units = (long long)args->input->wheel_down + (long long)args->input->wheel_across;
        EMBED_CALL(window_tabs_slide, GuiWindowCtx, .window = window,
                   .pixels = (long long)window->frame.tabs_left +
                             ((units * step * (long long)GUI_SCROLL_NOTCH_ROWS) / (long long)GUI_WHEEL_UNITS));
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
    // over the preview, its items, the view beside them led along and taken anew
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.preview, .x = args->input->x,
                   .y = args->input->y))
    {
        if (EMBED_CALL(gui_preview.wheel, GuiPreviewCfg, .frame = &window->frame, .rows = args->input->rows))
        {
            EMBED_CALL(window_view_take, GuiWindowCtx, .window = window, .edited = 0);
        }
        return;
    }
    // over the view that stands over the editor, its rows
    if (EMBED_CALL(gui_work.wheel, GuiWorkCfg, .work = &s_work, .frame = &window->frame, .glyphs = &window->glyphs,
                   .x = args->input->x, .y = args->input->y, .rows = args->input->rows))
    {
        window->pending = 1;
        return;
    }
    const char *const key = EMBED_CALL(window_shown_key, GuiWindowCtx, .at = 0u);
    if ((key != NULL) && (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.editor,
                                     .x = args->input->x, .y = args->input->y) ||
                          EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.map,
                                     .x = args->input->x, .y = args->input->y)))
    {
        // a change shown within its settle time stands against the wheel, the text staying under it
        if (!EMBED_CALL(cli_hunks.settled, CliHunksCfg, .at = 0u))
        {
            return;
        }
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
        // The wheel's turn, a part of a notch kept, moves where the scroll goes by GUI_SCROLL_NOTCH_ROWS lines a notch.
        // With Shift held, or on the sideways wheel, it moves across
        int line = 0;
        float cell = 0.0f;
        EMBED_CALL(window_scroll_units, GuiWindowCtx, .window_read = window, .line_out = &line, .cell_out = &cell);
        const double notch = (double)GUI_SCROLL_NOTCH_ROWS * (double)line / (double)GUI_WHEEL_UNITS;
        const int sideways = (args->input->mods & GUI_MOD_SHIFT) != 0u;
        const double down = sideways ? 0.0 : (double)args->input->wheel_down * notch;
        const double across =
            ((double)args->input->wheel_across + (sideways ? (double)args->input->wheel_down : 0.0)) * notch;
        EMBED_CALL(window_scroll_to, GuiWindowCtx, .window = window, .on = 1, .down = window->scroll_down_to + down,
                   .across = window->scroll_across_to + across);
    }
}

// a press of the middle button: on an editor's tab it closes the tab, and on a terminal's tab it ends the tab's shell
// and closes the tab, each as its × does; in the editor's text or gutter it starts the autoscroll; elsewhere it does
// nothing
EMBED_INLINE void window_middle_press(const GuiWindowCtx *args)
{
    GuiWindowHit pressed = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = args->window, .x = args->x, .y = args->y, .hit = &pressed);
    if ((pressed.hit == CLI_HIT_EDITOR) || (pressed.hit == CLI_HIT_GUTTER))
    {
        args->window->autoscroll = 1;
        args->window->autoscroll_y = args->y;
        args->window->autoscroll_pointer_y = args->y;
        args->window->autoscroll_stepped_ms = cli_clock_milliseconds();
    }
    else if ((pressed.hit == CLI_HIT_TAB) || (pressed.hit == CLI_HIT_TAB_CLOSE))
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

// The file as a grid of lines by cells, and the editor's view a window on it. Along one of its scrollbars, down or
// `on` across: the grid's whole, the part the view shows and the first line or cell column in view. The grid's width
// is the widest line the view has shown, and no less than the cells in view after the first
EMBED_INLINE void window_editor_bar_numbers(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    const CliViewFrame *const frame = window->editor.frame;
    *args->total_out = 0ULL;
    *args->shown_out = 0ULL;
    *args->first_out = 0ULL;
    if (frame == NULL)
    {
        return;
    }
    if (args->on)
    {
        const unsigned long long reach = frame->left + window->columns;
        *args->total_out = (frame->widest > reach) ? frame->widest : reach;
        *args->shown_out = window->columns;
        *args->first_out = frame->left;
        return;
    }
    *args->total_out = frame->lines;
    *args->shown_out = window->rows;
    *args->first_out = frame->top;
}

// the editor's scrollbar down `on` 0 or across `on` 1 laid out where the frame puts it: the one across runs under
// the text alone, from the gutter's right edge
EMBED_INLINE void window_editor_bar_layout(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    if (!args->on)
    {
        // The controls' box comes down over the editor's top right corner. The bar down the editor's side starts under
        // the box where the box stands over it, so the box is never under the bar's thumb
        GuiRect down = window->frame.editor_bar;
        const GuiRect box = window->frame.controls_pane;
        const int box_foot = box.y + box.height;
        if ((box.width > 0) && (box.height > 0) && (box.x < down.x + down.width) && (box.x + box.width > down.x) &&
            (box_foot > down.y) && (box_foot < down.y + down.height))
        {
            down.height -= box_foot - down.y;
            down.y = box_foot;
        }
        EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = &window->editor_down, .rect = down, .across = 0,
                   .scale = window->frame.scale);
        return;
    }
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = window->frame.scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &window->glyphs, .size = size);
    const int gutter = EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                  .editor_read = &window->editor, .cell = cell, .scale = window->frame.scale);
    const GuiRect bar = window->frame.editor_bar_across;
    const int from = (gutter < bar.width) ? gutter : bar.width;
    EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = &window->editor_across,
               .rect = (GuiRect){bar.x + from, bar.y, bar.width - from, bar.height}, .across = 1,
               .scale = window->frame.scale);
}

// The view's first line `row` put down, or `on` across its first cell column. A thumb's drag, `edited` 1, puts the view
// there at the next frame. An arrow and the track ease the scroll there
EMBED_INLINE void window_editor_bar_send(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    int line = 0;
    float cell = 0.0f;
    EMBED_CALL(window_scroll_units, GuiWindowCtx, .window_read = window, .line_out = &line, .cell_out = &cell);
    if (args->on)
    {
        // across the scroll counts from the grid's first cell column, so the column is where it goes
        EMBED_CALL(window_scroll_to, GuiWindowCtx, .window = window, .on = !args->edited,
                   .down = window->scroll_down_to, .across = (double)args->row * (double)cell);
        return;
    }
    if (args->edited)
    {
        // a thumb's drag puts the view at its line at the next frame
        window->scroll_drag_top = args->row;
        EMBED_CALL(window_scroll_to, GuiWindowCtx, .window = window, .on = 1, .down = window->scroll_down_to,
                   .across = window->scroll_across_to);
        return;
    }
    // an arrow and the track move where the scroll goes down by the lines between the top and their line
    const CliViewFrame *const frame = window->editor.frame;
    const double from = (frame != NULL) ? (double)frame->top : 0.0;
    EMBED_CALL(window_scroll_to, GuiWindowCtx, .window = window, .on = 1,
               .down = window->scroll_down_to + (((double)args->row - from) * (double)line),
               .across = window->scroll_across_to);
}

// A press at x, y on the editor's scrollbar down, `on` 0, or across, 1. An arrow moves the view one line or one cell,
// the track a view's length, and the thumb begins a drag, held where the press took it. It returns 1 where the press
// stood on the scrollbar
EMBED_INLINE int window_editor_bar_press(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const GuiScrollbar *const bar = args->on ? &window->editor_across : &window->editor_down;
    unsigned long long total = 0ULL;
    unsigned long long shown = 0ULL;
    unsigned long long first = 0ULL;
    EMBED_CALL(window_editor_bar_numbers, GuiWindowCtx, .window_read = window, .on = args->on, .total_out = &total,
               .shown_out = &shown, .first_out = &first);
    const unsigned int hit = EMBED_CALL(gui_scrollbar.hit, GuiScrollbarCfg, .bar_read = bar, .total = total,
                                        .shown = shown, .first = first, .x = args->x, .y = args->y);
    if (hit == GUI_SCROLLBAR_NONE)
    {
        return 0;
    }
    if (hit == GUI_SCROLLBAR_THUMB)
    {
        const GuiRect thumb = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = bar, .total = total,
                                         .shown = shown, .first = first);
        window->editor_bar_dragged = args->on ? 2u : 1u;
        window->editor_bar_grab = args->on ? (args->x - thumb.x) : (args->y - thumb.y);
        window->pending = 1;
        return 1;
    }
    const unsigned long long page = (shown != 0ULL) ? shown : 1ULL;
    unsigned long long moved = first;
    if (hit == GUI_SCROLLBAR_UP)
    {
        moved = (first > 0ULL) ? first - 1ULL : 0ULL;
    }
    else if (hit == GUI_SCROLLBAR_DOWN)
    {
        moved = first + 1ULL;
    }
    else if (hit == GUI_SCROLLBAR_ABOVE)
    {
        moved = (first > page) ? first - page : 0ULL;
    }
    else
    {
        moved = first + page;
    }
    EMBED_CALL(window_editor_bar_send, GuiWindowCtx, .window = window, .on = args->on, .row = moved);
    return 1;
}

// the thumb of the scrollbar a press took dragged to x, y, the view's first line or cell column where it puts it
EMBED_INLINE void window_editor_bar_drag(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const int across = window->editor_bar_dragged == 2u;
    const GuiScrollbar *const bar = across ? &window->editor_across : &window->editor_down;
    unsigned long long total = 0ULL;
    unsigned long long shown = 0ULL;
    unsigned long long first = 0ULL;
    EMBED_CALL(window_editor_bar_numbers, GuiWindowCtx, .window_read = window, .on = across, .total_out = &total,
               .shown_out = &shown, .first_out = &first);
    unsigned long long wanted = first;
    EMBED_CALL(gui_scrollbar.first_at, GuiScrollbarCfg, .bar_read = bar, .total = total, .shown = shown,
               .x = args->x - (across ? window->editor_bar_grab : 0),
               .y = args->y - (across ? 0 : window->editor_bar_grab), .first_out = &wanted);
    if (wanted != first)
    {
        EMBED_CALL(window_editor_bar_send, GuiWindowCtx, .window = window, .on = across, .row = wanted, .edited = 1);
    }
}

// The tabs stand in one strip, and the tabs row is a window on it. Along the row's scrollbar: the strip's width, the
// row's width and the pixels of the strip left of the row
EMBED_INLINE void window_tabs_bar_numbers(const GuiWindowCtx *args)
{
    const GuiWindow *const window = args->window_read;
    const int whole = EMBED_CALL(gui_tabs_glyphs.whole, GuiTabsGlyphsCfg, .glyphs = (GuiGlyphs *)&window->glyphs,
                                 .frame = &window->frame);
    const int width = window->frame.tabs.width;
    // each count is 0 or more, so the change to unsigned keeps its value
    *args->total_out = (whole > 0) ? (unsigned long long)whole : 0ULL;
    *args->shown_out = (width > 0) ? (unsigned long long)width : 0ULL;
    *args->first_out = (window->frame.tabs_left > 0) ? (unsigned long long)window->frame.tabs_left : 0ULL;
}

// The tabs row's scrollbar hangs below the row, over the editor's top, a scrollbar's thickness tall, as the page hangs
// it. The editor does not move for it
EMBED_INLINE void window_tabs_bar_layout(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    const int bar = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SCROLLBAR_WIDTH,
                               .scale = window->frame.scale);
    const GuiRect row = window->frame.tabs_row;
    EMBED_CALL(gui_scrollbar.layout, GuiScrollbarCfg, .bar = &window->tabs_bar,
               .rect = (GuiRect){window->frame.tabs.x, row.y + row.height, window->frame.tabs.width, bar}, .across = 1,
               .scale = window->frame.scale);
}

// the tabs row moved to show the strip from `pixels`, kept between the strip's start and its end less the row's width
EMBED_INLINE void window_tabs_slide(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    unsigned long long whole = 0ULL;
    unsigned long long width = 0ULL;
    unsigned long long left = 0ULL;
    EMBED_CALL(window_tabs_bar_numbers, GuiWindowCtx, .window_read = window, .total_out = &whole, .shown_out = &width,
               .first_out = &left);
    // the strip and the row are pixels on the screen, so each fits in an int
    const long long most = (whole > width) ? (long long)(whole - width) : 0LL;
    const long long wanted = (args->pixels < 0LL) ? 0LL : ((args->pixels > most) ? most : args->pixels);
    if (wanted != (long long)left)
    {
        window->frame.tabs_left = (int)wanted;
        window->pending = 1;
    }
}

// The tabs row kept on the strip. When the tab shown changes, the row moves the least that shows that tab whole, as the
// page scrolls the tab shown into sight
EMBED_INLINE void window_tabs_follow(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    long long left = window->frame.tabs_left;
    const int shown = cli_view_shown();
    if (shown != window->tabs_shown_seen)
    {
        window->tabs_shown_seen = shown;
        if (shown >= 0)
        {
            // the tab shown is a place among the tabs, 0 or more
            const GuiRect tab = EMBED_CALL(gui_tabs_glyphs.tab_rect, GuiTabsGlyphsCfg, .glyphs = &window->glyphs,
                                           .frame = &window->frame, .at = (unsigned int)shown);
            const GuiRect row = window->frame.tabs;
            if (tab.x < row.x)
            {
                left -= row.x - tab.x;
            }
            else if (tab.x + tab.width > row.x + row.width)
            {
                left += (tab.x + tab.width) - (row.x + row.width);
            }
        }
    }
    EMBED_CALL(window_tabs_slide, GuiWindowCtx, .window = window, .pixels = left);
}

// A press at x, y on the tabs row's scrollbar. An arrow moves the row by GUI_TABS_STEP pixels, the track by the row's
// width, and the thumb begins a drag. It returns 1 where the press stood on the scrollbar
EMBED_INLINE int window_tabs_bar_press(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    unsigned long long whole = 0ULL;
    unsigned long long width = 0ULL;
    unsigned long long left = 0ULL;
    EMBED_CALL(window_tabs_bar_numbers, GuiWindowCtx, .window_read = window, .total_out = &whole, .shown_out = &width,
               .first_out = &left);
    if (window->frame.tabs_share < GUI_FIXED_ONE)
    {
        return 0;
    }
    const unsigned int hit = EMBED_CALL(gui_scrollbar.hit, GuiScrollbarCfg, .bar_read = &window->tabs_bar,
                                        .total = whole, .shown = width, .first = left, .x = args->x, .y = args->y);
    if (hit == GUI_SCROLLBAR_NONE)
    {
        return 0;
    }
    if (hit == GUI_SCROLLBAR_THUMB)
    {
        const GuiRect thumb = EMBED_CALL(gui_scrollbar.thumb, GuiScrollbarCfg, .bar_read = &window->tabs_bar,
                                         .total = whole, .shown = width, .first = left);
        window->editor_bar_dragged = 3u;
        window->editor_bar_grab = args->x - thumb.x;
        window->pending = 1;
        return 1;
    }
    const long long step =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_TABS_STEP, .scale = window->frame.scale);
    // the row's width is pixels on the screen, so it fits in a long long
    const long long page = (long long)width;
    const long long from = (long long)left;
    const long long moved = (hit == GUI_SCROLLBAR_UP)      ? from - step
                            : (hit == GUI_SCROLLBAR_DOWN)  ? from + step
                            : (hit == GUI_SCROLLBAR_ABOVE) ? from - page
                                                           : from + page;
    EMBED_CALL(window_tabs_slide, GuiWindowCtx, .window = window, .pixels = moved);
    return 1;
}

// the tabs row's thumb dragged to x, the row moved to show the strip where the thumb puts it
EMBED_INLINE void window_tabs_bar_drag(const GuiWindowCtx *args)
{
    GuiWindow *const window = args->window;
    unsigned long long whole = 0ULL;
    unsigned long long width = 0ULL;
    unsigned long long left = 0ULL;
    EMBED_CALL(window_tabs_bar_numbers, GuiWindowCtx, .window_read = window, .total_out = &whole, .shown_out = &width,
               .first_out = &left);
    unsigned long long wanted = left;
    EMBED_CALL(gui_scrollbar.first_at, GuiScrollbarCfg, .bar_read = &window->tabs_bar, .total = whole, .shown = width,
               .x = args->x - window->editor_bar_grab, .y = args->y, .first_out = &wanted);
    // the strip is pixels on the screen, so the place fits in a long long
    EMBED_CALL(window_tabs_slide, GuiWindowCtx, .window = window, .pixels = (long long)wanted);
}

static void window_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    // the press after the one that started the autoscroll ends it, whichever button, and does nothing else
    if (window->autoscroll)
    {
        window->autoscroll = 0;
        return;
    }
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
    // A press in the top band while the claim keeps the bar away shows the bar at once, with no rest and no slide. The
    // press then goes to the bar's title under it, and not to the editor under the band. The bar shown stands lower
    // than the band, so the press is read at the middle of the titles' row
    GuiInput aimed;
    if (window->claim.autohidden && (window->claim.displayed != GUI_AUTOHIDE_TOP) &&
        (EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = window->frame.width,
                    .height = window->frame.height, .scale = window->frame.scale,
                    .controls_width = window->frame.controls.width, .x = input->x, .y = input->y) == GUI_AUTOHIDE_TOP))
    {
        EMBED_CALL(gui_autohide_pointer.show_now, GuiAutohidePointerCfg, .claim = &window->claim, .x = input->x,
                   .y = input->y, .zone = GUI_AUTOHIDE_TOP);
        window->autohide_moving = 1;
        EMBED_CALL(window_autohide_layout, GuiWindowCtx, .window = window);
        if (window->menubar.titles > 0u)
        {
            const GuiRect row = window->menubar.title[0].rect;
            aimed = *input;
            aimed.y = (input->y < row.y) ? (row.y + (row.height / 2)) : input->y;
            input = &aimed;
        }
    }
    const unsigned int hit = EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input);
    window->pressed =
        ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE)) ? hit : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
    // A press on the tabs row's scrollbar moves the row over the strip of tabs. A press on either of the editor's
    // scrollbars moves the view over the file's grid. Neither press goes anywhere else
    if (EMBED_CALL(window_tabs_bar_press, GuiWindowCtx, .window = window, .x = input->x, .y = input->y))
    {
        return;
    }
    if (!EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &s_work) &&
        (EMBED_CALL(window_editor_bar_press, GuiWindowCtx, .window = window, .on = 0, .x = input->x, .y = input->y) ||
         EMBED_CALL(window_editor_bar_press, GuiWindowCtx, .window = window, .on = 1, .x = input->x, .y = input->y)))
    {
        return;
    }
    GuiWindowHit pressed = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = input->x, .y = input->y, .hit = &pressed);
    // a press anywhere takes the keys from the shown pane's field; the field's own action takes them back
    EMBED_CALL(window_pane, GuiWindowCtx, .window = window)->keys = 0;
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
    // the autoscroll reads the pointer's y at each move, and the next frame scrolls by its distance
    if (window->autoscroll)
    {
        window->autoscroll_pointer_y = args->input->y;
        return;
    }
    if (window->editor_bar_dragged == 3u)
    {
        EMBED_CALL(window_tabs_bar_drag, GuiWindowCtx, .window = window, .x = args->input->x, .y = args->input->y);
        return;
    }
    if (window->editor_bar_dragged != 0u)
    {
        EMBED_CALL(window_editor_bar_drag, GuiWindowCtx, .window = window, .x = args->input->x, .y = args->input->y);
        return;
    }
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
        EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &window->claim, .x = args->input->x,
                   .y = args->input->y,
                   .zone =
                       EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = frame->width,
                                  .height = frame->height, .scale = frame->scale,
                                  .controls_width = frame->controls.width, .x = args->input->x, .y = args->input->y),
                   .over_parts = EMBED_CALL(window_over_parts, GuiWindowCtx, .window_read = window, .x = args->input->x,
                                            .y = args->input->y));
        if (EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &window->claim, .time_ms = now_ms))
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
    const GuiPaneTable *const table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window);
    const int pane_moved =
        (table != NULL) &&
        EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = table,
                   .pane = EMBED_CALL(window_pane, GuiWindowCtx, .window = window), .frame = &window->frame,
                   .glyphs = &window->glyphs, .x = args->input->x, .y = args->input->y);
    const GuiPaneTable *const foot = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window);
    const int foot_moved =
        (foot != NULL) &&
        EMBED_CALL(gui_pane.move, GuiPaneCfg, .table = foot,
                   .pane = EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window), .frame = &window->frame,
                   .glyphs = &window->glyphs, .x = args->input->x, .y = args->input->y);
    window->pending = pane_moved || foot_moved || window->pending;
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
        const CliViewInput pointed = {.type = CLI_VIEW_POINTER,
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
    EMBED_CALL(cli_autohide_still.leave, GuiAutohideStillCfg, .claim = &window->claim);
    EMBED_CALL(gui_sides.leave, GuiSidesCfg, .sides = &window->sides, .time_ms = cli_clock_milliseconds());
}

static void window_release(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    if (window->editor_bar_dragged != 0u)
    {
        window->editor_bar_dragged = 0u;
        window->pending = 1;
    }
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
        window->minimap.dragging = 0;
        window->pending = 1;
    }
    if (window->dragging)
    {
        window->dragging = 0;
        GuiWindowHit released = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
        EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = args->input->x, .y = args->input->y,
                   .hit = &released);
        const CliViewInput pointed = {.type = CLI_VIEW_POINTER,
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

// the focus come to the main window or to Preferences' window: the signal moves to the edge of the one that holds it
static void window_focus(const GuiInputHandlerCfg *args)
{
    args->window->preferences.focused = 0;
    args->window->pending = 1;
    args->window->preferences_pending = 1;
}

static void window_preferences_focus(const GuiInputHandlerCfg *args)
{
    args->window->preferences.focused = 1;
    args->window->pending = 1;
    args->window->preferences_pending = 1;
}

// the menu of the tab at `at` built as the menus' own rows, as tabs.js gives it: Close, Close others, which one tab
// alone cannot take, Close All, a line and Copy path: 1, or 0 where no view stands at `at`
EMBED_INLINE int window_tab_menu(const GuiWindowCtx *args)
{
    const char *const key = cli_view_key(args->at);
    if (key == NULL)
    {
        return 0;
    }
    // the rows run with the view's key, which stands while the view is open
    cli_menus_own_begin();
    cli_menus_own_row(&(CliItem){"Close", "tab-close", "", key, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Close others", "tab-close-others", "", key, (cli_view_count() < 2u) ? "never" : "",
                                 "", 0, 0, 0, 0, 0u});
    (void)cli_menus_own_named("close-all", "");
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy path", "file-copy-path", "", key, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

// the editor's menu built as the menus' own rows from the catalog's items, as the page's editor menu lists them: Go to
// Definition, Find Usages, Rename Symbol and Quick Fix, a line, Cut, Copy and Paste, a line, Select All, Find, Replace
// and Go to Line, a line and Save: 1, or 0 where no view is shown
EMBED_INLINE int window_editor_menu(const GuiWindowCtx *args)
{
    static const char *const s_items[] = {"definition", "usages",  "rename", "quick-fix", "-",
                                          "cut",        "copy",    "paste",  "-",         "select-all",
                                          "find",       "replace", "line",   "-",         "save"};
    if (args->window_read->editor.frame == NULL)
    {
        return 0;
    }
    cli_menus_own_begin();
    for (unsigned int at = 0u; at < sizeof(s_items) / sizeof(s_items[0]); at += 1u)
    {
        if (s_items[at][0] == '-')
        {
            cli_menus_own_line();
        }
        else
        {
            (void)cli_menus_own_named(s_items[at], "");
        }
    }
    return 1;
}

// the system's gesture for a menu: the menu of what stands at the pointer asked for there, none on a menu open, the
// menus open closed first: the terminal's panel's for every place on it, a tab's, the editor's, and the shown pane's
// table's for a row of it or for the pane off its rows
static void window_menu(const GuiInputHandlerCfg *args)
{
    GuiWindow *const window = args->window;
    const int x = args->input->x;
    const int y = args->input->y;
    GuiWindowHit asked = {CLI_HIT_NONE, 0u, 0u, 0ULL, 0ULL};
    EMBED_CALL(window_hit_read, GuiWindowCtx, .window_read = window, .x = x, .y = y, .hit = &asked);
    if (asked.hit == CLI_HIT_MENU_ROW)
    {
        return;
    }
    EMBED_CALL(window_menus_close, GuiWindowCtx, .window = window);
    int built = 0;
    if ((asked.hit >= CLI_HIT_TERMINAL) && (asked.hit <= CLI_HIT_TERMINAL_PANEL))
    {
        cli_menus_own_open(cli_terminal_tabs_menu(), CLI_TERMINAL_MENU_ROWS, CLI_MENU_PLACE_POINT, x, y);
        EMBED_CALL(window_menus_laid, GuiWindowCtx, .window = window);
        return;
    }
    if ((asked.hit == CLI_HIT_TAB) || (asked.hit == CLI_HIT_TAB_CLOSE))
    {
        built = EMBED_CALL(window_tab_menu, GuiWindowCtx, .at = asked.index);
    }
    else if ((asked.hit == CLI_HIT_EDITOR) || (asked.hit == CLI_HIT_GUTTER))
    {
        built = EMBED_CALL(window_editor_menu, GuiWindowCtx, .window_read = window);
    }
    else if (asked.hit == CLI_HIT_WORK)
    {
        built = EMBED_CALL(gui_work.menu, GuiWorkCfg, .work_read = &s_work, .frame = &window->frame,
                           .glyphs = &window->glyphs, .index = asked.index, .x = x, .y = y);
    }
    else if ((asked.hit == CLI_HIT_PANE_ROW) || (asked.hit == CLI_HIT_PANE_ACTION) ||
             ((asked.hit == CLI_HIT_NONE) &&
              EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = x, .y = y)))
    {
        built = EMBED_CALL(
            gui_pane.menu, GuiPaneCfg, .table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window),
            .row = (asked.hit == CLI_HIT_PANE_ROW) ? asked.index : GUI_PANE_OFF_ROWS, .root = cli_command_line_root());
    }
    else if ((asked.hit == CLI_HIT_FOOT_ROW) || (asked.hit == CLI_HIT_FOOT_ACTION) ||
             ((asked.hit == CLI_HIT_NONE) &&
              (EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window) != NULL) &&
              EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.terminal, .x = x, .y = y)))
    {
        built = EMBED_CALL(
            gui_pane.menu, GuiPaneCfg, .table = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window),
            .row = (asked.hit == CLI_HIT_FOOT_ROW) ? asked.index : GUI_PANE_OFF_ROWS, .root = cli_command_line_root());
    }
    if (built)
    {
        unsigned int count = 0u;
        const CliItem *const rows = cli_menus_own_rows(&count);
        cli_menus_own_open(rows, count, CLI_MENU_PLACE_POINT, x, y);
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
    // the view over the editor takes the text while it stands there
    if (EMBED_CALL(gui_work.text, GuiWorkCfg, .work = &s_work, .frame = &window->frame, .glyphs = &window->glyphs,
                   .text = args->input->text, .length = args->input->size))
    {
        window->pending = 1;
        return;
    }
    // the shown pane's field, the side's or the foot's, takes the text while it has the keys
    if (EMBED_CALL(gui_pane.text, GuiPaneCfg,
                   .table = EMBED_CALL(window_pane_table, GuiWindowCtx, .window_read = window),
                   .pane = EMBED_CALL(window_pane, GuiWindowCtx, .window = window), .text = args->input->text,
                   .length = args->input->size, .root = cli_command_line_root()) ||
        EMBED_CALL(gui_pane.text, GuiPaneCfg,
                   .table = EMBED_CALL(window_foot_table, GuiWindowCtx, .window_read = window),
                   .pane = EMBED_CALL(window_foot_pane, GuiWindowCtx, .window = window), .text = args->input->text,
                   .length = args->input->size, .root = cli_command_line_root()))
    {
        window->pending = 1;
        return;
    }
    const CliViewInput typed = {.type = CLI_VIEW_TEXT, .text = args->input->text, .length = args->input->size};
    EMBED_CALL(window_editing, GuiWindowCtx, .window = window, .on = 1);
    cli_servers_panels_text(args->input->text, args->input->size);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .view_input = &typed, .edited = 1);
    EMBED_CALL(window_panels_take, GuiWindowCtx, .window = window);
}

// what answers each table of input, in the main window and in Preferences' window
static const GuiInputHandlers s_window_handlers = {{[GUI_INPUT_MOVED] = window_move,
                                                    [GUI_INPUT_DOWN] = window_press,
                                                    [GUI_INPUT_UP] = window_release,
                                                    [GUI_INPUT_LEFT] = window_leave,
                                                    [GUI_INPUT_WHEEL] = window_wheel,
                                                    [GUI_INPUT_KEY] = window_key,
                                                    [GUI_INPUT_TEXT] = window_text,
                                                    [GUI_INPUT_MENU] = window_menu,
                                                    [GUI_INPUT_BLUR] = window_blur,
                                                    [GUI_INPUT_FOCUS] = window_focus}};
static const GuiInputHandlers s_preferences_handlers = {{[GUI_INPUT_MOVED] = window_preferences_move,
                                                         [GUI_INPUT_DOWN] = window_preferences_press,
                                                         [GUI_INPUT_UP] = window_preferences_release,
                                                         [GUI_INPUT_LEFT] = window_preferences_leave,
                                                         [GUI_INPUT_WHEEL] = window_preferences_wheel,
                                                         [GUI_INPUT_KEY] = window_preferences_key,
                                                         [GUI_INPUT_TEXT] = window_preferences_text,
                                                         [GUI_INPUT_FOCUS] = window_preferences_focus}};

void gui_window_input(const GuiWindowInputTypeCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &s_window_handlers, .window = window,
               .input = input);
}

void gui_window_preferences_input(const GuiWindowInputTypeCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    EMBED_CALL(gui_input_event.handle, GuiInputEventCfg, .handlers = &s_preferences_handlers, .window = window,
               .input = input);
}
