// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench.c: the command line's common paths timed over a corpus, the tree's own C: each file read, read as UTF-8,
// counted in UTF-16 units, decoded to letters, searched for a needle, sifted for it, its facts set in the tree's
// index, parsed, its declarations read, its lines colored with the C grammar; the corpus's paths sorted; the tree's
// JSON read and written again; and the tree searched. Each is run `repeat` times and the best time kept; a row is
// written for each as `name<tab>bytes<tab>microseconds<tab>megabytes a second`, the corpus and the repeats first.
// The corpus is a file of paths, one a line; the tree's top is the second argument
#include "../../../../../../ui/cli/src/code/symbols.h"
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
#define BENCH_NEEDLE "cli_cell_given"
#define BENCH_RUNS (1u << 20u)
#define BENCH_LINES (1u << 17u)
#define BENCH_FOLDS (1u << 16u)
#define BENCH_RANGES (1u << 18u)

static char (*s_paths)[CLI_PATH_LONGEST];
static unsigned int s_count;
static unsigned int s_repeat = 5u;
static char s_root[CLI_PATH_LONGEST];

static BenchHeld *s_held;

// what a pass over the corpus does with one kept file, its bytes counted into the pass's sum
typedef void (*BenchEach)(const char *path, const unsigned char *bytes, unsigned long long size, void *context);

void bench_row(const char *name, BenchMeasure measure)
{
    const double seconds = (double)measure.least / 1000000.0;
    const double rate = (seconds > 0.0) ? ((double)measure.bytes / (1024.0 * 1024.0)) / seconds : 0.0;
    printf("%s\t%llu\t%llu\t%.1f\n", name, measure.bytes, measure.least, rate);
    fflush(stdout);
}

// `each` run over every kept file `s_repeat` times: the bytes and the least time
static BenchMeasure bench_pass(BenchEach each, void *context)
{
    BenchMeasure measure = {0ull, ~0ull};
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int at = 0u; at < s_count; at += 1u)
        {
            if (s_held[at].bytes != NULL)
            {
                each(s_paths[at], s_held[at].bytes, s_held[at].size, context);
                bytes += s_held[at].size;
            }
        }
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = bytes;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    return measure;
}

static void each_held(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)bytes;
    (void)size;
    (void)context;
    unsigned long long read = 0ull;
    const unsigned char *const again = cli_file_held(path, &read);
    cli_file_let_go(again, read);
}

static void each_utf8(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)bytes;
    (void)size;
    (void)context;
    unsigned long long read = 0ull;
    const unsigned char *const again = cli_file_held_utf8(path, &read);
    cli_file_let_go(again, read);
}

static void each_units(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)path;
    *(unsigned long long *)context += cli_text_units((const char *)bytes, size);
}

static void each_letters(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)path;
    unsigned int *const letters = context;
    (void)cli_regexp_letters((const char *)bytes, size, letters, size);
}

static void each_find(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)path;
    unsigned long long at = 0ull;
    *(unsigned long long *)context +=
        (unsigned long long)cli_text_find((const char *)bytes, size, BENCH_NEEDLE, sizeof(BENCH_NEEDLE) - 1u, &at);
}

static void sift_found(void *context, unsigned long long at)
{
    (void)at;
    *(unsigned long long *)context += 1ull;
}

static void each_sift(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)path;
    (void)cli_sift_find_all(bytes, size, (const unsigned char *)BENCH_NEEDLE, sizeof(BENCH_NEEDLE) - 1u, 0, sift_found,
                            context);
}

static void each_facts(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)context;
    const char *const language = cli_inspect_language_of(path);
    if (language != NULL)
    {
        (void)cli_inspect_tree_set(path, language, (const char *)bytes, size);
    }
}

static CliInspectRun *s_runs;
static unsigned int *s_line_first;
static unsigned int *s_line_runs;
static unsigned int *s_fold_first;
static unsigned int *s_fold_last;
static CliInspectSpan *s_ranges;

static void each_parse(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    const char *const language = cli_inspect_language_of(path);
    if (language == NULL)
    {
        return;
    }
    CliParsed parsed;
    cli_parsed_begun(&parsed, s_runs, BENCH_RUNS, s_line_first, s_line_runs, BENCH_LINES, s_fold_first, s_fold_last,
                     BENCH_FOLDS, s_ranges, BENCH_RANGES);
    *(unsigned long long *)context +=
        (unsigned long long)cli_inspect_parse(language, (const char *)bytes, size, &parsed);
}

static void declared(void *context, const char *name, unsigned long long length, const char *kind, unsigned int line)
{
    (void)name;
    (void)length;
    (void)kind;
    (void)line;
    *(unsigned long long *)context += 1ull;
}

static void each_symbols(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    cli_symbols_declared(path, (const char *)bytes, size, declared, context);
}

// A file's lines for the colorer: a copy of its text with each newline made a terminator, and a pointer to each line
typedef struct
{
    const char *key;
    char *copy;
    const char **lines;
    CliLine out;
    unsigned long long colored;
} BenchColoring;

static void each_color(const char *path, const unsigned char *bytes, unsigned long long size, void *context)
{
    (void)path;
    BenchColoring *const coloring = context;
    memor.cpy(&(MemoriaCfg){.dst = coloring->copy, .src = bytes, .bytes = size});
    coloring->copy[size] = '\0';
    unsigned int count = 0u;
    coloring->lines[0] = coloring->copy;
    for (unsigned long long at = 0ull; (at < size) && (count + 1u < BENCH_LINES); at += 1ull)
    {
        if (coloring->copy[at] == '\n')
        {
            coloring->copy[at] = '\0';
            count += 1u;
            coloring->lines[count] = coloring->copy + at + 1ull;
        }
    }
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    coloring->out.at = 0ull;
    coloring->colored +=
        (unsigned long long)cli_highlight_color(coloring->key, "", coloring->lines, count + 1u, &coloring->out, &said);
}

BenchMeasure bench_colored(const char *key, const BenchHeld *held, unsigned int count, unsigned int repeat,
                           unsigned long long largest)
{
    (void)held;
    (void)count;
    (void)repeat;
    BenchColoring coloring = {key,
                              cli_cell_taken(largest + 1ull, "a file's lines copied"),
                              cli_cell_taken((unsigned long long)BENCH_LINES * sizeof(const char *), "line starts"),
                              {NULL, 0ull, 0ull},
                              0ull};
    (void)cli_line_growing(&coloring.out, 1ull << 22u, "the colored runs");
    const BenchMeasure measure = bench_pass(each_color, &coloring);
    cli_cell_given(coloring.out.out);
    cli_cell_given(coloring.copy);
    cli_cell_given(coloring.lines);
    return measure;
}

// the corpus read: each line of `list` a path, kept whole
static int bench_corpus(const char *list)
{
    unsigned long long size = 0ull;
    const unsigned char *const text = cli_file_held(list, &size);
    if (text == NULL)
    {
        fprintf(stderr, "the corpus list %s was not read\n", list);
        return 0;
    }
    s_paths = cli_cell_taken((unsigned long long)BENCH_FILES * CLI_PATH_LONGEST, "the corpus's paths");
    s_held = cli_cell_taken((unsigned long long)BENCH_FILES * sizeof(BenchHeld), "the corpus held");
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; (at <= size) && (s_count < BENCH_FILES); at += 1ull)
    {
        if ((at < size) && (text[at] != '\n'))
        {
            continue;
        }
        unsigned long long end = at;
        end -= ((end > start) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        if ((end > start) && (end - start < CLI_PATH_LONGEST))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_paths[s_count], .src = text + start, .bytes = end - start});
            s_paths[s_count][end - start] = '\0';
            s_held[s_count].bytes = cli_file_held_utf8(s_paths[s_count], &s_held[s_count].size);
            s_count += 1u;
        }
        start = at + 1ull;
    }
    cli_file_let_go(text, size);
    return s_count > 0u;
}

// the C grammar of the tree's plugin kept under bench-c: 1, or 0
static int bench_grammar_kept(void)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, "ui/plugins/c/plugin.json", path);
    unsigned long long size = 0ull;
    const unsigned char *const text = cli_file_held(path, &size);
    if (text == NULL)
    {
        return 0;
    }
    CliJson json;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, (const char *)text, size, 0u, 0ull, &said);
    const int kept =
        (root != CLI_JSON_NONE) && cli_highlight_keep("bench-c", &json, cli_json_member(&json, root, "grammar"), &said);
    cli_json_given(&json);
    cli_file_let_go(text, size);
    return kept;
}

// the tree's JSON, the C plugin and the toolchains, read into trees and written again, `s_repeat` times each
// the tree's JSON read into trees and written again, 20 times a file: the read's measure, and the write's into `write`
static BenchMeasure bench_json(BenchMeasure *write)
{
    static const char *const s_files[2] = {"ui/plugins/c/plugin.json", "ui/cli/toolchains.json"};
    BenchMeasure measure = {0ull, ~0ull};
    *write = measure;
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        unsigned long long took = 0ull;
        unsigned long long took_write = 0ull;
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            char path[CLI_PATH_LONGEST];
            cli_full(s_root, s_files[at], path);
            unsigned long long size = 0ull;
            const unsigned char *const text = cli_file_held(path, &size);
            if (text == NULL)
            {
                continue;
            }
            CliLine out;
            (void)cli_line_growing(&out, size + 1024ull, "the JSON written again");
            char why[CLI_SAID_LONGEST];
            CliLine said = {why, sizeof(why), 0ull};
            for (unsigned int times = 0u; times < 20u; times += 1u)
            {
                CliJson json;
                const unsigned long long began = cli_clock_microseconds();
                const unsigned int root = cli_json_taken(&json, (const char *)text, size, 0u, 0ull, &said);
                const unsigned long long read = cli_clock_microseconds();
                out.at = 0ull;
                cli_json_write(&json, root, &out, 0u, 0);
                took_write += cli_clock_microseconds() - read;
                took += read - began;
                cli_json_given(&json);
                bytes += size;
            }
            cli_cell_given(out.out);
            cli_file_let_go(text, size);
        }
        measure.bytes = bytes;
        measure.least = (took < measure.least) ? took : measure.least;
        write->bytes = bytes;
        write->least = (took_write < write->least) ? took_write : write->least;
    }
    return measure;
}

// the corpus's paths sorted, a copy each time
static BenchMeasure bench_sorted(void)
{
    BenchMeasure measure = {0ull, ~0ull};
    char (*const copy)[CLI_PATH_LONGEST] =
        cli_cell_taken((unsigned long long)s_count * CLI_PATH_LONGEST, "paths sorted");
    for (unsigned int round = 0u; round < s_repeat; round += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = s_paths, .bytes = (unsigned long long)s_count * CLI_PATH_LONGEST});
        const unsigned long long began = cli_clock_microseconds();
        cli_paths_sorted(copy, s_count);
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = (unsigned long long)s_count * CLI_PATH_LONGEST;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    cli_cell_given(copy);
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
            void *const cell = cli_cell_taken(4096ull, "a bench cell");
            cli_cell_given(cell);
        }
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = (unsigned long long)BENCH_CELL_TURNS * 4096ull;
        measure.least = (took < measure.least) ? took : measure.least;
    }
    return measure;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "cli_bench <corpus list> <tree> [repeat]\n");
        return 2;
    }
    s_repeat = (argc > 3) ? (unsigned int)strtoul(argv[3], NULL, 10) : s_repeat;
    s_repeat = (s_repeat == 0u) ? 1u : s_repeat;
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = argv[2],
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = argv[2], .cap = CLI_CELLS_POOL}) + 1u});
    if (!bench_corpus(argv[1]))
    {
        return 1;
    }
    unsigned long long whole = 0ull;
    unsigned long long largest = 0ull;
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        whole += s_held[at].size;
        largest = (s_held[at].size > largest) ? s_held[at].size : largest;
    }
    printf("corpus\t%llu\t%u\t%u\n", whole, s_count, s_repeat);
    bench_row("held", bench_pass(each_held, NULL));
    bench_row("utf8", bench_pass(each_utf8, NULL));
    unsigned long long sum = 0ull;
    bench_row("units", bench_pass(each_units, &sum));
    unsigned int *const letters = cli_cell_taken((largest + 1ull) * sizeof(unsigned int), "a file's letters");
    bench_row("letters", bench_pass(each_letters, letters));
    cli_cell_given(letters);
    sum = 0ull;
    bench_row("find", bench_pass(each_find, &sum));
    sum = 0ull;
    bench_row("sift", bench_pass(each_sift, &sum));
    bench_row("facts", bench_pass(each_facts, NULL));
    cli_inspect_tree_cleared();
    s_runs = cli_cell_taken((unsigned long long)BENCH_RUNS * sizeof(CliInspectRun), "the parse's runs");
    s_line_first = cli_cell_taken((unsigned long long)BENCH_LINES * sizeof(unsigned int), "the parse's lines");
    s_line_runs = cli_cell_taken((unsigned long long)BENCH_LINES * sizeof(unsigned int), "the parse's line runs");
    s_fold_first = cli_cell_taken((unsigned long long)BENCH_FOLDS * sizeof(unsigned int), "the parse's folds");
    s_fold_last = cli_cell_taken((unsigned long long)BENCH_FOLDS * sizeof(unsigned int), "the parse's fold ends");
    s_ranges = cli_cell_taken((unsigned long long)BENCH_RANGES * sizeof(CliInspectSpan), "the parse's ranges");
    sum = 0ull;
    bench_row("parse", bench_pass(each_parse, &sum));
    sum = 0ull;
    bench_row("symbols", bench_pass(each_symbols, &sum));
    if (bench_grammar_kept())
    {
        bench_row("color", bench_colored("bench-c", s_held, s_count, s_repeat, largest));
    }
    bench_rules(s_root, s_held, s_count, s_repeat, largest);
    BenchMeasure written;
    const BenchMeasure read = bench_json(&written);
    bench_row("json-read", read);
    bench_row("json-write", written);
    bench_row("sorted", bench_sorted());
    const BenchCorpus corpus = {s_root, s_paths, s_held, s_count, whole, s_repeat};
    bench_row("cells-live", bench_cells());
    bench_row("held-2", bench_held_by(&corpus, 2u, 0));
    bench_row("read-1", bench_held_by(&corpus, 1u, 1));
    bench_row("read-2", bench_held_by(&corpus, 2u, 1));
    bench_row("read-4", bench_held_by(&corpus, 4u, 1));
    bench_row("held-4", bench_held_by(&corpus, 4u, 0));
    bench_row("held-8", bench_held_by(&corpus, 8u, 0));
    bench_row("picked", bench_picked(&corpus));
    bench_row("search", bench_search(&corpus, BENCH_NEEDLE));
    bench_row("search-none", bench_search(&corpus, "zzqqxx_nowhere"));
    return 0;
}
