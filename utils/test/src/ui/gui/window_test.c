// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check on a tree of its own, the window started as the view starts it and driven as the
// command line and the system drive it: files opened and commands sent through the door, keys and text as the system
// hands them. A tab changed while its view stands away from its save point; Save and Save All writing the texts to
// their files with their line ends; a changed tab's close waiting on a second and Close All closing the unchanged tabs
// and marking the changed; Save and Save All chosen only while a text is changed; a save that cannot write posting its
// failure with the tab left changed; Reduce Motion shown on while the saved setting or the system asks for less
// motion; and the menu key opening the terminal's menu at its key field while the panel takes the keys, a right press
// away closing it
#include "../../../../../ui/cli/src/command_line/command_line.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/machine/reach.h"
#include "../../../../../ui/cli/src/machine/state.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/cli/src/view/parts/explorer.h"
#include "../../../../../ui/cli/src/view/window.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

#include <stdio.h>

static GuiWindow s_window;
static char s_root[CLI_PATH_MAX_LEN];

// A command asked of the window through the door on a thread of its own, as the command line asks it: the command, its
// word, NULL for none, what the window answered, what the asking came to, and whether it has come
typedef struct
{
    const char *command;
    const char *const *words;
    unsigned int count;
    CliReachAnswer answer;
    int requested;
    embed_atomic_u32 done;
} WindowAsk;

static WindowAsk s_ask;

static void window_woken(void)
{
}

// on the asking thread: the command sent through the door and its answer waited for
static void window_ask_run(void *context)
{
    WindowAsk *const ask = (WindowAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    ask->requested = EMBED_CALL(cli_reach_menu.request, CliReachMenuCfg, .root = cli_command_line_root(), .menu = "window",
                                .command = ask->command, .request = ask->words, .count = ask->count,
                                .answer = &ask->answer, .err_msg = &err_msg);
    EMBED_ATOMIC_STORE(&ask->done, 1u);
}

// the window stepped once and its tick run, as the view's thread runs them
static void stepped(void)
{
    EMBED_CALL(gui_window_table.step, GuiWindowCfg, .window = &s_window);
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &s_window);
}

// the command `command` with its `words`, `count` of them, asked of the window through the door, the window stepped
// until it answers or ten seconds pass: 1 where it ran
static int asked_words(const char *command, const char *const *words, unsigned int count)
{
    s_ask = (WindowAsk){.command = command, .words = words, .count = count};
    static CliThread s_thread;
    s_thread = (CliThread){window_ask_run, &s_ask, CLI_PRIORITY_WORK};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread))
    {
        return 0;
    }
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while ((EMBED_ATOMIC_LOAD(&s_ask.done) == 0u) && (cli_clock_milliseconds() < last_ms))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 1ULL);
    }
    while (EMBED_ATOMIC_LOAD(&s_ask.done) == 0u)
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 1ULL);
    }
    const int ran = (s_ask.requested > 0) && s_ask.answer.done;
    if (!ran)
    {
        // the reason the window gave, beside the check that reads it, and the pool as it stands
        const SeriesCellularum *const record = cli_pool_series_cellularum();
        (void)fprintf(stderr, "  ask %s refused (%d): %s; pool end %llu top %llu fragmentation %llu\n", command,
                      s_ask.requested, s_ask.answer.text,
                      (unsigned long long)mmgr_series_cellularum_persistent_end(record),
                      (unsigned long long)mmgr_series_cellularum_temporary_top(record),
                      (unsigned long long)mmgr_series_cellularum_fragmentation(record));
    }
    return ran;
}

// the command `command` with its word `word`, NULL for none, asked of the window through the door
static int asked(const char *command, const char *word)
{
    const char *const words[1] = {word};
    return asked_words(command, words, (word != NULL) ? 1u : 0u);
}

// the key `name` pressed as the system hands it on
static void key_pressed(const char *name)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window, .input = GUI_KEY_INPUT(name));
    stepped();
}

// `text` typed as the system hands it on
static void typed(const char *text)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = GUI_TEXT_INPUT(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES})));
    stepped();
}

// the full path of the tree's file `name` into `path`, sized for CLI_PATH_MAX_LEN
static void tree_path(const char *name, char *path)
{
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, s_root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
}

// the tree's file `name` written to read `text`
static void file_written(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    tree_path(name, path);
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = text,
                     .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}), .fresh = 1);
}

// whether the tree's file `name` reads `required` whole
static int file_reads(const char *name, const char *required)
{
    char path[CLI_PATH_MAX_LEN];
    tree_path(name, path);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_checks_owner);
    const int same = (bytes != NULL) && (size == cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES})) &&
                     (memor.cmp(&(MemoriaCfg){.src = bytes, .other = required, .bytes = size}) == 0);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_checks_owner));
    return same;
}

// the tree's file `name` opened as the tab shown: 1 where it stands shown
static int tab_opened(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    tree_path(name, path);
    const int opened = asked("file", path);
    const int shown = cli_view_shown();
    const int stands = opened && (shown >= 0) &&
                       cellul.eq(&(CatenaFinitaCfg){.src = cli_view_path_at((unsigned int)shown),
                                                    .cap = CLI_POOL_BYTES,
                                                    .other = name,
                                                    .other_cap = CLI_POOL_BYTES});
    if (!stands)
    {
        // a file the window refused is told through its notice
        const char *const notice = cli_notice_shown();
        (void)fprintf(stderr, "  open %s: shown %d, notice %s\n", name, shown, (notice != NULL) ? notice : "none");
    }
    return stands;
}

// whether the tab `at` has changes not saved
static int changed(unsigned int at)
{
    const char *const key = cli_view_key(at);
    return (key != NULL) && EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = key);
}

// every tab closed, a changed one by the second close it waits on, and the tree's files written afresh
static void tabs_closed(const char *first, const char *first_text, const char *second, const char *second_text)
{
    for (unsigned int asks = 0u; (cli_view_count() != 0u) && (asks < 16u); asks += 1u)
    {
        (void)asked("close-all", NULL);
    }
    file_written(first, first_text);
    if (second != NULL)
    {
        file_written(second, second_text);
    }
}

// the row labeled `label` of the menu titled `title` on the bar, read into `item`, the menus closed again: whether it
// can be chosen
static int row_chosen(const char *title, const char *label, CliItem *item)
{
    unsigned int at = 0u;
    while ((at < cli_menu_titles()) && !cellul.eq(&(CatenaFinitaCfg){.src = cli_menu_title_text(cli_menu_title_menu(at)),
                                                                     .cap = CLI_POOL_BYTES,
                                                                     .other = title,
                                                                     .other_cap = CLI_POOL_BYTES}))
    {
        at += 1u;
    }
    cli_menus_bar(cli_menu_titles(), 0);
    cli_menus_title_press(at);
    int chosen = -1;
    for (unsigned int row = 0u; (g_cli_menus_open.levels != 0u) && (row < g_cli_menus_open.level[0].rows); row += 1u)
    {
        if (cli_menus_row(0u, row, item) &&
            cellul.eq(&(CatenaFinitaCfg){
                .src = item->label, .cap = CLI_POOL_BYTES, .other = label, .other_cap = CLI_POOL_BYTES}))
        {
            chosen = cli_menus_selectable(0u, row);
            break;
        }
    }
    cli_menus_close(0u);
    cli_check(chosen != -1, label);
    return chosen == 1;
}

// whether View's Reduce Motion shows on
static int reduce_motion_on(void)
{
    CliItem item;
    (void)row_chosen("View", "Reduce Motion", &item);
    return g_cli_menus_open.checked(&item);
}

static void saves_checked(void)
{
    tabs_closed("saved.txt", "alpha\nbeta\n", NULL, NULL);
    cli_check(tab_opened("saved.txt"), "a file opened through the door stands as the tab shown");
    cli_check(!changed(0u), "a tab opened is the file as it is");
    typed("x");
    cli_check(changed(0u), "a tab typed in is changed");
    key_pressed("Mod+Z");
    cli_check(!changed(0u), "undo back to the text as it opened leaves it unchanged");
    key_pressed("Mod+Y");
    cli_check(changed(0u), "redo changes it again");
    cli_check(asked("save", NULL), "Save runs through the door");
    cli_check(file_reads("saved.txt", "xalpha\nbeta\n"), "Save writes the text to its file");
    cli_check(!changed(0u), "a tab saved is no longer changed");
    key_pressed("Mod+Z");
    cli_check(changed(0u), "the text as it opened is no longer the file's");
    key_pressed("Mod+Y");
    cli_check(!changed(0u), "redo back to the text as saved");

    tabs_closed("crlf.txt", "a\r\nb\r\n", NULL, NULL);
    cli_check(tab_opened("crlf.txt"), "a file whose lines end in a carriage return opens");
    typed("x");
    (void)asked("save", NULL);
    cli_check(file_reads("crlf.txt", "xa\r\nb\r\n"), "it saves with its carriage returns");

    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    cli_check(tab_opened("first.txt"), "the first file opens");
    typed("x");
    cli_check(tab_opened("second.txt"), "the second file opens");
    typed("y");
    cli_check(asked("save-all", NULL), "Save All runs through the door");
    cli_check(file_reads("first.txt", "xone\n") && file_reads("second.txt", "ytwo\n"), "Save All writes every changed tab");
    cli_check(!changed(0u) && !changed(1u), "and neither is changed after");
}

static void closes_checked(void)
{
    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    (void)tab_opened("second.txt");
    typed("x");
    (void)asked("close-editor", NULL);
    cli_check(cli_view_count() == 2u, "the changed tab stands after the first close");
    cli_check(cli_view_closing(1u), "and its close waits on a second");
    (void)asked("close-editor", NULL);
    cli_check(cli_view_count() == 1u, "the second close closes it");
    (void)asked("close-editor", NULL);
    cli_check(cli_view_count() == 0u, "the tab with no change closes at once");

    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    (void)tab_opened("second.txt");
    typed("x");
    (void)asked("close-all", NULL);
    cli_check(cli_view_count() == 1u, "Close All closes the unchanged tabs");
    cli_check(cli_view_closing(0u), "and marks the changed one");
}

static void save_rows_checked(void)
{
    CliItem item;
    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    cli_check(!row_chosen("File", "Save", &item), "Save cannot be chosen while no text is changed");
    cli_check(!row_chosen("File", "Save All", &item), "nor Save All");
    typed("x");
    cli_check(row_chosen("File", "Save", &item), "Save can be chosen while the text shown is changed");
    cli_check(row_chosen("File", "Save All", &item), "and Save All");
    (void)tab_opened("second.txt");
    cli_check(!row_chosen("File", "Save", &item), "Save cannot be chosen while the tab shown is not changed");
    cli_check(row_chosen("File", "Save All", &item), "Save All can while another tab is");
}

static void failed_save_checked(void)
{
    tabs_closed("saved.txt", "one\n", NULL, NULL);
    (void)tab_opened("saved.txt");
    typed("x");
    // a folder where the file stood, which no file is put in place of
    char path[CLI_PATH_MAX_LEN];
    tree_path("saved.txt", path);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    key_pressed("Mod+S");
    cli_check(changed(0u), "a save that cannot write leaves the tab changed");
    cli_check((cli_notice_shown() != NULL) && cli_notice_failed(), "and its failure is shown");
    cli_check((cli_notice_shown() != NULL) &&
                  cellul.has(&(CatenaFinitaCfg){
                      .src = cli_notice_shown(), .cap = CLI_POOL_BYTES, .other = "saved.txt", .other_cap = 10u}),
              "naming the file");
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
}

static void reduce_motion_checked(void)
{
    (void)asked("reduce-motion", "off");
    EMBED_CALL(gui_window_reduced.motion_wish, GuiWindowReducedCfg, .window = &s_window, .reduced = 0);
    cli_check(!reduce_motion_on(), "Reduce Motion is off while neither asks for less motion");
    EMBED_CALL(gui_window_reduced.motion_wish, GuiWindowReducedCfg, .window = &s_window, .reduced = 1);
    cli_check(reduce_motion_on(), "on while the system asks for less motion");
    (void)asked("reduce-motion", NULL);
    cli_check(reduce_motion_on(), "turned from on, the saved setting is off and the system's wish still shows");
    EMBED_CALL(gui_window_reduced.motion_wish, GuiWindowReducedCfg, .window = &s_window, .reduced = 0);
    cli_check(!reduce_motion_on(), "the system's wish gone, it is off");
    (void)asked("reduce-motion", "on");
    cli_check(reduce_motion_on(), "its word on sets it on");
    (void)asked("reduce-motion", "on");
    cli_check(reduce_motion_on(), "on again stays on");
    (void)asked("reduce-motion", "off");
    cli_check(!reduce_motion_on(), "its word off sets it off");
    (void)asked("reduce-motion", NULL);
    cli_check(reduce_motion_on(), "with no word it turns from what shows");
    (void)asked("reduce-motion", "off");
}

// a file opened through the door with a line and a column after its path, each a colon and digits counted from 1,
// the cursor standing there; with a line alone, at the line's start
static void place_checked(void)
{
    tabs_closed("placed.txt", "one\ntwo\nthree\n", NULL, NULL);
    char path[CLI_PATH_MAX_LEN];
    tree_path("placed.txt:2:3", path);
    cli_check(asked("file", path), "a path with a line and a column opens through the door");
    const int shown = cli_view_shown();
    cli_check((shown >= 0) && cellul.eq(&(CatenaFinitaCfg){.src = cli_view_path_at((unsigned int)shown),
                                                           .cap = CLI_POOL_BYTES,
                                                           .other = "placed.txt",
                                                           .other_cap = 11u}),
              "the file of the path before the line and the column stands as the tab shown");
    const CliViewFrame *const frame = s_window.editor.frame;
    cli_check((frame != NULL) && (frame->cursor.line == 1ULL) && (frame->cursor.col == 2ULL),
              "the cursor stands at the line and the column");
    tree_path("placed.txt:3", path);
    (void)asked("file", path);
    cli_check((s_window.editor.frame != NULL) && (s_window.editor.frame->cursor.line == 2ULL) &&
                  (s_window.editor.frame->cursor.col == 0ULL),
              "a line alone puts the cursor at the line's start");
}

// the caret's ink as the window draws it at the moment `now_ms`
static unsigned int caret_ink_at(unsigned long long now_ms)
{
    g_test_clock_ms = now_ms;
    (void)EMBED_CALL(gui_window_table.caret_draw, GuiWindowCfg, .window = &s_window);
    g_test_clock_ms = GUI_TEST_CLOCK_MACHINE;
    return s_window.caret_color;
}

static void caret_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the caret");
    const unsigned long long typed_ms = cli_clock_milliseconds();
    typed("x");
    const unsigned long long begun_ms = s_window.caret_blink_start_ms;
    cli_check(begun_ms >= typed_ms + GUI_CARET_INPUT_REST_MS, "a key puts the caret's blink past the input rest");
    cli_check(caret_ink_at(begun_ms - 1ULL) == 255u, "the caret stands shown through the input rest");
    cli_check(caret_ink_at(begun_ms) == 255u, "shown as its blink begins");
    cli_check(caret_ink_at(begun_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL) - 1ULL) == 255u,
              "shown through the first half of the blink");
    cli_check(caret_ink_at(begun_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL)) == 0u, "hidden from its second half");
    cli_check(caret_ink_at(begun_ms + GUI_CARET_BLINK_PERIOD_MS - 1ULL) == 0u, "hidden to the blink's end");
    cli_check(caret_ink_at(begun_ms + GUI_CARET_BLINK_PERIOD_MS) == 255u, "shown again as the next blink begins");
    cli_check(GUI_CARET_BLINK_PERIOD_MS == 1060ULL, "a blink takes 1.06 seconds");
}

// the pointer moved to x, y as the system hands it on
static void moved(int x, int y)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window, .input = GUI_MOVED_INPUT(x, y));
    stepped();
}

// the window stepped until `milliseconds` pass
static void stepped_for(unsigned long long milliseconds)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (cli_clock_milliseconds() < until)
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
}

// The explorer stays while the pointer works in it: the claim on, a rest in the left band brings the strip and the
// side, and a pointer that moves from the strip onto the side's rows arms no hide
static void explorer_stays_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the explorer");
    typed("x");
    cli_check(s_window.claim.autohidden, "typing claims the edges");
    moved(2, 400);
    stepped_for(GUI_AUTOHIDE_SHOW_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_LEFT, "a rest in the left band shows the left edge");
    // the strip's first tool, at its button's middle
    const GuiRect tool = EMBED_CALL(gui_strip.button, GuiStripCfg, .frame = &s_window.frame, .at = 0u);
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    stepped_for(GUI_SIDES_ICON_REST_MS + GUI_SIDES_SLIDE_MS + 100ULL);
    cli_check(s_window.frame.side_displayed, "a rest on the strip's tool shows its side");
    const GuiRect side = s_window.frame.side;
    moved(side.x + (side.width / 2), side.y + (side.height / 2));
    cli_check(s_window.claim.hide_deadline_ms == 0ULL, "the pointer on the side's rows arms no hide of the left edge");
    stepped_for(GUI_AUTOHIDE_HIDE_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check((s_window.claim.displayed == GUI_AUTOHIDE_LEFT) && s_window.frame.side_displayed,
              "the left edge and the side stand while the pointer rests on the side");
}

// The minimap's lines stand beside the view wherever it stands: a tall file opened has its map's lines taken at once,
// and the view placed deep in the file has the lines of the map's rows there, the map away or shown
static void minimap_lines_checked(void)
{
    static char s_tall[CLI_POOL_BYTES / 4096u];
    VerbaLinea tall = {s_tall, sizeof(s_tall), 0ULL};
    for (unsigned int line = 0u; line < 3000u; line += 1u)
    {
        mmgr_line_put(&tall, "int line_");
        mmgr_line_decimal(&tall, line, 1u);
        mmgr_line_put(&tall, ";\n");
    }
    (void)mmgr_line_finish(&tall);
    tabs_closed("tall.c", s_tall, NULL, NULL);
    cli_check(tab_opened("tall.c"), "a tall file opens for the map");
    const GuiMinimap *const map = &s_window.minimap;
    cli_check((map->lines != NULL) && (map->lines->from == 0u) && (map->lines->count > 1u),
              "the map's lines are taken as the file opens, from its first line");
    char path[CLI_PATH_MAX_LEN];
    tree_path("tall.c:2500", path);
    cli_check(asked("file", path), "the view is placed deep in the file");
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    cli_check(EMBED_CALL(gui_minimap_first.span, GuiMinimapFirstCfg, .map_read = map, .first = &first, .last = &last) &&
                  (first > 0ULL),
              "the map's first row follows the view down the file");
    cli_check((map->lines != NULL) && (map->lines->from == first) &&
                  (map->lines->from + (unsigned long long)map->lines->count == last + 1ULL),
              "the map's lines are the lines its rows stand for");
}

// The right edge comes on a rest and goes once the pointer leaves it for the text: the claim on, a rest in the right
// band shows the map, and the pointer moved into the text takes it away after the hide's rest
static void map_collapses_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the right edge");
    typed("x");
    cli_check(s_window.claim.autohidden, "typing claims the edges");
    moved((int)s_window.frame.width - 2, 400);
    stepped_for(GUI_AUTOHIDE_SHOW_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_RIGHT, "a rest in the right band shows the map");
    const GuiRect map = s_window.frame.map;
    moved(map.x + (map.width / 2), map.y + (map.height / 2));
    stepped_for(GUI_AUTOHIDE_HIDE_DELAY_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_RIGHT, "the pointer on the map keeps it");
    const GuiRect editor = s_window.frame.editor;
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    stepped_for(GUI_AUTOHIDE_HIDE_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_NONE, "the pointer moved into the text takes the map away");
}

// the button `button` pressed at x, y as the system hands it on
static void pressed_with(int x, int y, unsigned int button)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_DOWN, .x = x, .y = y, .button = button});
    stepped();
}

// A middle press in the text starts a scroll that follows the pointer's distance from the press point: the pointer
// below the press point scrolls the view down each frame, one above it scrolls it back up, and the next press
// ends it. The tall file the map's check opened, placed deep in it, is the text
static void autoscroll_checked(void)
{
    const int shown = cli_view_shown();
    cli_check(shown >= 0, "the tall file stands shown for the autoscroll");
    const char *const key = cli_view_key((unsigned int)shown);
    // the view placed deep in the file comes to rest there first
    stepped_for(GUI_SCROLL_STEP_MOST_MS * 10ULL);
    const unsigned long long start = cli_view_top(key);
    const GuiRect editor = s_window.frame.editor;
    const int x = editor.x + (editor.width / 2);
    const int y = editor.y + (editor.height / 2);
    pressed_with(x, y, GUI_BUTTON_MIDDLE);
    cli_check(s_window.autoscroll, "a middle press in the text starts the autoscroll");
    stepped_for(200ULL);
    cli_check(cli_view_top(key) == start, "the pointer still at the press point scrolls nothing");
    moved(x, y + 100);
    stepped_for(300ULL);
    const unsigned long long down = cli_view_top(key);
    cli_check(down > start, "the pointer held below the press point scrolls the view down");
    moved(x, y - 100);
    stepped_for(300ULL);
    cli_check(cli_view_top(key) < down, "the pointer held above the press point scrolls it back up");
    moved(x, y + 100);
    pressed_with(x, y + 100, GUI_BUTTON_LEFT);
    cli_check(!s_window.autoscroll, "the next press ends the autoscroll");
    // the scroll eases to where the autoscroll last put it, then stands
    stepped_for(GUI_SCROLL_STEP_MOST_MS * 10ULL);
    const unsigned long long ended = cli_view_top(key);
    stepped_for(200ULL);
    cli_check(cli_view_top(key) == ended, "the view stands once it ends");
}

static void terminal_menu_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(asked("toggle", NULL), "the terminal opens through the door");
    cli_check(cli_terminal_tabs_checked("terminal-keys"), "and takes the keys");
    cli_check(caret_ink_at(cli_clock_milliseconds()) == 0u,
              "the editor's caret is hidden while the terminal has the keys");
    key_pressed("ContextMenu");
    cli_check(g_cli_menus_open.levels == 1u, "the menu key opens a menu");
    cli_check(g_cli_menus_open.open == -1, "under no title of the bar");
    cli_check(g_cli_menus_open.place == CLI_MENU_PLACE_TERMINAL_KEYS, "at the terminal's key field");
    CliItem first;
    cli_check(cli_menus_row(0u, 0u, &first) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = first.label, .cap = CLI_POOL_BYTES, .other = "Copy", .other_cap = 5u}),
              "the terminal's menu, Copy first");
    cli_check(g_cli_menus_open.level[0].pointed == 1, "the first row that can be chosen marked");
    key_pressed("Down");
    key_pressed("ContextMenu");
    cli_check((g_cli_menus_open.levels == 1u) && (g_cli_menus_open.level[0].pointed == 3),
              "the menu key on a menu open does nothing");
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_MENU, .x = 700, .y = 200});
    cli_check(g_cli_menus_open.levels == 0u, "a right press away from it closes it");
    (void)asked("toggle", NULL);
}

// whether the text `text` is `required`
static int same(const char *text, const char *required)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = text, .other = required, .cap = CLI_POOL_BYTES});
}

// The theme commands through the door: a theme made from the scheme's colors and chosen, renamed, given an icon the
// explorer's marks take, given a color the palette takes, shared to the clipboard, left for the scheme's own colors
// and chosen again, read back from the clipboard as a theme of its own, its icon taken off, and deleted; the themes and
// the choice stand in the home's state; and the image commands stay refused
static void themes_checked(void)
{
    GuiThemes *const themes = s_window.themes;
    cli_check((themes != NULL) && (themes->count == 0u) && (s_window.theme_chosen == 0u),
              "the window starts on the scheme's own colors with no themes");
    const GuiColor accent = s_window.palette->signal;
    cli_check(asked("theme-new", NULL), "theme-new runs through the door");
    cli_check((themes->count == 1u) && (s_window.theme_chosen == 0u) && same(themes->theme[0].name, "Dark 2"),
              "a theme from the scheme's colors is named after it and chosen");
    cli_check(asked("theme-rename", "Night") && same(themes->theme[0].name, "Night"), "theme-rename renames it");
    const char *const icon_words[3] = {"*.txt", "Tx", "accent"};
    cli_check(asked_words("theme-icon", icon_words, 3u) && (themes->theme[0].icons == 1u),
              "theme-icon gives the theme an icon");
    char mark[GUI_EXPLORER_MARK_MOST];
    GuiColor own = 0u;
    const unsigned int marked =
        EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = "first.txt", .length = 9ULL, .text = mark,
                   .palette = s_window.palette, .color_out = &own);
    cli_check((marked == GUI_EXPLORER_MARK_OWN) && (own == accent) && same(mark, "Tx"),
              "the explorer marks a file the pattern covers with the icon in the palette's color");
    const char *const color_words[2] = {"accent", "#102030"};
    GuiColor set = 0u;
    cli_check(asked_words("theme-color", color_words, 2u) && (s_window.palette->signal == GUI_RGB(0x10u, 0x20u, 0x30u)) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = themes, .at = 0u,
                             .name = "accent", .color_pointer = &set) &&
                  (set == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-color sets the palette's color in the theme and the window");
    cli_check(asked("theme-export", NULL), "theme-export runs");
    char clipboard_text[8192];
    VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
    (void)cli_clipboard_text_read(&(CliCtx){.text_verba_linea_pointer = &clipboard});
    (void)mmgr_line_finish(&clipboard);
    cli_check(cellul.has(&(CatenaFinitaCfg){.src = clipboard_text, .other = "\"name\": \"Night\"", .cap = sizeof(clipboard_text), .other_cap = 16u}) &&
                  cellul.has(&(CatenaFinitaCfg){.src = clipboard_text, .other = "\"accent\": \"#102030\"", .cap = sizeof(clipboard_text), .other_cap = 20u}),
              "the clipboard holds the theme as text");
    cli_check(asked("theme-choose", "Dark") && (s_window.theme_chosen == 1u) && (s_window.palette->signal == accent),
              "theme-choose Dark puts the scheme's colors back");
    cli_check(asked("theme-choose", "Night") && (s_window.theme_chosen == 0u) &&
                  (s_window.palette->signal == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-choose by name lays the theme's colors again");
    cli_check(!asked("theme-choose", "Nowhere"), "a name no theme has is refused");
    cli_check(asked("theme-import", NULL) && (themes->count == 2u) && (s_window.theme_chosen == 1u) &&
                  same(themes->theme[1].name, "Night 2") && (themes->theme[1].icons == 1u) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = themes, .at = 1u,
                             .name = "accent", .color_pointer = &set) &&
                  (set == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-import reads the clipboard's theme in under a free name and chooses it");
    cli_check(asked("theme-icon-remove", "*.txt") && (themes->theme[1].icons == 0u),
              "theme-icon-remove takes the icon off the chosen theme");
    cli_check(asked("theme-delete", NULL) && (themes->count == 1u) && (s_window.theme_chosen == 1u) &&
                  (s_window.palette->signal == accent),
              "theme-delete takes the chosen theme out and goes back to the scheme's colors");
    char folder[CLI_PATH_MAX_LEN];
    cli_check(EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder), "the home is known");
    CliStateEntries *const entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner});
    const char *const kept = (entries != NULL) ? cli_state_entry_value(entries, "themes") : NULL;
    const char *const chosen = (entries != NULL) ? cli_state_entry_value(entries, "theme.dark") : NULL;
    cli_check((kept != NULL) && cellul.has(&(CatenaFinitaCfg){.src = kept, .other = "\"Night\"", .cap = CLI_POOL_BYTES, .other_cap = 8u}) &&
                  !cellul.has(&(CatenaFinitaCfg){.src = kept, .other = "Night 2", .cap = CLI_POOL_BYTES, .other_cap = 8u}),
              "the home's state keeps the themes as they stand");
    cli_check((chosen != NULL) && same(chosen, "Dark"), "and the choice");
    g_cli_guard->persistent_buf_release(entries, g_cli_view_owner);
    cli_check(!asked("theme-image", NULL), "theme-image stays refused: the window reads no image files");
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check_folder("window_tree", s_root);
    cli_check(EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_ROOT", .value = s_root),
              "the tree is set");
    char error_text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    if (!EMBED_CALL(gui_window_woken.begin, GuiWindowWokenCfg, .window = &s_window, .err_msg = &why,
                    .display_width = 1440u, .display_height = 920u))
    {
        (void)mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    cli_command_line_tree_begin(window_woken);
    EMBED_CALL(gui_window_table.start, GuiWindowCfg, .window = &s_window, .woken = window_woken);
    (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &s_window, .width = 1440u, .height = 920u,
                     .system_scale = 100);
    // the tree as the window began it, the name of each file the tabs show is its path from it
    memor.cpy(&(MemoriaCfg){
        .dst = s_root,
        .src = cli_command_line_root(),
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_command_line_root(), .cap = CLI_PATH_MAX_LEN}) + 1u});
    // the name's pane, which takes every key and all text while it stands, left once the tree is read
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (s_window.wordmark.exists && (cli_clock_milliseconds() < last_ms))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    cli_check(!s_window.wordmark.exists, "the name's pane leaves once the tree is read");
    // the place past the folder's last slash of either kind, 0 where it has none
    const unsigned long long past_slash =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_root,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = sizeof(s_root)}), .set = "/\\");
    cli_check((s_window.title != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = s_window.title,
                                                                       .cap = CLI_PATH_MAX_LEN,
                                                                       .other = s_root + past_slash,
                                                                       .other_cap = CLI_PATH_MAX_LEN}),
              "the window's title is the tree's name, the last part of its folder");
    saves_checked();
    closes_checked();
    save_rows_checked();
    failed_save_checked();
    reduce_motion_checked();
    place_checked();
    caret_checked();
    explorer_stays_checked();
    terminal_menu_checked();
    minimap_lines_checked();
    autoscroll_checked();
    map_collapses_checked();
    themes_checked();
    // the pool as the checks leave it: the end, the fragmentation, and the largest of a few takes the tier still meets
    {
        const SeriesCellularum *const record = cli_pool_series_cellularum();
        static const unsigned long long s_probe_bytes[] = {512ULL << 10u, 2ULL << 20u, 8ULL << 20u, 32ULL << 20u};
        unsigned long long met = 0ULL;
        for (unsigned int at = 0u; at < sizeof(s_probe_bytes) / sizeof(s_probe_bytes[0]); at += 1u)
        {
            void *const probe = g_cli_guard->persistent_buf_alloc(s_probe_bytes[at], g_checks_owner);
            met = (probe != NULL) ? s_probe_bytes[at] : met;
            g_cli_guard->persistent_buf_release(probe, g_checks_owner);
        }
        (void)fprintf(stderr, "  pool end %llu fragmentation %llu, a take of %llu bytes met\n",
                      (unsigned long long)mmgr_series_cellularum_persistent_end(record),
                      (unsigned long long)mmgr_series_cellularum_fragmentation(record), met);
    }
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &s_window);
    cli_command_line_tree_end();
    return cli_on_check();
}
