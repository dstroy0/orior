// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening_test.c: the window opening a file through the CLI, no system under it: the CLI's side begun on the tree the
// check runs in, a file of the tree opened by its path from the tree's top and by its whole path, its lines held
// and the first drawn; and a door command to open a file, as the CLI emits one, taken the same way and answered
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
    cli_check(cursor_came(&window, &count, 2ull), "two Right keys move the cursor two letters, the view answering frames");
    gui_window_key(&window, "Down");
    cli_check(cursor_came_at(&window, &count, 1ull, 2ull), "Down moves the cursor a line");
    gui_window_text(&window, "x", 1ull);
    cli_check(cursor_came_at(&window, &count, 1ull, 3ull) && line_starts(&window.editor, 1u, "//x"),
              "text typed is written into the file's branch and the frame shows it");
    gui_window_key(&window, "Mod+Z");
    cli_check(cursor_came_at(&window, &count, 1ull, 2ull) && line_starts(&window.editor, 1u, "// "),
              "undo takes the typing out");
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
    cli_check((window.active == 0) && (window.last_active == 2), "a tab pressed shows its file, the one shown before kept");
    cli_check(lines_came(&window, &count) && top_came(&window, &count, 3ull) && line_starts(&window.editor, 0u, "// space"),
              "the frame of its view comes again, the view where the wheel left it");
    cli_check(cursor_came_at(&window, &count, 1ull, 2ull), "the cursor stands where it was left");
    gui_window_file_opened(&window, "ui/gui/src/frame.c");
    cli_check((window.tabs == 3u) && (window.active == 2), "a file open already is shown, not opened again");
    const GuiRect close = gui_tab_close_rect(&window.glyphs, &window.frame, window.tab, window.tabs, 2u);
    gui_window_pressed(&window, close.x + (close.width / 2), close.y + 10, 0u);
    cli_check((window.tabs == 2u) && (window.active == 1) && lines_came(&window, &count) && line_starts(&window.editor, 2u, "// glyphs.c"),
              "the tab shown closed shows the one before it");

    // the catalog's keys for the window's own commands: Ctrl+Tab shows the tab shown before, Ctrl+W closes the shown
    gui_window_started(&window);
    cli_check(catalog_came(&window, &count), "the catalog comes as the window starts");
    gui_window_key(&window, "Mod+Tab");
    cli_check((window.active == 0) && (window.last_active == 1), "Mod+Tab shows the tab shown before");
    gui_editor_made(&window.editor);
    gui_window_key(&window, "Mod+W");
    cli_check((window.tabs == 1u) && (window.active == 0) && lines_came(&window, &count) && line_starts(&window.editor, 2u, "// glyphs.c"),
              "Mod+W closes the tab shown, the one left shown");

    // the explorer: the strip's explorer tool pressed opens the side with the tree's files, and a file pressed there
    // opens as a tab
    const GuiRect explorer_button = gui_strip_button(&window.frame, 0u);
    gui_window_pressed(&window, explorer_button.x + 10, explorer_button.y + 10, 0u);
    cli_check((window.frame.side.width > 0) && (window.frame.editor.x == window.frame.side.x + window.frame.side.width),
              "the explorer's tool pressed opens the side beside the editor");
    cli_check(rows_came(&window, &count) && (file_row(&window.explorer) >= 0), "the tree's top folder is listed into the side, with a file among its rows");
    // the row is among the rows; the wheel over the side scrolls it into view where it stands past the rows shown
    gui_window_wheel(&window, window.frame.side.x + 10, window.frame.side.y + 100, file_row(&window.explorer));
    const unsigned int row = (unsigned int)file_row(&window.explorer);
    const GuiRect file = gui_explorer_row_rect(&window.frame, &window.explorer, row);
    cli_check(gui_rect_holds(window.frame.side, file.x + 40, file.y + 10), "the wheel over the side scrolls the row into view");
    unsigned int node = 0u;
    unsigned long long length = 0ull;
    const char *const held = gui_explorer_row(&window.explorer, row, &node) ? gui_explorer_path(&window.explorer, node, &length) : "";
    char picked[CLI_PATH_MAX_LEN];
    VerbaLinea kept = {picked, sizeof(picked), 0ull};
    mmgr_line_put_n(&kept, held, length);
    (void)mmgr_line_finish(&kept);
    gui_editor_made(&window.editor);
    gui_window_pressed(&window, file.x + 40, file.y + 10, 0u);
    cli_check((window.tabs == 2u) && lines_came(&window, &count) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = window.tab[window.active].path, .other = picked, .cap = CLI_PATH_MAX_LEN}),
              "a file pressed in the tree opens as a tab");

    gui_window_ended(&window);
    cli_window_ended();
    return cli_checked();
}
