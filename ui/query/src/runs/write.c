// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// write.c: the writers, each over the part's files and the rulesets and reaching no part: `klq_write` writes
// `Lstar.klq` and each given language's `.klm` beside the rulesets; `krs_write` writes `sass.krs` and `sm_86.krs`
// beside `sm_86.khw` from the forms the part answered for; `gsm_write` writes `sm_86.gsm`, each relation the part
// answered as the cases it answered it on. And the query: the bridge written over two languages' rulesets, then its
// pairs put to the part where one is chosen
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

// the most rulesets one bridge is written over
#define QUERY_RULESETS_MOST 16u

// `name` under the bridge's folder into `path`
static void query_table_named(const QueryContext *context, const char *name, char *path)
{
    char table[QUERY_PATH_LONGEST];
    query_at(context, QUERY_TABLE, table);
    CliLine line = {path, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, table);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// `Lstar.klq` and each given language's `.klm` written beside the rulesets (`klq_write`), teed as `klq_write.console`:
// the writer's exit code
int query_bridge(QueryContext *context, const char *const *rulesets, unsigned int count, CliLine *said)
{
    static const char *const s_default[3] = {"cu.krs", "sass.krs", "openqasm2_0.krs"};
    const char *const *given = (count == 0u) ? s_default : rulesets;
    const unsigned int given_count = (count == 0u) ? 3u : ((count < QUERY_RULESETS_MOST) ? count : QUERY_RULESETS_MOST);
    static char s_klq_write[QUERY_PATH_LONGEST];
    static QueryRun s_run;
    if (!query_context_tool(context, "klq_write", s_klq_write, said) ||
        !query_context_started(context, &s_run, "build/engine/klq/qry", said))
    {
        return -1;
    }
    static char s_read[QUERY_RULESETS_MOST + 1u][QUERY_PATH_LONGEST];
    static const char *s_reads[QUERY_RULESETS_MOST + 1u];
    for (unsigned int at = 0u; at < given_count; at += 1u)
    {
        query_table_named(context, given[at], s_read[at]);
        s_reads[at] = s_read[at];
    }
    query_table_named(context, "Lstar.klq", s_read[given_count]);
    s_reads[given_count] = s_read[given_count];
    char table[QUERY_PATH_LONGEST];
    query_at(context, QUERY_TABLE, table);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_words_add(&s_words, s_klq_write);
    query_words_add(&s_words, table);
    for (unsigned int at = 0u; at < given_count; at += 1u)
    {
        query_words_add(&s_words, given[at]);
    }
    long long code = -1ll;
    const int whole = query_run_read(&s_run, s_reads, given_count + 1u, said) &&
                      query_context_teed(context, &s_run, "klq_write", s_words.word, &code, said);
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// `sass.krs` and `sm_86.krs` written beside `sm_86.khw` from the forms the part answered for (`krs_write`), teed as
// `krs_write.console`; or `sm_86.gsm`, each relation the part answered as the cases it answered it on (`gsm_write`).
// The writer's exit code
static int query_part_written(QueryContext *context, int krs, CliLine *said)
{
    static char s_tool[QUERY_PATH_LONGEST];
    static char s_writer[QUERY_PATH_LONGEST];
    static QueryFiles s_files;
    static QueryRun s_run;
    query_files_of(context->root, "sm_86", &s_files);
    if (!query_context_tool(context, krs ? "krs_write" : "gsm_write", s_tool, said) ||
        !query_context_tool(context, "khw_machine_write", s_writer, said) ||
        !query_context_started(context, &s_run, krs ? "build/engine/krs/qry" : "build/engine/gsm/qry", said))
    {
        return -1;
    }
    char part_krs[QUERY_PATH_LONGEST];
    char sass_krs[QUERY_PATH_LONGEST];
    char cu_krs[QUERY_PATH_LONGEST];
    query_table_named(context, "sm_86.krs", part_krs);
    query_table_named(context, "sass.krs", sass_krs);
    query_table_named(context, "cu.krs", cu_krs);
    const char *const krs_read[8] = {s_files.machine, s_files.costs,  part_krs,          sass_krs,
                                     cu_krs,          s_files.layout, s_files.mnemonics, NULL};
    const char *const gsm_read[4] = {s_files.machine, s_files.layout, s_files.mnemonics, NULL};
    const char *const krs_words[9] = {s_tool,   "sass", "sm_86", s_files.machine, s_files.layout, s_files.mnemonics,
                                      s_writer, cu_krs, NULL};
    const char *const gsm_words[7] = {s_tool,   "sm_86", s_files.machine, s_files.layout, s_files.mnemonics,
                                      s_writer, NULL};
    long long code = -1ll;
    const int whole =
        query_run_read(&s_run, krs ? krs_read : gsm_read, krs ? 7u : 3u, said) &&
        query_context_teed(context, &s_run, krs ? "krs_write" : "gsm_write", krs ? krs_words : gsm_words, &code, said);
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

int query_write_words(const char *const *words, unsigned int count, CliLine *said)
{
    if ((count >= 1u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "bridge", .cap = CLI_CELLS_POOL})))
    {
        return QUERY_WRITE_BRIDGE;
    }
    if ((count == 1u) && ((cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "krs", .cap = CLI_CELLS_POOL})) ||
                          (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "gsm", .cap = CLI_CELLS_POOL}))))
    {
        return (words[0][0] == 'k') ? QUERY_WRITE_KRS : QUERY_WRITE_GSM;
    }
    cli_line_text(said, "write takes bridge [<ruleset>...], krs or gsm");
    return -1;
}

int query_write_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    const int writer = query_write_words(words, count, said);
    if (writer == QUERY_WRITE_BRIDGE)
    {
        return query_bridge(context, &words[1], count - 1u, said);
    }
    return (writer < 0) ? -1 : query_part_written(context, writer == QUERY_WRITE_KRS, said);
}

// a name the table holds a whole set of files of: the language's `.klm` and `.krs` where `language`, else the part's
// `.khw`, `.kqr` and `.ksc`, each missing one said after `missing`'s others
static void query_missing_files(const QueryContext *context, const char *name, int language, CliLine *missing)
{
    static const char *const s_language[2] = {"klm", "krs"};
    static const char *const s_part[3] = {"khw", "kqr", "ksc"};
    const char *const *const kinds = language ? s_language : s_part;
    for (unsigned int at = 0u; at < (language ? 2u : 3u); at += 1u)
    {
        char file[QUERY_PATH_LONGEST];
        char path[QUERY_PATH_LONGEST];
        CliLine line = {file, sizeof(file), 0ull};
        cli_line_text(&line, name);
        cli_line_character(&line, '.');
        cli_line_text(&line, kinds[at]);
        cli_line_closed(&line);
        query_table_named(context, file, path);
        if (!query_is_file(path))
        {
            cli_line_text(missing, (missing->at == 0ull) ? "" : ", ");
            cli_line_text(missing, QUERY_TABLE "/");
            cli_line_text(missing, file);
        }
    }
}

// The query the words name, each held to the files that make it whole: `--<from> --<to> [--<part>] [--dry]`, or
// `--from <from> --to <to> [--part <part>] [--dry]` as the window's job says it. A name the table does not hold whole
// is answered with each file missing. 1, or 0 with why said
int query_query_words(const QueryContext *context, const char *const *words, unsigned int count, const char **names,
                      unsigned int *named, int *dry, CliLine *said)
{
    static const char *const s_usage = "query takes --<from> --<to> [--<part>] [--dry]: two languages, the part to run "
                                       "on, and dry to launch nothing";
    const char *plain[QUERY_WORDS_MOST];
    unsigned int plains = 0u;
    *dry = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--dry", .cap = CLI_CELLS_POOL}))
        {
            *dry = 1;
        }
        else if (plains < QUERY_WORDS_MOST)
        {
            plain[plains] = words[at];
            plains += 1u;
        }
    }
    *named = 0u;
    const int flagged =
        (plains != 0u) && ((cellul.eq(&(CatenaFinitaCfg){.src = plain[0], .other = "--from", .cap = CLI_CELLS_POOL})) ||
                           (cellul.eq(&(CatenaFinitaCfg){.src = plain[0], .other = "--to", .cap = CLI_CELLS_POOL})) ||
                           (cellul.eq(&(CatenaFinitaCfg){.src = plain[0], .other = "--part", .cap = CLI_CELLS_POOL})));
    if (flagged)
    {
        const char *from = NULL;
        const char *to = NULL;
        const char *part = NULL;
        for (unsigned int at = 0u; at < plains; at += 2u)
        {
            if ((at + 1u) >= plains)
            {
                cli_line_text(said, s_usage);
                return 0;
            }
            const char **const slot =
                (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--from", .cap = CLI_CELLS_POOL}))   ? &from
                : (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--to", .cap = CLI_CELLS_POOL}))   ? &to
                : (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--part", .cap = CLI_CELLS_POOL})) ? &part
                                                                                                              : NULL;
            if (slot == NULL)
            {
                cli_line_text(said, s_usage);
                return 0;
            }
            *slot = plain[at + 1u];
        }
        const char *const given[3] = {from, to, part};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (given[at] != NULL)
            {
                names[*named] = given[at];
                *named += 1u;
            }
        }
    }
    else
    {
        for (unsigned int at = 0u; at < plains; at += 1u)
        {
            if ((!cellul.starts(&(CatenaFinitaCfg){.src = plain[at], .other = "--", .cap = CLI_CELLS_POOL})) ||
                (*named >= 3u))
            {
                cli_line_text(said, s_usage);
                return 0;
            }
            names[*named] = plain[at] + 2;
            *named += 1u;
        }
    }
    for (unsigned int at = 0u; at < *named; at += 1u)
    {
        if ((names[at][0] == '\0') || (names[at][cli_text_upto(names[at], "/\\.")] != '\0'))
        {
            cli_line_text(said, s_usage);
            return 0;
        }
    }
    if ((*named < 2u) || (*named > 3u) || (*dry && (*named != 3u)))
    {
        cli_line_text(said, s_usage);
        return 0;
    }
    char missing_text[QUERY_SAID_LONGEST];
    CliLine missing = {missing_text, sizeof(missing_text), 0ull};
    for (unsigned int at = 0u; at < *named; at += 1u)
    {
        query_missing_files(context, names[at], at < 2u, &missing);
    }
    char klq[QUERY_PATH_LONGEST];
    query_table_named(context, "Lstar.klq", klq);
    if (!query_is_file(klq))
    {
        cli_line_text(&missing, (missing.at == 0ull) ? "" : ", ");
        cli_line_text(&missing, QUERY_TABLE "/Lstar.klq");
    }
    if (missing.at != 0ull)
    {
        cli_line_text(said, "query ");
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            cli_line_text(said, (at == 0u) ? "" : " ");
            cli_line_text(said, words[at]);
        }
        cli_line_text(said, ": not in the tree: ");
        cli_line_text_most(said, missing_text, missing.at);
        return 0;
    }
    return 1;
}

int query_query_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    const char *names[3] = {NULL, NULL, NULL};
    unsigned int named = 0u;
    int dry = 0;
    if (!query_query_words(context, words, count, names, &named, &dry, said))
    {
        return -1;
    }
    char from[QUERY_PATH_LONGEST];
    char to[QUERY_PATH_LONGEST];
    CliLine from_line = {from, sizeof(from), 0ull};
    cli_line_text(&from_line, names[0]);
    cli_line_text(&from_line, ".krs");
    cli_line_closed(&from_line);
    CliLine to_line = {to, sizeof(to), 0ull};
    cli_line_text(&to_line, names[1]);
    cli_line_text(&to_line, ".krs");
    cli_line_closed(&to_line);
    const char *const rulesets[2] = {from, to};
    const int code = query_bridge(context, rulesets, 2u, said);
    if ((code != 0) || (named < 3u))
    {
        return code;
    }
    return query_pairs_pair(context, names[2], names[1], dry, NULL, NULL, said);
}
