// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_learn_test.c: the learn's passes and the measuring stick: the files are handed with forward slashes; the
// carrier's words; the steps' words are read; the part's files are beside the bridge; paths given to the vendor are
// native; the stick is compiled again only where its text changed; and the stick's words name what is read
#include "../../../../../ui/query/src/learn/learn_internal.h"
#include "cli_check.h"

// the words joined by spaces into `out`, which holds `room`
static const char *learn_joined(const QueryWords *words, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    for (unsigned int at = 0u; at < words->count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? " " : "");
        cli_line_text(&line, words->word[at]);
    }
    (void)cli_line_closed(&line);
    return out;
}

// the files are handed with forward slashes; and the part's files are beside the bridge
static void learn_files(void)
{
    static QueryFiles s_files;
    query_files_of("D:/top/tree", "sm_86", &s_files);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_files.costs, .cap = CLI_CELLS_POOL});
    cli_check((cellul.chr(&(CatenaFinitaCfg){.src = s_files.costs, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL) &&
                  (length >= 16ull) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_files.costs + length - 16u, .other = "/table/sm_86.ksc", .cap = CLI_CELLS_POOL})) &&
                  (cellul.chr(&(CatenaFinitaCfg){.src = s_files.layout, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL),
              "the files are handed with forward slashes");
    query_files_of("T", "sm_86", &s_files);
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){
            .src = s_files.machine, .other = "T/" QUERY_TABLE "/sm_86.khw", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_files.record, .other = "T/" QUERY_TABLE "/sm_86.kqr", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_files.fields,
                                          .other = "T/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_nvidia.tsv",
                                          .cap = CLI_CELLS_POOL})),
        "the part's files are beside the bridge");
}

// the carrier's words
static void learn_carrier(void)
{
    static QueryWords s_words;
    char joined[1024];
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_carrier_words(&s_words, "C", "X", "N", "M", "L", 1);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = learn_joined(&s_words, joined, sizeof(joined)),
            .other = "C --held held.txt --lessons overseer.txt M L --pop C --dry --full --lessons overseer.txt M L "
                     "--cross X N SM86 airlock.md held.txt",
            .cap = CLI_CELLS_POOL}),
        "the carrier's words, the lessons with them");
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_carrier_words(&s_words, "C", "X", "N", "M", "L", 0);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = learn_joined(&s_words, joined, sizeof(joined)),
                  .other = "C --held held.txt M L --pop C --dry --full M L --cross X N SM86 airlock.md held.txt",
                  .cap = CLI_CELLS_POOL}),
              "and without them");
}

// the step `text`'s words read: 1, or 0 where it is refused
static int learn_step(const char *const *words, unsigned int count, QueryLearnStep *step)
{
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    memor.set(&(MemoriaCfg){.dst = step, .val = 0, .bytes = sizeof(*step)});
    return query_learn_step_read(words, count, step, &said);
}

// whether `a` and `b` are both NULL or the same text
static int learn_same(const char *a, const char *b)
{
    return ((a == NULL) && (b == NULL)) ||
           ((a != NULL) && (b != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = a, .other = b, .cap = CLI_CELLS_POOL})));
}

// the steps' words are read
static void learn_steps(void)
{
    QueryLearnStep step;
    static const char *const s_turns[3] = {"turns", "--base", "a.cu"};
    cli_check(learn_step(s_turns, 1u, &step) && (step.kind == QUERY_LEARN_PASS) && learn_same(step.word, "turns") &&
                  (step.base == NULL),
              "a pass");
    cli_check(learn_step(s_turns, 3u, &step) && (step.kind == QUERY_LEARN_PASS) && learn_same(step.base, "a.cu"),
              "a pass with its base");
    static const char *const s_costs[1] = {"costs"};
    cli_check(learn_step(s_costs, 1u, &step) && (step.kind == QUERY_LEARN_PASS) && learn_same(step.word, "costs"),
              "the costs pass");
    static const char *const s_seed[2] = {"seed", "listing.sass"};
    cli_check(learn_step(s_seed, 2u, &step) && (step.kind == QUERY_LEARN_SEED) && learn_same(step.word, "listing.sass"),
              "a seeded discover");
    static const char *const s_rounds[2] = {"rounds", "3"};
    cli_check(learn_step(s_rounds, 2u, &step) && (step.kind == QUERY_LEARN_ROUNDS) && (step.rounds == 3u),
              "rounds through the airlock");
    static const char *const s_dry[2] = {"dry", "2"};
    cli_check(learn_step(s_dry, 1u, &step) && (step.kind == QUERY_LEARN_DRY) && (step.rounds == 1u), "dry, one round");
    cli_check(learn_step(s_dry, 2u, &step) && (step.kind == QUERY_LEARN_DRY) && (step.rounds == 2u), "dry, two");
    static const char *const s_clock[3] = {"clock", "0", "4000"};
    cli_check(learn_step(s_clock, 1u, &step) && (step.kind == QUERY_LEARN_CLOCK) &&
                  learn_same(step.word, QUERY_CLOCK_FROM) && learn_same(step.base, QUERY_CLOCK_WORDS),
              "the clock over the span where none is given");
    cli_check(learn_step(s_clock, 3u, &step) && (step.kind == QUERY_LEARN_CLOCK) && learn_same(step.word, "0") &&
                  learn_same(step.base, "4000"),
              "and over the one given");
    static const char *const s_enumerate[2] = {"enumerate", "loop_back_if"};
    cli_check(learn_step(s_enumerate, 2u, &step) && (step.kind == QUERY_LEARN_ENUMERATE) &&
                  learn_same(step.word, "loop_back_if"),
              "one slot's questions");
    static const char *const s_many[2] = {"rounds", "many"};
    static const char *const s_fly[1] = {"fly"};
    cli_check(!learn_step(s_enumerate, 1u, &step) && !learn_step(s_seed, 1u, &step) && !learn_step(s_many, 2u, &step) &&
                  !learn_step(s_fly, 1u, &step) && !learn_step(NULL, 0u, &step),
              "and nothing else");
}

// paths given to the vendor are native; the stick is compiled again only where its text changed; and the stick's
// words name what is read
static void learn_stick(const char *root)
{
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = root;
    static QueryStickPaths s_paths;
    query_stick_paths(&context, &s_paths);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_paths.cu, .cap = CLI_CELLS_POOL});
    cli_check((length >= 18ull) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_paths.cu + length - 18u, .other = "measuring_stick.cu", .cap = CLI_CELLS_POOL})),
              "the stick's text by its name");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_paths.cu, .cap = CLI_CELLS_POOL, .other = "\\\\?\\", .other_cap = CLI_CELLS_POOL}) == NULL) &&
            (!cellul.starts(&(CatenaFinitaCfg){.src = s_paths.cu, .other = "/c/", .cap = CLI_CELLS_POOL})) &&
            (cellul.chr(&(CatenaFinitaCfg){.src = s_paths.cu, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL),
        "no cygwin path, and forward slashes, which the tree's programs parse");
#if defined(_WIN32)
    cli_check(s_paths.cu[1] == ':', "a Windows path with its drive");
#endif
    char out[QUERY_PATH_LONGEST];
    char cu[QUERY_PATH_LONGEST];
    char fresh[QUERY_PATH_LONGEST];
    char cubin[QUERY_PATH_LONGEST];
    cli_full(root, "build/query_test/stick_text", out);
    (void)query_removed(out);
    (void)query_folder_made(out);
    cli_full(out, "measuring_stick.cu", cu);
    cli_full(out, "measuring_stick.cu.new", fresh);
    cli_full(out, "measuring_stick_nvcc.cubin", cubin);
    (void)query_file_written(cu, "a", 1ull);
    (void)query_file_written(fresh, "a", 1ull);
    (void)query_file_written(cubin, "", 0ull);
    cli_check(!query_stick_changed(cu, fresh, cubin), "the same text is not compiled again");
    (void)query_file_written(fresh, "b", 1ull);
    cli_check(query_stick_changed(cu, fresh, cubin), "a changed text is");
    (void)query_file_written(fresh, "a", 1ull);
    (void)query_removed(cubin);
    cli_check(query_stick_changed(cu, fresh, cubin), "no container means nvcc runs");
    (void)query_removed(out);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    static const char *const s_vendor[1] = {"vendor"};
    static const char *const s_engine[2] = {"engine", "more"};
    static const char *const s_read[3] = {"read", "a.sass", "b.tsv"};
    static const char *const s_ruler[1] = {"ruler"};
    cli_check((query_stick_words(NULL, 0u, &said) == QUERY_STICK_VENDOR) &&
                  (query_stick_words(s_vendor, 1u, &said) == QUERY_STICK_VENDOR),
              "the vendor's reading");
    cli_check(query_stick_words(s_engine, 1u, &said) == QUERY_STICK_ENGINE, "the engine's");
    cli_check(query_stick_words(s_read, 3u, &said) == QUERY_STICK_READ, "the read, with its words");
    cli_check((query_stick_words(s_engine, 2u, &said) < 0) && (query_stick_words(s_ruler, 1u, &said) < 0),
              "and nothing else");
}

int main(void)
{
    learn_files();
    learn_carrier();
    learn_steps();
    char found[CLI_PATH_LONGEST];
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and the stick's paths are not read");
        return cli_checked();
    }
    char root[QUERY_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = root, .src = found, .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    learn_stick(root);
    return cli_checked();
}
