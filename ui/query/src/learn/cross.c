// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cross.c: the cross alone (measuring_stick_query.c): the latest listed questions carried into a run of its own from
// the latest run that contains them, each question's container popped by the carrier with the part taken out, and the
// cross read over them against the vendor's disassembler. It reaches no part
#include "../query_loops.h"

// the base source the words name, where they do, into `base`: 1, or 0 with why given
int query_base_of(const char *const *words, unsigned int count, const char **base, VerbaLinea *err_msg)
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
    mmgr_line_put(err_msg, "cross takes [--base <cu>]");
    return 0;
}

// the names the latest list names, a line each: questions.txt, and each question's code and cases, into `names`
static void query_list_named(const unsigned char *list, unsigned long long size, VerbaLinea *names)
{
    mmgr_line_put(names, "questions.txt\n");
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
            mmgr_line_put_n(names, word[0], length[0]);
            mmgr_line_ch(names, '\n');
            mmgr_line_put_n(names, word[2], length[2]);
            mmgr_line_ch(names, '\n');
        }
        at = past + 1ull;
    }
}

int query_cross_run(QueryContext *context, const char *const *words, unsigned int count, VerbaLinea *err_msg)
{
    const char *base = NULL;
    if (!query_base_of(words, count, &base, err_msg))
    {
        return -1;
    }
    static QueryFiles s_files;
    query_files_of(context->root, "sm_86", &s_files);
    static char s_carrier[QUERY_PATH_MAX_LEN];
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    static QueryRun s_here;
    if (!query_context_tool(context, "orior_cubin_run", s_carrier, err_msg) ||
        !query_context_tool(context, "orior_measuring_stick_query", s_cross, err_msg) ||
        !query_nvdisasm(context, s_nvdisasm, err_msg) ||
        !query_context_started(context, &s_here, QUERY_LEARN_FOLDER, err_msg))
    {
        return -1;
    }
    char folder[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_LEARN_FOLDER, folder);
    char crystal[QUERY_PATH_MAX_LEN];
    const int has_crystal = query_crystal(context->root, crystal, sizeof(crystal));
    int whole = 1;
    if (!s_here.joined)
    {
        // the latest listed questions carried in, with every code and cases the list names
        char latest[QUERY_PATH_MAX_LEN];
        if (!query_latest_run(folder, "questions.txt", s_here.writer.qry, &s_here.writer, has_crystal ? crystal : NULL,
                              latest, sizeof(latest)))
        {
            mmgr_line_put(err_msg, "no run in ");
            mmgr_line_put(err_msg, folder);
            mmgr_line_put(err_msg, " contains a questions.txt to read");
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
                mmgr_line_put(err_msg, latest);
                mmgr_line_put(err_msg, " contains no questions.txt");
            }
        }
        // the names a list writes, two of each line's words with a line ended after each, take less than the line,
        // so a cell of the list's size and the first name's line fits them
        static const char s_first[] = "questions.txt\n";
        char *const names_text =
            whole ? g_cli_cells->persistent_buf_alloc(size + sizeof(s_first), &g_query_cells) : NULL;
        whole = whole && (names_text != NULL);
        if (whole)
        {
            VerbaLinea names = {names_text, size + sizeof(s_first), 0ull};
            query_list_named(list, size, &names);
            mmgr_line_finish(&names);
            char carried[QUERY_LINE_MAX_LEN];
            VerbaLinea carried_line = {carried, sizeof(carried), 0ull};
            mmgr_line_put(&carried_line, "  ");
            whole = query_writer_carry_listed(&s_here.writer, latest, names_text, &carried_line);
            if (whole && (mmgr_line_finish(&carried_line) != 0ull))
            {
                query_print(context, carried);
            }
            else if (!whole)
            {
                mmgr_line_put_most(err_msg, carried + 2, carried_line.at - 2u);
            }
        }
        g_cli_cells->persistent_buf_release(names_text, &g_query_cells);
        if (whole)
        {
            char work[QUERY_PATH_MAX_LEN];
            char forms_work[QUERY_PATH_MAX_LEN];
            char forms[QUERY_PATH_MAX_LEN];
            query_at(context, QUERY_LEARN_WORK, work);
            VerbaLinea forms_work_line = {forms_work, sizeof(forms_work), 0ull};
            mmgr_line_put(&forms_work_line, work);
            mmgr_line_put(&forms_work_line, "/forms_work.txt");
            mmgr_line_finish(&forms_work_line);
            VerbaLinea forms_line = {forms, sizeof(forms), 0ull};
            mmgr_line_put(&forms_line, work);
            mmgr_line_put(&forms_line, "/forms.txt");
            mmgr_line_finish(&forms_line);
            const char *const files[3] = {forms_work, forms, "gated.txt"};
            whole = query_run_carried(&s_here, folder, files, 3u, has_crystal ? crystal : NULL, err_msg);
        }
    }
    const unsigned char *questions_blob = NULL;
    unsigned long long questions_size = 0ull;
    if (whole && !query_writer_latest(&s_here.writer, "questions.txt", &questions_blob, &questions_size))
    {
        mmgr_line_put(err_msg, "the run contains no questions.txt");
        whole = 0;
    }
    const char *const read[3] = {s_files.machine, s_files.layout, base};
    whole = whole && query_run_read(&s_here, read, (base != NULL) ? 3u : 2u, err_msg);
    long long code = -1ll;
    if (whole)
    {
        const char *const carried[9] = {s_carrier,       "--dry", "--full", s_files.machine, s_files.layout, "--list",
                                        "questions.txt", NULL,    NULL};
        whole = query_context_handed(context, &s_here, "cubin_run.txt", carried, &code, err_msg);
        if (whole && (code != 0ll))
        {
            mmgr_line_put(err_msg, "the carrier did not read the list");
            whole = 0;
        }
    }
    if (whole && (base != NULL))
    {
        whole = query_base_compiled(context, &s_here, base, err_msg);
    }
    if (whole)
    {
        const char *const crossed[8] = {
            s_cross, "questions.txt", s_nvdisasm, "SM86", "measuring_stick_query.md", "gated.txt", NULL, NULL};
        whole = query_context_teed(context, &s_here, "orior_measuring_stick_query", crossed, &code, err_msg);
    }
    if (whole)
    {
        char line[QUERY_PATH_MAX_LEN + 160u];
        VerbaLinea print_line = {line, sizeof(line), 0ull};
        mmgr_line_put(&print_line, "  the report is the blob measuring_stick_query.md of ");
        mmgr_line_put(&print_line, s_here.writer.qry);
        mmgr_line_put(&print_line, "; the vendor's feedback is its blob gated.txt");
        if (mmgr_line_finish(&print_line) != 0ull)
        {
            query_print(context, line);
        }
        query_print(context, "  a live run runs those encodings with: orior_cubin_run --gated gated.txt ...");
    }
    if (!query_context_ended(context, &s_here, err_msg) || !whole)
    {
        return -1;
    }
    if (code == 2ll)
    {
        query_print(context, "  cross: the vendor's reader was not reached");
    }
    // an exit code is far below what an int represents
    return (int)code;
}
