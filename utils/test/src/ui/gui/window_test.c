// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_test.c: the window's check on a tree of its own, the window started as the view starts it and driven as the
// command line and the system drive it: files opened and commands sent through the interface, keys and text as the
// system passes them. A tab changed while its view is away from its save point; Save and Save All writing the texts to
// their files with their line ends; a changed tab's close waiting on a second and Close All closing the unchanged tabs
// and marking the changed; Save and Save All chosen only while a text is changed; a save that cannot write posting its
// failure with the tab left changed; Reduce Motion displayed on while the saved setting or the system asks for less
// motion; and the menu key opening the terminal's menu at its key field while the panel takes the keys, a right press
// away closing it
#include "../../../../../includes/window/controls.h"
#include "../../../../../includes/window/top_menu.h"
#include "../../../../../ui/cli/src/code/references.h"
#include "../../../../../ui/cli/src/code/symbols_tree.h"
#include "../../../../../ui/cli/src/command_line/command_line.h"
#include "../../../../../ui/cli/src/comms/sessions.h"
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/machine/reach.h"
#include "../../../../../ui/cli/src/machine/state.h"
#include "../../../../../ui/cli/src/machine/trust.h"
#include "../../../../../ui/cli/src/plugins/templates.h"
#include "../../../../../ui/cli/src/runs/job_runs.h"
#include "../../../../../ui/cli/src/servers/servers.h"
#include "../../../../../ui/cli/src/terminal/terminal_tabs.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "../../../../../ui/gui/src/explorer.h"
#include "../../../../../ui/gui/src/run.h"
#include "../../../../../ui/gui/src/window.h"
#include "../../../../../ui/query/src/query_internal.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

#include <stdio.h>

static GuiWindow s_window;
static char s_root[CLI_PATH_MAX_LEN];

// The frame the window last built for the view on display, read into the checks' buffer for the check, NULL for none.
// It also goes into the window's editor record, which the window empties at the end of each of its calls
static const CliViewFrame *editor_frame(void)
{
    char key[CLI_NAME_MAX_LEN];
    const int displayed = cli_view_displayed();
    s_window.editor.frame =
        ((displayed >= 0) && cli_view_key(&(CliViewAtCfg){.at = (unsigned int)displayed, .key_out = key}))
            ? cli_check_frame_kept(key)
            : NULL;
    return s_window.editor.frame;
}

// the window's editor record, with the frame the window last built read into it for the check
static GuiEditor *editor_framed(void)
{
    (void)editor_frame();
    return &s_window.editor;
}

// A command the check asks of the window through the interface, on a thread of its own, as the command line asks it.
// The record holds the command, its word, NULL for none, what the window answered, what the ask came to, and whether
// the answer came.
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

// On the ask's thread: send the command through the interface and wait for its answer.
static void window_ask_run(void *context)
{
    WindowAsk *const ask = (WindowAsk *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    ask->requested = EMBED_CALL(cli_reach_menu.request, CliReachMenuCfg, .root = cli_command_line_root(),
                                .menu = "window", .command = ask->command, .request = ask->words, .count = ask->count,
                                .answer = &ask->answer, .err_msg = &err_msg);
    EMBED_ATOMIC_STORE(&ask->done, 1u);
}

// the window stepped once and its tick run, as the view's thread runs them
static void stepped(void)
{
    EMBED_CALL(gui_window_table.step, GuiWindowCfg, .window = &s_window);
    EMBED_CALL(gui_window_table.tick, GuiWindowCfg, .window = &s_window);
}

// Ask the command `command` with its `words`, `count` of them, of the window through the interface. Step the window
// until it answers or ten seconds pass. Return 1 if it ran.
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
        // the reason the window gave, beside the check that reads it, and the pool as it is
        const SeriesCellularum *const record = cli_pool_series_cellularum();
        (void)fprintf(stderr, "  ask %s refused (%d): %s; pool end %llu top %llu fragmentation %llu\n", command,
                      s_ask.requested, s_ask.answer.text,
                      (unsigned long long)mmgr_series_cellularum_persistent_end(record),
                      (unsigned long long)mmgr_series_cellularum_temporary_top(record),
                      (unsigned long long)mmgr_series_cellularum_fragmentation(record));
    }
    return ran;
}

// the command `command` with its word `word`, NULL for none, asked of the window through the interface
static int asked(const char *command, const char *word)
{
    const char *const words[1] = {word};
    return asked_words(command, words, (word != NULL) ? 1u : 0u);
}

// the key `name` pressed as the system passes it on
static void key_pressed(const char *name)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window, .input = GUI_KEY_INPUT(name));
    stepped();
}

// `text` typed as the system passes it on
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
    const int same = (bytes != NULL) &&
                     (size == cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES})) &&
                     (memor.cmp(&(MemoriaCfg){.src = bytes, .other = required, .bytes = size}) == 0);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_checks_owner));
    return same;
}

// Open the tree's file `name` as the tab displayed. Return 1 if the window displays it.
// Return whether the tree's file `name` is the tab displayed, after the command `command` asks it open through the
// interface.
static int tab_opened_by(const char *command, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    tree_path(name, path);
    const int opened = asked(command, path);
    const int displayed = cli_view_displayed();
    char displayed_path[CLI_PATH_MAX_LEN];
    const int is = opened && (displayed >= 0) &&
                   cli_view_path_at(&(CliViewAtCfg){.at = (unsigned int)displayed, .path_out = displayed_path}) &&
                   cellul.eq(&(CatenaFinitaCfg){
                       .src = displayed_path, .cap = CLI_POOL_BYTES, .other = name, .other_cap = CLI_POOL_BYTES});
    if (!is)
    {
        // The window's notice says why it refused a file.
        const char *const notice = cli_notice_displayed();
        (void)fprintf(stderr, "  open %s: displayed %d, notice %s\n", name, displayed,
                      (notice != NULL) ? notice : "none");
    }
    return is;
}

static int tab_opened(const char *name)
{
    return tab_opened_by("file", name);
}

// whether the tab `at` has changes not saved
static int changed(unsigned int at)
{
    char key[CLI_NAME_MAX_LEN];
    return cli_view_key(&(CliViewAtCfg){.at = at, .key_out = key}) &&
           EMBED_CALL(cli_view_table.modified, CliViewCfg, .key = key);
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

// Read the row labeled `label` of the menu titled `title` on the bar into `item`, and close the menus again. Return
// whether a press can choose the row.
static int row_chosen(const char *title, const char *label, CliItem *item)
{
    unsigned int at = 0u;
    while ((at < cli_menu_titles()) &&
           !cellul.eq(&(CatenaFinitaCfg){
               .src = cli_menu_title_text(&(CliMenusCfg){.menu = cli_menu_title_menu(&(CliMenusCfg){.at = at})}),
               .cap = CLI_POOL_BYTES,
               .other = title,
               .other_cap = CLI_POOL_BYTES}))
    {
        at += 1u;
    }
    cli_menus_bar(&(CliMenusCfg){.displayed = cli_menu_titles(), .folded = 0});
    cli_menus_title_press(&(CliMenusCfg){.at = at});
    int chosen = -1;
    for (unsigned int row = 0u; (g_cli_menus_open.levels != 0u) && (row < g_cli_menus_open.level[0].rows); row += 1u)
    {
        if (cli_menus_row(&(CliMenusCfg){.level = 0u, .at = row, .item = item}) &&
            cellul.eq(&(CatenaFinitaCfg){
                .src = item->label, .cap = CLI_POOL_BYTES, .other = label, .other_cap = CLI_POOL_BYTES}))
        {
            chosen = cli_menus_selectable(&(CliMenusCfg){.level = 0u, .at = row});
            break;
        }
    }
    cli_menus_close(&(CliMenusCfg){.level = 0u});
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
    cli_check(tab_opened("saved.txt"), "a file opened through the interface is as the tab displayed");
    cli_check(tab_opened_by("open-file", "saved.txt"), "File… with a path opens that file as the tab displayed");
    cli_check(!changed(0u), "a tab opened is the file as it is");
    typed("x");
    cli_check(changed(0u), "a tab typed in is changed");
    key_pressed("Mod+Z");
    cli_check(!changed(0u), "undo back to the text as it opened leaves it unchanged");
    key_pressed("Mod+Y");
    cli_check(changed(0u), "redo changes it again");
    cli_check(asked("save", NULL), "Save runs through the interface");
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
    cli_check(asked("save-all", NULL), "Save All runs through the interface");
    cli_check(file_reads("first.txt", "xone\n") && file_reads("second.txt", "ytwo\n"),
              "Save All writes every changed tab");
    cli_check(!changed(0u) && !changed(1u), "and neither is changed after");
}

static void closes_checked(void)
{
    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    (void)tab_opened("second.txt");
    typed("x");
    (void)asked("close-editor", NULL);
    cli_check(cli_view_count() == 2u, "the changed tab is after the first close");
    cli_check(cli_view_closing(&(CliViewAtCfg){.at = 1u}), "and its close waits on a second");
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
    cli_check(cli_view_closing(&(CliViewAtCfg){.at = 0u}), "and marks the changed one");
}

static void save_rows_checked(void)
{
    CliItem item;
    tabs_closed("first.txt", "one\n", "second.txt", "two\n");
    (void)tab_opened("first.txt");
    cli_check(!row_chosen("File", "Save", &item), "Save cannot be chosen while no text is changed");
    cli_check(!row_chosen("File", "Save All", &item), "nor Save All");
    typed("x");
    cli_check(row_chosen("File", "Save", &item), "Save can be chosen while the text displayed is changed");
    cli_check(row_chosen("File", "Save All", &item), "and Save All");
    (void)tab_opened("second.txt");
    cli_check(!row_chosen("File", "Save", &item), "Save cannot be chosen while the tab displayed is not changed");
    cli_check(row_chosen("File", "Save All", &item), "Save All can while another tab is");
}

static void failed_save_checked(void)
{
    tabs_closed("saved.txt", "one\n", NULL, NULL);
    (void)tab_opened("saved.txt");
    typed("x");
    // A folder stands where the file was, and no file takes its place.
    char path[CLI_PATH_MAX_LEN];
    tree_path("saved.txt", path);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
    key_pressed("Mod+S");
    cli_check(changed(0u), "a save that cannot write leaves the tab changed");
    cli_check((cli_notice_displayed() != NULL) && cli_notice_failed(), "and its failure is displayed");
    cli_check((cli_notice_displayed() != NULL) &&
                  cellul.has(&(CatenaFinitaCfg){
                      .src = cli_notice_displayed(), .cap = CLI_POOL_BYTES, .other = "saved.txt", .other_cap = 10u}),
              "naming the file");
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
}

// Return the bytes every owner of the cellblock has in use, summed. Put each owner's own bytes into `owners` if given.
// What a closed document gives back comes off these. The sum leaves the comms owner out, because its log records each
// ask the check makes.
static unsigned long long cells_in_use(unsigned long long *owners)
{
    unsigned long long in_use = 0ULL;
    for (size_t at = 0u; (at < mmgr_owner_table_count) && (at < MMGR_CARCER_OWNERS); at += 1u)
    {
        const int logs_asks = cellul.eq(
            &(CatenaFinitaCfg){.src = mmgr_owner_table[at].name, .other = "cli_comms", .cap = CLI_POOL_BYTES});
        const unsigned long long own = logs_asks ? 0ULL : (unsigned long long)mmgr_owner_in_use(&mmgr_owner_table[at]);
        in_use += own;
        if (owners != NULL)
        {
            owners[at] = own;
        }
    }
    return in_use;
}

// Step the window until no walker colors a text, or until `milliseconds` pass. Return 1 if no walker colors a text.
// A walk takes the branch's cells as it colors. A measure of the cells waits for the walk's end.
static int walk_rested(unsigned long long milliseconds)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while ((cli_document_walkers() != 0u) && (cli_clock_milliseconds() < until))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    return (cli_document_walkers() == 0u) ? 1 : 0;
}

// Return the cells in use after they rest. First wait until no walker colors a text, five seconds at most. Then read
// the cells again each 50 ms until two readings agree and no job of the servers waits or runs, five seconds at most. A
// release still in flight lands before a round's measure.
static unsigned long long cells_settled(unsigned long long *owners)
{
    CliWake pause = CLI_WAKE_INIT;
    (void)walk_rested(5000ULL);
    unsigned long long before = cells_in_use(owners);
    const unsigned long long last_ms = cli_clock_milliseconds() + 5000ULL;
    for (;;)
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 50ULL);
        const unsigned long long now = cells_in_use(owners);
        // a job in flight, as a git read for a tab opened, keeps its temporary cells until it ends
        if (((now == before) && cli_servers_jobs_idle()) || (cli_clock_milliseconds() >= last_ms))
        {
            return now;
        }
        before = now;
    }
}

// the bytes of each branch of the tree's symbols into `bytes`, 0 for a place no branch uses
static void branches_read(unsigned int *bytes)
{
    for (unsigned int file = 0u; file < CLI_BRANCHES_MAX; file += 1u)
    {
        CliTreeBranch branch;
        cli_branch_read(&(CliSymbolsTreeAtCfg){.file = file, .branch_out = &branch});
        bytes[file] = branch.used ? branch.bytes : 0u;
    }
}

// Write each branch that is new after the second round, or holds other bytes than then, beside the check. Write its
// path and its bytes after each round.
static void branches_grown(const unsigned int *second, const unsigned int *sixth)
{
    for (unsigned int file = 0u; file < CLI_BRANCHES_MAX; file += 1u)
    {
        if (second[file] == sixth[file])
        {
            continue;
        }
        CliTreeBranch branch;
        cli_branch_read(&(CliSymbolsTreeAtCfg){.file = file, .branch_out = &branch});
        char path[CLI_PATH_MAX_LEN] = "";
        if (branch.used)
        {
            cli_branch_path_read(&(CliSymbolsTreeBranchPathCfg){.branch = &branch, .path_out = path});
        }
        (void)fprintf(stderr, "    branch %u %s: %u then %u\n", file, path, second[file], sixth[file]);
    }
}

// the tiers' boundaries after a round, as their words hold them: the persistent end and each region's depth
typedef struct
{
    unsigned long long persistent_end;
    unsigned long long depth[MMGR_CARCER_REGION_COUNT];
} RoundTiers;

static void tiers_read(RoundTiers *tiers)
{
    const SeriesCellularum *const series = cli_pool_series_cellularum();
    tiers->persistent_end = mmgr_series_cellularum_persistent_end(series);
    for (size_t region = 0u; region < MMGR_CARCER_REGION_COUNT; region += 1u)
    {
        tiers->depth[region] = mmgr_series_cellularum_region_depth(series, region);
    }
}

// each tier boundary that differs after the sixth round from after the second, with its value after each round,
// beside the check that reads it. The persistent end names a persistent or movable cell. A region's depth names a
// temporary cell on a thread whose place selects that region, the last region the large one
static void tiers_moved(const RoundTiers *rounds)
{
    if (rounds[1].persistent_end != rounds[5].persistent_end)
    {
        (void)fprintf(stderr, "    persistent end");
        for (unsigned int round = 0u; round < 6u; round += 1u)
        {
            (void)fprintf(stderr, " %llu", rounds[round].persistent_end);
        }
        (void)fprintf(stderr, "\n");
    }
    for (size_t region = 0u; region < MMGR_CARCER_REGION_COUNT; region += 1u)
    {
        if (rounds[1].depth[region] != rounds[5].depth[region])
        {
            (void)fprintf(stderr, "    region %zu depth", region);
            for (unsigned int round = 0u; round < 6u; round += 1u)
            {
                (void)fprintf(stderr, " %llu", rounds[round].depth[region]);
            }
            (void)fprintf(stderr, "\n");
        }
    }
}

// A document of many lines opens, changes and closes over six rounds. The cells in use after the last round are no
// more than after the second. This holds that a closed document gives its cells back. The first round fills the
// tables sized to live data, the tree's records of the file among them. The second round reads them as they stand.
static void cells_rounds_checked(void)
{
    static char s_lines[96u * 1024u];
    VerbaLinea lines = {s_lines, sizeof(s_lines), 0ULL};
    for (unsigned int line = 0u; mmgr_line_ok(&lines) && (lines.at + 48ULL < sizeof(s_lines)); line += 1u)
    {
        mmgr_line_put(&lines, "line ");
        mmgr_line_decimal(&lines, line, 1u);
        mmgr_line_put(&lines, " of the rounds' document, held and let go\n");
    }
    (void)mmgr_line_finish(&lines);
    tabs_closed("rounds.txt", s_lines, NULL, NULL);
    static unsigned long long s_round_owners[6][MMGR_CARCER_OWNERS];
    static RoundTiers s_round_tiers[6];
    static unsigned int s_second_branches[CLI_BRANCHES_MAX];
    static unsigned int s_sixth_branches[CLI_BRANCHES_MAX];
    unsigned long long after_first = 0ULL;
    unsigned long long after_last = 0ULL;
    int every_round_opened = 1;
    for (unsigned int round = 0u; round < 6u; round += 1u)
    {
        every_round_opened = every_round_opened && tab_opened("rounds.txt");
        typed("x");
        for (unsigned int asks = 0u; (cli_view_count() != 0u) && (asks < 4u); asks += 1u)
        {
            (void)asked("close-all", NULL);
        }
        after_last = cells_settled(s_round_owners[round]);
        tiers_read(&s_round_tiers[round]);
        if (round == 1u)
        {
            after_first = after_last;
            branches_read(s_second_branches);
        }
    }
    branches_read(s_sixth_branches);
    cli_check(every_round_opened && (cli_view_count() == 0u), "the rounds' document opens and closes each round");
    if (after_last > after_first)
    {
        // Write each owner that holds more after the sixth round than after the second beside the check, with its
        // bytes in use after each round.
        (void)fprintf(stderr, "  cells in use after the second round %llu, after the sixth %llu\n", after_first,
                      after_last);
        for (size_t at = 0u; (at < mmgr_owner_table_count) && (at < MMGR_CARCER_OWNERS); at += 1u)
        {
            if (s_round_owners[5][at] > s_round_owners[1][at])
            {
                (void)fprintf(stderr, "    %s", mmgr_owner_table[at].name);
                for (unsigned int round = 0u; round < 6u; round += 1u)
                {
                    (void)fprintf(stderr, " %llu", s_round_owners[round][at]);
                }
                (void)fprintf(stderr, "\n");
            }
        }
        branches_grown(s_second_branches, s_sixth_branches);
        tiers_moved(s_round_tiers);
    }
    cli_check(after_last <= after_first, "a closed document gives its cells back over six rounds");
}

// the runs the editor's frame holds for its row `row`, 0 where it holds none
static unsigned int row_runs(unsigned int row)
{
    const CliViewFrame *const frame = editor_frame();
    return ((frame != NULL) && (row < frame->count))
               ? EMBED_CALL(cli_document_line_run_table.count, CliDocumentLineRunCfg, .key = frame->key,
                            .line = frame->row[row].number, .mirrored = 1)
               : 0u;
}

// the window stepped until the row `row` has runs or `milliseconds` pass: its runs
static unsigned int row_runs_waited(unsigned int row, unsigned long long milliseconds)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while ((row_runs(row) == 0u) && (cli_clock_milliseconds() < until))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    return row_runs(row);
}

static void cursor_put(unsigned long long line, unsigned long long col);
static void stepped_for(unsigned long long milliseconds);

// A C file opens and takes typed text. The walk colors the typed line again after each edit, and its runs come back
// in the frame.
static void typed_colored_checked(void)
{
    tabs_closed("typed.c", "int one(void)\n{\n    return 1;\n}\n", NULL, NULL);
    cli_check(tab_opened("typed.c"), "a C file opens for the typing check");
    cli_check(row_runs_waited(2u, 3000ULL) > 0u, "the walk colors the file's lines");
    cursor_put(2ULL, 4ULL);
    typed("x");
    cli_check(row_runs_waited(2u, 3000ULL) > 0u, "the line typed into is colored again after the edit");
    typed("y");
    cli_check(row_runs_waited(2u, 3000ULL) > 0u, "and after the next");
    // text typed at a line's head, a line break after it, then both undone: the window lives and the text is as it
    // was
    cursor_put(0ULL, 0ULL);
    typed("build/typed.c");
    key_pressed("Enter");
    cli_check(asked("undo", NULL), "the line break undoes");
    cli_check(asked("undo", NULL), "the text typed undoes");
    cli_check(asked("save", NULL) && file_reads("typed.c", "int one(void)\n{\n    xyreturn 1;\n}\n"),
              "text typed at a line's head and a line break after it undo back to the text as it was");
    (void)asked("close-all", NULL);
    (void)asked("close-all", NULL);
}

// the runs of the row `row` copied into `words`, `capacity` runs at most, a column then a name a run: how many runs
static unsigned int row_runs_copied(unsigned int row, unsigned int *words, unsigned int capacity)
{
    const CliViewFrame *const frame = editor_frame();
    const unsigned int runs = row_runs(row);
    if ((runs == 0u) || (runs > capacity))
    {
        return 0u;
    }
    for (unsigned int at = 0u; at < runs; at += 1u)
    {
        (void)EMBED_CALL(cli_document_line_run_table.read, CliDocumentLineRunCfg, .key = frame->key,
                         .line = frame->row[row].number, .mirrored = 1, .run = at, .column = &words[2u * at],
                         .name = &words[(2u * at) + 1u]);
    }
    return runs;
}

// A C file opens while the thread holds a temporary cell of 1.5 MiB. The grammar's store takes more than the room
// left in the thread's region. The walk colors the file's lines all the same.
static void grammar_held_checked(void)
{
    const unsigned long long held_bytes = 1536ULL << 10u;
    void *const held = g_cli_guard->temporary_buf_alloc(held_bytes, g_checks_owner);
    cli_check(held != NULL, "a cell of 1.5 MiB is taken on the thread");
    tabs_closed("held.c", "int one(void)\n{\n    return 1;\n}\n", NULL, NULL);
    cli_check(tab_opened("held.c"), "a C file opens beside the held cell");
    cli_check(row_runs_waited(2u, 3000ULL) > 0u, "the walk colors the file's lines while the thread holds a cell");
    g_cli_guard->persistent_buf_release(held, g_checks_owner);
    (void)asked("close-all", NULL);
}

static unsigned long long cells_settled(unsigned long long *owners);

// A C file of seven thousand lines opens and takes its first edit. The first edit gives the branch its own cells. The
// first count waits for the walk of the whole file, which the first edit can start again. Ten more edits of one
// character leave the cells in use within 64 KiB of the first edit's count. The check writes each owner that moved
// over the ten beside it.
static void edits_cells_checked(void)
{
    const unsigned long long capacity = 512ULL << 10u;
    char *const text = g_cli_guard->temporary_buf_alloc(capacity, g_checks_owner);
    cli_check(text != NULL, "a cell for the large text is taken");
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, capacity, 0ULL};
    for (unsigned int at = 0u; at < 7000u; at += 1u)
    {
        mmgr_line_put(&line, "static const unsigned int value_");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, " = ");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, "u; // one line of the large text\n");
    }
    (void)mmgr_line_finish(&line);
    tabs_closed("large.c", text, NULL, NULL);
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
    cli_check(tab_opened("large.c"), "the large C file opens");
    cli_check(row_runs_waited(2u, 5000ULL) > 0u, "the walk colors it");
    cursor_put(10ULL, 0ULL);
    typed("x");
    unsigned long long owners_first[MMGR_CARCER_OWNERS];
    const unsigned long long first = cells_settled(owners_first);
    for (unsigned int edit = 0u; edit < 10u; edit += 1u)
    {
        typed("x");
        (void)cells_settled(NULL);
    }
    unsigned long long owners_last[MMGR_CARCER_OWNERS];
    const unsigned long long last = cells_settled(owners_last);
    for (size_t at = 0u; (at < mmgr_owner_table_count) && (at < MMGR_CARCER_OWNERS); at += 1u)
    {
        if (owners_last[at] != owners_first[at])
        {
            (void)fprintf(stderr, "  owner %s %llu -> %llu\n", mmgr_owner_table[at].name, owners_first[at],
                          owners_last[at]);
        }
    }
    (void)fprintf(stderr, "  cells after the first edit %llu, after ten more %llu\n", first, last);
    cli_check(last <= first + (64ULL << 10u), "ten edits after the first leave the cells within 64 KiB");
    (void)asked("close-all", NULL);
}

// Return the private `working set` of this process, in bytes, 0 if the system does not say.
static unsigned long long working_set(void)
{
    CliProcessMemory measured;
    return EMBED_CALL(cli_pid_internal.process_memory, CliPidCtx, .pid = cli_process_own(), .memory = &measured)
               ? measured.working
               : 0ULL;
}

// A large C file opens and takes five edits, which the walk colors on its threads. The file closes. The idle pages
// of the cellblock go back, and the `working set` falls by a megabyte or more. The pages the walk's threads touched
// are no longer resident.
static void idle_pages_checked(void)
{
    const unsigned long long capacity = 512ULL << 10u;
    char *const text = g_cli_guard->temporary_buf_alloc(capacity, g_checks_owner);
    cli_check(text != NULL, "a cell for the large text is taken");
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, capacity, 0ULL};
    for (unsigned int at = 0u; at < 7000u; at += 1u)
    {
        mmgr_line_put(&line, "static const unsigned int value_");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, " = ");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, "u; // one line of the large text\n");
    }
    (void)mmgr_line_finish(&line);
    tabs_closed("large.c", text, NULL, NULL);
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
    cli_check(tab_opened("large.c"), "the large C file opens");
    cli_check(row_runs_waited(2u, 5000ULL) > 0u, "the walk colors it");
    for (unsigned int edit = 0u; edit < 5u; edit += 1u)
    {
        cursor_put(10ULL + (edit * 100ULL), 0ULL);
        typed("x");
        (void)cells_settled(NULL);
    }
    (void)asked("close-all", NULL);
    (void)cells_settled(NULL);
    const unsigned long long before = working_set();
    cli_pool_idle_released();
    const unsigned long long after = working_set();
    (void)fprintf(stderr, "  working set %llu before the idle pages go back, %llu after\n", before, after);
    cli_check((before != 0ULL) && (after + (1ULL << 20u) <= before),
              "the idle pages given back lower the working set by a megabyte or more");
}

// A C file with an include opens. The check erases the include's last quote and types it back. The line's runs are
// as they were before, and the string keeps its color as a string.
static void retyped_colored_checked(void)
{
    tabs_closed("retyped.c", "#include \"a.h\"\nint one;\n", NULL, NULL);
    cli_check(tab_opened("retyped.c"), "a C file with an include opens for the retyping check");
    cli_check(row_runs_waited(0u, 3000ULL) > 0u, "the walk colors the include line");
    unsigned int before[64];
    const unsigned int before_runs = row_runs_copied(0u, before, 32u);
    cursor_put(0ULL, 14ULL);
    key_pressed("Backspace");
    stepped_for(400ULL);
    typed("\"");
    stepped_for(400ULL);
    cli_check(row_runs_waited(0u, 3000ULL) > 0u, "the line is colored again after the quote is typed back");
    unsigned int after[64];
    const unsigned int after_runs = row_runs_copied(0u, after, 32u);
    int same = (before_runs != 0u) && (after_runs == before_runs);
    for (unsigned int at = 0u; same && (at < (2u * before_runs)); at += 1u)
    {
        same = before[at] == after[at];
    }
    cli_check(same, "the line's runs after the quote is typed back are as they were before it was erased");
    (void)asked("close-all", NULL);
    (void)asked("close-all", NULL);
}

// One language's sample: the file's name, whose extension names the plugin, and its text. The text has at most
// WINDOW_SAMPLE_LINES lines. Each line's last character is a plain one, which the check erases and types back.
typedef struct
{
    const char *name;
    const char *text;
} LanguageSample;

#define WINDOW_SAMPLE_LINES 5u
#define WINDOW_SAMPLE_RUNS 32u

static const LanguageSample s_language_samples[] = {
    {"sample.apib", "FORMAT: 1A\n# One\n## Two [/two]\n+ Response 200\n"},
    {"sample.bat", "@echo off\nset ONE=two\nif \"%ONE%\"==\"two\" echo three\nrem four\n"},
    {"sample.c",
     "#include \"a.h\"\n// one\nint two(int three) { return three + 1; }\nstatic const char *four = \"five\";\n"},
    {"sample.css", ".one { color: #fff; margin: 0 auto; }\n/* two */\n#three > a:hover { width: 10px; }\n"},
    {"sample.g", "grammar One;\ntwo : THREE+ ;\nTHREE : [a-z]+ ;\n// four\n"},
    {"sample.groovy", "def one = \"two\"\n// three\nclass Four { int five = 6 }\nprintln one\n"},
    {"sample.gsm", "1,1->2\n1,0->1\n0,1->1\n0,0->0\n"},
    {"sample.html", "<html>\n<!-- one -->\n<body class=\"two\">three</body>\n</html>\n"},
    {"sample.http", "GET /one HTTP/1.1\nHost: two.three\nAccept: */*\n"},
    {"sample.js", "// one\nconst two = \"three\";\nfunction four(five) { return five + 1; }\nexport { four };\n"},
    {"sample.jenkinsfile", "pipeline { agent any }\n// one\nnode { echo 'two' }\n"},
    {"sample.json", "{\"one\": \"two\",\n \"three\": [1, 2, 3],\n \"four\": 5}\n"},
    {"sample.kt", "fun one(two: Int): Int = two + 1\n// three\nval four = \"five\"\nclass Six(val seven: Int)\n"},
    {"sample.lean", "def one (two : Nat) : Nat := two + 1\n-- three\ntheorem four : one 1 = 2 := rfl\n"},
    {"sample.asc", "Version 4\nSHEET 1 880 680\nWIRE 96 80 32 80\nSYMBOL res 80 64 R0\n"},
    {"sample.md", "# One\nTwo *three* `four`\n- five\n[six](seven)\n"},
    {"sample.m", "function one = two(three)\n% four\none = three + 1;\nend\n"},
    {"sample.cir", "* one\nR1 1 0 1k\nV1 1 0 DC 5\n.end\n"},
    {"sample.org", "* One\n** Two\n- three\n#+TITLE: four\n"},
    {"sample.txt", "one two\nthree\nfour five six\n"},
    {"sample.ps1", "# one\n$two = \"three\"\nfunction Four { param($five) $five + 1 }\nWrite-Output $two\n"},
    {"sample.py", "# one\ndef two(three):\n    return three + 1\nfour = \"five\"\n"},
    {"sample.qmd", "---\ntitle: \"One\"\n---\n# Two\n"},
    {"sample.r", "# one\ntwo <- function(three) three + 1\nfour <- \"five\"\nprint(four)\n"},
    {"sample.raml", "#%RAML 1.0\ntitle: One\n/two:\n  get:\n"},
    {"sample.rb", "# one\ndef two(three)\n  three + 1\nend\n"},
    {"sample.rs", "// one\nfn two(three: u32) -> u32 { three + 1 }\nconst FOUR: &str = \"five\";\nstruct Six;\n"},
    {"sample.asm", "// one\n.section/pm seg_pmco;\nr0 = r1 + r2;\nrts;\n"},
    {"sample.sh", "#!/bin/sh\n# one\ntwo=\"three\"\necho \"$two\"\n"},
    {"sample.sql", "-- one\nSELECT two FROM three WHERE four = 'five';\nCREATE TABLE six (seven INT);\n"},
    {"sample.stg", "group one;\ntwo(three) ::= \"<three>\"\n// four\n"},
    {"sample.tex", "\\documentclass{article}\n\\begin{document}\nOne \\textbf{two}\n\\end{document}\n"},
    {"sample.toml", "# one\n[two]\nthree = \"four\"\nfive = 6\n"},
    {"sample.tsv", "one\ttwo\tthree\n1\t2\t3\n4\t5\t6\n"},
    {"sample.vhd", "-- one\nlibrary ieee;\nentity two is end;\narchitecture three of two is begin end;\n"},
    {"sample.ans", "\x1b[31mone\x1b[0m\ntwo \x1b[1mthree\x1b[0m\nfour\n"},
    {"sample.yml", "one: two\nthree:\n  - four\n  - five\n"},
    {"sample.zig", "// one\nconst two = @import(\"std\");\npub fn three(four: u32) u32 { return four + 1; }\nconst "
                   "five = \"six\";\n"},
};

// the runs of the sample's first rows copied into `words`, WINDOW_SAMPLE_RUNS runs a row at most, each row's count
// into `counts`: how many rows hold runs
static unsigned int sample_runs_copied(unsigned int rows, unsigned int words[][2u * WINDOW_SAMPLE_RUNS],
                                       unsigned int *counts)
{
    unsigned int held = 0u;
    for (unsigned int row = 0u; row < rows; row += 1u)
    {
        counts[row] = row_runs_copied(row, words[row], WINDOW_SAMPLE_RUNS);
        held += (counts[row] != 0u) ? 1u : 0u;
    }
    return held;
}

// whether the sample's first rows' runs are as `before` has them
static int sample_runs_same(unsigned int rows, unsigned int before[][2u * WINDOW_SAMPLE_RUNS],
                            const unsigned int *before_counts)
{
    unsigned int after[WINDOW_SAMPLE_LINES][2u * WINDOW_SAMPLE_RUNS];
    unsigned int after_counts[WINDOW_SAMPLE_LINES];
    (void)sample_runs_copied(rows, after, after_counts);
    int same = 1;
    for (unsigned int row = 0u; same && (row < rows); row += 1u)
    {
        same = (after_counts[row] == before_counts[row]) && (before_counts[row] != 0u);
        for (unsigned int at = 0u; same && (at < (2u * before_counts[row])); at += 1u)
        {
            same = before[row][at] == after[row][at];
        }
    }
    return same;
}

// One language's sample opens and the walk colors it. The check erases each line's last character and types it
// back. The rows' runs are as they were after every character typed back.
static void language_sample_checked(const LanguageSample *sample)
{
    tabs_closed(sample->name, sample->text, NULL, NULL);
    if (!tab_opened(sample->name))
    {
        cli_check(0, sample->name);
        return;
    }
    // each line's length in bytes, the lines WINDOW_SAMPLE_LINES at most
    unsigned int lengths[WINDOW_SAMPLE_LINES];
    unsigned int rows = 0u;
    unsigned int length = 0u;
    for (const char *at = sample->text; (*at != '\0') && (rows < WINDOW_SAMPLE_LINES); at += 1)
    {
        if (*at == '\n')
        {
            lengths[rows] = length;
            rows += 1u;
            length = 0u;
        }
        else
        {
            length += 1u;
        }
    }
    (void)row_runs_waited(rows - 1u, 3000ULL);
    unsigned int before[WINDOW_SAMPLE_LINES][2u * WINDOW_SAMPLE_RUNS];
    unsigned int before_counts[WINDOW_SAMPLE_LINES];
    int same = sample_runs_copied(rows, before, before_counts) == rows;
    const char *line = sample->text;
    for (unsigned int row = 0u; same && (row < rows); row += 1u)
    {
        const char last[2] = {line[lengths[row] - 1u], '\0'};
        cursor_put(row, lengths[row]);
        key_pressed("Backspace");
        stepped_for(150ULL);
        typed(last);
        stepped_for(150ULL);
        (void)row_runs_waited(row, 3000ULL);
        same = sample_runs_same(rows, before, before_counts);
        if (!same)
        {
            // Write the rows' runs before and after the character typed back beside the check.
            unsigned int after[WINDOW_SAMPLE_LINES][2u * WINDOW_SAMPLE_RUNS];
            unsigned int after_counts[WINDOW_SAMPLE_LINES];
            (void)sample_runs_copied(rows, after, after_counts);
            (void)fprintf(stderr, "  %s row %u retyped '%s'\n", sample->name, row, last);
            for (unsigned int each = 0u; each < rows; each += 1u)
            {
                (void)fprintf(stderr, "    row %u before %u:", each, before_counts[each]);
                for (unsigned int at = 0u; at < before_counts[each]; at += 1u)
                {
                    (void)fprintf(stderr, " %u:%u", before[each][2u * at], before[each][(2u * at) + 1u]);
                }
                (void)fprintf(stderr, " after %u:", after_counts[each]);
                for (unsigned int at = 0u; at < after_counts[each]; at += 1u)
                {
                    (void)fprintf(stderr, " %u:%u", after[each][2u * at], after[each][(2u * at) + 1u]);
                }
                (void)fprintf(stderr, "\n");
            }
        }
        line += lengths[row] + 1u;
    }
    cli_check(same, sample->name);
    (void)asked("close-all", NULL);
    (void)asked("close-all", NULL);
}

// Every language plugin's sample: an edit to a line leaves every line's colors as they were.
static void languages_colored_checked(void)
{
    for (unsigned int at = 0u; at < (sizeof(s_language_samples) / sizeof(s_language_samples[0])); at += 1u)
    {
        language_sample_checked(&s_language_samples[at]);
    }
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

// A file opens through the interface with a line and a column after its path, each a colon and digits counted from
// 1. The cursor goes there. With a line alone, the cursor goes to the line's start.
static void place_checked(void)
{
    tabs_closed("placed.txt", "one\ntwo\nthree\n", NULL, NULL);
    char path[CLI_PATH_MAX_LEN];
    tree_path("placed.txt:2:3", path);
    cli_check(asked("file", path), "a path with a line and a column opens through the interface");
    const int displayed = cli_view_displayed();
    char displayed_path[CLI_PATH_MAX_LEN];
    cli_check((displayed >= 0) &&
                  cli_view_path_at(&(CliViewAtCfg){.at = (unsigned int)displayed, .path_out = displayed_path}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = displayed_path, .cap = CLI_POOL_BYTES, .other = "placed.txt", .other_cap = 11u}),
              "the file of the path before the line and the column is as the tab displayed");
    const CliViewFrame *const frame = editor_frame();
    cli_check((frame != NULL) && (frame->cursor.line == 1ULL) && (frame->cursor.col == 2ULL),
              "the cursor is at the line and the column");
    tree_path("placed.txt:3", path);
    (void)asked("file", path);
    cli_check((editor_frame() != NULL) && (editor_frame()->cursor.line == 2ULL) && (editor_frame()->cursor.col == 0ULL),
              "a line alone puts the cursor at the line's start");
}

// A table file: the frame carries the delimiter character and the column offsets. The editor positions the caret at
// a field's column offset, the second field five cells right of the first.
// Return the caret's ink as the window draws it at the moment `now_ms`.
static unsigned int caret_ink_at(unsigned long long now_ms)
{
    g_test_clock_ms = now_ms;
    (void)EMBED_CALL(gui_window_table.caret_draw, GuiWindowCfg, .window = &s_window);
    g_test_clock_ms = GUI_TEST_CLOCK_MACHINE;
    return s_window.caret_color;
}

// A key shows the caret through the input rest. Then the caret blinks: it shows for the first half of each blink and
// hides for the second.
static void caret_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the caret");
    const unsigned long long typed_ms = cli_clock_milliseconds();
    typed("x");
    const unsigned long long begun_ms = s_window.caret_blink_start_ms;
    cli_check(begun_ms >= typed_ms + GUI_CARET_INPUT_REST_MS, "a key puts the caret's blink past the input rest");
    cli_check(caret_ink_at(begun_ms - 1ULL) == 255u, "the caret is displayed through the input rest");
    cli_check(caret_ink_at(begun_ms) == 255u, "displayed as its blink begins");
    cli_check(caret_ink_at(begun_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL) - 1ULL) == 255u,
              "displayed through the first half of the blink");
    cli_check(caret_ink_at(begun_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL)) == 0u, "hidden from its second half");
    cli_check(caret_ink_at(begun_ms + GUI_CARET_BLINK_PERIOD_MS - 1ULL) == 0u, "hidden to the blink's end");
    cli_check(caret_ink_at(begun_ms + GUI_CARET_BLINK_PERIOD_MS) == 255u, "displayed again as the next blink begins");
    cli_check(GUI_CARET_BLINK_PERIOD_MS == 1060ULL, "a blink takes 1.06 seconds");
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_BLUR});
    cli_check(caret_ink_at(begun_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL)) == 255u,
              "with another window holding the keys, the caret stands displayed");
    const unsigned long long focused_ms = cli_clock_milliseconds();
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_FOCUS});
    const unsigned long long back_ms = s_window.caret_blink_start_ms;
    cli_check(!s_window.blurred, "the keys come back to the window");
    cli_check(back_ms >= focused_ms + GUI_CARET_INPUT_REST_MS, "the caret's blink begins again after the input rest");
    cli_check(caret_ink_at(back_ms + (GUI_CARET_BLINK_PERIOD_MS / 2ULL)) == 0u, "and the caret blinks again");
}

// the pointer moved to x, y as the system passes it on
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

// The explorer stays while the pointer works in it. With the claim on, a rest in the left band brings the side_menu
// and the side. A pointer that moves from the side_menu onto the side's rows arms no hide.
static void explorer_stays_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the explorer");
    typed("x");
    cli_check(s_window.claim.autohidden, "typing claims the edges");
    moved(2, 400);
    stepped_for(GUI_AUTOHIDE_SHOW_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_LEFT, "a rest in the left band shows the left edge");
    // the side_menu's first tool, at its button's middle
    const GuiRect tool = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = &s_window.frame, .at = 0u);
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    stepped_for(GUI_SIDES_ICON_REST_MS + GUI_SIDES_SLIDE_MS + 100ULL);
    cli_check(s_window.frame.side_displayed, "a rest on the side_menu's tool shows its side");
    const GuiRect side = s_window.frame.side;
    moved(side.x + (side.width / 2), side.y + (side.height / 2));
    cli_check(s_window.claim.hide_deadline_ms == 0ULL, "the pointer on the side's rows arms no hide of the left edge");
    stepped_for(GUI_AUTOHIDE_HIDE_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check((s_window.claim.displayed == GUI_AUTOHIDE_LEFT) && s_window.frame.side_displayed,
              "the left edge and the side are while the pointer rests on the side");
}

// The minimap's lines are beside the view wherever it is. A tall file opened has its map's lines taken at once. The
// view placed deep in the file has the lines of the map's rows there, with the map away or displayed.
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
    unsigned long long held_from = 0ULL;
    unsigned int held_count = 0u;
    cli_check(map->lines && cli_view_map_lines(&(CliViewMapCfg){.from_out = &held_from, .count_out = &held_count}) &&
                  (held_from == 0u) && (held_count > 1u),
              "the map's lines are taken as the file opens, from its first line");
    char path[CLI_PATH_MAX_LEN];
    tree_path("tall.c:2500", path);
    cli_check(asked("file", path), "the view is placed deep in the file");
    unsigned long long first = 0ULL;
    unsigned long long last = 0ULL;
    cli_check(EMBED_CALL(gui_minimap_first.span, GuiMinimapFirstCfg, .map_read = map, .first = &first, .last = &last) &&
                  (first > 0ULL),
              "the map's first row follows the view down the file");
    cli_check(map->lines && cli_view_map_lines(&(CliViewMapCfg){.from_out = &held_from, .count_out = &held_count}) &&
                  (held_from == first) && (held_from + (unsigned long long)held_count == last + 1ULL),
              "the map's lines are the lines its rows are for");
}

// The right edge comes on a rest and goes after the pointer leaves it for the text. With the claim on, a rest in the
// right band shows the map. The pointer moved into the text takes it away after the hide's rest.
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
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_RIGHT, "the pointer on the map retains it");
    const GuiRect editor = s_window.frame.editor;
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    stepped_for(GUI_AUTOHIDE_HIDE_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.displayed == GUI_AUTOHIDE_NONE, "the pointer moved into the text takes the map away");
}

// the button `button` pressed at x, y as the system passes it on
static void pressed_with(int x, int y, unsigned int button)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_DOWN, .x = x, .y = y, .button = button});
    stepped();
}

// the button `button` let go at x, y as the system passes it on
static void released_with(int x, int y, unsigned int button)
{
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_UP, .x = x, .y = y, .button = button});
    stepped();
}

// the caret's rectangle as the editor positions it for the frame displayed, drawn or not
static GuiRect caret_box(void)
{
    stepped();
    return EMBED_CALL(gui_editor_position.caret_rect, GuiEditorPositionCfg, .editor_read = editor_framed(),
                      .glyphs = &s_window.glyphs, .place = s_window.frame.editor, .scale = s_window.frame.scale);
}

// the caret's left pixel
static int caret_x(void)
{
    return caret_box().x;
}

// the pixel `cells` cells right of the line's start `start`, each `cell` pixels wide, as the editor rounds it
static int cell_x(int start, float cell, int cells)
{
    return start + (int)(((float)cells * cell) + 0.5f);
}

// whether the pixel `x` is within a pixel of `cells` cells right of `start`
static int cell_near(int x, int start, float cell, int cells)
{
    const int gap = x - cell_x(start, cell, cells);
    return (gap >= -1) && (gap <= 1);
}

// Return the pixel of the displayed line's first cell at the frame displayed, as the editor positions it. It is the
// text's edge past the gutter, moved left by the cells the view slid past. The part of a cell the scroll is past them
// moves it left as well. The editor's rectangle moves as the keys' claim slides the edges. The check reads the start
// at the caret's own frame.
static int line_start_x(float cell)
{
    const CliViewFrame *const frame = editor_frame();
    const int gutter = EMBED_CALL(gui_editor_position_cell.gutter, GuiEditorPositionCellCfg,
                                  .editor_read = editor_framed(), .cell = cell, .scale = s_window.frame.scale);
    const float slid = (float)((frame != NULL) ? frame->left : 0ULL) * cell;
    return (int)(((float)(s_window.frame.editor.x + gutter) - slid - s_window.editor.scroll_x) + 0.5f);
}

// Return whether the caret is `cells` cells right of the line's start, both read at the frame displayed. If it is
// not, write the cursor, the caret and the cell beside the check.
static int caret_at_cells(float cell, int cells)
{
    const int x = caret_x();
    const int start = line_start_x(cell);
    const CliViewFrame *const frame = editor_frame();
    const int near = (frame != NULL) && cell_near(x, start, cell, cells);
    if (!near)
    {
        (void)fprintf(stderr, "  caret: cursor %llu:%llu, x %d, start %d, cell %.3f, cells %d, keys %s\n",
                      (frame != NULL) ? frame->cursor.line : 0ULL, (frame != NULL) ? frame->cursor.col : 0ULL, x, start,
                      (double)cell, cells, s_window.editing ? "editing" : "not editing");
    }
    return near;
}

static void table_checked(void)
{
    tabs_closed("grid.tsv", "a\tbb\tc\nxxxx\ty\tz\n", NULL, NULL);
    cli_check(tab_opened("grid.tsv"), "a tsv file opens as a table");
    const CliViewFrame *const frame = editor_frame();
    cli_check((frame != NULL) && (frame->table == '\t') && (frame->table_quote == 0u) && (frame->column_count == 3u) &&
                  (frame->column_offset[1] == 5ULL) && (frame->column_offset[2] == 8ULL),
              "the frame carries the tab delimiter and three column offsets, the second at cell 5 and the third at 8");
    // a cell's width as the glyphs give it, fractional where the face's advance is
    const float cell =
        EMBED_CALL(gui_glyphs_face_character.cell, GuiGlyphsFaceLetterCfg, .glyphs = &s_window.glyphs,
                   .size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                    .pixels = (int)GUI_CODE_SIZE, .scale = s_window.frame.scale));
    key_pressed("Right");
    key_pressed("Right");
    cli_check((cell > 0.0f) && caret_at_cells(cell, 5),
              "the caret at the second field's first character is five cells right of the line's start");
    key_pressed("End");
    cli_check(caret_at_cells(cell, 9), "the caret at the line's end is at cell 9");
    cli_check((editor_frame() != NULL) && (editor_frame()->column_offset[3] == 10ULL),
              "the offset past the last column is at cell 10");
    // the second column's line, at cell 8, dragged to cell 10 widens the second column from 2 cells to 5
    const GuiRect caret = caret_box();
    const int first_x = line_start_x(cell);
    const int row_y = caret.y + (caret.height / 2);
    pressed_with(cell_x(first_x, cell, 8), row_y, GUI_BUTTON_LEFT);
    cli_check(s_window.column_drag == 2u, "a press on the second column's line grips it");
    moved(cell_x(first_x, cell, 10), row_y);
    released_with(cell_x(first_x, cell, 10), row_y, GUI_BUTTON_LEFT);
    cli_check((s_window.column_drag == 0u) && (editor_frame() != NULL) && (editor_frame()->column_offset[2] == 11ULL),
              "the line let go leaves the third column's offset at cell 11");
}

// A middle press in the text starts a scroll that follows the pointer's distance from the press point. The pointer
// below the press point scrolls the view down each frame, and one above it scrolls it back up. The next press ends
// it. A middle button let go at the press point leaves the scroll to run. One let go after a drag past the rest
// pixels, in the window or out of it, ends the scroll. The text is the tall file the map's check opened, placed deep
// in it.
static void autoscroll_checked(void)
{
    const int displayed = cli_view_displayed();
    cli_check(displayed >= 0, "the tall file is displayed for the autoscroll");
    char key[CLI_NAME_MAX_LEN];
    if ((displayed < 0) || !cli_view_key(&(CliViewAtCfg){.at = (unsigned int)displayed, .key_out = key}))
    {
        return;
    }
    // the view placed deep in the file comes to rest there first
    stepped_for(GUI_SCROLL_STEP_MAX_MS * 10ULL);
    const unsigned long long start = cli_view_top(&(CliViewAtCfg){.key = key});
    const GuiRect editor = s_window.frame.editor;
    const int x = editor.x + (editor.width / 2);
    const int y = editor.y + (editor.height / 2);
    pressed_with(x, y, GUI_BUTTON_MIDDLE);
    cli_check(s_window.autoscroll, "a middle press in the text starts the autoscroll");
    stepped_for(200ULL);
    cli_check((cli_view_top(&(CliViewAtCfg){.key = key}) == start) && (s_window.pointer.shape == GUI_POINTER_MOVE),
              "the pointer still at the press point scrolls nothing, and the compass lights no arrow");
    moved(x, y + 100);
    stepped_for(300ULL);
    const unsigned long long down = cli_view_top(&(CliViewAtCfg){.key = key});
    cli_check((down > start) && (s_window.pointer.shape == GUI_POINTER_GLIDE_S),
              "the pointer held below the press point scrolls the view down, the compass's south lit");
    moved(x, y - 100);
    stepped_for(300ULL);
    cli_check((cli_view_top(&(CliViewAtCfg){.key = key}) < down) && (s_window.pointer.shape == GUI_POINTER_GLIDE_N),
              "the pointer held above the press point scrolls it back up, the compass's north lit");
    unsigned int ways = 0u;
    moved(x + 100, y + 100);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_SE) ? 1u : 0u;
    moved(x - 100, y - 100);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_NW) ? 1u : 0u;
    moved(x + 100, y - 100);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_NE) ? 1u : 0u;
    moved(x - 100, y + 100);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_SW) ? 1u : 0u;
    moved(x + 100, y + 30);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_E) ? 1u : 0u;
    moved(x - 100, y - 30);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_W) ? 1u : 0u;
    moved(x + 30, y - 100);
    ways += (s_window.pointer.shape == GUI_POINTER_GLIDE_N) ? 1u : 0u;
    cli_check((ways == 7u) && (s_window.autoscroll_pointer_x == (x + 30)),
              "each of the eight ways lights its own arrow, a way within an eighth of a half turn of north its north");
    moved(x, y - 100);
    moved(x, y + 100);
    pressed_with(x, y + 100, GUI_BUTTON_LEFT);
    cli_check(!s_window.autoscroll, "the next press ends the autoscroll");
    // the scroll eases to where the autoscroll last put it, then is
    stepped_for(GUI_SCROLL_STEP_MAX_MS * 10ULL);
    const unsigned long long ended = cli_view_top(&(CliViewAtCfg){.key = key});
    stepped_for(200ULL);
    cli_check(cli_view_top(&(CliViewAtCfg){.key = key}) == ended, "the view is once it ends");
    // a click: the button let go within the rest pixels of the press point leaves the scroll to run
    pressed_with(x, y, GUI_BUTTON_MIDDLE);
    moved(x, y + GUI_AUTOSCROLL_REST_PIXELS);
    released_with(x, y + GUI_AUTOSCROLL_REST_PIXELS, GUI_BUTTON_MIDDLE);
    cli_check(s_window.autoscroll, "the middle button let go at the press point leaves the autoscroll to run");
    pressed_with(x, y, GUI_BUTTON_LEFT);
    cli_check(!s_window.autoscroll, "the next press ends it");
    // a drag: the pointer past the rest pixels, let go below the window's bottom edge
    pressed_with(x, y, GUI_BUTTON_MIDDLE);
    moved(x, y + 100);
    stepped_for(300ULL);
    cli_check(cli_view_top(&(CliViewAtCfg){.key = key}) > ended, "the drag scrolls the view down");
    // the frame's height is a pixel count well inside an int
    const int below = (int)s_window.frame.height + 40;
    moved(x, below);
    released_with(x, below, GUI_BUTTON_MIDDLE);
    cli_check(!s_window.autoscroll,
              "the middle button let go after a drag ends the autoscroll, outside the window too");
    stepped_for(GUI_SCROLL_STEP_MAX_MS * 10ULL);
    const unsigned long long dragged = cli_view_top(&(CliViewAtCfg){.key = key});
    stepped_for(200ULL);
    cli_check(cli_view_top(&(CliViewAtCfg){.key = key}) == dragged, "the view is once the drag ends");
    // a middle press on the minimap starts the autoscroll as a middle press in the text does
    const GuiRect map = s_window.frame.map;
    const int map_x = map.x + (map.width / 2);
    const int map_y = map.y + (map.height / 2);
    pressed_with(map_x, map_y, GUI_BUTTON_MIDDLE);
    cli_check((map.width > 0) && s_window.autoscroll, "a middle press on the minimap starts the autoscroll");
    moved(map_x, map_y + 100);
    stepped_for(300ULL);
    cli_check(cli_view_top(&(CliViewAtCfg){.key = key}) > dragged,
              "the pointer held below the minimap's press point scrolls the view down");
    pressed_with(map_x, map_y + 100, GUI_BUTTON_LEFT);
    cli_check(!s_window.autoscroll, "the next press ends the minimap's autoscroll");
}

static int same(const char *text, const char *required)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = text, .other = required, .cap = CLI_POOL_BYTES});
}

// A rest on a button shows its tip. The pointer moved onto the first tool shows nothing at once. As GUI_TIP_REST_MS
// pass, it shows the tool's label and keys in a box under the button. Moved onto the minimize control, it shows the
// control's name. Moved into the text, the tip goes. A press on a button whose tip shows takes the tip away.
static void tips_checked(void)
{
    const GuiRect tool = EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = &s_window.frame, .at = 0u);
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    cli_check(!s_window.tip.displayed, "a tip waits for the rest on the button");
    stepped_for(GUI_TIP_REST_MS + 100ULL);
    cli_check(s_window.tip.displayed, "the rest on a tool shows its tip");
    cli_check((s_window.tip.label != NULL) && same(s_window.tip.label, g_gui_tools[0].label),
              "the tip carries the tool's label");
    cli_check((s_window.tip.keys != NULL) && same(s_window.tip.keys, g_gui_tools[0].keys),
              "the tip carries the tool's keys");
    // The box's rectangle comes from a drawn frame. The check draws one to read it.
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &s_window);
    cli_check((s_window.tip.rect.width > 0) && (s_window.tip.rect.x >= tool.x + tool.width) &&
                  (s_window.tip.rect.y < tool.y + tool.height) &&
                  (s_window.tip.rect.y + s_window.tip.rect.height > tool.y),
              "a side_menu tool's tip is at the button's right, on its row");
    const GuiRect minimize =
        EMBED_CALL(gui_controls.rect, GuiControlsCfg, .frame = &s_window.frame, .which = GUI_CONTROL_MINIMIZE);
    moved(minimize.x + (minimize.width / 2), minimize.y + (minimize.height / 2));
    cli_check(!s_window.tip.displayed, "the pointer moved onto another button starts the rest again");
    stepped_for(GUI_TIP_REST_MS + 100ULL);
    cli_check(s_window.tip.displayed && (s_window.tip.label != NULL) && same(s_window.tip.label, "Minimize"),
              "the rest on the minimize control names it");
    const GuiRect editor = s_window.frame.editor;
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    cli_check(!s_window.tip.displayed && (s_window.tip.label == NULL), "the tip goes as the pointer leaves the button");
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    stepped_for(GUI_TIP_REST_MS + 100ULL);
    cli_check(s_window.tip.displayed, "the tip shows again on the tool");
    pressed_with(tool.x + (tool.width / 2), tool.y + (tool.height / 2), GUI_BUTTON_LEFT);
    cli_check(!s_window.tip.displayed, "a press takes the tip away");
    released_with(tool.x + (tool.width / 2), tool.y + (tool.height / 2), GUI_BUTTON_LEFT);
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    // The window without the focus: a rest on a tool still lights it and shows its tip. The rest is long past the
    // side pane's own waits.
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_BLUR});
    stepped();
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    stepped_for(GUI_TIP_REST_MS + 100ULL);
    cli_check(s_window.tip.displayed, "the rest on a tool shows its tip while the window lacks the focus");
    stepped_for(1500ULL);
    cli_check(s_window.tip.displayed && (s_window.side_menu.pointed == 0),
              "the tip is and the tool stays lit through a longer rest without the focus");
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_FOCUS});
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    stepped();
}

// The bar's tools before the controls. The pointer on the Preferences tool lights it, and at rest names it with its
// keys. A press on it opens Preferences. The pointer off it unlights it. A press on Notifications presses the status
// bar's notice and leaves the window located.
static void bar_tools_checked(void)
{
    const GuiRect preferences = EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = &s_window.frame,
                                           .which = GUI_TOP_MENU_TOOL_PREFERENCES);
    cli_check((preferences.width > 0) && (preferences.x + preferences.width < s_window.frame.controls.x),
              "the Preferences tool is before the controls");
    const int x = preferences.x + (preferences.width / 2);
    const int y = preferences.y + (preferences.height / 2);
    moved(x, y);
    cli_check(s_window.top_menu_tool_pointed == GUI_TOP_MENU_TOOL_PREFERENCES,
              "the pointer on the Preferences tool lights it");
    stepped_for(GUI_TIP_REST_MS + 100ULL);
    cli_check(s_window.tip.displayed && (s_window.tip.label != NULL) && same(s_window.tip.label, "Preferences") &&
                  (s_window.tip.keys != NULL) && same(s_window.tip.keys, "Ctrl+,"),
              "the rest on the Preferences tool names it with its keys");
    pressed_with(x, y, GUI_BUTTON_LEFT);
    released_with(x, y, GUI_BUTTON_LEFT);
    cli_check(s_window.preferences.displayed, "a press on the Preferences tool opens Preferences");
    EMBED_CALL(gui_window_table.preferences_close, GuiWindowCfg, .window = &s_window);
    stepped();
    cli_check(!s_window.preferences.displayed, "Preferences closed again");
    const GuiRect editor = s_window.frame.editor;
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
    cli_check(s_window.top_menu_tool_pointed == GUI_TOP_MENU_TOOL_NONE, "the pointer off the tools lights none");
    const GuiRect bell = EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = &s_window.frame,
                                    .which = GUI_TOP_MENU_TOOL_NOTIFICATIONS);
    pressed_with(bell.x + (bell.width / 2), bell.y + (bell.height / 2), GUI_BUTTON_LEFT);
    released_with(bell.x + (bell.width / 2), bell.y + (bell.height / 2), GUI_BUTTON_LEFT);
    stepped();
    cli_check(!s_window.closing, "a press on Notifications leaves the window located");
    moved(editor.x + (editor.width / 2), editor.y + (editor.height / 2));
}

// Preferences' own window. The system sizes it. It owes a frame until the window draws it into its own raster. The
// check reads what a pixel of it is on. A press on a section's tab in it shows that section.
static void preferences_window_checked(void)
{
    key_pressed("Ctrl+,");
    cli_check(s_window.preferences.displayed, "Ctrl+, opens Preferences");
    EMBED_CALL(gui_window_width.preferences_resize, GuiWindowWidthCfg, .window = &s_window, .width = 900u,
               .height = 700u, .system_scale = GUI_SCALE_ONE);
    cli_check((s_window.preferences.width == 900u) && (s_window.preferences.height == 700u) &&
                  s_window.preferences_pending && (s_window.preferences_raster.pixels != NULL),
              "Preferences sized by the system owes a frame and has its raster");
    EMBED_CALL(gui_window_table.preferences_draw, GuiWindowCfg, .window = &s_window);
    unsigned int drawn = 0u;
    for (unsigned int at = 0u; at < (900u * 700u); at += 97u)
    {
        drawn |= s_window.preferences_raster.pixels[((at / 900u) * s_window.preferences_raster.width) + (at % 900u)];
    }
    cli_check(!s_window.preferences_pending && (drawn != 0u), "Preferences drawn into its raster owes no frame");
    const GuiRect close =
        EMBED_CALL(gui_preferences_row.close_rect, GuiPreferencesRowCfg, .preferences_read = &s_window.preferences);
    const GuiRect second = EMBED_CALL(gui_preferences_palette_values.section_rect, GuiPreferencesPaletteValuesCfg,
                                      .preferences_read = &s_window.preferences, .at = 1u);
    cli_check((EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &s_window,
                          .input = &(GuiInput){.x = 450, .y = 1}) == GUI_HIT_TOP) &&
                  (EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &s_window,
                              .input = &(GuiInput){.x = close.x + (close.width / 2),
                                                   .y = close.y + (close.height / 2)}) == GUI_HIT_CLIENT) &&
                  (EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &s_window,
                              .input = &(GuiInput){.x = second.x + (second.width / 2),
                                                   .y = second.y + (second.height / 2)}) == GUI_HIT_CLIENT) &&
                  (EMBED_CALL(cli_window_input.preferences_hit, GuiWindowInputCfg, .window_read = &s_window,
                              .input = &(GuiInput){.x = 450, .y = 350}) == GUI_HIT_CLIENT),
              "Preferences' top edge resizes it, and its close mark, its tabs and its body are its own");
    const int x = second.x + (second.width / 2);
    const int y = second.y + (second.height / 2);
    EMBED_CALL(gui_window_input_type.preferences_input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_DOWN, .x = x, .y = y, .button = GUI_BUTTON_LEFT});
    EMBED_CALL(gui_window_input_type.preferences_input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_UP, .x = x, .y = y, .button = GUI_BUTTON_LEFT});
    stepped();
    cli_check(s_window.preferences.section == 1u, "a press on the second section's tab shows that section");
    cli_check(s_window.preferences_pointer.shown, "the pointer over Preferences shows on Preferences' surface");
    EMBED_CALL(gui_window_table.preferences_close, GuiWindowCfg, .window = &s_window);
    stepped();
    cli_check(!s_window.preferences.displayed, "Preferences closed again");
    // a closed window sends no leave: the close itself takes its pointer off, and the main window's pointer follows
    cli_check(!s_window.preferences_pointer.shown, "Preferences closed takes its pointer off with it");
}

// the inputs the window's own pointer drove, each taken as the view's thread takes the system's own
static void driven_taken(void)
{
    for (unsigned int at = 0u; at < s_window.driven_count; at += 1u)
    {
        EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
                   .input = &s_window.driven[at]);
    }
    s_window.driven_count = 0u;
    stepped();
}

// The mouse's own motion while the window reads raw input. Each relative input adds its counts to the window's sums,
// across and down apart. No relative input moves the window's pointer.
static void raw_checked(void)
{
    const int x = s_window.pointer.x;
    const int y = s_window.pointer.y;
    s_window.relative_x = 0LL;
    s_window.relative_y = 0LL;
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
                   .input = &(GuiInput){.type = GUI_INPUT_RELATIVE, .x = 5, .y = -3});
    }
    stepped();
    cli_check((s_window.relative_x == 50LL) && (s_window.relative_y == -30LL) && (s_window.pointer.x == x) &&
                  (s_window.pointer.y == y),
              "ten relative inputs of 5 across and 3 up add up to 50 and -30, and the pointer stays where it is");
}

// The pointer the window draws. The checks read its shape over the text, over a tool of the bar and on the border.
// They read the sprite in the drawn frame, the pointer off the window on a leave, and the inputs the interface's
// pointer command drives.
static void pointer_checked(void)
{
    tabs_closed("pointed.txt", "one two three four five six seven eight nine ten\nsecond line of text\n", NULL, NULL);
    cli_check(tab_opened("pointed.txt"), "a file of text opens for the pointer");
    const GuiRect editor = s_window.frame.editor;
    const int text_x = editor.x + (editor.width / 2);
    const int text_y = editor.y + (editor.height / 2);
    moved(text_x, text_y);
    cli_check(s_window.pointer.shown && (s_window.pointer.shape == GUI_POINTER_TEXT) && (s_window.pointer.x == text_x),
              "over the text the pointer is the beam at the pointer's place");
    // The pointer is on a surface of its own, and a frame draws none of it. The frame's pixel at the beam's hot point
    // is the text's. A beam drawn over a copy of the frame changes that pixel, and the frame with the beam there does
    // not.
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &s_window);
    const unsigned int hot = ((unsigned int)text_y * s_window.raster->width) + (unsigned int)text_x;
    const GuiColor framed = s_window.raster->pixels[hot];
    (void)EMBED_CALL(gui_pointer_raster.draw, GuiPointerRasterCfg, .pointer = &s_window.pointer,
                     .raster = s_window.raster, .palette = s_window.palette, .scale = s_window.frame.scale);
    const GuiColor glassed = s_window.raster->pixels[hot];
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &s_window);
    cli_check((glassed != framed) && (s_window.raster->pixels[hot] == framed),
              "a frame draws no pointer: its pixel at the beam's hot point is the text's alone");
    const GuiRect tool = EMBED_CALL(gui_top_menu.tool_rect, GuiTopMenuCfg, .frame = &s_window.frame,
                                    .which = GUI_TOP_MENU_TOOL_PREFERENCES);
    moved(tool.x + (tool.width / 2), tool.y + (tool.height / 2));
    cli_check(s_window.pointer.shape == GUI_POINTER_HAND, "over a tool of the bar the pointer is the hand");
    moved(1, editor.y + (editor.height / 2));
    cli_check(s_window.pointer.shape == GUI_POINTER_RESIZE_EW, "on the left border the pointer is the arrow both ways");
    cli_check(asked("run-view", NULL) && (s_window.side_menu.foot != GUI_SIDE_MENU_NONE),
              "the Run tool opens its pane at the foot");
    stepped();
    const GuiRect foot = s_window.frame.terminal;
    moved(foot.x + (foot.width / 2), foot.y);
    cli_check((foot.height > 0) && (s_window.pointer.shape == GUI_POINTER_RESIZE_NS),
              "on the foot pane's top line the pointer is the arrow up and down");
    cli_check(asked("run-view", NULL) && (s_window.side_menu.foot == GUI_SIDE_MENU_NONE),
              "and the Run tool closes its pane again");
    stepped();
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window, .input = GUI_LEFT_INPUT());
    cli_check(!s_window.pointer.shown, "a leave takes the pointer off the window");

    const char *const place[2] = {"300", "260"};
    cli_check(asked_words("pointer", place, 2u) && (s_window.driven_count == 1u) &&
                  (s_window.driven[0].type == GUI_INPUT_MOVED) && (s_window.driven[0].x == 300) && s_window.pointer_own,
              "the pointer command queues a move to its place, the window's own pointer on");
    const char *const click[2] = {"click", "right"};
    cli_check(asked_words("pointer", click, 2u) && (s_window.driven_count == 3u) &&
                  (s_window.driven[1].type == GUI_INPUT_DOWN) && (s_window.driven[2].type == GUI_INPUT_UP) &&
                  (s_window.driven[1].button == GUI_BUTTON_RIGHT) && (s_window.driven[1].x == 300) &&
                  (s_window.driven[1].y == 260),
              "a click queues a press and a release of its button where the queued move puts the pointer");
    s_window.driven_count = 1u;
    driven_taken();
    cli_check(s_window.pointer.shown && (s_window.pointer.x == 300) && (s_window.pointer.y == 260),
              "the queued move taken puts the pointer at its place");
    const char *const drag[5] = {"drag", "320", "280", "340", "300"};
    cli_check(asked_words("pointer", drag, 5u) && (s_window.driven_count == 4u) &&
                  (s_window.driven[0].type == GUI_INPUT_DOWN) && (s_window.driven[1].type == GUI_INPUT_MOVED) &&
                  (s_window.driven[2].x == 340) && (s_window.driven[3].type == GUI_INPUT_UP) &&
                  (s_window.driven[3].y == 300),
              "a drag queues a press, a move to each place and a release at the last");
    s_window.driven_count = 0u;
    const char *const wheel[3] = {"wheel", "up", "2"};
    cli_check(asked_words("pointer", wheel, 3u) && (s_window.driven_count == 1u) &&
                  (s_window.driven[0].type == GUI_INPUT_WHEEL) && (s_window.driven[0].rows == -2) &&
                  (s_window.driven[0].wheel_down == (-2 * GUI_WHEEL_UNITS)),
              "the wheel up queues its notches toward the top");
    s_window.driven_count = 0u;
    const char *const wrong[2] = {"wheel", "sideways"};
    cli_check(!asked_words("pointer", wrong, 2u) && (s_window.driven_count == 0u),
              "words the pointer command does not know queue nothing");
    cli_check(asked("pointer", "off") && !s_window.pointer_own, "off gives the pointer back to the system");

    // A move to a control by its name goes at once if the speed is 0. The checks move to the strip's Run tool, a
    // window control, a menu by its title and a tab by its name. Each move ends at the middle of the control's
    // rectangle. The window refuses a type it does not know.
    unsigned int run_tool = g_gui_tools_count;
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        run_tool = same(g_gui_tools[at].name, "run") ? at : run_tool;
    }
    const GuiRect run_button =
        EMBED_CALL(gui_side_menu.button, GuiSideMenuCfg, .frame = &s_window.frame, .at = run_tool);
    s_window.driven_count = 0u;
    const char *const to_tool[5] = {"to", "tool", "run", "--speed", "0"};
    cli_check(asked_words("pointer", to_tool, 5u) && (s_window.driven_count == 1u) &&
                  (s_window.driven[0].type == GUI_INPUT_MOVED) &&
                  (s_window.driven[0].x == run_button.x + (run_button.width / 2)) &&
                  (s_window.driven[0].y == run_button.y + (run_button.height / 2)),
              "to tool run queues a move to the middle of the Run tool's button");
    const GuiRect close_box =
        EMBED_CALL(gui_controls.rect, GuiControlsCfg, .frame = &s_window.frame, .which = GUI_CONTROL_CLOSE);
    s_window.driven_count = 0u;
    const char *const to_control[5] = {"to", "control", "close", "--speed", "0"};
    cli_check(asked_words("pointer", to_control, 5u) && (s_window.driven_count == 1u) &&
                  (s_window.driven[0].x == close_box.x + (close_box.width / 2)) &&
                  (s_window.driven[0].y == close_box.y + (close_box.height / 2)),
              "to control close queues a move to the middle of the close control");
    const GuiRect file_title = s_window.menubar.title[0].rect;
    s_window.driven_count = 0u;
    const char *const to_menu[5] = {"to", "menu", "File", "--speed", "0"};
    cli_check(asked_words("pointer", to_menu, 5u) && (s_window.driven_count == 1u) &&
                  EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = file_title,
                             .x = s_window.driven[0].x, .y = s_window.driven[0].y),
              "to menu File queues a move inside the File title");
    s_window.driven_count = 0u;
    const char *const to_tab[5] = {"to", "tab", "pointed.txt", "--speed", "0"};
    cli_check(asked_words("pointer", to_tab, 5u) && (s_window.driven_count == 1u) &&
                  EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = s_window.frame.tabs,
                             .x = s_window.driven[0].x, .y = s_window.driven[0].y),
              "to tab pointed.txt queues a move inside the tabs row");
    s_window.driven_count = 0u;
    const char *const to_nowhere[3] = {"to", "nowhere", "x"};
    cli_check(!asked_words("pointer", to_nowhere, 3u) && (s_window.driven_count == 0u),
              "a type the window does not know is refused");

    // At a speed, the pointer moves one frame at a time. Each step moves it no further than the speed covers. The
    // glide ends at the control's middle.
    const char *const far_place[2] = {"1000", "800"};
    (void)asked_words("pointer", far_place, 2u);
    driven_taken();
    const char *const to_slow[5] = {"to", "tool", "run", "--speed", "400"};
    cli_check(asked_words("pointer", to_slow, 5u) && s_window.glide[0].active && (s_window.glide[0].speed == 400u),
              "to tool run at a speed starts a glide at that speed");
    stepped_for(50ULL);
    driven_taken();
    cli_check(s_window.glide[0].active && (s_window.pointer.x < 1000) &&
                  (s_window.pointer.x > run_button.x + run_button.width),
              "the first steps move the pointer part of the way");
    for (unsigned int frame = 0u; (frame < 400u) && s_window.glide[0].active; frame += 1u)
    {
        stepped_for(10ULL);
        driven_taken();
    }
    // The step that ends the glide queues its last move after the suite takes the inputs before it.
    driven_taken();
    cli_check(!s_window.glide[0].active && (s_window.pointer.x == run_button.x + (run_button.width / 2)) &&
                  (s_window.pointer.y == run_button.y + (run_button.height / 2)),
              "the glide ends with the pointer at the middle of the Run tool's button");
    // Put the pointer back where the hands' checks expect it, and give it back to the system.
    (void)asked_words("pointer", place, 2u);
    driven_taken();
    (void)asked("pointer", "off");

    // the hands: pointers beside the window's own, each named through --as
    const char *const named[4] = {"--as", "robot", "120", "80"};
    cli_check(asked_words("pointer", named, 4u) && (s_window.driven_count == 1u) && (s_window.driven[0].hand == 1u) &&
                  (s_window.driven[0].x == 120) && s_window.hands[0].used,
              "--as names a hand: the move queues on the hand, which takes the first free record");
    driven_taken();
    cli_check(s_window.hands[0].shown && (s_window.hands[0].x == 120) && (s_window.hands[0].y == 80) &&
                  !s_window.pointer_own && (s_window.pointer.x == 300),
              "the hand's move taken puts the hand at its place and leaves the window's own pointer alone");
    const char *const other[4] = {"--as", "teacher", "200", "90"};
    cli_check(asked_words("pointer", other, 4u) && (s_window.driven[0].hand == 2u) && s_window.hands[1].used,
              "a second name takes the second hand");
    driven_taken();
    const char *const again[3] = {"--as", "robot", "click"};
    cli_check(asked_words("pointer", again, 3u) && (s_window.driven_count == 2u) && (s_window.driven[0].hand == 1u) &&
                  (s_window.driven[0].type == GUI_INPUT_DOWN) && (s_window.driven[0].x == 120) &&
                  (s_window.driven[0].y == 80),
              "a hand named again is the same hand, and its click is where that hand is");
    driven_taken();
    cli_check((s_window.hands[0].x == 120) && (s_window.hands[1].x == 200), "each hand keeps its own place");
    static GuiColor marked[GUI_POINTER_PICTURE_MAX];
    static GuiColor other_marked[GUI_POINTER_PICTURE_MAX];
    EMBED_CALL(gui_pointer_hand.bake, GuiPointerHandCfg, .shape = GUI_POINTER_ARROW, .scale = s_window.frame.scale,
               .palette = s_window.palette, .fill = s_window.palette->mark[0], .picture = marked);
    EMBED_CALL(gui_pointer_hand.bake, GuiPointerHandCfg, .shape = GUI_POINTER_ARROW, .scale = s_window.frame.scale,
               .palette = s_window.palette, .fill = s_window.palette->mark[1], .picture = other_marked);
    const GuiRect hand_rect = EMBED_CALL(gui_pointer_hand.rect, GuiPointerHandCfg, .shape = GUI_POINTER_ARROW,
                                         .scale = s_window.frame.scale, .x = 120, .y = 80);
    int differs = 0;
    for (int at = 0; at < (hand_rect.width * hand_rect.height); at += 1)
    {
        differs = differs || (marked[at] != other_marked[at]);
    }
    cli_check(differs && (hand_rect.width > 0) && (hand_rect.x <= 120) && (120 < (hand_rect.x + hand_rect.width)) &&
                  (hand_rect.y <= 80) && (80 < (hand_rect.y + hand_rect.height)),
              "a hand's picture is baked in its own mark, and its rectangle holds its hot point");
    const char *const gone[3] = {"--as", "robot", "off"};
    cli_check(asked_words("pointer", gone, 3u) && !s_window.hands[0].used && !s_window.hands[0].shown &&
                  s_window.hands[1].used,
              "off takes the named hand away and leaves the other");
    const char *const other_gone[3] = {"--as", "teacher", "off"};
    cli_check(asked_words("pointer", other_gone, 3u) && !s_window.hands[1].used, "the other hand goes the same way");
}

// whether the window's last answer reads `text` whole
static int answered_is(const char *text)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = s_ask.answer.text, .other = text, .cap = CLI_POOL_BYTES});
}

// The sessions that reach other hosts' windows, as text. With none, the list says so, and the window refuses a leave
// with why. If ORIOR_IMP names a far end of impero's, the check joins it as a host and lists it. It sends a command
// that reaches this window itself through its own door, and that answers under the host's name. Then it leaves the
// host.
static void sessions_checked(void)
{
    cli_check(asked("sessions", NULL) && answered_is("no sessions"), "with no session the list answers no sessions");
    cli_check(!asked("session-leave", "nobody") && answered_is("no session is open on nobody"),
              "leaving a host with no session is refused with why");
    char imp[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_IMP",
                    .value_out = imp, .capacity = sizeof(imp)))
    {
        (void)fprintf(stderr, "  sessions: no impero is named by ORIOR_IMP, the far end is not checked\n");
        return;
    }
    const char *const words[4] = {imp, "sessions", "relay", NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    cli_check(
        EMBED_CALL(cli_sessions.join_words, CliSessionsCfg, .address = "self", .words = words, .err_msg = &err_msg),
        "a far end of impero's joins as a host");
    // the join's answer, given through the module since no door command holds it
    char text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    int done = 0;
    while (!EMBED_CALL(cli_sessions.step, CliSessionsCfg, .out = &line, .done = &done))
    {
    }
    cli_check(asked("sessions", NULL) && answered_is("self"), "the list names the host");
    const char *const sent[3] = {"help", "telemetry", "on"};
    cli_check(
        asked_words("session-send", sent, 3u) && answered_is("self: "),
        "a command sent to every host reaches this window through its own door and answers under the host's name");
    // The door macros. The door records steps as it runs them, and the sessions replay them on the host. Each pointer
    // step goes on a hand of the sender's name.
    cli_check(!asked("door-stop", NULL) && answered_is("no macro records"),
              "stopping with no recording is refused with why");
    cli_check(asked("door-record", "walk") && answered_is("recording walk"),
              "a door macro starts recording under its name");
    const char *const first[2] = {"130", "90"};
    const char *const second[1] = {"click"};
    cli_check(asked_words("pointer", first, 2u) && asked_words("pointer", second, 1u),
              "door commands run as the macro records them");
    cli_check(asked("door-stop", NULL) && answered_is("walk: 2 steps"), "the recording stops with its steps counted");
    cli_check(asked("door-macros", NULL) && answered_is("walk: 2 steps"), "the list names the macro and its steps");
    s_window.driven_count = 0u;
    const char *const twice[2] = {"walk", "2"};
    cli_check(asked_words("door-send", twice, 2u) && answered_is("4 steps on every host"),
              "the macro replays twice on every host, each step answered before the next goes");
    char machine[CLI_PATH_MAX_LEN];
    VerbaLinea machine_line = {machine, sizeof(machine), 0ULL};
    (void)cli_machine_name(&machine_line);
    (void)mmgr_line_finish(&machine_line);
    driven_taken();
    int hand_found = 0;
    for (unsigned int at = 0u; at < GUI_WINDOW_HANDS; at += 1u)
    {
        const GuiHand *const hand = &s_window.hands[at];
        hand_found =
            hand_found ||
            (hand->used && cellul.eq(&(CatenaFinitaCfg){.src = hand->name, .other = machine, .cap = CLI_POOL_BYTES}) &&
             (hand->x == 130) && (hand->y == 90));
    }
    cli_check(hand_found && (s_window.pointer.x != 130),
              "the replayed pointer steps drive a hand named for the sender, not the window's own pointer");
    cli_check(asked("door-forget", "walk") && answered_is("forgot walk") && asked("door-macros", NULL) &&
                  answered_is("no macros"),
              "a macro forgotten leaves the list empty");
    cli_check(asked("session-leave", "self") && answered_is("left self"), "the host left answers so");
    cli_check(asked("sessions", NULL) && answered_is("no sessions"), "the list is empty again");
}

// The window's redraws and waits. The window names the deadline that set a wait. It draws the changes again on the
// raster displayed and pays them, and pays the busy circle's step. The frame is at a stage while the claim slides,
// and at rest after.
static void redraws_checked(void)
{
    // every tab closed first: an editor with the keys holds the claim, and the slide below needs the claim free
    tabs_closed("first.txt", "one\n", NULL, NULL);
    stepped();
    const unsigned long long wait_ms =
        EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &s_window, .input = NULL);
    const unsigned int source = gui_window_timeout_source();
    cli_check((wait_ms == 0ULL) ? (source == GUI_WINDOW_WAKES)
                                : ((source < GUI_WINDOW_WAKES) && (g_gui_window_wake_names[source] != NULL)),
              "the deadline that set the wait is named, and none where the window waits on nothing");
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &s_window);
    s_window.damage = (GuiRect){10, 10, 50, 40};
    const GuiRect redrawn = EMBED_CALL(gui_window_table.damage_draw, GuiWindowCfg, .window = &s_window);
    cli_check((redrawn.x == 10) && (redrawn.y == 10) && (redrawn.width == 50) && (redrawn.height == 40) &&
                  EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = s_window.damage),
              "the changes are drawn again where they are, and paid");
    const GuiRect none = EMBED_CALL(gui_window_table.damage_draw, GuiWindowCfg, .window = &s_window);
    cli_check(EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = none),
              "with no changes nothing is drawn again");
    s_window.busy_alone = 1;
    (void)EMBED_CALL(gui_window_table.busy_draw, GuiWindowCfg, .window = &s_window);
    cli_check(!s_window.busy_alone, "the busy circle's step is paid as it is drawn again");

    const unsigned long long now_ms = cli_clock_milliseconds();
    const unsigned long long sent_ms = now_ms - 20000ULL;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &s_window.claim, .on = !s_window.claim.autohidden,
               .now_ms = sent_ms);
    EMBED_CALL(gui_window_autohide.autohide_layout, GuiWindowAutohideCfg, .window = &s_window,
               .time_ms = sent_ms + GUI_STAGE_MS);
    cli_check((s_window.stage != NULL) && (s_window.stages.count != 0u) &&
                  (s_window.frame.claim.claim == s_window.stage->frame.claim.claim),
              "while the claim slides the frame is the stage located at the moment");
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &s_window.claim, .on = !s_window.claim.autohidden,
               .now_ms = sent_ms + 10000ULL);
    EMBED_CALL(gui_window_autohide.autohide_layout, GuiWindowAutohideCfg, .window = &s_window, .time_ms = now_ms);
    cli_check((s_window.stage == NULL) && (s_window.stages.count == 0u) && s_window.pending,
              "at rest no stage is located and a frame is owed");
    stepped();
}

static void terminal_menu_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(asked("toggle", NULL), "the terminal opens through the interface");
    cli_check(cli_terminal_tabs_checked(&(CliTerminalTabsCfg){.name = "terminal-keys"}), "and takes the keys");
    cli_check(caret_ink_at(cli_clock_milliseconds()) == 0u,
              "the editor's caret is hidden while the terminal has the keys");
    key_pressed("ContextMenu");
    cli_check(g_cli_menus_open.levels == 1u, "the menu key opens a menu");
    cli_check(g_cli_menus_open.open == -1, "under no title of the bar");
    cli_check(g_cli_menus_open.place == CLI_MENU_PLACE_TERMINAL_KEYS, "at the terminal's key field");
    CliItem first;
    cli_check(
        cli_menus_row(&(CliMenusCfg){.level = 0u, .at = 0u, .item = &first}) &&
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

// The tab's menu on a right press is the list's Tab popup at the point. Close is first, Close others is off with one
// tab open, Close All is third, and a line follows it. It has fourteen rows in all, and Escape closes it.
static void tab_menu_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the tab's menu");
    // the tabs row is whole after the pointer rests in the head's band, with the claim on it or off
    moved(400, 2);
    stepped_for(GUI_AUTOHIDE_SHOW_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    const GuiRect tabs = s_window.frame.tabs;
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = &(GuiInput){.type = GUI_INPUT_MENU, .x = tabs.x + 20, .y = tabs.y + (tabs.height / 2)});
    cli_check((g_cli_menus_open.levels == 1u) && (g_cli_menus_open.open == -1),
              "a right press on the tab opens its menu");
    CliItem row;
    cli_check(
        cli_menus_row(&(CliMenusCfg){.level = 0u, .at = 0u, .item = &row}) &&
            cellul.eq(&(CatenaFinitaCfg){.src = row.label, .cap = CLI_POOL_BYTES, .other = "Close", .other_cap = 6u}),
        "Close first");
    cli_check(cli_menus_row(&(CliMenusCfg){.level = 0u, .at = 1u, .item = &row}) &&
                  !cli_menus_selectable(&(CliMenusCfg){.level = 0u, .at = 1u}),
              "Close others is off with one tab open");
    cli_check(cli_menus_row(&(CliMenusCfg){.level = 0u, .at = 2u, .item = &row}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = row.label, .cap = CLI_POOL_BYTES, .other = "Close All", .other_cap = 10u}),
              "Close All third");
    cli_check(cli_menus_row(&(CliMenusCfg){.level = 0u, .at = 3u, .item = &row}) && row.line,
              "a line after the closes");
    cli_check(g_cli_menus_open.level[0].rows == 14u, "fourteen rows, the list's Tab popup");
    key_pressed("Escape");
    cli_check(g_cli_menus_open.levels == 0u, "Escape closes it");
}

// The view's menu from the keys. The menu key and Shift+F10 open the editor's menu at the caret while the editor has
// the keys, under no title of the bar. Escape closes it.
static void editor_menu_checked(void)
{
    tabs_closed("first.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("first.txt"), "a file opens for the editor's menu");
    typed("x");
    key_pressed("ContextMenu");
    cli_check((g_cli_menus_open.levels == 1u) && (g_cli_menus_open.open == -1) &&
                  (g_cli_menus_open.place == CLI_MENU_PLACE_POINT),
              "the menu key opens the editor's menu at a point while the editor has the keys");
    key_pressed("Escape");
    cli_check(g_cli_menus_open.levels == 0u, "Escape closes the editor's menu");
    key_pressed("Shift+F10");
    cli_check(g_cli_menus_open.levels == 1u, "Shift+F10 opens the editor's menu too");
    key_pressed("Escape");
    cli_check(g_cli_menus_open.levels == 0u, "and Escape closes it again");
}

// Read the theme `at` of the window's into a record of the check's. Return the record, zero if `at` names none.
static GuiTheme s_theme_read;
static const GuiTheme *theme_at(unsigned int at)
{
    s_theme_read = (GuiTheme){0};
    (void)EMBED_CALL(gui_themes_at.get, GuiThemesAtCfg, .themes = &s_window.themes, .at = at,
                     .theme_out = &s_theme_read);
    return &s_theme_read;
}

// The theme commands through the interface. The check makes a theme from the scheme's colors and chooses it. It
// renames the theme, gives it an icon the explorer's marks take, and a color the palette takes. It shares the theme to
// the clipboard, leaves it for the scheme's own colors, and chooses it again. It reads the clipboard back as a theme
// of its own, takes its icon off, and deletes it. The themes and the choice are in the home's state, and the image
// commands stay refused.
static void themes_checked(void)
{
    GuiThemes *const themes = &s_window.themes;
    cli_check((themes->cell != NULL) && (themes->count == 0u) && (s_window.theme_chosen == 0u),
              "the window starts on the scheme's own colors with no themes");
    const GuiColor accent = s_window.palette->signal;
    cli_check(asked("theme-new", NULL), "theme-new runs through the interface");
    cli_check((themes->count == 1u) && (s_window.theme_chosen == 0u) && same(theme_at(0u)->name, "Dark 2"),
              "a theme from the scheme's colors is named after it and chosen");
    cli_check(asked("theme-rename", "Night") && same(theme_at(0u)->name, "Night"), "theme-rename renames it");
    const char *const icon_words[3] = {"*.txt", "Tx", "accent"};
    cli_check(asked_words("theme-icon", icon_words, 3u) && (theme_at(0u)->icons == 1u),
              "theme-icon gives the theme an icon");
    char mark[GUI_EXPLORER_MARK_MAX];
    GuiColor own = 0u;
    const unsigned int marked =
        EMBED_CALL(gui_explorer_mark_table.mark, GuiExplorerMarkCfg, .name = "first.txt", .length = 9ULL, .text = mark,
                   .palette = s_window.palette, .color_out = &own);
    cli_check((marked == GUI_EXPLORER_MARK_OWN) && (own == accent) && same(mark, "Tx"),
              "the explorer marks a file the pattern covers with the icon in the palette's color");
    const char *const color_words[2] = {"accent", "#102030"};
    GuiColor set = 0u;
    cli_check(asked_words("theme-color", color_words, 2u) &&
                  (s_window.palette->signal == GUI_RGB(0x10u, 0x20u, 0x30u)) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = themes, .at = 0u,
                             .name = "accent", .color_pointer = &set) &&
                  (set == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-color sets the palette's color in the theme and the window");
    cli_check(asked("theme-export", NULL), "theme-export runs");
    char clipboard_text[8192];
    VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
    (void)cli_clipboard_text_read(&(CliCtx){.text_verba_linea_pointer = &clipboard});
    (void)mmgr_line_finish(&clipboard);
    cli_check(
        cellul.has(&(CatenaFinitaCfg){
            .src = clipboard_text, .other = "\"name\": \"Night\"", .cap = sizeof(clipboard_text), .other_cap = 16u}) &&
            cellul.has(&(CatenaFinitaCfg){.src = clipboard_text,
                                          .other = "\"accent\": \"#102030\"",
                                          .cap = sizeof(clipboard_text),
                                          .other_cap = 20u}),
        "the clipboard holds the theme as text");
    cli_check(asked("theme-choose", "Dark") && (s_window.theme_chosen == 1u) && (s_window.palette->signal == accent),
              "theme-choose Dark puts the scheme's colors back");
    cli_check(asked("theme-choose", "Night") && (s_window.theme_chosen == 0u) &&
                  (s_window.palette->signal == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-choose by name positions the theme's colors again");
    cli_check(!asked("theme-choose", "Nowhere"), "a name no theme has is refused");
    cli_check(asked("theme-import", NULL) && (themes->count == 2u) && (s_window.theme_chosen == 1u) &&
                  same(theme_at(1u)->name, "Night 2") && (theme_at(1u)->icons == 1u) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = themes, .at = 1u,
                             .name = "accent", .color_pointer = &set) &&
                  (set == GUI_RGB(0x10u, 0x20u, 0x30u)),
              "theme-import reads the clipboard's theme in under a free name and chooses it");
    cli_check(asked("theme-icon-remove", "*.txt") && (theme_at(1u)->icons == 0u),
              "theme-icon-remove takes the icon off the chosen theme");
    cli_check(asked("theme-delete", NULL) && (themes->count == 1u) && (s_window.theme_chosen == 1u) &&
                  (s_window.palette->signal == accent),
              "theme-delete takes the chosen theme out and goes back to the scheme's colors");
    char folder[CLI_PATH_MAX_LEN];
    cli_check(EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder), "the home is known");
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner});
    char retained_text[4096];
    char chosen_text[CLI_NAME_MAX_LEN];
    const char *const retained = cli_state_entry_value(&(CliStateEntryCfg){
        .entries = &entries, .key = "themes", .out = retained_text, .capacity = sizeof(retained_text)});
    const char *const chosen = cli_state_entry_value(&(CliStateEntryCfg){
        .entries = &entries, .key = "theme.dark", .out = chosen_text, .capacity = sizeof(chosen_text)});
    cli_check((retained != NULL) &&
                  cellul.has(&(CatenaFinitaCfg){
                      .src = retained, .other = "\"Night\"", .cap = CLI_POOL_BYTES, .other_cap = 8u}) &&
                  !cellul.has(
                      &(CatenaFinitaCfg){.src = retained, .other = "Night 2", .cap = CLI_POOL_BYTES, .other_cap = 8u}),
              "the home's state retains the themes as they are");
    cli_check((chosen != NULL) && same(chosen, "Dark"), "and the choice");
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries});
    cli_check(!asked("theme-image", NULL), "theme-image stays refused: the window reads no image files");
}

// whether the View menu's row `label` shows checked
static int view_checked_on(const char *label)
{
    CliItem item;
    (void)row_chosen("View", label, &item);
    return g_cli_menus_open.checked(&item);
}

// The sticky rows stop short of the controls' pane. The check places the view inside a long function with the edges
// claimed. A rest in the controls' corner brings the pane down over the editor's top corner. The pinned row's ground
// ends before the border's bend round the pane.
static void sticky_notch_checked(void)
{
    static char s_long[8192];
    VerbaLinea text = {s_long, sizeof(s_long), 0ULL};
    mmgr_line_put(&text, "void one(void)\n{\n");
    for (unsigned int line = 0u; line < 200u; line += 1u)
    {
        mmgr_line_put(&text, "    int a_");
        mmgr_line_decimal(&text, line, 1u);
        mmgr_line_put(&text, " = 1;\n");
    }
    mmgr_line_put(&text, "}\n");
    (void)mmgr_line_finish(&text);
    tabs_closed("long.c", s_long, NULL, NULL);
    cli_check(tab_opened("long.c"), "a long function opens for the sticky rows");
    char path[CLI_PATH_MAX_LEN];
    tree_path("long.c:120", path);
    cli_check(asked("file", path), "the view is placed inside the function");
    typed("x");
    stepped_for(GUI_AUTOHIDE_ANIMATION_MS + 100ULL);
    cli_check(s_window.claim.autohidden && (editor_frame() != NULL) && (editor_frame()->stickies == 1u),
              "typing claims the edges and the function's head is the one sticky row");
    cli_check(EMBED_CALL(cli_window_input.timeout_ms, GuiWindowInputCfg, .window_read = &s_window) != 0ULL,
              "the window wants a tick while the caret blinks and the claim slides");
    const int line_height =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CODE_LINE, .scale = s_window.frame.scale);
    moved(s_window.frame.work.x + s_window.frame.work.width - 4, 4);
    stepped_for(GUI_AUTOHIDE_SHOW_DELAY_MS + GUI_AUTOHIDE_ANIMATION_MS + 200ULL);
    cli_check(s_window.frame.controls_pane_share >= GUI_FIXED_ONE,
              "a rest in the corner brings the controls' pane down");
    // The check draws the editor on a clear raster of its own, with the pane's notch at its top right. The sticky
    // row's ground reaches the raster's left and leaves the bend before the notch clear.
    const GuiColor clear = GUI_RGB(1u, 2u, 3u);
    const GuiRect place = {0, 0, 640, 4 * line_height};
    const GuiRect notch = {place.width - 120, 0, 120, 2 * line_height};
    GuiRaster raster;
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster,
                         .width = (unsigned int)place.width, .part = GUI_BLOCK_DIALOG_GROUND, .segment = 0u,
                         .height = (unsigned int)place.height),
              "a raster for the editor is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = place, .color = clear);
    EMBED_CALL(gui_editor_position_scale.draw, GuiEditorPositionScaleCfg, .editor = editor_framed(), .raster = &raster,
               .glyphs = &s_window.glyphs, .palette = s_window.palette, .place = place, .notch = notch,
               .radius = s_window.frame.radius, .scale = s_window.frame.scale, .key = "long.c");
    // the probes are inside the raster, whose sides an unsigned int represents
    const unsigned int y = (unsigned int)(line_height - 3);
    const GuiColor inside = raster.pixels[(y * raster.width) + 2u];
    const GuiColor bend =
        raster.pixels[(y * raster.width) + (unsigned int)(notch.x - ((int)s_window.frame.radius / 2))];
    cli_check(inside != clear, "the sticky row's ground is at the editor's left");
    cli_check(bend == clear, "and it stops short of the bend round the controls' pane");
    // A press on the sticky row puts its line at the view's top, and the frame follows within a second. The row is
    // under the top band, where the claim reads a press in as the bar's.
    const GuiRect editor = s_window.frame.editor;
    const unsigned long long pinned = editor_frame()->row[editor_frame()->count].number;
    pressed_with(editor.x + 60, editor.y + line_height - 2, GUI_BUTTON_LEFT);
    released_with(editor.x + 60, editor.y + line_height - 2, GUI_BUTTON_LEFT);
    const unsigned long long scrolled_until = cli_clock_milliseconds() + 1000ULL;
    while ((cli_clock_milliseconds() < scrolled_until) && ((editor_frame() == NULL) || (editor_frame()->top != pinned)))
    {
        stepped_for(20ULL);
    }
    cli_check((editor_frame() != NULL) && (editor_frame()->top == pinned),
              "a press on the sticky row puts its line at the view's top");
}

// The View menu's switches and tool views through the interface. The check turns whitespace marks, the eased scroll,
// the sticky rows and the claim off and on. Each switch saves its value under its key and shows checked in the menu. A
// tool view opens and closes the side_menu's tool.
static void view_checked(void)
{
    cli_check(asked("whitespace", "on") && s_window.editor.whitespace && view_checked_on("Show Whitespace"),
              "whitespace on marks spaces and tabs and shows checked");
    cli_check(asked("whitespace", NULL) && !s_window.editor.whitespace, "whitespace without a word turns it off");
    cli_check(asked("smooth-scroll", "off") && !s_window.scroll_smooth && !view_checked_on("Smooth Scrolling"),
              "smooth-scroll off stops the ease");
    cli_check(asked("smooth-scroll", "on") && s_window.scroll_smooth, "and on starts it again");
    cli_check(asked("sticky-scroll", "off") && !cli_view_sticky() && !view_checked_on("Sticky Scroll"),
              "sticky-scroll off takes the sticky rows out of the frames");
    cli_check(asked("sticky-scroll", "on") && cli_view_sticky(), "and on puts them back");
    cli_check(asked("auto-collapse", "off") && !s_window.autohide_wanted && !s_window.claim.autohidden &&
                  !view_checked_on("Auto Collapse Panes"),
              "auto-collapse off lets the claim go");
    cli_check(asked("auto-collapse", "on") && s_window.autohide_wanted, "and on allows it again");
    char folder[CLI_PATH_MAX_LEN];
    (void)EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder);
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner});
    char retained_text[CLI_NAME_MAX_LEN];
    const char *const retained = cli_state_entry_value(&(CliStateEntryCfg){
        .entries = &entries, .key = "whitespace", .out = retained_text, .capacity = sizeof(retained_text)});
    cli_check((retained != NULL) && same(retained, "false"), "a switch is saved as its setting");
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries});
    int comms = -1;
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        if (same(g_gui_tools[at].name, "comms"))
        {
            // the tools are fewer than an int counts
            comms = (int)at;
        }
    }
    cli_check(asked("comms-view", NULL) && (s_window.side_menu.foot == comms),
              "comms-view opens the Comms tool at the foot");
    cli_check(asked("comms-view", NULL) && (s_window.side_menu.foot == GUI_SIDE_MENU_NONE), "and closes it again");
}

// The check's catalog for the Run pane holds one job with one word param.
static CliJob s_run_jobs[4];
static CliParam s_run_params[4];
static CliStep s_run_steps[4];
static CliArg s_run_args[4];
static unsigned int s_run_lists[8];
static char s_run_text[1024];
static CliCatalog s_run_catalog;

// The Run pane is at the foot. A press on a job's param row through the window gives the pane the keys. The typed
// text reaches the param's value and leaves the editor as it is.
static void run_keys_checked(void)
{
    tabs_closed("run_typed.txt", "one\n", NULL, NULL);
    cli_check(tab_opened("run_typed.txt"), "a file of text opens for the Run pane check");
    cli_check(asked("run-view", NULL) && (s_window.side_menu.foot != GUI_SIDE_MENU_NONE),
              "the Run tool opens its pane at the foot");
    stepped();
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &s_run_catalog, .jobs = s_run_jobs,
               .jobs_max = 4u, .params = s_run_params, .params_max = 4u, .steps = s_run_steps, .steps_max = 4u,
               .request = s_run_args, .args_max = 4u, .lists = s_run_lists, .lists_max = 8u, .text_out = s_run_text,
               .text_max = sizeof(s_run_text));
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_run_catalog, .id = "build/alpha",
                     .group = "build", .title = "Alpha build", .file = "src/alpha.sh", .about = "builds the alpha",
                     .opens = "");
    EMBED_CALL(cli_catalog_key_type.param, CliCatalogKeyTypeCfg, .catalog = &s_run_catalog, .key = "name",
               .type = "word", .required = 1);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_run_catalog, .text = "job.sh",
               .program = CLI_PROGRAM_BASH);
    EMBED_CALL(cli_job_runs_root.read, CliJobRunsRootCfg, .root = NULL, .catalog = &s_run_catalog);
    EMBED_CALL(gui_window_table.draw, GuiWindowCfg, .window = &s_window);
    cli_check(EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 1u, "the one group is listed");
    // Find a row by its text and a head action by its label, each in the foot pane.
    const GuiRect group_row = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = &gui_run_table,
                                         .pane_read = &s_window.panes[(unsigned int)s_window.side_menu.foot],
                                         .frame = &s_window.frame, .row = 0u);
    s_window.driven_count = 0u;
    const char *const to_row[5] = {"to", "row", "build", "--speed", "0"};
    cli_check(asked_words("pointer", to_row, 5u) && (s_window.driven_count == 1u) &&
                  EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = group_row, .x = s_window.driven[0].x,
                             .y = s_window.driven[0].y),
              "to row build queues a move inside the group's row");
    const int rows_top = EMBED_CALL(gui_pane.rows_top, GuiPaneCfg, .table = &gui_run_table, .frame = &s_window.frame);
    s_window.driven_count = 0u;
    const char *const to_start[5] = {"to", "action", "Start", "--speed", "0"};
    cli_check(asked_words("pointer", to_start, 5u) && (s_window.driven_count == 1u) &&
                  (s_window.driven[0].y < rows_top) && (s_window.driven[0].y > s_window.frame.terminal.y) &&
                  (s_window.driven[0].x < s_window.frame.terminal.x + (s_window.frame.terminal.width / 2)),
              "to action Start queues a move onto Start in the pane's head");
    s_window.driven_count = 0u;
    // A press on the group's row at 0 opens it and puts the job's row at 1. A press on the job's row puts its file's
    // row at 2 and its param's row at 3.
    // The foot is one of the tools.
    const GuiPane *const pane = &s_window.panes[(unsigned int)s_window.side_menu.foot];
    static const unsigned int s_rows[3] = {0u, 1u, 3u};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const GuiRect row = EMBED_CALL(gui_pane.row_rect, GuiPaneCfg, .table = &gui_run_table, .pane_read = pane,
                                       .frame = &s_window.frame, .row = s_rows[at]);
        pressed_with(row.x + (row.width / 2), row.y + (row.height / 2), GUI_BUTTON_LEFT);
        released_with(row.x + (row.width / 2), row.y + (row.height / 2), GUI_BUTTON_LEFT);
    }
    cli_check(EMBED_CALL(cli_job_runs_row.rows, CliJobRunsRowCfg, .row_at = 0u) == 4u,
              "the group opened and the job chosen list the job, its file and its param");
    cli_check(pane->keys, "the param's row pressed through the window gives the pane the keys");
    typed("ab");
    CliJobRunRow param;
    cli_check(EMBED_CALL(cli_job_runs_row.row, CliJobRunsRowCfg, .row_at = 3u, .row_out = &param) &&
                  (param.second_length == 2ULL) &&
                  (memor.cmp(&(MemoriaCfg){.src = param.second, .other = "ab", .bytes = 2u}) == 0),
              "the text typed reaches the param's value");
    cli_check(asked("run-view", NULL) && (s_window.side_menu.foot == GUI_SIDE_MENU_NONE),
              "the Run tool closes its pane again");
    stepped();
}

// whether the folder at `path` is
static int folder_is(const char *path)
{
    return EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path);
}

// `root` and `rest` joined with a slash into `out`, sized CLI_PATH_MAX_LEN
static void joined(const char *root, const char *rest, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, rest);
    (void)mmgr_line_finish(&line);
}

// whether the explorer's rows name `name`
static int explorer_lists(const char *name)
{
    CliExplorerRow row;
    for (unsigned int at = 0u; cli_files_explorer_row(&(CliFilesExplorerCfg){.at = at, .row = &row}); at += 1u)
    {
        if (same(row.name, name))
        {
            return 1;
        }
    }
    return 0;
}

// the window stepped until the explorer lists `name` where `listed` is 1, or lists it no more where it is 0, within
// `milliseconds`: whether it does
static int explorer_settles(const char *name, int listed, unsigned long long milliseconds)
{
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while ((explorer_lists(name) != listed) && (cli_clock_milliseconds() < until))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 20ULL);
    }
    return explorer_lists(name) == listed;
}

// The File menu's commands through the interface. The check makes a folder in the tree and makes it a repository, and
// keeps the tree as a template. It turns auto save on, which saves a changed tab after the edits rest. It asks the
// trust question and keeps its answer, and saves the environment command's word.
static void file_checked(void)
{
    char path[CLI_PATH_MAX_LEN];
    joined(s_root, "made_here", path);
    cli_check(asked("create-folder", "made_here") && folder_is(path), "create-folder makes the folder in the tree");
    cli_check(!asked("create-folder", NULL), "create-folder without a name is refused");
    // A folder made and a folder deleted on the disk, outside the window, list the explorer again as the watch reports
    // them. The folder holds a file, which the poll reads on a system that cannot watch the tree.
    char gone[CLI_PATH_MAX_LEN];
    joined(s_root, "gone_here", gone);
    if (cli_files_explorer_rows() == 0u)
    {
        cli_files_explorer_show(&(CliFilesExplorerCfg){.root = s_root});
    }
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = gone);
    file_written("gone_here/note.txt", "retained\n");
    cli_check(explorer_settles("gone_here", 1, 8000ULL),
              "a folder made on the disk comes into the explorer as the watch reports it");
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = gone);
    cli_check(explorer_settles("gone_here", 0, 8000ULL),
              "a folder deleted on the disk goes from the explorer as the watch reports it");
    char git[CLI_PATH_MAX_LEN];
    joined(path, ".git", git);
    cli_check(asked("create-repository", "made_here") && folder_is(git),
              "create-repository makes the folder a repository");
    CliItem item;
    (void)row_chosen("File", "Auto Save", &item);
    cli_check(asked("auto-save", "on") && s_window.autosave && g_cli_menus_open.checked(&item),
              "auto-save on shows checked");
    cli_check(!asked("auto-save-after", NULL) && (s_window.autosave_ms == GUI_WINDOW_AUTOSAVE_DEFAULT_MS),
              "auto-save-after without the seconds is refused and the rest is");
    cli_check(same(s_ask.answer.text, "the window refused auto-save-after"),
              "a command of the window's that refuses its words is answered as refused");
    cli_check(!asked("no-such-command", NULL) &&
                  same(s_ask.answer.text, "no-such-command is not a command of the window's"),
              "a command the window has none of is answered as none of its");
    cli_check(asked("auto-save-after", "3") && (s_window.autosave_ms == 3000ULL),
              "auto-save-after sets the rest in seconds");
    cli_check(asked("trim", "on") && s_window.trim, "trim on is retained");
    tabs_closed("retained.txt", "alpha  \n", NULL, NULL);
    cli_check(tab_opened("retained.txt"), "a file opens");
    typed("x");
    // An edit that does not reach the view fails with a report of where the text went. The report names the views
    // open, the one displayed, the dialog, the part that takes the keys, and the notice displayed.
    char edited_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea edited_line = {edited_why, sizeof(edited_why), 0ULL};
    mmgr_line_put(&edited_line, "an edit marks the tab changed and starts the rest");
    if (!changed(0u) || (s_window.edited_ms == 0ULL))
    {
        mmgr_line_put(&edited_line, ": views ");
        mmgr_line_decimal(&edited_line, cli_view_count(), 1u);
        mmgr_line_put(&edited_line, (cli_view_displayed() < 0) ? ", none displayed" : ", displayed ");
        if (cli_view_displayed() >= 0)
        {
            // Explicit cast widens the index the view count bounds
            mmgr_line_decimal(&edited_line, (uint64_t)cli_view_displayed(), 1u);
        }
        mmgr_line_put(&edited_line, s_window.dialog.displayed ? ", dialog up" : ", dialog down");
        mmgr_line_put(&edited_line, cli_terminal_tabs_checked(&(CliTerminalTabsCfg){.name = "terminal-keys"})
                                        ? ", terminal keys"
                                        : ", editor keys");
        mmgr_line_put(&edited_line, s_window.editing ? ", editing" : ", not editing");
        mmgr_line_put(&edited_line, s_window.wordmark.exists ? ", name's pane up" : ", name's pane down");
        if (cli_notice_displayed() != NULL)
        {
            mmgr_line_put(&edited_line, ", notice ");
            mmgr_line_put(&edited_line, cli_notice_displayed());
        }
    }
    (void)mmgr_line_finish(&edited_line);
    cli_check(changed(0u) && (s_window.edited_ms != 0ULL), edited_why);
    g_test_clock_ms = s_window.edited_ms + 2000ULL;
    stepped();
    cli_check(changed(0u), "before the rest is up the tab stays changed");
    g_test_clock_ms = s_window.edited_ms + s_window.autosave_ms + 1ULL;
    stepped();
    g_test_clock_ms = GUI_TEST_CLOCK_MACHINE;
    // a save the file writer refused leaves a failure's notice, which the check carries
    char saved_why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea saved_line = {saved_why, sizeof(saved_why), 0ULL};
    mmgr_line_put(&saved_line, "the rest past, auto save clears the changed mark");
    if (changed(0u) && (cli_notice_displayed() != NULL))
    {
        mmgr_line_put(&saved_line, ": ");
        mmgr_line_put(&saved_line, cli_notice_displayed());
    }
    (void)mmgr_line_finish(&saved_line);
    cli_check(!changed(0u), saved_why);
    cli_check(file_reads("retained.txt", "xalpha  \n"),
              "the rest past, auto save writes the tab to the disk as it is, the blanks at its line's end retained");
    cli_check(asked("auto-save", "off") && !s_window.autosave, "auto-save off");
    cli_check(asked("final-newline", "on") && s_window.final_newline, "final-newline on is retained");
    tabs_closed("tidy.txt", "alpha  \nbeta\t\ngamma", NULL, NULL);
    cli_check(tab_opened("tidy.txt"),
              "a file with blanks at its lines' ends and no line end after its last line opens");
    typed("x");
    cli_check(asked("save", NULL) && !changed(0u) && file_reads("tidy.txt", "xalpha\nbeta\ngamma\n"),
              "Save takes the blanks off the lines' ends and puts a line end after the last line");
    key_pressed("Mod+Z");
    cli_check(changed(0u), "one undo takes the tidy back whole, and the tab reads changed");
    (void)row_chosen("File", "Format on Save", &item);
    cli_check(asked("format-on-save", "on") && s_window.format_on_save && g_cli_menus_open.checked(&item),
              "format-on-save on shows checked");
    cli_check(asked("save", NULL) && file_reads("tidy.txt", "xalpha\nbeta\ngamma\n"),
              "a save with Format on Save on writes a file whose language no formatter formats, tidied");
    cli_check(asked("format-on-save", "off") && !s_window.format_on_save, "format-on-save off");
    cli_check(asked("trim", "off") && !s_window.trim, "trim off");
    cli_check(asked("final-newline", "off") && !s_window.final_newline, "final-newline off");
    char templates[CLI_PATH_MAX_LEN];
    cli_check(EMBED_CALL(cli_templates_table.folder, CliTemplatesCfg, .folder = templates),
              "the templates' folder is known");
    char retained[CLI_PATH_MAX_LEN];
    joined(templates, "retained_tree", retained);
    cli_check(asked("retain-template", "retained_tree") && folder_is(retained),
              "retain-template copies the tree under its name");
    int trusted = 0;
    cli_check(!cli_trust_answer(&(CliTrustTreeCfg){.tree = s_root, .trusted = &trusted}),
              "the tree has no trust answer yet");
    cli_check(asked("trust-tree", NULL) && s_window.dialog.displayed && s_window.trust_asked &&
                  same(s_window.dialog.label[0], "Trust"),
              "trust-tree asks in the dialog");
    s_window.dialog_answer = (GuiDialogChange){GUI_DIALOG_ANSWERED, 0u};
    stepped();
    cli_check(!s_window.trust_asked && cli_trust_answer(&(CliTrustTreeCfg){.tree = s_root, .trusted = &trusted}) &&
                  trusted,
              "Trust pressed, the tree's answer is retained as trusted");
    cli_check(asked("environment", "none"), "environment runs with a word");
    cli_check(!asked("environment", NULL), "and is refused without one");
    cli_check(asked("reload-plugins", NULL), "reload-plugins runs");
    char folder[CLI_PATH_MAX_LEN];
    (void)EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder);
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner});
    char place_text[CLI_NAME_MAX_LEN];
    const char *const place = cli_state_entry_value(&(CliStateEntryCfg){
        .entries = &entries, .key = "environment", .out = place_text, .capacity = sizeof(place_text)});
    cli_check((place != NULL) && same(place, "none"), "the environment setting is saved");
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries});
}

// the text saved under `key` in the home's state, the same as `required`
static int state_saved(const char *key, const char *required)
{
    char folder[CLI_PATH_MAX_LEN];
    (void)EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder);
    CliStateEntries entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = folder, .owner = g_cli_view_owner});
    char retained_text[4096];
    const char *const retained = cli_state_entry_value(
        &(CliStateEntryCfg){.entries = &entries, .key = key, .out = retained_text, .capacity = sizeof(retained_text)});
    const int saved = (retained != NULL) && same(retained, required);
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries});
    return saved;
}

// whether the row whose check is `checks` shows checked
static int checks_on(const char *checks)
{
    const CliItem item = {.label = "", .command = "", .keys = "", .args = "", .needs = "", .checks = checks};
    return g_cli_menus_open.checked(&item);
}

// The Edit menu's commands through the interface. The check fills a paragraph to the documentation margin. It records
// a macro, plays it, keeps it and plays it by name. It compares the clipboard against the displayed file, displays
// Local History's snapshot, and saves the settings.
static void edit_checked(void)
{
    cli_check(asked("doc-margin", "12") && (cli_view_fill() == 12ULL), "doc-margin sets the fill's width");
    tabs_closed("fill.txt", "aa bb cc dd ee\nff gg\n", NULL, NULL);
    cli_check(tab_opened("fill.txt"), "a paragraph of two lines opens");
    cli_check(asked("fill-paragraph", NULL) && asked("save", NULL) && file_reads("fill.txt", "aa bb cc dd\nee ff gg\n"),
              "fill-paragraph positions the words again within the margin");
    cli_check(asked("doc-margin", "none") && (cli_view_fill() == 0ULL) && state_saved("doc-margin", "none"),
              "doc-margin none goes back to the view's columns and is saved");
    cli_check(!asked("doc-margin", NULL), "doc-margin without a word is refused");
    tabs_closed("macro.txt", "", NULL, NULL);
    cli_check(tab_opened("macro.txt"), "an empty file opens for the macro");
    cli_check(!asked("macro-play", NULL), "macro-play with nothing recorded is refused");
    cli_check(asked("macro-record", NULL) && s_window.macro_recording && checks_on("macro-recording"),
              "macro-record starts the recording and shows checked");
    typed("ab");
    key_pressed("Enter");
    cli_check(asked("macro-record", NULL) && !s_window.macro_recording && same(s_window.macro, "text ab\nkey Enter\n"),
              "macro-record again ends it with the text and the key recorded");
    cli_check(asked("macro-play", "2") && asked("save", NULL) && file_reads("macro.txt", "ab\nab\nab\n"),
              "macro-play 2 plays the macro twice");
    cli_check(!asked("macro-retain", NULL), "macro-retain without a name is refused");
    cli_check(asked("macro-retain", "two") && state_saved("macro.two", "text ab\nkey Enter\n"),
              "macro-retain saves the macro under its name");
    const char *const by_name[2] = {"1", "two"};
    cli_check(asked_words("macro-play", by_name, 2u) && asked("save", NULL) &&
                  file_reads("macro.txt", "ab\nab\nab\nab\n"),
              "macro-play by name plays the retained macro");
    cli_check(!asked("macro-play", "nowhere"), "macro-play with a name none is retained under is refused");
    (void)cli_clipboard_text_write(&(CliSizeTextCtx){.size = 3ULL, .text = "ab\n"});
    cli_check(asked("compare-clipboard", NULL), "compare-clipboard opens the clipboard against the file");
    key_pressed("Escape");
    cli_check(asked("undo-history", NULL), "undo-history shows the file against its newest snapshot");
    key_pressed("Escape");
    tabs_closed("fresh.txt", "x\n", NULL, NULL);
    cli_check(tab_opened("fresh.txt") && !asked("undo-history", NULL), "a file never saved here has no snapshot");
    cli_check(!asked("continuation", NULL), "continuation without words is refused");
    const char *const indents[3] = {"4", "8", "4"};
    cli_check(asked_words("continuation", indents, 3u) && state_saved("continuation", "4 8 4"),
              "continuation saves its three indents");
    cli_check(asked("operator-next-line", "on") && s_window.operator_next_line && checks_on("operator-next-line") &&
                  state_saved("operator-next-line", "true"),
              "operator-next-line on shows checked and is saved");
    cli_check(asked("operator-next-line", NULL) && !s_window.operator_next_line, "and without a word turns off");
}

// the line end the system's clipboard holds between lines
#if defined(_WIN32)
#define CLIPBOARD_BREAK "\r\n"
#else
#define CLIPBOARD_BREAK "\n"
#endif

static void cursor_put(unsigned long long line, unsigned long long col);

// whether the system's clipboard holds the text `expected`
static int clipboard_holds(const char *expected)
{
    static char s_held[256];
    VerbaLinea held = {s_held, sizeof(s_held), 0ULL};
    (void)cli_clipboard_text_read(&(CliCtx){.text_verba_linea_pointer = &held});
    (void)mmgr_line_finish(&held);
    return same(s_held, expected);
}

// Copy, Cut and Paste in the editor, checked against the system's clipboard. Copy puts a selection on the clipboard
// with the system's line end between its lines. With no selection, it copies the cursor's line and its line end.
// Paste puts the clipboard's text in place of a selection, and at the cursor. Cut puts its text on the clipboard and
// takes it out.
static void clipboard_checked(void)
{
    tabs_closed("copy.txt", "one\ntwo\nthree\n", NULL, NULL);
    cli_check(tab_opened("copy.txt"), "a file of three lines opens for the clipboard");
    cursor_put(0ULL, 0ULL);
    key_pressed("Shift+Down");
    key_pressed("Shift+End");
    key_pressed("Mod+C");
    cli_check(clipboard_holds("one" CLIPBOARD_BREAK "two"), "Copy puts the selection on the clipboard");
    cursor_put(1ULL, 1ULL);
    key_pressed("Mod+C");
    cli_check(clipboard_holds("two" CLIPBOARD_BREAK), "Copy with nothing chosen puts the cursor's line");
    (void)cli_clipboard_text_write(&(CliSizeTextCtx){.size = 6ULL, .text = "pasted"});
    cursor_put(2ULL, 0ULL);
    key_pressed("Shift+End");
    key_pressed("Mod+V");
    cli_check(asked("save", NULL) && file_reads("copy.txt", "one\ntwo\npasted\n"),
              "Paste puts the clipboard's text in place of the selection");
    cursor_put(1ULL, 3ULL);
    key_pressed("Mod+V");
    cli_check(asked("save", NULL) && file_reads("copy.txt", "one\ntwopasted\npasted\n"),
              "Paste puts the clipboard's text at the cursor");
    cursor_put(0ULL, 0ULL);
    key_pressed("Shift+End");
    key_pressed("Mod+X");
    cli_check(clipboard_holds("one") && asked("save", NULL) && file_reads("copy.txt", "\ntwopasted\npasted\n"),
              "Cut puts the selection on the clipboard and takes it out");
}

// the cursor of the view displayed put at the line `line` and the unit `col`
static void cursor_put(unsigned long long line, unsigned long long col)
{
    const int displayed = cli_view_displayed();
    char key[CLI_NAME_MAX_LEN];
    // the view displayed is among the views
    if ((displayed >= 0) && cli_view_key(&(CliViewAtCfg){.at = (unsigned int)displayed, .key_out = key}))
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const CliViewInput at = {.type = CLI_VIEW_POSITION, .row = line, .column = col};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &at, .err_msg = &err_msg);
    }
    stepped();
}

// whether the view displayed is the tree's file `name` with its cursor on the line `line`
static int displayed_at(const char *name, unsigned long long line)
{
    const int displayed = cli_view_displayed();
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
    CliPosition cursor = {0ULL, 0ULL};
    // the view displayed is among the views
    return (displayed >= 0) && cli_view_path_at(&(CliViewAtCfg){.at = (unsigned int)displayed, .path_out = path}) &&
           cli_view_key(&(CliViewAtCfg){.at = (unsigned int)displayed, .key_out = key}) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = path, .cap = CLI_PATH_MAX_LEN, .other = name, .other_cap = CLI_PATH_MAX_LEN}) &&
           cli_view_cursor(&(CliViewCursorCfg){.key = key, .cursor = &cursor, .anchor = NULL}) && (cursor.line == line);
}

// the place among the strip's tools of the tool `name`, the count where none is
static unsigned int tool_at(const char *name)
{
    for (unsigned int at = 0u; at < g_gui_tools_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_gui_tools[at].name, .other = name, .cap = 32ULL}))
        {
            return at;
        }
    }
    return g_gui_tools_count;
}

// The view thread's map of a surface: a pixel on a border, a corner, a control, a client rectangle, the caption or
// none of them. The window stores its place for a tree in orior's home and reads it back fitted to a monitor. Orior's
// home is a folder of the run while the check stores the place, and as it was after.
static void view_thread_checked(void)
{
    GuiRect client = {100, 0, 200, 30};
    const CliViewHits hits = {.width = 800u,
                              .height = 600u,
                              .border = 6,
                              .minimize = {650, 0, 40, 30},
                              .maximize = {700, 0, 40, 30},
                              .close = {745, 0, 40, 30},
                              .client = &client,
                              .clients = 1u,
                              .caption = {0, 0, 800, 30}};
    const CliViewHits whole = {0};
    cli_check(cli_view_hit_of(&(CliViewThreadCfg){.hits = &whole, .x = 2, .y = 2}) == GUI_HIT_CLIENT,
              "a surface of no width is the client everywhere");
    cli_check((cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 2, .y = 2}) == GUI_HIT_TOP_LEFT) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 797, .y = 597}) == GUI_HIT_BOTTOM_RIGHT) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 797, .y = 300}) == GUI_HIT_RIGHT) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 400, .y = 598}) == GUI_HIT_BOTTOM),
              "a pixel within the border's width is the border or the corner it is on");
    cli_check((cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 660, .y = 15}) == GUI_HIT_MINIMIZE) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 710, .y = 15}) == GUI_HIT_MAXIMIZE) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 760, .y = 15}) == GUI_HIT_CLOSE),
              "a pixel on a control is that control");
    cli_check((cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 150, .y = 15}) == GUI_HIT_CLIENT) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 400, .y = 15}) == GUI_HIT_CAPTION) &&
                  (cli_view_hit_of(&(CliViewThreadCfg){.hits = &hits, .x = 400, .y = 300}) == GUI_HIT_CLIENT),
              "the caption gives way to a client rectangle over it, and a pixel on neither is the client");

    char home_before[CLI_PATH_MAX_LEN] = {0};
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_HOME",
                     .value_out = home_before, .capacity = sizeof(home_before));
    char home[CLI_PATH_MAX_LEN];
    cli_check_folder("window_home", home);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
    const CliWindowArea monitor = {0, 0, 1920u, 1080u};
    CliWindowPosition place = {0};
    cli_check(
        !cli_view_place_saved(&(CliViewThreadCfg){.root = s_root, .areas = &monitor, .count = 1u, .place = &place}),
        "a tree with no place saved has none");
    CliWindowPosition stored = {120, 80, 1200u, 800u, 0};
    cli_view_place_store(&(CliViewThreadCfg){.root = s_root, .place = &stored});
    CliWindowPosition other = {0, 0, 1000u, 700u, 1};
    cli_view_place_store(&(CliViewThreadCfg){.root = "elsewhere", .place = &other});
    cli_check(
        cli_view_place_saved(&(CliViewThreadCfg){.root = s_root, .areas = &monitor, .count = 1u, .place = &place}) &&
            (place.x == 120) && (place.y == 80) && (place.width == 1200u) && (place.height == 800u) && !place.maximized,
        "a place stored for a tree is read back for it beside another tree's");
    CliWindowPosition off = {5000, 5000, 1000u, 700u, 0};
    cli_view_place_store(&(CliViewThreadCfg){.root = s_root, .place = &off});
    cli_check(
        cli_view_place_saved(&(CliViewThreadCfg){.root = s_root, .areas = &monitor, .count = 1u, .place = &place}) &&
            (place.x >= 0) && ((place.x + (int)place.width) <= 1920) && (place.y >= 0) &&
            ((place.y + (int)place.height) <= 1080),
        "a place on no monitor there is goes onto the first");
    CliWindowPosition small = {0, 0, 100u, 100u, 0};
    cli_view_place_store(&(CliViewThreadCfg){.root = s_root, .place = &small});
    cli_check(
        !cli_view_place_saved(&(CliViewThreadCfg){.root = s_root, .areas = &monitor, .count = 1u, .place = &place}),
        "a place below the least size is not put back");
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home_before);
}

// Go to Definition opens the declaration's file at its line. Find Usages lists the references by file in the
// References pane. Rename Symbol opens its box over the name, and Enter renames the name in every file.
static void symbols_checked(void)
{
    cli_check(tab_opened("sym/use.c"), "use.c opens for the symbol commands");
    cursor_put(3ULL, 12ULL);
    cli_check(asked("definition", NULL) && displayed_at("sym/area.c", 1ULL),
              "Go to Definition opens the declaration's file at its line");
    cli_check(tab_opened("sym/use.c"), "use.c is displayed again");
    cursor_put(3ULL, 12ULL);
    // the tools are fewer than an int counts
    cli_check(asked("usages", NULL) && (cli_references_rows() == 7u) &&
                  (s_window.side_menu.displayed == (int)tool_at("references")),
              "Find Usages lists the references by file in the References pane");
    cursor_put(3ULL, 12ULL);
    cli_check(asked("rename", NULL) && s_window.rename.open && s_window.rename.selected,
              "Rename Symbol opens its box over the name with the name selected");
    typed("extent");
    key_pressed("Enter");
    const int renamed =
        !s_window.rename.open && asked("save-all", NULL) &&
        file_reads("sym/use.c", "#include \"shape.h\"\nint total(int n)\n{\n    int w = extent(n) + SIDES;\n    "
                                "return w + extent(1);\n}\n") &&
        file_reads("sym/area.c", "#include \"shape.h\"\nint extent(int n)\n{\n    return n * n * SIDES;\n}\n") &&
        file_reads("sym/shape.h", "#ifndef SHAPE_H\n#define SHAPE_H\nint extent(int side);\n#define SIDES 4\n#endif\n");
    if (!renamed)
    {
        // Write the rename's notice, which says why the window refused it, beside the check.
        const char *const notice = cli_notice_displayed();
        (void)fprintf(stderr, "  rename: box open %d, notice %s\n", s_window.rename.open,
                      (notice != NULL) ? notice : "none");
    }
    cli_check(renamed, "Enter renames the name in every file and Save All writes them");
}

// A pointer that moves from cell to cell over the text owes no frame while no panel shows. A rest on a word asks the
// tree on a worker. The hover lands through the step.
static void hover_checked(void)
{
    cli_check(tab_opened("sym/use.c"), "use.c opens for the hover");
    cursor_put(3ULL, 12ULL);
    // the check reads the caret box at a frame that places the cursor on the word
    unsigned int frames = 0u;
    while ((frames < 100u) &&
           ((editor_frame() == NULL) || (editor_frame()->cursor.line != 3ULL) || (editor_frame()->cursor.col != 12ULL)))
    {
        stepped_for(20ULL);
        frames += 1u;
    }
    const GuiRect word = caret_box();
    const int x = word.x + (word.width / 2);
    const int y = word.y + (word.height / 2);
    // the pointer comes onto the text first, so that the moves after it go from cell to cell over it
    moved(x - (word.width * 8), y);
    s_window.pending = 0;
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = GUI_MOVED_INPUT(x - (word.width * 7), y));
    cli_check(!s_window.pending,
              "a pointer moved onto another cell of the text, with no panel displayed, owes no frame");
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window, .input = GUI_MOVED_INPUT(x, y));
    cli_check(!s_window.pending, "nor does one moved onto the word's cell");
    stepped();
    unsigned int waits = 0u;
    while ((s_window.hover.panel == NULL) && (waits < 200u))
    {
        stepped_for(50ULL);
        waits += 1u;
    }
    cli_check((s_window.hover.panel != NULL) && (s_window.hover_panel.line == 3ULL),
              "the pointer at rest on area brings the tree's hover of it, landed from a worker");
    s_window.pending = 0;
    EMBED_CALL(gui_window_input_type.input, GuiWindowInputTypeCfg, .window = &s_window,
               .input = GUI_MOVED_INPUT(x + word.width, y));
    cli_check(!s_window.pending, "a pointer moved within the hover's word owes no frame");
}

// Return whether the check named `name` runs. Every check runs if ORIOR_WINDOW_CHECKS has no value. Otherwise those it
// names run, separated by commas.
static int check_wanted(const char *name)
{
    char named[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_WINDOW_CHECKS",
                    .value_out = named, .capacity = sizeof(named)) ||
        (named[0] == '\0'))
    {
        return 1;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = sizeof(named)});
    unsigned long long from = 0ULL;
    while (from <= size)
    {
        const unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                 .text = named, .at = from, .size = size, .set = ",");
        if (((at - from) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = named + from, .other = name, .bytes = (embed_word)length}) == 0))
        {
            return 1;
        }
        from = at + 1ULL;
    }
    return 0;
}

// A check of the window after its start, by the name ORIOR_WINDOW_CHECKS selects it by
typedef struct
{
    const char *name;
    void (*run)(void);
} WindowCheck;

static const WindowCheck s_checks[] = {{"saves", saves_checked},
                                       {"closes", closes_checked},
                                       {"save_rows", save_rows_checked},
                                       {"failed_save", failed_save_checked},
                                       {"cells_rounds", cells_rounds_checked},
                                       {"typed_colored", typed_colored_checked},
                                       {"grammar_held", grammar_held_checked},
                                       {"edits_cells", edits_cells_checked},
                                       {"idle_pages", idle_pages_checked},
                                       {"retyped_colored", retyped_colored_checked},
                                       {"languages_colored", languages_colored_checked},
                                       {"reduce_motion", reduce_motion_checked},
                                       {"place", place_checked},
                                       {"table", table_checked},
                                       {"caret", caret_checked},
                                       {"explorer_stays", explorer_stays_checked},
                                       {"terminal_menu", terminal_menu_checked},
                                       {"tab_menu", tab_menu_checked},
                                       {"editor_menu", editor_menu_checked},
                                       {"minimap_lines", minimap_lines_checked},
                                       {"autoscroll", autoscroll_checked},
                                       {"tips", tips_checked},
                                       {"bar_tools", bar_tools_checked},
                                       {"preferences_window", preferences_window_checked},
                                       {"redraws", redraws_checked},
                                       {"pointer", pointer_checked},
                                       {"run_keys", run_keys_checked},
                                       {"sessions", sessions_checked},
                                       {"raw", raw_checked},
                                       {"map_collapses", map_collapses_checked},
                                       {"themes", themes_checked},
                                       {"sticky_notch", sticky_notch_checked},
                                       {"view", view_checked},
                                       {"edit", edit_checked},
                                       {"clipboard", clipboard_checked},
                                       {"symbols", symbols_checked},
                                       {"hover", hover_checked},
                                       {"view_thread", view_thread_checked},
                                       {"file", file_checked}};

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check_folder("window_tree", s_root);
    // Write the symbol commands' files before the tree's read, which lets its walk read them.
    char sym[CLI_PATH_MAX_LEN];
    cli_check_folder("window_tree/sym", sym);
    file_written("sym/shape.h", "#ifndef SHAPE_H\n#define SHAPE_H\nint area(int side);\n#define SIDES 4\n#endif\n");
    file_written("sym/area.c", "#include \"shape.h\"\nint area(int n)\n{\n    return n * n * SIDES;\n}\n");
    file_written(
        "sym/use.c",
        "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n");
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
    // the checks read the window's telemetry lines, which a window writes only when asked
    s_window.telemetry = 1;
    cli_document_telemetry_on();
    (void)EMBED_CALL(gui_window_width.resize, GuiWindowWidthCfg, .window = &s_window, .width = 1440u, .height = 920u,
                     .system_scale = 100);
    // the tree as the window began it, the name of each file the tabs show is its path from it
    memor.cpy(&(MemoriaCfg){
        .dst = s_root,
        .src = cli_command_line_root(),
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_command_line_root(), .cap = CLI_PATH_MAX_LEN}) + 1u});
    // Leave the name's pane, which takes every key and all text while it is, after the tree's read.
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (s_window.wordmark.exists && (cli_clock_milliseconds() < last_ms))
    {
        stepped();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    cli_check(!s_window.wordmark.exists, "the name's pane leaves once the tree is read");
    // the place past the folder's last slash of either type, 0 where it has none
    const unsigned long long past_slash =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = s_root,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = s_root, .cap = sizeof(s_root)}), .set = "/\\");
    cli_check((s_window.title != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = s_window.title,
                                                                       .cap = CLI_PATH_MAX_LEN,
                                                                       .other = s_root + past_slash,
                                                                       .other_cap = CLI_PATH_MAX_LEN}),
              "the window's title is the tree's name, the last part of its folder");
    for (unsigned int at = 0u; at < (sizeof(s_checks) / sizeof(s_checks[0])); at += 1u)
    {
        if (check_wanted(s_checks[at].name))
        {
            s_checks[at].run();
        }
    }
    EMBED_CALL(gui_window_table.end, GuiWindowCfg, .window = &s_window);
    cli_command_line_tree_end();
    return cli_on_check();
}
