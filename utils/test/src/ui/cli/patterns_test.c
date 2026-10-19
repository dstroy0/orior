// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// patterns_test.c: the patterns' check: a pattern hides what it names and what is in it, a pattern that keeps hides
// every other file, a set names its files and takes some out, git picks the files the patterns leave, and the pathspecs
// name what the patterns name
#include "../../../../../ui/cli/src/text/patterns.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliPatterns s_patterns;
static CliWords s_words;

// the patterns of `count` lines read into s_patterns
static const CliPatterns *patterns_read(const char *const *lines, unsigned int count)
{
    cli_patterns_read(lines, count, &s_patterns);
    return &s_patterns;
}

// a pattern hides what it names and what is in it
static void patterns_hiding(void)
{
    static const char *const s_lines[7] = {"target", "*.log", "docs/gen/", "# a note", "", "src/**/*.min.js", "?.tmp"};
    const CliPatterns *const p = patterns_read(s_lines, 7u);
    cli_check(cli_patterns_hides(p, "target", 1) && cli_patterns_hides(p, "src/ui/target/debug/orior.exe", 0),
              "a name hides itself and what is in it, wherever it stands");
    cli_check(cli_patterns_hides(p, "run.log", 0) && cli_patterns_hides(p, "deep/in/it/run.log", 0) &&
                  !cli_patterns_hides(p, "run.log.txt", 0),
              "a glob hides the names it fits");
    cli_check(cli_patterns_hides(p, "docs/gen", 1) && cli_patterns_hides(p, "docs/gen/page.html", 0) &&
                  !cli_patterns_hides(p, "docs/gen", 0) && !cli_patterns_hides(p, "other/docs/gen/page.html", 0),
              "a folder's path hides that folder alone");
    cli_check(cli_patterns_hides(p, "src/a.min.js", 0) && cli_patterns_hides(p, "src/ui/vendor/a.min.js", 0) &&
                  !cli_patterns_hides(p, "lib/a.min.js", 0),
              "`**` takes any depth");
    cli_check(cli_patterns_hides(p, "x.tmp", 0) && !cli_patterns_hides(p, "xy.tmp", 0), "`?` takes one letter");
    cli_check(!cli_patterns_hides(p, "note", 0), "and a note hides nothing");
}

// a pattern that keeps hides every other file
static void patterns_keeping(void)
{
    static const char *const s_lines[3] = {"!src/**", "!*.md", "src/**/*.o"};
    const CliPatterns *const p = patterns_read(s_lines, 3u);
    cli_check(cli_patterns_keeps_only(p), "a pattern of `!` keeps only what it names");
    cli_check(!cli_patterns_hides(p, "src/ui/main.rs", 0) && !cli_patterns_hides(p, "README.md", 0) &&
                  !cli_patterns_hides(p, "docs/guide.md", 0),
              "what it names stays");
    cli_check(cli_patterns_hides(p, "Cargo.toml", 0) && cli_patterns_hides(p, "src/ui/main.o", 0),
              "every other file is hidden, and what a pattern hides among the kept");
    cli_check(!cli_patterns_hides(p, "docs", 1), "a folder is hidden only where a pattern names it");
    cli_check(cli_patterns_keeps_folder(p, "src/ui") && !cli_patterns_keeps_folder(p, "docs"),
              "a kept folder is known");
    static const char *const s_src[1] = {"!src"};
    cli_check(!cli_patterns_hides(patterns_read(s_src, 1u), "src/a.rs", 0), "a kept folder keeps what is in it");
}

// git run in `root` with `count` words, its lines sorted and joined by `|` into `out`, which holds 1024
static void patterns_git(const char *root, const char *const *words, unsigned int count, char *out)
{
    static char s_lines[64][CLI_PATH_LONGEST];
    CliRan *const ran = cli_ran_taken();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned int lines = 0u;
    if (cli_ran(words, root, NULL, 0u, ran, &said))
    {
        for (unsigned long long at = 0ull; (at < ran->out_size) && (lines < 64u);)
        {
            const char *const end =
                memor.chr(&(MemoriaCfg){.src = ran->out + at, .val = '\n', .bytes = ran->out_size - at});
            // a line ends inside the output, at or past its start
            const unsigned long long length =
                (end != NULL) ? (unsigned long long)(end - (ran->out + at)) : ran->out_size - at;
            CliLine line = {s_lines[lines], CLI_PATH_LONGEST, 0ull};
            cli_line_counted(&line, ran->out + at, length);
            (void)cli_line_closed(&line);
            lines += (length > 0ull) ? 1u : 0u;
            at += length + 1ull;
        }
    }
    cli_ran_given(ran);
    for (unsigned int at = 1u; at < lines; at += 1u)
    {
        char held[CLI_PATH_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = held,
                          .src = s_lines[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_lines[at], .cap = CLI_CELLS_POOL}) + 1u});
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(s_lines[place - 1u], held) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = s_lines[place],
                .src = s_lines[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_lines[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_lines[place],
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliLine joined = {out, 1024u, 0ull};
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        cli_line_text(&joined, (at > 0u) ? "|" : "");
        cli_line_text(&joined, s_lines[at]);
    }
    (void)cli_line_closed(&joined);
}

// git picks the files the patterns leave
static void patterns_with_git(void)
{
    static const char *const s_init[4] = {"git", "init", "-q", NULL};
    static const char *const s_sets[4][3] = {{"target", "*.log", "docs/gen/"},
                                             {"!src/**", "src/gen", NULL},
                                             {"!*.md", NULL, NULL},
                                             {"**/*.min.js", "!lib/**", NULL}};
    static const unsigned int s_counts[4] = {3u, 2u, 1u, 2u};
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("patterns_git", root);
    static const char *const s_all[9] = {"src/a.rs",      "src/gen/b.rs",       "docs/gen/x.md",
                                         "docs/guide.md", "target/debug/o.exe", "run.log",
                                         "deep/run.log",  "README.md",          "lib/vendor/a.min.js"};
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        cli_full(root, s_all[at], path);
        char *const slash =
            cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
        if (slash != NULL)
        {
            slash[0] = '\0';
            (void)query_folder_made(path);
            slash[0] = '/';
        }
        (void)query_file_written(path, "x", 1ull);
    }
    char listed[1024];
    patterns_git(root, s_init, 3u, listed);
    int same = 1;
    for (unsigned int set = 0u; set < 4u; set += 1u)
    {
        const CliPatterns *const p = patterns_read(s_sets[set], s_counts[set]);
        s_words.count = 0u;
        s_words.used = 0u;
        s_words.full = 0;
        static const char *const s_head[5] = {"git", "ls-files", "--cached", "--others", "--"};
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            cli_words_put(&s_words, s_head[at]);
        }
        cli_patterns_pathspecs(p, &s_words);
        s_words.word[s_words.count] = NULL;
        patterns_git(root, s_words.word, s_words.count, listed);
        char left[1024];
        CliLine line = {left, sizeof(left), 0ull};
        static const char *const s_sorted[9] = {"README.md",     "deep/run.log",        "docs/gen/x.md",
                                                "docs/guide.md", "lib/vendor/a.min.js", "run.log",
                                                "src/a.rs",      "src/gen/b.rs",        "target/debug/o.exe"};
        for (unsigned int at = 0u; at < 9u; at += 1u)
        {
            if (!cli_patterns_hides(p, s_sorted[at], 0))
            {
                cli_line_text(&line, (line.at > 0ull) ? "|" : "");
                cli_line_text(&line, s_sorted[at]);
            }
        }
        (void)cli_line_closed(&line);
        same &= cellul.eq(&(CatenaFinitaCfg){.src = listed, .other = left, .cap = CLI_CELLS_POOL});
    }
    cli_check(same, "git picks the files the patterns leave");
}

int main(void)
{
    patterns_hiding();
    patterns_keeping();
    static const char *const s_set[4] = {"src/**", "!src/gen/**", "", "# a note"};
    cli_patterns_read_set(s_set, 4u, &s_patterns);
    cli_check(!cli_patterns_hides(&s_patterns, "src/a.rs", 0) && cli_patterns_hides(&s_patterns, "src/gen/b.rs", 0) &&
                  cli_patterns_hides(&s_patterns, "docs/x.md", 0),
              "a set names its files and takes some out");
    cli_patterns_read_set(NULL, 0u, &s_patterns);
    cli_check(cli_patterns_empty(&s_patterns), "and a set of none is empty");
    patterns_with_git();
#if defined(_WIN32)
#define PATTERNS_CASE ",icase"
#else
#define PATTERNS_CASE ""
#endif
    s_words.count = 0u;
    s_words.used = 0u;
    cli_patterns_pathspecs(patterns_read(NULL, 0u), &s_words);
    cli_check((s_words.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[0], .other = ".", .cap = CLI_CELLS_POOL})),
              "no patterns name the whole tree");
    static const char *const s_specs[3] = {"target", "!src/**", "gen/"};
    static const char *const s_wanted[5] = {":(glob" PATTERNS_CASE ")src/**", ":(glob" PATTERNS_CASE ")src/**/**",
                                            ":(exclude,glob" PATTERNS_CASE ")**/target",
                                            ":(exclude,glob" PATTERNS_CASE ")**/target/**",
                                            ":(exclude,glob" PATTERNS_CASE ")**/gen/**"};
    s_words.count = 0u;
    s_words.used = 0u;
    cli_patterns_pathspecs(patterns_read(s_specs, 3u), &s_words);
    int named = s_words.count == 5u;
    for (unsigned int at = 0u; named && (at < 5u); at += 1u)
    {
        named = cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[at], .other = s_wanted[at], .cap = CLI_CELLS_POOL});
    }
    cli_check(named, "the pathspecs name what the patterns name");
    return cli_checked();
}
