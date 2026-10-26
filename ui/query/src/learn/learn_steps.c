// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_steps.c: each part pass run: the clock queried from the bus, a live pass, rounds, the dry pass and the
// enumeration
#include "learn_internal.h"

// `name` under the working folder into `path`
static void query_work_named(const QueryContext *context, const char *name, char *path)
{
    char work[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_LEARN_WORK, work);
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, work);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

// the timer register, found by querying through the BAR0 driver, the first register of the span that advances stored in
// the part's .ksc; where the BAR0 driver's service is not started the install is reported, elevated, and nothing is
// queried
static int query_learn_clock(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step,
                             VerbaLinea *err_msg)
{
    if (!query_bar0_driver_runs())
    {
        char script[QUERY_PATH_MAX_LEN];
        query_at(context, "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/install_bar0.ps1", script);
        mmgr_line_put(err_msg, "the BAR0 driver's service " QUERY_LISTENER " is not running: in an elevated shell, "
                               "pwsh -File ");
        mmgr_line_put(err_msg, script);
        mmgr_line_put(err_msg, " -Sign (the machine in test-signing mode)");
        return -1;
    }
    static char s_clock[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!query_context_tool(context, "orior_bus_clock", s_clock, err_msg) ||
        !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, err_msg))
    {
        return -1;
    }
    const char *const read[1] = {files->costs};
    const char *const words[6] = {s_clock, files->costs, step->word, step->base, "10", NULL};
    long long code = -1ll;
    const int ran = query_run_read(&s_run, read, 1u, err_msg) &&
                    query_context_teed(context, &s_run, "orior_bus_clock", words, &code, err_msg);
    if (!query_context_ended(context, &s_run, err_msg) || !ran)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

// a live pass: discover, turns, registers or costs, or a discover seeded from a listing
static int query_learn_pass(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step,
                            const char *writer, const char *carrier, const char *teacher, VerbaLinea *err_msg)
{
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!query_context_tool(context, "orior_measuring_stick_query", s_cross, err_msg) ||
        !query_nvdisasm(context, s_nvdisasm, err_msg) ||
        !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, err_msg))
    {
        return -1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_teacher_words(&s_words, files->machine, files, writer);
    char step_word[64];
    VerbaLinea step_line = {step_word, sizeof(step_word), 0ull};
    mmgr_line_put(&step_line, "--");
    mmgr_line_put(&step_line, (step->kind == QUERY_LEARN_PASS) ? step->word : "discover");
    mmgr_line_finish(&step_line);
    query_words_add(&s_words, step_word);
    const char *const read[7] = {files->machine,   files->costs,  files->record, files->layout,
                                 files->mnemonics, files->fields, step->base};
    const unsigned int reads = ((step->kind == QUERY_LEARN_PASS) && (step->base != NULL)) ? 7u : 6u;
    int whole = query_learn_carried(context, &s_run, 1, err_msg) && query_run_read(&s_run, read, reads, err_msg);
    if (whole && (step->kind == QUERY_LEARN_SEED))
    {
        static char s_seeds[QUERY_PATH_MAX_LEN];
        const char *const seeds[4] = {s_seeds, step->word, "--modifiers", NULL};
        long long seeded = -1ll;
        whole = query_context_tool(context, "orior_sass_listing_seeds", s_seeds, err_msg) &&
                query_context_handed(context, &s_run, "seeds.txt", seeds, &seeded, err_msg);
        if (whole && (seeded != 0ll))
        {
            mmgr_line_put(err_msg, "the seeds were not handed");
            whole = 0;
        }
        query_words_add(&s_words, "--seeds");
        query_words_add(&s_words, "seeds.txt");
    }
    if (whole && (step->kind == QUERY_LEARN_PASS) && (step->base != NULL))
    {
        whole = query_base_compiled(context, &s_run, step->base, err_msg);
    }
    long long code = -1ll;
    if (whole)
    {
        query_words_add(&s_words, "--");
        query_carrier_words(&s_words, carrier, s_cross, s_nvdisasm, files->machine, files->layout, 1);
        static const char *s_teacher[QUERY_WORDS_MOST + 2u];
        s_teacher[0] = teacher;
        memor.cpy(&(MemoriaCfg){.dst = &s_teacher[1],
                                .src = s_words.word,
                                .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word[0])});
        whole = query_context_teed(context, &s_run, "orior_khw_learn", s_teacher, &code, err_msg);
    }
    if (!query_context_ended(context, &s_run, err_msg) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

// rounds, dry or enumerate: the teacher's words before `--`, then the carrier's for rounds; `machine`, `costs` and
// `record` the files the pass reads, the working folder's copies for a dry pass and an enumeration
static int query_learn_offline(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step,
                               const char *writer, const char *carrier, const char *teacher, VerbaLinea *err_msg)
{
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    char machine[QUERY_PATH_MAX_LEN];
    char costs[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    const int copied = step->kind != QUERY_LEARN_ROUNDS;
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
        query_work_named(context, "sm_86.khw", machine);
        query_work_named(context, "sm_86.ksc", costs);
        query_work_named(context, "sm_86.kqr", record);
    }
    if (step->kind == QUERY_LEARN_DRY)
    {
        // the dry machine file, the costs and the record beside it under the working folder, each the tree's copied
        // there so that the dry pass leaves the tree's as they are
        const char *const from[3] = {files->machine, files->costs, files->record};
        const char *const to[3] = {machine, costs, record};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (!query_file_copied(from[at], to[at]))
            {
                mmgr_line_put(err_msg, from[at]);
                mmgr_line_put(err_msg, " could not be copied under the working folder");
                return -1;
            }
        }
    }
    const int needs_cross = step->kind != QUERY_LEARN_ENUMERATE;
    static QueryRun s_run;
    if ((needs_cross && (!query_context_tool(context, "orior_measuring_stick_query", s_cross, err_msg) ||
                         !query_nvdisasm(context, s_nvdisasm, err_msg))) ||
        !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, err_msg))
    {
        return -1;
    }
    const char *const read[6] = {machine, costs, record, files->layout, files->mnemonics, files->fields};
    int whole = query_learn_carried(context, &s_run, 0, err_msg) && query_run_read(&s_run, read, 6u, err_msg);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_teacher_words(&s_words, machine, files, writer);
    char rounds[32];
    VerbaLinea rounds_line = {rounds, sizeof(rounds), 0ull};
    mmgr_line_decimal(&rounds_line, step->rounds, 0u);
    mmgr_line_finish(&rounds_line);
    if (step->kind == QUERY_LEARN_ENUMERATE)
    {
        const char *const forms = (query_learn_lines(&s_run, "forms_work.txt") != 0u) ? "forms_work.txt" : "forms.txt";
        const unsigned int seeded = query_learn_lines(&s_run, forms);
        query_words_add(&s_words, "--position");
        query_words_add(&s_words, step->word);
        if (seeded != 0u)
        {
            query_words_add(&s_words, "--forms");
            query_words_add(&s_words, forms);
            char line[128];
            VerbaLinea print_line = {line, sizeof(line), 0ull};
            mmgr_line_put(&print_line, "  seeding from the ");
            mmgr_line_decimal(&print_line, seeded, 0u);
            mmgr_line_put(&print_line, " forms in ");
            mmgr_line_put(&print_line, forms);
            if (mmgr_line_finish(&print_line) != 0ull)
            {
                query_print(context, line);
            }
        }
        query_words_add(&s_words, "--");
        query_words_add(&s_words, "dry");
    }
    else
    {
        query_words_add(&s_words, rounds);
        query_words_add(&s_words, "--");
        if (step->kind == QUERY_LEARN_DRY)
        {
            query_words_add(&s_words, "dry");
        }
        query_carrier_words(&s_words, carrier, s_cross, s_nvdisasm, machine, files->layout, 0);
    }
    static const char *s_teacher[QUERY_WORDS_MOST + 2u];
    s_teacher[0] = teacher;
    memor.cpy(&(MemoriaCfg){
        .dst = &s_teacher[1], .src = s_words.word, .bytes = (size_t)(s_words.count + 1u) * sizeof(s_words.word[0])});
    long long code = -1ll;
    whole = whole && query_context_teed(context, &s_run, "orior_khw_learn", s_teacher, &code, err_msg);
    if (whole && (step->kind != QUERY_LEARN_ROUNDS))
    {
        if (code != 0ll)
        {
            mmgr_line_put(err_msg, (step->kind == QUERY_LEARN_DRY) ? "the protocol did not run dry"
                                                                   : "the enumerate did not run");
            whole = 0;
        }
        const unsigned int questions = whole ? query_learn_lines(&s_run, "questions.txt") : 0u;
        if (whole && (questions == 0u))
        {
            mmgr_line_put(err_msg, (step->kind == QUERY_LEARN_DRY) ? "the protocol put no question"
                                                                   : "the enumerate put no question");
            whole = 0;
        }
        char line[QUERY_PATH_MAX_LEN + 128u];
        VerbaLinea print_line = {line, sizeof(line), 0ull};
        if (whole && (step->kind == QUERY_LEARN_DRY))
        {
            // the gate reads the list against the tree's machine file, whose forms stand whatever the dry pass wrote
            mmgr_line_put(&print_line, "  the protocol put ");
            mmgr_line_decimal(&print_line, questions, 0u);
            mmgr_line_put(&print_line, " questions; each checked by cubin_safe with the part taken out:");
            if (mmgr_line_finish(&print_line) != 0ull)
            {
                query_print(context, line);
            }
            const char *const carried[7] = {carrier,         "--dry", files->machine, files->layout, "--list",
                                            "questions.txt", NULL};
            long long carried_code = -1ll;
            whole = query_context_teed(context, &s_run, "orior_cubin_run", carried, &carried_code, err_msg);
            if (whole && (carried_code != 0ll))
            {
                mmgr_line_put(err_msg, "the carrier did not read the list");
                whole = 0;
            }
            if (whole)
            {
                query_verdicts_print(context, &s_run);
            }
        }
        else if (whole)
        {
            mmgr_line_put(&print_line, "  the place");
            mmgr_line_put(&print_line, step->word);
            mmgr_line_put(&print_line, "'s questions: questions.txt of ");
            mmgr_line_put(&print_line, s_run.writer.qry);
            mmgr_line_put(&print_line, ", ");
            mmgr_line_decimal(&print_line, questions, 0u);
            mmgr_line_put(&print_line, " of them");
            if (mmgr_line_finish(&print_line) != 0ull)
            {
                query_print(context, line);
            }
            query_print(context, "  cross-check them: orior_cli protocol cross [--base <cu>]");
        }
        code = 0ll;
    }
    if (!query_context_ended(context, &s_run, err_msg) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

int query_learn_run(QueryContext *context, const char *const *words, unsigned int count, VerbaLinea *err_msg)
{
    QueryLearnStep step;
    if (!query_learn_step_read(words, count, &step, err_msg))
    {
        return -1;
    }
    static QueryFiles s_files;
    query_files_of(context->root, "sm_86", &s_files);
    static char s_writer[QUERY_PATH_MAX_LEN];
    static char s_carrier[QUERY_PATH_MAX_LEN];
    static char s_teacher[QUERY_PATH_MAX_LEN];
    char work[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_LEARN_WORK, work);
    if (!query_context_tool(context, "orior_khw_machine_write", s_writer, err_msg) ||
        !query_context_tool(context, "orior_cubin_run", s_carrier, err_msg) ||
        !query_context_tool(context, "orior_khw_learn", s_teacher, err_msg))
    {
        return -1;
    }
    if (!query_folder_made(work))
    {
        mmgr_line_put(err_msg, work);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    if (step.kind == QUERY_LEARN_CLOCK)
    {
        return query_learn_clock(context, &s_files, &step, err_msg);
    }
    if ((step.kind == QUERY_LEARN_PASS) || (step.kind == QUERY_LEARN_SEED))
    {
        return query_learn_pass(context, &s_files, &step, s_writer, s_carrier, s_teacher, err_msg);
    }
    return query_learn_offline(context, &s_files, &step, s_writer, s_carrier, s_teacher, err_msg);
}
