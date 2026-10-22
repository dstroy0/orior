// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench_rules.c: the C grammar's root rules, each read as a pattern of its own and tried at every letter of every
// held file as the colorer tries it, the letters moved past a match; a row a rule, named by the pattern's start
#include "../../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../../ui/cli/src/text/json_tree.h"
#include "../../../../../../ui/cli/src/text/regexp.h"
#include "bench.h"

#include <stdio.h>

#define BENCH_RULE_NAME 28u

static CliRegexpRoom s_room;

// the pattern tried over `count` kept files, the letters decoded into `letters` each time: the measure
static BenchMeasure rules_tried(const CliRegexp *regexp, const BenchHeld *held, unsigned int count, unsigned int repeat,
                                unsigned int *letters, unsigned long long *matched)
{
    BenchMeasure measure = {0ull, ~0ull};
    CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
    for (unsigned int round = 0u; round < repeat; round += 1u)
    {
        unsigned long long bytes = 0ull;
        unsigned long long hits = 0ull;
        const unsigned long long began = cli_clock_microseconds();
        for (unsigned int file = 0u; file < count; file += 1u)
        {
            if (held[file].bytes == NULL)
            {
                continue;
            }
            const unsigned long long length =
                cli_regexp_letters((const char *)held[file].bytes, held[file].size, letters, held[file].size);
            for (unsigned long long at = 0ull; at < length;)
            {
                const int found = cli_regexp_match_into(regexp, letters, length, at, groups);
                const unsigned long long span = (found && groups[0].set) ? (groups[0].to - groups[0].from) : 0ull;
                hits += found ? 1ull : 0ull;
                at += (span > 0ull) ? span : 1ull;
            }
            bytes += held[file].size;
        }
        const unsigned long long took = cli_clock_microseconds() - began;
        measure.bytes = bytes;
        measure.least = (took < measure.least) ? took : measure.least;
        *matched = hits;
    }
    return measure;
}

// The grammars cut down from the C grammar's: each a name and its root's rules as JSON, every one with the C grammar's
// word lists and whitespace state
typedef struct
{
    const char *name;
    const char *rules;
} BenchCutDown;

static const BenchCutDown s_cut_down[4] = {
    {"color:whitespace", "[{\"include\":\"@whitespace\"}]"},
    {"color:identifier-plain", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",\"identifier\"]]"},
    {"color:identifier-cases", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\","
                               "\"@words\":\"keyword\",\"@default\":\"identifier\"}}]]"},
    {"color:identifier-number-string",
     "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\",\"@words\":\"keyword\","
     "\"@default\":\"identifier\"}}],[\"(0[xX][\\\\da-fA-F_']+|0[bB][01_']+|(?:\\\\d[\\\\d_']*)?\\\\.?\\\\d[\\\\d_']*"
     "(?:[eE][+-]?\\\\d+)?)([fFuUlLjJzZ]*)\",[\"number\",\"number.suffix\"]],[\"[uUL]?\\\"\",\"string\",\"@string\"]]"},
};

// the corpus colored with each cut-down grammar, kept under bench-cut from JSON written here: the C grammar's words
// and types, the root given, and the C grammar's whitespace and string states
static void rules_cut_down(const CliJson *json, unsigned int grammar, const BenchHeld *held, unsigned int count,
                           unsigned int repeat, unsigned long long largest)
{
    const unsigned int tokenizer = cli_json_member(json, grammar, "tokenizer");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        CliLine made;
        if (!cli_line_growing(&made, 1ull << 16u, "a cut-down grammar"))
        {
            return;
        }
        cli_line_text(&made, "{\"words\":");
        cli_json_write(json, cli_json_member(json, grammar, "words"), &made, 0u, 0);
        cli_line_text(&made, ",\"types\":");
        cli_json_write(json, cli_json_member(json, grammar, "types"), &made, 0u, 0);
        cli_line_text(&made, ",\"tokenizer\":{\"root\":");
        cli_line_text(&made, s_cut_down[at].rules);
        cli_line_text(&made, ",\"whitespace\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "whitespace"), &made, 0u, 0);
        cli_line_text(&made, ",\"comment\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "comment"), &made, 0u, 0);
        cli_line_text(&made, ",\"string\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "string"), &made, 0u, 0);
        cli_line_text(&made, "}}");
        CliJson cut;
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        const unsigned int top = cli_json_taken(&cut, made.out, made.at, 0u, 0ull, &said);
        if ((top != CLI_JSON_NONE) && cli_highlight_keep("bench-cut", &cut, top, &said))
        {
            bench_row(s_cut_down[at].name, bench_colored("bench-cut", held, count, repeat, largest));
        }
        else
        {
            printf("%s\tnot kept: %.*s\n", s_cut_down[at].name, (int)said.at, why);
        }
        cli_json_given(&cut);
        cli_cell_given(made.out);
    }
}

void bench_rules(const char *root, const BenchHeld *held, unsigned int count, unsigned int repeat,
                 unsigned long long largest)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, "ui/plugins/c/plugin.json", path);
    unsigned long long size = 0ull;
    const unsigned char *const text = cli_file_held(path, &size);
    if (text == NULL)
    {
        return;
    }
    CliJson json;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int top = cli_json_taken(&json, (const char *)text, size, 0u, 0ull, &said);
    const unsigned int grammar = cli_json_member(&json, top, "grammar");
    const unsigned int rules = cli_json_member(&json, cli_json_member(&json, grammar, "tokenizer"), "root");
    const int fold = cli_json_is(&json, cli_json_member(&json, grammar, "ignoreCase"), CFG_JSON_TRUE);
    unsigned int *const letters = cli_cell_taken((largest + 1ull) * sizeof(unsigned int), "a file's letters");
    for (unsigned int index = 0u; cli_json_item(&json, rules, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int rule = cli_json_item(&json, rules, index);
        unsigned long long length = 0ull;
        const char *const source = cli_json_string(&json, cli_json_item(&json, rule, 0u), &length);
        if (source == NULL)
        {
            continue;
        }
        // the pattern as a terminated text of its own, the tree's being bounded by its length alone
        char pattern[1024];
        CliLine pattern_line = {pattern, sizeof(pattern), 0ull};
        cli_line_counted(&pattern_line, source, length);
        if (cli_line_closed(&pattern_line) == 0ull)
        {
            continue;
        }
        CliRegexp regexp;
        cli_regexp_roomed(&regexp, &s_room);
        said.at = 0ull;
        if (!cli_regexp_read(&regexp, pattern, fold ? "i" : "", &said))
        {
            continue;
        }
        unsigned long long matched = 0ull;
        const BenchMeasure measure = rules_tried(&regexp, held, count, repeat, letters, &matched);
        char name[BENCH_RULE_NAME + 24u];
        CliLine name_line = {name, sizeof(name), 0ull};
        cli_line_text(&name_line, "rule:");
        cli_line_text_most(&name_line, pattern, BENCH_RULE_NAME);
        cli_line_text(&name_line, " hits=");
        cli_line_decimal(&name_line, matched, 1u);
        (void)cli_line_closed(&name_line);
        bench_row(name, measure);
    }
    cli_cell_given(letters);
    rules_cut_down(&json, grammar, held, count, repeat, largest);
    cli_json_given(&json);
    cli_file_let_go(text, size);
}
