// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench.c: the command line's common paths timed over a corpus, the tree's own C: each file read, read as UTF-8,
// counted in UTF-16 units, decoded to letters, searched for a needle, sifted for it, its facts set in the tree's
// index, parsed, its declarations read, its lines colored with the C grammar; the corpus's paths sorted; the tree's
// JSON read and written again; and the tree searched. Each is run `repeat` times and the best time recorded; a row is
// written for each as `name<tab>bytes<tab>microseconds<tab>megabytes a second`, the corpus and the repeats first.
// The corpus is a file of paths, one a line; the tree's top is the second argument
#include "../../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../../../ui/cli/src/editor/files.h"
#include "../../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../../ui/cli/src/inspect/inspect.h"
#include "../../../../../../ui/cli/src/inspect/inspect_tree.h"
#include "../../../../../../ui/cli/src/text/json_tree.h"
#include "../../../../../../ui/cli/src/text/regexp.h"
#include "../../../../../../ui/cli/src/text/sift.h"
#include "../../../../../../ui/cli/src/text/text.h"
#include "bench.h"

#include <stdio.h>
#include <stdlib.h>

#define BENCH_FILES 4096u
#define BENCH_NEEDLE "persistent_buf_release"
#define BENCH_RUNS (1u << 20u)
#define BENCH_LINES (1u << 17u)
#define BENCH_FOLDS (1u << 16u)
#define BENCH_RANGES (1u << 18u)

static char (*s_paths)[CLI_PATH_MAX_LEN];
static unsigned int s_count;
static unsigned int s_repeat = 5u;
static char s_root[CLI_PATH_MAX_LEN];

static BenchFile *s_corpus;

// what a pass over the corpus does with one loaded file, its bytes counted into the pass's sum
typedef void (*BenchEach)(const BenchEachCfg *args);

void bench_row(const char *name, BenchMeasure measure)
{
    const double seconds = (double)measure.min / 1000000.0;
    const double rate = (seconds > 0.0) ? ((double)measure.bytes / (1024.0 * 1024.0)) / seconds : 0.0;
    printf("%s\t%llu\t%llu\t%.1f\n", name, measure.bytes, measure.min, rate);
    fflush(stdout);
}

// `each` run over every loaded file `s_repeat` times: the bytes and the least time
static BenchMeasure bench_pass(BenchEach each, void *context)
{
    BenchMeasure measure = {0ull, ~0ull};
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int at = 0u; at < s_count; at += 1u)
        {
            if (s_corpus[at].bytes != NULL)
            {
                EMBED_CALL(each, BenchEachCfg, .path = s_paths[at], .bytes = s_corpus[at].bytes, .size = s_corpus[at].size, .context = context);
                bytes += s_corpus[at].size;
            }
        }
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = bytes;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    return measure;
}

static void each_read(const BenchEachCfg *args)
{
    const char *path = args->path;
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    (void)bytes;
    (void)size;
    unsigned long long read = 0ull;
    const unsigned char *const again = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &read, .owner = g_checks_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)again, (g_checks_owner));
}

static void each_utf8(const BenchEachCfg *args)
{
    const char *path = args->path;
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    (void)bytes;
    (void)size;
    unsigned long long read = 0ull;
    const unsigned char *const again = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = path, .size_pointer = &read, .owner = g_checks_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)again, (g_checks_owner));
}

static void each_units(const BenchEachCfg *args)
{
    unsigned long long size = args->size;
    void *context = args->context;
    *(unsigned long long *)context += EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = (const char *)args->bytes, .size = size);
}

static void each_letters(const BenchEachCfg *args)
{
    unsigned long long size = args->size;
    unsigned int *const letters = args->context;
    (void)EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = (const char *)args->bytes, .size = size, .letters = letters, .capacity_u64 = size);
}

// the file read as UTF-8 a byte that starts no letter at a time read as U+FFFD, into the line `context` from its start
static void each_lossy(const BenchEachCfg *args)
{
    VerbaLinea *const line = args->context;
    line->at = 0ull;
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = line, .text_bytes = args->bytes, .length = args->size);
}

static void each_find(const BenchEachCfg *args)
{
    unsigned long long size = args->size;
    void *context = args->context;
    unsigned long long at = 0ull;
    *(unsigned long long *)context +=
        (unsigned long long)EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = (const char *)args->bytes, .length_value = size, .required = BENCH_NEEDLE, .size = sizeof(BENCH_NEEDLE) - 1u, .at_pointer = &at);
}

static void sift_found(const CliEachAtCfg *args)
{
    void *context = args->context;
    *(unsigned long long *)context += 1ull;
}

static void each_sift(const BenchEachCfg *args)
{
    void *context = args->context;
    (void)EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = args->bytes, .length = args->size, .needle = (const unsigned char *)BENCH_NEEDLE, .needle_length = sizeof(BENCH_NEEDLE) - 1u, .fold = 0, .found = sift_found, .context = context);
}

// the corpus joined into one text of `whole` bytes and sifted for the needle twice over, `s_repeat` times each, the
// least time of each recorded: once with each window counted for its plan, and once planned from the text's census,
// counted once as a text stored in lines stores it. The rows "sift whole" and "sift stored"
static void bench_sift_file(unsigned long long whole)
{
    unsigned char *const text = g_cli_guard->persistent_buf_alloc(whole + 1ull, g_checks_owner);
    if (text == NULL)
    {
        return;
    }
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        if (s_corpus[at].bytes != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = text + used, .src = s_corpus[at].bytes, .bytes = s_corpus[at].size});
            used += s_corpus[at].size;
        }
    }
    CliHistogram histogram;
    memor.set(&(MemoriaCfg){.dst = &histogram, .val = 0, .bytes = sizeof(histogram)});
    EMBED_CALL(cli_sift_length_histogram.histogram_count, CliSiftLengthHistogramCfg, .histogram = &histogram, .bytes = text, .length = used);
    BenchMeasure counted = {used, ~0ull};
    BenchMeasure stored = {used, ~0ull};
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long sum = 0ull;
        unsigned long long began = cli_clock_microseconds();
        (void)EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = text, .length = used, .needle = (const unsigned char *)BENCH_NEEDLE, .needle_length = sizeof(BENCH_NEEDLE) - 1u, .fold = 0, .found = sift_found, .context = &sum);
        unsigned long long consumed = cli_clock_microseconds() - began;
        counted.min = (consumed < counted.min) ? consumed : counted.min;
        began = cli_clock_microseconds();
        (void)EMBED_CALL(cli_sift_length_fold.find_all_with, CliSiftLengthFoldCfg, .histogram_read = &histogram, .text = text, .length = used, .needle = (const unsigned char *)BENCH_NEEDLE, .needle_length = sizeof(BENCH_NEEDLE) - 1u, .fold = 0, .found = sift_found, .context = &sum);
        consumed = cli_clock_microseconds() - began;
        stored.min = (consumed < stored.min) ? consumed : stored.min;
    }
    g_cli_guard->persistent_buf_release(text, g_checks_owner);
    bench_row("sift whole", counted);
    bench_row("sift stored", stored);
}

static void each_facts(const BenchEachCfg *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = path);
    if (language != NULL)
    {
        (void)EMBED_CALL(cli_inspect_tree_path_language.set, CliInspectTreePathLanguageCfg, .path = path, .language = language, .text = (const char *)args->bytes, .size = size);
    }
}

static CliInspectRun *s_runs;
static unsigned int *s_line_first;
static unsigned int *s_line_runs;
static unsigned int *s_fold_first;
static unsigned int *s_fold_last;
static CliInspectSpan *s_ranges;

static void each_parse(const BenchEachCfg *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    void *context = args->context;
    const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = path);
    if (language == NULL)
    {
        return;
    }
    CliParsed parsed;
    EMBED_CALL(cli_inspect_parsed.parsed_begin, CliInspectParsedCfg, .parsed = &parsed, .runs = s_runs, .runs_max = BENCH_RUNS, .line_first = s_line_first, .line_runs = s_line_runs, .lines_max = BENCH_LINES, .fold_first = s_fold_first, .fold_last = s_fold_last, .folds_max = BENCH_FOLDS, .ranges = s_ranges, .ranges_max = BENCH_RANGES);
    *(unsigned long long *)context +=
        (unsigned long long)EMBED_CALL(cli_inspect_language_text.parse, CliInspectLanguageTextCfg, .language = language, .text = (const char *)args->bytes, .size = size, .parsed = &parsed);
}

static void declared(const CliDeclaredCfg *args)
{
    void *context = args->context;
    *(unsigned long long *)context += 1ull;
}

static void each_symbols(const BenchEachCfg *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    void *context = args->context;
    EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = path, .text = (const char *)args->bytes, .size = size, .each_declared = declared, .context = context);
}

// A file's lines for the colorer: a copy of its text with each newline made a terminator, and a pointer to each line
typedef struct
{
    const char *key;
    char *copy;
    const char **lines;
    VerbaLinea out;
    unsigned long long colored;
} BenchColoring;

static void each_color(const BenchEachCfg *args)
{
    const unsigned char *bytes = args->bytes;
    BenchColoring *const coloring = args->context;
    memor.cpy(&(MemoriaCfg){.dst = coloring->copy, .src = bytes, .bytes = args->size});
    coloring->copy[args->size] = '\0';
    unsigned int count = 0u;
    coloring->lines[0] = coloring->copy;
    for (unsigned long long at = 0ull; (at < args->size) && (count + 1u < BENCH_LINES); at += 1ull)
    {
        if (coloring->copy[at] == '\n')
        {
            coloring->copy[at] = '\0';
            count += 1u;
            coloring->lines[count] = coloring->copy + at + 1ull;
        }
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    coloring->out.at = 0ull;
    coloring->colored += (unsigned long long)EMBED_CALL(cli_highlight_key_err_msg.color, CliHighlightKeyErrMsgCfg, .key = coloring->key, .state = "", .lines = coloring->lines, .count = count + 1u, .out = &coloring->out, .err_msg = &err_msg);
}

BenchMeasure bench_colored(const char *key, const BenchFile *files, unsigned int count, unsigned int repeat,
                           unsigned long long largest)
{
    (void)files;
    (void)count;
    (void)repeat;
    BenchColoring coloring = {
        key,
        g_cli_guard->persistent_buf_alloc(largest + 1ull, g_checks_owner),
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_LINES * sizeof(const char *), g_checks_owner),
        {NULL, 0ull, 0ull},
        0ull};
    char *const out_cell = g_cli_guard->temporary_buf_alloc(1ull << 22u, g_checks_owner);
    coloring.out = mmgr_line_over(out_cell, 1ull << 22u);
    const BenchMeasure measure = bench_pass(each_color, &coloring);
    g_cli_guard->persistent_buf_release(coloring.out.out, g_checks_owner);
    g_cli_guard->persistent_buf_release(coloring.copy, g_checks_owner);
    g_cli_guard->persistent_buf_release(coloring.lines, g_checks_owner);
    return measure;
}

// the corpus read: each line of `list` a path, loaded whole
static int bench_corpus(const char *list)
{
    unsigned long long size = 0ull;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = list, .size_pointer = &size, .owner = g_checks_owner);
    if (text == NULL)
    {
        fprintf(stderr, "the corpus list %s was not read\n", list);
        return 0;
    }
    s_paths = g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_FILES * CLI_PATH_MAX_LEN, g_checks_owner);
    s_corpus = g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_FILES * sizeof(BenchFile), g_checks_owner);
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; (at <= size) && (s_count < BENCH_FILES); at += 1ull)
    {
        if ((at < size) && (text[at] != '\n'))
        {
            continue;
        }
        unsigned long long end = at;
        end -= ((end > start) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        if ((end > start) && (end - start < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_paths[s_count], .src = text + start, .bytes = end - start});
            s_paths[s_count][end - start] = '\0';
            s_corpus[s_count].bytes = EMBED_CALL(cli_path_internal.file_alloc_utf8, CliPathCtx, .path = s_paths[s_count], .size_pointer = &s_corpus[s_count].size, .owner = g_checks_owner);
            s_count += 1u;
        }
        start = at + 1ull;
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)text, (g_checks_owner));
    return s_count > 0u;
}

// the C grammar of the tree's plugin cached under bench-c: 1, or 0
static int bench_grammar_cached(void)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "ui/plugins/c/plugin.json", .joined = path);
    unsigned long long size = 0ull;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size, .owner = g_checks_owner);
    if (text == NULL)
    {
        return 0;
    }
    CliJson json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = (const char *)text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = g_checks_owner);
    const int cached = (root != CLI_JSON_NONE) &&
                       EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = "bench-c", .json = &json, .def = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = root, .name = "grammar"), .err_msg = &err_msg);
    g_cli_guard->persistent_buf_release(json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)text, (g_checks_owner));
    return cached;
}

// the tree's JSON, the C plugin and the toolchains, read into trees and written again, `s_repeat` times each
// the tree's JSON read into trees and written again, 20 times a file: the read's measure, and the write's into `write`
// the tree's JSON the JSON passes read
static const char *const s_json_files[2] = {"ui/plugins/c/plugin.json", "ui/cli/toolchains.json"};

// the tree's JSON files' tokens counted, as a tree read from one is sized, each file 200 times a repeat, the counts
// summed into `sum`: the measure
static BenchMeasure bench_json_tokens(unsigned long long *sum)
{
    BenchMeasure measure = {0ull, ~0ull};
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        unsigned long long consumed = 0ull;
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = s_json_files[at], .joined = path);
            unsigned long long size = 0ull;
            const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size, .owner = g_checks_owner);
            if (text == NULL)
            {
                continue;
            }
            const unsigned long long began = cli_clock_microseconds();
            for (unsigned int times = 0u; times < 200u; times += 1u)
            {
                *sum += EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = (const char *)text, .size = size);
            }
            consumed += cli_clock_microseconds() - began;
            bytes += 200ull * size;
            g_cli_guard->persistent_buf_release((void *)(uintptr_t)text, (g_checks_owner));
        }
        measure.bytes = bytes;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    return measure;
}

static BenchMeasure bench_json(BenchMeasure *write)
{
    BenchMeasure measure = {0ull, ~0ull};
    *write = measure;
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        unsigned long long consumed = 0ull;
        unsigned long long consumed_write = 0ull;
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = s_json_files[at], .joined = path);
            unsigned long long size = 0ull;
            const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size, .owner = g_checks_owner);
            if (text == NULL)
            {
                continue;
            }
            const unsigned long long out_capacity = size + 1024ull;
            char *const out_cell = g_cli_guard->temporary_buf_alloc(out_capacity, g_checks_owner);
            VerbaLinea out = mmgr_line_over(out_cell, out_capacity);
            char why[CLI_ERR_MSG_MAX_LEN];
            VerbaLinea err_msg = {why, sizeof(why), 0ull};
            for (unsigned int times = 0u; times < 20u; times += 1u)
            {
                CliJson json;
                const unsigned long long began = cli_clock_microseconds();
                const unsigned int root =
                    EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = (const char *)text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = g_checks_owner);
                const unsigned long long read = cli_clock_microseconds();
                out.at = 0ull;
                EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = root, .out = &out, .indent = 0u, .sorted = 0);
                consumed_write += cli_clock_microseconds() - read;
                consumed += read - began;
                g_cli_guard->persistent_buf_release(json.node,
                                                    mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
                EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                           .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
                bytes += size;
            }
            g_cli_guard->persistent_buf_release(out.out, g_checks_owner);
            g_cli_guard->persistent_buf_release((void *)(uintptr_t)text, (g_checks_owner));
        }
        measure.bytes = bytes;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
        write->bytes = bytes;
        write->min = (consumed_write < write->min) ? consumed_write : write->min;
    }
    return measure;
}

// the corpus's paths sorted, a copy each time
static BenchMeasure bench_sorted(void)
{
    BenchMeasure measure = {0ull, ~0ull};
    char (*const copy)[CLI_PATH_MAX_LEN] =
        g_cli_guard->persistent_buf_alloc((unsigned long long)s_count * CLI_PATH_MAX_LEN, g_checks_owner);
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = s_paths, .bytes = (unsigned long long)s_count * CLI_PATH_MAX_LEN});
        const unsigned long long began = cli_clock_microseconds();
        EMBED_CALL(cli_text_paths.paths_sort, CliTextPathsCfg, .paths = copy, .count = s_count);
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = (unsigned long long)s_count * CLI_PATH_MAX_LEN;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    g_cli_guard->persistent_buf_release(copy, g_checks_owner);
    return measure;
}

// a cell of 4 KiB taken and given back `BENCH_CELL_TURNS` times while the corpus's cells stand: the cost of one take
// and one give with that many cells alive
#define BENCH_CELL_TURNS 2000u
static BenchMeasure bench_cells(void)
{
    BenchMeasure measure = {0ull, ~0ull};
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int turn = 0u; turn < BENCH_CELL_TURNS; turn += 1u)
        {
            void *const cell = g_cli_guard->persistent_buf_alloc(4096ull, g_checks_owner);
            g_cli_guard->persistent_buf_release(cell, g_checks_owner);
        }
        const unsigned long long consumed = cli_clock_microseconds() - began;
        measure.bytes = (unsigned long long)BENCH_CELL_TURNS * 4096ull;
        measure.min = (consumed < measure.min) ? consumed : measure.min;
    }
    return measure;
}

int main(int argc, char **argv)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    if (argc < 3)
    {
        fprintf(stderr, "cli_bench <corpus list> <tree> [repeat]\n");
        return 2;
    }
    s_repeat = (argc > 3) ? (unsigned int)strtoul(argv[3], NULL, 10) : s_repeat;
    s_repeat = (s_repeat == 0u) ? 1u : s_repeat;
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = argv[2],
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = argv[2], .cap = CLI_POOL_BYTES}) + 1u});
    if (!bench_corpus(argv[1]))
    {
        return 1;
    }
    unsigned long long whole = 0ull;
    unsigned long long largest = 0ull;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        whole += s_corpus[at].size;
        largest = (s_corpus[at].size > largest) ? s_corpus[at].size : largest;
    }
    printf("corpus\t%llu\t%u\t%u\n", whole, s_count, s_repeat);
    bench_row("cell-read", bench_pass(each_read, NULL));
    bench_row("utf8", bench_pass(each_utf8, NULL));
    unsigned long long sum = 0ull;
    bench_row("units", bench_pass(each_units, &sum));
    unsigned int *const letters =
        g_cli_guard->persistent_buf_alloc((largest + 1ull) * sizeof(unsigned int), g_checks_owner);
    bench_row("letters", bench_pass(each_letters, letters));
    g_cli_guard->persistent_buf_release(letters, g_checks_owner);
    // each byte that starts no letter written as U+FFFD's three bytes at the most
    char *const lossy_cell = g_cli_guard->persistent_buf_alloc((3ull * largest) + 1ull, g_checks_owner);
    VerbaLinea lossy = {lossy_cell, (3ull * largest) + 1ull, 0ull};
    bench_row("lossy", bench_pass(each_lossy, &lossy));
    g_cli_guard->persistent_buf_release(lossy_cell, g_checks_owner);
    sum = 0ull;
    bench_row("find", bench_pass(each_find, &sum));
    sum = 0ull;
    bench_row("sift", bench_pass(each_sift, &sum));
    bench_sift_file(whole);
    bench_row("facts", bench_pass(each_facts, NULL));
    cli_inspect_tree_clear();
    s_runs = g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_RUNS * sizeof(CliInspectRun), g_checks_owner);
    s_line_first =
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_LINES * sizeof(unsigned int), g_checks_owner);
    s_line_runs =
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_LINES * sizeof(unsigned int), g_checks_owner);
    s_fold_first =
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_FOLDS * sizeof(unsigned int), g_checks_owner);
    s_fold_last =
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_FOLDS * sizeof(unsigned int), g_checks_owner);
    s_ranges =
        g_cli_guard->persistent_buf_alloc((unsigned long long)BENCH_RANGES * sizeof(CliInspectSpan), g_checks_owner);
    sum = 0ull;
    bench_row("parse", bench_pass(each_parse, &sum));
    sum = 0ull;
    bench_row("symbols", bench_pass(each_symbols, &sum));
    if (bench_grammar_cached())
    {
        bench_row("color", bench_colored("bench-c", s_corpus, s_count, s_repeat, largest));
    }
    bench_rules(s_root, s_corpus, s_count, s_repeat, largest);
    BenchMeasure written;
    const BenchMeasure read = bench_json(&written);
    bench_row("json-read", read);
    bench_row("json-write", written);
    sum = 0ull;
    bench_row("json-tokens", bench_json_tokens(&sum));
    bench_row("sorted", bench_sorted());
    const BenchCorpus corpus = {s_root, s_paths, s_corpus, s_count, whole, s_repeat};
    bench_row("cells-live", bench_cells());
    bench_row("cell-read-2", bench_files_read_by(&corpus, 2u, 0));
    bench_row("read-1", bench_files_read_by(&corpus, 1u, 1));
    bench_row("read-2", bench_files_read_by(&corpus, 2u, 1));
    bench_row("read-4", bench_files_read_by(&corpus, 4u, 1));
    bench_row("cell-read-4", bench_files_read_by(&corpus, 4u, 0));
    bench_row("cell-read-8", bench_files_read_by(&corpus, 8u, 0));
    bench_row("picked", bench_picked(&corpus));
    bench_row("search", bench_search(&corpus, BENCH_NEEDLE));
    bench_row("search-none", bench_search(&corpus, "zzqqxx_nowhere"));
    return 0;
}
