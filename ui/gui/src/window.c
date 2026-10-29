// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: the window apart from the system. A file opens through branch_open, a text read whole and a file too large
// to read whole alike, and is shown through the CLI's editor view, which answers each input with the frame drawn; the
// walk's colors come as branch-colors events. The minimap's lines are asked of editor_view_map for the span the map
// stands for, after a frame moves the span or a key, text or the walk's colors make the lines held stale, one ask in
// flight at a time
#include "window.h"

#include "../../cli/src/editor/view.h"
#include "answer.h"
#include "parts/chrome.h"
#include "system.h"

// the head of the key a file opened is stored under
#define GUI_OPEN_KEY_HEAD "window:"

int gui_window_begun(GuiWindow *window, GuiLinkWoken woken, void *context, VerbaLinea *err_msg)
{
    memor.set(&(MemoriaCfg){.dst = window, .val = 0, .bytes = sizeof(*window)});
    window->palette = gui_palette(GUI_SCHEME_DARK);
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

void gui_window_ended(GuiWindow *window)
{
    gui_link_ended(&window->link);
    gui_editor_given(&window->editor);
    if (window->cataloged)
    {
        cli_json_given(&window->catalog);
        window->cataloged = 0;
    }
    gui_glyphs_given(&window->glyphs);
    gui_icons_given(&window->icons);
    gui_explorer_given(&window->explorer);
    gui_minimap_given(&window->minimap);
    gui_raster_given(&window->raster);
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
    return gui_link_called(&window->link, own, name, args->out, mmgr_line_ok(args) ? args->at : 0ull);
}

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

// the map's lines asked for where the map placed beside the view stands for lines it does not hold or holds stale,
// one ask in flight at a time, the next wanted as the answer comes
static void window_map_asked(GuiWindow *window)
{
    if (!window->viewed || gui_rect_empty(window->frame.map))
    {
        return;
    }
    gui_minimap_placed(&window->minimap, window->frame.map, window->frame.scale, &window->editor, window_on_screen(window));
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

// the frame laid for the claim's numbers now, the menu bar's titles laid over the bar again, and a frame owed
static void window_claim_laid(GuiWindow *window)
{
    GuiClaimNumbers numbers;
    gui_claim_numbers(&window->claim, cli_clock_milliseconds(), &numbers);
    gui_frame_claimed(&window->frame, &numbers);
    gui_menubar_laid(&window->menubar, &window->glyphs, &window->frame);
    window->owed = 1;
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

// the work's inside laid again as the strip's tool shown has a side or not, the view told the editor's size, and the
// top folder's listing asked for the first time the side shows
static void window_sided(GuiWindow *window)
{
    const int side = window_side_shown(window);
    gui_frame_sided(&window->frame, side);
    window->owed = 1;
    window_view_sized(window);
    if (side && !gui_explorer_root_asked(&window->explorer))
    {
        window_listing_asked(window, "");
    }
}

int gui_window_sized(GuiWindow *window, unsigned int width, unsigned int height, float scale)
{
    gui_frame_laid(&window->frame, width, height, scale);
    gui_frame_sided(&window->frame, window_side_shown(window));
    window_claim_laid(window);
    gui_raster_given(&window->raster);
    const int made = gui_raster_made(&window->raster, width, height);
    window_view_sized(window);
    return made;
}

void gui_window_ticked(GuiWindow *window)
{
    const unsigned long long now = cli_clock_milliseconds();
    (void)gui_claim_stepped(&window->claim, now);
    const int moving = gui_claim_moving(&window->claim, now);
    window_claim_laid(window);
    if (window->claim_moving && !moving)
    {
        // the slides at rest, the view is told the editor's size and the map its span
        window_view_sized(window);
        window_map_asked(window);
    }
    window->claim_moving = moving;
}

unsigned long long gui_window_due(const GuiWindow *window)
{
    return gui_claim_due(&window->claim, cli_clock_milliseconds());
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
    const unsigned int string = cli_json_taken(&quoted, text, size, 0u, 0ull, &err_msg);
    unsigned long long inner_size = 0ull;
    const char *const inner = cli_json_string(&quoted, string, &inner_size);
    const unsigned int root = (inner != NULL) ? cli_json_taken(&window->catalog, inner, inner_size, 0u, 0ull, &err_msg) : CLI_JSON_NONE;
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
}

// the catalog's commands the window runs itself; whether `command` is among them
static const char *const s_window_commands[] = {"exit", "close-editor", "close-all", "last-editor"};

static int window_runs(const char *command)
{
    for (unsigned int at = 0u; at < sizeof(s_window_commands) / sizeof(s_window_commands[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = s_window_commands[at], .cap = 256ull}))
        {
            return 1;
        }
    }
    return 0;
}

// a menu's choice run: an item with keys runs as those keys pressed, the editor's view taking them; `exit` closes the
// window as the close control does; `close-editor` closes the tab shown, `close-all` every tab, and `last-editor`
// shows the tab shown before
static void window_chosen(GuiWindow *window, const GuiChoice *choice)
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
    if (choice->keys[0] != '\0')
    {
        gui_window_key(window, choice->keys);
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
    gui_editor_made(&window->editor);
    gui_minimap_cleared(&window->minimap);
    window->owed = 1;
}

void gui_window_tab_shown(GuiWindow *window, unsigned int at)
{
    if (at >= window->tabs)
    {
        return;
    }
    window_tab_taken(window, at);
    // the view open on the file answers its frame as it is told the editor's size
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
        (void)window_called(window, 1, "editor_view_close", &args);
    }
    (void)window_called(window, 1, "document_release", &args);
    for (unsigned int after = at + 1u; after < window->tabs; after += 1u)
    {
        window->tab[after - 1u] = window->tab[after];
    }
    window->tabs -= 1u;
    // the tabs are fewer than an int counts
    window->last_active = ((window->last_active > (int)at) ? window->last_active - 1 : (window->last_active == (int)at) ? -1 : window->last_active);
    window->owed = 1;
    if (window->tabs == 0u)
    {
        window->active = -1;
        window->path[0] = '\0';
        window->key[0] = '\0';
        window->viewed = 0;
        window->frame_seq = 0ull;
        window->menubar.has_editor = 0;
        gui_editor_made(&window->editor);
        gui_minimap_cleared(&window->minimap);
        window_claimed(window, 0);
        return;
    }
    if ((int)at == window->active)
    {
        window->active = -1;
        gui_window_tab_shown(window, (at < window->tabs) ? at : window->tabs - 1u);
    }
    else if ((int)at < window->active)
    {
        window->active -= 1;
    }
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
    window->asked_open = window_called(window, 1, "branch_open", &args);
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

// the open's answer taken: the grammar's names kept, and the text, read whole or paged, shown through the CLI's
// editor view, opened on it sized to the editor
static void window_opened(GuiWindow *window, const CliJson *json, unsigned int root)
{
    const unsigned int names = cli_json_member(json, root, "names");
    if (names != CLI_JSON_NONE)
    {
        (void)gui_editor_named(&window->editor, json, names);
    }
    const int paged = gui_answer_member_true(json, root, "windowed") &&
                      cli_json_is(json, cli_json_member(json, root, "shape"), CFG_JSON_OBJECT);
    // a text read whole says how its lines end: a carriage return before a newline, as the text or as its escape
    unsigned long long text_length = 0ull;
    const char *const whole = cli_json_string(json, cli_json_member(json, root, "text"), &text_length);
    if ((whole != NULL) && (window->active >= 0))
    {
        int crlf = 0;
        for (unsigned long long at = 0ull; (at + 1ull < text_length) && !crlf; at += 1ull)
        {
            crlf = ((whole[at] == '\r') && (whole[at + 1ull] == '\n')) ||
                   ((whole[at] == '\\') && (whole[at + 1ull] == 'r') && (at + 3ull < text_length) && (whole[at + 2ull] == '\\') && (whole[at + 3ull] == 'n'));
        }
        window->tab[window->active].crlf = crlf;
    }
    if (paged || (whole != NULL))
    {
        char text[(2u * CLI_PATH_MAX_LEN) + 128u];
        VerbaLinea args = {text, sizeof(text), 0ull};
        mmgr_line_put(&args, "{\"key\":");
        cli_json_quoted(&args, window->key, cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)}));
        mmgr_line_put(&args, ",\"path\":");
        cli_json_quoted(&args, window->path, cellul.len(&(CatenaFinitaCfg){.src = window->path, .cap = sizeof(window->path)}));
        mmgr_line_put(&args, ",\"rows\":");
        mmgr_line_decimal(&args, window_rows(window), 1u);
        mmgr_line_put(&args, ",\"columns\":");
        mmgr_line_decimal(&args, window_columns(window), 1u);
        mmgr_line_ch(&args, '}');
        window->asked_view = window_called(window, 1, "editor_view_open", &args);
    }
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
}

// whether an answer is a frame of the view, by what it carries
static int window_is_frame(const CliJson *json, unsigned int root)
{
    return (cli_json_member(json, root, "rows") != CLI_JSON_NONE) && (cli_json_member(json, root, "cursor") != CLI_JSON_NONE) &&
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
                        (memor.cmp(&(MemoriaCfg){.src = name, .other = "open-file", .bytes = 9u}) == 0) && (path != NULL);
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
    mmgr_line_put(&answer, opening ? ",\"done\":true,\"message\":\"\"}" : ",\"done\":false,\"message\":\"the window has no command ");
    if (!opening)
    {
        mmgr_line_put_n(&answer, name, name_length);
        mmgr_line_put(&answer, "\"}");
    }
    (void)window_called(window, 0, "command_answer", &answer);
}

// one answer or event taken
static void window_taken(void *context, unsigned long long ticket, const char *name, int ok, const char *text,
                         unsigned long long size)
{
    GuiWindow *const window = (GuiWindow *)context;
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    CliJson json;
    const unsigned int root = ok ? cli_json_taken(&json, text, size, 0u, 0ull, &err_msg) : CLI_JSON_NONE;
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
        }
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
    else if ((ticket != 0ull) && (ticket == window->asked_open))
    {
        window->asked_open = 0ull;
        if (root != CLI_JSON_NONE)
        {
            window_opened(window, &json, root);
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
    else if ((ticket != 0ull) && (root != CLI_JSON_NONE) && window_is_frame(&json, root))
    {
        window->asked_view = (ticket == window->asked_view) ? 0ull : window->asked_view;
        window_frame_taken(window, &json, root);
        window_map_asked(window);
    }
    else if ((ticket != 0ull) && (ticket == window->asked_memory))
    {
        window->asked_memory = 0ull;
        if (ok && (root != CLI_JSON_NONE))
        {
            window->memory_bytes = gui_answer_member_whole(&json, cli_json_member(&json, root, "orior"), "working", 0ull);
        }
    }
    else if ((ticket != 0ull) && ok && (root != CLI_JSON_NONE) && gui_explorer_listed(&window->explorer, ticket, &json, root))
    {
        window->owed = 1;
    }
    else if ((ticket == 0ull) && (root != CLI_JSON_NONE) &&
             cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "branch-colors", .cap = 64ull}))
    {
        unsigned long long length = 0ull;
        const char *const key = cli_json_string(&json, cli_json_member(&json, root, "key"), &length);
        if ((key != NULL) && (length == cellul.len(&(CatenaFinitaCfg){.src = window->key, .cap = sizeof(window->key)})) &&
            (memor.cmp(&(MemoriaCfg){.src = key, .other = window->key, .bytes = length}) == 0))
        {
            const unsigned int names = cli_json_member(&json, root, "names");
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
}

void gui_window_drawn(GuiWindow *window)
{
    const GuiFrame *const frame = &window->frame;
    const GuiPalette *const palette = window->palette;
    GuiRaster *const raster = &window->raster;
    window->owed = 0;
    if (raster->pixels == NULL)
    {
        return;
    }
    gui_raster_filled(raster, (GuiRect){0, 0, (int)raster->width, (int)raster->height}, palette->ground);
    // the work's inside: the side where one is shown, the tabs row, the editor with the map beside it, the corners
    // cut round, and the holes the parts shown over the work make in it while the claim holds
    gui_tabs_drawn(raster, &window->glyphs, palette, frame, window->tab, window->tabs, window->active);
    gui_editor_drawn(&window->editor, raster, &window->glyphs, palette, frame->editor, frame->scale);
    gui_minimap_drawn(&window->minimap, raster, palette, &window->editor, frame->map, frame->scale, window_on_screen(window));
    if (!gui_rect_empty(frame->side) && (window->strip.shown >= 0))
    {
        // the tool shown is among the tools
        const GuiTool *const tool = gui_strip_tool((unsigned int)window->strip.shown);
        gui_explorer_drawn(&window->explorer, raster, &window->glyphs, &window->icons, palette, frame, tool->label,
                           cellul.eq(&(CatenaFinitaCfg){.src = tool->name, .other = "explorer", .cap = 32ull}), window->title,
                           window->path);
    }
    gui_raster_corners_cut(raster, frame->inside, frame->radius, palette->ground);
    GuiRect holes[4];
    const unsigned int count = gui_frame_holes(frame, holes);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        gui_raster_notched(raster, frame->inside, holes[at], frame->radius, palette->ground);
    }
    // the parts about the work, each on its pane: the strip with the frame's gap column, the bar with the menu bar's
    // titles and the controls, and the status bar
    gui_strip_drawn(raster, &window->icons, palette, frame, &window->strip);
    gui_bar_drawn(raster, &window->glyphs, palette, frame, window->title);
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
    // the menus open go over every part
    gui_menubar_menus_drawn(&window->menubar, raster, &window->glyphs, palette, frame);
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
    // the menu bar's titles and its open menus are the client's, not the caption's
    if (gui_menubar_holds(&window->menubar, x, y))
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
    // the wheel over the side scrolls the explorer's rows; over the editor or the map, the text
    if (gui_rect_holds(window->frame.side, x, y))
    {
        window->owed = gui_explorer_wheeled(&window->explorer, &window->frame, rows) || window->owed;
        return;
    }
    if (!gui_rect_holds(window->frame.editor, x, y) && !gui_rect_holds(window->frame.map, x, y))
    {
        return;
    }
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
    window->pressed = ((hit == GUI_HIT_MINIMIZE) || (hit == GUI_HIT_MAXIMIZE) || (hit == GUI_HIT_CLOSE))
                          ? hit
                          : GUI_HIT_CLIENT;
    if (hit != GUI_HIT_CLIENT)
    {
        return;
    }
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
            window_chosen(window, &choice);
        }
        return;
    }
    window->owed = window->owed || menus_open;
    // a press on the strip shows a tool's window or closes it, the keys going to it
    if (gui_strip_pressed(&window->strip, &window->frame, x, y))
    {
        window_claimed(window, 0);
        window_sided(window);
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
    window->dragging = window_pointed(window, CLI_VIEW_POINTER_DOWN, x, y, window->clicks, (mods & GUI_MOD_SHIFT) != 0u);
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
        gui_claim_moved(&window->claim, x, y, gui_claim_zone(frame->width, frame->height, frame->scale, frame->controls.width, x, y),
                        window_over_parts(window, x, y), now);
        if ((window->claim.near != near_before) || gui_claim_moving(&window->claim, now))
        {
            window->claim_moving = 1;
            window_claim_laid(window);
        }
    }
    if (gui_menubar_moved(&window->menubar, &window->glyphs, &window->frame, x, y))
    {
        window->owed = 1;
    }
    if (gui_strip_moved(&window->strip, &window->frame, x, y))
    {
        window->owed = 1;
    }
    if (gui_explorer_moved(&window->explorer, &window->frame, x, y))
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
            window_chosen(window, &choice);
        }
        return;
    }
    // the keys of a command the window runs itself run it, a tool's keys show its window; every other key is the
    // view's, which takes the keys and makes the claim
    if (gui_menubar_keys_command(&window->menubar, name, &choice) && window_runs(choice.command))
    {
        window_chosen(window, &choice);
        return;
    }
    if (gui_strip_keyed(&window->strip, name))
    {
        window_claimed(window, 0);
        window_sided(window);
        return;
    }
    window_claimed(window, 1);
    // a key may edit the text, which the map's lines then no longer show
    window->minimap.stale = 1;
    char more[96];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"name\":");
    cli_json_quoted(&line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64ull}));
    (void)mmgr_line_finish(&line);
    window_view_input(window, CLI_VIEW_KEY, more);
}

void gui_window_text(GuiWindow *window, const char *text, unsigned long long size)
{
    char more[CLI_PATH_MAX_LEN];
    VerbaLinea line = {more, sizeof(more), 0ull};
    mmgr_line_put(&line, ",\"text\":");
    cli_json_quoted(&line, text, size);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    window_claimed(window, 1);
    window->minimap.stale = 1;
    window_view_input(window, CLI_VIEW_TEXT, more);
}
