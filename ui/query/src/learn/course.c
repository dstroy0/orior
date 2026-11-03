// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// course.c: the course: the whole protocol as one job and one run, its steps run in order. The course opens the run
// and names it to every step, each step joining it as a member as every module does, its own buffer handed to the
// run's one writer. The course reports its steps before the first, each step as it begins, where it stops and that it
// ended, through its own buffer into the run, which the window's panel reads to draw the line and walk the fire along
// it
#include "../query_loops.h"

// the folder the course's runs are stored in, a path from the top of the tree
#define QUERY_COURSE_FOLDER "build/engine/course/qry"

// One step of the course: its name on the line, the loop it runs and the word it gives it
typedef struct
{
    const char *name;
    int (*loop)(const QueryLoopCfg *args);
    const char *word;
} QueryStep;

// The steps in order: the part's tables are pulled off the vendor's disassembler, the widening discovers forms at the
// place, their turns and register runs classify them, the ruleset is written from them, the engine's stick is built
// from the ruleset, the pairs check the stick's chains against the part and query how far a container's code reaches,
// the forms are timed, and the ruleset is written again with the costs
static const QueryStep s_steps[10] = {
    {"vendor tables", query_tables_run, "sm_86"}, {"discover", query_learn_run, "discover"},
    {"turns", query_learn_run, "turns"},          {"registers", query_learn_run, "registers"},
    {"write krs", query_write_run, "krs"},        {"stick engine", query_stick_run, "engine"},
    {"pairs stick", query_pairs_run, "stick"},    {"pairs past", query_pairs_run, "past"},
    {"costs", query_learn_run, "costs"},          {"write krs", query_write_run, "krs"}};

const char *const g_query_course_names[10] = {"vendor tables", "discover",    "turns",      "registers", "write krs",
                                              "stick engine",  "pairs stick", "pairs past", "costs",     "write krs"};

const unsigned int g_query_course_steps = 10u;

// the names of the steps joined by `, ` into `line`
void query_names_join(const QueryLoopsCfg *args)
{
    for (unsigned int at = 0u; at < g_query_course_steps; at += 1u)
    {
        mmgr_line_put(args->line, (at == 0u) ? "" : ", ");
        mmgr_line_put(args->line, s_steps[at].name);
    }
}

// The index of the step `--from` names in `words`, the first where none is named; a name no step has, or a flag with
// no name after it, refused. A name shared by two steps names the first. 1, or 0 with why given
int query_course_from(const QueryLoopsErrMsgWordsCountCfg *args)
{
    unsigned int *first = args->first;
    *first = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = "--from", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        if ((at + 1u) >= args->count)
        {
            mmgr_line_put(args->err_msg, "--from takes the name of a step");
            return 0;
        }
        for (unsigned int step = 0u; step < g_query_course_steps; step += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = s_steps[step].name, .other = args->words[at + 1u], .cap = CLI_CELLS_POOL}))
            {
                *first = step;
                return 1;
            }
        }
        mmgr_line_put(args->err_msg, "no step of the course is ");
        mmgr_line_put(args->err_msg, args->words[at + 1u]);
        mmgr_line_put(args->err_msg, "; the steps are ");
        EMBED_CALL(cli_loops.names_join, QueryLoopsCfg, .line = args->err_msg);
        return 0;
    }
    return 1;
}

int query_course_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int first = 0u;
    if (!EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = words,
                    .count = count, .first = &first, .err_msg = err_msg))
    {
        return -1;
    }
    // the course's run, open on the context so that every step joins it; a course run inside a run already
    // open joins that one
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_COURSE_FOLDER, .err_msg = err_msg))
    {
        return -1;
    }
    char saved_open[QUERY_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = saved_open, .src = context->open, .bytes = sizeof(saved_open)});
    memor.cpy(
        &(MemoriaCfg){.dst = context->open,
                      .src = s_run.writer.qry,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_run.writer.qry, .cap = CLI_CELLS_POOL}) + 1u});
    char line[QUERY_LINE_MAX_LEN];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, "course: ");
    mmgr_line_decimal(&print_line, g_query_course_steps, 0u);
    mmgr_line_put(&print_line, " steps: ");
    EMBED_CALL(cli_loops.names_join, QueryLoopsCfg, .line = &print_line);
    mmgr_line_finish(&print_line);
    EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
    int ended = 0;
    for (unsigned int index = first; index < g_query_course_steps; index += 1u)
    {
        const unsigned int number = index + 1u;
        print_line.at = 0ULL;
        mmgr_line_put(&print_line, "course: step ");
        mmgr_line_decimal(&print_line, number, 0u);
        mmgr_line_put(&print_line, " of ");
        mmgr_line_decimal(&print_line, g_query_course_steps, 0u);
        mmgr_line_put(&print_line, ": ");
        mmgr_line_put(&print_line, s_steps[index].name);
        mmgr_line_finish(&print_line);
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
        char step_err_buf[QUERY_LINE_MAX_LEN];
        VerbaLinea step_err_msg = {step_err_buf, sizeof(step_err_buf), 0ULL};
        const char *const supplied[1] = {s_steps[index].word};
        const int code = EMBED_CALL(s_steps[index].loop, QueryLoopCfg, .context = context, .words = supplied,
                                    .count = 1u, .err_msg = &step_err_msg);
        // a step that joined the run parts from it as it ends: the course is the run's member again
        EMBED_CALL(query_err_msg_writer_internal.writer_join, QueryErrMsgWriterCtx, .writer_out = &s_run.writer);
        if (code == 0)
        {
            continue;
        }
        print_line.at = 0ULL;
        mmgr_line_put(&print_line, "course: stopped at step ");
        mmgr_line_decimal(&print_line, number, 0u);
        mmgr_line_put(&print_line, ", ");
        mmgr_line_put(&print_line, s_steps[index].name);
        if (code > 0)
        {
            mmgr_line_put(&print_line, ", exit ");
            mmgr_line_decimal(&print_line, (unsigned long long)code, 0u);
            ended = code;
        }
        else
        {
            mmgr_line_put(&print_line, ": ");
            mmgr_line_put_most(&print_line, step_err_buf, step_err_msg.at);
            mmgr_line_put_most(err_msg, step_err_buf, step_err_msg.at);
            ended = -1;
        }
        mmgr_line_finish(&print_line);
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
        break;
    }
    if (ended == 0)
    {
        print_line.at = 0ULL;
        mmgr_line_put(&print_line, "course: ended, ");
        mmgr_line_decimal(&print_line, g_query_course_steps, 0u);
        mmgr_line_put(&print_line, " steps");
        mmgr_line_finish(&print_line);
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
    }
    memor.cpy(&(MemoriaCfg){.dst = context->open, .src = saved_open, .bytes = sizeof(saved_open)});
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg))
    {
        return -1;
    }
    return ended;
}
