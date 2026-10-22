// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_pairs_test.c: the pairs' modes: the part's name for the vendor's reader; the modes and their targets; the dry
// carrier's flags come first as the carrier reads them; the kernel is handed before the frame and the pairs; and the
// words are read
#include "../../../../../ui/query/src/learn/pairs_internal.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static QueryWords s_words;

// the words joined by spaces from `from` into `out`, which holds `room`
static const char *pairs_joined(unsigned int from, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    for (unsigned int at = from; at < s_words.count; at += 1u)
    {
        cli_line_text(&line, (at > from) ? " " : "");
        cli_line_text(&line, s_words.word[at]);
    }
    (void)cli_line_closed(&line);
    return out;
}

// the targets of `mode` over part sm_86 in the tree T, with `words` given, joined by spaces into `out`
static const char *pairs_targets(const char *mode, const char *const *words, unsigned int count, char *out,
                                 unsigned long long room)
{
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = "T";
    QueryPairsMode read;
    memor.set(&(MemoriaCfg){.dst = &read, .val = 0, .bytes = sizeof(read)});
    read.mode = mode;
    read.part = "sm_86";
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        read.words[at] = words[at];
    }
    read.word_count = count;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_targets(&context, &read, &s_words);
    return pairs_joined(0u, out, room);
}

// the part's name for the vendor's reader; and the modes and their targets, forward slashed throughout
static void pairs_targets_of(void)
{
    char name[64];
    query_part_read("sm_86", name, sizeof(name));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "SM86", .cap = CLI_CELLS_POOL}),
              "the part's name for the vendor's reader");
    query_part_read("sm_120a", name, sizeof(name));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "SM120A", .cap = CLI_CELLS_POOL}),
              "and one with a letter after it");
    char out[1024];
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = pairs_targets("queue", NULL, 0u, out, sizeof(out)),
                                           .other = "T/build/measuring_stick/measuring_stick.tsv",
                                           .cap = CLI_CELLS_POOL}),
              "the queue's stick");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = pairs_targets("pair", NULL, 0u, out, sizeof(out)),
                                           .other = "T/" QUERY_TABLE "/Lstar.klq T/" QUERY_TABLE "/sm_86.khw",
                                           .cap = CLI_CELLS_POOL}),
              "the pairs' bridge and machine file");
    cli_check(cellul.chr(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL, .byte = '\\'}) == NULL,
              "forward slashed throughout");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = pairs_targets("stall", NULL, 0u, out, sizeof(out)),
                                           .other = "T/" QUERY_TABLE "/sm_86.khw",
                                           .cap = CLI_CELLS_POOL}),
              "the stall's machine file");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = pairs_targets("cost", NULL, 0u, out, sizeof(out)),
                                           .other = "T/" QUERY_TABLE "/sm_86.kdm T/" QUERY_TABLE "/sm_86.khw",
                                           .cap = CLI_CELLS_POOL}),
              "the cost's");
    static const char *const s_curve[2] = {"1006", "1007"};
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = pairs_targets("curve", s_curve, 2u, out, sizeof(out)),
                                           .other = "1006 1007",
                                           .cap = CLI_CELLS_POOL}),
              "the curve's own words");
    cli_check((pairs_targets("register", NULL, 0u, out, sizeof(out))[0] == '\0') &&
                  (pairs_targets("frame", NULL, 0u, out, sizeof(out))[0] == '\0') &&
                  (pairs_targets("fixed", NULL, 0u, out, sizeof(out))[0] == '\0'),
              "and none for the modes that take none");
}

// the dry carrier's flags come first as the carrier reads them; and the kernel is handed before the frame and the pairs
static void pairs_carrier(void)
{
    char dry[1024];
    char live[1024];
    char rest[1024];
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_pairs_carry_words(&s_words, "C", "X", "N", "M", "L", "sm_86", 1);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = pairs_joined(0u, dry, sizeof(dry)),
                  .other = "C --dry --full --held held.txt --lessons overseer.txt M L --pop C --dry --full --lessons "
                           "overseer.txt M L --cross X N SM86 airlock.md held.txt",
                  .cap = CLI_CELLS_POOL}),
              "the dry carrier's flags come first as the carrier reads them");
    (void)pairs_joined(9u, rest, sizeof(rest));
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_pairs_carry_words(&s_words, "C", "X", "N", "M", "L", "sm_86", 0);
    cli_check(cli_text_same(pairs_joined(0u, live, sizeof(live)), "C --held held.txt --lessons overseer.txt M L ", 44u),
              "a live carrier without them");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = pairs_joined(7u, live, sizeof(live)), .other = rest, .cap = CLI_CELLS_POOL}),
              "the rest alike");
    static QueryFiles s_files;
    query_files_of("T", "sm_86", &s_files);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_kernel_words(&s_words, "sm_86", &s_files);
    const char *const *const word = s_words.word;
    cli_check(
        (s_words.count == 6u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = word[0], .other = "sm_86", .cap = CLI_CELLS_POOL})) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = word[1], .cap = CLI_CELLS_POOL, .other = "/table/sm_86.khw", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = word[2],
                                            .cap = CLI_CELLS_POOL,
                                            .other = "/nvidia/elf64_nvidia.tsv",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = word[3],
                                            .cap = CLI_CELLS_POOL,
                                            .other = "/nvidia/mnemonic_nvidia.tsv",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = word[4], .other = "kernel.bin", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = word[5], .other = "kernel", .cap = CLI_CELLS_POOL})),
        "the kernel's words");
    cli_check(query_kernel_handed("frame") && query_kernel_handed("fixed") && query_kernel_handed("pair") &&
                  !query_kernel_handed("register"),
              "the kernel is handed before the frame and the pairs");
}

// `words` read as a pairs' mode into `mode`: 1, or 0 where they are refused
static int pairs_read(const char *const *words, unsigned int count, QueryPairsMode *mode)
{
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    return query_pairs_read(words, count, mode, &said);
}

// the words are read
static void pairs_words(void)
{
    QueryPairsMode mode;
    static const char *const s_pair[6] = {"pair", "--part", "sm_86", "--dry", "--seed", "2"};
    cli_check(pairs_read(s_pair, 6u, &mode) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.mode, .other = "pair", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.part, .other = "sm_86", .cap = CLI_CELLS_POOL})) &&
                  mode.dry && (mode.seed != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.seed, .other = "2", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.to, .other = "sass", .cap = CLI_CELLS_POOL})) &&
                  (mode.word_count == 0u),
              "a pair's words");
    static const char *const s_curve[4] = {"curve", "1006", "--trace", "build/x"};
    cli_check(pairs_read(s_curve, 4u, &mode) && (mode.word_count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.words[0], .other = "1006", .cap = CLI_CELLS_POOL})) &&
                  (mode.trace != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.trace, .other = "build/x", .cap = CLI_CELLS_POOL})),
              "a curve's, with its trace");
    static const char *const s_stick[3] = {"stick", "sass.krs", "c.krs"};
    cli_check(pairs_read(s_stick, 3u, &mode) && (mode.word_count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.words[0], .other = "sass.krs", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.words[1], .other = "c.krs", .cap = CLI_CELLS_POOL})),
              "a stick's rulesets");
    static const char *const s_frame[3] = {"frame", "--part", "sm_86"};
    cli_check(pairs_read(s_frame, 3u, &mode) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.mode, .other = "frame", .cap = CLI_CELLS_POOL})) &&
                  (mode.word_count == 0u),
              "a frame's");
    static const char *const s_fixed[2] = {"fixed", "--dry"};
    cli_check(pairs_read(s_fixed, 2u, &mode) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode.mode, .other = "fixed", .cap = CLI_CELLS_POOL})) &&
                  mode.dry && (mode.word_count == 0u),
              "a fixed's, dry");
    static const char *const s_ptx[3] = {"pair", "--to", "ptx"};
    cli_check(!pairs_read(s_ptx, 3u, &mode), "orior_klq_identity writes forms in sass alone");
    static const char *const s_fly[1] = {"fly"};
    static const char *const s_seed[2] = {"pair", "--seed"};
    cli_check(!pairs_read(s_fly, 1u, &mode) && !pairs_read(s_seed, 2u, &mode) && !pairs_read(NULL, 0u, &mode),
              "and nothing else");
}

int main(void)
{
    pairs_targets_of();
    pairs_carrier();
    pairs_words();
    return cli_checked();
}
