// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal_tests.c: the tree's tests run at the terminal and waited for, each test that failed said with the first
// line it said, each file's coverage, and the counts, a run that ran no test or failed one refused; and the settings a
// file or a folder carries taken in, each file and setting named or every one that differs, and a taking undone
#include "../machine/carry.h"
#include "../tests/test_runs.h"
#include "../text/json_tree.h"
#include "at_terminal.h"

// the most files and settings one taking takes
#define CLI_AT_TERMINAL_TAKEN 4096u

// what a run of tests has said: the tests that failed and the coverage, each a line, the counts once it is done, and
// whether it is, under the lock its waiter waits on
static char s_failed[1u << 20u];
static unsigned long long s_failed_used;
static char s_covered[1u << 20u];
static unsigned long long s_covered_used;
static char s_counts[4096];
static unsigned long long s_counts_used;
static int s_done;
static int s_good;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static CliJsonNode s_node[1u << 16u];
static char s_text[1u << 22u];
static CfgJsonToken s_token[1u << 16u];
static CliJson s_json;

// `text` put after the lines kept in `room`, a line of its own
static void tests_line_put(char *room, unsigned long long most, unsigned long long *used, const char *text,
                           unsigned long long size)
{
    CliLine line = {room, most, *used};
    cli_line_text(&line, (*used > 0ull) ? "\n" : "");
    cli_line_counted(&line, text, size);
    *used = cli_line_fits(&line) ? line.at : *used;
}

// a whole number member of `node`, 0 where it has none
static unsigned long long tests_whole(unsigned int node, const char *name)
{
    const unsigned int member = cli_json_member(&s_json, node, name);
    unsigned long long value = 0ull;
    const char *const digits =
        cli_json_is(&s_json, member, CFG_JSON_NUMBER) ? s_json.text + s_json.node[member].text : "";
    for (unsigned long long at = 0ull; (digits[at] >= '0') && (digits[at] <= '9'); at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    return value;
}

// the counts of a run's end written, `{passed} passed, {failed} failed, {skipped} skipped in {seconds} s`, the
// seconds to a tenth, and what it said after a colon where it said anything
static void tests_counts(unsigned int done)
{
    CliLine line = {s_counts, sizeof(s_counts), 0ull};
    const unsigned long long passed = tests_whole(done, "passed");
    const unsigned long long failed = tests_whole(done, "failed");
    const unsigned long long skipped = tests_whole(done, "skipped");
    cli_line_decimal(&line, passed, 1u);
    cli_line_text(&line, " passed, ");
    cli_line_decimal(&line, failed, 1u);
    cli_line_text(&line, " failed, ");
    cli_line_decimal(&line, skipped, 1u);
    cli_line_text(&line, " skipped in ");
    const unsigned int seconds = cli_json_member(&s_json, done, "seconds");
    const char *const written =
        cli_json_is(&s_json, seconds, CFG_JSON_NUMBER) ? s_json.text + s_json.node[seconds].text : "0";
    // the seconds to a tenth: the whole seconds, then the tenths rounded from the hundredths
    unsigned long long whole = 0ull;
    unsigned long long at = 0ull;
    for (; (written[at] >= '0') && (written[at] <= '9'); at += 1ull)
    {
        whole = (whole * 10ull) + (unsigned long long)(written[at] - '0');
    }
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
    cli_line_decimal(&line, whole, 1u);
    cli_line_character(&line, '.');
    cli_line_decimal(&line, tenths % 10ull, 1u);
    cli_line_text(&line, " s");
    unsigned long long size = 0ull;
    const char *const said = cli_json_string(&s_json, cli_json_member(&s_json, done, "said"), &size);
    if ((said != NULL) && (size > 0ull))
    {
        cli_line_text(&line, ": ");
        cli_line_counted(&line, said, size);
    }
    s_counts_used = cli_line_fits(&line) ? line.at : 0ull;
    s_good = (failed == 0ull) && (passed + skipped > 0ull);
}

// what a run tells taken: a test that failed, a file's coverage, and the counts at its end
static void tests_told(void *context, const char *heard, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
    char quiet[256];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int root =
        cli_json_read(&s_json, heard, size, s_token, sizeof(s_token) / sizeof(s_token[0]), &ignored);
    const char *const kind = cli_json_string(&s_json, cli_json_member(&s_json, root, "kind"), NULL);
    const char *const what = (kind != NULL) ? kind : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "result", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int outcome = cli_json_member(&s_json, root, "outcome");
        const char *const how = cli_json_string(&s_json, cli_json_member(&s_json, outcome, "outcome"), NULL);
        if ((how != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "failed", .cap = CLI_CELLS_POOL})))
        {
            char line[8192];
            CliLine failed = {line, sizeof(line), 0ull};
            unsigned long long message_size = 0ull;
            const char *const id = cli_json_string(&s_json, cli_json_member(&s_json, outcome, "id"), NULL);
            const char *const message =
                cli_json_string(&s_json, cli_json_member(&s_json, outcome, "message"), &message_size);
            unsigned long long first = 0ull;
            while ((message != NULL) && (first < message_size) && (message[first] != '\n'))
            {
                first += 1ull;
            }
            first -= ((first > 0ull) && (message[first - 1ull] == '\r')) ? 1ull : 0ull;
            cli_line_text(&failed, "FAILED ");
            cli_line_text(&failed, (id != NULL) ? id : "");
            cli_line_text(&failed, ": ");
            cli_line_counted(&failed, (message != NULL) ? message : "", first);
            tests_line_put(s_failed, sizeof(s_failed), &s_failed_used, line, cli_line_fits(&failed) ? failed.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "coverage", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int files = cli_json_member(&s_json, root, "files");
        for (unsigned int file = cli_json_is(&s_json, files, CFG_JSON_ARRAY) ? s_json.node[files].first : CLI_JSON_NONE;
             file != CLI_JSON_NONE; file = s_json.node[file].next)
        {
            const unsigned int ran = cli_json_member(&s_json, file, "ran");
            const unsigned int missed = cli_json_member(&s_json, file, "missed");
            const unsigned int ran_count = cli_json_is(&s_json, ran, CFG_JSON_ARRAY) ? s_json.node[ran].count : 0u;
            const unsigned int missed_count =
                cli_json_is(&s_json, missed, CFG_JSON_ARRAY) ? s_json.node[missed].count : 0u;
            const char *const name = cli_json_string(&s_json, cli_json_member(&s_json, file, "file"), NULL);
            char line[CLI_PATH_LONGEST + 64u];
            CliLine covered = {line, sizeof(line), 0ull};
            cli_line_text(&covered, (name != NULL) ? name : "");
            cli_line_text(&covered, ": ");
            cli_line_decimal(&covered, ran_count, 1u);
            cli_line_text(&covered, " of ");
            cli_line_decimal(&covered, ran_count + missed_count, 1u);
            cli_line_text(&covered, " lines ran");
            tests_line_put(s_covered, sizeof(s_covered), &s_covered_used, line,
                           cli_line_fits(&covered) ? covered.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "done", .cap = CLI_CELLS_POOL}))
    {
        tests_counts(root);
        s_done = 1;
        cli_wake_all(&s_wake);
    }
    cli_lock_give(&s_lock);
}

int cli_at_terminal_tests(const char *root, const char *const *given, unsigned int count, int cover, CliLine *out,
                          CliLine *said)
{
    static CliLock s_running = CLI_LOCK_INIT;
    cli_lock_take(&s_running);
    cli_lock_take(&s_lock);
    s_failed_used = 0ull;
    s_covered_used = 0ull;
    s_counts_used = 0ull;
    s_done = 0;
    s_good = 0;
    cli_lock_give(&s_lock);
    const CliTell tell = {tests_told, NULL};
    unsigned long long run = 0ull;
    if (!cli_test_runs_start(root, given, count, 1, cover, tell, &run, said))
    {
        cli_lock_give(&s_running);
        return 0;
    }
    cli_lock_take(&s_lock);
    while (!s_done)
    {
        cli_wake_wait(&s_wake, &s_lock);
    }
    CliLine *const into = s_good ? out : said;
    if (s_failed_used > 0ull)
    {
        cli_line_counted(into, s_failed, s_failed_used);
        cli_line_character(into, '\n');
    }
    if (s_covered_used > 0ull)
    {
        cli_line_counted(into, s_covered, s_covered_used);
        cli_line_character(into, '\n');
    }
    cli_line_counted(into, s_counts, s_counts_used);
    const int good = s_good;
    cli_lock_give(&s_lock);
    cli_lock_give(&s_running);
    return good;
}

int cli_at_terminal_take(const char *home, const char *from, const char *const *chosen, unsigned int count,
                         CliLine *out, CliLine *said)
{
    static CliCarried s_there;
    static char s_differences[1u << 22u];
    static char s_taking[1u << 20u];
    static const char *s_places[CLI_AT_TERMINAL_TAKEN];
    static const char *s_keys[CLI_AT_TERMINAL_TAKEN];
    static CliLock s_take_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_take_lock);
    if (!cli_carry_read(from, &s_there, said))
    {
        cli_lock_give(&s_take_lock);
        return 0;
    }
    CliLine differences = {s_differences, sizeof(s_differences), 0ull};
    cli_carry_differences(home, &s_there, &differences);
    cli_lock_take(&s_lock);
    cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
    char quiet[256];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int all =
        cli_json_read(&s_json, s_differences, differences.at, s_token, sizeof(s_token) / sizeof(s_token[0]), &ignored);
    unsigned int places = 0u;
    unsigned int keys = 0u;
    for (unsigned int one = cli_json_is(&s_json, all, CFG_JSON_ARRAY) ? s_json.node[all].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = s_json.node[one].next)
    {
        const char *const place = cli_json_string(&s_json, cli_json_member(&s_json, one, "place"), NULL);
        const char *const how = cli_json_string(&s_json, cli_json_member(&s_json, one, "how"), NULL);
        int wanted = (count == 0u);
        for (unsigned int at = 0u; !wanted && (place != NULL) && (at < count); at += 1u)
        {
            wanted = (cellul.eq(&(CatenaFinitaCfg){.src = chosen[at], .other = place, .cap = CLI_CELLS_POOL}));
        }
        if ((place != NULL) && (how != NULL) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "here", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL})) && wanted &&
            (places < CLI_AT_TERMINAL_TAKEN))
        {
            s_places[places] = place;
            places += 1u;
        }
        const unsigned int settings = cli_json_member(&s_json, one, "settings");
        for (unsigned int setting = cli_json_is(&s_json, settings, CFG_JSON_ARRAY) ? s_json.node[settings].first
                                                                                   : CLI_JSON_NONE;
             setting != CLI_JSON_NONE; setting = s_json.node[setting].next)
        {
            const char *const key = cli_json_string(&s_json, cli_json_member(&s_json, setting, "key"), NULL);
            int named = (count == 0u);
            for (unsigned int at = 0u; !named && (key != NULL) && (at < count); at += 1u)
            {
                named = (cellul.eq(&(CatenaFinitaCfg){.src = chosen[at], .other = key, .cap = CLI_CELLS_POOL}));
            }
            if ((key != NULL) && named && (keys < CLI_AT_TERMINAL_TAKEN))
            {
                s_keys[keys] = key;
                keys += 1u;
            }
        }
    }
    if ((keys > 0u) && (places < CLI_AT_TERMINAL_TAKEN))
    {
        s_places[places] = "settings.json";
        places += 1u;
    }
    int done = 1;
    if (places == 0u)
    {
        cli_line_text(out, from);
        cli_line_text(out, " holds the settings this machine has.");
    }
    else
    {
        CliLine taking = {s_taking, sizeof(s_taking), 0ull};
        done = cli_carry_take(home, from, &s_there, s_places, places, s_keys, keys, &taking, said);
        if (done)
        {
            cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
            const unsigned int took =
                cli_json_read(&s_json, s_taking, taking.at, s_token, sizeof(s_token) / sizeof(s_token[0]), &ignored);
            const unsigned int files = cli_json_member(&s_json, took, "files");
            const unsigned int taken = cli_json_is(&s_json, files, CFG_JSON_ARRAY) ? s_json.node[files].count : 0u;
            unsigned long long id_size = 0ull;
            const char *const id = cli_json_string(&s_json, cli_json_member(&s_json, took, "id"), &id_size);
            cli_line_text(out, "Took ");
            cli_line_decimal(out, taken, 1u);
            cli_line_text(out, (taken == 1u) ? " file from " : " files from ");
            cli_line_text(out, from);
            cli_line_text(out, "; carry-undo ");
            cli_line_counted(out, (id != NULL) ? id : "", id_size);
            cli_line_text(out, " writes them back.");
        }
    }
    cli_lock_give(&s_lock);
    cli_lock_give(&s_take_lock);
    return done;
}

int cli_at_terminal_undo(const char *home, const char *id, CliLine *out, CliLine *said)
{
    static char s_taking[1u << 20u];
    cli_lock_take(&s_lock);
    CliLine taking = {s_taking, sizeof(s_taking), 0ull};
    const int done = cli_carry_undo(home, id, &taking, said);
    if (done)
    {
        cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int took =
            cli_json_read(&s_json, s_taking, taking.at, s_token, sizeof(s_token) / sizeof(s_token[0]), &ignored);
        unsigned long long size = 0ull;
        const char *const from = cli_json_string(&s_json, cli_json_member(&s_json, took, "from"), &size);
        cli_line_text(out, "The taking from ");
        cli_line_counted(out, (from != NULL) ? from : "", size);
        cli_line_text(out, " is undone.");
    }
    cli_lock_give(&s_lock);
    return done;
}
