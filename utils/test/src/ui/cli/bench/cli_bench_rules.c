// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_bench_rules.c: the C grammar's root rules, each read as a pattern of its own and tried at every letter of every
// loaded file as the colorer tries it, the letters moved past a match; a row a rule, named by the pattern's start
#include "../../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../../ui/cli/src/text/json_tree.h"
#include "../../../../../../ui/cli/src/text/regexp.h"
#include "bench.h"

#include <stdio.h>

#define BENCH_RULE_NAME 28u

static CliRegexpRoom s_room;

// the pattern tried over `count` loaded files, the letters decoded into `letters` each time: the measure
static BenchMeasure rules_tried(const CliRegexp *regexp, const BenchFile *files, unsigned int count,
                                unsigned int repeat, unsigned int *letters, unsigned long long *matched)
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
            if (files[file].bytes == NULL)
            {
                continue;
            }
            const unsigned long long length =
                cli_regexp_letters((const char *)files[file].bytes, files[file].size, letters, files[file].size);
            for (unsigned long long at = 0ull; at < length;)
            {
                const int found = cli_regexp_match_into(regexp, letters, length, at, groups);
                const unsigned long long span = (found && groups[0].set) ? (groups[0].to - groups[0].from) : 0ull;
                hits += found ? 1ull : 0ull;
                at += (span > 0ull) ? span : 1ull;
            }
            bytes += files[file].size;
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

static const BenchCutDown s_cut_down[5] = {
    {"color:none", "[]"},
    {"color:whitespace", "[{\"include\":\"@whitespace\"}]"},
    {"color:identifier-plain", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",\"identifier\"]]"},
    {"color:identifier-cases", "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\","
                               "\"@words\":\"keyword\",\"@default\":\"identifier\"}}]]"},
    {"color:identifier-number-string",
     "[{\"include\":\"@whitespace\"},[\"[A-Za-z_]\\\\w*\",{\"cases\":{\"@types\":\"type\",\"@words\":\"keyword\","
     "\"@default\":\"identifier\"}}],[\"(0[xX][\\\\da-fA-F_']+|0[bB][01_']+|(?:\\\\d[\\\\d_']*)?\\\\.?\\\\d[\\\\d_']*"
     "(?:[eE][+-]?\\\\d+)?)([fFuUlLjJzZ]*)\",[\"number\",\"number.suffix\"]],[\"[uUL]?\\\"\",\"string\",\"@string\"]]"},
};

// the corpus colored with each cut-down grammar, cached under bench-cut from JSON written here: the C grammar's words
// and types, the root given, and the C grammar's whitespace and string states
static void rules_cut_down(const CliJson *json, unsigned int grammar, const BenchFile *files, unsigned int count,
                           unsigned int repeat, unsigned long long largest)
{
    const unsigned int tokenizer = cli_json_member(json, grammar, "tokenizer");
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        VerbaLinea made;
        if (!cli_growing_line_made(&made, 1ull << 16u, &g_checks_cells))
        {
            return;
        }
        mmgr_line_put(&made, "{\"words\":");
        cli_json_write(json, cli_json_member(json, grammar, "words"), &made, 0u, 0);
        mmgr_line_put(&made, ",\"types\":");
        cli_json_write(json, cli_json_member(json, grammar, "types"), &made, 0u, 0);
        mmgr_line_put(&made, ",\"tokenizer\":{\"root\":");
        mmgr_line_put(&made, s_cut_down[at].rules);
        mmgr_line_put(&made, ",\"whitespace\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "whitespace"), &made, 0u, 0);
        mmgr_line_put(&made, ",\"comment\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "comment"), &made, 0u, 0);
        mmgr_line_put(&made, ",\"string\":");
        cli_json_write(json, cli_json_member(json, tokenizer, "string"), &made, 0u, 0);
        mmgr_line_put(&made, "}}");
        CliJson cut;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        const unsigned int top = cli_json_taken(&cut, made.out, made.at, 0u, 0ull, &err_msg, &g_checks_cells);
        if ((top != CLI_JSON_NONE) && cli_highlight_store("bench-cut", &cut, top, &err_msg))
        {
            bench_row(s_cut_down[at].name, bench_colored("bench-cut", files, count, repeat, largest));
        }
        else
        {
            printf("%s\tnot cached: %.*s\n", s_cut_down[at].name, (int)err_msg.at, why);
        }
        cli_json_given(&cut);
        cli_cell_release(made.out, &g_checks_cells);
    }
}

void bench_rules(const char *root, const BenchFile *files, unsigned int count, unsigned int repeat,
                 unsigned long long largest)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(root, "ui/plugins/c/plugin.json", path);
    unsigned long long size = 0ull;
    const unsigned char *const text = cli_file_read(path, &size);
    if (text == NULL)
    {
        return;
    }
    CliJson json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int top = cli_json_taken(&json, (const char *)text, size, 0u, 0ull, &err_msg, &g_checks_cells);
    const unsigned int grammar = cli_json_member(&json, top, "grammar");
    const unsigned int rules = cli_json_member(&json, cli_json_member(&json, grammar, "tokenizer"), "root");
    const int fold = cli_json_is(&json, cli_json_member(&json, grammar, "ignoreCase"), CFG_JSON_TRUE);
    unsigned int *const letters =
        g_cli_cells->persistent_buf_alloc((largest + 1ull) * sizeof(unsigned int), &g_checks_cells);
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
        VerbaLinea pattern_line = {pattern, sizeof(pattern), 0ull};
        mmgr_line_put_n(&pattern_line, source, length);
        if (mmgr_line_finish(&pattern_line) == 0ull)
        {
            continue;
        }
        CliRegexp regexp;
        cli_regexp_roomed(&regexp, &s_room);
        err_msg.at = 0ull;
        if (!cli_regexp_read(&regexp, pattern, fold ? "i" : "", &err_msg))
        {
            continue;
        }
        unsigned long long matched = 0ull;
        const BenchMeasure measure = rules_tried(&regexp, files, count, repeat, letters, &matched);
        char name[BENCH_RULE_NAME + 24u];
        VerbaLinea name_line = {name, sizeof(name), 0ull};
        mmgr_line_put(&name_line, "rule:");
        mmgr_line_put_most(&name_line, pattern, BENCH_RULE_NAME);
        mmgr_line_put(&name_line, " hits=");
        mmgr_line_decimal(&name_line, matched, 1u);
        (void)mmgr_line_finish(&name_line);
        bench_row(name, measure);
    }
    cli_cell_release(letters, &g_checks_cells);
    rules_cut_down(&json, grammar, files, count, repeat, largest);
    cli_json_given(&json);
    cli_file_let_go(text, size);
}
