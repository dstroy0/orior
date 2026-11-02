// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_loops.h: the loops of `orior_cli protocol`, each run under a context with its words, the context's calls they
// share (commands.c), and the dispatch (commands_list.c). A loop gives its exit code, or -1 with why it could not run
// written into `err_msg`
#ifndef QUERY_LOOPS_H
#define QUERY_LOOPS_H
typedef struct QueryLoopsWordsWordsCarrierCfg QueryLoopsWordsWordsCarrierCfg;
typedef struct QueryLoopsFolderCfg QueryLoopsFolderCfg;
typedef struct QueryLoopsContextFolderCfg QueryLoopsContextFolderCfg;
typedef struct QueryLoopsContextErrMsgPartCfg QueryLoopsContextErrMsgPartCfg;
typedef struct QueryLoopsErrMsgContextWordsCfg QueryLoopsErrMsgContextWordsCfg;
typedef struct QueryLoopsContextErrMsgNameRunCfg QueryLoopsContextErrMsgNameRunCfg;
typedef struct QueryLoopsErrMsgWordsCountGivenCfg QueryLoopsErrMsgWordsCountGivenCfg;
typedef struct QueryLoopsContextErrMsgNameCfg QueryLoopsContextErrMsgNameCfg;
typedef struct QueryLoopsRootKindsCfg QueryLoopsRootKindsCfg;
typedef struct QueryLoopsContextErrMsgCfg QueryLoopsContextErrMsgCfg;
typedef struct QueryLoopsErrMsgWordsCfg QueryLoopsErrMsgWordsCfg;
typedef struct QueryLoopsContextErrMsgRulesetsCfg QueryLoopsContextErrMsgRulesetsCfg;
typedef struct QueryLoopsContextRunCfg QueryLoopsContextRunCfg;
typedef struct QueryLoopsErrMsgContextCfg QueryLoopsErrMsgContextCfg;
typedef struct QueryLoopsErrMsgWordsCountCfg QueryLoopsErrMsgWordsCountCfg;
typedef struct QueryLoopsErrMsgWordsCountModeCfg QueryLoopsErrMsgWordsCountModeCfg;
typedef struct QueryLoopsRootCfg QueryLoopsRootCfg;
typedef struct QueryLoopsErrMsgCfg QueryLoopsErrMsgCfg;
typedef struct QueryLoopsCuCfg QueryLoopsCuCfg;
typedef struct QueryLoopsContextCfg QueryLoopsContextCfg;
typedef struct QueryLoopsWordsWordsCfg QueryLoopsWordsWordsCfg;
typedef struct QueryLoopsWordsWordsTextCfg QueryLoopsWordsWordsTextCfg;
typedef struct QueryLoopsCfg QueryLoopsCfg;

#include "embed_dispatch_layout.h"
#include "query_internal.h"

#define QUERY_USAGE                                                                                                    \
    "orior_cli protocol takes one of course, tables, decode, query, learn, cross, write, pairs, stick, retention, "    \
    "sweep"

// the folder the bridge, the rulesets and the part's files stand in
#define QUERY_TABLE "src/cu/transpiler/lstar/protocol/table"

// a run started in `folder`, a path from the top of the tree, or joined: the run the context has open where it has
// one, every step of the course a member of the course's run
int query_context_start(const QueryLoopsContextRunCfg *args);
// the tool `name` built where stale, its path into `binary`, sized for QUERY_PATH_MAX_LEN
int query_context_tool(const QueryLoopsErrMsgContextCfg *args);
// `words` run in the run's environment, every setting appended first: teed into the run as `<name>.console`; run
// with this process's own output; or its output whole handed as the blob `name`. Its exit code into `code`
int query_context_teed(const QueryLoopsContextErrMsgNameRunCfg *args);
int query_context_run(const QueryLoopsContextErrMsgNameRunCfg *args);
int query_context_dispatch(const QueryLoopsContextErrMsgNameRunCfg *args);
// the run ended as the context's leave specifies
int query_context_end(const QueryLoopsContextRunCfg *args);

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
    char text[QUERY_TEXTS_MOST][QUERY_PATH_MAX_LEN];
    unsigned int texts;
} QueryWords;

// `word` added to `words`; a copy of `text` added to `words`, stored in their texts
void query_words_add(const QueryLoopsWordsWordsCfg *args);
void query_words_text(const QueryLoopsWordsWordsTextCfg *args);

// The part's files in the tree and the vendor's tables
typedef struct
{
    char machine[QUERY_PATH_MAX_LEN];
    char costs[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    char layout[QUERY_PATH_MAX_LEN];
    char mnemonics[QUERY_PATH_MAX_LEN];
    char fields[QUERY_PATH_MAX_LEN];
} QueryFiles;

void query_files_get(const QueryLoopsRootCfg *args);
// the carrier's words for a live pass, added to `words`: the gated list it will not reach the part without, the
// lessons the conferences taught where `lessons`, and the airlock, the pop and the cross
void query_carrier_words(const QueryLoopsWordsWordsCarrierCfg *args);
// the base source through the vendor's compiler, what it printed handed as `nvcc.txt`, its container as
// `base_nvcc.cubin` and its listing as `base_nvcc.sass`
int query_base_compile(const QueryLoopsContextErrMsgCfg *args);
// nvdisasm beside nvcc, which the airlock reads with, into `path`; without it nothing goes to the part
int query_nvdisasm(const QueryLoopsErrMsgCfg *args);

// the bridge written over `rulesets`, or over cu.krs, sass.krs and openqasm2_0.krs where `count` is 0 (write.c)
int query_bridge(const QueryLoopsContextErrMsgRulesetsCfg *args);
// the bridge's pairs put to the part `part` toward the language `to`, dry or live, with a seed and a trace folder
// where they are given (pairs.c)
int query_pairs_pair(const QueryLoopsContextErrMsgPartCfg *args);

// every `.bin` under `folder`, each the code of the kernel `<prefix><number>` of the cubin at `pattern`, read by the
// vendor at one start (`orior_cubin_listing`), and each code's listing, the run's `<number>.dis`, handed to the run's
// writer as the `.dis` beside it (stick.c)
int query_listings_read(const QueryLoopsContextFolderCfg *args);

// the run `name` of the folder containing it newest other than `own` (run_folder.c), into `found`
int query_latest_run(const QueryLoopsFolderCfg *args);

// the steps of the course by name, in order, as the window offers them to start from (course.c)
extern const char *const g_query_course_names[];
extern const unsigned int g_query_course_steps;

// the loop `name` run with `words`, `count` of them
int query_loop_run(const QueryLoopsContextErrMsgNameCfg *args);

typedef struct
{
    QueryContext *context;
    const char *const *words;
    unsigned int count;
    VerbaLinea *err_msg;
} QueryLoopCfg;

int query_course_run(const QueryLoopCfg *args);
int query_tables_run(const QueryLoopCfg *args);
int query_decode_run(const QueryLoopCfg *args);
int query_learn_run(const QueryLoopCfg *args);
int query_cross_run(const QueryLoopCfg *args);
int query_write_run(const QueryLoopCfg *args);
int query_query_run(const QueryLoopCfg *args);
int query_pairs_run(const QueryLoopCfg *args);
int query_stick_run(const QueryLoopCfg *args);
int query_retention_run(const QueryLoopCfg *args);
int query_sweep_run(const QueryLoopCfg *args);

// the most bytes of a name the table contains whole
#define QUERY_WHOLE_MAX_LEN 64u

// the writers `write` runs, by its words
#define QUERY_WRITE_BRIDGE 0
#define QUERY_WRITE_KRS 1
#define QUERY_WRITE_GSM 2

// the words the loops read, as their checks reach them: `--leave <word>` taken off `words`, `*count` of them, where
// it is there, into `leave`, `given` set where it was: 1, or 0 with why given (commands.c); every name the table
// contains a file of each of `kinds` of, sorted, into `names`, its list into `named`: their count (commands_list.c);
// the names of the course's steps joined by `, `; and the index of the step `--from` names, the first where none is
// named: 1, or 0 with why given (course.c); the base source the cross's words name, NULL where none: 1, or 0 with why
// given (cross.c); the reading of a pulled table the words request, its mode in `mode`: 1, or 0 where they request a
// pull, -1 with why given (tables_pull.c); the names a query's words give and whether it is dry: 1, or 0 with why
// given, each file missing named; and the writer `write`'s words name, -1 with why given (write.c)
int query_leave_take(const QueryLoopsErrMsgWordsCountGivenCfg *args);
unsigned int query_whole(const QueryLoopsRootKindsCfg *args);
void query_names_join(const QueryLoopsCfg *args);
int query_course_from(const QueryLoopsErrMsgWordsCountCfg *args);
int query_base_get(const QueryLoopsErrMsgWordsCfg *args);
int query_reading(const QueryLoopsErrMsgWordsCountModeCfg *args);
int query_query_words(const QueryLoopsErrMsgContextWordsCfg *args);
int query_write_words(const QueryLoopsErrMsgWordsCfg *args);

// The vendor's reading of the stick, each file where the stick's folder stores it
typedef struct
{
    char out[QUERY_PATH_MAX_LEN];
    char cu[QUERY_PATH_MAX_LEN];
    char cu_new[QUERY_PATH_MAX_LEN];
    char tsv[QUERY_PATH_MAX_LEN];
    char cubin[QUERY_PATH_MAX_LEN];
    char sass[QUERY_PATH_MAX_LEN];
} QueryStickPaths;

// what `stick` does, by its words
#define QUERY_STICK_VENDOR 0
#define QUERY_STICK_ENGINE 1
#define QUERY_STICK_READ 2

// the stick's files under the tree; 1 where the stick's text at `fresh` differs from the one at `cu` nvcc compiled,
// or no container is there; and what `stick`'s words request, -1 with why given (stick.c)
void query_stick_paths(const QueryLoopsContextCfg *args);
int query_stick_change(const QueryLoopsCuCfg *args);
int query_stick_words(const QueryLoopsErrMsgWordsCfg *args);

struct QueryLoopsWordsWordsCarrierCfg
{
    QueryWords *words_words_pointer;
    const char *carrier;
    const char *cross;
    const char *nvdisasm;
    const char *machine;
    const char *layout;
    int lessons;
};

struct QueryLoopsFolderCfg
{
    const char *folder;
    const char *name;
    const char *own;
    const QueryWriter *writer;
    const char *crystal;
    char *found;
    size_t room;
};

struct QueryLoopsContextFolderCfg
{
    QueryContext *context;
    const char *folder;
    VerbaLinea *err_msg;
    const QueryRun *run;
    const char *pattern;
    const char *prefix;
    const char *architecture;
};

struct QueryLoopsContextErrMsgPartCfg
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *part;
    const char *to;
    int dry;
    const char *seed;
    const char *trace;
};

struct QueryLoopsErrMsgContextWordsCfg
{
    VerbaLinea *err_msg;
    const QueryContext *context_read;
    const char *const *words;
    unsigned int count;
    const char **names_letters;
    unsigned int *named_u32s;
    int *dry_pointer;
};

struct QueryLoopsContextErrMsgNameRunCfg
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *name;
    const QueryRun *run;
    const char *const *words;
    long long *code;
};

struct QueryLoopsErrMsgWordsCountGivenCfg
{
    VerbaLinea *err_msg;
    const char **words_read;
    unsigned int *count_pointer;
    int *given;
    QueryLeave *leave;
};

struct QueryLoopsContextErrMsgNameCfg
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *name;
    const char *const *words;
    unsigned int count;
};

struct QueryLoopsRootKindsCfg
{
    const char *root;
    const char *const *kinds;
    unsigned int kind_count;
    char (*names)[QUERY_WHOLE_MAX_LEN];
    const char **named;
};

struct QueryLoopsContextErrMsgCfg
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const QueryRun *run;
    const char *base;
};

struct QueryLoopsErrMsgWordsCfg
{
    VerbaLinea *err_msg;
    const char *const *words;
    unsigned int count;
    const char **base_pointer;
};

struct QueryLoopsContextErrMsgRulesetsCfg
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *const *rulesets;
    unsigned int count;
};

struct QueryLoopsContextRunCfg
{
    QueryContext *context;
    QueryRun *run_out;
    const char *folder;
    VerbaLinea *err_msg;
};

struct QueryLoopsErrMsgContextCfg
{
    VerbaLinea *err_msg;
    const QueryContext *context_read;
    const char *name;
    char *binary;
};

struct QueryLoopsErrMsgWordsCountCfg
{
    VerbaLinea *err_msg;
    const char *const *words;
    unsigned int count;
    unsigned int *first;
};

struct QueryLoopsErrMsgWordsCountModeCfg
{
    VerbaLinea *err_msg;
    const char *const *words;
    unsigned int count;
    const char **mode;
};

struct QueryLoopsRootCfg
{
    const char *root;
    const char *part;
    QueryFiles *files;
};

struct QueryLoopsErrMsgCfg
{
    VerbaLinea *err_msg;
    const QueryContext *context_read;
    char *path;
};

struct QueryLoopsCuCfg
{
    const char *cu;
    const char *fresh;
    const char *cubin;
};

struct QueryLoopsContextCfg
{
    const QueryContext *context_read;
    QueryStickPaths *paths;
};

struct QueryLoopsWordsWordsCfg
{
    QueryWords *words_words_pointer;
    const char *word;
};

struct QueryLoopsWordsWordsTextCfg
{
    QueryWords *words_words_pointer;
    const char *text;
};

struct QueryLoopsCfg
{
    VerbaLinea *line;
};

typedef struct
{
    int (*context_start)(const QueryLoopsContextRunCfg *args);
    int (*context_end)(const QueryLoopsContextRunCfg *args);
} QueryLoopsContextRunNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextRunNs, context_start, context_end);
EMBED_TABLE_STORAGE QueryLoopsContextRunNs query_loops_context_run EMBED_UNUSED = {
    .context_start = query_context_start,
    .context_end = query_context_end,
};

typedef struct
{
    int (*context_tool)(const QueryLoopsErrMsgContextCfg *args);
} QueryLoopsErrMsgContextNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgContextNs, context_tool);
EMBED_TABLE_STORAGE QueryLoopsErrMsgContextNs query_loops_err_msg_context EMBED_UNUSED = {
    .context_tool = query_context_tool,
};

typedef struct
{
    int (*context_teed)(const QueryLoopsContextErrMsgNameRunCfg *args);
    int (*context_run)(const QueryLoopsContextErrMsgNameRunCfg *args);
    int (*context_dispatch)(const QueryLoopsContextErrMsgNameRunCfg *args);
} QueryLoopsContextErrMsgNameRunNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextErrMsgNameRunNs, context_teed, context_run, context_dispatch);
EMBED_TABLE_STORAGE QueryLoopsContextErrMsgNameRunNs query_loops_context_err_msg_name_run EMBED_UNUSED = {
    .context_teed = query_context_teed,
    .context_run = query_context_run,
    .context_dispatch = query_context_dispatch,
};

typedef struct
{
    void (*words_add)(const QueryLoopsWordsWordsCfg *args);
} QueryLoopsWordsWordsNs;
EMBED_TABLE_LAYOUT(QueryLoopsWordsWordsNs, words_add);
EMBED_TABLE_STORAGE QueryLoopsWordsWordsNs query_loops_words_words EMBED_UNUSED = {
    .words_add = query_words_add,
};

typedef struct
{
    void (*words_text)(const QueryLoopsWordsWordsTextCfg *args);
} QueryLoopsWordsWordsTextNs;
EMBED_TABLE_LAYOUT(QueryLoopsWordsWordsTextNs, words_text);
EMBED_TABLE_STORAGE QueryLoopsWordsWordsTextNs query_loops_words_words_text EMBED_UNUSED = {
    .words_text = query_words_text,
};

typedef struct
{
    void (*files_get)(const QueryLoopsRootCfg *args);
} QueryLoopsRootNs;
EMBED_TABLE_LAYOUT(QueryLoopsRootNs, files_get);
EMBED_TABLE_STORAGE QueryLoopsRootNs query_loops_root EMBED_UNUSED = {
    .files_get = query_files_get,
};

typedef struct
{
    void (*carrier_words)(const QueryLoopsWordsWordsCarrierCfg *args);
} QueryLoopsWordsWordsCarrierNs;
EMBED_TABLE_LAYOUT(QueryLoopsWordsWordsCarrierNs, carrier_words);
EMBED_TABLE_STORAGE QueryLoopsWordsWordsCarrierNs query_loops_words_words_carrier EMBED_UNUSED = {
    .carrier_words = query_carrier_words,
};

typedef struct
{
    int (*base_compile)(const QueryLoopsContextErrMsgCfg *args);
} QueryLoopsContextErrMsgNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextErrMsgNs, base_compile);
EMBED_TABLE_STORAGE QueryLoopsContextErrMsgNs query_loops_context_err_msg EMBED_UNUSED = {
    .base_compile = query_base_compile,
};

typedef struct
{
    int (*nvdisasm)(const QueryLoopsErrMsgCfg *args);
} QueryLoopsErrMsgNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgNs, nvdisasm);
EMBED_TABLE_STORAGE QueryLoopsErrMsgNs query_loops_err_msg EMBED_UNUSED = {
    .nvdisasm = query_nvdisasm,
};

typedef struct
{
    int (*bridge)(const QueryLoopsContextErrMsgRulesetsCfg *args);
} QueryLoopsContextErrMsgRulesetsNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextErrMsgRulesetsNs, bridge);
EMBED_TABLE_STORAGE QueryLoopsContextErrMsgRulesetsNs query_loops_context_err_msg_rulesets EMBED_UNUSED = {
    .bridge = query_bridge,
};

typedef struct
{
    int (*pairs_pair)(const QueryLoopsContextErrMsgPartCfg *args);
} QueryLoopsContextErrMsgPartNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextErrMsgPartNs, pairs_pair);
EMBED_TABLE_STORAGE QueryLoopsContextErrMsgPartNs query_loops_context_err_msg_part EMBED_UNUSED = {
    .pairs_pair = query_pairs_pair,
};

typedef struct
{
    int (*listings_read)(const QueryLoopsContextFolderCfg *args);
} QueryLoopsContextFolderNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextFolderNs, listings_read);
EMBED_TABLE_STORAGE QueryLoopsContextFolderNs query_loops_context_folder EMBED_UNUSED = {
    .listings_read = query_listings_read,
};

typedef struct
{
    int (*latest_run)(const QueryLoopsFolderCfg *args);
} QueryLoopsFolderNs;
EMBED_TABLE_LAYOUT(QueryLoopsFolderNs, latest_run);
EMBED_TABLE_STORAGE QueryLoopsFolderNs query_loops_folder EMBED_UNUSED = {
    .latest_run = query_latest_run,
};

typedef struct
{
    int (*loop_run)(const QueryLoopsContextErrMsgNameCfg *args);
} QueryLoopsContextErrMsgNameNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextErrMsgNameNs, loop_run);
EMBED_TABLE_STORAGE QueryLoopsContextErrMsgNameNs query_loops_context_err_msg_name EMBED_UNUSED = {
    .loop_run = query_loop_run,
};

typedef struct
{
    int (*course_run)(const QueryLoopCfg *args);
    int (*tables_run)(const QueryLoopCfg *args);
    int (*decode_run)(const QueryLoopCfg *args);
    int (*learn_run)(const QueryLoopCfg *args);
    int (*cross_run)(const QueryLoopCfg *args);
    int (*write_run)(const QueryLoopCfg *args);
    int (*query_run)(const QueryLoopCfg *args);
    int (*pairs_run)(const QueryLoopCfg *args);
    int (*stick_run)(const QueryLoopCfg *args);
    int (*retention_run)(const QueryLoopCfg *args);
    int (*sweep_run)(const QueryLoopCfg *args);
} QueryLoopNs;
EMBED_TABLE_LAYOUT(QueryLoopNs, course_run, tables_run, decode_run, learn_run, cross_run, write_run, query_run,
                   pairs_run, stick_run, retention_run, sweep_run);
EMBED_TABLE_STORAGE QueryLoopNs query_loop EMBED_UNUSED = {
    .course_run = query_course_run,
    .tables_run = query_tables_run,
    .decode_run = query_decode_run,
    .learn_run = query_learn_run,
    .cross_run = query_cross_run,
    .write_run = query_write_run,
    .query_run = query_query_run,
    .pairs_run = query_pairs_run,
    .stick_run = query_stick_run,
    .retention_run = query_retention_run,
    .sweep_run = query_sweep_run,
};

typedef struct
{
    int (*leave_take)(const QueryLoopsErrMsgWordsCountGivenCfg *args);
} QueryLoopsErrMsgWordsCountGivenNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgWordsCountGivenNs, leave_take);
EMBED_TABLE_STORAGE QueryLoopsErrMsgWordsCountGivenNs query_loops_err_msg_words_count_given EMBED_UNUSED = {
    .leave_take = query_leave_take,
};

typedef struct
{
    unsigned int (*whole)(const QueryLoopsRootKindsCfg *args);
} QueryLoopsRootKindsNs;
EMBED_TABLE_LAYOUT(QueryLoopsRootKindsNs, whole);
EMBED_TABLE_STORAGE QueryLoopsRootKindsNs query_loops_root_kinds EMBED_UNUSED = {
    .whole = query_whole,
};

typedef struct
{
    void (*names_join)(const QueryLoopsCfg *args);
} CliLoopsNs;
EMBED_TABLE_LAYOUT(CliLoopsNs, names_join);
EMBED_TABLE_STORAGE CliLoopsNs cli_loops EMBED_UNUSED = {
    .names_join = query_names_join,
};

typedef struct
{
    int (*course_from)(const QueryLoopsErrMsgWordsCountCfg *args);
} QueryLoopsErrMsgWordsCountNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgWordsCountNs, course_from);
EMBED_TABLE_STORAGE QueryLoopsErrMsgWordsCountNs query_loops_err_msg_words_count EMBED_UNUSED = {
    .course_from = query_course_from,
};

typedef struct
{
    int (*base_get)(const QueryLoopsErrMsgWordsCfg *args);
    int (*write_words)(const QueryLoopsErrMsgWordsCfg *args);
    int (*stick_words)(const QueryLoopsErrMsgWordsCfg *args);
} QueryLoopsErrMsgWordsNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgWordsNs, base_get, write_words, stick_words);
EMBED_TABLE_STORAGE QueryLoopsErrMsgWordsNs query_loops_err_msg_words EMBED_UNUSED = {
    .base_get = query_base_get,
    .write_words = query_write_words,
    .stick_words = query_stick_words,
};

typedef struct
{
    int (*reading)(const QueryLoopsErrMsgWordsCountModeCfg *args);
} QueryLoopsErrMsgWordsCountModeNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgWordsCountModeNs, reading);
EMBED_TABLE_STORAGE QueryLoopsErrMsgWordsCountModeNs query_loops_err_msg_words_count_mode EMBED_UNUSED = {
    .reading = query_reading,
};

typedef struct
{
    int (*query_words)(const QueryLoopsErrMsgContextWordsCfg *args);
} QueryLoopsErrMsgContextWordsNs;
EMBED_TABLE_LAYOUT(QueryLoopsErrMsgContextWordsNs, query_words);
EMBED_TABLE_STORAGE QueryLoopsErrMsgContextWordsNs query_loops_err_msg_context_words EMBED_UNUSED = {
    .query_words = query_query_words,
};

typedef struct
{
    void (*stick_paths)(const QueryLoopsContextCfg *args);
} QueryLoopsContextNs;
EMBED_TABLE_LAYOUT(QueryLoopsContextNs, stick_paths);
EMBED_TABLE_STORAGE QueryLoopsContextNs query_loops_context EMBED_UNUSED = {
    .stick_paths = query_stick_paths,
};

typedef struct
{
    int (*stick_change)(const QueryLoopsCuCfg *args);
} QueryLoopsCuNs;
EMBED_TABLE_LAYOUT(QueryLoopsCuNs, stick_change);
EMBED_TABLE_STORAGE QueryLoopsCuNs query_loops_cu EMBED_UNUSED = {
    .stick_change = query_stick_change,
};

#endif
