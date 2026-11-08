// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_steps.c: each part pass run: the clock queried from the bus, a live pass, rounds, the dry pass and the
// enumeration
#include "learn_internal.h"

typedef struct
{
    const QueryContext *context_read;
    const char *name;
    char *path;
    QueryContext *context;
    const QueryFiles *files;
    const QueryLearnStep *step;
    VerbaLinea *err_msg;
    const char *writer;
    const char *transport;
    const char *teacher;
    char *machine;
    char *costs;
    char *record;
    QueryRun *run;
    QueryWords *words;
    const char *cross;
    const char *nvdisasm;
    long long code;
    unsigned int questions;
} LearnStepsCtx;

// `name` under the working folder into `path`
EMBED_INLINE void query_work_by_name(const LearnStepsCtx *args)
{
    const QueryContext *context = args->context_read;
    char *path = args->path;
    char work[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_LEARN_WORK, .path_out = work);
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, work);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// the timer register, found by querying through the BAR0 driver, the first register of the span that advances stored in
// the part's .ksc; where the BAR0 driver's service is not started the install is reported, elevated, and nothing is
// queried
EMBED_INLINE int query_learn_clock(const LearnStepsCtx *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    if (!query_bar0_driver_runs())
    {
        char script[QUERY_PATH_MAX_LEN];
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
                   .under = "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/install_bar0.ps1",
                   .path_out = script);
        mmgr_line_put(err_msg, "the BAR0 driver's service " QUERY_LISTENER " is not running: in an elevated shell, "
                               "pwsh -File ");
        mmgr_line_put(err_msg, script);
        mmgr_line_put(err_msg, " -Sign (the machine in test-signing mode)");
        return -1;
    }
    static char s_clock[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_bus_clock", .binary = s_clock, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_LEARN_FOLDER, .err_msg = err_msg))
    {
        return -1;
    }
    const char *const read[1] = {args->files->costs};
    const char *const words[6] = {s_clock, args->files->costs, args->step->word_text, args->step->base, "10", NULL};
    long long code = -1LL;
    const int run_result = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                                      .paths_letters = read, .count = 1u, .err_msg = err_msg) &&
                           EMBED_CALL(query_loops_context_err_msg_name_run.context_teed,
                                      QueryLoopsContextErrMsgNameRunCfg, .context = context, .run = &s_run,
                                      .name = "orior_bus_clock", .words = words, .code = &code, .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !run_result)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

// a live pass: discover, turns, registers or costs, or a discover seeded from a listing
EMBED_INLINE int query_learn_pass(const LearnStepsCtx *args)
{
    QueryContext *context = args->context;
    const QueryFiles *files = args->files;
    const char *writer = args->writer;
    const char *transport = args->transport;
    VerbaLinea *err_msg = args->err_msg;
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_query", .binary = s_cross, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg.nvdisasm, QueryLoopsErrMsgCfg, .context_read = context, .path = s_nvdisasm,
                    .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_LEARN_FOLDER, .err_msg = err_msg))
    {
        return -1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_learn_words_words_internal.teacher_words, QueryLearnWordsWordsCtx, .words_words_pointer = &s_words,
               .machine = files->machine, .files = files, .writer = writer);
    char step_word[64];
    VerbaLinea step_line = {step_word, sizeof(step_word), 0ULL};
    mmgr_line_put(&step_line, "--");
    mmgr_line_put(&step_line, (args->step->type == QUERY_LEARN_PASS) ? args->step->word_text : "discover");
    mmgr_line_finish(&step_line);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = step_word);
    const char *const read[7] = {files->machine,   files->costs,  files->record,   files->layout,
                                 files->mnemonics, files->fields, args->step->base};
    const unsigned int reads = ((args->step->type == QUERY_LEARN_PASS) && (args->step->base != NULL)) ? 7u : 6u;
    int whole = EMBED_CALL(query_learn_err_msg_internal.files_add, QueryLearnErrMsgCtx, .context = context,
                           .run_out = &s_run, .lessons = 1, .err_msg = err_msg) &&
                EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = reads, .err_msg = err_msg);
    if (whole && (args->step->type == QUERY_LEARN_SEED))
    {
        static char s_seeds[QUERY_PATH_MAX_LEN];
        const char *const seeds[4] = {s_seeds, args->step->word_text, "--modifiers", NULL};
        long long seeded = -1LL;
        whole =
            EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                       .name = "orior_sass_listing_seeds", .binary = s_seeds, .err_msg = err_msg) &&
            EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                       .context = context, .run = &s_run, .name = "seeds.txt", .words = seeds, .code = &seeded,
                       .err_msg = err_msg);
        if (whole && (seeded != 0LL))
        {
            mmgr_line_put(err_msg, "the seeds were not handed");
            whole = 0;
        }
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "--seeds");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "seeds.txt");
    }
    if (whole && (args->step->type == QUERY_LEARN_PASS) && (args->step->base != NULL))
    {
        whole = EMBED_CALL(query_loops_context_err_msg.base_compile, QueryLoopsContextErrMsgCfg, .context = context,
                           .run = &s_run, .base = args->step->base, .err_msg = err_msg);
    }
    long long code = -1LL;
    if (whole)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "--");
        EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg,
                   .words_words_pointer = &s_words, .transport = transport, .cross = s_cross, .nvdisasm = s_nvdisasm,
                   .machine = files->machine, .layout = files->layout, .lessons = 1);
        static const char *s_teacher[QUERY_WORDS_MAX + 2u];
        s_teacher[0] = args->teacher;
        memor.cpy(&(MemoriaCfg){.dst = &s_teacher[1],
                                .src = s_words.word_text,
                                .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word_text[0])});
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "orior_khw_learn", .words = s_teacher,
                           .code = &code, .err_msg = err_msg);
    }
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

// the files an offline step reads into `machine`, `costs` and `record`: the tree's for rounds, else the working
// folder's, the tree's copied there for a dry pass; 1, or 0 with why given
EMBED_INLINE int query_offline_files(const LearnStepsCtx *args)
{
    const QueryFiles *const files = args->files;
    memor.cpy(
        &(MemoriaCfg){.dst = args->machine,
                      .src = files->machine,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->machine, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->costs,
                            .src = files->costs,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->costs, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->record,
                            .src = files->record,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->record, .cap = CLI_POOL_BYTES}) + 1u});
    if (args->step->type != QUERY_LEARN_ROUNDS)
    {
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = args->context, .name = "sm_86.khw",
                   .path = args->machine);
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = args->context, .name = "sm_86.ksc",
                   .path = args->costs);
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = args->context, .name = "sm_86.kqr",
                   .path = args->record);
    }
    if (args->step->type != QUERY_LEARN_DRY)
    {
        return 1;
    }
    // the dry machine file, the costs and the record beside it under the working folder, each the tree's copied
    // there so that the dry pass leaves the tree's as they are
    const char *const from[3] = {files->machine, files->costs, files->record};
    const char *const to[3] = {args->machine, args->costs, args->record};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if (!EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from[at], .to = to[at]))
        {
            mmgr_line_put(args->err_msg, from[at]);
            mmgr_line_put(args->err_msg, " could not be copied under the working folder");
            return 0;
        }
    }
    return 1;
}

// the teacher's words for an enumeration put after its own in `words`: the place, the forms of `run` it is seeded from
// where it holds any, told, then `--` and dry
EMBED_INLINE void query_enumerate_words(const LearnStepsCtx *args)
{
    QueryWords *const words = args->words;
    const char *const forms =
        (EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = args->run, .name = "forms_work.txt") != 0u)
            ? "forms_work.txt"
            : "forms.txt";
    const unsigned int seeded = EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = args->run, .name = forms);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--position");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->step->word_text);
    if (seeded != 0u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = "--forms");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = forms);
        char line[128];
        VerbaLinea print_line = {line, sizeof(line), 0ULL};
        mmgr_line_put(&print_line, "  seeding from the ");
        mmgr_line_decimal(&print_line, seeded, 0u);
        mmgr_line_put(&print_line, " forms in ");
        mmgr_line_put(&print_line, forms);
        if (mmgr_line_finish(&print_line) != 0ULL)
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = line);
        }
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "dry");
}

// the teacher's words for rounds or a dry pass put after its own in `words`: the rounds, `--`, dry for a dry pass, and
// the carrier's, `cross`, `nvdisasm` and the `machine` file among them
EMBED_INLINE void query_rounds_words(const LearnStepsCtx *args)
{
    QueryWords *const words = args->words;
    char rounds[32];
    VerbaLinea rounds_line = {rounds, sizeof(rounds), 0ULL};
    mmgr_line_decimal(&rounds_line, args->step->rounds, 0u);
    mmgr_line_finish(&rounds_line);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = rounds);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--");
    if (args->step->type == QUERY_LEARN_DRY)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = "dry");
    }
    EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg,
               .words_words_pointer = words, .transport = args->transport, .cross = args->cross,
               .nvdisasm = args->nvdisasm, .machine = args->machine, .layout = args->files->layout, .lessons = 0);
}

// the `questions` a dry pass put, told, carried by the carrier from their list with the part taken out, and the
// verdicts printed; 1, or 0 with why given
EMBED_INLINE int query_dry_carried(const LearnStepsCtx *args)
{
    const QueryFiles *const files = args->files;
    char line[QUERY_PATH_MAX_LEN + 128u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    // the gate reads the list against the tree's machine file, whose forms stand whatever the dry pass wrote
    mmgr_line_put(&print_line, "  the protocol put ");
    mmgr_line_decimal(&print_line, args->questions, 0u);
    mmgr_line_put(&print_line, " questions; each checked by cubin_safe with the part taken out:");
    if (mmgr_line_finish(&print_line) != 0ULL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = line);
    }
    const char *const carried[7] = {args->transport, "--dry", files->machine, files->layout, "--list",
                                    "questions.txt", NULL};
    long long transferred_code = -1LL;
    int whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = args->context, .run = args->run, .name = "orior_cubin_run", .words = carried,
                           .code = &transferred_code, .err_msg = args->err_msg);
    if (whole && (transferred_code != 0LL))
    {
        mmgr_line_put(args->err_msg, "the carrier did not read the list");
        whole = 0;
    }
    if (whole)
    {
        EMBED_CALL(query_learn_run_internal.verdicts_print, QueryLearnRunCtx, .context = args->context,
                   .run = args->run);
    }
    return whole;
}

// a dry pass or an enumeration checked once the teacher ended with `code`: it ran and put questions, a dry pass's
// carried and an enumeration's place told; 1, or 0 with why given
EMBED_INLINE int query_offline_checked(const LearnStepsCtx *args)
{
    const int dry = args->step->type == QUERY_LEARN_DRY;
    if (args->code != 0LL)
    {
        mmgr_line_put(args->err_msg, dry ? "the protocol did not run dry" : "the enumerate did not run");
        return 0;
    }
    const unsigned int questions =
        EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = args->run, .name = "questions.txt");
    if (questions == 0u)
    {
        mmgr_line_put(args->err_msg, dry ? "the protocol put no question" : "the enumerate put no question");
        return 0;
    }
    if (dry)
    {
        return EMBED_CALL(query_dry_carried, LearnStepsCtx, .context = args->context, .files = args->files,
                          .run = args->run, .transport = args->transport, .questions = questions,
                          .err_msg = args->err_msg);
    }
    char line[QUERY_PATH_MAX_LEN + 128u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, "  the place");
    mmgr_line_put(&print_line, args->step->word_text);
    mmgr_line_put(&print_line, "'s questions: questions.txt of ");
    mmgr_line_put(&print_line, args->run->writer.qry);
    mmgr_line_put(&print_line, ", ");
    mmgr_line_decimal(&print_line, questions, 0u);
    mmgr_line_put(&print_line, " of them");
    if (mmgr_line_finish(&print_line) != 0ULL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = line);
    }
    EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context,
               .text = "  cross-check them: orior_cli protocol cross [--base <cu>]");
    return 1;
}

// rounds, dry or enumerate: the teacher's words before `--`, then the carrier's for rounds; `machine`, `costs` and
// `record` the files the pass reads, the working folder's copies for a dry pass and an enumeration
EMBED_INLINE int query_learn_offline(const LearnStepsCtx *args)
{
    QueryContext *context = args->context;
    const QueryFiles *files = args->files;
    const char *writer = args->writer;
    VerbaLinea *err_msg = args->err_msg;
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    char machine[QUERY_PATH_MAX_LEN];
    char costs[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_offline_files, LearnStepsCtx, .context = context, .files = files, .step = args->step,
                    .machine = machine, .costs = costs, .record = record, .err_msg = err_msg))
    {
        return -1;
    }
    const int needs_cross = args->step->type != QUERY_LEARN_ENUMERATE;
    static QueryRun s_run;
    if ((needs_cross &&
         (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                      .name = "orior_measuring_stick_query", .binary = s_cross, .err_msg = err_msg) ||
          !EMBED_CALL(query_loops_err_msg.nvdisasm, QueryLoopsErrMsgCfg, .context_read = context, .path = s_nvdisasm,
                      .err_msg = err_msg))) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_LEARN_FOLDER, .err_msg = err_msg))
    {
        return -1;
    }
    const char *const read[6] = {machine, costs, record, files->layout, files->mnemonics, files->fields};
    int whole = EMBED_CALL(query_learn_err_msg_internal.files_add, QueryLearnErrMsgCtx, .context = context,
                           .run_out = &s_run, .lessons = 0, .err_msg = err_msg) &&
                EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = 6u, .err_msg = err_msg);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_learn_words_words_internal.teacher_words, QueryLearnWordsWordsCtx, .words_words_pointer = &s_words,
               .machine = machine, .files = files, .writer = writer);
    if (args->step->type == QUERY_LEARN_ENUMERATE)
    {
        EMBED_CALL(query_enumerate_words, LearnStepsCtx, .context = context, .step = args->step, .run = &s_run,
                   .words = &s_words);
    }
    else
    {
        EMBED_CALL(query_rounds_words, LearnStepsCtx, .files = files, .step = args->step, .words = &s_words,
                   .transport = args->transport, .cross = s_cross, .nvdisasm = s_nvdisasm, .machine = machine);
    }
    static const char *s_teacher[QUERY_WORDS_MAX + 2u];
    s_teacher[0] = args->teacher;
    memor.cpy(&(MemoriaCfg){.dst = &s_teacher[1],
                            .src = s_words.word_text,
                            .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word_text[0])});
    long long code = -1LL;
    whole = whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                .context = context, .run = &s_run, .name = "orior_khw_learn", .words = s_teacher,
                                .code = &code, .err_msg = err_msg);
    if (whole && (args->step->type != QUERY_LEARN_ROUNDS))
    {
        whole = EMBED_CALL(query_offline_checked, LearnStepsCtx, .context = context, .files = files, .step = args->step,
                           .run = &s_run, .transport = args->transport, .code = code, .err_msg = err_msg);
        code = 0LL;
    }
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

int query_learn_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    QueryLearnStep step;
    if (!EMBED_CALL(query_learn_words_internal.step_read, QueryLearnWordsCtx, .words = words, .count = count,
                    .step = &step, .err_msg = err_msg))
    {
        return -1;
    }
    static QueryFiles s_files;
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = context->root, .part = "sm_86",
               .files = &s_files);
    static char s_writer[QUERY_PATH_MAX_LEN];
    static char s_transport[QUERY_PATH_MAX_LEN];
    static char s_teacher[QUERY_PATH_MAX_LEN];
    char work[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_LEARN_WORK, .path_out = work);
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_khw_machine_write", .binary = s_writer, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_cubin_run", .binary = s_transport, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_khw_learn", .binary = s_teacher, .err_msg = err_msg))
    {
        return -1;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = work))
    {
        mmgr_line_put(err_msg, work);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    if (step.type == QUERY_LEARN_CLOCK)
    {
        return EMBED_CALL(query_learn_clock, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                          .err_msg = err_msg);
    }
    if ((step.type == QUERY_LEARN_PASS) || (step.type == QUERY_LEARN_SEED))
    {
        return EMBED_CALL(query_learn_pass, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                          .writer = s_writer, .transport = s_transport, .teacher = s_teacher, .err_msg = err_msg);
    }
    return EMBED_CALL(query_learn_offline, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                      .writer = s_writer, .transport = s_transport, .teacher = s_teacher, .err_msg = err_msg);
}
