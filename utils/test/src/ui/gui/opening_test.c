// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening_test.c: the window opening a file through the CLI, no system under it: the CLI's side begun on the tree the
// check runs in, a file of the tree opened by its path from the tree's top and by its whole path, its lines stored
// and the first drawn; a door command to open a file, as the CLI emits one, taken the same way and answered; the
// tabs and the one shown kept in orior's folder, the checks' own, and brought back by a window started again
#include "../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../ui/cli/src/machine/state.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/gui/src/window.h"
#include "../cli/cli_check.h"

// the wake the link gives the window's thread, counted
typedef struct
{
    CliLock lock;
    CliWake wake;
    unsigned int woken;
} Woken;

static void woken(void *context)
{
    Woken *const count = (Woken *)context;
    cli_lock_take(&count->lock);
    count->woken += 1u;
    cli_wake_all(&count->wake);
    cli_lock_give(&count->lock);
}

// the window's thread draining each wake until the editor holds lines or five seconds pass: whether it does
static int lines_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->editor.count > 0u)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the cursor stands at `line`, `col`, or the view's top at `top`, or
// five seconds pass
static int cursor_came_at(GuiWindow *window, Woken *count, unsigned long long line, unsigned long long col)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->editor.has_cursor && (window->editor.cursor.line == line) && (window->editor.cursor.col == col))
        {
            return 1;
        }
    }
    return 0;
}

static int cursor_came(GuiWindow *window, Woken *count, unsigned long long col)
{
    return cursor_came_at(window, count, 0ull, col);
}

static int top_came(GuiWindow *window, Woken *count, unsigned long long top)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->editor.top == top)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the catalog came, or five seconds pass
static int catalog_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->cataloged)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until every ask it has made is answered, or five seconds pass
static int asks_answered(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        unsigned long long since = 0ull;
        if (!gui_busy_waiting(&window->busy, &since))
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the explorer holds rows, or five seconds pass
static int rows_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (gui_explorer_rows(&window->explorer) > 0u)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until what orior's folder keeps names `needle`, or five seconds pass
static int kept_came(GuiWindow *window, Woken *count, const char *needle)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(folder))
    {
        return 0;
    }
    const unsigned long long wanted = cellul.len(&(CatenaFinitaCfg){.src = needle, .cap = CLI_PATH_MAX_LEN});
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        const unsigned long long line_room = 1ull << 20u;
        char *const line_cell = g_cli_cells->temporary_buf_alloc(line_room, &g_checks_cells);
        VerbaLinea line = mmgr_line_over(line_cell, line_room);
        if (line.out == NULL)
        {
            return 0;
        }
        unsigned long long at = 0ull;
        const int found = cli_state_read(folder, &line) && mmgr_line_ok(&line) &&
                          cli_text_find(line.out, line.at, needle, wanted, &at);
        g_cli_cells->persistent_buf_release(line.out, &g_checks_cells);
        if (found)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the tree's kept tabs are back and `tabs` of them stand, or five
// seconds pass
static int tabs_came(GuiWindow *window, Woken *count, unsigned int tabs)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->restored && (window->tabs == tabs))
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the hover is shown, the completion's list open or the signature
// shown, or twenty seconds pass, the language server's first answers taking their time
static int hover_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 2000u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->hover.shown)
        {
            return 1;
        }
    }
    return 0;
}

static int suggest_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 2000u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->suggest.open)
        {
            return 1;
        }
    }
    return 0;
}

static int signature_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 2000u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (window->signature.shown)
        {
            return 1;
        }
    }
    return 0;
}

// the window's thread draining each wake until the shown tab's view is served, or twenty seconds pass
static int served_came(GuiWindow *window, Woken *count)
{
    for (unsigned int left = 2000u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if ((window->active >= 0) && window->tab[window->active].served)
        {
            return 1;
        }
    }
    return 0;
}

// the session the system names answered and read against the one kept: the name standing or the eye home, 0 where
// neither came within ten seconds
static int name_came(GuiWindow *window, Woken *count)
{
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while (!window->wordmark.stands && !window->wordmark.home && (cli_clock_milliseconds() < until))
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
    }
    return window->wordmark.stands || window->wordmark.home;
}

// the window's thread waiting `ms` milliseconds, draining each wake meanwhile
static void waited(GuiWindow *window, Woken *count, unsigned long long ms)
{
    const unsigned long long until = cli_clock_milliseconds() + ms;
    while (cli_clock_milliseconds() < until)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
    }
}

// whether `length` bytes of `text` contain `needle`
static int contains(const char *text, unsigned long long length, const char *needle)
{
    unsigned long long at = 0ull;
    return cli_text_find(text, length, needle, cellul.len(&(CatenaFinitaCfg){.src = needle, .cap = 256ull}), &at);
}

// whether the editor's line `line` contains `needle`
static int line_has(const GuiEditor *editor, unsigned int line, const char *needle)
{
    return (line < editor->count) &&
           contains(editor->bytes + editor->line[line].text, editor->line[line].length, needle);
}

// whether the completion's list shows an item labeled `label`
static int suggest_shows(const GuiSuggest *suggest, const char *label)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 256ull});
    for (unsigned int at = 0u; at < suggest->count; at += 1u)
    {
        const GuiItem *const item = &suggest->item[suggest->shown[at]];
        if ((item->label_length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = suggest->texts.out + item->label, .other = label, .bytes = length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the first row of the explorer that stands for a file, or -1
static int file_row(const GuiExplorer *explorer)
{
    for (unsigned int row = 0u; row < gui_explorer_rows(explorer); row += 1u)
    {
        unsigned int node = 0u;
        if (gui_explorer_row(explorer, row, &node) && !explorer->node[node].dir)
        {
            // the rows are fewer than an int counts
            return (int)row;
        }
    }
    return -1;
}

// The pane's row of the diagnostic whose message is `message`, -1 where it lists none: a language server reports on the
// files the check has open, and the pane lists theirs beside it
static int problem_row(const GuiProblems *problems, const char *message)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_NAME_MAX_LEN});
    const unsigned int rows = gui_problems_rows(problems);
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        unsigned int file = 0u;
        int item = GUI_PROBLEMS_NONE;
        if (gui_problems_row(problems, row, &file, &item) && (item != GUI_PROBLEMS_NONE) &&
            (problems->item[item].message_length == length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = problems->texts.out + problems->item[item].message, .other = message, .bytes = length}) == 0))
        {
            // the rows are fewer than an int counts
            return (int)row;
        }
    }
    return -1;
}

static int line_starts(const GuiEditor *editor, unsigned int line, const char *head)
{
    if (line >= editor->count)
    {
        return 0;
    }
    const char *const text = editor->bytes + editor->line[line].text;
    unsigned long long at = 0ull;
    while ((head[at] != '\0') && (at < editor->line[line].length) && (text[at] == head[at]))
    {
        at += 1ull;
    }
    return head[at] == '\0';
}

// whether the line `line` comes to begin with `head`, the frames drained as they come, within five seconds
static int line_came(GuiWindow *window, Woken *count, unsigned int line, const char *head)
{
    for (unsigned int left = 500u; left > 0u; left -= 1u)
    {
        cli_lock_take(&count->lock);
        (void)cli_wake_wait_for(&count->wake, &count->lock, 10ull);
        cli_lock_give(&count->lock);
        gui_window_drained(window);
        if (line_starts(&window->editor, line, head))
        {
            return 1;
        }
    }
    return 0;
}

// the first line of the editor that begins with `head`, or the editor's count where none does
static unsigned int line_beginning(const GuiEditor *editor, const char *head)
{
    unsigned int line = 0u;
    while ((line < editor->count) && !line_starts(editor, line, head))
    {
        line += 1u;
    }
    return line;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char why_text[1024];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    Woken count = {CLI_LOCK_INIT, CLI_WAKE_INIT, 0u};
    static GuiWindow window;
    if (!gui_window_begun(&window, woken, &count, &why))
    {
        mmgr_line_finish(&why);
        cli_check(0, why_text);
        return cli_checked();
    }
    cli_window_begun(gui_link_emitted, &window.link);
    cli_check(gui_window_sized(&window, 800u, 500u, 1.0f), "the window is sized");

    gui_window_file_opened(&window, "ui/gui/src/raster.c");
    cli_check(lines_came(&window, &count), "a file of the tree opened by its path from the top shows its lines");
    cli_check(line_starts(&window.editor, 0u, "// orior"), "the first line is the file's first");
    cli_check(window.editor.lines > 100ull, "the file's line count is the file's");
    cli_check(window.viewed && window.editor.has_cursor && (window.editor.cursor.line == 0ull) &&
                  (window.editor.cursor.col == 0ull),
              "the file is shown through the CLI's view, the cursor at its start");
    gui_window_drawn(&window);
    gui_window_key(&window, "Right");
    gui_window_key(&window, "Right");
    cli_check(cursor_came(&window, &count, 2ull),
              "two Right keys move the cursor two letters, the view answering frames");
    gui_window_key(&window, "Down");
    cli_check(cursor_came_at(&window, &count, 1ull, 2ull), "Down moves the cursor a line");
    gui_window_text(&window, "x", 1ull);
    cli_check(cursor_came_at(&window, &count, 1ull, 3ull) && line_starts(&window.editor, 1u, "//x"),
              "text typed is written into the file's branch and the frame shows it");
    gui_window_key(&window, "Mod+Z");
    cli_check(cursor_came_at(&window, &count, 1ull, 2ull) && line_starts(&window.editor, 1u, "// "),
              "undo takes the typing out");
    // a line edit of the menus with no keys of its own reaches the view by its name
    gui_window_key(&window, "Mod+L");
    cli_check(cursor_came_at(&window, &count, 2ull, 0ull), "Mod+L selects the line");
    const GuiChoice upper = {"upper-case", ""};
    gui_window_chosen(&window, &upper);
    cli_check(line_came(&window, &count, 1u, "// SPDX-LICENSE-IDENTIFIER"), "Transform to Uppercase, a command with no keys, reaches the view by its name");
    gui_window_key(&window, "Mod+Z");
    cli_check(line_came(&window, &count, 1u, "// SPDX-License-Identifier"), "undo takes it back");

    // the panels over the editor through the CLI: the pointer resting on a declared name shows its declaration, a
    // word's letters typed on a line opened inside a function open the completion's list with the file's words, and
    // a call's bracket typed shows the signature of the call
    const float cell = gui_glyphs_cell(&window.glyphs, GUI_CODE_SIZE);
    // the view served, the server is told the view's rows as they rest; the keys made the claim: its slide run, the
    // frame is laid for it before the pointer's place is read off it; and the tree's declarations are read once
    // before a word is asked about, the first read taking its time
    cli_check(served_came(&window, &count), "the view is handed to the file's language server as it opens");
    waited(&window, &count, GUI_CLAIM_SLIDE + 50ull);
    gui_window_ticked(&window);
    cli_check(window.view_told && (window.view_told_top == window.editor.top) && (window.view_moved_ms == 0ull),
              "the server is told the view the rows rest at");
    {
        static const char s_query[] = "{\"query\":\"gui_rect_empty\"}";
        const unsigned int room = cli_window_called(0, "symbols_find", s_query, sizeof(s_query) - 1u);
        const char *answer = NULL;
        unsigned long long answer_size = 0ull;
        cli_check(cli_window_answer(room, &answer, &answer_size),
                  "a search of the tree's declarations begins their first read");
        cli_window_given(room);
    }
    for (unsigned int left = 600u; (left > 0u) && cli_tree_unwalked(); left -= 1u)
    {
        waited(&window, &count, 100ull);
    }
    cli_check(!cli_tree_unwalked(), "the tree's declarations are read within a minute of the first search");
    // the file is a file of the tree: the declaration is found, and the statement two lines under it is pressed
    const unsigned int declared = line_beginning(&window.editor, "int gui_rect_empty(");
    const unsigned long long statement = (unsigned long long)declared + 2ull;
    cli_check(declared < window.editor.count, "the declaration's line is among the file's lines");
    GuiRect named;
    cli_check(gui_editor_place_rect(&window.editor, window.frame.editor, cell, 1.0f, declared, 6ull, &named),
              "the declaration's seventh cell is among the rows drawn");
    gui_window_moved(&window, named.x + 2, named.y + 5);
    cli_check(window.hover.in_text && (window.hover.row == declared) && (gui_window_due(&window) <= GUI_HOVER_RESTS),
              "the pointer in the text begins the hover's rest");
    waited(&window, &count, GUI_HOVER_RESTS + 20ull);
    gui_window_ticked(&window);
    cli_check(hover_came(&window, &count) && contains(window.hover.text, window.hover.length, "gui_rect_empty"),
              "the rest up, the hover shows the declaration of the name under the pointer");
    gui_window_drawn(&window);
    cli_check((window.hover.rect.height > 0) && (window.hover.rect.y >= named.y + named.height),
              "the hover's box is drawn under the word's row");
    cli_check(!window.suggest.open, "no completion stands open for the letter typed and taken back");
    gui_window_key(&window, "Escape");
    cli_check(!window.hover.shown, "Escape hides the hover");
    GuiRect start;
    cli_check(gui_editor_place_rect(&window.editor, window.frame.editor, cell, 1.0f, statement, 0ull, &start),
              "the statement's first cell is among the rows drawn");
    gui_window_pressed(&window, start.x + 1, start.y + 5, 0u);
    gui_window_released(&window, start.x + 1, start.y + 5);
    cli_check(cursor_came_at(&window, &count, statement, 0ull), "a press in the text places the cursor at its cell");
    gui_window_key(&window, "Enter");
    cli_check(cursor_came_at(&window, &count, statement + 1ull, 0ull), "Enter opens a line before the statement");
    gui_window_key(&window, "Up");
    cli_check(cursor_came_at(&window, &count, statement, 0ull), "Up stands on the line opened");
    gui_window_text(&window, "gui_re", 6ull);
    cli_check(cursor_came_at(&window, &count, statement, 6ull) && suggest_came(&window, &count),
              "a word's letters typed open the completion's list");
    cli_check(suggest_shows(&window.suggest, "gui_rect_met") && suggest_shows(&window.suggest, "gui_rect_empty") &&
                  cellul.eq(&(CatenaFinitaCfg){.src = window.suggest.prefix, .other = "gui_re", .cap = 16ull}),
              "the list shows the file's words that answer the prefix");
    gui_window_key(&window, "Escape");
    cli_check(!window.suggest.open, "Escape closes the list");
    gui_window_text(&window, "ct_met(", 7ull);
    cli_check(cursor_came_at(&window, &count, statement, 13ull) &&
                  line_has(&window.editor, (unsigned int)statement, "gui_rect_met("),
              "the call's name and its bracket are written");
    cli_check(gui_window_due(&window) <= GUI_SIGNATURE_WAIT, "the bracket typed begins the signature's wait");
    waited(&window, &count, GUI_SIGNATURE_WAIT + 20ull);
    gui_window_ticked(&window);
    cli_check(signature_came(&window, &count) &&
                  contains(window.signature.label, window.signature.length, "gui_rect_met") &&
                  (window.signature.params == 2u) && (window.signature.active == 0),
              "the wait up, the signature shows the call's declaration with its first parameter active");
    gui_window_drawn(&window);
    cli_check(window.signature.rect.height > 0, "the signature's box is drawn");
    gui_window_key(&window, "Escape");
    cli_check(!window.signature.shown, "Escape hides the signature");
    // the keys made the claim, which the ticks laid: let go, as a menu opening lets it go, the tabs row and the strip
    // stand again for the presses below; ticked as the slide begins and again as it rests, as the window's loop ticks
    // through a slide
    gui_claim_set(&window.claim, 0, cli_clock_milliseconds());
    gui_window_ticked(&window);
    waited(&window, &count, GUI_CLAIM_SLIDE + 50ull);
    gui_window_ticked(&window);
    cli_check(window.frame.tabs.height > 0, "the claim let go, the tabs row stands again");
    gui_window_wheel(&window, window.frame.editor.x + 100, window.frame.editor.y + 100, 3);
    cli_check(top_came(&window, &count, 3ull), "the wheel scrolls the view through the CLI");

    char whole[CLI_PATH_MAX_LEN];
    VerbaLinea path = {whole, sizeof(whole), 0ull};
    char root[CLI_PATH_MAX_LEN];
    cli_check(cli_serve_root_now(root), "the tree is open on the CLI's side");
    mmgr_line_put(&path, root);
    mmgr_line_put(&path, "/ui/gui/src/glyphs.c");
    (void)mmgr_line_finish(&path);
    gui_editor_made(&window.editor);
    gui_window_file_opened(&window, whole);
    cli_check(lines_came(&window, &count), "a file opened by its whole path shows its lines");
    cli_check(line_starts(&window.editor, 2u, "// glyphs.c"), "the third line names the file");

    gui_editor_made(&window.editor);
    static const char s_command[] = "{\"id\":\"1\",\"command\":\"open-file\",\"args\":[\"ui/gui/src/frame.c\"]}";
    gui_link_emitted(&window.link, "command", s_command, sizeof(s_command) - 1u);
    cli_check(lines_came(&window, &count), "a door command to open a file shows its lines");
    cli_check(line_starts(&window.editor, 2u, "// frame.c"), "the file the command names is shown");

    // the files opened are tabs: each open stands as its own, the last shown; a tab pressed shows its file again, one
    // open already is shown, and a tab closed shows the one beside it
    cli_check((window.tabs == 3u) && (window.active == 2) && window.tab[0].viewed && window.tab[1].viewed,
              "each file opened is a tab, the last one shown, the others' views kept open");
    const GuiRect first = gui_tab_rect(&window.glyphs, &window.frame, window.tab, window.tabs, 0u);
    gui_window_pressed(&window, first.x + 10, first.y + 10, 0u);
    cli_check((window.active == 0) && (window.last_active == 2),
              "a tab pressed shows its file, the one shown before kept");
    cli_check(lines_came(&window, &count) && top_came(&window, &count, 3ull) &&
                  line_starts(&window.editor, 0u, "// space"),
              "the frame of its view comes again, the view where the wheel left it");
    cli_check(cursor_came_at(&window, &count, statement, 13ull), "the cursor stands where it was left");
    gui_window_file_opened(&window, "ui/gui/src/frame.c");
    cli_check((window.tabs == 3u) && (window.active == 2), "a file open already is shown, not opened again");
    const GuiRect close = gui_tab_close_rect(&window.glyphs, &window.frame, window.tab, window.tabs, 2u);
    gui_window_pressed(&window, close.x + (close.width / 2), close.y + 10, 0u);
    cli_check((window.tabs == 2u) && (window.active == 1) && lines_came(&window, &count) &&
                  line_starts(&window.editor, 2u, "// glyphs.c"),
              "the tab shown closed shows the one before it");

    // the catalog's keys for the window's own commands: Ctrl+Tab shows the tab shown before, Ctrl+W closes the shown
    gui_window_started(&window);
    cli_check(catalog_came(&window, &count), "the catalog comes as the window starts");
    // the first start in the system's session, nothing kept in the checks' own folder, stands the name on its pane,
    // which takes the view's presses and keys; the pane let go for what the check drives through the view
    cli_check(name_came(&window, &count) && window.wordmark.stands && !window.wordmark.home,
              "the first start in the session stands the name on its pane");
    const unsigned int tabs_before = window.tabs;
    gui_window_key(&window, "Mod+W");
    cli_check(window.tabs == tabs_before, "while the pane stands the view takes no keys");
    window.wordmark.stands = 0;
    gui_wordmark_landed(&window.wordmark);
    gui_window_key(&window, "Mod+Tab");
    cli_check((window.active == 0) && (window.last_active == 1), "Mod+Tab shows the tab shown before");
    gui_editor_made(&window.editor);
    gui_window_key(&window, "Mod+W");
    cli_check((window.tabs == 1u) && (window.active == 0) && lines_came(&window, &count) &&
                  line_starts(&window.editor, 2u, "// glyphs.c"),
              "Mod+W closes the tab shown, the one left shown");

    // every ask the window has made through all of this comes to be answered, which lets the busy circle go; a ticket the circle
    // kept and no answer reached would leave it swirling for good
    cli_check(asks_answered(&window, &count), "every ask the window has made is answered, the busy circle's count of them coming to none");
    gui_window_ticked(&window);
    cli_check((window.busy.shown_ms == 0ull) || !gui_busy_ticked(&window.busy, cli_clock_milliseconds() + GUI_BUSY_STANDS), "and it goes, if it came");
    cli_check(!window.busy.shown, "no circle stands with nothing asked");
    // the tree's check runs on past its ask's answer, its progress coming as events: the circle's wait runs from the first until
    // the last says it is done
    unsigned long long check_since = 0ull;
    static const char s_check_running[] = "{\"done\":3,\"total\":10}";
    gui_link_emitted(&window.link, "tree-check", s_check_running, sizeof(s_check_running) - 1ull);
    gui_window_drained(&window);
    cli_check(gui_busy_waiting(&window.busy, &check_since), "the tree's check running stands as an ask does");
    static const char s_check_done[] = "{\"done\":10,\"total\":10}";
    gui_link_emitted(&window.link, "tree-check", s_check_done, sizeof(s_check_done) - 1ull);
    gui_window_drained(&window);
    cli_check(asks_answered(&window, &count), "its last file done, nothing stands");

    // a menu opened by its keys while the claim holds lets the claim go and stays open through the slide, under the bar
    // as the bar comes back; the window sized closes it
    gui_window_key(&window, "Right");
    waited(&window, &count, GUI_CLAIM_SLIDE + 50ull);
    gui_window_ticked(&window);
    cli_check(window.claim.claimed && !gui_claim_moving(&window.claim, cli_clock_milliseconds()), "a key the view takes makes the claim");
    const int bar_away = window.frame.bar.y + window.frame.bar.height;
    gui_window_key(&window, "Alt+F");
    const GuiRect menu_opened = window.menubar.menu[0].rect;
    cli_check((window.menubar.levels != 0u) && !window.claim.claimed, "a menu opened by its keys lets the claim go and stays open");
    waited(&window, &count, GUI_CLAIM_SLIDE / 2ull);
    gui_window_ticked(&window);
    cli_check(window.menubar.levels != 0u, "partway through the slide it stands open");
    waited(&window, &count, GUI_CLAIM_SLIDE);
    gui_window_ticked(&window);
    gui_window_ticked(&window);
    cli_check((window.menubar.levels != 0u) && (window.menubar.menu[0].rect.y == window.frame.bar.y + window.frame.bar.height + gui_scaled(GUI_MENU_BELOW, window.frame.scale)),
              "at rest it stands open under the bar come back");
    cli_check((window.menubar.menu[0].rect.y > menu_opened.y) && (window.frame.bar.y + window.frame.bar.height > bar_away),
              "the menu came down with the bar");
    gui_window_key(&window, "Down");
    cli_check(window.menubar.menu[0].pointed >= 0, "the keys step its rows as it stands");
    cli_check(gui_window_sized(&window, 820u, 500u, 1.0f) && (window.menubar.levels == 0u), "the window sized closes it");
    cli_check(gui_window_sized(&window, 800u, 500u, 1.0f), "the window sized back");
    gui_window_key(&window, "Alt+F");
    gui_window_key(&window, "Escape");
    cli_check(window.menubar.levels == 0u, "Escape closes it");

    // the explorer: the strip's explorer tool pressed opens the side with the tree's files, and a file pressed there
    // opens as a tab
    const GuiRect explorer_button = gui_strip_button(&window.frame, 0u);
    gui_window_pressed(&window, explorer_button.x + 10, explorer_button.y + 10, 0u);
    cli_check((window.frame.side.width > 0) && (window.frame.editor.x == window.frame.side.x + window.frame.side.width),
              "the explorer's tool pressed opens the side beside the editor");
    cli_check(rows_came(&window, &count) && (file_row(&window.explorer) >= 0),
              "the tree's top folder is listed into the side, with a file among its rows");
    // the row is among the rows; the wheel over the side scrolls it into view where it stands past the rows shown
    gui_window_wheel(&window, window.frame.side.x + 10, window.frame.side.y + 100, file_row(&window.explorer));
    const unsigned int row = (unsigned int)file_row(&window.explorer);
    const GuiRect file = gui_explorer_row_rect(&window.frame, &window.explorer, row);
    cli_check(gui_rect_holds(window.frame.side, file.x + 40, file.y + 10),
              "the wheel over the side scrolls the row into view");
    unsigned int node = 0u;
    unsigned long long length = 0ull;
    const char *const stored_text =
        gui_explorer_row(&window.explorer, row, &node) ? gui_explorer_path(&window.explorer, node, &length) : "";
    char picked[CLI_PATH_MAX_LEN];
    VerbaLinea kept = {picked, sizeof(picked), 0ull};
    mmgr_line_put_n(&kept, stored_text, length);
    (void)mmgr_line_finish(&kept);
    gui_editor_made(&window.editor);
    gui_window_pressed(&window, file.x + 40, file.y + 10, 0u);
    cli_check(window.tabs == 2u, "a file pressed in the tree opens as a tab");
    cli_check(lines_came(&window, &count), "the file pressed shows its lines");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = window.tab[window.active].path, .other = picked, .cap = CLI_PATH_MAX_LEN}),
        "the tab shown is the file pressed");

    // the problems pane: a file's diagnostics come as an event, the strip's Problems tool is marked while an error
    // stands, the tool pressed shows the pane in the side, and a diagnostic's row pressed opens its file at its place.
    // The file is one no tab has opened: no language server is reporting on it and none clears what the event says
    static const char s_diagnostics[] = "{\"path\":\"ui/gui/src/"
                                        "palette.c\",\"items\":[{\"from\":{\"line\":13,\"col\":4},\"to\":{\"line\":13,"
                                        "\"col\":18},\"severity\":1,\"message\":\"a test's error\"}]}";
    gui_link_emitted(&window.link, "lsp-diagnostics", s_diagnostics, sizeof(s_diagnostics) - 1u);
    waited(&window, &count, 50ull);
    cli_check((problem_row(&window.problems, "a test's error") >= 0) && window.strip.problems_marked,
              "a file's diagnostics event is taken and the Problems tool marked");
    int problems_tool = -1;
    for (unsigned int at = 0u; at < gui_strip_tools(); at += 1u)
    {
        // the tools are fewer than an int counts
        problems_tool =
            cellul.eq(&(CatenaFinitaCfg){.src = gui_strip_tool(at)->name, .other = "problems", .cap = 32ull})
                ? (int)at
                : problems_tool;
    }
    cli_check(problems_tool >= 0, "the strip has the Problems tool");
    // the tool is among the tools
    const GuiRect problems_button = gui_strip_button(&window.frame, (unsigned int)problems_tool);
    gui_window_pressed(&window, problems_button.x + 10, problems_button.y + 10, 0u);
    cli_check((window.strip.shown == problems_tool) && (window.frame.side.width > 0),
              "the Problems tool pressed shows its pane in the side");
    gui_window_drawn(&window);
    const int problem_at = problem_row(&window.problems, "a test's error");
    cli_check(problem_at >= 0, "the pane lists the file and its diagnostic");
    // the row is among the rows; the wheel over the side scrolls it into view where it stands past the rows shown
    gui_window_wheel(&window, window.frame.side.x + 10, window.frame.side.y + 100, problem_at);
    const GuiRect problem_rect =
        gui_problems_row_rect(&window.problems, &window.frame, (problem_at >= 0) ? (unsigned int)problem_at : 0u);
    gui_editor_made(&window.editor);
    gui_window_pressed(&window, problem_rect.x + 40, problem_rect.y + 5, 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = window.tab[window.active].path, .other = "ui/gui/src/palette.c", .cap = CLI_PATH_MAX_LEN}),
              "a diagnostic's row pressed opens its file");
    cli_check(cursor_came_at(&window, &count, 13ull, 4ull), "the cursor stands at the diagnostic's place");
    gui_window_pressed(&window, explorer_button.x + 10, explorer_button.y + 10, 0u);
    cli_check(window.strip.shown == 0, "the explorer's tool pressed shows the tree again");

    // a file closed and opened again keeps its view as its close ends: another tab shown, then the file's again, its
    // lines come
    const unsigned int shown_tab = (unsigned int)window.active;
    gui_window_tab_shown(&window, 0u);
    cli_check(lines_came(&window, &count), "another tab shown shows its lines");
    gui_window_tab_shown(&window, shown_tab);
    cli_check(lines_came(&window, &count), "a file closed and opened again keeps its view as its close ends");

    // the tabs and the one shown are kept in orior's folder, the checks' own, as the page keeps them, and a window
    // started again on the tree brings them back with the one shown
    cli_check(kept_came(&window, &count, picked) && kept_came(&window, &count, "active"),
              "the tabs and the one shown are kept in orior's folder");
    // the last change is kept however soon the window ends after it
    gui_window_tab_shown(&window, 0u);
    gui_window_tab_shown(&window, shown_tab);
    gui_window_ended(&window);
    static GuiWindow again;
    const int begun_again = gui_window_begun(&again, woken, &count, &why) && gui_window_sized(&again, 800u, 500u, 1.0f);
    cli_check(begun_again, "a window begins again");
    if (!begun_again)
    {
        cli_window_ended();
        return cli_checked();
    }
    gui_window_started(&again);
    cli_check(tabs_came(&again, &count, 3u) && (again.active >= 0) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = again.tab[again.active].path, .other = "ui/gui/src/palette.c", .cap = CLI_PATH_MAX_LEN}),
              "the window started again brings the kept tabs back and shows the one kept shown");
    cli_check(lines_came(&again, &count), "the tab brought back shows its lines");
    cli_check(name_came(&again, &count) && again.wordmark.home && !again.wordmark.stands,
              "a start after it in the same session has the eye home at once");
    // View's Reduce Motion is kept in orior's folder and read before the name stands on the next start
    const GuiChoice reduce = {"reduce-motion", ""};
    gui_window_chosen(&again, &reduce);
    cli_check(again.motion_kept && kept_came(&again, &count, "reduce-motion"), "View's Reduce Motion is kept in orior's folder");
    gui_window_ended(&again);
    static GuiWindow third;
    const int begun_third = gui_window_begun(&third, woken, &count, &why) && gui_window_sized(&third, 800u, 500u, 1.0f);
    cli_check(begun_third, "a window begins a third time");
    if (!begun_third)
    {
        cli_window_ended();
        return cli_checked();
    }
    gui_window_started(&third);
    cli_check(name_came(&third, &count) && third.motion_kept && third.motion_still && third.claim.still && third.wordmark.still,
              "the window started again reads it before the name stands");
    gui_window_ended(&third);
    cli_window_ended();
    return cli_checked();
}
