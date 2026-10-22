// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands_list.c: the commands of the query, the one list the command line and the window read, each with its params
// and the words that run it, and the dispatch of a command's name to its loop
#include "../../cli/src/text/text.h"
#include "query_loops.h"

// the most names the bridge's folder holds whole of one set of kinds, and the longest one
#define QUERY_WHOLE_MOST 64u

#define QUERY_WHOLE_LONGEST 64u

// What the walk for the names the folder holds whole keeps: the folder, the kinds, the names found and their count
typedef struct
{
    const char *folder;
    const char *const *kinds;
    unsigned int kind_count;
    char (*names)[QUERY_WHOLE_LONGEST];
    unsigned int count;
} QueryWholeWalk;

static void query_whole_walked(void *context, const char *name, int is_folder)
{
    QueryWholeWalk *const walk = (QueryWholeWalk *)context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const size_t kind = cellul.len(&(CatenaFinitaCfg){.src = walk->kinds[0], .cap = CLI_CELLS_POOL});
    if (is_folder || (walk->count >= QUERY_WHOLE_MOST) || (length <= (kind + 1u)) ||
        (name[length - kind - 1u] != '.') ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - kind, .other = walk->kinds[0], .cap = CLI_CELLS_POOL})) ||
        ((length - kind - 1u) >= QUERY_WHOLE_LONGEST))
    {
        return;
    }
    char stem[QUERY_WHOLE_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = stem, .src = name, .bytes = length - kind - 1u});
    stem[length - kind - 1u] = '\0';
    // every kind of the stem a file of the folder
    for (unsigned int at = 0u; at < walk->kind_count; at += 1u)
    {
        char path[QUERY_PATH_LONGEST];
        CliLine line = {path, sizeof(path), 0ull};
        cli_line_text(&line, walk->folder);
        cli_line_character(&line, '/');
        cli_line_text(&line, stem);
        cli_line_character(&line, '.');
        cli_line_text(&line, walk->kinds[at]);
        if ((cli_line_closed(&line) == 0ull) || !query_is_file(path))
        {
            return;
        }
    }
    unsigned int at = walk->count;
    while ((at > 0u) && (cli_text_order(walk->names[at - 1u], stem) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->names[at], .src = walk->names[at - 1u], .bytes = QUERY_WHOLE_LONGEST});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->names[at], .src = stem, .bytes = QUERY_WHOLE_LONGEST});
    walk->count += 1u;
}

// Every name the table holds a file of each of `kinds` of, sorted, into `names`, its list into `named`: their count
unsigned int query_whole(const char *root, const char *const *kinds, unsigned int kind_count,
                         char (*names)[QUERY_WHOLE_LONGEST], const char **named)
{
    char folder[QUERY_PATH_LONGEST];
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_text(&line, root);
    cli_line_character(&line, '/');
    cli_line_text(&line, QUERY_TABLE);
    QueryWholeWalk walk = {folder, kinds, kind_count, names, 0u};
    if (cli_line_closed(&line) != 0ull)
    {
        query_folder_each(folder, query_whole_walked, &walk);
    }
    for (unsigned int at = 0u; at < walk.count; at += 1u)
    {
        named[at] = names[at];
    }
    return walk.count;
}

static char s_languages[QUERY_WHOLE_MOST][QUERY_WHOLE_LONGEST];
static const char *s_language[QUERY_WHOLE_MOST];
static char s_parts[QUERY_WHOLE_MOST][QUERY_WHOLE_LONGEST];
static const char *s_part[QUERY_WHOLE_MOST];

static const char *const s_leave_choices[] = {"all", "none"};
static const char *const s_dry_choices[] = {"dry"};
static const char *const s_learn_steps[] = {"discover", "seed", "turns",     "registers", "costs",
                                            "rounds",   "dry",  "enumerate", "clock"};
static const char *const s_writers[] = {"bridge", "krs", "gsm"};
static const char *const s_pairs_modes[] = {"frame", "fixed", "pair",  "cost",          "stall", "register",
                                            "past",  "queue", "curve", "text_identity", "stick"};
static const char *const s_stick_what[] = {"vendor", "engine", "read"};

#define QUERY_COUNT(list_) ((unsigned int)(sizeof(list_) / sizeof((list_)[0])))

static QueryParam s_course_params[2];
static const QueryParam s_tables_params[] = {{"architectures", "words", NULL, 0u, 0},
                                             {"leave", "choice", s_leave_choices, 2u, 0}};
static const QueryParam s_decode_params[] = {{"words", "words", NULL, 0u, 0}};
static QueryParam s_query_params[5];
static const QueryParam s_learn_params[] = {{"step", "choice", s_learn_steps, QUERY_COUNT(s_learn_steps), 1},
                                            {"word", "word", NULL, 0u, 0},
                                            {"base", "word", NULL, 0u, 0},
                                            {"leave", "choice", s_leave_choices, 2u, 0}};
static const QueryParam s_cross_params[] = {{"base", "word", NULL, 0u, 0}, {"leave", "choice", s_leave_choices, 2u, 0}};
static const QueryParam s_write_params[] = {{"writer", "choice", s_writers, 3u, 1},
                                            {"rulesets", "words", NULL, 0u, 0},
                                            {"leave", "choice", s_leave_choices, 2u, 0}};
static QueryParam s_pairs_params[9];
static const QueryParam s_stick_params[] = {{"what", "choice", s_stick_what, 3u, 0},
                                            {"words", "words", NULL, 0u, 0},
                                            {"leave", "choice", s_leave_choices, 2u, 0}};

static const QueryWord s_course_words[] = {{QUERY_WORD_LITERAL, "course", NULL, NULL},
                                           {QUERY_WORD_FLAG, "--from", "from", NULL},
                                           {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_tables_words[] = {{QUERY_WORD_LITERAL, "tables", NULL, NULL},
                                           {QUERY_WORD_WORDS, "architectures", NULL, NULL},
                                           {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_decode_words[] = {{QUERY_WORD_LITERAL, "decode", NULL, NULL},
                                           {QUERY_WORD_WORDS, "words", NULL, NULL}};
static const QueryWord s_query_words[] = {
    {QUERY_WORD_LITERAL, "query", NULL, NULL}, {QUERY_WORD_FLAG, "--from", "from", NULL},
    {QUERY_WORD_FLAG, "--to", "to", NULL},     {QUERY_WORD_FLAG, "--part", "part", NULL},
    {QUERY_WORD_WHEN, "dry", "dry", "--dry"},  {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_learn_words[] = {{QUERY_WORD_LITERAL, "learn", NULL, NULL},
                                          {QUERY_WORD_VALUE, "step", NULL, NULL},
                                          {QUERY_WORD_VALUE, "word", NULL, NULL},
                                          {QUERY_WORD_FLAG, "--base", "base", NULL},
                                          {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_cross_words[] = {{QUERY_WORD_LITERAL, "cross", NULL, NULL},
                                          {QUERY_WORD_FLAG, "--base", "base", NULL},
                                          {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_write_words[] = {{QUERY_WORD_LITERAL, "write", NULL, NULL},
                                          {QUERY_WORD_VALUE, "writer", NULL, NULL},
                                          {QUERY_WORD_WORDS, "rulesets", NULL, NULL},
                                          {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_pairs_words[] = {
    {QUERY_WORD_LITERAL, "pairs", NULL, NULL},  {QUERY_WORD_VALUE, "mode", NULL, NULL},
    {QUERY_WORD_WORDS, "words", NULL, NULL},    {QUERY_WORD_FLAG, "--part", "part", NULL},
    {QUERY_WORD_FLAG, "--to", "to", NULL},      {QUERY_WORD_WHEN, "dry", "dry", "--dry"},
    {QUERY_WORD_FLAG, "--seed", "seed", NULL},  {QUERY_WORD_FLAG, "--trace", "trace", NULL},
    {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_stick_words[] = {{QUERY_WORD_LITERAL, "stick", NULL, NULL},
                                          {QUERY_WORD_VALUE, "what", NULL, NULL},
                                          {QUERY_WORD_WORDS, "words", NULL, NULL},
                                          {QUERY_WORD_FLAG, "--leave", "leave", NULL}};
static const QueryWord s_kept_words[] = {{QUERY_WORD_LITERAL, "kept", NULL, NULL}};
static const QueryWord s_sweep_words[] = {{QUERY_WORD_LITERAL, "sweep", NULL, NULL}};

static QueryCommand s_commands[11];

unsigned int query_commands(const char *root, const QueryCommand **commands)
{
    static const char *const s_language_kinds[] = {"klm", "krs"};
    static const char *const s_part_kinds[] = {"khw", "kqr", "ksc"};
    const unsigned int languages = query_whole(root, s_language_kinds, 2u, s_languages, s_language);
    const unsigned int parts = query_whole(root, s_part_kinds, 3u, s_parts, s_part);
    s_course_params[0] = (QueryParam){"from", "choice", g_query_course_names, g_query_course_steps, 0};
    s_course_params[1] = (QueryParam){"leave", "choice", s_leave_choices, 2u, 0};
    s_query_params[0] = (QueryParam){"from", "choice", s_language, languages, 1};
    s_query_params[1] = (QueryParam){"to", "choice", s_language, languages, 1};
    s_query_params[2] = (QueryParam){"part", "choice", s_part, parts, 0};
    s_query_params[3] = (QueryParam){"dry", "choice", s_dry_choices, 1u, 0};
    s_query_params[4] = (QueryParam){"leave", "choice", s_leave_choices, 2u, 0};
    s_pairs_params[0] = (QueryParam){"mode", "choice", s_pairs_modes, QUERY_COUNT(s_pairs_modes), 1};
    s_pairs_params[1] = (QueryParam){"words", "words", NULL, 0u, 0};
    s_pairs_params[2] = (QueryParam){"part", "choice", s_part, parts, 0};
    s_pairs_params[3] = (QueryParam){"to", "choice", s_language, languages, 0};
    s_pairs_params[4] = (QueryParam){"dry", "choice", s_dry_choices, 1u, 0};
    s_pairs_params[5] = (QueryParam){"seed", "word", NULL, 0u, 0};
    s_pairs_params[6] = (QueryParam){"trace", "word", NULL, 0u, 0};
    s_pairs_params[7] = (QueryParam){"leave", "choice", s_leave_choices, 2u, 0};
    s_commands[0] = (QueryCommand){"course",
                                   "protocol",
                                   "the whole protocol in order: vendor tables, discover, turns, registers, write krs, "
                                   "stick engine, pairs stick, pairs past, costs, write krs",
                                   s_course_params,
                                   2u,
                                   s_course_words,
                                   QUERY_COUNT(s_course_words)};
    s_commands[1] = (QueryCommand){"tables",
                                   "tables",
                                   "the vendor's tables, the instruction set and the latencies of every architecture, "
                                   "pulled off its disassembler; `read <arch> [--bank <size>] <low> <high>...` names "
                                   "each encoding's class and its constant read, `fields <arch>` the constant address "
                                   "spans",
                                   s_tables_params,
                                   QUERY_COUNT(s_tables_params),
                                   s_tables_words,
                                   QUERY_COUNT(s_tables_words)};
    s_commands[2] = (QueryCommand){"decode",
                                   "decode",
                                   "a run's table or blob printed through the decoder: --table <table> <run.qry>, or "
                                   "--blob <name> <run.qry>",
                                   s_decode_params,
                                   QUERY_COUNT(s_decode_params),
                                   s_decode_words,
                                   QUERY_COUNT(s_decode_words)};
    s_commands[3] =
        (QueryCommand){"query",
                       "query",
                       "the bridge written over the two languages' rulesets, then its pairs put to the part "
                       "where one is chosen",
                       s_query_params,
                       5u,
                       s_query_words,
                       QUERY_COUNT(s_query_words)};
    s_commands[4] = (QueryCommand){"learn",
                                   "learn",
                                   "a part pass: discover, seed, turns, registers or costs through the airlock, or "
                                   "rounds, dry or enumerate",
                                   s_learn_params,
                                   QUERY_COUNT(s_learn_params),
                                   s_learn_words,
                                   QUERY_COUNT(s_learn_words)};
    s_commands[5] = (QueryCommand){"cross",
                                   "cross",
                                   "the cross alone over the latest listed questions, reaching no part",
                                   s_cross_params,
                                   QUERY_COUNT(s_cross_params),
                                   s_cross_words,
                                   QUERY_COUNT(s_cross_words)};
    s_commands[6] = (QueryCommand){"write",
                                   "write",
                                   "the writers: the bridge over rulesets, the part's krs, or its gsm",
                                   s_write_params,
                                   QUERY_COUNT(s_write_params),
                                   s_write_words,
                                   QUERY_COUNT(s_write_words)};
    s_commands[7] = (QueryCommand){"pairs",
                                   "pairs",
                                   "the bridge's pairs put to the part, and the other modes of orior_klq_identity",
                                   s_pairs_params,
                                   8u,
                                   s_pairs_words,
                                   QUERY_COUNT(s_pairs_words)};
    s_commands[8] = (QueryCommand){"stick",
                                   "stick",
                                   "the vendor's reading of the stick, the engine's stick held against it, or the read "
                                   "with its words",
                                   s_stick_params,
                                   QUERY_COUNT(s_stick_params),
                                   s_stick_words,
                                   QUERY_COUNT(s_stick_words)};
    s_commands[9] = (QueryCommand){"kept",
                                   "kept",
                                   "the runs of the tree kept while the host is idle",
                                   NULL,
                                   0u,
                                   s_kept_words,
                                   QUERY_COUNT(s_kept_words)};
    s_commands[10] =
        (QueryCommand){"sweep",
                       "sweep",
                       "the host swept: the writer's and the file window's sizes timed, and qry_host.h and "
                       "files_host.js written from the sweeps' median",
                       NULL,
                       0u,
                       s_sweep_words,
                       QUERY_COUNT(s_sweep_words)};
    *commands = s_commands;
    return 11u;
}

int query_loop_run(QueryContext *context, const char *name, const char *const *words, unsigned int count, CliLine *said)
{
    static const struct
    {
        const char *name;
        int (*loop)(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
    } s_loops[11] = {{"course", query_course_run}, {"tables", query_tables_run}, {"decode", query_decode_run},
                     {"learn", query_learn_run},   {"cross", query_cross_run},   {"write", query_write_run},
                     {"query", query_query_run},   {"pairs", query_pairs_run},   {"stick", query_stick_run},
                     {"kept", query_kept_run},     {"sweep", query_sweep_run}};
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_loops[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            return s_loops[at].loop(context, words, count, said);
        }
    }
    cli_line_text(said, QUERY_USAGE);
    return -1;
}
