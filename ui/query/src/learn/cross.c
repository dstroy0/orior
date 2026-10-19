// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cross.c: the cross alone (measuring_stick_query.c): the latest listed questions carried into a run of its own from
// the latest run that holds them, each question's container popped by the carrier with the part taken out, and the
// cross read over them against the vendor's disassembler. It reaches no part
#include "../query_loops.h"

// the base source the words name, where they do, into `base`: 1, or 0 with why said
int query_base_of(const char *const *words, unsigned int count, const char **base, CliLine *said)
{
    *base = NULL;
    if (count == 0u)
    {
        return 1;
    }
    if ((count == 2u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "--base", .cap = CLI_CELLS_POOL})))
    {
        *base = words[1];
        return 1;
    }
    cli_line_text(said, "cross takes [--base <cu>]");
    return 0;
}

// the names the latest list names, a line each: questions.txt, and each question's code and cases, into `names`
static void query_list_named(const unsigned char *list, unsigned long long size, CliLine *names)
{
    cli_line_text(names, "questions.txt\n");
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const end = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = list + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long past = (end != NULL) ? (unsigned long long)(end - list) : size;
        // the line's words: its code first and its cases third
        const char *word[3] = {NULL, NULL, NULL};
        unsigned long long length[3] = {0ull, 0ull, 0ull};
        unsigned int words = 0u;
        unsigned long long walk = at;
        while ((walk < past) && (words < 3u))
        {
            while ((walk < past) && ((list[walk] == ' ') || (list[walk] == '\t') || (list[walk] == '\r')))
            {
                walk += 1ull;
            }
            const unsigned long long first = walk;
            while ((walk < past) && (list[walk] != ' ') && (list[walk] != '\t') && (list[walk] != '\r'))
            {
                walk += 1ull;
            }
            if (walk > first)
            {
                word[words] = (const char *)(list + first);
                length[words] = walk - first;
                words += 1u;
            }
        }
        if (words >= 3u)
        {
            cli_line_counted(names, word[0], length[0]);
            cli_line_character(names, '\n');
            cli_line_counted(names, word[2], length[2]);
            cli_line_character(names, '\n');
        }
        at = past + 1ull;
    }
}

int query_cross_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    const char *base = NULL;
    if (!query_base_of(words, count, &base, said))
    {
        return -1;
    }
    static QueryFiles s_files;
    query_files_of(context->root, "sm_86", &s_files);
    static char s_carrier[QUERY_PATH_LONGEST];
    static char s_cross[QUERY_PATH_LONGEST];
    static char s_nvdisasm[QUERY_PATH_LONGEST];
    static QueryRun s_here;
    if (!query_context_tool(context, "cubin_run", s_carrier, said) ||
        !query_context_tool(context, "measuring_stick_query", s_cross, said) ||
        !query_nvdisasm(context, s_nvdisasm, said) ||
        !query_context_started(context, &s_here, QUERY_LEARN_FOLDER, said))
    {
        return -1;
    }
    char folder[QUERY_PATH_LONGEST];
    query_at(context, QUERY_LEARN_FOLDER, folder);
    char crystal[QUERY_PATH_LONGEST];
    const int has_crystal = query_crystal(context->root, crystal, sizeof(crystal));
    int whole = 1;
    if (!s_here.joined)
    {
        // the latest listed questions carried in, with every code and cases the list names
        char latest[QUERY_PATH_LONGEST];
        if (!query_latest_run(folder, "questions.txt", s_here.writer.qry, &s_here.writer, has_crystal ? crystal : NULL,
                              latest, sizeof(latest)))
        {
            cli_line_text(said, "no run in ");
            cli_line_text(said, folder);
            cli_line_text(said, " holds a questions.txt to read");
            whole = 0;
        }
        static QueryWriter s_other;
        s_other = s_here.writer;
        s_other.member = 0;
        const unsigned char *list = NULL;
        unsigned long long size = 0ull;
        if (whole)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_other.qry,
                              .src = latest,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = latest, .cap = CLI_CELLS_POOL}) + 1u});
            whole = query_writer_latest(&s_other, "questions.txt", &list, &size);
            if (!whole)
            {
                cli_line_text(said, latest);
                cli_line_text(said, " holds no questions.txt");
            }
        }
        // the names a list writes, two of each line's words with a line ended after each, take less than the line,
        // so a cell of the list's size and the first name's line holds them
        static const char s_first[] = "questions.txt\n";
        char *const names_text =
            whole ? cli_cell_taken_by(&g_query_cells, size + sizeof(s_first), "the names of a list carried") : NULL;
        whole = whole && (names_text != NULL);
        if (whole)
        {
            CliLine names = {names_text, size + sizeof(s_first), 0ull};
            query_list_named(list, size, &names);
            cli_line_closed(&names);
            char carried[QUERY_SAID_LONGEST];
            CliLine carried_line = {carried, sizeof(carried), 0ull};
            cli_line_text(&carried_line, "  ");
            whole = query_writer_carry_listed(&s_here.writer, latest, names_text, &carried_line);
            if (whole && (cli_line_closed(&carried_line) != 0ull))
            {
                query_said(context, carried);
            }
            else if (!whole)
            {
                cli_line_text_most(said, carried + 2, carried_line.at - 2u);
            }
        }
        cli_cell_given_by(&g_query_cells, names_text);
        if (whole)
        {
            char work[QUERY_PATH_LONGEST];
            char forms_work[QUERY_PATH_LONGEST];
            char forms[QUERY_PATH_LONGEST];
            query_at(context, QUERY_LEARN_WORK, work);
            CliLine forms_work_line = {forms_work, sizeof(forms_work), 0ull};
            cli_line_text(&forms_work_line, work);
            cli_line_text(&forms_work_line, "/forms_work.txt");
            cli_line_closed(&forms_work_line);
            CliLine forms_line = {forms, sizeof(forms), 0ull};
            cli_line_text(&forms_line, work);
            cli_line_text(&forms_line, "/forms.txt");
            cli_line_closed(&forms_line);
            const char *const files[3] = {forms_work, forms, "held.txt"};
            whole = query_run_carried(&s_here, folder, files, 3u, has_crystal ? crystal : NULL, said);
        }
    }
    const unsigned char *held_list = NULL;
    unsigned long long held_size = 0ull;
    if (whole && !query_writer_latest(&s_here.writer, "questions.txt", &held_list, &held_size))
    {
        cli_line_text(said, "the run holds no questions.txt");
        whole = 0;
    }
    const char *const read[3] = {s_files.machine, s_files.layout, base};
    whole = whole && query_run_read(&s_here, read, (base != NULL) ? 3u : 2u, said);
    long long code = -1ll;
    if (whole)
    {
        const char *const carried[9] = {s_carrier,       "--dry", "--full", s_files.machine, s_files.layout, "--list",
                                        "questions.txt", NULL,    NULL};
        whole = query_context_handed(context, &s_here, "cubin_run.txt", carried, &code, said);
        if (whole && (code != 0ll))
        {
            cli_line_text(said, "the carrier did not read the list");
            whole = 0;
        }
    }
    if (whole && (base != NULL))
    {
        whole = query_base_compiled(context, &s_here, base, said);
    }
    if (whole)
    {
        const char *const crossed[8] = {
            s_cross, "questions.txt", s_nvdisasm, "SM86", "measuring_stick_query.md", "held.txt", NULL, NULL};
        whole = query_context_teed(context, &s_here, "measuring_stick_query", crossed, &code, said);
    }
    if (whole)
    {
        char line[QUERY_PATH_LONGEST + 160u];
        CliLine said_line = {line, sizeof(line), 0ull};
        cli_line_text(&said_line, "  the report is the blob measuring_stick_query.md of ");
        cli_line_text(&said_line, s_here.writer.qry);
        cli_line_text(&said_line, "; the vendor's feedback is its blob held.txt");
        if (cli_line_closed(&said_line) != 0ull)
        {
            query_said(context, line);
        }
        query_said(context, "  a live run holds those encodings with: cubin_run --held held.txt ...");
    }
    if (!query_context_ended(context, &s_here, said) || !whole)
    {
        return -1;
    }
    if (code == 2ll)
    {
        query_said(context, "  cross: the vendor's reader was not reached");
    }
    // an exit code is far below what an int holds
    return (int)code;
}
