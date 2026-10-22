// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_steps.c: each part pass run: the clock asked of the bus, a live pass, rounds, the dry pass and the enumeration
#include "learn_internal.h"

// `name` under the working folder into `path`
static void query_work_named(const QueryContext *context, const char *name, char *path)
{
    char work[QUERY_PATH_LONGEST];
    query_at(context, QUERY_LEARN_WORK, work);
    CliLine line = {path, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, work);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// the timer register, found by asking through the BAR0 driver, the first register of the span that advances kept in
// the part's .ksc; where the BAR0 driver's service is not started the install is said, elevated, and nothing is asked
static int query_learn_clock(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step, CliLine *said)
{
    if (!query_bar0_driver_runs())
    {
        char script[QUERY_PATH_LONGEST];
        query_at(context, "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/install_bar0.ps1", script);
        cli_line_text(said, "the BAR0 driver's service " QUERY_LISTENER " is not running: in an elevated shell, "
                            "pwsh -File ");
        cli_line_text(said, script);
        cli_line_text(said, " -Sign (the machine in test-signing mode)");
        return -1;
    }
    static char s_clock[QUERY_PATH_LONGEST];
    static QueryRun s_run;
    if (!query_context_tool(context, "orior_bus_clock", s_clock, said) ||
        !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, said))
    {
        return -1;
    }
    const char *const read[1] = {files->costs};
    const char *const words[6] = {s_clock, files->costs, step->word, step->base, "10", NULL};
    long long code = -1ll;
    const int ran =
        query_run_read(&s_run, read, 1u, said) && query_context_teed(context, &s_run, "orior_bus_clock", words, &code, said);
    if (!query_context_ended(context, &s_run, said) || !ran)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// a live pass: discover, turns, registers or costs, or a discover seeded from a listing
static int query_learn_pass(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step,
                            const char *writer, const char *carrier, const char *teacher, CliLine *said)
{
    static char s_cross[QUERY_PATH_LONGEST];
    static char s_nvdisasm[QUERY_PATH_LONGEST];
    static QueryRun s_run;
    if (!query_context_tool(context, "orior_measuring_stick_query", s_cross, said) ||
        !query_nvdisasm(context, s_nvdisasm, said) || !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, said))
    {
        return -1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_teacher_words(&s_words, files->machine, files, writer);
    char step_word[64];
    CliLine step_line = {step_word, sizeof(step_word), 0ull};
    cli_line_text(&step_line, "--");
    cli_line_text(&step_line, (step->kind == QUERY_LEARN_PASS) ? step->word : "discover");
    cli_line_closed(&step_line);
    query_words_add(&s_words, step_word);
    const char *const read[7] = {files->machine,   files->costs,  files->record, files->layout,
                                 files->mnemonics, files->fields, step->base};
    const unsigned int reads = ((step->kind == QUERY_LEARN_PASS) && (step->base != NULL)) ? 7u : 6u;
    int whole = query_learn_carried(context, &s_run, 1, said) && query_run_read(&s_run, read, reads, said);
    if (whole && (step->kind == QUERY_LEARN_SEED))
    {
        static char s_seeds[QUERY_PATH_LONGEST];
        const char *const seeds[4] = {s_seeds, step->word, "--modifiers", NULL};
        long long seeded = -1ll;
        whole = query_context_tool(context, "orior_sass_listing_seeds", s_seeds, said) &&
                query_context_handed(context, &s_run, "seeds.txt", seeds, &seeded, said);
        if (whole && (seeded != 0ll))
        {
            cli_line_text(said, "the seeds were not handed");
            whole = 0;
        }
        query_words_add(&s_words, "--seeds");
        query_words_add(&s_words, "seeds.txt");
    }
    if (whole && (step->kind == QUERY_LEARN_PASS) && (step->base != NULL))
    {
        whole = query_base_compiled(context, &s_run, step->base, said);
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
        whole = query_context_teed(context, &s_run, "orior_khw_learn", s_teacher, &code, said);
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// rounds, dry or enumerate: the teacher's words before `--`, then the carrier's for rounds; `machine`, `costs` and
// `record` the files the pass reads, the working folder's copies for a dry pass and an enumeration
static int query_learn_offline(QueryContext *context, const QueryFiles *files, const QueryLearnStep *step,
                               const char *writer, const char *carrier, const char *teacher, CliLine *said)
{
    static char s_cross[QUERY_PATH_LONGEST];
    static char s_nvdisasm[QUERY_PATH_LONGEST];
    char machine[QUERY_PATH_LONGEST];
    char costs[QUERY_PATH_LONGEST];
    char record[QUERY_PATH_LONGEST];
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
                cli_line_text(said, from[at]);
                cli_line_text(said, " could not be copied under the working folder");
                return -1;
            }
        }
    }
    const int needs_cross = step->kind != QUERY_LEARN_ENUMERATE;
    static QueryRun s_run;
    if ((needs_cross && (!query_context_tool(context, "orior_measuring_stick_query", s_cross, said) ||
                         !query_nvdisasm(context, s_nvdisasm, said))) ||
        !query_context_started(context, &s_run, QUERY_LEARN_FOLDER, said))
    {
        return -1;
    }
    const char *const read[6] = {machine, costs, record, files->layout, files->mnemonics, files->fields};
    int whole = query_learn_carried(context, &s_run, 0, said) && query_run_read(&s_run, read, 6u, said);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_teacher_words(&s_words, machine, files, writer);
    char rounds[32];
    CliLine rounds_line = {rounds, sizeof(rounds), 0ull};
    cli_line_decimal(&rounds_line, step->rounds, 0u);
    cli_line_closed(&rounds_line);
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
            CliLine said_line = {line, sizeof(line), 0ull};
            cli_line_text(&said_line, "  seeding from the ");
            cli_line_decimal(&said_line, seeded, 0u);
            cli_line_text(&said_line, " forms in ");
            cli_line_text(&said_line, forms);
            if (cli_line_closed(&said_line) != 0ull)
            {
                query_said(context, line);
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
    whole = whole && query_context_teed(context, &s_run, "orior_khw_learn", s_teacher, &code, said);
    if (whole && (step->kind != QUERY_LEARN_ROUNDS))
    {
        if (code != 0ll)
        {
            cli_line_text(said, (step->kind == QUERY_LEARN_DRY) ? "the protocol did not run dry"
                                                                : "the enumerate did not run");
            whole = 0;
        }
        const unsigned int questions = whole ? query_learn_lines(&s_run, "questions.txt") : 0u;
        if (whole && (questions == 0u))
        {
            cli_line_text(said, (step->kind == QUERY_LEARN_DRY) ? "the protocol put no question"
                                                                : "the enumerate put no question");
            whole = 0;
        }
        char line[QUERY_PATH_LONGEST + 128u];
        CliLine said_line = {line, sizeof(line), 0ull};
        if (whole && (step->kind == QUERY_LEARN_DRY))
        {
            // the gate reads the list against the tree's machine file, whose forms stand whatever the dry pass wrote
            cli_line_text(&said_line, "  the protocol put ");
            cli_line_decimal(&said_line, questions, 0u);
            cli_line_text(&said_line, " questions; each held to cubin_safe with the part taken out:");
            if (cli_line_closed(&said_line) != 0ull)
            {
                query_said(context, line);
            }
            const char *const carried[7] = {carrier,         "--dry", files->machine, files->layout, "--list",
                                            "questions.txt", NULL};
            long long carried_code = -1ll;
            whole = query_context_teed(context, &s_run, "orior_cubin_run", carried, &carried_code, said);
            if (whole && (carried_code != 0ll))
            {
                cli_line_text(said, "the carrier did not read the list");
                whole = 0;
            }
            if (whole)
            {
                query_verdicts_said(context, &s_run);
            }
        }
        else if (whole)
        {
            cli_line_text(&said_line, "  the slot ");
            cli_line_text(&said_line, step->word);
            cli_line_text(&said_line, "'s questions: questions.txt of ");
            cli_line_text(&said_line, s_run.writer.qry);
            cli_line_text(&said_line, ", ");
            cli_line_decimal(&said_line, questions, 0u);
            cli_line_text(&said_line, " of them");
            if (cli_line_closed(&said_line) != 0ull)
            {
                query_said(context, line);
            }
            query_said(context, "  cross-check them: orior_cli protocol cross [--base <cu>]");
        }
        code = 0ll;
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

int query_learn_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    QueryLearnStep step;
    if (!query_learn_step_read(words, count, &step, said))
    {
        return -1;
    }
    static QueryFiles s_files;
    query_files_of(context->root, "sm_86", &s_files);
    static char s_writer[QUERY_PATH_LONGEST];
    static char s_carrier[QUERY_PATH_LONGEST];
    static char s_teacher[QUERY_PATH_LONGEST];
    char work[QUERY_PATH_LONGEST];
    query_at(context, QUERY_LEARN_WORK, work);
    if (!query_context_tool(context, "orior_khw_machine_write", s_writer, said) ||
        !query_context_tool(context, "orior_cubin_run", s_carrier, said) ||
        !query_context_tool(context, "orior_khw_learn", s_teacher, said))
    {
        return -1;
    }
    if (!query_folder_made(work))
    {
        cli_line_text(said, work);
        cli_line_text(said, " could not be made");
        return -1;
    }
    if (step.kind == QUERY_LEARN_CLOCK)
    {
        return query_learn_clock(context, &s_files, &step, said);
    }
    if ((step.kind == QUERY_LEARN_PASS) || (step.kind == QUERY_LEARN_SEED))
    {
        return query_learn_pass(context, &s_files, &step, s_writer, s_carrier, s_teacher, said);
    }
    return query_learn_offline(context, &s_files, &step, s_writer, s_carrier, s_teacher, said);
}
