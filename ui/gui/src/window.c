// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the window apart from the system. A file opens through branch_open, a text read whole and a file too large
// to read whole alike, and is shown through the CLI's editor view, which answers each input with the frame drawn; the
// walk's colors come as branch-colors events. The minimap's lines are asked of editor_view_map for the span the map
// stands for, after a frame moves the span or a key, text or the walk's colors make the lines stored stale, one ask in
// flight at a time. What the tree saves, its tabs and the one shown and the explorer's group, is read from orior's
// folder as the root comes, under the keys the page saves them by, and written back as they change
#include "window.h"
#include "../../cli/src/command_line/callback_cfg.h"

#include "../../cli/src/editor/view.h"
#include "../../cli/src/text/text.h"
#include "answer.h"
#include "parts/chrome.h"
#include "system.h"

#include <math.h>

// the head of the key a file opened is stored under; the keys orior's folder saves a tree's session and the explorer's
// group under, the tree's root after the session's; and the bytes the session is written in
#define GUI_OPEN_KEY_HEAD "window:"
#define GUI_SAVED_SESSION "orior.session."
#define GUI_SAVED_GROUP "orior.panes.group"
#define GUI_SAVED_MORE 256ull

int gui_window_begin(const GuiWindowWokenCfg *args)
{
    GuiLinkWoken woken = args->woken;
    void *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    memor.set(&(MemoriaCfg){.dst = args->window, .val = 0, .bytes = sizeof(*args->window)});
    EMBED_CALL(cli_palette.own_init, GuiPaletteCfg, .own = &args->window->palette_own, .scheme = GUI_SCHEME_DARK);
    args->window->palette = &args->window->palette_own.colors;
    EMBED_CALL(gui_preferences_size.init, GuiPreferencesSizeCfg, .preferences = &args->window->preferences);
    args->window->preference_values.zoom_step = GUI_PREFERENCE_ZOOM_WHOLE;
    args->window->preference_values.bar_percent = GUI_WINDOW_BAR_OPACITY;
    args->window->preference_values.editor_percent = GUI_WINDOW_EDITOR_OPACITY;
    args->window->raster = &args->window->framebuffer.raster[0];
    args->window->active = -1;
    args->window->last_active = -1;
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &args->window->editor);
    EMBED_CALL(gui_menubar_glyphs.init, GuiMenubarGlyphsCfg, .menubar = &args->window->menubar);
    EMBED_CALL(gui_strip_name.init, GuiStripNameCfg, .strip = &args->window->strip);
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &args->window->icons);
    if (!EMBED_CALL(cli_explorer.init, GuiExplorerCfg, .explorer = &args->window->explorer))
    {
        mmgr_line_put(err_msg, "the explorer's cells could not be taken");
        return 0;
    }
    if (!EMBED_CALL(gui_minimap_json.init, GuiMinimapJsonCfg, .map = &args->window->minimap))
    {
        EMBED_CALL(cli_explorer.free, GuiExplorerCfg, .explorer = &args->window->explorer);
        mmgr_line_put(err_msg, "the minimap's cells could not be taken");
        return 0;
    }
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &args->window->claim);
    EMBED_CALL(cli_wordmark_still.init, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    EMBED_CALL(cli_busy_still.init, GuiBusyStillCfg, .busy = &args->window->busy);
    EMBED_CALL(gui_hover_state.init, GuiHoverStateCfg, .hover = &args->window->hover);
    EMBED_CALL(gui_signature_json.init, GuiSignatureJsonCfg, .signature = &args->window->signature);
    if (!EMBED_CALL(gui_suggest_rows.init, GuiSuggestRowsCfg, .suggest = &args->window->suggest))
    {
        EMBED_CALL(cli_explorer.free, GuiExplorerCfg, .explorer = &args->window->explorer);
        EMBED_CALL(gui_minimap_json.free, GuiMinimapJsonCfg, .map = &args->window->minimap);
        mmgr_line_put(err_msg, "the completion's cells could not be taken");
        return 0;
    }
    if (!EMBED_CALL(cli_problems.init, GuiProblemsCfg, .problems_out = &args->window->problems))
    {
        EMBED_CALL(gui_suggest_rows.free, GuiSuggestRowsCfg, .suggest = &args->window->suggest);
        EMBED_CALL(cli_explorer.free, GuiExplorerCfg, .explorer = &args->window->explorer);
        EMBED_CALL(gui_minimap_json.free, GuiMinimapJsonCfg, .map = &args->window->minimap);
        mmgr_line_put(err_msg, "the problems' cells could not be taken");
        return 0;
    }
    if (!EMBED_CALL(gui_ground.init, GuiGroundCfg, .ground = &args->window->ground))
    {
        EMBED_CALL(cli_problems.free, GuiProblemsCfg, .problems_out = &args->window->problems);
        EMBED_CALL(gui_suggest_rows.free, GuiSuggestRowsCfg, .suggest = &args->window->suggest);
        EMBED_CALL(cli_explorer.free, GuiExplorerCfg, .explorer = &args->window->explorer);
        EMBED_CALL(gui_minimap_json.free, GuiMinimapJsonCfg, .map = &args->window->minimap);
        mmgr_line_put(err_msg, "the lattice has no slot left for the ground");
        return 0;
    }
    if (!EMBED_CALL(cli_glyphs.init, GuiGlyphsCfg, .glyphs = &args->window->glyphs, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_link.begin, GuiLinkCfg, .link = &args->window->link, .woken = woken, .context = context))
    {
        EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &args->window->glyphs);
        mmgr_line_put(err_msg, "the link to orior_cli could not begin");
        return 0;
    }
    args->window->pressed = GUI_HIT_CLIENT;
    return 1;
}

typedef struct
{
    GuiWindow *window;
    int ended;
    const GuiWindow *window_read;
    const char *name;
    int own;
    const VerbaLinea *request;
    unsigned int kind;
    const char *more;
    VerbaLinea *args_out;
    unsigned long long row;
    unsigned long long column;
    int server;
    unsigned int at;
    unsigned long long line;
    unsigned long long col;
    const char *path;
    unsigned long long ticket;
    const CliJson *json;
    unsigned int root;
    char byte;
    int on;
    int x;
    int y;
    const char *dir;
    const char *key;
    unsigned long long length;
    unsigned int names;
    unsigned long long through;
    unsigned int from;
    unsigned int to;
    GuiRect caret;
    unsigned long long one;
    unsigned long long other;
    const char *text;
    unsigned long long size;
    const char *command;
    const char *const *names_letters;
    unsigned int count;
    const char *value;
    unsigned int percent;
    const GuiPreferenceChange *change;
    unsigned int *percent_pointer;
    const char *root_text;
    int is_object;
    const char *named;
    long long delta;
    unsigned long long top;
    unsigned int pointer;
    unsigned int clicks;
    int shift;
} GuiWindowCtx;
static void window_session_write(const GuiWindowCtx *args);

void gui_window_end(const GuiWindowCfg *args)
{
    EMBED_CALL(cli_link.end, GuiLinkCfg, .link = &args->window->link);
    EMBED_CALL(window_session_write, GuiWindowCtx, .window = args->window, .ended = 1);
    for (unsigned int at = 0u; at < args->window->tabs; at += 1u)
    {
        g_cli_cells->persistent_buf_release(args->window->tab[at].names, &g_gui_cells);
        args->window->tab[at].names = NULL;
    }
    EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &args->window->editor);
    if (args->window->cataloged)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->window->catalog);
        args->window->cataloged = 0;
    }
    EMBED_CALL(cli_glyphs.free, GuiGlyphsCfg, .glyphs = &args->window->glyphs);
    EMBED_CALL(cli_icons.free, GuiIconsCfg, .icons = &args->window->icons);
    EMBED_CALL(cli_wordmark_still.free, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    EMBED_CALL(cli_busy_still.free, GuiBusyStillCfg, .busy = &args->window->busy);
    EMBED_CALL(gui_chrome_raster.controls_shadow_free, GuiChromeRasterCfg, .shadow = &args->window->controls_shadow);
    EMBED_CALL(cli_explorer.free, GuiExplorerCfg, .explorer = &args->window->explorer);
    EMBED_CALL(gui_minimap_json.free, GuiMinimapJsonCfg, .map = &args->window->minimap);
    EMBED_CALL(gui_hover_state.free, GuiHoverStateCfg, .hover = &args->window->hover);
    EMBED_CALL(gui_suggest_rows.free, GuiSuggestRowsCfg, .suggest = &args->window->suggest);
    EMBED_CALL(gui_signature_json.free, GuiSignatureJsonCfg, .signature = &args->window->signature);
    EMBED_CALL(cli_problems.free, GuiProblemsCfg, .problems_out = &args->window->problems);
    EMBED_CALL(gui_ground.free, GuiGroundCfg, .ground = &args->window->ground);
    EMBED_CALL(gui_stages_claim.free, GuiStagesClaimCfg, .stages = &args->window->stages);
    args->window->stage = NULL;
    EMBED_CALL(gui_base_width.free, GuiBaseWidthCfg, .base = &args->window->base);
    EMBED_CALL(gui_framebuffer_most_width.free, GuiFramebufferMostWidthCfg, .framebuffer = &args->window->framebuffer);
}

// whether the strip's tool shown is named `name`
EMBED_INLINE int window_tool_by_name(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return (args->window_read->strip.shown >= 0) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = (unsigned int)args->window_read->strip.shown)->name,
               .other = args->name,
               .cap = 32ull});
}

// the rows the editor shows, and the columns
EMBED_INLINE unsigned int window_rows(const GuiWindowCtx *args)
{
    const int line_tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE,
                                     .scale = args->window_read->frame.scale);
    const int rows = (line_tall > 0) ? args->window_read->frame.editor.height / line_tall : 0;
    // rows are counted where there is room for them
    return (rows > 0) ? (unsigned int)rows : 0u;
}

EMBED_INLINE unsigned int window_columns(const GuiWindowCtx *args)
{
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = args->window->frame.scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs, .size = size);
    const int gutter =
        EMBED_CALL(gui_editor_place_cell.gutter, GuiEditorPlaceCellCfg, .editor_read = &args->window->editor,
                   .cell = cell, .scale = args->window->frame.scale);
    const float wide = (float)(args->window->frame.editor.width - gutter);
    // columns are counted where there is room for them
    return ((cell > 0.0f) && (wide > 0.0f)) ? (unsigned int)(wide / cell) : 0u;
}

// the lines the editor shows, with the fraction of the last
EMBED_INLINE float window_on_screen(const GuiWindowCtx *args)
{
    const int line_tall = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE,
                                     .scale = args->window_read->frame.scale);
    return (line_tall > 0) ? (float)args->window_read->frame.editor.height / (float)line_tall : 0.0f;
}

// a call of the window's own or the tree's made with `args`: its ticket
EMBED_INLINE unsigned long long window_call(const GuiWindowCtx *args)
{
    int own = args->own;
    const char *name = args->name;
    const VerbaLinea *arguments = args->request;
    const unsigned long long ticket =
        EMBED_CALL(gui_link_own.call, GuiLinkOwnCfg, .link = &args->window->link, .own = own, .name = name,
                   .request = arguments->out, .size = mmgr_line_ok(arguments) ? arguments->at : 0ull);
    if (ticket != 0ull)
    {
        EMBED_CALL(gui_busy_ticket.request, GuiBusyTicketCfg, .busy = &args->window->busy, .ticket = ticket);
    }
    return ticket;
}

_Static_assert(GUI_BUSY_ASKS > ((2u * GUI_LINK_PENDING) + GUI_LINK_WORKERS + GUI_LINK_DONE),
               "the circle keeps every ask the link can stand unanswered");

// an input of the view's sent, `{"key", "kind", ...}` with `more` after the kind, its own members written already
EMBED_INLINE void window_view_input(const GuiWindowCtx *args)
{
    if (!args->window->viewed)
    {
        return;
    }
    char text[CLI_PATH_MAX_LEN + 512u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
               .text = args->window->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->window->key, .cap = sizeof(args->window->key)}));
    mmgr_line_put(&arguments, ",\"kind\":");
    mmgr_line_decimal(&arguments, args->kind, 1u);
    mmgr_line_put(&arguments, args->more);
    mmgr_line_ch(&arguments, '}');
    (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "editor_view_input",
                     .request = &arguments);
}

// the panels over the editor hidden: the hover, the completion and the signature, and no completion wanted
EMBED_INLINE void window_panels_hide(const GuiWindowCtx *args)
{
    const int owed = args->window->hover.shown || args->window->suggest.open || args->window->signature.shown;
    EMBED_CALL(gui_hover_state.hide, GuiHoverStateCfg, .hover = &args->window->hover);
    EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &args->window->suggest);
    EMBED_CALL(gui_signature_json.hide, GuiSignatureJsonCfg, .signature = &args->window->signature);
    args->window->complete_wanted = 0;
    args->window->complete_forced = 0;
    args->window->owed = args->window->owed || owed;
}

// the shown view's key written as the one argument of a call, `{"key": ...` with `more` after it
EMBED_INLINE void window_key_args(const GuiWindowCtx *args)
{
    mmgr_line_put(args->args_out, "{\"key\":");
    EMBED_CALL(
        cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->args_out,
        .text = args->window_read->key,
        .length = cellul.len(&(CatenaFinitaCfg){.src = args->window_read->key, .cap = sizeof(args->window_read->key)}));
    mmgr_line_put(args->args_out, args->more);
    mmgr_line_ch(args->args_out, '}');
}

// whether the shown tab's view is served by its language server
EMBED_INLINE int window_serve(const GuiWindowCtx *args)
{
    return (args->window_read->active >= 0) && args->window_read->tab[args->window_read->active].served;
}

// the hover asked for the word at the cell `column` of the row `row` of the shown view: without the server where
// `server` is 0, the tree's parts coming at once, the ask made again with the server as that answer comes
EMBED_INLINE void window_hover_request(const GuiWindowCtx *args)
{
    if (!args->window->viewed)
    {
        return;
    }
    char more[128];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"row\":");
    mmgr_line_decimal(&line, args->row, 1u);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, args->column, 1u);
    mmgr_line_put(&line, args->server ? ",\"server\":true" : ",\"server\":false");
    (void)mmgr_line_finish(&line);
    char text[CLI_PATH_MAX_LEN + 160u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    EMBED_CALL(window_key_args, GuiWindowCtx, .window_read = args->window, .args_out = &arguments, .more = more);
    // the panels' asks wait on the tree and the language server: they run on the pool, off the inputs' lane
    args->window->asked_hover = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                           .name = "editor_view_hover", .request = &arguments);
    args->window->hover_again = !args->server && EMBED_CALL(window_serve, GuiWindowCtx, .window_read = args->window);
}

// the completions asked for the word before the shown view's cursor, one ask in flight at a time: the file's words
// first, without the server, and the server's items in an ask made again as that answer comes
EMBED_INLINE void window_complete_request(const GuiWindowCtx *args)
{
    args->window->complete_wanted = 0;
    if (!args->window->viewed || (args->window->asked_complete != 0ull))
    {
        args->window->complete_wanted = args->window->viewed;
        return;
    }
    char text[CLI_PATH_MAX_LEN + 48u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    EMBED_CALL(window_key_args, GuiWindowCtx, .window_read = args->window, .args_out = &arguments,
               .more = args->server ? ",\"server\":true" : ",\"server\":false");
    args->window->asked_complete = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                              .name = "editor_view_complete", .request = &arguments);
    args->window->complete_again = !args->server && EMBED_CALL(window_serve, GuiWindowCtx, .window_read = args->window);
}

// the signature asked for the call the shown view's cursor is in, the cursor's place recorded as the one asked
// about: the tree's first, without the server, and the server's in an ask made again as that answer comes
EMBED_INLINE void window_signature_request(const GuiWindowCtx *args)
{
    if (!args->window->viewed)
    {
        return;
    }
    char text[CLI_PATH_MAX_LEN + 48u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    EMBED_CALL(window_key_args, GuiWindowCtx, .window_read = args->window, .args_out = &arguments,
               .more = args->server ? ",\"server\":true" : ",\"server\":false");
    args->window->signature_place = args->window->editor.cursor;
    args->window->asked_signature = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                               .name = "editor_view_signature", .request = &arguments);
    args->window->signature_again =
        !args->server && EMBED_CALL(window_serve, GuiWindowCtx, .window_read = args->window);
}

// the tab `at`'s view handed to its file's language server, asked once as the view opens
EMBED_INLINE void window_serve_request(const GuiWindowCtx *args)
{
    GuiTab *const tab = &args->window->tab[args->at];
    if (tab->serve_asked)
    {
        return;
    }
    tab->serve_asked = 1;
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = tab->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
    mmgr_line_ch(&arguments, '}');
    // the server's start waits on the server: it runs on the pool, off the inputs' lane
    tab->asked_serve = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                  .name = "editor_view_serve", .request = &arguments);
}

// the server told the view the shown tab's rows rest at, its lines from the top to the last row's, where the tab is
// served: the server's slice is cut for the view, as the page told it through the same call
EMBED_INLINE void window_view_tell(const GuiWindowCtx *args)
{
    args->window->viewport_notify_deadline_ms = 0ull;
    if (!args->window->viewed || (args->window->active < 0) || !args->window->tab[args->window->active].served)
    {
        return;
    }
    const unsigned long long top = args->window->editor.top;
    const unsigned int rows = EMBED_CALL(window_rows, GuiWindowCtx, .window_read = args->window);
    unsigned long long last = top + ((rows > 0u) ? rows - 1u : 0u);
    last = ((args->window->editor.lines > 0ull) && (last >= args->window->editor.lines))
               ? args->window->editor.lines - 1ull
               : last;
    char text[(2u * CLI_PATH_MAX_LEN) + 96u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
               .text = args->window->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->window->path, .cap = sizeof(args->window->path)}));
    mmgr_line_put(&arguments, ",\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
               .text = args->window->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->window->key, .cap = sizeof(args->window->key)}));
    mmgr_line_put(&arguments, ",\"first\":");
    mmgr_line_decimal(&arguments, top, 1u);
    mmgr_line_put(&arguments, ",\"last\":");
    mmgr_line_decimal(&arguments, last, 1u);
    mmgr_line_ch(&arguments, '}');
    (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0, .name = "lsp_view",
                     .request = &arguments);
    args->window->view_told = 1;
    args->window->view_told_top = top;
    args->window->view_told_rows = rows;
}

// the view's rows moved: the server is told where they rest after GUI_VIEWPORT_DEBOUNCE_MS
EMBED_INLINE void window_view_move(const GuiWindowCtx *args)
{
    if (!args->window->view_told || (args->window->view_told_top != args->window->editor.top) ||
        (args->window->view_told_rows != EMBED_CALL(window_rows, GuiWindowCtx, .window_read = args->window)))
    {
        args->window->viewport_notify_deadline_ms = cli_clock_milliseconds();
    }
}

// the shown view's cursor put at `line`, `col`, the view following it
EMBED_INLINE void window_on_place(const GuiWindowCtx *args)
{
    char more[64];
    VerbaLinea text = {more, sizeof(more), 0ull};
    mmgr_line_put(&text, ",\"row\":");
    mmgr_line_decimal(&text, args->line, 1u);
    mmgr_line_put(&text, ",\"column\":");
    mmgr_line_decimal(&text, args->col, 1u);
    (void)mmgr_line_finish(&text);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .kind = CLI_VIEW_PLACE, .more = more);
}

// the file at `path` opened as a tab shown with its cursor at `line`, `col`: at once where its view is shown,
// else as its view's next frame comes
EMBED_INLINE void window_opened_at(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    const char *path = args->path;
    window->place_wanted = (GuiPlace){args->line, args->col};
    window->place_pending = 1;
    EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = path);
    if (window->viewed && (window->active >= 0) &&
        cellul.eq(&(CatenaFinitaCfg){.src = window->tab[window->active].path, .other = path, .cap = CLI_PATH_MAX_LEN}))
    {
        window->place_pending = 0;
        EMBED_CALL(window_on_place, GuiWindowCtx, .window = window, .line = args->line, .col = args->col);
    }
}

// the tab whose serve is in flight under `ticket`, or -1
EMBED_INLINE int window_tab_by_serve(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < args->window_read->tabs; at += 1u)
    {
        if (args->window_read->tab[at].asked_serve == args->ticket)
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the completion chosen accepted: its text written over the prefix before the cursor, the list closed
EMBED_INLINE void window_complete(const GuiWindowCtx *args)
{
    unsigned long long length = 0ull;
    const char *const insert = EMBED_CALL(cli_suggest.choose, GuiSuggestCfg, .suggest_read = &args->window->suggest,
                                          .length_pointer = &length);
    if (insert == NULL)
    {
        EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &args->window->suggest);
        return;
    }
    const GuiPlace cursor = args->window->editor.cursor;
    const unsigned long long back =
        ((cursor.line == args->window->suggest.line) && (cursor.col >= args->window->suggest.from))
            ? cursor.col - args->window->suggest.from
            : cellul.len(
                  &(CatenaFinitaCfg){.src = args->window->suggest.prefix, .cap = sizeof(args->window->suggest.prefix)});
    char more[CLI_PATH_MAX_LEN];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"text\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = insert,
               .length = length);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, back, 1u);
    const int fits = mmgr_line_finish(&line) != 0ull;
    EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &args->window->suggest);
    args->window->owed = 1;
    if (fits)
    {
        args->window->minimap.stale = 1;
        args->window->told_wanted = 1;
        EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .kind = CLI_VIEW_COMPLETE, .more = more);
    }
}

// whether the completion answer `root` of `json` is for the prefix before the cursor as it stands: its `from` on the
// cursor's line, the prefix's units ending at the cursor
EMBED_INLINE int window_complete_current(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->root, .name = "from");
    unsigned long long length = 0ull;
    const char *const prefix =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "prefix"),
                   .length_pointer = &length);
    if (!args->window_read->editor.has_cursor || (prefix == NULL) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = from,
                    .kind = CFG_JSON_OBJECT))
    {
        return 0;
    }
    const unsigned long long line = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                               .object = from, .name = "line", .otherwise = ~0ull);
    const unsigned long long col = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                              .object = from, .name = "col", .otherwise = ~0ull);
    return (line == args->window_read->editor.cursor.line) &&
           (col + EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = prefix, .size = length) ==
            args->window_read->editor.cursor.col);
}

// whether a byte of UTF-8 typed is part of a word: a letter, a digit, `_`, `$`, or any byte of a letter past ASCII
EMBED_INLINE int window_word_byte(const GuiWindowCtx *args)
{
    const unsigned char value = (unsigned char)args->byte;
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the map's lines asked for where the map placed beside the view stands for lines it does not hold or holds stale,
// one ask in flight at a time, the next wanted as the answer comes
EMBED_INLINE void window_map_request(const GuiWindowCtx *args)
{
    if (!args->window->viewed ||
        EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->window->frame.map))
    {
        return;
    }
    EMBED_CALL(gui_minimap_place.place, GuiMinimapPlaceCfg, .map = &args->window->minimap,
               .place = args->window->frame.map, .scale = args->window->frame.scale, .editor = &args->window->editor,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = args->window));
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if (!EMBED_CALL(gui_minimap_first.span, GuiMinimapFirstCfg, .map_read = &args->window->minimap, .first = &first,
                    .last = &last) ||
        EMBED_CALL(cli_minimap.current, GuiMinimapCfg, .map_read = &args->window->minimap, .first_value = first,
                   .last_value = last))
    {
        return;
    }
    if (args->window->asked_map != 0ull)
    {
        args->window->map_wanted = 1;
        return;
    }
    char text[CLI_PATH_MAX_LEN + 96u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
               .text = args->window->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->window->key, .cap = sizeof(args->window->key)}));
    mmgr_line_put(&arguments, ",\"first\":");
    mmgr_line_decimal(&arguments, first, 1u);
    mmgr_line_put(&arguments, ",\"last\":");
    mmgr_line_decimal(&arguments, last, 1u);
    mmgr_line_ch(&arguments, '}');
    args->window->asked_map = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                         .name = "editor_view_map", .request = &arguments);
}

// the view told the editor's size
EMBED_INLINE void window_view_resize(const GuiWindowCtx *args)
{
    char more[64];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"rows\":");
    mmgr_line_decimal(&line, EMBED_CALL(window_rows, GuiWindowCtx, .window_read = args->window), 1u);
    mmgr_line_put(&line, ",\"columns\":");
    mmgr_line_decimal(&line, window_columns(args), 1u);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .kind = CLI_VIEW_SIZE, .more = more);
}

void gui_window_claim_layout(const GuiWindowClaimCfg *args)
{
    args->window->owed = 1;
    if (EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &args->window->claim, .time_ms = args->time_ms))
    {
        if ((args->window->stages.stage == NULL) || (args->window->stages.made_for != args->window->claim.sent) ||
            args->window->stages_stale)
        {
            if (args->window->stage != NULL)
            {
                EMBED_CALL(gui_stages_table.stage_claim_take, GuiStagesCfg, .stage = args->window->stage,
                           .claim = &args->window->claim);
            }
            args->window->stage = NULL;
            args->window->stages_stale = 0;
            (void)EMBED_CALL(gui_stages_claim.init, GuiStagesClaimCfg, .stages = &args->window->stages,
                             .claim = &args->window->claim, .frame = &args->window->frame, .base = &args->window->base,
                             .palette = args->window->palette, .menubar = &args->window->menubar,
                             .glyphs = &args->window->glyphs, .time_ms = args->time_ms);
        }
        const GuiStage *const stage =
            EMBED_CALL(gui_stages_read.at, GuiStagesReadCfg, .stages_read = &args->window->stages, .time_ms = args->time_ms);
        if (stage != NULL)
        {
            args->window->stage = stage;
            args->window->frame = stage->frame;
            EMBED_CALL(gui_menubar_frame.titles_take, GuiMenubarFrameCfg, .menubar = &args->window->menubar,
                       .title = stage->title, .count = stage->titles, .folded_from = stage->folded_from,
                       .frame = &args->window->frame);
            return;
        }
    }
    // at rest, or the stages' cell not taken: the frame the numbers lay now, and no stage left standing
    args->window->stage = NULL;
    args->window->stages_stale = 0;
    EMBED_CALL(gui_stages_claim.free, GuiStagesClaimCfg, .stages = &args->window->stages);
    GuiClaimNumbers numbers;
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &args->window->claim, .time_ms = args->time_ms, .numbers = &numbers);
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = &args->window->frame, .numbers = &numbers);
    EMBED_CALL(gui_menubar_json.titles_layout, GuiMenubarJsonCfg, .menubar = &args->window->menubar,
               .glyphs = &args->window->glyphs, .frame = &args->window->frame);
}

EMBED_INLINE void window_claim_layout(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    EMBED_CALL(gui_window_claim.claim_layout, GuiWindowClaimCfg, .window = window, .time_ms = cli_clock_milliseconds());
}

// the claim made, as the editor takes the keys while a file is shown, or let go, as they leave it
EMBED_INLINE void window_claim(const GuiWindowCtx *args)
{
    int on = args->on;
    if ((on && !args->window->viewed) || (on == args->window->claim.claimed))
    {
        return;
    }
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &args->window->claim, .on = on);
    args->window->claim_moving = 1;
    window_claim_layout(args);
}

// whether the pointer at x, y stands over the part the claim shows
EMBED_INLINE int window_over_parts(const GuiWindowCtx *args)
{
    int x = args->x;
    int y = args->y;
    const GuiFrame *const frame = &args->window_read->frame;
    const unsigned int shown = args->window_read->claim.shown;
    if (shown == GUI_CLAIM_TOP)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->bar_slat,
                                             .other = frame->bar),
                          .x = x, .y = y) ||
               EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->tabs, .x = x, .y = y) ||
               EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->controls_pane, .x = x, .y = y);
    }
    if (shown == GUI_CLAIM_BOTTOM)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->status_slat,
                                             .other = frame->status),
                          .x = x, .y = y);
    }
    if (shown == GUI_CLAIM_LEFT)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                          .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_slat,
                                             .other = frame->strip),
                          .x = x, .y = y);
    }
    if (shown == GUI_CLAIM_RIGHT)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->map, .x = x, .y = y);
    }
    if (shown == GUI_CLAIM_CONTROLS)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->controls_pane, .x = x, .y = y);
    }
    return 0;
}

// whether the strip's tool shown has the side pane as its window
EMBED_INLINE int window_side_show(const GuiWindowCtx *args)
{
    // the tool shown is among the tools
    return (args->window_read->strip.shown >= 0) &&
           EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = (unsigned int)args->window_read->strip.shown)->side;
}

// the folder at `dir`, the top folder where empty, asked of the tree for its listing, the explorer told the ticket
EMBED_INLINE void window_listing_request(const GuiWindowCtx *args)
{
    const char *dir = args->dir;
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"dir\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = dir,
               .length = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_PATH_MAX_LEN}));
    mmgr_line_ch(&arguments, '}');
    const unsigned long long ticket = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                                 .name = "tree_list", .request = &arguments);
    if (ticket != 0ull)
    {
        EMBED_CALL(cli_explorer.listing, GuiExplorerCfg, .explorer = &args->window->explorer, .dir = dir,
                   .ticket = ticket);
    }
}

// the tab whose open is in flight under `ticket`, or -1
EMBED_INLINE int window_tab_by_ticket(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < args->window_read->tabs; at += 1u)
    {
        if (args->window_read->tab[at].asked_open == args->ticket)
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the tab whose key is `length` bytes of `key`, or -1
EMBED_INLINE int window_tab_by_key(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < args->window_read->tabs; at += 1u)
    {
        const GuiTab *const tab = &args->window_read->tab[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = tab->key, .other = args->key, .bytes = args->length}) == 0))
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the JSON text of the array `names` of `json`, the grammar's token names, stored in the tab `at`, replacing the
// text stored before
EMBED_INLINE void window_names_save(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    unsigned int names = args->names;
    GuiTab *const tab = &args->window->tab[args->at];
    g_cli_cells->persistent_buf_release(tab->names, &g_gui_cells);
    tab->names = NULL;
    tab->names_size = 0ull;
    const unsigned long long line_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = json);
    char *const line_cell = g_cli_cells->persistent_buf_alloc(line_room, &g_gui_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    if (line.out == NULL)
    {
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = names, .out = &line,
               .indent = 0u, .sorted = 0);
    if (!mmgr_line_ok(&line))
    {
        g_cli_cells->persistent_buf_release(line.out, &g_gui_cells);
        return;
    }
    tab->names = line.out;
    tab->names_size = line.at;
}

// the tab `at`'s names given to the editor, where any came
EMBED_INLINE void window_names_free(const GuiWindowCtx *args)
{
    const GuiTab *const tab = &args->window->tab[args->at];
    if (tab->names == NULL)
    {
        return;
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    CliJson json;
    const unsigned int names =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = tab->names, .size = tab->names_size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = &err_msg, .owner = &g_gui_cells);
    if (names != CLI_JSON_NONE)
    {
        (void)EMBED_CALL(gui_editor_json_names.by_name, GuiEditorJsonNamesCfg, .editor = &args->window->editor,
                         .json = &json, .names = names);
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    }
}

// the tree's session and the explorer's group written to orior's folder under the keys the page saves them by, once
// the tree's saved tabs are back: the tabs' paths in order, the one shown, and the strip's side tool shown; a window
// with no tabs and no session known writes nothing, which saves a session the folder holds from being written over
// by a read that failed to bring it. The write goes through the link while the window runs and straight to the CLI as
// it ends, when the link has let go of what it had not run
static void window_session_write(const GuiWindowCtx *args)
{
    if (!args->window->restored || (args->window->root[0] == '\0') ||
        (!args->window->session_known && (args->window->tabs == 0u)))
    {
        return;
    }
    // each tab's path twice and the one shown once, each byte escaped to at most 6
    const unsigned long long session_room =
        (((2ull * args->window->tabs) + 1ull) * ((CLI_PATH_MAX_LEN * 6ull) + 8ull)) + GUI_SAVED_MORE;
    char *const session_cell = g_cli_cells->temporary_buf_alloc(session_room, &g_gui_cells);
    VerbaLinea session = mmgr_line_over(session_cell, session_room);
    // the session as the page saves it: the tabs, the same as the main side's, the one shown, no split and no groups
    mmgr_line_put(&session, "{\"tabs\":[");
    for (unsigned int at = 0u; at < args->window->tabs; at += 1u)
    {
        mmgr_line_put(&session, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &session,
                   .text = args->window->tab[at].path,
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = args->window->tab[at].path, .cap = CLI_PATH_MAX_LEN}));
    }
    mmgr_line_put(&session, "],\"main\":[");
    for (unsigned int at = 0u; at < args->window->tabs; at += 1u)
    {
        mmgr_line_put(&session, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &session,
                   .text = args->window->tab[at].path,
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = args->window->tab[at].path, .cap = CLI_PATH_MAX_LEN}));
    }
    mmgr_line_put(&session, "],\"active\":");
    if (args->window->active >= 0)
    {
        EMBED_CALL(
            cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &session, .text = args->window->path,
            .length = cellul.len(&(CatenaFinitaCfg){.src = args->window->path, .cap = sizeof(args->window->path)}));
    }
    else
    {
        mmgr_line_put(&session, "null");
    }
    mmgr_line_put(&session, ",\"split\":null,\"groups\":[]}");
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ull};
    mmgr_line_put(&keyed, GUI_SAVED_SESSION);
    mmgr_line_put(&keyed, args->window->root);
    // the session quoted again, its bytes escaped to at most 6, and the key and the tool's name beside it
    const unsigned long long args_room = (session.at * 6ull) + (sizeof(key) * 6ull) + GUI_SAVED_MORE;
    char *const args_cell = g_cli_cells->temporary_buf_alloc(args_room, &g_gui_cells);
    VerbaLinea arguments = mmgr_line_over(args_cell, args_room);
    mmgr_line_put(&arguments, "{\"changes\":{");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = key,
               .length = mmgr_line_finish(&keyed) != 0ull ? keyed.at : 0ull);
    mmgr_line_ch(&arguments, ':');
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = session.out,
               .length = mmgr_line_ok(&session) ? session.at : 0ull);
    if (EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window))
    {
        // the tool shown is among the tools
        const GuiTool *const tool =
            EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = (unsigned int)args->window->strip.shown);
        mmgr_line_put(&arguments, ",\"" GUI_SAVED_GROUP "\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
                   .text = tool->name, .length = cellul.len(&(CatenaFinitaCfg){.src = tool->name, .cap = 32ull}));
    }
    mmgr_line_put(&arguments, "}}");
    if (mmgr_line_ok(&arguments) && mmgr_line_ok(&session))
    {
        if (args->ended)
        {
            cli_window_given(cli_window_called(1, "state_write", arguments.out, arguments.at));
        }
        else
        {
            (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "state_write",
                             .request = &arguments);
        }
    }
    g_cli_cells->persistent_buf_release(session.out, &g_gui_cells);
    g_cli_cells->persistent_buf_release(arguments.out, &g_gui_cells);
}

// the session saved through the link, as each change to it is made
EMBED_INLINE void window_save(const GuiWindowCtx *args)
{
    EMBED_CALL(window_session_write, GuiWindowCtx, .window = args->window, .ended = 0);
}

// the work's inside laid again as the strip's tool shown has a side or not, the view told the editor's size, the top
// folder's listing asked for the first time the side shows, and the group shown saved
EMBED_INLINE void window_side(const GuiWindowCtx *args)
{
    const int side = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window);
    EMBED_CALL(gui_frame_shown.side, GuiFrameShownCfg, .frame = &args->window->frame, .shown = side);
    args->window->owed = 1;
    // a movement under way runs on with the side as it now stands
    if (args->window->stage != NULL)
    {
        args->window->stages_stale = 1;
        window_claim_layout(args);
    }
    window_view_resize(args);
    if (side &&
        !EMBED_CALL(gui_explorer_node.root_request, GuiExplorerNodeCfg, .explorer_read = &args->window->explorer))
    {
        EMBED_CALL(window_listing_request, GuiWindowCtx, .window = args->window, .dir = "");
    }
    window_save(args);
}

int gui_window_resize(const GuiWindowWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    int system_scale = args->system_scale;
    // the frame is laid at the zoom's share of the scale the system gives
    args->window->system_scale = system_scale;
    // a percent of a percent is far inside what an int represents
    const int scale =
        (system_scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                        .step = args->window->preference_values.zoom_step)) /
        GUI_SCALE_WHOLE;
    // every take the size needs is made first, with nothing of the window changed: the rasters at the first size with
    // room for the largest monitor, and the base's tile tables where it has none yet; none of them from the shared
    // cells at any size after the first. A window past the rasters' room, or a take refused, saves the size it has,
    // whole
    if (args->window->framebuffer.pages == NULL)
    {
        unsigned int most_width = 0u;
        unsigned int most_height = 0u;
        EMBED_CALL(gui_system_width.display_most, GuiSystemWidthCfg, .width = &most_width, .height = &most_height);
        if (!EMBED_CALL(gui_framebuffer_most_width.init, GuiFramebufferMostWidthCfg,
                        .framebuffer = &args->window->framebuffer, .width = width, .height = height,
                        .most_width = most_width, .most_height = most_height))
        {
            return 0;
        }
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
    (void)EMBED_CALL(gui_framebuffer_most_width.fit, GuiFramebufferMostWidthCfg,
                     .framebuffer = &args->window->framebuffer, .width = width, .height = height);
    args->window->shown = NULL;
    EMBED_CALL(gui_base_width.resize, GuiBaseWidthCfg, .base = &args->window->base,
               .width = EMBED_CALL(gui_raster_width.pitch, GuiRasterWidthCfg, .width = width), .height = height);
    const int laid =
        EMBED_CALL(gui_ground_raster.layout, GuiGroundRasterCfg, .ground = &args->window->ground,
                   .raster = &args->window->framebuffer.raster[GUI_FRAMEBUFFER_GROUND], .width = width,
                   .height = height, .scale = scale, .palette = args->window->palette);
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &args->window->frame, .width = width,
               .height = height, .scale = scale);
    EMBED_CALL(gui_frame_shown.side, GuiFrameShownCfg, .frame = &args->window->frame,
               .shown = EMBED_CALL(window_side_show, GuiWindowCtx, .window_read = args->window));
    args->window->stages_stale = 1;
    EMBED_CALL(gui_menubar_json.close, GuiMenubarJsonCfg, .menubar = &args->window->menubar);
    EMBED_CALL(window_claim_layout, GuiWindowCtx, .window = args->window);
    EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
    return laid;
}

int gui_window_icon(const GuiWindowIconCfg *args)
{
    unsigned int size = args->size;
    // the eye's line at the size, no thinner than a pixel
    const float stroke = GUI_EYE_STROKE * (float)size / GUI_EYE_SIDE;
    const GuiMask *const eye = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &args->window->icons, .name = "eye",
                                          .size = size, .stroke = (stroke < 1.0f) ? 1.0f : stroke);
    if ((eye == NULL) || (eye->cover == NULL) ||
        !EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = args->icon, .width = size, .height = size))
    {
        return 0;
    }
    EMBED_CALL(gui_raster_xy_color.stencil, GuiRasterXYColorCfg, .raster = args->icon, .mask = eye,
               .color = args->window->palette->text);
    return 1;
}

// less motion as the system's wish and the saved setting now ask: the claim's slides standing where they are bound, the
// name home with no flight and the lattice saved still, and a frame owed
EMBED_INLINE void window_motion_layout(const GuiWindowCtx *args)
{
    args->window->motion_still = args->window->motion_system || args->window->motion_saved;
    EMBED_CALL(cli_claim_still.still, GuiClaimStillCfg, .claim = &args->window->claim,
               .still = args->window->motion_still);
    EMBED_CALL(cli_wordmark_still.still, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark,
               .still = args->window->motion_still);
    EMBED_CALL(cli_busy_still.still, GuiBusyStillCfg, .busy = &args->window->busy, .still = args->window->motion_still);
    EMBED_CALL(gui_ground.pause, GuiGroundCfg, .ground = &args->window->ground,
               .paused = args->window->motion_still || args->window->off_screen);
    args->window->owed = 1;
    if (args->window->stage != NULL)
    {
        window_claim_layout(args);
    }
}

void gui_window_motion_tell(const GuiWindowReducedCfg *args)
{
    args->window->motion_system = args->reduced;
    EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = args->window);
}

void gui_window_show(const GuiWindowShownCfg *args)
{
    args->window->off_screen = !args->shown;
    EMBED_CALL(gui_ground.pause, GuiGroundCfg, .ground = &args->window->ground,
               .paused = !args->shown || args->window->motion_still);
    args->window->owed = args->shown || args->window->owed;
}

// the milliseconds each rest of the caret's breath takes, at full ink and at dim ink alike
#define GUI_CARET_STEADY_MS ((GUI_CARET_BLINK_PERIOD_MS - (2ull * GUI_CARET_FADE_MS)) / 2ull)

// the ink of a slide `through` its milliseconds, from `from` to `to`, eased on a cosine: still at both ends, moving
// fastest in the middle
EMBED_INLINE unsigned int window_caret_slid(const GuiWindowCtx *args)
{
    // the share of the slide passed, 0 to 1, is a double's
    const double eased = (1.0 - cos(3.14159265358979323846 * ((double)args->through / (double)GUI_CARET_FADE_MS))) / 2.0;
    // an ink between two bytes, rounded, is a byte
    return (unsigned int)((double)args->from + (((double)args->to - (double)args->from) * eased) + 0.5);
}

// the caret's breath, its ink at each stage of it, made once: full through the first rest, sliding down, dim through
// the second rest, and sliding up again
#define GUI_CARET_BLINK_STEP_MS 16ull
static unsigned char s_caret_ink[(GUI_CARET_BLINK_PERIOD_MS / GUI_CARET_BLINK_STEP_MS) + 1ull];
static int s_caret_ink_made;

static void window_caret_init(void)
{
    for (unsigned long long stage = 0ull; stage <= GUI_CARET_BLINK_PERIOD_MS / GUI_CARET_BLINK_STEP_MS; stage += 1ull)
    {
        const unsigned long long at = stage * GUI_CARET_BLINK_STEP_MS;
        unsigned int ink = 255u;
        if ((at >= GUI_CARET_STEADY_MS) && (at < GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS))
        {
            ink = EMBED_CALL(window_caret_slid, GuiWindowCtx, .through = at - GUI_CARET_STEADY_MS, .from = 255u,
                             .to = GUI_CARET_DIM);
        }
        else if ((at >= GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS) && (at < (2ull * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS))
        {
            ink = GUI_CARET_DIM;
        }
        else if (at >= (2ull * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS)
        {
            ink = EMBED_CALL(window_caret_slid, GuiWindowCtx, .through = at - (2ull * GUI_CARET_STEADY_MS) - GUI_CARET_FADE_MS,
                             .from = GUI_CARET_DIM, .to = 255u);
        }
        // an ink is a byte
        s_caret_ink[stage] = (unsigned char)ink;
    }
    s_caret_ink_made = 1;
}

// the caret's ink at `now`, 0 to 255, where its breath stands since it began; full while no cursor stands or less
// motion is asked for
EMBED_INLINE unsigned int window_caret_ink(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (!args->window_read->editor.has_cursor || args->window_read->motion_still)
    {
        return 255u;
    }
    if (!s_caret_ink_made)
    {
        window_caret_init();
    }
    return s_caret_ink[((now_ms - args->window_read->caret_blink_start_ms) % GUI_CARET_BLINK_PERIOD_MS) / GUI_CARET_BLINK_STEP_MS];
}

// the milliseconds until the caret's ink next changes: the rest's end while it rests, else 1, which the system holds
// to the display's refresh; 0 where no cursor stands
EMBED_INLINE unsigned long long window_caret_deadline_ms(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (!args->window_read->editor.has_cursor || args->window_read->motion_still)
    {
        return 0ull;
    }
    const unsigned long long at = (now_ms - args->window_read->caret_blink_start_ms) % GUI_CARET_BLINK_PERIOD_MS;
    if (at < GUI_CARET_STEADY_MS)
    {
        return GUI_CARET_STEADY_MS - at;
    }
    const unsigned long long dim_rest_ends = (2ull * GUI_CARET_STEADY_MS) + GUI_CARET_FADE_MS;
    if ((at >= GUI_CARET_STEADY_MS + GUI_CARET_FADE_MS) && (at < dim_rest_ends))
    {
        return dim_rest_ends - at;
    }
    return 1ull;
}

void gui_window_tick(const GuiWindowCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    (void)EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &args->window->claim);
    const int moving = EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &args->window->claim, .time_ms = now_ms);
    // the frame laid for the claim's numbers while a slide moves and as it comes to rest; a tick with the claim at
    // rest owes nothing for it
    if (moving || args->window->claim_moving)
    {
        EMBED_CALL(window_claim_layout, GuiWindowCtx, .window = args->window);
    }
    if (args->window->claim_moving && !moving)
    {
        // the slides at rest, the view is told the editor's size and the map its span
        EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
        EMBED_CALL(window_map_request, GuiWindowCtx, .window = args->window);
    }
    args->window->claim_moving = moving;
    args->window->owed =
        EMBED_CALL(gui_wordmark_frame.tick, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark,
                   .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = args->window->palette,
                   .frame = &args->window->frame) ||
        args->window->owed;
    // the ground where its loop shows a change, and the menus where a rest has ended
    args->window->owed = EMBED_CALL(gui_ground_raster.tick, GuiGroundRasterCfg, .ground = &args->window->ground,
                                    .palette = args->window->palette) ||
                         args->window->owed;
    args->window->owed =
        EMBED_CALL(gui_menubar_glyphs_frame_x.tick, GuiMenubarGlyphsFrameXCfg, .menubar = &args->window->menubar,
                   .glyphs = &args->window->glyphs, .frame = &args->window->frame) ||
        args->window->owed;
    // the hover's rest and linger, and the signature's wait
    int ask = 0;
    args->window->owed =
        EMBED_CALL(gui_hover_state.tick, GuiHoverStateCfg, .hover = &args->window->hover, .ask = &ask) ||
        args->window->owed;
    if (ask)
    {
        EMBED_CALL(window_hover_request, GuiWindowCtx, .window = args->window, .row = args->window->hover.asked_row,
                   .column = args->window->hover.asked_column, .server = 0);
    }
    if (EMBED_CALL(gui_signature_text.tick, GuiSignatureTextCfg, .signature = &args->window->signature))
    {
        EMBED_CALL(window_signature_request, GuiWindowCtx, .window = args->window, .server = 0);
    }
    if ((args->window->viewport_notify_deadline_ms != 0ull) && (now_ms - args->window->viewport_notify_deadline_ms >= GUI_VIEWPORT_DEBOUNCE_MS))
    {
        EMBED_CALL(window_view_tell, GuiWindowCtx, .window = args->window);
    }
    // the busy circle coming or going owes a frame whole
    args->window->owed = EMBED_CALL(gui_busy_ticket.tick, GuiBusyTicketCfg, .busy = &args->window->busy) ||
                         args->window->owed;
    // the caret where its ink has changed and the circle where its stage has: where nothing else owes a frame, each
    // drawn again pays it alone, and a frame owed after this tick is still paid whole; no caret stands while the view
    // is not drawn, and a caret under the circle owes the frame whole, neither knowing what the other has laid
    const int caret_changed = !args->window->wordmark.stands &&
                              (EMBED_CALL(window_caret_ink, GuiWindowCtx, .window_read = args->window) !=
                               args->window->caret_ink);
    const int busy_stepped = args->window->busy.under_saved && !args->window->off_screen &&
                             (EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .busy_read = &args->window->busy) !=
                              args->window->busy.stage_drawn);
    const int crossed =
        args->window->busy.under_saved && args->window->editor.caret_saved &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window->busy.under_rect, .other = args->window->editor.caret_rect));
    const int alone = !args->window->owed && !moving && !crossed;
    args->window->caret_alone = caret_changed && alone;
    args->window->busy_alone = busy_stepped && alone;
    args->window->owed = args->window->owed || ((caret_changed || busy_stepped) && !alone);
}

// whether a panel or a menu lies over `caret`, which saves the caret from being drawn over them
EMBED_INLINE int window_caret_cover(const GuiWindowCtx *args)
{
    if (args->window_read->hover.shown &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->hover.rect, .other = args->caret)))
    {
        return 1;
    }
    if (args->window_read->suggest.open &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->suggest.rect, .other = args->caret)))
    {
        return 1;
    }
    if (args->window_read->signature.shown &&
        !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                       .one = args->window_read->signature.rect, .other = args->caret)))
    {
        return 1;
    }
    for (unsigned int level = 0u; level < args->window_read->menubar.levels; level += 1u)
    {
        if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg,
                        .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                           .one = args->window_read->menubar.menu[level].rect, .other = args->caret)))
        {
            return 1;
        }
    }
    return 0;
}

void gui_window_flip(const GuiWindowCfg *args)
{
    args->window->shown = args->window->raster;
    args->window->raster = EMBED_CALL(gui_framebuffer_most_width.flip, GuiFramebufferMostWidthCfg,
                                      .framebuffer = &args->window->framebuffer);
}

GuiRect gui_window_caret_draw(const GuiWindowCfg *args)
{
    args->window->caret_alone = 0;
    args->window->caret_ink =
        EMBED_CALL(window_caret_ink, GuiWindowCtx, .window_read = args->window);
    args->window->editor.caret_ink = args->window->caret_ink;
    return EMBED_CALL(gui_editor_raster.caret_redrawn, GuiEditorRasterCfg, .editor = &args->window->editor,
                      .raster = (args->window->shown != NULL) ? args->window->shown : args->window->raster,
                      .palette = args->window->palette);
}

GuiRect gui_window_busy_draw(const GuiWindowCfg *args)
{
    args->window->busy_alone = 0;
    return EMBED_CALL(gui_busy_frame.redrawn, GuiBusyFrameCfg, .busy = &args->window->busy,
                      .raster = (args->window->shown != NULL) ? args->window->shown : args->window->raster);
}

// the milliseconds until the view's rest is told, 0 where no telling waits
EMBED_INLINE unsigned long long window_view_due(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->window_read->viewport_notify_deadline_ms == 0ull)
    {
        return 0ull;
    }
    const unsigned long long passed = now_ms - args->window_read->viewport_notify_deadline_ms;
    return (passed < GUI_VIEWPORT_DEBOUNCE_MS) ? GUI_VIEWPORT_DEBOUNCE_MS - passed : 1ull;
}

// the lesser of two waits, 0 standing for never
EMBED_INLINE unsigned long long window_sooner(const GuiWindowCtx *args)
{
    if (args->one == 0ull)
    {
        return args->other;
    }
    return ((args->other != 0ull) && (args->other < args->one)) ? args->other : args->one;
}

unsigned long long gui_window_timeout_ms(const GuiWindowInputCfg *args)
{
    unsigned long long due =
        EMBED_CALL(gui_claim.next_deadline_ms, GuiClaimCfg, .claim_read = &args->window_read->claim);
    due = EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                     .other =
                         EMBED_CALL(gui_hover.next_deadline_ms, GuiHoverCfg, .hover_read = &args->window_read->hover));
    due = EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                     .other = EMBED_CALL(gui_signature.next_deadline_ms, GuiSignatureCfg,
                                         .signature_read = &args->window_read->signature));
    due = EMBED_CALL(
        window_sooner, GuiWindowCtx, .one = due,
        .other = EMBED_CALL(gui_ground_clock.next_deadline_ms, GuiGroundClockCfg, .ground = &args->window_read->ground));
    due = EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                     .other = EMBED_CALL(gui_wordmark_read.next_deadline_ms, GuiWordmarkReadCfg,
                                         .wordmark_read = &args->window_read->wordmark));
    due = EMBED_CALL(
        window_sooner, GuiWindowCtx, .one = due,
        .other = EMBED_CALL(gui_menubar.next_deadline_ms, GuiMenubarCfg, .menubar_read = &args->window_read->menubar));
    due = EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                     .other = EMBED_CALL(window_caret_deadline_ms, GuiWindowCtx, .window_read = args->window_read));
    // the circle's clock stands while the window is off screen, an answer waking it as any does
    due = EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                     .other =
                         args->window_read->off_screen
                             ? 0ull
                             : EMBED_CALL(gui_busy.next_deadline_ms, GuiBusyCfg, .busy_read = &args->window_read->busy));
    return EMBED_CALL(window_sooner, GuiWindowCtx, .one = due,
                      .other = EMBED_CALL(window_view_due, GuiWindowCtx, .window_read = args->window_read));
}

void gui_window_start(const GuiWindowCfg *args)
{
    char text[8];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{}");
    args->window->asked_launch = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                            .name = "launch_take", .request = &arguments);
    args->window->asked_root =
        EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "root", .request = &arguments);
    args->window->asked_catalog = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                             .name = "commands_read", .request = &arguments);
}

// the catalog of commands, answered as a JSON string holding its text, taken as its own tree, the menu bar laid out
// over it
EMBED_INLINE void window_catalog(const GuiWindowCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    if (args->window->cataloged)
    {
        EMBED_CALL(gui_menubar_json.catalog, GuiMenubarJsonCfg, .menubar = &args->window->menubar, .json = NULL,
                   .menus = CLI_JSON_NONE);
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->window->catalog);
        args->window->cataloged = 0;
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    CliJson quoted;
    const unsigned int string = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                           .json = &quoted, .text = text, .size = size, .nodes_more = 0u,
                                           .text_more = 0ull, .err_msg = &err_msg, .owner = &g_gui_cells);
    unsigned long long inner_size = 0ull;
    const char *const inner = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                         .json_read = &quoted, .node = string, .length_pointer = &inner_size);
    const unsigned int root =
        (inner != NULL) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                     .json = &args->window->catalog, .text = inner, .size = inner_size,
                                     .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_gui_cells)
                        : CLI_JSON_NONE;
    if (string != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &quoted);
    }
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    args->window->cataloged = 1;
    EMBED_CALL(gui_menubar_json.catalog, GuiMenubarJsonCfg, .menubar = &args->window->menubar,
               .json = &args->window->catalog,
               .menus = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                   .json_read = &args->window->catalog, .object = root, .name = "menus"));
    EMBED_CALL(gui_menubar_json.layout, GuiMenubarJsonCfg, .menubar = &args->window->menubar,
               .glyphs = &args->window->glyphs, .frame = &args->window->frame);
    args->window->stages_stale = args->window->stage != NULL;
}

// the catalog's commands the window runs itself; whether `command` is among them
static const char *const s_window_commands[] = {"exit",        "close-editor",  "close-all",
                                                "last-editor", "reduce-motion", "preferences"};
// the line edits of the menus with no keys of their own, which the view runs by name
static const char *const s_view_commands[] = {"sort-ascending", "sort-descending", "unique-lines",
                                              "upper-case",     "lower-case",      "title-case"};

// whether `command` is among `count` names
EMBED_INLINE int window_name_among(const GuiWindowCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = args->names_letters[at], .cap = 256ull}))
        {
            return 1;
        }
    }
    return 0;
}

EMBED_INLINE int window_runs(const GuiWindowCtx *args)
{
    return EMBED_CALL(window_name_among, GuiWindowCtx, .command = args->command, .names_letters = s_window_commands,
                      .count = sizeof(s_window_commands) / sizeof(s_window_commands[0]));
}

// `key` saved in orior's folder as `value`
EMBED_INLINE void window_preference_save(const GuiWindowCtx *args)
{
    char text[256];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"changes\":{\"");
    mmgr_line_put(&arguments, args->key);
    mmgr_line_put(&arguments, "\":\"");
    mmgr_line_put(&arguments, args->value);
    mmgr_line_put(&arguments, "\"}}");
    (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "state_write",
                     .request = &arguments);
}

// the frame laid again at the zoom and the palette, the system's scale and the window's size standing: a color or a
// zoom reaches the ground, the frame and the view through it
EMBED_INLINE void window_preferences_layout(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = window, .width = window->frame.width,
                     .height = window->frame.height, .system_scale = window->system_scale);
    window->owed = 1;
    if (window->preferences_raster.pixels != NULL)
    {
        EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = window,
                   .width = window->preferences.width, .height = window->preferences.height,
                   .system_scale = window->preferences_system_scale);
    }
}

// a percent saved in orior's folder
EMBED_INLINE void window_percent_save(const GuiWindowCtx *args)
{
    char text[8];
    VerbaLinea value = {text, sizeof(text), 0ull};
    mmgr_line_decimal(&value, args->percent, 1u);
    (void)mmgr_line_finish(&value);
    EMBED_CALL(window_preference_save, GuiWindowCtx, .window = args->window, .key = args->key, .value = text);
}

// a row of Settings moved, the window acting on it and orior's folder saving it
EMBED_INLINE void window_preference_step(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    GuiPreferenceValues *const values = &window->preference_values;
    if (args->change->at == GUI_PREFERENCE_ZOOM)
    {
        values->zoom_step = EMBED_CALL(gui_preferences_step.preference_zoom_move, GuiPreferencesStepCfg,
                                       .step = values->zoom_step, .by = args->change->by, .round = args->change->round);
        EMBED_CALL(window_percent_save, GuiWindowCtx, .window = window, .key = GUI_WINDOW_ZOOM_KEY,
                   .percent = EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                         .step = values->zoom_step));
        EMBED_CALL(window_preferences_layout, GuiWindowCtx, .window = window);
    }
    else if (args->change->at == GUI_PREFERENCE_REDUCE)
    {
        const GuiChoice reduce = {"reduce-motion", ""};
        EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &reduce);
    }
    else if (args->change->at == GUI_PREFERENCE_BAR)
    {
        values->bar_percent =
            EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg,
                       .percent = values->bar_percent, .by = args->change->by, .round = args->change->round);
        EMBED_CALL(window_percent_save, GuiWindowCtx, .window = window, .key = GUI_WINDOW_BAR_KEY,
                   .percent = values->bar_percent);
        EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &window->palette_own,
                   .bar_percent = values->bar_percent, .editor_percent = values->editor_percent);
    }
    else
    {
        values->editor_percent =
            EMBED_CALL(gui_preferences_percent.preference_opacity_move, GuiPreferencesPercentCfg,
                       .percent = values->editor_percent, .by = args->change->by, .round = args->change->round);
        EMBED_CALL(window_percent_save, GuiWindowCtx, .window = window, .key = GUI_WINDOW_EDITOR_KEY,
                   .percent = values->editor_percent);
        EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg, .own = &window->palette_own,
                   .bar_percent = values->bar_percent, .editor_percent = values->editor_percent);
    }
}

// what a key or a press changed in Preferences acted on
EMBED_INLINE void window_preference_change(const GuiWindowCtx *args)
{
    args->window->preferences_owed = 1;
    if (args->change->kind == GUI_PREFERENCE_NOTHING)
    {
        return;
    }
    args->window->owed = 1;
    if (args->change->kind == GUI_PREFERENCE_STEPPED)
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
        VerbaLinea line = {key, sizeof(key), 0ull};
        mmgr_line_put(&line, GUI_WINDOW_COLOR_KEY);
        mmgr_line_put(&line, name);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = args->change->color,
                   .text = value, .size = sizeof(value));
        EMBED_CALL(window_preference_save, GuiWindowCtx, .window = args->window, .key = key, .value = value);
        EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &args->window->palette_own,
                   .at = args->change->at, .color = args->change->color);
        window_preferences_layout(args);
    }
}

// the percent saved under `key` into `percent`, digits alone and no more than three: 1, or 0 where none is saved
EMBED_INLINE int window_percent_read(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    unsigned int *percent = args->percent_pointer;
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = args->key),
                   .length_pointer = &length);
    if ((text == NULL) || (length == 0ull) || (length > 3ull))
    {
        return 0;
    }
    unsigned int value = 0u;
    for (unsigned long long letter = 0ull; letter < length; letter += 1ull)
    {
        if ((text[letter] < '0') || (text[letter] > '9'))
        {
            return 0;
        }
        // a digit's difference from zero is far inside what an unsigned int represents
        value = (value * 10u) + (unsigned int)(text[letter] - '0');
    }
    *percent = value;
    return 1;
}

// what Preferences sets as saved in orior's folder, read as the window starts; an unknown key or an invalid value
// changes nothing
EMBED_INLINE void window_preferences_read(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    GuiPreferenceValues *const values = &args->window->preference_values;
    const unsigned int zoom_before = values->zoom_step;
    int colored = 0;
    unsigned long long length = 0ull;
    unsigned int saved = 0u;
    if (EMBED_CALL(window_percent_read, GuiWindowCtx, .json = json, .root = args->root, .key = GUI_WINDOW_ZOOM_KEY,
                   .percent_pointer = &saved))
    {
        (void)EMBED_CALL(gui_preferences.preference_zoom_step, GuiPreferencesCfg, .percent = saved,
                         .step_pointer = &values->zoom_step);
    }
    const char *const keys[2] = {GUI_WINDOW_BAR_KEY, GUI_WINDOW_EDITOR_KEY};
    unsigned int *const percents[2] = {&values->bar_percent, &values->editor_percent};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (EMBED_CALL(window_percent_read, GuiWindowCtx, .json = json, .root = args->root, .key = keys[at],
                       .percent_pointer = &saved) &&
            (saved <= GUI_PREFERENCE_OPACITY_MOST) && ((saved % GUI_PREFERENCE_OPACITY_STEP) == 0u))
        {
            *percents[at] = saved;
        }
    }
    const unsigned int colors =
        EMBED_CALL(gui_palette_length.name_count, GuiPaletteLengthCfg, .palette = args->window->palette);
    for (unsigned int at = 0u; at < colors; at += 1u)
    {
        char name[GUI_PREFERENCES_TEXT];
        char key[GUI_PREFERENCES_TEXT + 16u];
        EMBED_CALL(gui_palette_at.name_name, GuiPaletteAtCfg, .palette = args->window->palette, .at = at, .name = name,
                   .size = sizeof(name));
        VerbaLinea line = {key, sizeof(key), 0ull};
        mmgr_line_put(&line, GUI_WINDOW_COLOR_KEY);
        mmgr_line_put(&line, name);
        (void)mmgr_line_finish(&line);
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = key),
                       .length_pointer = &length);
        GuiColor color = 0u;
        if ((text != NULL) &&
            EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = text, .length = length,
                       .current = EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg,
                                             .palette = args->window->palette, .at = at),
                       .color_pointer = &color) &&
            (color !=
             EMBED_CALL(gui_palette_at.name_color, GuiPaletteAtCfg, .palette = args->window->palette, .at = at)))
        {
            EMBED_CALL(gui_palette_own.name_set, GuiPaletteOwnCfg, .own = &args->window->palette_own, .at = at,
                       .color = color);
            colored = 1;
        }
    }
    EMBED_CALL(gui_palette_own_bar_percent.opacity_layout, GuiPaletteOwnBarPercentCfg,
               .own = &args->window->palette_own, .bar_percent = values->bar_percent,
               .editor_percent = values->editor_percent);
    // the frame is laid again only for a zoom or a color that differs from what stands: laying it closes a menu open
    if ((args->window->frame.width != 0u) && (colored || (values->zoom_step != zoom_before)))
    {
        window_preferences_layout(args);
    }
    args->window->owed = 1;
}

void gui_window_preferences_resize(const GuiWindowWidthCfg *args)
{
    unsigned int width = args->width;
    unsigned int height = args->height;
    args->window->preferences_system_scale = args->system_scale;
    const int scale =
        (args->system_scale * (int)EMBED_CALL(gui_preferences_step.preference_zoom_percent, GuiPreferencesStepCfg,
                                              .step = args->window->preference_values.zoom_step)) /
        GUI_SCALE_WHOLE;
    EMBED_CALL(gui_preferences_width.resize, GuiPreferencesWidthCfg, .preferences = &args->window->preferences,
               .width = width, .height = height, .scale = scale);
    args->window->preferences_owed = 1;
    if ((args->window->preferences_raster.width == width) && (args->window->preferences_raster.height == height))
    {
        return;
    }
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &args->window->preferences_raster);
    (void)EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &args->window->preferences_raster,
                     .width = width, .height = height);
}

static void window_preferences_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    GuiPreferenceChange change;
    (void)EMBED_CALL(gui_preferences_palette_change.press, GuiPreferencesPaletteChangeCfg,
                     .preferences = &window->preferences, .palette = window->palette, .x = args->input->x,
                     .y = args->input->y, .change = &change);
    EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
}

static void window_preferences_move(const GuiInputHandlerCfg *args)
{
    args->window->preferences_owed =
        EMBED_CALL(gui_preferences_palette_change.move, GuiPreferencesPaletteChangeCfg,
                   .preferences = &args->window->preferences, .palette = args->window->palette, .x = args->input->x,
                   .y = args->input->y) ||
        args->window->preferences_owed;
}

// the pointer gone from its window: nothing is under it
static void window_preferences_leave(const GuiInputHandlerCfg *args)
{
    EMBED_CALL(window_preferences_move, GuiInputHandlerCfg, .window = args->window, .input = GUI_MOVED_INPUT(-1, -1));
}

static void window_preferences_wheel(const GuiInputHandlerCfg *args)
{
    args->window->preferences_owed =
        EMBED_CALL(gui_preferences_palette.wheel, GuiPreferencesPaletteCfg, .preferences = &args->window->preferences,
                   .palette = args->window->palette, .rows = args->input->rows) ||
        args->window->preferences_owed;
}

static void window_preferences_key(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // the command's own keys close it from its own window as they open it from the main one
    GuiChoice choice;
    if (EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &window->menubar,
                   .name = args->input->name, .choice = &choice) &&
        cellul.eq(&(CatenaFinitaCfg){.src = choice.command, .other = "preferences", .cap = sizeof(choice.command)}))
    {
        EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &choice);
        return;
    }
    GuiPreferenceChange change;
    EMBED_CALL(gui_preferences_palette_name.key, GuiPreferencesPaletteNameCfg, .preferences = &window->preferences,
               .palette = window->palette, .name = args->input->name, .change = &change);
    EMBED_CALL(window_preference_change, GuiWindowCtx, .window = window, .change = &change);
}

static void window_preferences_text(const GuiInputHandlerCfg *args)
{
    args->window->preferences_owed =
        EMBED_CALL(gui_preferences_size.type, GuiPreferencesSizeCfg, .preferences = &args->window->preferences,
                   .text = args->input->text, .size = args->input->size) ||
        args->window->preferences_owed;
}

unsigned int gui_window_preferences_hit(const GuiWindowInputCfg *args)
{
    const GuiPreferences *const preferences = &args->window_read->preferences;
    const int border =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BORDER, .scale = preferences->scale);
    // a client's sides are far inside what an int represents
    const int left = args->input->x < border;
    const int right = args->input->x >= (int)preferences->width - border;
    const int top = args->input->y < border;
    const int bottom = args->input->y >= (int)preferences->height - border;
    if (top)
    {
        return left ? GUI_HIT_TOP_LEFT : (right ? GUI_HIT_TOP_RIGHT : GUI_HIT_TOP);
    }
    if (bottom)
    {
        return left ? GUI_HIT_BOTTOM_LEFT : (right ? GUI_HIT_BOTTOM_RIGHT : GUI_HIT_BOTTOM);
    }
    if (left || right)
    {
        return left ? GUI_HIT_LEFT : GUI_HIT_RIGHT;
    }
    if (EMBED_CALL(
            gui_raster_rect.rect_holds, GuiRasterRectCfg,
            .rect = EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = preferences),
            .x = args->input->x, .y = args->input->y))
    {
        return GUI_HIT_CLIENT;
    }
    // its head moves its window
    return (args->input->y < EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PREFERENCES_EDGE,
                                        .scale = preferences->scale))
               ? GUI_HIT_CAPTION
               : GUI_HIT_CLIENT;
}

void gui_window_preferences_draw(const GuiWindowCfg *args)
{
    args->window->preferences_owed = 0;
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

void gui_window_choose(const GuiWindowCfg *args)
{
    if (cellul.eq(
            &(CatenaFinitaCfg){.src = args->choice->command, .other = "exit", .cap = sizeof(args->choice->command)}))
    {
        args->window->pressed = GUI_HIT_CLOSE;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){
            .src = args->choice->command, .other = "preferences", .cap = sizeof(args->choice->command)}))
    {
        // a system window of its own, which the program opens and closes as this says: the main window saves its keys
        // and its claim
        args->window->preferences.shown = !args->window->preferences.shown;
        args->window->preferences.entering = 0;
        args->window->preferences_owed = 1;
        args->window->owed = 1;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){
            .src = args->choice->command, .other = "close-editor", .cap = sizeof(args->choice->command)}))
    {
        if (args->window->active >= 0)
        {
            // the tab shown is among the tabs
            EMBED_CALL(gui_window_at.tab_close, GuiWindowAtCfg, .window = args->window,
                       .at = (unsigned int)args->window->active);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){
            .src = args->choice->command, .other = "close-all", .cap = sizeof(args->choice->command)}))
    {
        while (args->window->tabs != 0u)
        {
            EMBED_CALL(gui_window_at.tab_close, GuiWindowAtCfg, .window = args->window, .at = args->window->tabs - 1u);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){
            .src = args->choice->command, .other = "last-editor", .cap = sizeof(args->choice->command)}))
    {
        if ((args->window->last_active >= 0) && ((unsigned int)args->window->last_active < args->window->tabs))
        {
            EMBED_CALL(gui_window_at.tab_show, GuiWindowAtCfg, .window = args->window,
                       .at = (unsigned int)args->window->last_active);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){
            .src = args->choice->command, .other = "reduce-motion", .cap = sizeof(args->choice->command)}))
    {
        // chosen, it asks for the opposite of what the window now does, and orior's folder saves it
        args->window->motion_saved = !args->window->motion_still;
        char text[128];
        VerbaLinea arguments = {text, sizeof(text), 0ull};
        mmgr_line_put(&arguments, "{\"changes\":{\"" GUI_WINDOW_REDUCE_KEY "\":");
        mmgr_line_put(&arguments, args->window->motion_saved ? "\"true\"}}" : "\"false\"}}");
        (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "state_write",
                         .request = &arguments);
        EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = args->window);
        return;
    }
    if (args->choice->keys[0] != '\0')
    {
        EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = args->window,
                   .input = GUI_KEY_INPUT(args->choice->keys));
    }
    else if (EMBED_CALL(window_name_among, GuiWindowCtx, .command = args->choice->command,
                        .names_letters = s_view_commands,
                        .count = sizeof(s_view_commands) / sizeof(s_view_commands[0])))
    {
        EMBED_CALL(gui_window_input_kind.input, GuiWindowInputKindCfg, .window = args->window,
                   .input = GUI_KEY_INPUT(args->choice->command));
    }
}

// the shown file's path and key set to the tab `at`'s, the editor emptied for its frames
EMBED_INLINE void window_tab_take(const GuiWindowCtx *args)
{
    const GuiTab *const tab = &args->window->tab[args->at];
    // the tabs are fewer than an int counts
    args->window->last_active = ((args->window->active >= 0) && (args->window->active != (int)args->at))
                                    ? args->window->active
                                    : args->window->last_active;
    args->window->active = (int)args->at;
    VerbaLinea path = {args->window->path, sizeof(args->window->path), 0ull};
    mmgr_line_put(&path, tab->path);
    (void)mmgr_line_finish(&path);
    VerbaLinea key = {args->window->key, sizeof(args->window->key), 0ull};
    mmgr_line_put(&key, tab->key);
    (void)mmgr_line_finish(&key);
    args->window->viewed = tab->viewed;
    args->window->frame_seq = 0ull;
    args->window->menubar.has_editor = 1;
    EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &args->window->editor);
    EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &args->window->editor);
    window_names_free(args);
    EMBED_CALL(gui_minimap_json.clear, GuiMinimapJsonCfg, .map = &args->window->minimap);
    window_panels_hide(args);
    args->window->view_told = 0;
    args->window->viewport_notify_deadline_ms = 0ull;
    args->window->owed = 1;
    window_save(args);
}

void gui_window_tab_show(const GuiWindowAtCfg *args)
{
    if (args->at >= args->window->tabs)
    {
        return;
    }
    EMBED_CALL(window_tab_take, GuiWindowCtx, .window = args->window, .at = args->at);
    // the view open on the file returns a frame when it is sent the editor's size
    if (args->window->viewed)
    {
        EMBED_CALL(window_view_resize, GuiWindowCtx, .window = args->window);
    }
}

void gui_window_tab_close(const GuiWindowAtCfg *args)
{
    if (args->at >= args->window->tabs)
    {
        return;
    }
    const GuiTab *const tab = &args->window->tab[args->at];
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = tab->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
    mmgr_line_ch(&arguments, '}');
    if (tab->viewed)
    {
        // the view closes on the inputs' lane, in order with a later open under its key; the server's letting go waits
        // on the server: it runs on the pool, off that lane
        (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "editor_view_close",
                         .request = &arguments);
    }
    if (tab->served)
    {
        char file_text[CLI_PATH_MAX_LEN + 32u];
        VerbaLinea file_args = {file_text, sizeof(file_text), 0ull};
        mmgr_line_put(&file_args, "{\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &file_args, .text = tab->path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}));
        mmgr_line_ch(&file_args, '}');
        (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0, .name = "editor_server_close",
                         .request = &file_args);
    }
    (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "document_release",
                     .request = &arguments);
    g_cli_cells->persistent_buf_release(tab->names, &g_gui_cells);
    for (unsigned int after = args->at + 1u; after < args->window->tabs; after += 1u)
    {
        args->window->tab[after - 1u] = args->window->tab[after];
    }
    args->window->tabs -= 1u;
    // the tabs are fewer than an int counts
    args->window->last_active = ((args->window->last_active > (int)args->at)    ? args->window->last_active - 1
                                 : (args->window->last_active == (int)args->at) ? -1
                                                                                : args->window->last_active);
    args->window->owed = 1;
    if (args->window->tabs == 0u)
    {
        args->window->active = -1;
        args->window->path[0] = '\0';
        args->window->key[0] = '\0';
        args->window->viewed = 0;
        args->window->frame_seq = 0ull;
        args->window->menubar.has_editor = 0;
        EMBED_CALL(gui_editor_json_names.free, GuiEditorJsonNamesCfg, .editor = &args->window->editor);
        EMBED_CALL(gui_editor_table.init, GuiEditorCfg, .editor = &args->window->editor);
        EMBED_CALL(gui_minimap_json.clear, GuiMinimapJsonCfg, .map = &args->window->minimap);
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = args->window);
        EMBED_CALL(window_claim, GuiWindowCtx, .window = args->window, .on = 0);
        // the last tab closed is the session's own change: saved, tabs or none
        args->window->session_known = 1;
        EMBED_CALL(window_save, GuiWindowCtx, .window = args->window);
        return;
    }
    if ((int)args->at == args->window->active)
    {
        args->window->active = -1;
        EMBED_CALL(gui_window_at.tab_show, GuiWindowAtCfg, .window = args->window,
                   .at = (args->at < args->window->tabs) ? args->at : args->window->tabs - 1u);
        return;
    }
    if ((int)args->at < args->window->active)
    {
        args->window->active -= 1;
    }
    EMBED_CALL(window_save, GuiWindowCtx, .window = args->window);
}

// the file at `length` bytes of `path`, a path from the tree's top, opened as a tab shown: its tab where it is open
// already, else a new one with its branch opened under a key of its own
static void window_branch_open(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    unsigned long long length = args->length;
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        const GuiTab *const tab = &window->tab[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = tab->path, .other = args->path, .bytes = length}) == 0))
        {
            EMBED_CALL(gui_window_at.tab_show, GuiWindowAtCfg, .window = window, .at = at);
            return;
        }
    }
    if (window->tabs >= GUI_TABS_MOST)
    {
        return;
    }
    GuiTab *const tab = &window->tab[window->tabs];
    VerbaLinea saved = {tab->path, sizeof(tab->path), 0ull};
    mmgr_line_put_n(&saved, args->path, length);
    (void)mmgr_line_finish(&saved);
    VerbaLinea key = {tab->key, sizeof(tab->key), 0ull};
    mmgr_line_put(&key, GUI_OPEN_KEY_HEAD);
    mmgr_line_put_n(&key, args->path, length);
    (void)mmgr_line_finish(&key);
    tab->viewed = 0;
    tab->crlf = 0;
    tab->names = NULL;
    tab->names_size = 0ull;
    tab->asked_serve = 0ull;
    tab->serve_asked = 0;
    tab->served = 0;
    window->tabs += 1u;
    EMBED_CALL(window_tab_take, GuiWindowCtx, .window = window, .at = window->tabs - 1u);
    char text[(2u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = window->key,
               .length = key.at);
    mmgr_line_put(&arguments, ",\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = window->path,
               .length = saved.at);
    mmgr_line_put(&arguments, ",\"first\":0,\"last\":");
    mmgr_line_decimal(&arguments, EMBED_CALL(window_rows, GuiWindowCtx, .window_read = window), 1u);
    mmgr_line_put(&arguments, ",\"columns\":0}");
    tab->asked_open =
        EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 1, .name = "branch_open", .request = &arguments);
}

void gui_window_file_open(const GuiWindowPathCfg *args)
{
    // a path that is not whole is from the tree's top already; a whole one is made the tree's by the CLI, and saved as
    // given for the open where the tree holds no shorter one for it
    const int whole =
        (args->path[0] == '/') || (args->path[0] == '\\') || ((args->path[0] != '\0') && (args->path[1] == ':'));
    VerbaLinea saved = {args->window->path, sizeof(args->window->path), 0ull};
    mmgr_line_put(&saved, args->path);
    (void)mmgr_line_finish(&saved);
    if (!whole)
    {
        EMBED_CALL(window_branch_open, GuiWindowCtx, .window = args->window, .path = args->path, .length = saved.at);
        return;
    }
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = args->path,
               .length = saved.at);
    mmgr_line_ch(&arguments, '}');
    args->window->asked_relative = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 0,
                                              .name = "tree_relative", .request = &arguments);
}

int gui_window_text_put(const GuiWindowTextCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    args->window->owed = 1;
    return EMBED_CALL(gui_editor_text.text_put, GuiEditorTextCfg, .editor = &args->window->editor, .text = text,
                      .length = length);
}

// the tree's name: the last part of its path
EMBED_INLINE void window_title(const GuiWindowCtx *args)
{
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        if (((args->root_text[at] == '/') || (args->root_text[at] == '\\')) && (at + 1ull < args->length))
        {
            start = at + 1ull;
        }
    }
    VerbaLinea title = {args->window->title, sizeof(args->window->title), 0ull};
    mmgr_line_put_n(&title, args->root_text + start, args->length - start);
    (void)mmgr_line_finish(&title);
}

// the open's answer for the tab `at` taken: the grammar's names saved with the tab and given to the editor where the
// tab is shown, and the text, read whole or paged, shown through the CLI's editor view, opened on it sized to the
// editor
EMBED_INLINE void window_on_open(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    GuiTab *const tab = &args->window->tab[args->at];
    // the tabs are fewer than an int counts
    const int shown = (int)args->at == args->window->active;
    const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "names");
    if (names != CLI_JSON_NONE)
    {
        EMBED_CALL(window_names_save, GuiWindowCtx, .window = args->window, .at = args->at, .json = json,
                   .names = names);
        if (shown)
        {
            (void)EMBED_CALL(gui_editor_json_names.by_name, GuiEditorJsonNamesCfg, .editor = &args->window->editor,
                             .json = json, .names = names);
        }
    }
    const int paged = EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = json, .object = args->root,
                                 .name = "windowed") &&
                      EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = json, .object = args->root, .name = "shape"),
                                 .kind = CFG_JSON_OBJECT);
    // a text read whole says how its lines end: a carriage return before a newline, as the text or as its escape
    unsigned long long text_length = 0ull;
    const char *const whole =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "text"),
                   .length_pointer = &text_length);
    if (whole != NULL)
    {
        int crlf = 0;
        for (unsigned long long each = 0ull; (each + 1ull < text_length) && !crlf; each += 1ull)
        {
            crlf = ((whole[each] == '\r') && (whole[each + 1ull] == '\n')) ||
                   ((whole[each] == '\\') && (whole[each + 1ull] == 'r') && (each + 3ull < text_length) &&
                    (whole[each + 2ull] == '\\') && (whole[each + 3ull] == 'n'));
        }
        tab->crlf = crlf;
    }
    if (paged || (whole != NULL))
    {
        char text[(2u * CLI_PATH_MAX_LEN) + 128u];
        VerbaLinea arguments = {text, sizeof(text), 0ull};
        mmgr_line_put(&arguments, "{\"key\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = tab->key,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
        mmgr_line_put(&arguments, ",\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments, .text = tab->path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}));
        mmgr_line_put(&arguments, ",\"rows\":");
        mmgr_line_decimal(&arguments, EMBED_CALL(window_rows, GuiWindowCtx, .window_read = args->window), 1u);
        mmgr_line_put(&arguments, ",\"columns\":");
        mmgr_line_decimal(&arguments, window_columns(args), 1u);
        mmgr_line_ch(&arguments, '}');
        const unsigned long long ticket = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                                     .name = "editor_view_open", .request = &arguments);
        args->window->asked_view = shown ? ticket : args->window->asked_view;
    }
}

// what orior's folder saves for the tree taken, the answer `root` of `json`: the explorer's group shown as the
// strip's tool of that name with its side, and the tree's session, its tabs opened in order and the one shown; the
// session is saved again from here on
EMBED_INLINE void window_restore(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    const CliJson *json = args->json;
    unsigned long long length = 0ull;
    const char *const group =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = GUI_SAVED_GROUP),
                   .length_pointer = &length);
    for (unsigned int at = 0u; (group != NULL) && (at < gui_strip_tools()); at += 1u)
    {
        const GuiTool *const tool = EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = at);
        if (tool->side && (cellul.len(&(CatenaFinitaCfg){.src = tool->name, .cap = 32ull}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = tool->name, .other = group, .bytes = length}) == 0))
        {
            // the tools are fewer than an int counts
            window->strip.shown = (int)at;
            EMBED_CALL(window_side, GuiWindowCtx, .window = window);
        }
    }
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ull};
    mmgr_line_put(&keyed, GUI_SAVED_SESSION);
    mmgr_line_put(&keyed, window->root);
    unsigned long long size = 0ull;
    const char *const session =
        (mmgr_line_finish(&keyed) != 0ull)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->root, .name = key),
                         .length_pointer = &size)
            : NULL;
    window->session_known = (session != NULL);
    if (session != NULL)
    {
        char err_text[256];
        VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
        CliJson saved;
        const unsigned int object = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = &saved, .text = session, .size = size, .nodes_more = 0u,
                                               .text_more = 0ull, .err_msg = &err_msg, .owner = &g_gui_cells);
        if (object != CLI_JSON_NONE)
        {
            const unsigned int tabs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &saved, .object = object, .name = "tabs");
            const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = &saved, .node = tabs, .kind = CFG_JSON_ARRAY)
                                           ? saved.node[tabs].count
                                           : 0u;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                unsigned long long path_length = 0ull;
                const char *const path =
                    EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &saved,
                               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                  .json_read = &saved, .array = tabs, .index = at),
                               .length_pointer = &path_length);
                char copied[CLI_PATH_MAX_LEN];
                VerbaLinea line = {copied, sizeof(copied), 0ull};
                mmgr_line_put_n(&line, (path != NULL) ? path : "", path_length);
                if ((path != NULL) && (mmgr_line_finish(&line) != 0ull))
                {
                    EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = copied);
                }
            }
            unsigned long long active_length = 0ull;
            const char *const active =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &saved,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &saved, .object = object, .name = "active"),
                           .length_pointer = &active_length);
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea line = {shown, sizeof(shown), 0ull};
            mmgr_line_put_n(&line, (active != NULL) ? active : "", active_length);
            if ((active != NULL) && (mmgr_line_finish(&line) != 0ull))
            {
                EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = shown);
            }
            EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &saved);
        }
    }
    window->restored = 1;
}

// the completion's list, where it is open, followed to the frame's cursor: filtered again by the word between the
// prefix's start and the cursor, read off the editor's own line, and closed where the cursor has left the line, stands
// before the prefix's start, or has a byte between that is no word's
EMBED_INLINE void window_suggest_follow(const GuiWindowCtx *args)
{
    GuiSuggest *const suggest = &args->window->suggest;
    if (!suggest->open)
    {
        return;
    }
    const GuiPlace cursor = args->window->editor.cursor;
    const char *text = NULL;
    unsigned long long length = 0ull;
    if (!args->window->editor.has_cursor || (cursor.line != suggest->line) || (cursor.col < suggest->from) ||
        !EMBED_CALL(gui_editor_from.units_text, GuiEditorFromCfg, .editor_read = &args->window->editor,
                    .line = cursor.line, .from = suggest->from, .to = cursor.col, .text_pointer = &text,
                    .length_pointer = &length))
    {
        EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = suggest);
        return;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (!EMBED_CALL(window_word_byte, GuiWindowCtx, .byte = text[at]))
        {
            EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = suggest);
            return;
        }
    }
    (void)EMBED_CALL(gui_suggest_json.prefix, GuiSuggestJsonCfg, .suggest = suggest, .prefix = text, .length = length);
    args->window->owed = 1;
}

// a frame of the view taken, where it is the open file's and no older than the last taken
EMBED_INLINE void window_frame_take(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ull;
    const char *const key =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "key"),
                   .length_pointer = &length);
    const unsigned long long seq = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                              .object = args->root, .name = "seq", .otherwise = 0ull);
    if ((key == NULL) ||
        (length != cellul.len(&(CatenaFinitaCfg){.src = args->window->key, .cap = sizeof(args->window->key)})) ||
        (memor.cmp(&(MemoriaCfg){.src = key, .other = args->window->key, .bytes = length}) != 0) ||
        (seq < args->window->frame_seq))
    {
        return;
    }
    args->window->frame_seq = seq;
    args->window->viewed = 1;
    if (args->window->active >= 0)
    {
        args->window->tab[args->window->active].viewed = 1;
    }
    (void)EMBED_CALL(gui_editor_json_frame.frame_put, GuiEditorJsonFrameCfg, .editor = &args->window->editor,
                     .json = json, .frame = args->root);
    // the caret's breath begins again with every frame, which saves the caret at full ink while the keys go
    args->window->caret_blink_start_ms = cli_clock_milliseconds();
    window_view_move(args);
    window_suggest_follow(args);
}

// whether an answer is a frame of the view, by what it carries
EMBED_INLINE int window_is_frame(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    return (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->root,
                       .name = "rows") != CLI_JSON_NONE) &&
           (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->root,
                       .name = "cursor") != CLI_JSON_NONE) &&
           (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->root,
                       .name = "seq") != CLI_JSON_NONE);
}

// a door command run: `open-file` opens its file; the command answered done, or not and why
EMBED_INLINE void window_command(const GuiWindowCtx *args)
{
    GuiWindow *window = args->window;
    const CliJson *json = args->json;
    unsigned long long id_length = 0ull;
    const char *const id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "id"),
                   .length_pointer = &id_length);
    unsigned long long name_length = 0ull;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->root, .name = "command"),
                   .length_pointer = &name_length);
    if ((id == NULL) || (name == NULL))
    {
        return;
    }
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->root, .name = "args");
    unsigned long long path_length = 0ull;
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                      .array = arguments, .index = 0u),
                   .length_pointer = &path_length);
    const int opening = (name_length == 9ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = name, .other = "open-file", .bytes = 9u}) == 0) &&
                        (path != NULL);
    if (opening)
    {
        char copied[CLI_PATH_MAX_LEN];
        VerbaLinea line = {copied, sizeof(copied), 0ull};
        mmgr_line_put_n(&line, path, path_length);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = copied);
    }
    char text[CLI_NAME_MAX_LEN + 256u];
    VerbaLinea answer = {text, sizeof(text), 0ull};
    mmgr_line_put(&answer, "{\"id\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &answer, .text = id,
               .length = id_length);
    mmgr_line_put(&answer, opening ? ",\"done\":true,\"message\":\"\"}"
                                   : ",\"done\":false,\"message\":\"the window has no command ");
    if (!opening)
    {
        mmgr_line_put_n(&answer, name, name_length);
        mmgr_line_put(&answer, "\"}");
    }
    (void)EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 0, .name = "command_answer",
                     .request = &answer);
}

// what orior's folder saves read for the name, `is_object` 0 where it could not be: the session last started in down to
// its two words, and the session the system names asked for
EMBED_INLINE void window_mark_saved_read(const GuiWindowCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ull;
    const char *const saved =
        args->is_object
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->root, .name = GUI_WORDMARK_SESSION_KEY),
                         .length_pointer = &length)
            : NULL;
    (void)EMBED_CALL(gui_wordmark_text.session_read, GuiWordmarkTextCfg, .text = saved, .size = length,
                     .session = &args->window->wordmark.saved);
    char text[8];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{}");
    args->window->asked_mark_session = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                                  .name = "session_key", .request = &arguments);
}

// the session the system names come, `named`, `size` bytes of it, NULL where it names none: saved as the session
// started in, and the name set going on the first start in it, or the eye home at once on any start after
EMBED_INLINE void window_mark_session_by_name(const GuiWindowCtx *args)
{
    unsigned long long size = args->size;
    GuiWordmarkSession session;
    (void)EMBED_CALL(gui_wordmark_text.session_read, GuiWordmarkTextCfg, .text = args->named, .size = size,
                     .session = &session);
    const int first = EMBED_CALL(gui_wordmark.first_in, GuiWordmarkCfg, .saved = &args->window->wordmark.saved,
                                 .now_wordmark_session_pointer = &session);
    if (args->named != NULL)
    {
        char text[2048];
        VerbaLinea arguments = {text, sizeof(text), 0ull};
        mmgr_line_put(&arguments, "{\"changes\":{\"" GUI_WORDMARK_SESSION_KEY "\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &arguments,
                   .text = args->named, .length = size);
        mmgr_line_put(&arguments, "}}");
        (void)EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1, .name = "state_write",
                         .request = &arguments);
    }
    if (first)
    {
        EMBED_CALL(gui_wordmark_frame.stand, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark);
    }
    else
    {
        EMBED_CALL(cli_wordmark_still.land, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    }
}

// the name told the tree is read while its pane stands: the root answered, and what the folder saves for a tree back
// with its tabs opened, where there is a tree
EMBED_INLINE void window_mark_tree_read(const GuiWindowCtx *args)
{
    if (args->window->wordmark.stands && !args->window->wordmark.tree_read && (args->window->asked_root == 0ull) &&
        (args->window->asked_state == 0ull) && ((args->window->root[0] == '\0') || args->window->restored))
    {
        EMBED_CALL(cli_wordmark_still.tree_read, GuiWordmarkStillCfg, .wordmark = &args->window->wordmark);
    }
}

// one answer or event taken
static void window_take(const GuiLinkEachCfg *args)
{
    unsigned long long ticket = args->ticket;
    const char *name = args->name;
    const char *text = args->text;
    unsigned long long size = args->size;
    GuiWindow *const window = (GuiWindow *)args->context;
    if (ticket != 0ull)
    {
        EMBED_CALL(gui_busy_ticket.answer, GuiBusyTicketCfg, .busy = &window->busy, .ticket = ticket);
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    // the answer's tree read on the window thread's lane against a mark, the lane left before the answer is handled:
    // what the handling saves is taken where it is saved, and the lane released at the mark once the tree is given
    // back
    CliJson json;
    const unsigned int root =
        args->ok ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                              .json = &json, .text = text, .size = size, .nodes_more = 0u, .text_more = 0ull,
                              .err_msg = &err_msg, .owner = &g_gui_cells)
                 : CLI_JSON_NONE;
    window->owed = 1;
    if ((ticket != 0ull) && (ticket == window->asked_launch))
    {
        window->asked_launch = 0ull;
        unsigned long long length = 0ull;
        const char *const command =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "command"),
                       .length_pointer = &length);
        unsigned long long path_length = 0ull;
        const char *const path = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                               .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = &json, .object = root, .name = "args"),
                               .index = 0u),
            .length_pointer = &path_length);
        if ((command != NULL) && (length == 9ull) &&
            (memor.cmp(&(MemoriaCfg){.src = command, .other = "open-file", .bytes = 9u}) == 0) && (path != NULL))
        {
            char copied[CLI_PATH_MAX_LEN];
            VerbaLinea line = {copied, sizeof(copied), 0ull};
            mmgr_line_put_n(&line, path, path_length);
            (void)mmgr_line_finish(&line);
            EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = copied);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_root))
    {
        window->asked_root = 0ull;
        unsigned long long length = 0ull;
        const char *const tree = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &json, .node = root, .length_pointer = &length);
        if (tree != NULL)
        {
            EMBED_CALL(window_title, GuiWindowCtx, .window = window, .root_text = tree, .length = length);
            VerbaLinea saved = {window->root, sizeof(window->root), 0ull};
            mmgr_line_put_n(&saved, tree, length);
            (void)mmgr_line_finish(&saved);
        }
        // what orior's folder saves is asked once whether there is a tree is known: the tree's places and the name's
        // session are one answer
        char args_text[8];
        VerbaLinea arguments = {args_text, sizeof(args_text), 0ull};
        mmgr_line_put(&arguments, "{}");
        window->asked_state = EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 1, .name = "state_read",
                                         .request = &arguments);
    }
    else if ((ticket != 0ull) && (ticket == window->asked_state))
    {
        window->asked_state = 0ull;
        const int is_object = args->ok && (root != CLI_JSON_NONE) &&
                              EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                         .node = root, .kind = CFG_JSON_OBJECT);
        // less motion as orior's folder saves it, read before the name stands
        unsigned long long reduce_length = 0ull;
        const char *const reduce =
            is_object
                ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                             .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = root, .name = GUI_WINDOW_REDUCE_KEY),
                             .length_pointer = &reduce_length)
                : NULL;
        window->motion_saved = (reduce != NULL) && (reduce_length == 4ull) &&
                               (memor.cmp(&(MemoriaCfg){.src = reduce, .other = "true", .bytes = 4u}) == 0);
        EMBED_CALL(window_motion_layout, GuiWindowCtx, .window = window);
        if (is_object)
        {
            EMBED_CALL(window_preferences_read, GuiWindowCtx, .window = window, .json = &json, .root = root);
        }
        EMBED_CALL(window_mark_saved_read, GuiWindowCtx, .window = window, .is_object = is_object, .json = &json,
                   .root = root);
        if (is_object && (window->root[0] != '\0'))
        {
            EMBED_CALL(window_restore, GuiWindowCtx, .window = window, .json = &json, .root = root);
        }
        else
        {
            window->restored = 1;
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_mark_session))
    {
        window->asked_mark_session = 0ull;
        EMBED_CALL(window_mark_session_by_name, GuiWindowCtx, .window = window, .named = args->ok ? text : NULL,
                   .size = size);
    }
    else if ((ticket != 0ull) && (ticket == window->asked_catalog))
    {
        window->asked_catalog = 0ull;
        if (args->ok)
        {
            EMBED_CALL(window_catalog, GuiWindowCtx, .window = window, .text = text, .size = size);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_relative))
    {
        window->asked_relative = 0ull;
        unsigned long long length = 0ull;
        const char *const relative = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &json, .node = root, .length_pointer = &length);
        if (relative != NULL)
        {
            EMBED_CALL(window_branch_open, GuiWindowCtx, .window = window, .path = relative, .length = length);
        }
        else
        {
            EMBED_CALL(window_branch_open, GuiWindowCtx, .window = window, .path = window->path,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = window->path, .cap = sizeof(window->path)}));
        }
    }
    else if ((ticket != 0ull) &&
             (EMBED_CALL(window_tab_by_ticket, GuiWindowCtx, .window_read = window, .ticket = ticket) >= 0))
    {
        // the tab is among the tabs
        const unsigned int at =
            (unsigned int)EMBED_CALL(window_tab_by_ticket, GuiWindowCtx, .window_read = window, .ticket = ticket);
        window->tab[at].asked_open = 0ull;
        if (root != CLI_JSON_NONE)
        {
            EMBED_CALL(window_on_open, GuiWindowCtx, .window = window, .json = &json, .root = root, .at = at);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_map))
    {
        window->asked_map = 0ull;
        if (args->ok && (root != CLI_JSON_NONE))
        {
            (void)EMBED_CALL(gui_minimap_json.put, GuiMinimapJsonCfg, .map = &window->minimap, .json = &json,
                             .root = root);
        }
        if (window->map_wanted)
        {
            window->map_wanted = 0;
            EMBED_CALL(window_map_request, GuiWindowCtx, .window = window);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_hover))
    {
        window->asked_hover = 0ull;
        if (args->ok && (root != CLI_JSON_NONE))
        {
            (void)EMBED_CALL(gui_hover_state.put, GuiHoverStateCfg, .hover = &window->hover, .json = &json, .root = root);
        }
        // the answer without the server shown, the server is asked where the pointer still rests on the cell
        if (window->hover_again && window->hover.asked && (window->hover.row == window->hover.asked_row) &&
            (window->hover.column == window->hover.asked_column))
        {
            EMBED_CALL(window_hover_request, GuiWindowCtx, .window = window, .row = window->hover.asked_row,
                       .column = window->hover.asked_column, .server = 1);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_complete))
    {
        // an answer for a prefix the cursor has left, an edit having come between the ask and the answer, is dropped
        window->asked_complete = 0ull;
        const int current =
            args->ok && (root != CLI_JSON_NONE) &&
            EMBED_CALL(window_complete_current, GuiWindowCtx, .window_read = window, .json = &json, .root = root);
        if (current)
        {
            (void)EMBED_CALL(gui_suggest_json.put, GuiSuggestJsonCfg, .suggest = &window->suggest, .json = &json,
                             .root = root, .forced = window->complete_forced);
        }
        const int again = window->complete_again && current && window->suggest.open;
        window->complete_again = 0;
        if (window->complete_wanted)
        {
            EMBED_CALL(window_complete_request, GuiWindowCtx, .window = window, .server = 0);
        }
        else if (again)
        {
            // the list open on the file's words, the server's items are asked for, the forced ask saved forced
            const int forced = window->complete_forced;
            EMBED_CALL(window_complete_request, GuiWindowCtx, .window = window, .server = 1);
            window->complete_forced = forced;
        }
        else
        {
            window->complete_forced = 0;
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_signature))
    {
        window->asked_signature = 0ull;
        if (args->ok && (root != CLI_JSON_NONE))
        {
            (void)EMBED_CALL(gui_signature_json.put, GuiSignatureJsonCfg, .signature = &window->signature,
                             .json = &json, .root = root);
        }
        else
        {
            EMBED_CALL(gui_signature_json.hide, GuiSignatureJsonCfg, .signature = &window->signature);
        }
        // the tree's answer shown, the server's is asked for where the cursor stands where it was asked
        if (window->signature_again && (window->editor.cursor.line == window->signature_place.line) &&
            (window->editor.cursor.col == window->signature_place.col))
        {
            EMBED_CALL(window_signature_request, GuiWindowCtx, .window = window, .server = 1);
        }
    }
    else if ((ticket != 0ull) &&
             (EMBED_CALL(window_tab_by_serve, GuiWindowCtx, .window_read = window, .ticket = ticket) >= 0))
    {
        // the tab is among the tabs
        GuiTab *const tab = &window->tab[(unsigned int)EMBED_CALL(window_tab_by_serve, GuiWindowCtx,
                                                                  .window_read = window, .ticket = ticket)];
        tab->asked_serve = 0ull;
        tab->served = args->ok && (root != CLI_JSON_NONE) &&
                      EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = &json, .object = root,
                                 .name = "served");
        // a view served is told to the server as its rows rest
        if (tab->served && (tab == &window->tab[window->active]))
        {
            window->view_told = 0;
            EMBED_CALL(window_view_move, GuiWindowCtx, .window = window);
        }
    }
    else if ((ticket != 0ull) && (root != CLI_JSON_NONE) &&
             EMBED_CALL(window_is_frame, GuiWindowCtx, .json = &json, .root = root))
    {
        // a frame of any tab's view says its view is open; the shown tab's is drawn; a view open is handed to its
        // file's language server once
        window->asked_view = (ticket == window->asked_view) ? 0ull : window->asked_view;
        unsigned long long length = 0ull;
        const char *const key =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "key"),
                       .length_pointer = &length);
        const int at = (key != NULL) ? EMBED_CALL(window_tab_by_key, GuiWindowCtx, .window_read = window, .key = key,
                                                  .length = length)
                                     : -1;
        if (at >= 0)
        {
            window->tab[at].viewed = 1;
            // the tab is among the tabs
            EMBED_CALL(window_serve_request, GuiWindowCtx, .window = window, .at = (unsigned int)at);
        }
        if ((at >= 0) && (at == window->active))
        {
            EMBED_CALL(window_frame_take, GuiWindowCtx, .window = window, .json = &json, .root = root);
            EMBED_CALL(window_map_request, GuiWindowCtx, .window = window);
            // a place wanted, as a problem's row picked it, is put as the view's first frame comes
            if (window->place_pending)
            {
                window->place_pending = 0;
                EMBED_CALL(window_on_place, GuiWindowCtx, .window = window, .line = window->place_wanted.line,
                           .col = window->place_wanted.col);
            }
            // the completion waits for the frame that places the cursor after the key typed; a signature shown
            // follows the cursor, asked again as the cursor leaves the place it was asked at
            if (window->told_wanted)
            {
                // the server is told the text an input changed, off the inputs' lane
                window->told_wanted = 0;
                char text[CLI_PATH_MAX_LEN + 32u];
                VerbaLinea arguments = {text, sizeof(text), 0ull};
                EMBED_CALL(window_key_args, GuiWindowCtx, .window_read = window, .args_out = &arguments, .more = "");
                (void)EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 0, .name = "editor_view_told",
                                 .request = &arguments);
            }
            if (window->complete_wanted)
            {
                EMBED_CALL(window_complete_request, GuiWindowCtx, .window = window, .server = 0);
            }
            const int moved = (window->editor.cursor.line != window->signature_place.line) ||
                              (window->editor.cursor.col != window->signature_place.col);
            if (window->signature.shown && (window->asked_signature == 0ull) && moved)
            {
                EMBED_CALL(window_signature_request, GuiWindowCtx, .window = window, .server = 0);
            }
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_memory))
    {
        window->asked_memory = 0ull;
        if (args->ok && (root != CLI_JSON_NONE))
        {
            window->memory_bytes =
                EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json,
                           .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = root, .name = "orior"),
                           .name = "working", .otherwise = 0ull);
        }
    }
    else if ((ticket != 0ull) && args->ok && (root != CLI_JSON_NONE) &&
             EMBED_CALL(gui_explorer_ticket.list, GuiExplorerTicketCfg, .explorer = &window->explorer, .ticket = ticket,
                        .json = &json, .root = root))
    {
        window->owed = 1;
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "branch-colors", .cap = 64ull}))
    {
        unsigned long long length = 0ull;
        const char *const key =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "key"),
                       .length_pointer = &length);
        const int at = (key != NULL) ? EMBED_CALL(window_tab_by_key, GuiWindowCtx, .window_read = window, .key = key,
                                                  .length = length)
                                     : -1;
        const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = root, .name = "names");
        if ((at >= 0) && (names != CLI_JSON_NONE))
        {
            // the tab is among the tabs
            EMBED_CALL(window_names_save, GuiWindowCtx, .window = window, .at = (unsigned int)at, .json = &json,
                       .names = names);
        }
        if ((at >= 0) && (at == window->active))
        {
            if (names != CLI_JSON_NONE)
            {
                (void)EMBED_CALL(gui_editor_json_names.by_name, GuiEditorJsonNamesCfg, .editor = &window->editor,
                                 .json = &json, .names = names);
            }
            (void)EMBED_CALL(gui_editor_json.color, GuiEditorJsonCfg, .editor = &window->editor, .json = &json,
                             .colors = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "colors"));
            // the bars take their names from the runs, which the colors change
            window->minimap.stale = 1;
            EMBED_CALL(window_map_request, GuiWindowCtx, .window = window);
        }
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "command", .cap = 64ull}))
    {
        EMBED_CALL(window_command, GuiWindowCtx, .window = window, .json = &json, .root = root);
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "lsp-diagnostics", .cap = 64ull}))
    {
        // a file's diagnostics come as its server or a checker reports them; the strip's Problems tool is marked
        // while errors stand
        (void)EMBED_CALL(gui_problems_json.put, GuiProblemsJsonCfg, .problems_out = &window->problems, .json = &json,
                         .root = root);
        window->strip.problems_marked = window->problems.errors > 0u;
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "tree-check", .cap = 64ull}))
    {
        EMBED_CALL(cli_problems.checking, GuiProblemsCfg, .problems_out = &window->problems,
                   .done = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root,
                                      .name = "done", .otherwise = 0ull),
                   .total = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json,
                                       .object = root, .name = "total", .otherwise = 0ull));
        // the check runs on past its ask's answer, its progress coming as these events: the busy circle stands through
        // it
        EMBED_CALL(gui_busy_running.work, GuiBusyRunningCfg, .busy = &window->busy,
                   .running = EMBED_CALL(gui_problems_path.running, GuiProblemsPathCfg, .problems = &window->problems));
    }
    else if ((ticket != 0ull) && (ticket == window->asked_check))
    {
        window->asked_check = 0ull;
    }
    if (args->ok)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    }
}

// the memory reading asked for, where none is in flight and the last came GUI_MEMORY_POLL_MS milliseconds ago
EMBED_INLINE void window_memory_request(const GuiWindowCtx *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if ((args->window->asked_memory != 0ull) ||
        ((args->window->memory_poll_deadline_ms != 0ull) && (now_ms - args->window->memory_poll_deadline_ms < GUI_MEMORY_POLL_MS)))
    {
        return;
    }
    char text[8];
    VerbaLinea arguments = {text, sizeof(text), 0ull};
    mmgr_line_put(&arguments, "{}");
    args->window->memory_poll_deadline_ms = now_ms;
    args->window->asked_memory = EMBED_CALL(window_call, GuiWindowCtx, .window = args->window, .own = 1,
                                            .name = "memory_usage", .request = &arguments);
}

void gui_window_drain(const GuiWindowCfg *args)
{
    (void)EMBED_CALL(gui_link_context.drain, GuiLinkContextCfg, .link = &args->window->link, .each = window_take,
                     .context = args->window);
    EMBED_CALL(window_memory_request, GuiWindowCtx, .window = args->window);
    EMBED_CALL(window_mark_tree_read, GuiWindowCtx, .window = args->window);
}

// the panels over the editor drawn inside the work's inside: the hover under its word, hidden where theword's line
// has left the rows; the completion under the prefix's first cell; and the signature over the cursor's row
EMBED_INLINE void window_panels_draw(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = frame->scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs, .size = size);
    GuiRect anchor;
    GuiRect last;
    if (args->window->hover.shown)
    {
        const unsigned long long end = (args->window->hover.to > args->window->hover.from)
                                           ? args->window->hover.to - 1ull
                                           : args->window->hover.from;
        if (EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = &args->window->editor,
                       .place = frame->editor, .cell = cell, .scale = frame->scale, .line = args->window->hover.line,
                       .col = args->window->hover.from, .rect = &anchor) &&
            EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = &args->window->editor,
                       .place = frame->editor, .cell = cell, .scale = frame->scale, .line = args->window->hover.line,
                       .col = end, .rect = &last))
        {
            EMBED_CALL(gui_hover_raster.draw, GuiHoverRasterCfg, .hover = &args->window->hover,
                       .raster = args->window->raster, .glyphs = &args->window->glyphs,
                       .palette = args->window->palette, .anchor = anchor, .last = last, .bounds = frame->inside,
                       .scale = frame->scale);
        }
        else
        {
            EMBED_CALL(gui_hover_state.hide, GuiHoverStateCfg, .hover = &args->window->hover);
        }
    }
    if (args->window->suggest.open)
    {
        if (EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = &args->window->editor,
                       .place = frame->editor, .cell = cell, .scale = frame->scale, .line = args->window->suggest.line,
                       .col = args->window->suggest.from, .rect = &anchor))
        {
            EMBED_CALL(gui_suggest_raster.draw, GuiSuggestRasterCfg, .suggest = &args->window->suggest,
                       .raster = args->window->raster, .glyphs = &args->window->glyphs,
                       .palette = args->window->palette, .anchor = anchor, .bounds = frame->inside,
                       .scale = frame->scale);
        }
        else
        {
            EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &args->window->suggest);
        }
    }
    if (args->window->signature.shown && args->window->editor.has_cursor)
    {
        if (EMBED_CALL(gui_editor_place_cell.place_rect, GuiEditorPlaceCellCfg, .editor_read = &args->window->editor,
                       .place = frame->editor, .cell = cell, .scale = frame->scale,
                       .line = args->window->editor.cursor.line, .col = args->window->editor.cursor.col,
                       .rect = &anchor))
        {
            EMBED_CALL(gui_signature_raster.draw, GuiSignatureRasterCfg, .signature = &args->window->signature,
                       .raster = args->window->raster, .glyphs = &args->window->glyphs,
                       .palette = args->window->palette, .anchor = anchor, .bounds = frame->inside,
                       .scale = frame->scale);
        }
        else
        {
            EMBED_CALL(gui_signature_json.hide, GuiSignatureJsonCfg, .signature = &args->window->signature);
        }
    }
}

// the view drawn: the tabs row, the editor with the map beside it, and the side where one is shown, each on the ground
// the base laid it, and the panels over them
EMBED_INLINE void window_view_draw(const GuiWindowCtx *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    EMBED_CALL(gui_tabs_glyphs_frame.draw, GuiTabsGlyphsFrameCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame, .tab = args->window->tab, .count = args->window->tabs,
               .active = args->window->active);
    EMBED_CALL(gui_editor_place_scale.draw, GuiEditorPlaceScaleCfg, .editor = &args->window->editor, .raster = raster,
               .glyphs = &args->window->glyphs, .palette = palette, .place = frame->editor, .scale = frame->scale);
    // the shown file's diagnostics underlined on the rows drawn
    const unsigned int code_size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                            .pixels = GUI_CODE_SIZE, .scale = frame->scale);
    EMBED_CALL(gui_problems_path_raster.underlined, GuiProblemsPathRasterCfg, .problems = &args->window->problems,
               .path = args->window->path, .editor = &args->window->editor, .raster = raster, .palette = palette,
               .place = frame->editor,
               .cell = EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs,
                                  .size = code_size),
               .scale = frame->scale);
    EMBED_CALL(gui_minimap_place.draw, GuiMinimapPlaceCfg, .map = &args->window->minimap, .raster = raster,
               .palette = palette, .editor = &args->window->editor, .place = frame->map, .scale = frame->scale,
               .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = args->window));
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = frame->side) &&
        (args->window->strip.shown >= 0))
    {
        // the tool shown is among the tools
        const GuiTool *const tool =
            EMBED_CALL(gui_strip.tool, GuiStripCfg, .at = (unsigned int)args->window->strip.shown);
        EMBED_CALL(gui_explorer_frame_xy.draw, GuiExplorerFrameXYCfg, .explorer_read = &args->window->explorer,
                   .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette,
                   .frame = frame, .title = tool->label,
                   .folders = cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = "explorer", .cap = 32ull}),
                   .tree = args->window->title, .shown = args->window->path);
        if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = args->window, .name = "problems"))
        {
            EMBED_CALL(gui_problems_row_frame.draw, GuiProblemsRowFrameCfg, .problems = &args->window->problems,
                       .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
        }
    }
    window_panels_draw(args);
}

void gui_window_draw(const GuiWindowCfg *args)
{
    const GuiFrame *const frame = &args->window->frame;
    const GuiPalette *const palette = args->window->palette;
    GuiRaster *const raster = args->window->raster;
    args->window->owed = 0;
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
    const GuiRaster *ground = &args->window->ground.raster;
    if ((ground->pixels == NULL) || (ground->width != raster->width) || (ground->height != raster->height))
    {
        ground = NULL;
    }
    const int view_shown = !args->window->wordmark.stands;
    // a stage of the claim's movement brings its map and its holes; only a frame at rest lays them
    const GuiStage *const stage = view_shown ? args->window->stage : NULL;
    if (stage != NULL)
    {
        EMBED_CALL(gui_base_frame.paint, GuiBaseFrameCfg, .base = &args->window->base, .palette = palette);
        EMBED_CALL(gui_base_raster.map_draw, GuiBaseRasterCfg, .base_read = &args->window->base, .map = &stage->map,
                   .raster = raster, .ground = ground, .color = palette->deep);
    }
    else
    {
        const GuiFrame *based = frame;
        GuiFrame bare;
        if (!view_shown)
        {
            bare = *frame;
            bare.side = (GuiRect){0, 0, 0, 0};
            bare.tabs = (GuiRect){0, 0, 0, 0};
            bare.editor = (GuiRect){0, 0, 0, 0};
            bare.map = (GuiRect){0, 0, 0, 0};
            based = &bare;
        }
        (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = &args->window->base, .frame = based,
                         .palette = palette, .controls_steady = frame->controls_pane_share >= GUI_WHOLE);
        EMBED_CALL(gui_base_raster.draw, GuiBaseRasterCfg, .base_read = &args->window->base, .raster = raster,
                   .ground = ground, .color = palette->deep);
    }
    // the table the panes lie on: the lattice with the sheet's tint and grain; each pane's corners rounded on it
    const GuiUnder table =
        EMBED_CALL(gui_glass.table, GuiGlassCfg, .ground = ground, .color = palette->deep, .glass = palette->glass);
    const GuiColor pane = EMBED_CALL(gui_glass.pane_tint, GuiGlassCfg, .glass = palette->glass);
    EMBED_CALL(
        gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = raster, .ground = table,
        .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->bar_slat, .other = frame->bar),
        .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = raster, .ground = table,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->strip_slat,
                                  .other = frame->strip),
               .radius = frame->pane_radius, .inner = pane, .corners_only = 1);
    EMBED_CALL(gui_raster_rect_radius_ground.pane, GuiRasterRectRadiusGroundCfg, .raster = raster, .ground = table,
               .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = frame->status_slat,
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
    const GuiUnder clear = {ground, 0u, NULL, 0u, palette->deep};
    GuiRect holes_laid[4];
    const unsigned int count =
        (stage != NULL) ? stage->hole_count
                        : EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = frame, .holes = holes_laid);
    EMBED_CALL(gui_raster_radius_work.outline, GuiRasterRadiusWorkCfg, .raster = raster, .work = frame->work,
               .inside = frame->inside, .radius = frame->radius, .lines = &lines,
               .holes = (stage != NULL) ? stage->holes : holes_laid, .count = count, .table = table, .clear = clear);
    // the work's inside: the tabs row, the editor with the map beside it, and the side where one is shown, each on
    // the ground the base laid it
    if (view_shown)
    {
        EMBED_CALL(window_view_draw, GuiWindowCtx, .window = args->window);
    }
    // the parts about the work, each on its pane: the strip, the bar with the menu bar's titles and the controls,
    // and the status bar
    EMBED_CALL(gui_strip_frame_raster.draw, GuiStripFrameRasterCfg, .raster = raster, .icons = &args->window->icons,
               .palette = palette, .frame = frame, .strip_read = &args->window->strip);
    EMBED_CALL(gui_chrome_raster.bar_draw, GuiChromeRasterCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .palette = palette, .frame = frame, .title = args->window->title, .table = table,
               .shadow = &args->window->controls_shadow);
    EMBED_CALL(gui_menubar_glyphs_frame.titles_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &args->window->menubar,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
    const GuiStatus status = {args->window->path,
                              args->window->editor.has_cursor,
                              args->window->editor.cursor.line,
                              args->window->editor.cursor.col,
                              args->window->editor.anchor.line,
                              args->window->editor.anchor.col,
                              (args->window->active >= 0) ? args->window->tab[args->window->active].crlf : 0,
                              args->window->editor.indent_tabs,
                              args->window->editor.indent_size,
                              args->window->memory_bytes};
    EMBED_CALL(gui_statusbar_status.draw, GuiStatusbarStatusCfg, .raster = raster, .glyphs = &args->window->glyphs,
               .icons = &args->window->icons, .palette = palette, .frame = frame, .status = &status);
    // the large name as it stands or goes, the funnel over the whole window, and the eye over the bar and the strip
    const unsigned long long drawn_ms = cli_clock_milliseconds();
    EMBED_CALL(gui_wordmark_frame.draw, GuiWordmarkFrameCfg, .wordmark = &args->window->wordmark,
               .raster = raster, .glyphs = &args->window->glyphs, .icons = &args->window->icons, .palette = palette,
               .frame = frame);
    EMBED_CALL(gui_wordmark_frame.eye_draw, GuiWordmarkFrameCfg, .wordmark_read = &args->window->wordmark,
               .raster = raster, .icons = &args->window->icons, .palette = palette, .frame = frame);
    // the menus open go over every part
    EMBED_CALL(gui_menubar_glyphs_frame.menus_draw, GuiMenubarGlyphsFrameCfg, .menubar_read = &args->window->menubar,
               .raster = raster, .glyphs = &args->window->glyphs, .palette = palette, .frame = frame);
    // the caret last, at the ink its breath stands at, where no panel or menu lies over it: drawn over the frame
    // whole, its breath's steps draw it again alone; none stands while the view is not drawn
    args->window->caret_ink = EMBED_CALL(window_caret_ink, GuiWindowCtx, .window_read = args->window);
    args->window->editor.caret_ink = args->window->caret_ink;
    const GuiRect caret =
        EMBED_CALL(gui_editor_place.caret_rect, GuiEditorPlaceCfg, .editor_read = &args->window->editor,
                   .glyphs = &args->window->glyphs, .place = frame->editor, .scale = frame->scale);
    if (view_shown && !EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = caret) &&
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
    EMBED_CALL(gui_busy_frame.draw, GuiBusyFrameCfg, .busy = &args->window->busy, .raster = raster,
               .palette = palette, .frame = frame);
}

unsigned int gui_window_hit(const GuiWindowInputCfg *args)
{
    const GuiFrame *const frame = &args->window_read->frame;
    const int border =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BORDER, .scale = frame->scale);
    // the window's sides are far inside what an int represents
    const int wide = (int)frame->width;
    const int tall = (int)frame->height;
    const int left = args->input->x < border;
    const int right = args->input->x >= wide - border;
    const int top = args->input->y < border;
    const int bottom = args->input->y >= tall - border;
    if (!args->window_read->maximized && (left || right || top || bottom))
    {
        if (top)
        {
            return left ? GUI_HIT_TOP_LEFT : (right ? GUI_HIT_TOP_RIGHT : GUI_HIT_TOP);
        }
        if (bottom)
        {
            return left ? GUI_HIT_BOTTOM_LEFT : (right ? GUI_HIT_BOTTOM_RIGHT : GUI_HIT_BOTTOM);
        }
        return left ? GUI_HIT_LEFT : GUI_HIT_RIGHT;
    }
    const unsigned int control =
        EMBED_CALL(gui_chrome.controls_hit, GuiChromeCfg, .frame = frame, .x = args->input->x, .y = args->input->y);
    if (control == GUI_CONTROL_MINIMIZE)
    {
        return GUI_HIT_MINIMIZE;
    }
    if (control == GUI_CONTROL_MAXIMIZE)
    {
        return GUI_HIT_MAXIMIZE;
    }
    if (control == GUI_CONTROL_CLOSE)
    {
        return GUI_HIT_CLOSE;
    }
    // the menu bar's titles and its open menus are the client's, not the caption's, and the eye in the bar is too
    if (EMBED_CALL(gui_menubar_x.holds, GuiMenubarXCfg, .menubar_read = &args->window_read->menubar,
                   .x = args->input->x, .y = args->input->y) ||
        (args->window_read->wordmark.home &&
         EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                    .rect = EMBED_CALL(gui_wordmark_frame.eye_square, GuiWordmarkFrameCfg, .frame = frame),
                    .x = args->input->x, .y = args->input->y)))
    {
        return GUI_HIT_CLIENT;
    }
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = frame->bar, .x = args->input->x,
                      .y = args->input->y)
               ? GUI_HIT_CAPTION
               : GUI_HIT_CLIENT;
}

// the view's top moved `delta` lines, no further than the file's first and last lines, and the view told
EMBED_INLINE void window_scroll(const GuiWindowCtx *args)
{
    long long delta = args->delta;
    EMBED_CALL(gui_editor_delta.scroll, GuiEditorDeltaCfg, .editor = &args->window->editor, .delta = delta,
               .rows = EMBED_CALL(window_rows, GuiWindowCtx, .window_read = args->window));
    args->window->owed = 1;
    char more[48];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"top\":");
    mmgr_line_decimal(&line, args->window->editor.top, 1u);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .kind = CLI_VIEW_SCROLL, .more = more);
}

// the view's top moved to `top`
EMBED_INLINE void window_scrolled_to(const GuiWindowCtx *args)
{
    const unsigned long long top_before = args->window->editor.top;
    // the lines between the tops are far inside what a long long represents
    EMBED_CALL(window_scroll, GuiWindowCtx, .window = args->window,
               .delta = (args->top >= top_before) ? (long long)(args->top - top_before) : -(long long)(top_before - args->top));
}

static void window_wheel(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // while the name's pane stands no view takes the wheel
    if (window->wordmark.stands)
    {
        return;
    }
    // the wheel over the side scrolls the explorer's rows; over the editor or the map, the text
    if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.side, .x = args->input->x,
                   .y = args->input->y))
    {
        const int moved =
            EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .name = "problems")
                ? EMBED_CALL(gui_problems_row_frame.wheel, GuiProblemsRowFrameCfg, .problems_out = &window->problems,
                             .frame = &window->frame, .rows = args->input->rows)
                : EMBED_CALL(gui_explorer_frame.wheel, GuiExplorerFrameCfg, .explorer = &window->explorer,
                             .frame = &window->frame, .rows = args->input->rows);
        window->owed = moved || window->owed;
        return;
    }
    // the wheel over the completion's list scrolls its rows
    if (EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &window->suggest, .x = args->input->x,
                   .y = args->input->y))
    {
        window->owed = EMBED_CALL(gui_suggest_rows.wheel, GuiSuggestRowsCfg, .suggest = &window->suggest,
                                  .rows = args->input->rows) ||
                       window->owed;
        return;
    }
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.editor, .x = args->input->x,
                    .y = args->input->y) &&
        !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.map, .x = args->input->x,
                    .y = args->input->y))
    {
        return;
    }
    EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
    EMBED_CALL(window_scroll, GuiWindowCtx, .window = window, .delta = args->input->rows);
}

// the pointer's event at x, y sent to the view where the pixel stands in the editor's text or its gutter: 1 where it
// was
static int window_point(const GuiWindowCtx *args)
{
    int x = args->x;
    int y = args->y;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = args->window->frame.scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &args->window->glyphs, .size = size);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    int gutter = 0;
    if (!args->window->viewed)
    {
        return 0;
    }
    if (!EMBED_CALL(gui_editor_place_cell_scale.cell_at, GuiEditorPlaceCellScaleCfg,
                    .editor_read = &args->window->editor, .place = args->window->frame.editor, .cell = cell,
                    .scale = args->window->frame.scale, .x = x, .y = y, .row = &row, .column = &column))
    {
        if (!EMBED_CALL(gui_editor_place_cell_scale.row_at, GuiEditorPlaceCellScaleCfg,
                        .editor_read = &args->window->editor, .place = args->window->frame.editor, .cell = cell,
                        .scale = args->window->frame.scale, .x = x, .y = y, .row = &row))
        {
            return 0;
        }
        gutter = 1;
    }
    char more[160];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"pointer\":");
    mmgr_line_decimal(&line, args->pointer, 1u);
    mmgr_line_put(&line, ",\"row\":");
    mmgr_line_decimal(&line, row, 1u);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, column, 1u);
    mmgr_line_put(&line, ",\"clicks\":");
    mmgr_line_decimal(&line, args->clicks, 1u);
    mmgr_line_put(&line, args->shift ? ",\"shift\":true" : ",\"shift\":false");
    mmgr_line_put(&line, gutter ? ",\"gutter\":true" : ",\"gutter\":false");
    (void)mmgr_line_finish(&line);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = args->window, .kind = CLI_VIEW_POINTER, .more = more);
    return 1;
}

static void window_press(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const GuiInput *input = args->input;
    const unsigned int hit = EMBED_CALL(cli_window_input.hit, GuiWindowInputCfg, .window_read = window, .input = input);
    window->pressed =
        ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE)) ? hit : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
    // a press on a completion's row accepts its item; a press in the hover's box or the signature's is theirs; a
    // press anywhere else hides the panels and goes on
    if (EMBED_CALL(gui_suggest_xy.press, GuiSuggestXYCfg, .suggest = &window->suggest, .x = input->x, .y = input->y) ==
        GUI_SUGGEST_ACCEPTED)
    {
        EMBED_CALL(window_complete, GuiWindowCtx, .window = window);
        return;
    }
    if (EMBED_CALL(gui_suggest_x.holds, GuiSuggestXCfg, .suggest_read = &window->suggest, .x = input->x,
                   .y = input->y) ||
        EMBED_CALL(gui_hover_x.holds, GuiHoverXCfg, .hover_read = &window->hover, .x = input->x, .y = input->y) ||
        EMBED_CALL(gui_signature_x.holds, GuiSignatureXCfg, .signature_read = &window->signature, .x = input->x,
                   .y = input->y))
    {
        return;
    }
    EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
    // the menu bar takes a press on a title or in a menu; a press outside its menus closes them and goes on; a menu
    // open takes the keys from the editor, which lets the claim go
    GuiChoice choice;
    const int menus_open = window->menubar.levels != 0u;
    if (EMBED_CALL(gui_menubar_json.press, GuiMenubarJsonCfg, .menubar = &window->menubar, .glyphs = &window->glyphs,
                   .frame = &window->frame, .x = input->x, .y = input->y, .choice = &choice))
    {
        window->owed = 1;
        if (window->menubar.levels != 0u)
        {
            EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        }
        if (choice.command[0] != '\0')
        {
            EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &choice);
        }
        return;
    }
    window->owed = window->owed || menus_open;
    // a press on the name opens orior's repository, and the rest of the name's pane takes a press to no view
    if (EMBED_CALL(gui_wordmark_frame.holds, GuiWordmarkFrameCfg, .wordmark_read = &window->wordmark,
                   .frame = &window->frame, .x = input->x, .y = input->y))
    {
        char text[64];
        VerbaLinea arguments = {text, sizeof(text), 0ull};
        mmgr_line_put(&arguments, "{\"url\":\"" GUI_WORDMARK_REPOSITORY "\"}");
        (void)EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 1, .name = "report_open",
                         .request = &arguments);
        return;
    }
    if (window->wordmark.stands && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = window->frame.work,
                                              .x = input->x, .y = input->y))
    {
        return;
    }
    // a press on the strip shows a tool's window or closes it, the keys going to it
    if (EMBED_CALL(gui_strip_frame_x.press, GuiStripFrameXCfg, .strip = &window->strip, .frame = &window->frame,
                   .x = input->x, .y = input->y))
    {
        EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        EMBED_CALL(window_side, GuiWindowCtx, .window = window);
        return;
    }
    // a press in the problems pane starts or stops the tree's check, closes or opens a file's rows, or opens a file
    // at a diagnostic's place
    GuiProblemPick problem;
    if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .name = "problems") &&
        EMBED_CALL(gui_problems_frame.press, GuiProblemsFrameCfg, .problems_out = &window->problems,
                   .frame = &window->frame, .x = input->x, .y = input->y, .pick = &problem))
    {
        window->owed = 1;
        EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        if ((problem.kind == GUI_PROBLEMS_PICK_CHECK) || (problem.kind == GUI_PROBLEMS_PICK_STOP))
        {
            char args_text[8];
            VerbaLinea arguments = {args_text, sizeof(args_text), 0ull};
            mmgr_line_put(&arguments, "{}");
            window->asked_check =
                EMBED_CALL(window_call, GuiWindowCtx, .window = window, .own = 0,
                           .name = (problem.kind == GUI_PROBLEMS_PICK_CHECK) ? "problems_check" : "problems_check_stop",
                           .request = &arguments);
        }
        else if (problem.kind == GUI_PROBLEMS_PICK_OPEN)
        {
            EMBED_CALL(window_opened_at, GuiWindowCtx, .window = window, .path = problem.path, .line = problem.line,
                       .col = problem.col);
        }
        return;
    }
    // a press in the side opens or closes a folder, asking for its listing the first time, or opens a file
    GuiPick pick;
    if (EMBED_CALL(gui_explorer_frame_x.press, GuiExplorerFrameXCfg, .explorer = &window->explorer,
                   .frame = &window->frame, .x = input->x, .y = input->y, .pick = &pick))
    {
        window->owed = 1;
        EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        if (pick.kind == GUI_PICK_LIST)
        {
            EMBED_CALL(window_listing_request, GuiWindowCtx, .window = window, .dir = pick.path);
        }
        else if (pick.kind == GUI_PICK_OPEN)
        {
            EMBED_CALL(gui_window_path.file_open, GuiWindowPathCfg, .window = window, .path = pick.path);
        }
        return;
    }
    // a press on a tab shows its file, on its close mark closes it
    unsigned int at = 0u;
    int closing = 0;
    if (EMBED_CALL(gui_tabs_glyphs_frame_count.hit, GuiTabsGlyphsFrameCountCfg, .glyphs = &window->glyphs,
                   .frame = &window->frame, .tab = window->tab, .count = window->tabs, .x = input->x, .y = input->y,
                   .at_pointer = &at, .closing = &closing))
    {
        if (closing)
        {
            EMBED_CALL(gui_window_at.tab_close, GuiWindowAtCfg, .window = window, .at = at);
        }
        else
        {
            EMBED_CALL(gui_window_at.tab_show, GuiWindowAtCfg, .window = window, .at = at);
        }
        return;
    }
    // a press on the map goes to that part of the text and may begin the slider's drag
    unsigned long long top = 0ull;
    if (EMBED_CALL(gui_minimap_place_scale.press, GuiMinimapPlaceScaleCfg, .map = &window->minimap,
                   .place = window->frame.map, .scale = window->frame.scale, .editor = &window->editor,
                   .on_screen = EMBED_CALL(window_on_screen, GuiWindowCtx, .window_read = window), .x = input->x,
                   .y = input->y, .top = &top))
    {
        EMBED_CALL(window_scrolled_to, GuiWindowCtx, .window = window, .top = top);
        return;
    }
    // presses in a row within the time and near the place count up
    const unsigned long long now_ms = cli_clock_milliseconds();
    const int near =
        ((input->x - window->clicked_x) <= GUI_CLICK_NEAR) && ((window->clicked_x - input->x) <= GUI_CLICK_NEAR) &&
        ((input->y - window->clicked_y) <= GUI_CLICK_NEAR) && ((window->clicked_y - input->y) <= GUI_CLICK_NEAR);
    window->clicks = ((now_ms - window->double_click_deadline_ms) <= GUI_DOUBLE_CLICK_MS) && near ? window->clicks + 1u : 1u;
    window->double_click_deadline_ms = now_ms;
    window->clicked_x = input->x;
    window->clicked_y = input->y;
    window->dragging =
        EMBED_CALL(window_point, GuiWindowCtx, .window = window, .pointer = CLI_VIEW_POINTER_DOWN, .x = input->x,
                   .y = input->y, .clicks = window->clicks, .shift = (input->mods & GUI_MOD_SHIFT) != 0u);
    // a press in the editor's text gives it the keys
    if (window->dragging)
    {
        EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 1);
    }
}

static void window_move(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    if (window->claim.claimed)
    {
        const GuiFrame *const frame = &window->frame;
        const unsigned long long now_ms = cli_clock_milliseconds();
        const int near_before = window->claim.near;
        EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &window->claim, .x = args->input->x,
                   .y = args->input->y,
                   .zone = EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = frame->width,
                                      .height = frame->height, .scale = frame->scale,
                                      .controls_wide = frame->controls.width, .x = args->input->x, .y = args->input->y),
                   .over_parts = EMBED_CALL(window_over_parts, GuiWindowCtx, .window_read = window, .x = args->input->x,
                                            .y = args->input->y));
        if ((window->claim.near != near_before) ||
            EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &window->claim, .time_ms = now_ms))
        {
            window->claim_moving = 1;
            EMBED_CALL(window_claim_layout, GuiWindowCtx, .window = window);
        }
    }
    if (EMBED_CALL(gui_menubar_glyphs_frame_x.move, GuiMenubarGlyphsFrameXCfg, .menubar = &window->menubar,
                   .glyphs = &window->glyphs, .frame = &window->frame, .x = args->input->x, .y = args->input->y))
    {
        window->owed = 1;
    }
    if (EMBED_CALL(gui_strip_frame_x.move, GuiStripFrameXCfg, .strip = &window->strip, .frame = &window->frame,
                   .x = args->input->x, .y = args->input->y))
    {
        window->owed = 1;
    }
    if (EMBED_CALL(window_tool_by_name, GuiWindowCtx, .window_read = window, .name = "problems")
            ? EMBED_CALL(gui_problems_frame.move, GuiProblemsFrameCfg, .problems_out = &window->problems,
                         .frame = &window->frame, .x = args->input->x, .y = args->input->y)
            : EMBED_CALL(gui_explorer_frame_x.move, GuiExplorerFrameXCfg, .explorer = &window->explorer,
                         .frame = &window->frame, .x = args->input->x, .y = args->input->y))
    {
        window->owed = 1;
    }
    unsigned long long top = 0ull;
    if (EMBED_CALL(gui_minimap_place_scale.drag, GuiMinimapPlaceScaleCfg, .map = &window->minimap, .y = args->input->y,
                   .top = &top))
    {
        if (top != window->editor.top)
        {
            EMBED_CALL(window_scrolled_to, GuiWindowCtx, .window = window, .top = top);
        }
        return;
    }
    if (window->dragging)
    {
        (void)EMBED_CALL(window_point, GuiWindowCtx, .window = window, .pointer = CLI_VIEW_POINTER_MOVED,
                         .x = args->input->x, .y = args->input->y, .clicks = window->clicks, .shift = 0);
        return;
    }
    // the pointer over the editor's text begins the hover's rest at its cell, where no menu is open and no drag runs
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_CODE_SIZE, .scale = window->frame.scale);
    const float cell =
        EMBED_CALL(gui_glyphs_face_letter.cell, GuiGlyphsFaceLetterCfg, .glyphs = &window->glyphs, .size = size);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    const int in_text =
        window->viewed && (window->menubar.levels == 0u) &&
        EMBED_CALL(gui_editor_place_cell_scale.cell_at, GuiEditorPlaceCellScaleCfg, .editor_read = &window->editor,
                   .place = window->frame.editor, .cell = cell, .scale = window->frame.scale, .x = args->input->x,
                   .y = args->input->y, .row = &row, .column = &column);
    EMBED_CALL(gui_hover_in.move, GuiHoverInCfg, .hover = &window->hover, .in_text = in_text, .row = row,
               .column = column, .x = args->input->x, .y = args->input->y);
}

static void window_leave(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    const int near_before = window->claim.near;
    EMBED_CALL(cli_claim_still.leave, GuiClaimStillCfg, .claim = &window->claim);
    if (window->claim.near != near_before)
    {
        window->claim_moving = 1;
        EMBED_CALL(window_claim_layout, GuiWindowCtx, .window = window);
    }
}

static void window_release(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    if (window->minimap.dragging)
    {
        EMBED_CALL(gui_minimap_json.release, GuiMinimapJsonCfg, .map = &window->minimap);
        window->owed = 1;
    }
    if (window->dragging)
    {
        window->dragging = 0;
        (void)EMBED_CALL(window_point, GuiWindowCtx, .window = window, .pointer = CLI_VIEW_POINTER_UP,
                         .x = args->input->x, .y = args->input->y, .clicks = window->clicks, .shift = 0);
    }
}

static void window_key(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // the menu bar takes the keys while a menu is open, and Alt with a title's letter; a menu open takes the keys from
    // the editor, which lets the claim go
    GuiChoice choice;
    if (EMBED_CALL(gui_menubar_glyphs.key, GuiMenubarGlyphsCfg, .menubar = &window->menubar, .glyphs = &window->glyphs,
                   .frame = &window->frame, .given = args->input->name, .choice = &choice))
    {
        window->owed = 1;
        if (window->menubar.levels != 0u)
        {
            EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        }
        if (choice.command[0] != '\0')
        {
            EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &choice);
        }
        return;
    }
    // while the name's pane stands no view takes the keys
    if (window->wordmark.stands)
    {
        return;
    }
    // the keys of a command the window runs itself run it, a tool's keys show its window; every other key is the
    // view's, which takes the keys and makes the claim
    if (EMBED_CALL(gui_menubar_choice.keys_command, GuiMenubarChoiceCfg, .menubar_read = &window->menubar,
                   .name = args->input->name, .choice = &choice) &&
        EMBED_CALL(window_runs, GuiWindowCtx, .command = choice.command))
    {
        EMBED_CALL(gui_window_table.choose, GuiWindowCfg, .window = window, .choice = &choice);
        return;
    }
    if (EMBED_CALL(gui_strip_name.key, GuiStripNameCfg, .strip = &window->strip, .name = args->input->name))
    {
        EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 0);
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
        EMBED_CALL(window_side, GuiWindowCtx, .window = window);
        return;
    }
    // the completion's list takes its keys while it is open; Escape closes the signature and the hover; Mod+Space
    // opens the list with nothing typed
    const unsigned int did =
        EMBED_CALL(gui_suggest_json.key, GuiSuggestJsonCfg, .suggest = &window->suggest, .name = args->input->name);
    if (did == GUI_SUGGEST_ACCEPTED)
    {
        EMBED_CALL(window_complete, GuiWindowCtx, .window = window);
        return;
    }
    if (did != GUI_SUGGEST_NONE)
    {
        window->owed = 1;
        return;
    }
    const int escape = cellul.eq(&(CatenaFinitaCfg){.src = args->input->name, .other = "Escape", .cap = 64ull});
    if (escape && (window->signature.shown || window->hover.shown))
    {
        EMBED_CALL(window_panels_hide, GuiWindowCtx, .window = window);
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->input->name, .other = "Mod+Space", .cap = 64ull}))
    {
        window->complete_forced = 1;
        EMBED_CALL(window_complete_request, GuiWindowCtx, .window = window, .server = 0);
        return;
    }
    EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 1);
    EMBED_CALL(gui_hover_state.hide, GuiHoverStateCfg, .hover = &window->hover);
    // a key that takes letters out saves the list open on the prefix left; any other key closes it
    if (window->suggest.open)
    {
        const int edits = cellul.eq(&(CatenaFinitaCfg){.src = args->input->name, .other = "Backspace", .cap = 64ull}) ||
                          cellul.eq(&(CatenaFinitaCfg){.src = args->input->name, .other = "Delete", .cap = 64ull});
        window->complete_wanted = edits;
        if (!edits)
        {
            EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &window->suggest);
        }
    }
    // a key may edit the text, which the map's lines then no longer show and the server is told
    window->minimap.stale = 1;
    window->told_wanted = 1;
    char more[96];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->input->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->input->name, .cap = 64ull}));
    (void)mmgr_line_finish(&line);
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .kind = CLI_VIEW_KEY, .more = more);
}

static void window_text(const GuiInputHandlerCfg *args)
{
    GuiWindow *window = args->window;
    // while the name's pane stands no view takes the text
    if (window->wordmark.stands)
    {
        return;
    }
    char more[CLI_PATH_MAX_LEN];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"text\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = args->input->text,
               .length = args->input->size);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    EMBED_CALL(window_claim, GuiWindowCtx, .window = window, .on = 1);
    EMBED_CALL(gui_hover_state.hide, GuiHoverStateCfg, .hover = &window->hover);
    // a word's letter typed opens the completion's list or filters it as the frame comes; any other letter closes
    // it; a bracket or a comma begins the signature's wait
    if ((args->input->size > 0ull) &&
        EMBED_CALL(window_word_byte, GuiWindowCtx, .byte = args->input->text[args->input->size - 1ull]))
    {
        window->complete_wanted = 1;
    }
    else if (window->suggest.open)
    {
        EMBED_CALL(gui_suggest_json.close, GuiSuggestJsonCfg, .suggest = &window->suggest);
        window->complete_wanted = 0;
    }
    (void)EMBED_CALL(gui_signature_text.type, GuiSignatureTextCfg, .signature = &window->signature,
                     .text = args->input->text, .size = args->input->size);
    window->minimap.stale = 1;
    window->told_wanted = 1;
    EMBED_CALL(window_view_input, GuiWindowCtx, .window = window, .kind = CLI_VIEW_TEXT, .more = more);
}

// what answers each kind of input, in the main window and in Preferences' window
static const GuiInputHandlers s_window_handlers = {{[GUI_INPUT_MOVED] = window_move,
                                                    [GUI_INPUT_DOWN] = window_press,
                                                    [GUI_INPUT_UP] = window_release,
                                                    [GUI_INPUT_LEFT] = window_leave,
                                                    [GUI_INPUT_WHEEL] = window_wheel,
                                                    [GUI_INPUT_KEY] = window_key,
                                                    [GUI_INPUT_TEXT] = window_text}};
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
