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
#include "callback_cfg.h"

// Lines a run prints, buffered through the run: the line over a cell of the most they hold, and how many are buffered
typedef struct
{
    VerbaLinea line;
    unsigned long long lines;
} CliTestLines;
// the most files and settings one taking takes; the most bytes of the lines a run's failed tests or its coverage print;
// and the most bytes of what is carried written as its differences or as a taking
#define CLI_AT_TERMINAL_ALLOCATED 4096u
#define CLI_AT_TERMINAL_LINES (1u << 20u)
#define CLI_AT_TERMINAL_TRANSFERRED (8ull << 20u)

// what a run of tests has reported: the tests that failed and the coverage, each a line, the counts once it is done,
// and whether it is, under the lock its waiter waits on
static CliTestLines s_failed;
static CliTestLines s_covered;
static char s_counts[4096];
static unsigned long long s_counts_used;
static int s_done;
static int s_good;
static CliWake s_wake = CLI_WAKE_INIT;

typedef struct
{
    CliTestLines *lines;
    const char *text;
    unsigned long long size;
    VerbaLinea *into;
    const CliJson *json;
    unsigned int node;
    const char *name;
    unsigned int done;
    CliJson *json_out;
    char *cell;
    const VerbaLinea *line;
} AtTerminalTestsCtx;
// `size` bytes of `text` put after the lines buffered, a line of its own, a line past the most the lines contain left
// out
EMBED_INLINE void tests_line_append(const AtTerminalTestsCtx *args)
{
    CliTestLines *lines = args->lines;
    if (lines->line.at + args->size + 1ull > CLI_AT_TERMINAL_LINES)
    {
        return;
    }
    mmgr_line_put(&lines->line, (lines->lines > 0ull) ? "\n" : "");
    mmgr_line_put_n(&lines->line, args->text, args->size);
    lines->lines += 1ull;
}

// the line of the lines buffered begun over a cell of the most they hold
EMBED_INLINE void tests_lines_begin(const AtTerminalTestsCtx *args)
{
    CliTestLines *lines = args->lines;
    const unsigned long long capacity = CLI_AT_TERMINAL_LINES + 1ull;
    char *const text = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_command_line_cells);
    lines->line = mmgr_line_over(text, capacity);
    lines->lines = 0ull;
}

// the cell of the lines buffered given back
EMBED_INLINE void tests_lines_free(const AtTerminalTestsCtx *args)
{
    CliTestLines *lines = args->lines;
    g_cli_cells->persistent_buf_release(lines->line.out, &g_cli_command_line_cells);
    lines->line = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    lines->lines = 0ull;
}

// the lines buffered written into `into` and a line's end after them, where any are buffered, and their cell given back
EMBED_INLINE void tests_lines_write(const AtTerminalTestsCtx *args)
{
    if (mmgr_line_ok(&args->lines->line) && (args->lines->lines > 0ull))
    {
        (void)mmgr_line_put_n(args->into, args->lines->line.out, args->lines->line.at);
        mmgr_line_ch(args->into, '\n');
    }
    tests_lines_free(args);
}

// a whole number member of `node` in `json`, 0 where it has none
EMBED_INLINE unsigned long long tests_integer(const AtTerminalTestsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = node, .name = name);
    unsigned long long value = 0ull;
    const char *const digits = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = member, .kind = CFG_JSON_NUMBER)
                                   ? json->text + json->node[member].text
                                   : "";
    value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = digits, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = digits, .cap = CLI_CELLS_POOL}), .end = NULL);
    return value;
}

// the counts of a run's end written, `{passed} passed, {failed} failed, {skipped} skipped in {seconds} s`, the
// seconds to a tenth, and what it printed after a colon where it printed anything
EMBED_INLINE void tests_counts(const AtTerminalTestsCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea line = {s_counts, sizeof(s_counts), 0ull};
    const unsigned long long passed =
        EMBED_CALL(tests_integer, AtTerminalTestsCtx, .json = json, .node = args->done, .name = "passed");
    const unsigned long long failed =
        EMBED_CALL(tests_integer, AtTerminalTestsCtx, .json = json, .node = args->done, .name = "failed");
    const unsigned long long skipped =
        EMBED_CALL(tests_integer, AtTerminalTestsCtx, .json = json, .node = args->done, .name = "skipped");
    mmgr_line_decimal(&line, passed, 1u);
    mmgr_line_put(&line, " passed, ");
    mmgr_line_decimal(&line, failed, 1u);
    mmgr_line_put(&line, " failed, ");
    mmgr_line_decimal(&line, skipped, 1u);
    mmgr_line_put(&line, " skipped in ");
    const unsigned int seconds = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->done, .name = "seconds");
    const char *const written = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = seconds, .kind = CFG_JSON_NUMBER)
                                    ? json->text + json->node[seconds].text
                                    : "0";
    // the seconds to a tenth: the whole seconds, then the tenths rounded from the hundredths
    unsigned long long whole = 0ull;
    unsigned long long at = 0ull;
    whole = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = written, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = written, .cap = CLI_CELLS_POOL}), .end = &at);
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
    const char *const err_text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->done, .name = "message"),
                   .length_pointer = &size);
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
static void tests_event(const CliEachJsonSizeCfg *args)
{
    unsigned long long size = args->size;
    CliJson tree;
    CliJson *const json = &tree;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = json, .text = args->json, .size = size, .nodes_more = 0u,
                                         .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_command_line_cells);
    const char *const kind =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = root, .name = "kind"),
                   .length_pointer = NULL);
    const char *const what = (kind != NULL) ? kind : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "result", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int outcome = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = root, .name = "outcome");
        const char *const how =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = outcome, .name = "outcome"),
                       .length_pointer = NULL);
        if ((how != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "failed", .cap = CLI_CELLS_POOL})))
        {
            char line[8192];
            VerbaLinea failed = {line, sizeof(line), 0ull};
            unsigned long long message_size = 0ull;
            const char *const id =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = outcome, .name = "id"),
                           .length_pointer = NULL);
            const char *const message =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = outcome, .name = "message"),
                           .length_pointer = &message_size);
            const char *const output = (message != NULL) ? message : "";
            unsigned long long first = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = output, .at = 0ull, .size = message_size, .set = "\n");
            first -= ((first > 0ull) && (output[first - 1ull] == '\r')) ? 1ull : 0ull;
            mmgr_line_put(&failed, "FAILED ");
            mmgr_line_put(&failed, (id != NULL) ? id : "");
            mmgr_line_put(&failed, ": ");
            mmgr_line_put_n(&failed, output, first);
            EMBED_CALL(tests_line_append, AtTerminalTestsCtx, .lines = &s_failed, .text = line,
                       .size = mmgr_line_ok(&failed) ? failed.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "coverage", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = root, .name = "files");
        for (unsigned int file = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = files, .kind = CFG_JSON_ARRAY)
                                     ? json->node[files].first
                                     : CLI_JSON_NONE;
             file != CLI_JSON_NONE; file = json->node[file].next)
        {
            const unsigned int run_result = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = json, .object = file, .name = "ran");
            const unsigned int missed = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = json, .object = file, .name = "missed");
            const unsigned int ran_count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                      .json_read = json, .node = run_result, .kind = CFG_JSON_ARRAY)
                                               ? json->node[run_result].count
                                               : 0u;
            const unsigned int missed_count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                         .json_read = json, .node = missed, .kind = CFG_JSON_ARRAY)
                                                  ? json->node[missed].count
                                                  : 0u;
            const char *const name =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = file, .name = "file"),
                           .length_pointer = NULL);
            char line[CLI_PATH_MAX_LEN + 64u];
            VerbaLinea covered = {line, sizeof(line), 0ull};
            mmgr_line_put(&covered, (name != NULL) ? name : "");
            mmgr_line_put(&covered, ": ");
            mmgr_line_decimal(&covered, ran_count, 1u);
            mmgr_line_put(&covered, " of ");
            mmgr_line_decimal(&covered, ran_count + missed_count, 1u);
            mmgr_line_put(&covered, " lines ran");
            EMBED_CALL(tests_line_append, AtTerminalTestsCtx, .lines = &s_covered, .text = line,
                       .size = mmgr_line_ok(&covered) ? covered.at : 0ull);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = what, .other = "done", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(tests_counts, AtTerminalTestsCtx, .json = json, .done = root);
        s_done = 1;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
}

int cli_at_terminal_tests(const CliAtTerminalRootCfg *args)
{
    const char *root = args->root;
    const char *const *supplied = args->supplied;
    unsigned int count = args->count;
    int cover = args->cover;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(tests_lines_begin, AtTerminalTestsCtx, .lines = &s_failed);
    EMBED_CALL(tests_lines_begin, AtTerminalTestsCtx, .lines = &s_covered);
    s_counts_used = 0ull;
    s_done = 0;
    s_good = 0;
    const CliJsonSink sink = {tests_event, NULL};
    unsigned long long run = 0ull;
    if (!EMBED_CALL(cli_test_runs_root.start, CliTestRunsRootCfg, .root = root, .supplied = supplied, .count = count,
                    .parallel = 1, .cover = cover, .sink = sink, .number = &run, .err_msg = err_msg))
    {
        EMBED_CALL(tests_lines_free, AtTerminalTestsCtx, .lines = &s_failed);
        EMBED_CALL(tests_lines_free, AtTerminalTestsCtx, .lines = &s_covered);
        return 0;
    }
    while (!s_done)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
    VerbaLinea *const into = s_good ? args->out : err_msg;
    EMBED_CALL(tests_lines_write, AtTerminalTestsCtx, .lines = &s_failed, .into = into);
    EMBED_CALL(tests_lines_write, AtTerminalTestsCtx, .lines = &s_covered, .into = into);
    mmgr_line_put_n(into, s_counts, s_counts_used);
    const int good = s_good;
    return good;
}

// a cell of the most what is carried writes to
static char *tests_transferred_alloc(void)
{
    return g_cli_cells->temporary_buf_alloc(CLI_AT_TERMINAL_TRANSFERRED, &g_cli_command_line_cells);
}

// what `line` wrote read into `json`, a tree of its size, and the cell it wrote in given back: the tree's root, or
// CLI_JSON_NONE with the tree containing nothing where the text or its tree could not be parsed
EMBED_INLINE unsigned int tests_tree_get(const AtTerminalTestsCtx *args)
{
    CliJson *json = args->json_out;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int root =
        mmgr_line_ok(args->line)
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                         .json = json, .text = args->line->out, .size = args->line->at, .nodes_more = 0u,
                         .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_command_line_cells)
            : CLI_JSON_NONE;
    g_cli_cells->persistent_buf_release(args->cell, &g_cli_command_line_cells);
    return root;
}

int cli_at_terminal_alloc(const CliAtTerminalCountCfg *args)
{
    const char *home = args->home;
    const char *from = args->from;
    unsigned int count = args->count;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static CliTransferred s_there;
    if (!EMBED_CALL(cli_machine_transfer_from.read, CliMachineTransferFromCfg, .from = from, .there_out = &s_there,
                    .err_msg = err_msg))
    {
        return 0;
    }
    char *const differences_text = tests_transferred_alloc();
    VerbaLinea differences = mmgr_line_over(differences_text, CLI_AT_TERMINAL_TRANSFERRED);
    EMBED_CALL(cli_machine_transfer_home.differences, CliMachineTransferHomeCfg, .home = home, .there = &s_there,
               .out = &differences);
    CliJson tree;
    CliJson *const json = &tree;
    const unsigned int all = EMBED_CALL(tests_tree_get, AtTerminalTestsCtx, .json_out = json, .cell = differences_text,
                                        .line = &differences);
    // a place and a key are each a node of the differences: as many as their nodes, and settings.json, contain them
    const unsigned int capacity =
        ((json->nodes + 1u) < CLI_AT_TERMINAL_ALLOCATED) ? (json->nodes + 1u) : CLI_AT_TERMINAL_ALLOCATED;
    const char **const chosen_positions =
        g_cli_cells->temporary_buf_alloc(2ull * capacity * sizeof(const char *), &g_cli_command_line_cells);
    if (chosen_positions == NULL)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
        mmgr_line_put(err_msg, "the cell the places and settings taken are listed in could not be taken");
        return 0;
    }
    const char **const chosen_keys = chosen_positions + capacity;
    unsigned int positions = 0u;
    unsigned int keys = 0u;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = all, .kind = CFG_JSON_ARRAY)
                                ? json->node[all].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        const char *const place =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "place"),
                       .length_pointer = NULL);
        const char *const how =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "how"),
                       .length_pointer = NULL);
        int required = (count == 0u);
        for (unsigned int at = 0u; !required && (place != NULL) && (at < count); at += 1u)
        {
            required = (cellul.eq(&(CatenaFinitaCfg){.src = args->chosen[at], .other = place, .cap = CLI_CELLS_POOL}));
        }
        if ((place != NULL) && (how != NULL) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "here", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL})) &&
            required && (positions < capacity))
        {
            chosen_positions[positions] = place;
            positions += 1u;
        }
        const unsigned int settings = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = one, .name = "settings");
        for (unsigned int setting = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                               .json_read = json, .node = settings, .kind = CFG_JSON_ARRAY)
                                        ? json->node[settings].first
                                        : CLI_JSON_NONE;
             setting != CLI_JSON_NONE; setting = json->node[setting].next)
        {
            const char *const key =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = setting, .name = "key"),
                           .length_pointer = NULL);
            int named = (count == 0u);
            for (unsigned int at = 0u; !named && (key != NULL) && (at < count); at += 1u)
            {
                named = (cellul.eq(&(CatenaFinitaCfg){.src = args->chosen[at], .other = key, .cap = CLI_CELLS_POOL}));
            }
            if ((key != NULL) && named && (keys < capacity))
            {
                chosen_keys[keys] = key;
                keys += 1u;
            }
        }
    }
    if ((keys > 0u) && (positions < capacity))
    {
        chosen_positions[positions] = "settings.json";
        positions += 1u;
    }
    int done = 1;
    if (positions == 0u)
    {
        mmgr_line_put(out, from);
        mmgr_line_put(out, " contains the settings this machine has.");
    }
    else
    {
        char *const allocating_text = tests_transferred_alloc();
        VerbaLinea allocating = mmgr_line_over(allocating_text, CLI_AT_TERMINAL_TRANSFERRED);
        done = EMBED_CALL(cli_machine_transfer_home.take, CliMachineTransferHomeCfg, .home = home, .from = from,
                          .there = &s_there, .positions = chosen_positions, .count = positions, .keys = chosen_keys,
                          .key_count = keys, .out = &allocating, .err_msg = err_msg);
        CliJson consumed_tree;
        const unsigned int consumed = EMBED_CALL(tests_tree_get, AtTerminalTestsCtx, .json_out = &consumed_tree,
                                                 .cell = allocating_text, .line = &allocating);
        if (done)
        {
            const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &consumed_tree, .object = consumed, .name = "files");
            const unsigned int reserved = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                     .json_read = &consumed_tree, .node = files, .kind = CFG_JSON_ARRAY)
                                              ? consumed_tree.node[files].count
                                              : 0u;
            unsigned long long id_size = 0ull;
            const char *const id =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &consumed_tree,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &consumed_tree, .object = consumed, .name = "id"),
                           .length_pointer = &id_size);
            mmgr_line_put(out, "Took ");
            mmgr_line_decimal(out, reserved, 1u);
            mmgr_line_put(out, (reserved == 1u) ? " file from " : " files from ");
            mmgr_line_put(out, from);
            mmgr_line_put(out, "; carry-undo ");
            mmgr_line_put_n(out, (id != NULL) ? id : "", id_size);
            mmgr_line_put(out, " writes them back.");
        }
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &consumed_tree);
    }
    g_cli_cells->persistent_buf_release(chosen_positions, &g_cli_command_line_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
    return done;
}

int cli_at_terminal_undo(const CliAtTerminalCfg *args)
{
    const char *home = args->home;
    const char *id = args->id;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char *const allocating_text = tests_transferred_alloc();
    VerbaLinea allocating = mmgr_line_over(allocating_text, CLI_AT_TERMINAL_TRANSFERRED);
    const int done = EMBED_CALL(cli_machine_transfer_home_out.undo, CliMachineTransferHomeOutCfg, .home = home,
                                .id = id, .out = &allocating, .err_msg = err_msg);
    CliJson tree;
    const unsigned int consumed =
        EMBED_CALL(tests_tree_get, AtTerminalTestsCtx, .json_out = &tree, .cell = allocating_text, .line = &allocating);
    if (done)
    {
        unsigned long long size = 0ull;
        const char *const from =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &tree,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &tree,
                                          .object = consumed, .name = "from"),
                       .length_pointer = &size);
        mmgr_line_put(out, "The taking from ");
        mmgr_line_put_n(out, (from != NULL) ? from : "", size);
        mmgr_line_put(out, " is undone.");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &tree);
    return done;
}
