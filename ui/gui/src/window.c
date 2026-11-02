// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the window apart from the system. A file opens through branch_open, a text read whole and a file too large
// to read whole alike, and is shown through the CLI's editor view, which answers each input with the frame drawn; the
// walk's colors come as branch-colors events. The minimap's lines are asked of editor_view_map for the span the map
// stands for, after a frame moves the span or a key, text or the walk's colors make the lines stored stale, one ask in
// flight at a time. What the tree keeps, its tabs and the one shown and the explorer's group, is read from orior's
// folder as the root comes, under the keys the page keeps them by, and written back as they change
#include "window.h"

#include "../../cli/src/editor/view.h"
#include "../../cli/src/text/text.h"
#include "answer.h"
#include "parts/chrome.h"
#include "system.h"

#include <math.h>

// the head of the key a file opened is stored under; the keys orior's folder keeps a tree's session and the explorer's
// group under, the tree's root after the session's; and the bytes the session is written in
#define GUI_OPEN_KEY_HEAD "window:"
#define GUI_KEPT_SESSION "orior.session."
#define GUI_KEPT_GROUP "orior.panes.group"
#define GUI_KEPT_MORE 256ull

int gui_window_begun(GuiWindow *window, GuiLinkWoken woken, void *context, VerbaLinea *err_msg)
{
    memor.set(&(MemoriaCfg){.dst = window, .val = 0, .bytes = sizeof(*window)});
    window->palette = gui_palette(GUI_SCHEME_DARK);
    window->raster = &window->framebuffer.raster[0];
    window->active = -1;
    window->last_active = -1;
    gui_editor_made(&window->editor);
    gui_menubar_made(&window->menubar);
    gui_strip_made(&window->strip);
    gui_icons_made(&window->icons);
    if (!gui_explorer_made(&window->explorer))
    {
        mmgr_line_put(err_msg, "the explorer's cells could not be taken");
        return 0;
    }
    if (!gui_minimap_made(&window->minimap))
    {
        gui_explorer_given(&window->explorer);
        mmgr_line_put(err_msg, "the minimap's cells could not be taken");
        return 0;
    }
    gui_claim_made(&window->claim);
    gui_wordmark_made(&window->wordmark);
    gui_busy_made(&window->busy);
    gui_hover_made(&window->hover);
    gui_signature_made(&window->signature);
    if (!gui_suggest_made(&window->suggest))
    {
        gui_explorer_given(&window->explorer);
        gui_minimap_given(&window->minimap);
        mmgr_line_put(err_msg, "the completion's cells could not be taken");
        return 0;
    }
    if (!gui_problems_made(&window->problems))
    {
        gui_suggest_given(&window->suggest);
        gui_explorer_given(&window->explorer);
        gui_minimap_given(&window->minimap);
        mmgr_line_put(err_msg, "the problems' cells could not be taken");
        return 0;
    }
    if (!gui_ground_made(&window->ground, cli_clock_milliseconds()))
    {
        gui_problems_given(&window->problems);
        gui_suggest_given(&window->suggest);
        gui_explorer_given(&window->explorer);
        gui_minimap_given(&window->minimap);
        mmgr_line_put(err_msg, "the lattice has no slot left for the ground");
        return 0;
    }
    if (!gui_glyphs_made(&window->glyphs, err_msg))
    {
        return 0;
    }
    if (!gui_link_begun(&window->link, woken, context))
    {
        gui_glyphs_given(&window->glyphs);
        mmgr_line_put(err_msg, "the link to orior_cli could not begin");
        return 0;
    }
    window->pressed = GUI_HIT_CLIENT;
    return 1;
}

static void window_session_written(GuiWindow *window, int ended);

void gui_window_ended(GuiWindow *window)
{
    gui_link_ended(&window->link);
    window_session_written(window, 1);
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        g_cli_cells->persistent_buf_release(window->tab[at].names, &g_gui_cells);
        window->tab[at].names = NULL;
    }
    gui_editor_given(&window->editor);
    if (window->cataloged)
    {
        cli_json_given(&window->catalog);
        window->cataloged = 0;
    }
    gui_glyphs_given(&window->glyphs);
    gui_icons_given(&window->icons);
    gui_wordmark_given(&window->wordmark);
    gui_busy_given(&window->busy);
    gui_controls_shadow_given(&window->controls_shadow);
    gui_explorer_given(&window->explorer);
    gui_minimap_given(&window->minimap);
    gui_hover_given(&window->hover);
    gui_suggest_given(&window->suggest);
    gui_signature_given(&window->signature);
    gui_problems_given(&window->problems);
    gui_ground_given(&window->ground);
    gui_stages_given(&window->stages);
    window->stage = NULL;
    gui_base_given(&window->base);
    gui_framebuffer_given(&window->framebuffer);
}

// whether the strip's tool shown is named `name`
static int window_tool_named(const GuiWindow *window, const char *name)
{
    // the tool shown is among the tools
    return (window->strip.shown >= 0) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = gui_strip_tool((unsigned int)window->strip.shown)->name, .other = name, .cap = 32ull});
}

// the rows the editor shows, and the columns
static unsigned int window_rows(const GuiWindow *window)
{
    const int line_tall = gui_scaled(GUI_CODE_LINE, window->frame.scale);
    const int rows = (line_tall > 0) ? window->frame.editor.height / line_tall : 0;
    // rows are counted where there is room for them
    return (rows > 0) ? (unsigned int)rows : 0u;
}

static unsigned int window_columns(GuiWindow *window)
{
    const unsigned int size = (unsigned int)(((float)GUI_CODE_SIZE * window->frame.scale) + 0.5f);
    const float cell = gui_glyphs_cell(&window->glyphs, size);
    const int gutter = gui_editor_gutter(&window->editor, cell, window->frame.scale);
    const float wide = (float)(window->frame.editor.width - gutter);
    // columns are counted where there is room for them
    return ((cell > 0.0f) && (wide > 0.0f)) ? (unsigned int)(wide / cell) : 0u;
}

// the lines the editor shows, with the fraction of the last
static float window_on_screen(const GuiWindow *window)
{
    const int line_tall = gui_scaled(GUI_CODE_LINE, window->frame.scale);
    return (line_tall > 0) ? (float)window->frame.editor.height / (float)line_tall : 0.0f;
}

// a call of the window's own or the tree's made with `args`: its ticket
static unsigned long long window_called(GuiWindow *window, int own, const char *name, const VerbaLinea *args)
{
    const unsigned long long ticket = gui_link_called(&window->link, own, name, args->out, mmgr_line_ok(args) ? args->at : 0ull);
    if (ticket != 0ull)
    {
        gui_busy_asked(&window->busy, ticket, cli_clock_milliseconds());
    }
    return ticket;
}

_Static_assert(GUI_BUSY_ASKS > ((2u * GUI_LINK_PENDING) + GUI_LINK_WORKERS + GUI_LINK_DONE), "the circle keeps every ask the link can stand unanswered");

// an input of the view's sent, `{"key", "kind", ...}` with `more` after the kind, its own members written already
static void window_view_input(GuiWindow *window, unsigned int kind, const char *more)
{
    if (!window->viewed)
    {
        return;
    }
    char text[CLI_PATH_MAX_LEN + 512u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"key\":");
    cli_json_quoted(&args, window->key, cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)}));
    mmgr_line_put(&args, ",\"kind\":");
    mmgr_line_decimal(&args, kind, 1u);
    mmgr_line_put(&args, more);
    mmgr_line_ch(&args, '}');
    (void)window_called(window, 1, "editor_view_input", &args);
}

// the panels over the editor hidden: the hover, the completion and the signature, and no completion wanted
static void window_panels_hidden(GuiWindow *window)
{
    const int owed = window->hover.shown || window->suggest.open || window->signature.shown;
    gui_hover_hidden(&window->hover);
    gui_suggest_closed(&window->suggest);
    gui_signature_hidden(&window->signature);
    window->complete_wanted = 0;
    window->complete_forced = 0;
    window->owed = window->owed || owed;
}

// the shown view's key written as the one argument of a call, `{"key": ...` with `more` after it
static void window_key_args(const GuiWindow *window, VerbaLinea *args, const char *more)
{
    mmgr_line_put(args, "{\"key\":");
    cli_json_quoted(args, window->key, cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)}));
    mmgr_line_put(args, more);
    mmgr_line_ch(args, '}');
}

// whether the shown tab's view is served by its language server
static int window_served(const GuiWindow *window)
{
    return (window->active >= 0) && window->tab[window->active].served;
}

// the hover asked for the word at the cell `column` of the row `row` of the shown view: without the server where
// `server` is 0, the tree's parts coming at once, the ask made again with the server as that answer comes
static void window_hover_asked(GuiWindow *window, unsigned long long row, unsigned long long column, int server)
{
    if (!window->viewed)
    {
        return;
    }
    char more[128];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"row\":");
    mmgr_line_decimal(&line, row, 1u);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, column, 1u);
    mmgr_line_put(&line, server ? ",\"server\":true" : ",\"server\":false");
    (void)mmgr_line_finish(&line);
    char text[CLI_PATH_MAX_LEN + 160u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    window_key_args(window, &args, more);
    // the panels' asks wait on the tree and the language server: they run on the pool, off the inputs' lane
    window->asked_hover = window_called(window, 0, "editor_view_hover", &args);
    window->hover_again = !server && window_served(window);
}

// the completions asked for the word before the shown view's cursor, one ask in flight at a time: the file's words
// first, without the server, and the server's items in an ask made again as that answer comes
static void window_complete_asked(GuiWindow *window, int server)
{
    window->complete_wanted = 0;
    if (!window->viewed || (window->asked_complete != 0ull))
    {
        window->complete_wanted = window->viewed;
        return;
    }
    char text[CLI_PATH_MAX_LEN + 48u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    window_key_args(window, &args, server ? ",\"server\":true" : ",\"server\":false");
    window->asked_complete = window_called(window, 0, "editor_view_complete", &args);
    window->complete_again = !server && window_served(window);
}

// the signature asked for the call the shown view's cursor is in, the cursor's place recorded as the one asked
// about: the tree's first, without the server, and the server's in an ask made again as that answer comes
static void window_signature_asked(GuiWindow *window, int server)
{
    if (!window->viewed)
    {
        return;
    }
    char text[CLI_PATH_MAX_LEN + 48u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    window_key_args(window, &args, server ? ",\"server\":true" : ",\"server\":false");
    window->signature_place = window->editor.cursor;
    window->asked_signature = window_called(window, 0, "editor_view_signature", &args);
    window->signature_again = !server && window_served(window);
}

// the tab `at`'s view handed to its file's language server, asked once as the view opens
static void window_serve_asked(GuiWindow *window, unsigned int at)
{
    GuiTab *const tab = &window->tab[at];
    if (tab->serve_asked)
    {
        return;
    }
    tab->serve_asked = 1;
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"key\":");
    cli_json_quoted(&args, tab->key, cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
    mmgr_line_ch(&args, '}');
    // the server's start waits on the server: it runs on the pool, off the inputs' lane
    tab->asked_serve = window_called(window, 0, "editor_view_serve", &args);
}

// the server told the view the shown tab's rows rest at, its lines from the top to the last row's, where the tab is
// served: the server's slice is cut for the view, as the page told it through the same call
static void window_view_told(GuiWindow *window)
{
    window->view_moved_ms = 0ull;
    if (!window->viewed || (window->active < 0) || !window->tab[window->active].served)
    {
        return;
    }
    const unsigned long long top = window->editor.top;
    const unsigned int rows = window_rows(window);
    unsigned long long last = top + ((rows > 0u) ? rows - 1u : 0u);
    last = ((window->editor.lines > 0ull) && (last >= window->editor.lines)) ? window->editor.lines - 1ull : last;
    char text[(2u * CLI_PATH_MAX_LEN) + 96u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"path\":");
    cli_json_quoted(&args, window->path,
                    cellul.len(&(CatenaFinitaCfg){.src = window->path, .cap = sizeof(window->path)}));
    mmgr_line_put(&args, ",\"key\":");
    cli_json_quoted(&args, window->key, cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)}));
    mmgr_line_put(&args, ",\"first\":");
    mmgr_line_decimal(&args, top, 1u);
    mmgr_line_put(&args, ",\"last\":");
    mmgr_line_decimal(&args, last, 1u);
    mmgr_line_ch(&args, '}');
    (void)window_called(window, 0, "lsp_view", &args);
    window->view_told = 1;
    window->view_told_top = top;
    window->view_told_rows = rows;
}

// the view's rows moved: the server is told where they rest after GUI_VIEW_RESTS
static void window_view_moved(GuiWindow *window)
{
    if (!window->view_told || (window->view_told_top != window->editor.top) ||
        (window->view_told_rows != window_rows(window)))
    {
        window->view_moved_ms = cli_clock_milliseconds();
    }
}

// the shown view's cursor put at `line`, `col`, the view following it
static void window_placed(GuiWindow *window, unsigned long long line, unsigned long long col)
{
    char more[64];
    VerbaLinea text = {more, sizeof(more), 0ull};
    mmgr_line_put(&text, ",\"row\":");
    mmgr_line_decimal(&text, line, 1u);
    mmgr_line_put(&text, ",\"column\":");
    mmgr_line_decimal(&text, col, 1u);
    (void)mmgr_line_finish(&text);
    window_view_input(window, CLI_VIEW_PLACE, more);
}

// the file at `path` opened as a tab shown with its cursor at `line`, `col`: at once where its view is shown,
// else as its view's next frame comes
static void window_opened_at(GuiWindow *window, const char *path, unsigned long long line, unsigned long long col)
{
    window->place_wanted = (GuiPlace){line, col};
    window->place_pending = 1;
    gui_window_file_opened(window, path);
    if (window->viewed && (window->active >= 0) &&
        cellul.eq(&(CatenaFinitaCfg){.src = window->tab[window->active].path, .other = path, .cap = CLI_PATH_MAX_LEN}))
    {
        window->place_pending = 0;
        window_placed(window, line, col);
    }
}

// the tab whose serve is in flight under `ticket`, or -1
static int window_tab_by_serve(const GuiWindow *window, unsigned long long ticket)
{
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        if (window->tab[at].asked_serve == ticket)
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the completion chosen accepted: its text written over the prefix before the cursor, the list closed
static void window_completed(GuiWindow *window)
{
    unsigned long long length = 0ull;
    const char *const insert = gui_suggest_chosen(&window->suggest, &length);
    if (insert == NULL)
    {
        gui_suggest_closed(&window->suggest);
        return;
    }
    const GuiPlace cursor = window->editor.cursor;
    const unsigned long long back =
        ((cursor.line == window->suggest.line) && (cursor.col >= window->suggest.from))
            ? cursor.col - window->suggest.from
            : cellul.len(&(CatenaFinitaCfg){.src = window->suggest.prefix, .cap = sizeof(window->suggest.prefix)});
    char more[CLI_PATH_MAX_LEN];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"text\":");
    cli_json_quoted(&line, insert, length);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, back, 1u);
    const int fits = mmgr_line_finish(&line) != 0ull;
    gui_suggest_closed(&window->suggest);
    window->owed = 1;
    if (fits)
    {
        window->minimap.stale = 1;
        window->told_wanted = 1;
        window_view_input(window, CLI_VIEW_COMPLETE, more);
    }
}

// whether the completion answer `root` of `json` is for the prefix before the cursor as it stands: its `from` on the
// cursor's line, the prefix's units ending at the cursor
static int window_complete_current(const GuiWindow *window, const CliJson *json, unsigned int root)
{
    const unsigned int from = cli_json_member(json, root, "from");
    unsigned long long length = 0ull;
    const char *const prefix = cli_json_string(json, cli_json_member(json, root, "prefix"), &length);
    if (!window->editor.has_cursor || (prefix == NULL) || !cli_json_is(json, from, CFG_JSON_OBJECT))
    {
        return 0;
    }
    const unsigned long long line = gui_answer_member_whole(json, from, "line", ~0ull);
    const unsigned long long col = gui_answer_member_whole(json, from, "col", ~0ull);
    return (line == window->editor.cursor.line) && (col + cli_text_units(prefix, length) == window->editor.cursor.col);
}

// whether a byte of UTF-8 typed is part of a word: a letter, a digit, `_`, `$`, or any byte of a letter past ASCII
static int window_word_byte(char byte)
{
    const unsigned char value = (unsigned char)byte;
    return ((value >= 'a') && (value <= 'z')) || ((value >= 'A') && (value <= 'Z')) ||
           ((value >= '0') && (value <= '9')) || (value == '_') || (value == '$') || (value >= 0x80u);
}

// the map's lines asked for where the map placed beside the view stands for lines it does not hold or holds stale,
// one ask in flight at a time, the next wanted as the answer comes
static void window_map_asked(GuiWindow *window)
{
    if (!window->viewed || gui_rect_empty(window->frame.map))
    {
        return;
    }
    gui_minimap_placed(&window->minimap, window->frame.map, window->frame.scale, &window->editor,
                       window_on_screen(window));
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if (!gui_minimap_span(&window->minimap, &first, &last) || gui_minimap_current(&window->minimap, first, last))
    {
        return;
    }
    if (window->asked_map != 0ull)
    {
        window->map_wanted = 1;
        return;
    }
    char text[CLI_PATH_MAX_LEN + 96u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"key\":");
    cli_json_quoted(&args, window->key, cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)}));
    mmgr_line_put(&args, ",\"first\":");
    mmgr_line_decimal(&args, first, 1u);
    mmgr_line_put(&args, ",\"last\":");
    mmgr_line_decimal(&args, last, 1u);
    mmgr_line_ch(&args, '}');
    window->asked_map = window_called(window, 1, "editor_view_map", &args);
}

// the view told the editor's size
static void window_view_sized(GuiWindow *window)
{
    char more[64];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"rows\":");
    mmgr_line_decimal(&line, window_rows(window), 1u);
    mmgr_line_put(&line, ",\"columns\":");
    mmgr_line_decimal(&line, window_columns(window), 1u);
    (void)mmgr_line_finish(&line);
    window_view_input(window, CLI_VIEW_SIZE, more);
}

void gui_window_claim_laid(GuiWindow *window, unsigned long long now)
{
    window->owed = 1;
    if (gui_claim_moving(&window->claim, now))
    {
        if ((window->stages.stage == NULL) || (window->stages.made_for != window->claim.sent) || window->stages_stale)
        {
            if (window->stage != NULL)
            {
                gui_stage_claim_taken(window->stage, &window->claim);
            }
            window->stage = NULL;
            window->stages_stale = 0;
            (void)gui_stages_made(&window->stages, &window->claim, &window->frame, &window->base, window->palette, &window->menubar, &window->glyphs, now);
        }
        const GuiStage *const stage = gui_stages_at(&window->stages, now);
        if (stage != NULL)
        {
            window->stage = stage;
            window->frame = stage->frame;
            gui_menubar_titles_taken(&window->menubar, stage->title, stage->titles, stage->folded_from, &window->frame);
            return;
        }
    }
    // at rest, or the stages' cell not taken: the frame the numbers lay now, and no stage left standing
    window->stage = NULL;
    window->stages_stale = 0;
    gui_stages_given(&window->stages);
    GuiClaimNumbers numbers;
    gui_claim_numbers(&window->claim, now, &numbers);
    gui_frame_claimed(&window->frame, &numbers);
    gui_menubar_titles_laid(&window->menubar, &window->glyphs, &window->frame);
}

static void window_claim_laid(GuiWindow *window)
{
    gui_window_claim_laid(window, cli_clock_milliseconds());
}

// the claim made, as the editor takes the keys while a file is shown, or let go, as they leave it
static void window_claimed(GuiWindow *window, int on)
{
    if ((on && !window->viewed) || (on == window->claim.claimed))
    {
        return;
    }
    gui_claim_set(&window->claim, on, cli_clock_milliseconds());
    window->claim_moving = 1;
    window_claim_laid(window);
}

// whether the pointer at x, y stands over the part the claim shows
static int window_over_parts(const GuiWindow *window, int x, int y)
{
    const GuiFrame *const frame = &window->frame;
    const unsigned int shown = window->claim.shown;
    if (shown == GUI_CLAIM_TOP)
    {
        return gui_rect_holds(gui_rect_met(frame->bar_slat, frame->bar), x, y) || gui_rect_holds(frame->tabs, x, y) ||
               gui_rect_holds(frame->controls_pane, x, y);
    }
    if (shown == GUI_CLAIM_BOTTOM)
    {
        return gui_rect_holds(gui_rect_met(frame->status_slat, frame->status), x, y);
    }
    if (shown == GUI_CLAIM_LEFT)
    {
        return gui_rect_holds(gui_rect_met(frame->strip_slat, frame->strip), x, y);
    }
    if (shown == GUI_CLAIM_RIGHT)
    {
        return gui_rect_holds(frame->map, x, y);
    }
    if (shown == GUI_CLAIM_CONTROLS)
    {
        return gui_rect_holds(frame->controls_pane, x, y);
    }
    return 0;
}

// whether the strip's tool shown has the side pane as its window
static int window_side_shown(const GuiWindow *window)
{
    // the tool shown is among the tools
    return (window->strip.shown >= 0) && gui_strip_tool((unsigned int)window->strip.shown)->side;
}

// the folder at `dir`, the top folder where empty, asked of the tree for its listing, the explorer told the ticket
static void window_listing_asked(GuiWindow *window, const char *dir)
{
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"dir\":");
    cli_json_quoted(&args, dir, cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_PATH_MAX_LEN}));
    mmgr_line_ch(&args, '}');
    const unsigned long long ticket = window_called(window, 0, "tree_list", &args);
    if (ticket != 0ull)
    {
        gui_explorer_listing(&window->explorer, dir, ticket);
    }
}

// the tab whose open is in flight under `ticket`, or -1
static int window_tab_by_ticket(const GuiWindow *window, unsigned long long ticket)
{
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        if (window->tab[at].asked_open == ticket)
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the tab whose key is `length` bytes of `key`, or -1
static int window_tab_by_key(const GuiWindow *window, const char *key, unsigned long long length)
{
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        const GuiTab *const tab = &window->tab[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = tab->key, .other = key, .bytes = length}) == 0))
        {
            // the tabs are fewer than an int counts
            return (int)at;
        }
    }
    return -1;
}

// the JSON text of the array `names` of `json`, the grammar's token names, stored in the tab `at`, replacing the
// text stored before
static void window_names_kept(GuiWindow *window, unsigned int at, const CliJson *json, unsigned int names)
{
    GuiTab *const tab = &window->tab[at];
    g_cli_cells->persistent_buf_release(tab->names, &g_gui_cells);
    tab->names = NULL;
    tab->names_size = 0ull;
    const unsigned long long line_room = cli_json_written_room(json);
    char *const line_cell = g_cli_cells->persistent_buf_alloc(line_room, &g_gui_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    if (line.out == NULL)
    {
        return;
    }
    cli_json_write(json, names, &line, 0u, 0);
    if (!mmgr_line_ok(&line))
    {
        g_cli_cells->persistent_buf_release(line.out, &g_gui_cells);
        return;
    }
    tab->names = line.out;
    tab->names_size = line.at;
}

// the tab `at`'s names given to the editor, where any came
static void window_names_given(GuiWindow *window, unsigned int at)
{
    const GuiTab *const tab = &window->tab[at];
    if (tab->names == NULL)
    {
        return;
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    CliJson json;
    const unsigned int names =
        cli_json_taken_temporary(&json, tab->names, tab->names_size, 0u, 0ull, &err_msg, &g_gui_cells);
    if (names != CLI_JSON_NONE)
    {
        (void)gui_editor_named(&window->editor, &json, names);
        cli_json_given(&json);
    }
}

// the tree's session and the explorer's group written to orior's folder under the keys the page keeps them by, once
// the tree's kept tabs are back: the tabs' paths in order, the one shown, and the strip's side tool shown; a window
// with no tabs and no session known writes nothing, which keeps a session the folder holds from being written over
// by a read that failed to bring it. The write goes through the link while the window runs and straight to the CLI as
// it ends, when the link has let go of what it had not run
static void window_session_written(GuiWindow *window, int ended)
{
    if (!window->restored || (window->root[0] == '\0') || (!window->session_known && (window->tabs == 0u)))
    {
        return;
    }
    // each tab's path twice and the one shown once, each byte escaped to at most 6
    const unsigned long long session_room =
        (((2ull * window->tabs) + 1ull) * ((CLI_PATH_MAX_LEN * 6ull) + 8ull)) + GUI_KEPT_MORE;
    char *const session_cell = g_cli_cells->temporary_buf_alloc(session_room, &g_gui_cells);
    VerbaLinea session = mmgr_line_over(session_cell, session_room);
    // the session as the page keeps it: the tabs, the same as the main side's, the one shown, no split and no groups
    mmgr_line_put(&session, "{\"tabs\":[");
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        mmgr_line_put(&session, (at > 0u) ? "," : "");
        cli_json_quoted(&session, window->tab[at].path,
                        cellul.len(&(CatenaFinitaCfg){.src = window->tab[at].path, .cap = CLI_PATH_MAX_LEN}));
    }
    mmgr_line_put(&session, "],\"main\":[");
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        mmgr_line_put(&session, (at > 0u) ? "," : "");
        cli_json_quoted(&session, window->tab[at].path,
                        cellul.len(&(CatenaFinitaCfg){.src = window->tab[at].path, .cap = CLI_PATH_MAX_LEN}));
    }
    mmgr_line_put(&session, "],\"active\":");
    if (window->active >= 0)
    {
        cli_json_quoted(&session, window->path,
                        cellul.len(&(CatenaFinitaCfg){.src = window->path, .cap = sizeof(window->path)}));
    }
    else
    {
        mmgr_line_put(&session, "null");
    }
    mmgr_line_put(&session, ",\"split\":null,\"groups\":[]}");
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ull};
    mmgr_line_put(&keyed, GUI_KEPT_SESSION);
    mmgr_line_put(&keyed, window->root);
    // the session quoted again, its bytes escaped to at most 6, and the key and the tool's name beside it
    const unsigned long long args_room = (session.at * 6ull) + (sizeof(key) * 6ull) + GUI_KEPT_MORE;
    char *const args_cell = g_cli_cells->temporary_buf_alloc(args_room, &g_gui_cells);
    VerbaLinea args = mmgr_line_over(args_cell, args_room);
    mmgr_line_put(&args, "{\"changes\":{");
    cli_json_quoted(&args, key, mmgr_line_finish(&keyed) != 0ull ? keyed.at : 0ull);
    mmgr_line_ch(&args, ':');
    cli_json_quoted(&args, session.out, mmgr_line_ok(&session) ? session.at : 0ull);
    if (window_side_shown(window))
    {
        // the tool shown is among the tools
        const GuiTool *const tool = gui_strip_tool((unsigned int)window->strip.shown);
        mmgr_line_put(&args, ",\"" GUI_KEPT_GROUP "\":");
        cli_json_quoted(&args, tool->name, cellul.len(&(CatenaFinitaCfg){.src = tool->name, .cap = 32ull}));
    }
    mmgr_line_put(&args, "}}");
    if (mmgr_line_ok(&args) && mmgr_line_ok(&session))
    {
        if (ended)
        {
            cli_window_given(cli_window_called(1, "state_write", args.out, args.at));
        }
        else
        {
            (void)window_called(window, 1, "state_write", &args);
        }
    }
    g_cli_cells->persistent_buf_release(session.out, &g_gui_cells);
    g_cli_cells->persistent_buf_release(args.out, &g_gui_cells);
}

// the session kept through the link, as each change to it is made
static void window_kept(GuiWindow *window)
{
    window_session_written(window, 0);
}

// the work's inside laid again as the strip's tool shown has a side or not, the view told the editor's size, the top
// folder's listing asked for the first time the side shows, and the group shown kept
static void window_sided(GuiWindow *window)
{
    const int side = window_side_shown(window);
    gui_frame_sided(&window->frame, side);
    window->owed = 1;
    // a movement under way runs on with the side as it now stands
    if (window->stage != NULL)
    {
        window->stages_stale = 1;
        window_claim_laid(window);
    }
    window_view_sized(window);
    if (side && !gui_explorer_root_asked(&window->explorer))
    {
        window_listing_asked(window, "");
    }
    window_kept(window);
}

int gui_window_sized(GuiWindow *window, unsigned int width, unsigned int height, float scale)
{
    // every take the size needs is made first, with nothing of the window changed: the rasters at the first size with room
    // for the largest monitor, and the base's tile tables where it has none yet; none of them from the shared cells at any
    // size after the first. A window past the rasters' room, or a take refused, keeps the size it has, whole
    if (window->framebuffer.pages == NULL)
    {
        unsigned int most_width = 0u;
        unsigned int most_height = 0u;
        gui_system_display_most(&most_width, &most_height);
        if (!gui_framebuffer_made(&window->framebuffer, width, height, most_width, most_height))
        {
            return 0;
        }
    }
    if (!gui_framebuffer_fits(&window->framebuffer, width, height))
    {
        return 0;
    }
    if ((window->base.tile_red_blue == NULL) && !gui_base_made(&window->base, gui_raster_pitch(width), height))
    {
        return 0;
    }
    // then the size taken in one step: the rasters, the base's map and the ground at the size, and the frame laid for it
    (void)gui_framebuffer_fitted(&window->framebuffer, width, height);
    window->shown = NULL;
    gui_base_sized(&window->base, gui_raster_pitch(width), height);
    const int laid = gui_ground_laid(&window->ground, &window->framebuffer.raster[GUI_FRAMEBUFFER_GROUND], width,
                                     height, scale, window->palette, cli_clock_milliseconds());
    gui_frame_laid(&window->frame, width, height, scale);
    gui_frame_sided(&window->frame, window_side_shown(window));
    window->stages_stale = 1;
    gui_menubar_closed(&window->menubar);
    window_claim_laid(window);
    window_view_sized(window);
    return laid;
}

int gui_window_icon(GuiWindow *window, GuiRaster *icon, unsigned int size)
{
    // the eye's line at the size, no thinner than a pixel
    const float stroke = GUI_EYE_STROKE * (float)size / GUI_EYE_SIDE;
    const GuiMask *const eye = gui_icon(&window->icons, "eye", size, (stroke < 1.0f) ? 1.0f : stroke);
    if ((eye == NULL) || (eye->cover == NULL) || !gui_raster_made(icon, size, size))
    {
        return 0;
    }
    gui_raster_stenciled(icon, eye, window->palette->text);
    return 1;
}

// less motion as the system's wish and the kept setting now ask: the claim's slides standing where they are bound, the
// name home with no flight and the lattice kept still, and a frame owed
static void window_motion_laid(GuiWindow *window)
{
    window->motion_still = window->motion_system || window->motion_kept;
    gui_claim_still(&window->claim, window->motion_still);
    gui_wordmark_still(&window->wordmark, window->motion_still);
    gui_busy_still(&window->busy, window->motion_still);
    gui_ground_paused(&window->ground, window->motion_still || window->off_screen, cli_clock_milliseconds());
    window->owed = 1;
    if (window->stage != NULL)
    {
        window_claim_laid(window);
    }
}

void gui_window_motion_told(GuiWindow *window, int reduced)
{
    window->motion_system = reduced;
    window_motion_laid(window);
}

void gui_window_shown(GuiWindow *window, int shown)
{
    window->off_screen = !shown;
    gui_ground_paused(&window->ground, !shown || window->motion_still, cli_clock_milliseconds());
    window->owed = shown || window->owed;
}

// the milliseconds each rest of the caret's breath takes, at full ink and at dim ink alike
#define GUI_CARET_REST ((GUI_CARET_BREATH - (2ull * GUI_CARET_SLIDE)) / 2ull)

// the ink of a slide `through` its milliseconds, from `from` to `to`, eased on a cosine: still at both ends, moving
// fastest in the middle
static unsigned int window_caret_slid(unsigned long long through, unsigned int from, unsigned int to)
{
    // the share of the slide passed, 0 to 1, is a double's
    const double eased = (1.0 - cos(3.14159265358979323846 * ((double)through / (double)GUI_CARET_SLIDE))) / 2.0;
    // an ink between two bytes, rounded, is a byte
    return (unsigned int)((double)from + (((double)to - (double)from) * eased) + 0.5);
}

// the caret's breath, its ink at each stage of it, made once: full through the first rest, sliding down, dim through the
// second rest, and sliding up again
#define GUI_CARET_STAGE 16ull
static unsigned char s_caret_ink[(GUI_CARET_BREATH / GUI_CARET_STAGE) + 1ull];
static int s_caret_ink_made;

static void window_caret_made(void)
{
    for (unsigned long long stage = 0ull; stage <= GUI_CARET_BREATH / GUI_CARET_STAGE; stage += 1ull)
    {
        const unsigned long long at = stage * GUI_CARET_STAGE;
        unsigned int ink = 255u;
        if ((at >= GUI_CARET_REST) && (at < GUI_CARET_REST + GUI_CARET_SLIDE))
        {
            ink = window_caret_slid(at - GUI_CARET_REST, 255u, GUI_CARET_DIM);
        }
        else if ((at >= GUI_CARET_REST + GUI_CARET_SLIDE) && (at < (2ull * GUI_CARET_REST) + GUI_CARET_SLIDE))
        {
            ink = GUI_CARET_DIM;
        }
        else if (at >= (2ull * GUI_CARET_REST) + GUI_CARET_SLIDE)
        {
            ink = window_caret_slid(at - (2ull * GUI_CARET_REST) - GUI_CARET_SLIDE, GUI_CARET_DIM, 255u);
        }
        // an ink is a byte
        s_caret_ink[stage] = (unsigned char)ink;
    }
    s_caret_ink_made = 1;
}

// the caret's ink at `now`, 0 to 255, where its breath stands since it began; full while no cursor stands or less motion
// is asked for
static unsigned int window_caret_ink(const GuiWindow *window, unsigned long long now)
{
    if (!window->editor.has_cursor || window->motion_still)
    {
        return 255u;
    }
    if (!s_caret_ink_made)
    {
        window_caret_made();
    }
    return s_caret_ink[((now - window->caret_since_ms) % GUI_CARET_BREATH) / GUI_CARET_STAGE];
}

// the milliseconds until the caret's ink next changes: the rest's end while it rests, else 1, which the system holds
// to the display's refresh; 0 where no cursor stands
static unsigned long long window_caret_due(const GuiWindow *window, unsigned long long now)
{
    if (!window->editor.has_cursor || window->motion_still)
    {
        return 0ull;
    }
    const unsigned long long at = (now - window->caret_since_ms) % GUI_CARET_BREATH;
    if (at < GUI_CARET_REST)
    {
        return GUI_CARET_REST - at;
    }
    const unsigned long long dim_rest_ends = (2ull * GUI_CARET_REST) + GUI_CARET_SLIDE;
    if ((at >= GUI_CARET_REST + GUI_CARET_SLIDE) && (at < dim_rest_ends))
    {
        return dim_rest_ends - at;
    }
    return 1ull;
}

void gui_window_ticked(GuiWindow *window)
{
    const unsigned long long now = cli_clock_milliseconds();
    (void)gui_claim_stepped(&window->claim, now);
    const int moving = gui_claim_moving(&window->claim, now);
    // the frame laid for the claim's numbers while a slide moves and as it comes to rest; a tick with the claim at
    // rest owes nothing for it
    if (moving || window->claim_moving)
    {
        window_claim_laid(window);
    }
    if (window->claim_moving && !moving)
    {
        // the slides at rest, the view is told the editor's size and the map its span
        window_view_sized(window);
        window_map_asked(window);
    }
    window->claim_moving = moving;
    window->owed = gui_wordmark_ticked(&window->wordmark, &window->glyphs, &window->icons, window->palette, &window->frame, now) || window->owed;
    // the ground where its loop shows a change, and the menus where a rest has ended
    window->owed = gui_ground_ticked(&window->ground, window->palette, now) || window->owed;
    window->owed = gui_menubar_ticked(&window->menubar, &window->glyphs, &window->frame, now) || window->owed;
    // the hover's rest and linger, and the signature's wait
    int ask = 0;
    window->owed = gui_hover_ticked(&window->hover, now, &ask) || window->owed;
    if (ask)
    {
        window_hover_asked(window, window->hover.asked_row, window->hover.asked_column, 0);
    }
    if (gui_signature_ticked(&window->signature, now))
    {
        window_signature_asked(window, 0);
    }
    if ((window->view_moved_ms != 0ull) && (now - window->view_moved_ms >= GUI_VIEW_RESTS))
    {
        window_view_told(window);
    }
    // the busy circle coming or going owes a frame whole
    window->owed = gui_busy_ticked(&window->busy, now) || window->owed;
    // the caret where its ink has changed and the circle where its stage has: where nothing else owes a frame, each drawn again
    // pays it alone, and a frame owed after this tick is still paid whole; no caret stands while the view is not drawn, and a
    // caret under the circle owes the frame whole, neither knowing what the other has laid
    const int caret_changed = !window->wordmark.stands && (window_caret_ink(window, now) != window->caret_ink);
    const int busy_stepped = window->busy.under_kept && !window->off_screen && (gui_busy_stage_at(&window->busy, now) != window->busy.stage_drawn);
    const int crossed = window->busy.under_kept && window->editor.caret_kept && !gui_rect_empty(gui_rect_met(window->busy.under_rect, window->editor.caret_rect));
    const int alone = !window->owed && !moving && !crossed;
    window->caret_alone = caret_changed && alone;
    window->busy_alone = busy_stepped && alone;
    window->owed = window->owed || ((caret_changed || busy_stepped) && !alone);
}

// whether a panel or a menu lies over `caret`, which keeps the caret from being drawn over them
static int window_caret_covered(const GuiWindow *window, GuiRect caret)
{
    if (window->hover.shown && !gui_rect_empty(gui_rect_met(window->hover.rect, caret)))
    {
        return 1;
    }
    if (window->suggest.open && !gui_rect_empty(gui_rect_met(window->suggest.rect, caret)))
    {
        return 1;
    }
    if (window->signature.shown && !gui_rect_empty(gui_rect_met(window->signature.rect, caret)))
    {
        return 1;
    }
    for (unsigned int level = 0u; level < window->menubar.levels; level += 1u)
    {
        if (!gui_rect_empty(gui_rect_met(window->menubar.menu[level].rect, caret)))
        {
            return 1;
        }
    }
    return 0;
}

void gui_window_flipped(GuiWindow *window)
{
    window->shown = window->raster;
    window->raster = gui_framebuffer_flipped(&window->framebuffer);
}

GuiRect gui_window_caret_drawn(GuiWindow *window)
{
    window->caret_alone = 0;
    window->caret_ink = window_caret_ink(window, cli_clock_milliseconds());
    window->editor.caret_ink = window->caret_ink;
    return gui_editor_caret_redrawn(&window->editor, (window->shown != NULL) ? window->shown : window->raster,
                                    window->palette);
}

GuiRect gui_window_busy_drawn(GuiWindow *window)
{
    window->busy_alone = 0;
    return gui_busy_redrawn(&window->busy, (window->shown != NULL) ? window->shown : window->raster, cli_clock_milliseconds());
}

// the milliseconds until the view's rest is told, 0 where no telling waits
static unsigned long long window_view_due(const GuiWindow *window, unsigned long long now)
{
    if (window->view_moved_ms == 0ull)
    {
        return 0ull;
    }
    const unsigned long long passed = now - window->view_moved_ms;
    return (passed < GUI_VIEW_RESTS) ? GUI_VIEW_RESTS - passed : 1ull;
}

// the lesser of two waits, 0 standing for never
static unsigned long long window_sooner(unsigned long long one, unsigned long long other)
{
    if (one == 0ull)
    {
        return other;
    }
    return ((other != 0ull) && (other < one)) ? other : one;
}

unsigned long long gui_window_due(const GuiWindow *window)
{
    const unsigned long long now = cli_clock_milliseconds();
    unsigned long long due = gui_claim_due(&window->claim, now);
    due = window_sooner(due, gui_hover_due(&window->hover, now));
    due = window_sooner(due, gui_signature_due(&window->signature, now));
    due = window_sooner(due, gui_ground_due(&window->ground, now));
    due = window_sooner(due, gui_wordmark_due(&window->wordmark, now));
    due = window_sooner(due, gui_menubar_due(&window->menubar, now));
    due = window_sooner(due, window_caret_due(window, now));
    // the circle's clock stands while the window is off screen, an answer waking it as any does
    due = window_sooner(due, window->off_screen ? 0ull : gui_busy_due(&window->busy, now));
    return window_sooner(due, window_view_due(window, now));
}

void gui_window_started(GuiWindow *window)
{
    char text[8];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{}");
    window->asked_launch = window_called(window, 1, "launch_take", &args);
    window->asked_root = window_called(window, 1, "root", &args);
    window->asked_catalog = window_called(window, 1, "commands_read", &args);
}

// the catalog of commands, answered as a JSON string holding its text, taken as its own tree, the menu bar laid out
// over it
static void window_cataloged(GuiWindow *window, const char *text, unsigned long long size)
{
    if (window->cataloged)
    {
        gui_menubar_cataloged(&window->menubar, NULL, CLI_JSON_NONE);
        cli_json_given(&window->catalog);
        window->cataloged = 0;
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    CliJson quoted;
    const unsigned int string = cli_json_taken(&quoted, text, size, 0u, 0ull, &err_msg, &g_gui_cells);
    unsigned long long inner_size = 0ull;
    const char *const inner = cli_json_string(&quoted, string, &inner_size);
    const unsigned int root =
        (inner != NULL) ? cli_json_taken(&window->catalog, inner, inner_size, 0u, 0ull, &err_msg, &g_gui_cells)
                        : CLI_JSON_NONE;
    if (string != CLI_JSON_NONE)
    {
        cli_json_given(&quoted);
    }
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    window->cataloged = 1;
    gui_menubar_cataloged(&window->menubar, &window->catalog, cli_json_member(&window->catalog, root, "menus"));
    gui_menubar_laid(&window->menubar, &window->glyphs, &window->frame);
    window->stages_stale = window->stage != NULL;
}

// the catalog's commands the window runs itself; whether `command` is among them
static const char *const s_window_commands[] = {"exit", "close-editor", "close-all", "last-editor", "reduce-motion"};
// the line edits of the menus with no keys of their own, which the view runs by name
static const char *const s_view_commands[] = {"sort-ascending", "sort-descending", "unique-lines", "upper-case", "lower-case", "title-case"};

// whether `command` is among `count` names
static int window_named_among(const char *command, const char *const *names, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = names[at], .cap = 256ull}))
        {
            return 1;
        }
    }
    return 0;
}

static int window_runs(const char *command)
{
    return window_named_among(command, s_window_commands, sizeof(s_window_commands) / sizeof(s_window_commands[0]));
}

void gui_window_chosen(GuiWindow *window, const GuiChoice *choice)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = choice->command, .other = "exit", .cap = sizeof(choice->command)}))
    {
        window->pressed = GUI_HIT_CLOSE;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = choice->command, .other = "close-editor", .cap = sizeof(choice->command)}))
    {
        if (window->active >= 0)
        {
            // the tab shown is among the tabs
            gui_window_tab_closed(window, (unsigned int)window->active);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = choice->command, .other = "close-all", .cap = sizeof(choice->command)}))
    {
        while (window->tabs != 0u)
        {
            gui_window_tab_closed(window, window->tabs - 1u);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = choice->command, .other = "last-editor", .cap = sizeof(choice->command)}))
    {
        if ((window->last_active >= 0) && ((unsigned int)window->last_active < window->tabs))
        {
            gui_window_tab_shown(window, (unsigned int)window->last_active);
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = choice->command, .other = "reduce-motion", .cap = sizeof(choice->command)}))
    {
        // chosen, it asks for the opposite of what the window now does, and orior's folder keeps it
        window->motion_kept = !window->motion_still;
        char text[128];
        VerbaLinea args = {text, sizeof(text), 0ull};
        mmgr_line_put(&args, "{\"changes\":{\"" GUI_WINDOW_REDUCE_KEY "\":");
        mmgr_line_put(&args, window->motion_kept ? "\"true\"}}" : "\"false\"}}");
        (void)window_called(window, 1, "state_write", &args);
        window_motion_laid(window);
        return;
    }
    if (choice->keys[0] != '\0')
    {
        gui_window_key(window, choice->keys);
    }
    else if (window_named_among(choice->command, s_view_commands, sizeof(s_view_commands) / sizeof(s_view_commands[0])))
    {
        gui_window_key(window, choice->command);
    }
}

// the shown file's path and key set to the tab `at`'s, the editor emptied for its frames
static void window_tab_taken(GuiWindow *window, unsigned int at)
{
    const GuiTab *const tab = &window->tab[at];
    // the tabs are fewer than an int counts
    window->last_active = ((window->active >= 0) && (window->active != (int)at)) ? window->active : window->last_active;
    window->active = (int)at;
    VerbaLinea path = {window->path, sizeof(window->path), 0ull};
    mmgr_line_put(&path, tab->path);
    (void)mmgr_line_finish(&path);
    VerbaLinea key = {window->key, sizeof(window->key), 0ull};
    mmgr_line_put(&key, tab->key);
    (void)mmgr_line_finish(&key);
    window->viewed = tab->viewed;
    window->frame_seq = 0ull;
    window->menubar.has_editor = 1;
    gui_editor_given(&window->editor);
    gui_editor_made(&window->editor);
    window_names_given(window, at);
    gui_minimap_cleared(&window->minimap);
    window_panels_hidden(window);
    window->view_told = 0;
    window->view_moved_ms = 0ull;
    window->owed = 1;
    window_kept(window);
}

void gui_window_tab_shown(GuiWindow *window, unsigned int at)
{
    if (at >= window->tabs)
    {
        return;
    }
    window_tab_taken(window, at);
    // the view open on the file returns a frame when it is sent the editor's size
    if (window->viewed)
    {
        window_view_sized(window);
    }
}

void gui_window_tab_closed(GuiWindow *window, unsigned int at)
{
    if (at >= window->tabs)
    {
        return;
    }
    const GuiTab *const tab = &window->tab[at];
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"key\":");
    cli_json_quoted(&args, tab->key, cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
    mmgr_line_ch(&args, '}');
    if (tab->viewed)
    {
        // the view closes on the inputs' lane, in order with a later open under its key; the server's letting go waits
        // on the server: it runs on the pool, off that lane
        (void)window_called(window, 1, "editor_view_close", &args);
    }
    if (tab->served)
    {
        char file_text[CLI_PATH_MAX_LEN + 32u];
        VerbaLinea file_args = {file_text, sizeof(file_text), 0ull};
        mmgr_line_put(&file_args, "{\"path\":");
        cli_json_quoted(&file_args, tab->path,
                        cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}));
        mmgr_line_ch(&file_args, '}');
        (void)window_called(window, 0, "editor_server_close", &file_args);
    }
    (void)window_called(window, 1, "document_release", &args);
    g_cli_cells->persistent_buf_release(tab->names, &g_gui_cells);
    for (unsigned int after = at + 1u; after < window->tabs; after += 1u)
    {
        window->tab[after - 1u] = window->tab[after];
    }
    window->tabs -= 1u;
    // the tabs are fewer than an int counts
    window->last_active = ((window->last_active > (int)at)    ? window->last_active - 1
                           : (window->last_active == (int)at) ? -1
                                                              : window->last_active);
    window->owed = 1;
    if (window->tabs == 0u)
    {
        window->active = -1;
        window->path[0] = '\0';
        window->key[0] = '\0';
        window->viewed = 0;
        window->frame_seq = 0ull;
        window->menubar.has_editor = 0;
        gui_editor_given(&window->editor);
        gui_editor_made(&window->editor);
        gui_minimap_cleared(&window->minimap);
        window_panels_hidden(window);
        window_claimed(window, 0);
        // the last tab closed is the session's own change: kept, tabs or none
        window->session_known = 1;
        window_kept(window);
        return;
    }
    if ((int)at == window->active)
    {
        window->active = -1;
        gui_window_tab_shown(window, (at < window->tabs) ? at : window->tabs - 1u);
        return;
    }
    if ((int)at < window->active)
    {
        window->active -= 1;
    }
    window_kept(window);
}

// the file at `length` bytes of `path`, a path from the tree's top, opened as a tab shown: its tab where it is open
// already, else a new one with its branch opened under a key of its own
static void window_branch_opened(GuiWindow *window, const char *path, unsigned long long length)
{
    for (unsigned int at = 0u; at < window->tabs; at += 1u)
    {
        const GuiTab *const tab = &window->tab[at];
        if ((cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = tab->path, .other = path, .bytes = length}) == 0))
        {
            gui_window_tab_shown(window, at);
            return;
        }
    }
    if (window->tabs >= GUI_TABS_MOST)
    {
        return;
    }
    GuiTab *const tab = &window->tab[window->tabs];
    VerbaLinea kept = {tab->path, sizeof(tab->path), 0ull};
    mmgr_line_put_n(&kept, path, length);
    (void)mmgr_line_finish(&kept);
    VerbaLinea key = {tab->key, sizeof(tab->key), 0ull};
    mmgr_line_put(&key, GUI_OPEN_KEY_HEAD);
    mmgr_line_put_n(&key, path, length);
    (void)mmgr_line_finish(&key);
    tab->viewed = 0;
    tab->crlf = 0;
    tab->names = NULL;
    tab->names_size = 0ull;
    tab->asked_serve = 0ull;
    tab->serve_asked = 0;
    tab->served = 0;
    window->tabs += 1u;
    window_tab_taken(window, window->tabs - 1u);
    char text[(2u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"key\":");
    cli_json_quoted(&args, window->key, key.at);
    mmgr_line_put(&args, ",\"path\":");
    cli_json_quoted(&args, window->path, kept.at);
    mmgr_line_put(&args, ",\"first\":0,\"last\":");
    mmgr_line_decimal(&args, window_rows(window), 1u);
    mmgr_line_put(&args, ",\"columns\":0}");
    tab->asked_open = window_called(window, 1, "branch_open", &args);
}

void gui_window_file_opened(GuiWindow *window, const char *path)
{
    // a path that is not whole is from the tree's top already; a whole one is made the tree's by the CLI, and kept as
    // given for the open where the tree holds no shorter one for it
    const int whole = (path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':'));
    VerbaLinea kept = {window->path, sizeof(window->path), 0ull};
    mmgr_line_put(&kept, path);
    (void)mmgr_line_finish(&kept);
    if (!whole)
    {
        window_branch_opened(window, path, kept.at);
        return;
    }
    char text[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{\"path\":");
    cli_json_quoted(&args, path, kept.at);
    mmgr_line_ch(&args, '}');
    window->asked_relative = window_called(window, 0, "tree_relative", &args);
}

int gui_window_text_put(GuiWindow *window, const char *text, unsigned long long length)
{
    window->owed = 1;
    return gui_editor_text_put(&window->editor, text, length);
}

// the tree's name: the last part of its path
static void window_titled(GuiWindow *window, const char *root, unsigned long long length)
{
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (((root[at] == '/') || (root[at] == '\\')) && (at + 1ull < length))
        {
            start = at + 1ull;
        }
    }
    VerbaLinea title = {window->title, sizeof(window->title), 0ull};
    mmgr_line_put_n(&title, root + start, length - start);
    (void)mmgr_line_finish(&title);
}

// the open's answer for the tab `at` taken: the grammar's names kept with the tab and given to the editor where the
// tab is shown, and the text, read whole or paged, shown through the CLI's editor view, opened on it sized to the
// editor
static void window_opened(GuiWindow *window, const CliJson *json, unsigned int root, unsigned int at)
{
    GuiTab *const tab = &window->tab[at];
    // the tabs are fewer than an int counts
    const int shown = (int)at == window->active;
    const unsigned int names = cli_json_member(json, root, "names");
    if (names != CLI_JSON_NONE)
    {
        window_names_kept(window, at, json, names);
        if (shown)
        {
            (void)gui_editor_named(&window->editor, json, names);
        }
    }
    const int paged = gui_answer_member_true(json, root, "windowed") &&
                      cli_json_is(json, cli_json_member(json, root, "shape"), CFG_JSON_OBJECT);
    // a text read whole says how its lines end: a carriage return before a newline, as the text or as its escape
    unsigned long long text_length = 0ull;
    const char *const whole = cli_json_string(json, cli_json_member(json, root, "text"), &text_length);
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
        VerbaLinea args = {text, sizeof(text), 0ull};
        mmgr_line_put(&args, "{\"key\":");
        cli_json_quoted(&args, tab->key, cellul.len(&(CatenaFinitaCfg){.src = tab->key, .cap = sizeof(tab->key)}));
        mmgr_line_put(&args, ",\"path\":");
        cli_json_quoted(&args, tab->path, cellul.len(&(CatenaFinitaCfg){.src = tab->path, .cap = sizeof(tab->path)}));
        mmgr_line_put(&args, ",\"rows\":");
        mmgr_line_decimal(&args, window_rows(window), 1u);
        mmgr_line_put(&args, ",\"columns\":");
        mmgr_line_decimal(&args, window_columns(window), 1u);
        mmgr_line_ch(&args, '}');
        const unsigned long long ticket = window_called(window, 1, "editor_view_open", &args);
        window->asked_view = shown ? ticket : window->asked_view;
    }
}

// what orior's folder keeps for the tree taken, the answer `root` of `json`: the explorer's group shown as the
// strip's tool of that name with its side, and the tree's session, its tabs opened in order and the one shown; the
// session is kept again from here on
static void window_restored(GuiWindow *window, const CliJson *json, unsigned int root)
{
    unsigned long long length = 0ull;
    const char *const group = cli_json_string(json, cli_json_member(json, root, GUI_KEPT_GROUP), &length);
    for (unsigned int at = 0u; (group != NULL) && (at < gui_strip_tools()); at += 1u)
    {
        const GuiTool *const tool = gui_strip_tool(at);
        if (tool->side && (cellul.len(&(CatenaFinitaCfg){.src = tool->name, .cap = 32ull}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = tool->name, .other = group, .bytes = length}) == 0))
        {
            // the tools are fewer than an int counts
            window->strip.shown = (int)at;
            window_sided(window);
        }
    }
    char key[CLI_PATH_MAX_LEN + 32u];
    VerbaLinea keyed = {key, sizeof(key), 0ull};
    mmgr_line_put(&keyed, GUI_KEPT_SESSION);
    mmgr_line_put(&keyed, window->root);
    unsigned long long size = 0ull;
    const char *const session =
        (mmgr_line_finish(&keyed) != 0ull) ? cli_json_string(json, cli_json_member(json, root, key), &size) : NULL;
    window->session_known = (session != NULL);
    if (session != NULL)
    {
        char err_text[256];
        VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
        CliJson kept;
        const unsigned int object = cli_json_taken(&kept, session, size, 0u, 0ull, &err_msg, &g_gui_cells);
        if (object != CLI_JSON_NONE)
        {
            const unsigned int tabs = cli_json_member(&kept, object, "tabs");
            const unsigned int count = cli_json_is(&kept, tabs, CFG_JSON_ARRAY) ? kept.node[tabs].count : 0u;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                unsigned long long path_length = 0ull;
                const char *const path = cli_json_string(&kept, cli_json_item(&kept, tabs, at), &path_length);
                char copied[CLI_PATH_MAX_LEN];
                VerbaLinea line = {copied, sizeof(copied), 0ull};
                mmgr_line_put_n(&line, (path != NULL) ? path : "", path_length);
                if ((path != NULL) && (mmgr_line_finish(&line) != 0ull))
                {
                    gui_window_file_opened(window, copied);
                }
            }
            unsigned long long active_length = 0ull;
            const char *const active = cli_json_string(&kept, cli_json_member(&kept, object, "active"), &active_length);
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea line = {shown, sizeof(shown), 0ull};
            mmgr_line_put_n(&line, (active != NULL) ? active : "", active_length);
            if ((active != NULL) && (mmgr_line_finish(&line) != 0ull))
            {
                gui_window_file_opened(window, shown);
            }
            cli_json_given(&kept);
        }
    }
    window->restored = 1;
}

// the completion's list, where it is open, followed to the frame's cursor: filtered again by the word between the
// prefix's start and the cursor, read off the editor's own line, and closed where the cursor has left the line, stands
// before the prefix's start, or has a byte between that is no word's
static void window_suggest_followed(GuiWindow *window)
{
    GuiSuggest *const suggest = &window->suggest;
    if (!suggest->open)
    {
        return;
    }
    const GuiPlace cursor = window->editor.cursor;
    const char *text = NULL;
    unsigned long long length = 0ull;
    if (!window->editor.has_cursor || (cursor.line != suggest->line) || (cursor.col < suggest->from) ||
        !gui_editor_units_text(&window->editor, cursor.line, suggest->from, cursor.col, &text, &length))
    {
        gui_suggest_closed(suggest);
        return;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (!window_word_byte(text[at]))
        {
            gui_suggest_closed(suggest);
            return;
        }
    }
    (void)gui_suggest_prefixed(suggest, text, length);
    window->owed = 1;
}

// a frame of the view taken, where it is the open file's and no older than the last taken
static void window_frame_taken(GuiWindow *window, const CliJson *json, unsigned int root)
{
    unsigned long long length = 0ull;
    const char *const key = cli_json_string(json, cli_json_member(json, root, "key"), &length);
    const unsigned long long seq = gui_answer_member_whole(json, root, "seq", 0ull);
    if ((key == NULL) || (length != cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)})) ||
        (memor.cmp(&(MemoriaCfg){.src = key, .other = window->key, .bytes = length}) != 0) || (seq < window->frame_seq))
    {
        return;
    }
    window->frame_seq = seq;
    window->viewed = 1;
    if (window->active >= 0)
    {
        window->tab[window->active].viewed = 1;
    }
    (void)gui_editor_frame_put(&window->editor, json, root);
    // the caret's breath begins again with every frame, which keeps the caret at full ink while the keys go
    window->caret_since_ms = cli_clock_milliseconds();
    window_view_moved(window);
    window_suggest_followed(window);
}

// whether an answer is a frame of the view, by what it carries
static int window_is_frame(const CliJson *json, unsigned int root)
{
    return (cli_json_member(json, root, "rows") != CLI_JSON_NONE) &&
           (cli_json_member(json, root, "cursor") != CLI_JSON_NONE) &&
           (cli_json_member(json, root, "seq") != CLI_JSON_NONE);
}

// a door command run: `open-file` opens its file; the command answered done, or not and why
static void window_commanded(GuiWindow *window, const CliJson *json, unsigned int root)
{
    unsigned long long id_length = 0ull;
    const char *const id = cli_json_string(json, cli_json_member(json, root, "id"), &id_length);
    unsigned long long name_length = 0ull;
    const char *const name = cli_json_string(json, cli_json_member(json, root, "command"), &name_length);
    if ((id == NULL) || (name == NULL))
    {
        return;
    }
    const unsigned int args = cli_json_member(json, root, "args");
    unsigned long long path_length = 0ull;
    const char *const path = cli_json_string(json, cli_json_item(json, args, 0u), &path_length);
    const int opening = (name_length == 9ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = name, .other = "open-file", .bytes = 9u}) == 0) &&
                        (path != NULL);
    if (opening)
    {
        char copied[CLI_PATH_MAX_LEN];
        VerbaLinea line = {copied, sizeof(copied), 0ull};
        mmgr_line_put_n(&line, path, path_length);
        (void)mmgr_line_finish(&line);
        gui_window_file_opened(window, copied);
    }
    char text[CLI_NAME_MAX_LEN + 256u];
    VerbaLinea answer = {text, sizeof(text), 0ull};
    mmgr_line_put(&answer, "{\"id\":");
    cli_json_quoted(&answer, id, id_length);
    mmgr_line_put(&answer, opening ? ",\"done\":true,\"message\":\"\"}"
                                   : ",\"done\":false,\"message\":\"the window has no command ");
    if (!opening)
    {
        mmgr_line_put_n(&answer, name, name_length);
        mmgr_line_put(&answer, "\"}");
    }
    (void)window_called(window, 0, "command_answer", &answer);
}

// what orior's folder keeps read for the name, `is_object` 0 where it could not be: the session last started in down to
// its two words, and the session the system names asked for
static void window_mark_kept_read(GuiWindow *window, int is_object, const CliJson *json, unsigned int root)
{
    unsigned long long length = 0ull;
    const char *const kept = is_object ? cli_json_string(json, cli_json_member(json, root, GUI_WORDMARK_SESSION_KEY), &length) : NULL;
    (void)gui_wordmark_session_read(kept, length, &window->wordmark.kept);
    char text[8];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{}");
    window->asked_mark_session = window_called(window, 1, "session_key", &args);
}

// the session the system names come, `named`, `size` bytes of it, NULL where it names none: kept as the session started
// in, and the name set going on the first start in it, or the eye home at once on any start after
static void window_mark_session_named(GuiWindow *window, const char *named, unsigned long long size)
{
    GuiWordmarkSession now;
    (void)gui_wordmark_session_read(named, size, &now);
    const int first = gui_wordmark_first_in(&window->wordmark.kept, &now);
    if (named != NULL)
    {
        char text[2048];
        VerbaLinea args = {text, sizeof(text), 0ull};
        mmgr_line_put(&args, "{\"changes\":{\"" GUI_WORDMARK_SESSION_KEY "\":");
        cli_json_quoted(&args, named, size);
        mmgr_line_put(&args, "}}");
        (void)window_called(window, 1, "state_write", &args);
    }
    if (first)
    {
        gui_wordmark_stood(&window->wordmark, cli_clock_milliseconds());
    }
    else
    {
        gui_wordmark_landed(&window->wordmark);
    }
}

// the name told the tree is read while its pane stands: the root answered, and what the folder keeps for a tree back
// with its tabs opened, where there is a tree
static void window_mark_tree_read(GuiWindow *window)
{
    if (window->wordmark.stands && !window->wordmark.tree_read && (window->asked_root == 0ull) && (window->asked_state == 0ull) &&
        ((window->root[0] == '\0') || window->restored))
    {
        gui_wordmark_tree_read(&window->wordmark);
    }
}

// one answer or event taken
static void window_taken(void *context, unsigned long long ticket, const char *name, int ok, const char *text,
                         unsigned long long size)
{
    GuiWindow *const window = (GuiWindow *)context;
    if (ticket != 0ull)
    {
        gui_busy_answered(&window->busy, ticket);
    }
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    // the answer's tree read on the window thread's lane against a mark, the lane left before the answer is handled:
    // what the handling keeps is taken where it is kept, and the lane released at the mark once the tree is given
    // back
    CliJson json;
    const unsigned int root =
        ok ? cli_json_taken_temporary(&json, text, size, 0u, 0ull, &err_msg, &g_gui_cells) : CLI_JSON_NONE;
    window->owed = 1;
    if ((ticket != 0ull) && (ticket == window->asked_launch))
    {
        window->asked_launch = 0ull;
        unsigned long long length = 0ull;
        const char *const command = cli_json_string(&json, cli_json_member(&json, root, "command"), &length);
        unsigned long long path_length = 0ull;
        const char *const path =
            cli_json_string(&json, cli_json_item(&json, cli_json_member(&json, root, "args"), 0u), &path_length);
        if ((command != NULL) && (length == 9ull) &&
            (memor.cmp(&(MemoriaCfg){.src = command, .other = "open-file", .bytes = 9u}) == 0) && (path != NULL))
        {
            char copied[CLI_PATH_MAX_LEN];
            VerbaLinea line = {copied, sizeof(copied), 0ull};
            mmgr_line_put_n(&line, path, path_length);
            (void)mmgr_line_finish(&line);
            gui_window_file_opened(window, copied);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_root))
    {
        window->asked_root = 0ull;
        unsigned long long length = 0ull;
        const char *const tree = cli_json_string(&json, root, &length);
        if (tree != NULL)
        {
            window_titled(window, tree, length);
            VerbaLinea kept = {window->root, sizeof(window->root), 0ull};
            mmgr_line_put_n(&kept, tree, length);
            (void)mmgr_line_finish(&kept);
        }
        // what orior's folder keeps is asked once whether there is a tree is known: the tree's places and the name's
        // session are one answer
        char args_text[8];
        VerbaLinea args = {args_text, sizeof(args_text), 0ull};
        mmgr_line_put(&args, "{}");
        window->asked_state = window_called(window, 1, "state_read", &args);
    }
    else if ((ticket != 0ull) && (ticket == window->asked_state))
    {
        window->asked_state = 0ull;
        const int is_object = ok && (root != CLI_JSON_NONE) && cli_json_is(&json, root, CFG_JSON_OBJECT);
        // less motion as orior's folder keeps it, read before the name stands
        unsigned long long reduce_length = 0ull;
        const char *const reduce = is_object ? cli_json_string(&json, cli_json_member(&json, root, GUI_WINDOW_REDUCE_KEY), &reduce_length) : NULL;
        window->motion_kept = (reduce != NULL) && (reduce_length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = reduce, .other = "true", .bytes = 4u}) == 0);
        window_motion_laid(window);
        window_mark_kept_read(window, is_object, &json, root);
        if (is_object && (window->root[0] != '\0'))
        {
            window_restored(window, &json, root);
        }
        else
        {
            window->restored = 1;
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_mark_session))
    {
        window->asked_mark_session = 0ull;
        window_mark_session_named(window, ok ? text : NULL, size);
    }
    else if ((ticket != 0ull) && (ticket == window->asked_catalog))
    {
        window->asked_catalog = 0ull;
        if (ok)
        {
            window_cataloged(window, text, size);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_relative))
    {
        window->asked_relative = 0ull;
        unsigned long long length = 0ull;
        const char *const relative = cli_json_string(&json, root, &length);
        if (relative != NULL)
        {
            window_branch_opened(window, relative, length);
        }
        else
        {
            window_branch_opened(window, window->path,
                                 cellul.len(&(CatenaFinitaCfg){.src = window->path, .cap = sizeof(window->path)}));
        }
    }
    else if ((ticket != 0ull) && (window_tab_by_ticket(window, ticket) >= 0))
    {
        // the tab is among the tabs
        const unsigned int at = (unsigned int)window_tab_by_ticket(window, ticket);
        window->tab[at].asked_open = 0ull;
        if (root != CLI_JSON_NONE)
        {
            window_opened(window, &json, root, at);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_map))
    {
        window->asked_map = 0ull;
        if (ok && (root != CLI_JSON_NONE))
        {
            (void)gui_minimap_put(&window->minimap, &json, root);
        }
        if (window->map_wanted)
        {
            window->map_wanted = 0;
            window_map_asked(window);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_hover))
    {
        window->asked_hover = 0ull;
        if (ok && (root != CLI_JSON_NONE))
        {
            (void)gui_hover_put(&window->hover, &json, root);
        }
        // the answer without the server shown, the server is asked where the pointer still rests on the cell
        if (window->hover_again && window->hover.asked && (window->hover.row == window->hover.asked_row) &&
            (window->hover.column == window->hover.asked_column))
        {
            window_hover_asked(window, window->hover.asked_row, window->hover.asked_column, 1);
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_complete))
    {
        // an answer for a prefix the cursor has left, an edit having come between the ask and the answer, is dropped
        window->asked_complete = 0ull;
        const int current = ok && (root != CLI_JSON_NONE) && window_complete_current(window, &json, root);
        if (current)
        {
            (void)gui_suggest_put(&window->suggest, &json, root, window->complete_forced);
        }
        const int again = window->complete_again && current && window->suggest.open;
        window->complete_again = 0;
        if (window->complete_wanted)
        {
            window_complete_asked(window, 0);
        }
        else if (again)
        {
            // the list open on the file's words, the server's items are asked for, the forced ask kept forced
            const int forced = window->complete_forced;
            window_complete_asked(window, 1);
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
        if (ok && (root != CLI_JSON_NONE))
        {
            (void)gui_signature_put(&window->signature, &json, root);
        }
        else
        {
            gui_signature_hidden(&window->signature);
        }
        // the tree's answer shown, the server's is asked for where the cursor stands where it was asked
        if (window->signature_again && (window->editor.cursor.line == window->signature_place.line) &&
            (window->editor.cursor.col == window->signature_place.col))
        {
            window_signature_asked(window, 1);
        }
    }
    else if ((ticket != 0ull) && (window_tab_by_serve(window, ticket) >= 0))
    {
        // the tab is among the tabs
        GuiTab *const tab = &window->tab[(unsigned int)window_tab_by_serve(window, ticket)];
        tab->asked_serve = 0ull;
        tab->served = ok && (root != CLI_JSON_NONE) && gui_answer_member_true(&json, root, "served");
        // a view served is told to the server as its rows rest
        if (tab->served && (tab == &window->tab[window->active]))
        {
            window->view_told = 0;
            window_view_moved(window);
        }
    }
    else if ((ticket != 0ull) && (root != CLI_JSON_NONE) && window_is_frame(&json, root))
    {
        // a frame of any tab's view says its view is open; the shown tab's is drawn; a view open is handed to its
        // file's language server once
        window->asked_view = (ticket == window->asked_view) ? 0ull : window->asked_view;
        unsigned long long length = 0ull;
        const char *const key = cli_json_string(&json, cli_json_member(&json, root, "key"), &length);
        const int at = (key != NULL) ? window_tab_by_key(window, key, length) : -1;
        if (at >= 0)
        {
            window->tab[at].viewed = 1;
            // the tab is among the tabs
            window_serve_asked(window, (unsigned int)at);
        }
        if ((at >= 0) && (at == window->active))
        {
            window_frame_taken(window, &json, root);
            window_map_asked(window);
            // a place wanted, as a problem's row picked it, is put as the view's first frame comes
            if (window->place_pending)
            {
                window->place_pending = 0;
                window_placed(window, window->place_wanted.line, window->place_wanted.col);
            }
            // the completion waits for the frame that places the cursor after the key typed; a signature shown
            // follows the cursor, asked again as the cursor leaves the place it was asked at
            if (window->told_wanted)
            {
                // the server is told the text an input changed, off the inputs' lane
                window->told_wanted = 0;
                char text[CLI_PATH_MAX_LEN + 32u];
                VerbaLinea args = {text, sizeof(text), 0ull};
                window_key_args(window, &args, "");
                (void)window_called(window, 0, "editor_view_told", &args);
            }
            if (window->complete_wanted)
            {
                window_complete_asked(window, 0);
            }
            const int moved = (window->editor.cursor.line != window->signature_place.line) ||
                              (window->editor.cursor.col != window->signature_place.col);
            if (window->signature.shown && (window->asked_signature == 0ull) && moved)
            {
                window_signature_asked(window, 0);
            }
        }
    }
    else if ((ticket != 0ull) && (ticket == window->asked_memory))
    {
        window->asked_memory = 0ull;
        if (ok && (root != CLI_JSON_NONE))
        {
            window->memory_bytes =
                gui_answer_member_whole(&json, cli_json_member(&json, root, "orior"), "working", 0ull);
        }
    }
    else if ((ticket != 0ull) && ok && (root != CLI_JSON_NONE) &&
             gui_explorer_listed(&window->explorer, ticket, &json, root))
    {
        window->owed = 1;
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "branch-colors", .cap = 64ull}))
    {
        unsigned long long length = 0ull;
        const char *const key = cli_json_string(&json, cli_json_member(&json, root, "key"), &length);
        const int at = (key != NULL) ? window_tab_by_key(window, key, length) : -1;
        const unsigned int names = cli_json_member(&json, root, "names");
        if ((at >= 0) && (names != CLI_JSON_NONE))
        {
            // the tab is among the tabs
            window_names_kept(window, (unsigned int)at, &json, names);
        }
        if ((at >= 0) && (at == window->active))
        {
            if (names != CLI_JSON_NONE)
            {
                (void)gui_editor_named(&window->editor, &json, names);
            }
            (void)gui_editor_colored(&window->editor, &json, cli_json_member(&json, root, "colors"));
            // the bars take their names from the runs, which the colors change
            window->minimap.stale = 1;
            window_map_asked(window);
        }
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "command", .cap = 64ull}))
    {
        window_commanded(window, &json, root);
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "lsp-diagnostics", .cap = 64ull}))
    {
        // a file's diagnostics come as its server or a checker reports them; the strip's Problems tool is marked
        // while errors stand
        (void)gui_problems_put(&window->problems, &json, root);
        window->strip.problems_marked = window->problems.errors > 0u;
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "tree-check", .cap = 64ull}))
    {
        gui_problems_checking(&window->problems, gui_answer_member_whole(&json, root, "done", 0ull),
                              gui_answer_member_whole(&json, root, "total", 0ull));
        // the check runs on past its ask's answer, its progress coming as these events: the busy circle stands through it
        gui_busy_work(&window->busy, gui_problems_running(&window->problems), cli_clock_milliseconds());
    }
    else if ((ticket != 0ull) && (ticket == window->asked_check))
    {
        window->asked_check = 0ull;
    }
    if (ok)
    {
        cli_json_given(&json);
    }
}

// the memory reading asked for, where none is in flight and the last came GUI_MEMORY_EVERY milliseconds ago
static void window_memory_asked(GuiWindow *window)
{
    const unsigned long long now = cli_clock_milliseconds();
    if ((window->asked_memory != 0ull) || ((window->memory_ms != 0ull) && (now - window->memory_ms < GUI_MEMORY_EVERY)))
    {
        return;
    }
    char text[8];
    VerbaLinea args = {text, sizeof(text), 0ull};
    mmgr_line_put(&args, "{}");
    window->memory_ms = now;
    window->asked_memory = window_called(window, 1, "memory_usage", &args);
}

void gui_window_drained(GuiWindow *window)
{
    (void)gui_link_drained(&window->link, window_taken, window);
    window_memory_asked(window);
    window_mark_tree_read(window);
}

// the panels over the editor drawn inside the work's inside: the hover under its word, hidden where theword's line
// has left the rows; the completion under the prefix's first cell; and the signature over the cursor's row
static void window_panels_drawn(GuiWindow *window)
{
    const GuiFrame *const frame = &window->frame;
    const unsigned int size = (unsigned int)(((float)GUI_CODE_SIZE * frame->scale) + 0.5f);
    const float cell = gui_glyphs_cell(&window->glyphs, size);
    GuiRect anchor;
    GuiRect last;
    if (window->hover.shown)
    {
        const unsigned long long end =
            (window->hover.to > window->hover.from) ? window->hover.to - 1ull : window->hover.from;
        if (gui_editor_place_rect(&window->editor, frame->editor, cell, frame->scale, window->hover.line,
                                  window->hover.from, &anchor) &&
            gui_editor_place_rect(&window->editor, frame->editor, cell, frame->scale, window->hover.line, end, &last))
        {
            gui_hover_drawn(&window->hover, window->raster, &window->glyphs, window->palette, anchor, last,
                            frame->inside, frame->scale);
        }
        else
        {
            gui_hover_hidden(&window->hover);
        }
    }
    if (window->suggest.open)
    {
        if (gui_editor_place_rect(&window->editor, frame->editor, cell, frame->scale, window->suggest.line,
                                  window->suggest.from, &anchor))
        {
            gui_suggest_drawn(&window->suggest, window->raster, &window->glyphs, window->palette, anchor, frame->inside,
                              frame->scale);
        }
        else
        {
            gui_suggest_closed(&window->suggest);
        }
    }
    if (window->signature.shown && window->editor.has_cursor)
    {
        if (gui_editor_place_rect(&window->editor, frame->editor, cell, frame->scale, window->editor.cursor.line,
                                  window->editor.cursor.col, &anchor))
        {
            gui_signature_drawn(&window->signature, window->raster, &window->glyphs, window->palette, anchor,
                                frame->inside, frame->scale);
        }
        else
        {
            gui_signature_hidden(&window->signature);
        }
    }
}

// the view drawn: the tabs row, the editor with the map beside it, and the side where one is shown, each on the ground
// the base laid it, and the panels over them
static void window_view_drawn(GuiWindow *window)
{
    const GuiFrame *const frame = &window->frame;
    const GuiPalette *const palette = window->palette;
    GuiRaster *const raster = window->raster;
    gui_tabs_drawn(raster, &window->glyphs, palette, frame, window->tab, window->tabs, window->active);
    gui_editor_drawn(&window->editor, raster, &window->glyphs, palette, frame->editor, frame->scale);
    // the shown file's diagnostics underlined on the rows drawn
    const unsigned int code_size = (unsigned int)(((float)GUI_CODE_SIZE * frame->scale) + 0.5f);
    gui_problems_underlined(&window->problems, window->path, &window->editor, raster, palette, frame->editor,
                            gui_glyphs_cell(&window->glyphs, code_size), frame->scale);
    gui_minimap_drawn(&window->minimap, raster, palette, &window->editor, frame->map, frame->scale,
                      window_on_screen(window));
    if (!gui_rect_empty(frame->side) && (window->strip.shown >= 0))
    {
        // the tool shown is among the tools
        const GuiTool *const tool = gui_strip_tool((unsigned int)window->strip.shown);
        gui_explorer_drawn(&window->explorer, raster, &window->glyphs, &window->icons, palette, frame, tool->label,
                           cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = "explorer", .cap = 32ull}),
                           window->title, window->path);
        if (window_tool_named(window, "problems"))
        {
            gui_problems_drawn(&window->problems, raster, &window->glyphs, palette, frame);
        }
    }
    window_panels_drawn(window);
}

void gui_window_drawn(GuiWindow *window)
{
    const GuiFrame *const frame = &window->frame;
    const GuiPalette *const palette = window->palette;
    GuiRaster *const raster = window->raster;
    window->owed = 0;
    window->caret_alone = 0;
    window->busy_alone = 0;
    if (raster->pixels == NULL)
    {
        return;
    }
    // the base: the lattice as it stands, where its raster is the window's size, else the deep color, with the
    // glass sheet about the work, the work's parts' grounds and the panes that have slid in laid over it in one
    // pass through the map; the controls' pane is the base's once it stands; while the name's pane stands the view is
    // not drawn and the lattice shows through the work
    const GuiRaster *ground = &window->ground.raster;
    if ((ground->pixels == NULL) || (ground->width != raster->width) || (ground->height != raster->height))
    {
        ground = NULL;
    }
    const int view_shown = !window->wordmark.stands;
    // a stage of the claim's movement brings its map and its holes; only a frame at rest lays them
    const GuiStage *const stage = view_shown ? window->stage : NULL;
    if (stage != NULL)
    {
        gui_base_painted(&window->base, palette);
        gui_base_map_drawn(&window->base, &stage->map, raster, ground, palette->deep);
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
        (void)gui_base_laid(&window->base, based, palette, frame->controls_pane_share >= GUI_WHOLE);
        gui_base_drawn(&window->base, raster, ground, palette->deep);
    }
    // the table the panes lie on: the lattice with the sheet's tint and grain; each pane's corners rounded on it
    const GuiUnder table = gui_glass_table(ground, palette->deep, palette->glass);
    const GuiColor pane = gui_glass_pane_tint(palette->glass);
    gui_raster_paned(raster, table, gui_rect_met(frame->bar_slat, frame->bar), frame->pane_radius, pane, 1);
    gui_raster_paned(raster, table, gui_rect_met(frame->strip_slat, frame->strip), frame->pane_radius, pane, 1);
    gui_raster_paned(raster, table, gui_rect_met(frame->status_slat, frame->status), frame->pane_radius, pane, 1);
    // the work's outline: the lines it stands in, as the page draws them, the signal along its head, a thin line of
    // it down its left, the link darkened down its right and along its foot, bent round its corners and round the
    // holes the parts shown over the work make in it while the claim holds
    const GuiColor right_line =
        GUI_RGB((GUI_RED_OF(palette->link) * 65u) / 100u, (GUI_GREEN_OF(palette->link) * 65u) / 100u,
                (GUI_BLUE_OF(palette->link) * 65u) / 100u);
    const GuiOutline lines = {palette->signal,
                              right_line,
                              right_line,
                              (palette->signal & 0x00ffffffu) | (0x59u << 24u),
                              gui_scaled(GUI_WORK_PAD_TOP, frame->scale),
                              gui_scaled(GUI_WORK_PAD_RIGHT, frame->scale),
                              gui_scaled(GUI_WORK_PAD_BOTTOM, frame->scale),
                              gui_scaled(GUI_WORK_PAD_LEFT, frame->scale)};
    const GuiUnder clear = {ground, 0u, NULL, 0u, palette->deep};
    GuiRect holes_laid[4];
    const unsigned int count = (stage != NULL) ? stage->hole_count : gui_frame_holes(frame, holes_laid);
    gui_raster_outlined(raster, frame->work, frame->inside, frame->radius, &lines, (stage != NULL) ? stage->holes : holes_laid, count, table, clear);
    // the work's inside: the tabs row, the editor with the map beside it, and the side where one is shown, each on
    // the ground the base laid it
    if (view_shown)
    {
        window_view_drawn(window);
    }
    // the parts about the work, each on its pane: the strip, the bar with the menu bar's titles and the controls,
    // and the status bar
    gui_strip_drawn(raster, &window->icons, palette, frame, &window->strip);
    gui_bar_drawn(raster, &window->glyphs, palette, frame, window->title, table, &window->controls_shadow);
    gui_menubar_titles_drawn(&window->menubar, raster, &window->glyphs, palette, frame);
    const GuiStatus status = {window->path,
                              window->editor.has_cursor,
                              window->editor.cursor.line,
                              window->editor.cursor.col,
                              window->editor.anchor.line,
                              window->editor.anchor.col,
                              (window->active >= 0) ? window->tab[window->active].crlf : 0,
                              window->editor.indent_tabs,
                              window->editor.indent_size,
                              window->memory_bytes};
    gui_statusbar_drawn(raster, &window->glyphs, &window->icons, palette, frame, &status);
    // the large name as it stands or goes, the funnel over the whole window, and the eye over the bar and the strip
    const unsigned long long drawn_ms = cli_clock_milliseconds();
    gui_wordmark_drawn(&window->wordmark, raster, &window->glyphs, &window->icons, palette, frame, drawn_ms);
    gui_wordmark_eye_drawn(&window->wordmark, raster, &window->icons, palette, frame);
    // the menus open go over every part
    gui_menubar_menus_drawn(&window->menubar, raster, &window->glyphs, palette, frame);
    // the caret last, at the ink its breath stands at, where no panel or menu lies over it: drawn over the frame
    // whole, its breath's steps draw it again alone; none stands while the view is not drawn
    window->caret_ink = window_caret_ink(window, drawn_ms);
    window->editor.caret_ink = window->caret_ink;
    const GuiRect caret = gui_editor_caret_rect(&window->editor, &window->glyphs, frame->editor, frame->scale);
    if (view_shown && !gui_rect_empty(caret) && !window_caret_covered(window, caret))
    {
        (void)gui_editor_caret_drawn(&window->editor, raster, palette, caret);
    }
    else
    {
        window->editor.caret_kept = 0;
    }
    // the busy circle over all of it, the pixels under it kept for its stages' steps
    gui_busy_drawn(&window->busy, raster, palette, frame, drawn_ms);
}

unsigned int gui_window_hit(const GuiWindow *window, int x, int y)
{
    const GuiFrame *const frame = &window->frame;
    const int border = gui_scaled(GUI_BORDER, frame->scale);
    // the window's sides are far inside what an int represents
    const int wide = (int)frame->width;
    const int tall = (int)frame->height;
    const int left = x < border;
    const int right = x >= wide - border;
    const int top = y < border;
    const int bottom = y >= tall - border;
    if (!window->maximized && (left || right || top || bottom))
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
    const unsigned int control = gui_controls_hit(frame, x, y);
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
    if (gui_menubar_holds(&window->menubar, x, y) ||
        (window->wordmark.home && gui_rect_holds(gui_wordmark_eye_square(frame), x, y)))
    {
        return GUI_HIT_CLIENT;
    }
    return gui_rect_holds(frame->bar, x, y) ? GUI_HIT_CAPTION : GUI_HIT_CLIENT;
}

// the view's top moved `delta` lines, no further than the file's first and last lines, and the view told
static void window_scrolled(GuiWindow *window, long long delta)
{
    gui_editor_scrolled(&window->editor, delta, window_rows(window));
    window->owed = 1;
    char more[48];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"top\":");
    mmgr_line_decimal(&line, window->editor.top, 1u);
    (void)mmgr_line_finish(&line);
    window_view_input(window, CLI_VIEW_SCROLL, more);
}

// the view's top moved to `top`
static void window_scrolled_to(GuiWindow *window, unsigned long long top)
{
    const unsigned long long now = window->editor.top;
    // the lines between the tops are far inside what a long long represents
    window_scrolled(window, (top >= now) ? (long long)(top - now) : -(long long)(now - top));
}

void gui_window_wheel(GuiWindow *window, int x, int y, int rows)
{
    // while the name's pane stands no view takes the wheel
    if (window->wordmark.stands)
    {
        return;
    }
    // the wheel over the side scrolls the explorer's rows; over the editor or the map, the text
    if (gui_rect_holds(window->frame.side, x, y))
    {
        const int moved = window_tool_named(window, "problems")
                              ? gui_problems_wheeled(&window->problems, &window->frame, rows)
                              : gui_explorer_wheeled(&window->explorer, &window->frame, rows);
        window->owed = moved || window->owed;
        return;
    }
    // the wheel over the completion's list scrolls its rows
    if (gui_suggest_holds(&window->suggest, x, y))
    {
        window->owed = gui_suggest_wheeled(&window->suggest, rows) || window->owed;
        return;
    }
    if (!gui_rect_holds(window->frame.editor, x, y) && !gui_rect_holds(window->frame.map, x, y))
    {
        return;
    }
    window_panels_hidden(window);
    window_scrolled(window, rows);
}

// the pointer's event at x, y sent to the view where the pixel stands in the editor's text or its gutter: 1 where it
// was
static int window_pointed(GuiWindow *window, unsigned int pointer, int x, int y, unsigned int clicks, int shift)
{
    const unsigned int size = (unsigned int)(((float)GUI_CODE_SIZE * window->frame.scale) + 0.5f);
    const float cell = gui_glyphs_cell(&window->glyphs, size);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    int gutter = 0;
    if (!window->viewed)
    {
        return 0;
    }
    if (!gui_editor_cell_at(&window->editor, window->frame.editor, cell, window->frame.scale, x, y, &row, &column))
    {
        if (!gui_editor_row_at(&window->editor, window->frame.editor, cell, window->frame.scale, x, y, &row))
        {
            return 0;
        }
        gutter = 1;
    }
    char more[160];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"pointer\":");
    mmgr_line_decimal(&line, pointer, 1u);
    mmgr_line_put(&line, ",\"row\":");
    mmgr_line_decimal(&line, row, 1u);
    mmgr_line_put(&line, ",\"column\":");
    mmgr_line_decimal(&line, column, 1u);
    mmgr_line_put(&line, ",\"clicks\":");
    mmgr_line_decimal(&line, clicks, 1u);
    mmgr_line_put(&line, shift ? ",\"shift\":true" : ",\"shift\":false");
    mmgr_line_put(&line, gutter ? ",\"gutter\":true" : ",\"gutter\":false");
    (void)mmgr_line_finish(&line);
    window_view_input(window, CLI_VIEW_POINTER, more);
    return 1;
}

void gui_window_pressed(GuiWindow *window, int x, int y, unsigned int mods)
{
    const unsigned int hit = gui_window_hit(window, x, y);
    window->pressed =
        ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE)) ? hit : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
    // a press on a completion's row accepts its item; a press in the hover's box or the signature's is theirs; a
    // press anywhere else hides the panels and goes on
    if (gui_suggest_pressed(&window->suggest, x, y) == GUI_SUGGEST_ACCEPTED)
    {
        window_completed(window);
        return;
    }
    if (gui_suggest_holds(&window->suggest, x, y) || gui_hover_holds(&window->hover, x, y) ||
        gui_signature_holds(&window->signature, x, y))
    {
        return;
    }
    window_panels_hidden(window);
    // the menu bar takes a press on a title or in a menu; a press outside its menus closes them and goes on; a menu
    // open takes the keys from the editor, which lets the claim go
    GuiChoice choice;
    const int menus_open = window->menubar.levels != 0u;
    if (gui_menubar_pressed(&window->menubar, &window->glyphs, &window->frame, x, y, &choice))
    {
        window->owed = 1;
        if (window->menubar.levels != 0u)
        {
            window_claimed(window, 0);
        }
        if (choice.command[0] != '\0')
        {
            gui_window_chosen(window, &choice);
        }
        return;
    }
    window->owed = window->owed || menus_open;
    // a press on the name opens orior's repository, and the rest of the name's pane takes a press to no view
    if (gui_wordmark_holds(&window->wordmark, &window->frame, x, y))
    {
        char text[64];
        VerbaLinea args = {text, sizeof(text), 0ull};
        mmgr_line_put(&args, "{\"url\":\"" GUI_WORDMARK_REPOSITORY "\"}");
        (void)window_called(window, 1, "report_open", &args);
        return;
    }
    if (window->wordmark.stands && gui_rect_holds(window->frame.work, x, y))
    {
        return;
    }
    // a press on the strip shows a tool's window or closes it, the keys going to it
    if (gui_strip_pressed(&window->strip, &window->frame, x, y))
    {
        window_claimed(window, 0);
        window_sided(window);
        return;
    }
    // a press in the problems pane starts or stops the tree's check, closes or opens a file's rows, or opens a file
    // at a diagnostic's place
    GuiProblemPick problem;
    if (window_tool_named(window, "problems") &&
        gui_problems_pressed(&window->problems, &window->frame, x, y, &problem))
    {
        window->owed = 1;
        window_claimed(window, 0);
        if ((problem.kind == GUI_PROBLEMS_PICK_CHECK) || (problem.kind == GUI_PROBLEMS_PICK_STOP))
        {
            char args_text[8];
            VerbaLinea args = {args_text, sizeof(args_text), 0ull};
            mmgr_line_put(&args, "{}");
            window->asked_check = window_called(
                window, 0, (problem.kind == GUI_PROBLEMS_PICK_CHECK) ? "problems_check" : "problems_check_stop", &args);
        }
        else if (problem.kind == GUI_PROBLEMS_PICK_OPEN)
        {
            window_opened_at(window, problem.path, problem.line, problem.col);
        }
        return;
    }
    // a press in the side opens or closes a folder, asking for its listing the first time, or opens a file
    GuiPick pick;
    if (gui_explorer_pressed(&window->explorer, &window->frame, x, y, &pick))
    {
        window->owed = 1;
        window_claimed(window, 0);
        if (pick.kind == GUI_PICK_LIST)
        {
            window_listing_asked(window, pick.path);
        }
        else if (pick.kind == GUI_PICK_OPEN)
        {
            gui_window_file_opened(window, pick.path);
        }
        return;
    }
    // a press on a tab shows its file, on its close mark closes it
    unsigned int at = 0u;
    int closing = 0;
    if (gui_tabs_hit(&window->glyphs, &window->frame, window->tab, window->tabs, x, y, &at, &closing))
    {
        if (closing)
        {
            gui_window_tab_closed(window, at);
        }
        else
        {
            gui_window_tab_shown(window, at);
        }
        return;
    }
    // a press on the map goes to that part of the text and may begin the slider's drag
    unsigned long long top = 0ull;
    if (gui_minimap_pressed(&window->minimap, window->frame.map, window->frame.scale, &window->editor,
                            window_on_screen(window), x, y, &top))
    {
        window_scrolled_to(window, top);
        return;
    }
    // presses in a row within the time and near the place count up
    const unsigned long long now = cli_clock_milliseconds();
    const int near = ((x - window->clicked_x) <= GUI_CLICK_NEAR) && ((window->clicked_x - x) <= GUI_CLICK_NEAR) &&
                     ((y - window->clicked_y) <= GUI_CLICK_NEAR) && ((window->clicked_y - y) <= GUI_CLICK_NEAR);
    window->clicks = ((now - window->clicked_ms) <= GUI_CLICK_WITHIN) && near ? window->clicks + 1u : 1u;
    window->clicked_ms = now;
    window->clicked_x = x;
    window->clicked_y = y;
    window->dragging =
        window_pointed(window, CLI_VIEW_POINTER_DOWN, x, y, window->clicks, (mods & GUI_MOD_SHIFT) != 0u);
    // a press in the editor's text gives it the keys
    if (window->dragging)
    {
        window_claimed(window, 1);
    }
}

void gui_window_moved(GuiWindow *window, int x, int y)
{
    if (window->claim.claimed)
    {
        const GuiFrame *const frame = &window->frame;
        const unsigned long long now = cli_clock_milliseconds();
        const int near_before = window->claim.near;
        gui_claim_moved(&window->claim, x, y,
                        gui_claim_zone(frame->width, frame->height, frame->scale, frame->controls.width, x, y),
                        window_over_parts(window, x, y), now);
        if ((window->claim.near != near_before) || gui_claim_moving(&window->claim, now))
        {
            window->claim_moving = 1;
            window_claim_laid(window);
        }
    }
    if (gui_menubar_moved(&window->menubar, &window->glyphs, &window->frame, x, y, cli_clock_milliseconds()))
    {
        window->owed = 1;
    }
    if (gui_strip_moved(&window->strip, &window->frame, x, y))
    {
        window->owed = 1;
    }
    if (window_tool_named(window, "problems") ? gui_problems_moved(&window->problems, &window->frame, x, y)
                                              : gui_explorer_moved(&window->explorer, &window->frame, x, y))
    {
        window->owed = 1;
    }
    unsigned long long top = 0ull;
    if (gui_minimap_dragged(&window->minimap, y, &top))
    {
        if (top != window->editor.top)
        {
            window_scrolled_to(window, top);
        }
        return;
    }
    if (window->dragging)
    {
        (void)window_pointed(window, CLI_VIEW_POINTER_MOVED, x, y, window->clicks, 0);
        return;
    }
    // the pointer over the editor's text begins the hover's rest at its cell, where no menu is open and no drag runs
    const unsigned int size = (unsigned int)(((float)GUI_CODE_SIZE * window->frame.scale) + 0.5f);
    const float cell = gui_glyphs_cell(&window->glyphs, size);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    const int in_text =
        window->viewed && (window->menubar.levels == 0u) &&
        gui_editor_cell_at(&window->editor, window->frame.editor, cell, window->frame.scale, x, y, &row, &column);
    gui_hover_moved(&window->hover, in_text, row, column, x, y, cli_clock_milliseconds());
}

void gui_window_left(GuiWindow *window)
{
    const int near_before = window->claim.near;
    gui_claim_left(&window->claim);
    if (window->claim.near != near_before)
    {
        window->claim_moving = 1;
        window_claim_laid(window);
    }
}

void gui_window_released(GuiWindow *window, int x, int y)
{
    if (window->minimap.dragging)
    {
        gui_minimap_released(&window->minimap);
        window->owed = 1;
    }
    if (window->dragging)
    {
        window->dragging = 0;
        (void)window_pointed(window, CLI_VIEW_POINTER_UP, x, y, window->clicks, 0);
    }
}

void gui_window_key(GuiWindow *window, const char *name)
{
    // the menu bar takes the keys while a menu is open, and Alt with a title's letter; a menu open takes the keys from
    // the editor, which lets the claim go
    GuiChoice choice;
    if (gui_menubar_keyed(&window->menubar, &window->glyphs, &window->frame, name, &choice))
    {
        window->owed = 1;
        if (window->menubar.levels != 0u)
        {
            window_claimed(window, 0);
        }
        if (choice.command[0] != '\0')
        {
            gui_window_chosen(window, &choice);
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
    if (gui_menubar_keys_command(&window->menubar, name, &choice) && window_runs(choice.command))
    {
        gui_window_chosen(window, &choice);
        return;
    }
    if (gui_strip_keyed(&window->strip, name))
    {
        window_claimed(window, 0);
        window_panels_hidden(window);
        window_sided(window);
        return;
    }
    // the completion's list takes its keys while it is open; Escape closes the signature and the hover; Mod+Space
    // opens the list with nothing typed
    const unsigned int did = gui_suggest_keyed(&window->suggest, name);
    if (did == GUI_SUGGEST_ACCEPTED)
    {
        window_completed(window);
        return;
    }
    if (did != GUI_SUGGEST_NONE)
    {
        window->owed = 1;
        return;
    }
    const int escape = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Escape", .cap = 64ull});
    if (escape && (window->signature.shown || window->hover.shown))
    {
        window_panels_hidden(window);
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Mod+Space", .cap = 64ull}))
    {
        window->complete_forced = 1;
        window_complete_asked(window, 0);
        return;
    }
    window_claimed(window, 1);
    gui_hover_hidden(&window->hover);
    // a key that takes letters out keeps the list open on the prefix left; any other key closes it
    if (window->suggest.open)
    {
        const int edits = cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Backspace", .cap = 64ull}) ||
                          cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Delete", .cap = 64ull});
        window->complete_wanted = edits;
        if (!edits)
        {
            gui_suggest_closed(&window->suggest);
        }
    }
    // a key may edit the text, which the map's lines then no longer show and the server is told
    window->minimap.stale = 1;
    window->told_wanted = 1;
    char more[96];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"name\":");
    cli_json_quoted(&line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64ull}));
    (void)mmgr_line_finish(&line);
    window_view_input(window, CLI_VIEW_KEY, more);
}

void gui_window_text(GuiWindow *window, const char *text, unsigned long long size)
{
    // while the name's pane stands no view takes the text
    if (window->wordmark.stands)
    {
        return;
    }
    char more[CLI_PATH_MAX_LEN];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"text\":");
    cli_json_quoted(&line, text, size);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    window_claimed(window, 1);
    gui_hover_hidden(&window->hover);
    // a word's letter typed opens the completion's list or filters it as the frame comes; any other letter closes
    // it; a bracket or a comma begins the signature's wait
    if ((size > 0ull) && window_word_byte(text[size - 1ull]))
    {
        window->complete_wanted = 1;
    }
    else if (window->suggest.open)
    {
        gui_suggest_closed(&window->suggest);
        window->complete_wanted = 0;
    }
    (void)gui_signature_typed(&window->signature, text, size, cli_clock_milliseconds());
    window->minimap.stale = 1;
    window->told_wanted = 1;
    window_view_input(window, CLI_VIEW_TEXT, more);
}
