// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal_tests.c: the tree's tests run at the terminal and waited for, each test that failed reported with the
// first line it printed, each file's coverage, and the counts, a run that ran no test or failed one refused; and the
// settings a file or a folder carries taken in, each file and setting named or every one that differs, and a taking
// undone
#include "../machine/carry.h"
#include "../tests/test_runs.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "at_terminal.h"

// the most files and settings one taking takes; the most bytes of the lines a run's failed tests or its coverage print;
// and the most bytes of what is carried written as its differences or as a taking
#define CLI_AT_TERMINAL_TAKEN 4096u
#define CLI_AT_TERMINAL_LINES (1u << 20u)
#define CLI_AT_TERMINAL_CARRIED (8ull << 20u)

// Lines a run prints, buffered through the run: the line over a cell of the most they hold, and how many are buffered
typedef struct
{
    VerbaLinea line;
    unsigned long long lines;
} CliTestLines;

// what a run of tests has reported: the tests that failed and the coverage, each a line, the counts once it is done,
// and whether it is, under the lock its waiter waits on
static CliTestLines s_failed;
static CliTestLines s_covered;
static char s_counts[4096];
static unsigned long long s_counts_used;
static int s_done;
static int s_good;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// `size` bytes of `text` put after the lines buffered, a line of its own, a line past the most the lines contain left
// out
static void tests_line_put(CliTestLines *lines, const char *text, unsigned long long size)
{
    if (lines->line.at + size + 1ull > CLI_AT_TERMINAL_LINES)
    {
        return;
    }
    mmgr_line_put(&lines->line, (lines->lines > 0ull) ? "\n" : "");
    mmgr_line_put_n(&lines->line, text, size);
    lines->lines += 1ull;
}

// the line of the lines buffered begun over a cell of the most they hold
static void tests_lines_begun(CliTestLines *lines)
{
    const unsigned long long room = CLI_AT_TERMINAL_LINES + 1ull;
    char *const text = g_cli_cells->temporary_buf_alloc(room, &g_cli_command_line_cells);
    lines->line = mmgr_line_over(text, room);
    lines->lines = 0ull;
}

// the cell of the lines buffered given back
static void tests_lines_given(CliTestLines *lines)
{
    g_cli_cells->persistent_buf_release(lines->line.out, &g_cli_command_line_cells);
    lines->line = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    lines->lines = 0ull;
}

// the lines buffered written into `into` and a line's end after them, where any are buffered, and their cell given back
static void tests_lines_written(CliTestLines *lines, VerbaLinea *into)
{
    if (mmgr_line_ok(&lines->line) && (lines->lines > 0ull))
    {
        (void)mmgr_line_put_n(into, lines->line.out, lines->line.at);
        mmgr_line_ch(into, '\n');
    }
    tests_lines_given(lines);
}

// a whole number member of `node` in `json`, 0 where it has none
static unsigned long long tests_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int member = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    const char *const digits = cli_json_is(json, member, CFG_JSON_NUMBER) ? json->text + json->node[member].text : "";
    value = cli_text_number(digits, 0ull, cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_CELLS_POOL}), NULL);
    return value;
}

// the counts of a run's end written, `{passed} passed, {failed} failed, {skipped} skipped in {seconds} s`, the
// seconds to a tenth, and what it printed after a colon where it printed anything
static void tests_counts(const CliJson *json, unsigned int done)
{
    VerbaLinea line = {s_counts, sizeof(s_counts), 0ull};
    const unsigned long long passed = tests_whole(json, done, "passed");
    const unsigned long long failed = tests_whole(json, done, "failed");
    const unsigned long long skipped = tests_whole(json, done, "skipped");
    mmgr_line_decimal(&line, passed, 1u);
    mmgr_line_put(&line, " passed, ");
    mmgr_line_decimal(&line, failed, 1u);
    mmgr_line_put(&line, " failed, ");
    mmgr_line_decimal(&line, skipped, 1u);
    mmgr_line_put(&line, " skipped in ");
    const unsigned int seconds = cli_json_member(json, done, "seconds");
    const char *const written =
        cli_json_is(json, seconds, CFG_JSON_NUMBER) ? json->text + json->node[seconds].text : "0";
    // the seconds to a tenth: the whole seconds, then the tenths rounded from the hundredths
    unsigned long long whole = 0ull;
    unsigned long long at = 0ull;
    whole = cli_text_number(written, 0ull, cellul.len(&(CatenaFinitaCfg){.src = written, .cap = CLI_CELLS_POOL}), &at);
    unsigned long long tenths = 0ull;
    if (written[at] == '.')
    {
        const unsigned long long tenth = ((written[at + 1u] >= '0') && (written[at + 1u] <= '9'))
                                             ? (unsigned long long)(written[at + 1u] - '0')
                                             : 0ull;
        const int up =
            (tenth < 10ull) && (written[at + 1u] != '\0') && (written[at + 2u] >= '5') && (written[at + 2u] <= '9');
        tenths = tenth + (up ? 1ull : 0ull);
    }
    whole += tenths / 10ull;
    mmgr_line_decimal(&line, whole, 1u);
    mmgr_line_ch(&line, '.');
    mmgr_line_decimal(&line, tenths % 10ull, 1u);
    mmgr_line_put(&line, " s");
    unsigned long long size = 0ull;
    const char *const err_text = cli_json_string(json, cli_json_member(json, done, "message"), &size);
    if ((err_text != NULL) && (size > 0ull))
    {
        mmgr_line_put(&line, ": ");
        mmgr_line_put_n(&line, err_text, size);
    }
    s_counts_used = mmgr_line_ok(&line) ? line.at : 0ull;
    s_good = (failed == 0ull) && (passed + skipped > 0ull);
}

// what a run reports taken, read into a tree of its size given back after: a test that failed, a file's coverage, and
// the counts at its end
static void tests_event(void *context, const char *body, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    CliJson tree;
    CliJson *const json = &tree;
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = cli_json_taken(json, body, size, 0u, 0ull, &ignored, &g_cli_command_line_cells);
    const char *const kind = cli_json_string(json, cli_json_member(json, root, "kind"), NULL);
    const char *const what = (kind != NULL) ? kind : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "result", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int outcome = cli_json_member(json, root, "outcome");
        const char *const how = cli_json_string(json, cli_json_member(json, outcome, "outcome"), NULL);
        if ((how != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "failed", .cap = CLI_CELLS_POOL})))
        {
            char line[8192];
            VerbaLinea failed = {line, sizeof(line), 0ull};
            unsigned long long message_size = 0ull;
            const char *const id = cli_json_string(json, cli_json_member(json, outcome, "id"), NULL);
            const char *const message = cli_json_string(json, cli_json_member(json, outcome, "message"), &message_size);
            const char *const said = (message != NULL) ? message : "";
            unsigned long long first = cli_text_to_any(said, 0ull, message_size, "\n");
            first -= ((first > 0ull) && (said[first - 1ull] == '\r')) ? 1ull : 0ull;
            mmgr_line_put(&failed, "FAILED ");
            mmgr_line_put(&failed, (id != NULL) ? id : "");
            mmgr_line_put(&failed, ": ");
            mmgr_line_put_n(&failed, said, first);
            tests_line_put(&s_failed, line, mmgr_line_ok(&failed) ? failed.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "coverage", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int files = cli_json_member(json, root, "files");
        for (unsigned int file = cli_json_is(json, files, CFG_JSON_ARRAY) ? json->node[files].first : CLI_JSON_NONE;
             file != CLI_JSON_NONE; file = json->node[file].next)
        {
            const unsigned int ran = cli_json_member(json, file, "ran");
            const unsigned int missed = cli_json_member(json, file, "missed");
            const unsigned int ran_count = cli_json_is(json, ran, CFG_JSON_ARRAY) ? json->node[ran].count : 0u;
            const unsigned int missed_count = cli_json_is(json, missed, CFG_JSON_ARRAY) ? json->node[missed].count : 0u;
            const char *const name = cli_json_string(json, cli_json_member(json, file, "file"), NULL);
            char line[CLI_PATH_MAX_LEN + 64u];
            VerbaLinea covered = {line, sizeof(line), 0ull};
            mmgr_line_put(&covered, (name != NULL) ? name : "");
            mmgr_line_put(&covered, ": ");
            mmgr_line_decimal(&covered, ran_count, 1u);
            mmgr_line_put(&covered, " of ");
            mmgr_line_decimal(&covered, ran_count + missed_count, 1u);
            mmgr_line_put(&covered, " lines ran");
            tests_line_put(&s_covered, line, mmgr_line_ok(&covered) ? covered.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "done", .cap = CLI_CELLS_POOL}))
    {
        tests_counts(json, root);
        s_done = 1;
        cli_wake_all(&s_wake);
    }
    cli_json_given(json);
    cli_lock_give(&s_lock);
}

int cli_at_terminal_tests(const char *root, const char *const *given, unsigned int count, int cover, VerbaLinea *out,
                          VerbaLinea *err_msg)
{
    static CliLock s_running = CLI_LOCK_INIT;
    cli_lock_take(&s_running);
    cli_lock_take(&s_lock);
    tests_lines_begun(&s_failed);
    tests_lines_begun(&s_covered);
    s_counts_used = 0ull;
    s_done = 0;
    s_good = 0;
    cli_lock_give(&s_lock);
    const CliJsonSink sink = {tests_event, NULL};
    unsigned long long run = 0ull;
    if (!cli_test_runs_start(root, given, count, 1, cover, sink, &run, err_msg))
    {
        cli_lock_take(&s_lock);
        tests_lines_given(&s_failed);
        tests_lines_given(&s_covered);
        cli_lock_give(&s_lock);
        cli_lock_give(&s_running);
        return 0;
    }
    cli_lock_take(&s_lock);
    while (!s_done)
    {
        cli_wake_wait(&s_wake, &s_lock);
    }
    VerbaLinea *const into = s_good ? out : err_msg;
    tests_lines_written(&s_failed, into);
    tests_lines_written(&s_covered, into);
    mmgr_line_put_n(into, s_counts, s_counts_used);
    const int good = s_good;
    cli_lock_give(&s_lock);
    cli_lock_give(&s_running);
    return good;
}

// a cell of the most what is carried writes to
static char *tests_carried_taken(void)
{
    return g_cli_cells->temporary_buf_alloc(CLI_AT_TERMINAL_CARRIED, &g_cli_command_line_cells);
}

// what `line` wrote read into `json`, a tree of its size, and the cell it wrote in given back: the tree's root, or
// CLI_JSON_NONE with the tree containing nothing where the text or its tree could not be parsed
static unsigned int tests_tree_of(CliJson *json, char *cell, const VerbaLinea *line)
{
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = mmgr_line_ok(line) ? cli_json_taken_temporary(json, line->out, line->at, 0u, 0ull,
                                                                            &ignored, &g_cli_command_line_cells)
                                                 : CLI_JSON_NONE;
    g_cli_cells->persistent_buf_release(cell, &g_cli_command_line_cells);
    return root;
}

int cli_at_terminal_take(const char *home, const char *from, const char *const *chosen, unsigned int count,
                         VerbaLinea *out, VerbaLinea *err_msg)
{
    static CliCarried s_there;
    static CliLock s_take_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_take_lock);
    if (!cli_carry_read(from, &s_there, err_msg))
    {
        cli_lock_give(&s_take_lock);
        return 0;
    }
    char *const differences_text = tests_carried_taken();
    VerbaLinea differences = mmgr_line_over(differences_text, CLI_AT_TERMINAL_CARRIED);
    cli_carry_differences(home, &s_there, &differences);
    CliJson tree;
    CliJson *const json = &tree;
    const unsigned int all = tests_tree_of(json, differences_text, &differences);
    // a place and a key are each a node of the differences: as many as their nodes, and settings.json, contain them
    const unsigned int room = ((json->nodes + 1u) < CLI_AT_TERMINAL_TAKEN) ? (json->nodes + 1u) : CLI_AT_TERMINAL_TAKEN;
    const char **const chosen_places =
        g_cli_cells->temporary_buf_alloc(2ull * room * sizeof(const char *), &g_cli_command_line_cells);
    if (chosen_places == NULL)
    {
        cli_json_given(json);
        cli_lock_give(&s_take_lock);
        mmgr_line_put(err_msg, "the cell the places and settings taken are listed in could not be taken");
        return 0;
    }
    const char **const chosen_keys = chosen_places + room;
    unsigned int places = 0u;
    unsigned int keys = 0u;
    for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        const char *const place = cli_json_string(json, cli_json_member(json, one, "place"), NULL);
        const char *const how = cli_json_string(json, cli_json_member(json, one, "how"), NULL);
        int wanted = (count == 0u);
        for (unsigned int at = 0u; !wanted && (place != NULL) && (at < count); at += 1u)
        {
            wanted = (cellul.eq(&(CatenaFinitaCfg){.src = chosen[at], .other = place, .cap = CLI_CELLS_POOL}));
        }
        if ((place != NULL) && (how != NULL) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "here", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL})) && wanted &&
            (places < room))
        {
            chosen_places[places] = place;
            places += 1u;
        }
        const unsigned int settings = cli_json_member(json, one, "settings");
        for (unsigned int setting = cli_json_is(json, settings, CFG_JSON_ARRAY) ? json->node[settings].first
                                                                                : CLI_JSON_NONE;
             setting != CLI_JSON_NONE; setting = json->node[setting].next)
        {
            const char *const key = cli_json_string(json, cli_json_member(json, setting, "key"), NULL);
            int named = (count == 0u);
            for (unsigned int at = 0u; !named && (key != NULL) && (at < count); at += 1u)
            {
                named = (cellul.eq(&(CatenaFinitaCfg){.src = chosen[at], .other = key, .cap = CLI_CELLS_POOL}));
            }
            if ((key != NULL) && named && (keys < room))
            {
                chosen_keys[keys] = key;
                keys += 1u;
            }
        }
    }
    if ((keys > 0u) && (places < room))
    {
        chosen_places[places] = "settings.json";
        places += 1u;
    }
    int done = 1;
    if (places == 0u)
    {
        mmgr_line_put(out, from);
        mmgr_line_put(out, " contains the settings this machine has.");
    }
    else
    {
        char *const taking_text = tests_carried_taken();
        VerbaLinea taking = mmgr_line_over(taking_text, CLI_AT_TERMINAL_CARRIED);
        done = cli_carry_take(home, from, &s_there, chosen_places, places, chosen_keys, keys, &taking, err_msg);
        CliJson took_tree;
        const unsigned int took = tests_tree_of(&took_tree, taking_text, &taking);
        if (done)
        {
            const unsigned int files = cli_json_member(&took_tree, took, "files");
            const unsigned int taken =
                cli_json_is(&took_tree, files, CFG_JSON_ARRAY) ? took_tree.node[files].count : 0u;
            unsigned long long id_size = 0ull;
            const char *const id = cli_json_string(&took_tree, cli_json_member(&took_tree, took, "id"), &id_size);
            mmgr_line_put(out, "Took ");
            mmgr_line_decimal(out, taken, 1u);
            mmgr_line_put(out, (taken == 1u) ? " file from " : " files from ");
            mmgr_line_put(out, from);
            mmgr_line_put(out, "; carry-undo ");
            mmgr_line_put_n(out, (id != NULL) ? id : "", id_size);
            mmgr_line_put(out, " writes them back.");
        }
        cli_json_given(&took_tree);
    }
    g_cli_cells->persistent_buf_release(chosen_places, &g_cli_command_line_cells);
    cli_json_given(json);
    cli_lock_give(&s_take_lock);
    return done;
}

int cli_at_terminal_undo(const char *home, const char *id, VerbaLinea *out, VerbaLinea *err_msg)
{
    char *const taking_text = tests_carried_taken();
    VerbaLinea taking = mmgr_line_over(taking_text, CLI_AT_TERMINAL_CARRIED);
    const int done = cli_carry_undo(home, id, &taking, err_msg);
    CliJson tree;
    const unsigned int took = tests_tree_of(&tree, taking_text, &taking);
    if (done)
    {
        unsigned long long size = 0ull;
        const char *const from = cli_json_string(&tree, cli_json_member(&tree, took, "from"), &size);
        mmgr_line_put(out, "The taking from ");
        mmgr_line_put_n(out, (from != NULL) ? from : "", size);
        mmgr_line_put(out, " is undone.");
    }
    cli_json_given(&tree);
    return done;
}
