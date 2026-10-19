// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_loops.h: the loops of `orior protocol`, each run under a context with its words, the context's calls they
// share (commands.c), and the dispatch (commands_list.c). A loop gives its exit code, or -1 with why it could not run
// said into `said`
#ifndef QUERY_LOOPS_H
#define QUERY_LOOPS_H

#include "query_internal.h"

#define QUERY_USAGE                                                                                                    \
    "orior protocol takes one of course, tables, decode, query, learn, cross, write, pairs, stick, kept, sweep"

// the folder the bridge, the rulesets and the part's files stand in
#define QUERY_TABLE "src/cu/transpiler/lstar/protocol/table"

// a run started in `folder`, a path from the top of the tree, or joined: the run the context holds open where it holds
// one, every step of the course a member of the course's run
int query_context_started(QueryContext *context, QueryRun *run, const char *folder, CliLine *said);
// the tool `name` built where stale, its path into `binary`, which holds QUERY_PATH_LONGEST
int query_context_tool(const QueryContext *context, const char *name, char *binary, CliLine *said);
// `words` run in the run's environment, every setting appended first: teed into the run as `<name>.console`; run
// with this process's own output; or its output whole handed as the blob `name`. Its exit code into `code`
int query_context_teed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                       long long *code, CliLine *said);
int query_context_ran(QueryContext *context, const QueryRun *run, const char *const *words, long long *code,
                      CliLine *said);
int query_context_handed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                         long long *code, CliLine *said);
// the run ended as the context's leave says
int query_context_ended(QueryContext *context, QueryRun *run, CliLine *said);

// the vendor's folder, the run folder of every part pass, and the working folder beside it: the forms lists and the
// dry machine file
#define QUERY_NVIDIA "src/cu/transpiler/vendor_bin_layouts/nvidia"

#define QUERY_LEARN_FOLDER "build/engine/khw/qry"

#define QUERY_LEARN_WORK "build/engine/khw"

// the most texts one list of words composes, beyond the words it points at
#define QUERY_TEXTS_MOST 24u

// A list of words for a program: the words, NULL after the last, and the texts composed for some of them
typedef struct
{
    const char *word[QUERY_WORDS_MOST + 1u];
    unsigned int count;
    char text[QUERY_TEXTS_MOST][QUERY_PATH_LONGEST];
    unsigned int texts;
} QueryWords;

// `word` added to `words`; a copy of `text` added to `words`, kept in their texts
void query_words_add(QueryWords *words, const char *word);
void query_words_text(QueryWords *words, const char *text);

// The part's files in the tree and the vendor's tables
typedef struct
{
    char machine[QUERY_PATH_LONGEST];
    char costs[QUERY_PATH_LONGEST];
    char record[QUERY_PATH_LONGEST];
    char layout[QUERY_PATH_LONGEST];
    char mnemonics[QUERY_PATH_LONGEST];
    char fields[QUERY_PATH_LONGEST];
} QueryFiles;

void query_files_of(const char *root, const char *part, QueryFiles *files);
// the carrier's words for a live pass, added to `words`: the held list it will not reach the part without, the
// lessons the conferences taught where `lessons`, and the airlock, the pop and the cross
void query_carrier_words(QueryWords *words, const char *carrier, const char *cross, const char *nvdisasm,
                         const char *machine, const char *layout, int lessons);
// the base source through the vendor's compiler, what it said handed as `nvcc.txt`, its container as
// `base_nvcc.cubin` and its listing as `base_nvcc.sass`
int query_base_compiled(QueryContext *context, const QueryRun *run, const char *base, CliLine *said);
// nvdisasm beside nvcc, which the airlock reads with, into `path`; without it nothing goes to the part
int query_nvdisasm(const QueryContext *context, char *path, CliLine *said);

// the bridge written over `rulesets`, or over cu.krs, sass.krs and openqasm2_0.krs where `count` is 0 (write.c)
int query_bridge(QueryContext *context, const char *const *rulesets, unsigned int count, CliLine *said);
// the bridge's pairs put to the part `part` toward the language `to`, dry or live, with a seed and a trace folder
// where they are given (pairs.c)
int query_pairs_pair(QueryContext *context, const char *part, const char *to, int dry, const char *seed,
                     const char *trace, CliLine *said);

// every `.bin` under `folder`, each the code of the kernel `<prefix><number>` of the cubin at `pattern`, read by the
// vendor at one start (`cubin_listing`), and each code's listing, the run's `<number>.dis`, handed to the run's writer
// as the `.dis` beside it (stick.c)
int query_listings_read(QueryContext *context, const QueryRun *run, const char *pattern, const char *prefix,
                        const char *architecture, const char *folder, CliLine *said);

// the run `name` of the folder holding it newest other than `own` (run_folder.c), into `found`
int query_latest_run(const char *folder, const char *name, const char *own, const QueryWriter *writer,
                     const char *crystal, char *found, size_t room);

// the steps of the course by name, in order, as the window offers them to start from (course.c)
extern const char *const g_query_course_names[];
extern const unsigned int g_query_course_steps;

// the loop `name` run with `words`, `count` of them
int query_loop_run(QueryContext *context, const char *name, const char *const *words, unsigned int count,
                   CliLine *said);

int query_course_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_tables_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_decode_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_learn_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_cross_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_write_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_query_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_pairs_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_stick_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_kept_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
int query_sweep_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);

// the most bytes of a name the table holds whole
#define QUERY_WHOLE_LONGEST 64u

// the writers `write` runs, by its words
#define QUERY_WRITE_BRIDGE 0
#define QUERY_WRITE_KRS 1
#define QUERY_WRITE_GSM 2

// the words the loops read, as their checks reach them: `--leave <word>` taken off `words`, `*count` of them, where
// it is there, into `leave`, `given` set where it was: 1, or 0 with why said (commands.c); every name the table
// holds a file of each of `kinds` of, sorted, into `names`, its list into `named`: their count (commands_list.c);
// the names of the course's steps joined by `, `; and the index of the step `--from` names, the first where none is
// named: 1, or 0 with why said (course.c); the base source the cross's words name, NULL where none: 1, or 0 with why
// said (cross.c); the reading of a pulled table the words ask for, its mode in `mode`: 1, or 0 where they ask for a
// pull, -1 with why said (tables_pull.c); the names a query's words give and whether it is dry: 1, or 0 with why
// said, each file missing named; and the writer `write`'s words name, -1 with why said (write.c)
int query_leave_taken(const char **words, unsigned int *count, int *given, QueryLeave *leave, CliLine *said);
unsigned int query_whole(const char *root, const char *const *kinds, unsigned int kind_count,
                         char (*names)[QUERY_WHOLE_LONGEST], const char **named);
void query_names_joined(CliLine *line);
int query_course_from(const char *const *words, unsigned int count, unsigned int *first, CliLine *said);
int query_base_of(const char *const *words, unsigned int count, const char **base, CliLine *said);
int query_reading(const char *const *words, unsigned int count, const char **mode, CliLine *said);
int query_query_words(const QueryContext *context, const char *const *words, unsigned int count, const char **names,
                      unsigned int *named, int *dry, CliLine *said);
int query_write_words(const char *const *words, unsigned int count, CliLine *said);

// The vendor's reading of the stick, each file where the stick's folder keeps it
typedef struct
{
    char out[QUERY_PATH_LONGEST];
    char cu[QUERY_PATH_LONGEST];
    char cu_new[QUERY_PATH_LONGEST];
    char tsv[QUERY_PATH_LONGEST];
    char cubin[QUERY_PATH_LONGEST];
    char sass[QUERY_PATH_LONGEST];
} QueryStickPaths;

// what `stick` does, by its words
#define QUERY_STICK_VENDOR 0
#define QUERY_STICK_ENGINE 1
#define QUERY_STICK_READ 2

// the stick's files under the tree; 1 where the stick's text at `fresh` differs from the one at `cu` nvcc compiled,
// or no container is there; and what `stick`'s words ask, -1 with why said (stick.c)
void query_stick_paths(const QueryContext *context, QueryStickPaths *paths);
int query_stick_changed(const char *cu, const char *fresh, const char *cubin);
int query_stick_words(const char *const *words, unsigned int count, CliLine *said);

#endif
