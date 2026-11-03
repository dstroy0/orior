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
    const char *const words[6] = {s_clock, args->files->costs, args->step->word, args->step->base, "10", NULL};
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
    mmgr_line_put(&step_line, (args->step->kind == QUERY_LEARN_PASS) ? args->step->word : "discover");
    mmgr_line_finish(&step_line);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = step_word);
    const char *const read[7] = {files->machine,   files->costs,  files->record,   files->layout,
                                 files->mnemonics, files->fields, args->step->base};
    const unsigned int reads = ((args->step->kind == QUERY_LEARN_PASS) && (args->step->base != NULL)) ? 7u : 6u;
    int whole = EMBED_CALL(query_learn_err_msg_internal.carry, QueryLearnErrMsgCtx, .context = context,
                           .run_out = &s_run, .lessons = 1, .err_msg = err_msg) &&
                EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = reads, .err_msg = err_msg);
    if (whole && (args->step->kind == QUERY_LEARN_SEED))
    {
        static char s_seeds[QUERY_PATH_MAX_LEN];
        const char *const seeds[4] = {s_seeds, args->step->word, "--modifiers", NULL};
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
                   .word = "--seeds");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "seeds.txt");
    }
    if (whole && (args->step->kind == QUERY_LEARN_PASS) && (args->step->base != NULL))
    {
        whole = EMBED_CALL(query_loops_context_err_msg.base_compile, QueryLoopsContextErrMsgCfg, .context = context,
                           .run = &s_run, .base = args->step->base, .err_msg = err_msg);
    }
    long long code = -1LL;
    if (whole)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "--");
        EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg,
                   .words_words_pointer = &s_words, .transport = transport, .cross = s_cross, .nvdisasm = s_nvdisasm,
                   .machine = files->machine, .layout = files->layout, .lessons = 1);
        static const char *s_teacher[QUERY_WORDS_MAX + 2u];
        s_teacher[0] = args->teacher;
        memor.cpy(&(MemoriaCfg){.dst = &s_teacher[1],
                                .src = s_words.word,
                                .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word[0])});
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

// rounds, dry or enumerate: the teacher's words before `--`, then the carrier's for rounds; `machine`, `costs` and
// `record` the files the pass reads, the working folder's copies for a dry pass and an enumeration
EMBED_INLINE int query_learn_offline(const LearnStepsCtx *args)
{
    QueryContext *context = args->context;
    const QueryFiles *files = args->files;
    const char *writer = args->writer;
    const char *transport = args->transport;
    VerbaLinea *err_msg = args->err_msg;
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    char machine[QUERY_PATH_MAX_LEN];
    char costs[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    const int copied = args->step->kind != QUERY_LEARN_ROUNDS;
    memor.cpy(
        &(MemoriaCfg){.dst = machine,
                      .src = files->machine,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->machine, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = costs,
                            .src = files->costs,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->costs, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = record,
                            .src = files->record,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = files->record, .cap = CLI_CELLS_POOL}) + 1u});
    if (copied)
    {
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = context, .name = "sm_86.khw", .path = machine);
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = context, .name = "sm_86.ksc", .path = costs);
        EMBED_CALL(query_work_by_name, LearnStepsCtx, .context_read = context, .name = "sm_86.kqr", .path = record);
    }
    if (args->step->kind == QUERY_LEARN_DRY)
    {
        // the dry machine file, the costs and the record beside it under the working folder, each the tree's copied
        // there so that the dry pass leaves the tree's as they are
        const char *const from[3] = {files->machine, files->costs, files->record};
        const char *const to[3] = {machine, costs, record};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (!EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from[at], .to = to[at]))
            {
                mmgr_line_put(err_msg, from[at]);
                mmgr_line_put(err_msg, " could not be copied under the working folder");
                return -1;
            }
        }
    }
    const int needs_cross = args->step->kind != QUERY_LEARN_ENUMERATE;
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
    int whole = EMBED_CALL(query_learn_err_msg_internal.carry, QueryLearnErrMsgCtx, .context = context,
                           .run_out = &s_run, .lessons = 0, .err_msg = err_msg) &&
                EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = 6u, .err_msg = err_msg);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_learn_words_words_internal.teacher_words, QueryLearnWordsWordsCtx, .words_words_pointer = &s_words,
               .machine = machine, .files = files, .writer = writer);
    char rounds[32];
    VerbaLinea rounds_line = {rounds, sizeof(rounds), 0ULL};
    mmgr_line_decimal(&rounds_line, args->step->rounds, 0u);
    mmgr_line_finish(&rounds_line);
    if (args->step->kind == QUERY_LEARN_ENUMERATE)
    {
        const char *const forms =
            (EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = &s_run, .name = "forms_work.txt") != 0u)
                ? "forms_work.txt"
                : "forms.txt";
        const unsigned int seeded = EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = &s_run, .name = forms);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "--position");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = args->step->word);
        if (seeded != 0u)
        {
            EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                       .word = "--forms");
            EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                       .word = forms);
            char line[128];
            VerbaLinea print_line = {line, sizeof(line), 0ULL};
            mmgr_line_put(&print_line, "  seeding from the ");
            mmgr_line_decimal(&print_line, seeded, 0u);
            mmgr_line_put(&print_line, " forms in ");
            mmgr_line_put(&print_line, forms);
            if (mmgr_line_finish(&print_line) != 0ULL)
            {
                EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
            }
        }
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "--");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "dry");
    }
    else
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = rounds);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = "--");
        if (args->step->kind == QUERY_LEARN_DRY)
        {
            EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                       .word = "dry");
        }
        EMBED_CALL(query_loops_words_words_transport.transport_words, QueryLoopsWordsWordsTransportCfg,
                   .words_words_pointer = &s_words, .transport = transport, .cross = s_cross, .nvdisasm = s_nvdisasm,
                   .machine = machine, .layout = files->layout, .lessons = 0);
    }
    static const char *s_teacher[QUERY_WORDS_MAX + 2u];
    s_teacher[0] = args->teacher;
    memor.cpy(&(MemoriaCfg){
        .dst = &s_teacher[1], .src = s_words.word, .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word[0])});
    long long code = -1LL;
    whole = whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                .context = context, .run = &s_run, .name = "orior_khw_learn", .words = s_teacher,
                                .code = &code, .err_msg = err_msg);
    if (whole && (args->step->kind != QUERY_LEARN_ROUNDS))
    {
        if (code != 0LL)
        {
            mmgr_line_put(err_msg, (args->step->kind == QUERY_LEARN_DRY) ? "the protocol did not run dry"
                                                                         : "the enumerate did not run");
            whole = 0;
        }
        const unsigned int questions =
            whole ? EMBED_CALL(query_learn_internal.lines, QueryLearnCtx, .run = &s_run, .name = "questions.txt") : 0u;
        if (whole && (questions == 0u))
        {
            mmgr_line_put(err_msg, (args->step->kind == QUERY_LEARN_DRY) ? "the protocol put no question"
                                                                         : "the enumerate put no question");
            whole = 0;
        }
        char line[QUERY_PATH_MAX_LEN + 128u];
        VerbaLinea print_line = {line, sizeof(line), 0ULL};
        if (whole && (args->step->kind == QUERY_LEARN_DRY))
        {
            // the gate reads the list against the tree's machine file, whose forms stand whatever the dry pass wrote
            mmgr_line_put(&print_line, "  the protocol put ");
            mmgr_line_decimal(&print_line, questions, 0u);
            mmgr_line_put(&print_line, " questions; each checked by cubin_safe with the part taken out:");
            if (mmgr_line_finish(&print_line) != 0ULL)
            {
                EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
            }
            const char *const carried[7] = {transport,       "--dry", files->machine, files->layout, "--list",
                                            "questions.txt", NULL};
            long long transferred_code = -1LL;
            whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                               .context = context, .run = &s_run, .name = "orior_cubin_run", .words = carried,
                               .code = &transferred_code, .err_msg = err_msg);
            if (whole && (transferred_code != 0LL))
            {
                mmgr_line_put(err_msg, "the carrier did not read the list");
                whole = 0;
            }
            if (whole)
            {
                EMBED_CALL(query_learn_run_internal.verdicts_print, QueryLearnRunCtx, .context = context,
                           .run = &s_run);
            }
        }
        else if (whole)
        {
            mmgr_line_put(&print_line, "  the place");
            mmgr_line_put(&print_line, args->step->word);
            mmgr_line_put(&print_line, "'s questions: questions.txt of ");
            mmgr_line_put(&print_line, s_run.writer.qry);
            mmgr_line_put(&print_line, ", ");
            mmgr_line_decimal(&print_line, questions, 0u);
            mmgr_line_put(&print_line, " of them");
            if (mmgr_line_finish(&print_line) != 0ULL)
            {
                EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
            }
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                       .text = "  cross-check them: orior_cli protocol cross [--base <cu>]");
        }
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
    if (step.kind == QUERY_LEARN_CLOCK)
    {
        return EMBED_CALL(query_learn_clock, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                          .err_msg = err_msg);
    }
    if ((step.kind == QUERY_LEARN_PASS) || (step.kind == QUERY_LEARN_SEED))
    {
        return EMBED_CALL(query_learn_pass, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                          .writer = s_writer, .transport = s_transport, .teacher = s_teacher, .err_msg = err_msg);
    }
    return EMBED_CALL(query_learn_offline, LearnStepsCtx, .context = context, .files = &s_files, .step = &step,
                      .writer = s_writer, .transport = s_transport, .teacher = s_teacher, .err_msg = err_msg);
}
