// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// write.c: the writers, each over the part's files and the rulesets and reaching no part: `orior_klq_write` writes
// `Lstar.klq` and each given language's `.klm` beside the rulesets; `orior_krs_write` writes `sass.krs` and `sm_86.krs`
// beside `sm_86.khw` from the forms the part answered for; `orior_gsm_write` writes `sm_86.gsm`, each relation the part
// answered as the cases it answered it on. And the query: the bridge written over two languages' rulesets, then its
// pairs put to the part where one is chosen
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

// the most rulesets one bridge is written over
#define QUERY_RULESETS_MAX 16u

typedef struct
{
    const QueryContext *context;
    const char *name;
    char *path;
    QueryContext *context_out;
    int krs;
    VerbaLinea *err_msg;
    int language;
    VerbaLinea *missing;
} WriteCtx;
// `name` under the bridge's folder into `path`
EMBED_INLINE void query_table_by_name(const WriteCtx *args)
{
    const QueryContext *context = args->context;
    char *path = args->path;
    char table[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = QUERY_TABLE,
               .path_out = table);
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, table);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// `Lstar.klq` and each given language's `.klm` written beside the rulesets (`orior_klq_write`), teed as
// `orior_klq_write.console`: the writer's exit code
int query_bridge(const QueryLoopsContextErrMsgRulesetsCfg *args)
{
    QueryContext *context = args->context;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_default[3] = {"cu.krs", "sass.krs", "openqasm2_0.krs"};
    const char *const *supplied = (count == 0u) ? s_default : args->rulesets;
    const unsigned int input_count = (count == 0u) ? 3u : ((count < QUERY_RULESETS_MAX) ? count : QUERY_RULESETS_MAX);
    static char s_klq_write[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_klq_write", .binary = s_klq_write, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = "build/engine/klq/qry", .err_msg = err_msg))
    {
        return -1;
    }
    static char s_read[QUERY_RULESETS_MAX + 1u][QUERY_PATH_MAX_LEN];
    static const char *s_reads[QUERY_RULESETS_MAX + 1u];
    for (unsigned int at = 0u; at < input_count; at += 1u)
    {
        EMBED_CALL(query_table_by_name, WriteCtx, .context = context, .name = supplied[at], .path = s_read[at]);
        s_reads[at] = s_read[at];
    }
    EMBED_CALL(query_table_by_name, WriteCtx, .context = context, .name = "Lstar.klq", .path = s_read[input_count]);
    s_reads[input_count] = s_read[input_count];
    char table[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = QUERY_TABLE,
               .path_out = table);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = s_klq_write);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = table);
    for (unsigned int at = 0u; at < input_count; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = supplied[at]);
    }
    long long code = -1LL;
    const int whole = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                                 .paths_letters = s_reads, .count = input_count + 1u, .err_msg = err_msg) &&
                      EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                 .context = context, .run = &s_run, .name = "orior_klq_write",
                                 .words = s_words.word_text, .code = &code, .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

// `sass.krs` and `sm_86.krs` written beside `sm_86.khw` from the forms the part answered for (`orior_krs_write`), teed
// as `orior_krs_write.console`; or `sm_86.gsm`, each relation the part answered as the cases it answered it on
// (`orior_gsm_write`). The writer's exit code
EMBED_INLINE int query_part_write(const WriteCtx *args)
{
    QueryContext *context = args->context_out;
    VerbaLinea *err_msg = args->err_msg;
    static char s_tool[QUERY_PATH_MAX_LEN];
    static char s_writer[QUERY_PATH_MAX_LEN];
    static QueryFiles s_files;
    static QueryRun s_run;
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = context->root, .part = "sm_86",
               .files = &s_files);
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = args->krs ? "orior_krs_write" : "orior_gsm_write", .binary = s_tool, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_khw_machine_write", .binary = s_writer, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = args->krs ? "build/engine/krs/qry" : "build/engine/gsm/qry",
                    .err_msg = err_msg))
    {
        return -1;
    }
    char part_krs[QUERY_PATH_MAX_LEN];
    char sass_krs[QUERY_PATH_MAX_LEN];
    char cu_krs[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_table_by_name, WriteCtx, .context = context, .name = "sm_86.krs", .path = part_krs);
    EMBED_CALL(query_table_by_name, WriteCtx, .context = context, .name = "sass.krs", .path = sass_krs);
    EMBED_CALL(query_table_by_name, WriteCtx, .context = context, .name = "cu.krs", .path = cu_krs);
    const char *const krs_read[8] = {s_files.machine, s_files.costs,  part_krs,          sass_krs,
                                     cu_krs,          s_files.layout, s_files.mnemonics, NULL};
    const char *const gsm_read[4] = {s_files.machine, s_files.layout, s_files.mnemonics, NULL};
    const char *const krs_words[9] = {s_tool,   "sass", "sm_86", s_files.machine, s_files.layout, s_files.mnemonics,
                                      s_writer, cu_krs, NULL};
    const char *const gsm_words[7] = {s_tool,   "sm_86", s_files.machine, s_files.layout, s_files.mnemonics,
                                      s_writer, NULL};
    long long code = -1LL;
    const int whole =
        EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                   .paths_letters = args->krs ? krs_read : gsm_read, .count = args->krs ? 7u : 3u,
                   .err_msg = err_msg) &&
        EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                   .context = context, .run = &s_run, .name = args->krs ? "orior_krs_write" : "orior_gsm_write",
                   .words = args->krs ? krs_words : gsm_words, .code = &code, .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

int query_write_words(const QueryLoopsErrMsgWordsCfg *args)
{
    if ((args->count >= 1u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "bridge", .cap = CLI_CELLS_POOL})))
    {
        return QUERY_WRITE_BRIDGE;
    }
    if ((args->count == 1u) &&
        ((cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "krs", .cap = CLI_CELLS_POOL})) ||
         (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "gsm", .cap = CLI_CELLS_POOL}))))
    {
        return (args->words[0][0] == 'k') ? QUERY_WRITE_KRS : QUERY_WRITE_GSM;
    }
    mmgr_line_put(args->err_msg, "write takes bridge [<ruleset>...], krs or gsm");
    return -1;
}

int query_write_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    const int writer = EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = words,
                                  .count = count, .err_msg = err_msg);
    if (writer == QUERY_WRITE_BRIDGE)
    {
        return EMBED_CALL(query_loops_context_err_msg_rulesets.bridge, QueryLoopsContextErrMsgRulesetsCfg,
                          .context = context, .rulesets = &words[1], .count = count - 1u, .err_msg = err_msg);
    }
    return (writer < 0) ? -1
                        : EMBED_CALL(query_part_write, WriteCtx, .context_out = context,
                                     .krs = writer == QUERY_WRITE_KRS, .err_msg = err_msg);
}

// a name the table contains a whole set of files of: the language's `.klm` and `.krs` where `language`, else the part's
// `.khw`, `.kqr` and `.ksc`, each missing one listed after `missing`'s others
EMBED_INLINE void query_missing_files(const WriteCtx *args)
{
    static const char *const s_language[2] = {"klm", "krs"};
    static const char *const s_part[3] = {"khw", "kqr", "ksc"};
    const char *const *const kinds = args->language ? s_language : s_part;
    for (unsigned int at = 0u; at < (args->language ? 2u : 3u); at += 1u)
    {
        char file[QUERY_PATH_MAX_LEN];
        char path[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {file, sizeof(file), 0ULL};
        mmgr_line_put(&line, args->name);
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, kinds[at]);
        mmgr_line_finish(&line);
        EMBED_CALL(query_table_by_name, WriteCtx, .context = args->context, .name = file, .path = path);
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            mmgr_line_put(args->missing, (args->missing->at == 0ULL) ? "" : ", ");
            mmgr_line_put(args->missing, QUERY_TABLE "/");
            mmgr_line_put(args->missing, file);
        }
    }
}

// The query the words name, each checked against the files that make it whole: `--<from> --<to> [--<part>] [--dry]`, or
// `--from <from> --to <to> [--part <part>] [--dry]` as the window's job specifies it. A name the table does not contain
// whole is answered with each file missing. 1, or 0 with why given
int query_query_words(const QueryLoopsErrMsgContextWordsCfg *args)
{
    unsigned int *named = args->named_u32s;
    int *dry = args->dry_pointer;
    static const char *const s_usage = "query takes --<from> --<to> [--<part>] [--dry]: two languages, the part to run "
                                       "on, and dry to launch nothing";
    const char *plain[QUERY_WORDS_MAX];
    unsigned int plains = 0u;
    *dry = 0;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--dry", .cap = CLI_CELLS_POOL}))
        {
            *dry = 1;
        }
        else if (plains < QUERY_WORDS_MAX)
        {
            plain[plains] = args->words[at];
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
                mmgr_line_put(args->err_msg, s_usage);
                return 0;
            }
            const char **const field =
                (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--from", .cap = CLI_CELLS_POOL}))   ? &from
                : (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--to", .cap = CLI_CELLS_POOL}))   ? &to
                : (cellul.eq(&(CatenaFinitaCfg){.src = plain[at], .other = "--part", .cap = CLI_CELLS_POOL})) ? &part
                                                                                                              : NULL;
            if (field == NULL)
            {
                mmgr_line_put(args->err_msg, s_usage);
                return 0;
            }
            *field = plain[at + 1u];
        }
        const char *const supplied[3] = {from, to, part};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (supplied[at] != NULL)
            {
                args->names_letters[*named] = supplied[at];
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
                mmgr_line_put(args->err_msg, s_usage);
                return 0;
            }
            args->names_letters[*named] = plain[at] + 2;
            *named += 1u;
        }
    }
    for (unsigned int at = 0u; at < *named; at += 1u)
    {
        if ((args->names_letters[at][0] == '\0') ||
            (args->names_letters[at][EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = args->names_letters[at],
                                                .set = "/\\.")] != '\0'))
        {
            mmgr_line_put(args->err_msg, s_usage);
            return 0;
        }
    }
    if ((*named < 2u) || (*named > 3u) || (*dry && (*named != 3u)))
    {
        mmgr_line_put(args->err_msg, s_usage);
        return 0;
    }
    char missing_text[QUERY_LINE_MAX_LEN];
    VerbaLinea missing = {missing_text, sizeof(missing_text), 0ULL};
    for (unsigned int at = 0u; at < *named; at += 1u)
    {
        EMBED_CALL(query_missing_files, WriteCtx, .context = args->context_read, .name = args->names_letters[at],
                   .language = at < 2u, .missing = &missing);
    }
    char klq[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_table_by_name, WriteCtx, .context = args->context_read, .name = "Lstar.klq", .path = klq);
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = klq))
    {
        mmgr_line_put(&missing, (missing.at == 0ULL) ? "" : ", ");
        mmgr_line_put(&missing, QUERY_TABLE "/Lstar.klq");
    }
    if (missing.at != 0ULL)
    {
        mmgr_line_put(args->err_msg, "query ");
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            mmgr_line_put(args->err_msg, (at == 0u) ? "" : " ");
            mmgr_line_put(args->err_msg, args->words[at]);
        }
        mmgr_line_put(args->err_msg, ": not in the tree: ");
        mmgr_line_put_most(args->err_msg, missing_text, missing.at);
        return 0;
    }
    return 1;
}

int query_query_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    const char *names[3] = {NULL, NULL, NULL};
    unsigned int named = 0u;
    int dry = 0;
    if (!EMBED_CALL(query_loops_err_msg_context_words.query_words, QueryLoopsErrMsgContextWordsCfg,
                    .context_read = context, .words = words, .count = count, .names_letters = names,
                    .named_u32s = &named, .dry_pointer = &dry, .err_msg = err_msg))
    {
        return -1;
    }
    char from[QUERY_PATH_MAX_LEN];
    char to[QUERY_PATH_MAX_LEN];
    VerbaLinea from_line = {from, sizeof(from), 0ULL};
    mmgr_line_put(&from_line, names[0]);
    mmgr_line_put(&from_line, ".krs");
    mmgr_line_finish(&from_line);
    VerbaLinea to_line = {to, sizeof(to), 0ULL};
    mmgr_line_put(&to_line, names[1]);
    mmgr_line_put(&to_line, ".krs");
    mmgr_line_finish(&to_line);
    const char *const rulesets[2] = {from, to};
    const int code = EMBED_CALL(query_loops_context_err_msg_rulesets.bridge, QueryLoopsContextErrMsgRulesetsCfg,
                                .context = context, .rulesets = rulesets, .count = 2u, .err_msg = err_msg);
    if ((code != 0) || (named < 3u))
    {
        return code;
    }
    return EMBED_CALL(query_loops_context_err_msg_part.pairs_pair, QueryLoopsContextErrMsgPartCfg, .context = context,
                      .part = names[2], .to = names[1], .dry = dry, .seed = NULL, .trace = NULL, .err_msg = err_msg);
}
