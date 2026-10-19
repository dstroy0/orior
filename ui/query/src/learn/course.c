// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// course.c: the course: the whole protocol as one job and one run, its steps run in order. The course opens the run
// and names it to every step, each step joining it as a member as every module does, its own buffer handed to the
// run's one writer. The course says its steps before the first, each step as it begins, where it stops and that it
// ended, through its own buffer into the run, which the window's panel reads to draw the line and walk the fire along
// it
#include "../query_loops.h"

// the folder the course's runs are kept in, a path from the top of the tree
#define QUERY_COURSE_FOLDER "build/engine/course/qry"

// One step of the course: its name on the line, the loop it runs and the word it gives it
typedef struct
{
    const char *name;
    int (*loop)(QueryContext *context, const char *const *words, unsigned int count, CliLine *said);
    const char *word;
} QueryStep;

// The steps in order: the part's tables are pulled off the vendor's disassembler, the widening discovers forms at the
// slot, their turns and register runs classify them, the ruleset is written from them, the engine's stick is built
// from the ruleset, the pairs hold the stick's chains against the part and ask how far a container's code reaches,
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
void query_names_joined(CliLine *line)
{
    for (unsigned int at = 0u; at < g_query_course_steps; at += 1u)
    {
        cli_line_text(line, (at == 0u) ? "" : ", ");
        cli_line_text(line, s_steps[at].name);
    }
}

// The index of the step `--from` names in `words`, the first where none is named; a name no step has, or a flag with
// no name after it, refused. A name shared by two steps names the first. 1, or 0 with why said
int query_course_from(const char *const *words, unsigned int count, unsigned int *first, CliLine *said)
{
    *first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--from", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        if ((at + 1u) >= count)
        {
            cli_line_text(said, "--from takes the name of a step");
            return 0;
        }
        for (unsigned int step = 0u; step < g_query_course_steps; step += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = s_steps[step].name, .other = words[at + 1u], .cap = CLI_CELLS_POOL}))
            {
                *first = step;
                return 1;
            }
        }
        cli_line_text(said, "no step of the course is ");
        cli_line_text(said, words[at + 1u]);
        cli_line_text(said, "; the steps are ");
        query_names_joined(said);
        return 0;
    }
    return 1;
}

int query_course_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    unsigned int first = 0u;
    if (!query_course_from(words, count, &first, said))
    {
        return -1;
    }
    // the course's run, held open on the context so that every step joins it; a course run inside a run already
    // open joins that one
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, QUERY_COURSE_FOLDER, said))
    {
        return -1;
    }
    char held[QUERY_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = held, .src = context->open, .bytes = sizeof(held)});
    memor.cpy(
        &(MemoriaCfg){.dst = context->open,
                      .src = s_run.writer.qry,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_run.writer.qry, .cap = CLI_CELLS_POOL}) + 1u});
    char line[QUERY_SAID_LONGEST];
    CliLine said_line = {line, sizeof(line), 0ull};
    cli_line_text(&said_line, "course: ");
    cli_line_decimal(&said_line, g_query_course_steps, 0u);
    cli_line_text(&said_line, " steps: ");
    query_names_joined(&said_line);
    cli_line_closed(&said_line);
    query_said(context, line);
    int ended = 0;
    for (unsigned int index = first; index < g_query_course_steps; index += 1u)
    {
        const unsigned int number = index + 1u;
        said_line.at = 0ull;
        cli_line_text(&said_line, "course: step ");
        cli_line_decimal(&said_line, number, 0u);
        cli_line_text(&said_line, " of ");
        cli_line_decimal(&said_line, g_query_course_steps, 0u);
        cli_line_text(&said_line, ": ");
        cli_line_text(&said_line, s_steps[index].name);
        cli_line_closed(&said_line);
        query_said(context, line);
        char step_said_text[QUERY_SAID_LONGEST];
        CliLine step_said = {step_said_text, sizeof(step_said_text), 0ull};
        const char *const given[1] = {s_steps[index].word};
        const int code = s_steps[index].loop(context, given, 1u, &step_said);
        // a step that joined the run parts from it as it ends: the course is the run's member again
        query_writer_joined(&s_run.writer);
        if (code == 0)
        {
            continue;
        }
        said_line.at = 0ull;
        cli_line_text(&said_line, "course: stopped at step ");
        cli_line_decimal(&said_line, number, 0u);
        cli_line_text(&said_line, ", ");
        cli_line_text(&said_line, s_steps[index].name);
        if (code > 0)
        {
            cli_line_text(&said_line, ", exit ");
            cli_line_decimal(&said_line, (unsigned long long)code, 0u);
            ended = code;
        }
        else
        {
            cli_line_text(&said_line, ": ");
            cli_line_text_most(&said_line, step_said_text, step_said.at);
            cli_line_text_most(said, step_said_text, step_said.at);
            ended = -1;
        }
        cli_line_closed(&said_line);
        query_said(context, line);
        break;
    }
    if (ended == 0)
    {
        said_line.at = 0ull;
        cli_line_text(&said_line, "course: ended, ");
        cli_line_decimal(&said_line, g_query_course_steps, 0u);
        cli_line_text(&said_line, " steps");
        cli_line_closed(&said_line);
        query_said(context, line);
    }
    memor.cpy(&(MemoriaCfg){.dst = context->open, .src = held, .bytes = sizeof(held)});
    if (!query_context_ended(context, &s_run, said))
    {
        return -1;
    }
    return ended;
}
