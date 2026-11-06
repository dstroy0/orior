// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cross.c: the cross alone (measuring_stick_query.c): the latest listed questions carried into a run of its own from
// the latest run that contains them, each question's container popped by the carrier with the part taken out, and the
// cross read over them against the vendor's disassembler. It reaches no part
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

// the base source the words name, where they do, into `base`: 1, or 0 with why given
int query_base_get(const QueryLoopsErrMsgWordsCfg *args)
{
    const char **base = args->base_pointer;
    *base = NULL;
    if (args->count == 0u)
    {
        return 1;
    }
    if ((args->count == 2u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "--base", .cap = CLI_POOL_BYTES})))
    {
        *base = args->words[1];
        return 1;
    }
    mmgr_line_put(args->err_msg, "cross takes [--base <cu>]");
    return 0;
}

typedef struct
{
    const unsigned char *list;
    unsigned long long size;
    VerbaLinea *names;
    QueryContext *context;
    const char *folder;
    const char *crystal;
    VerbaLinea *err_msg;
    const char *latest;
    const char *base;
    long long *code;
} CrossCtx;

// the part's files, the paths of the carrier, the cross and the vendor's disassembler, and the cross's run
static QueryFiles s_files;
static char s_transport[QUERY_PATH_MAX_LEN];
static char s_cross[QUERY_PATH_MAX_LEN];
static char s_nvdisasm[QUERY_PATH_MAX_LEN];
static QueryRun s_here;
// the names the latest list names, a line each: questions.txt, and each question's code and cases, into `names`
EMBED_INLINE void query_list_by_name(const CrossCtx *args)
{
    unsigned long long size = args->size;
    mmgr_line_put(args->names, "questions.txt\n");
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned char *const newline = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = args->list + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - args->list) : size;
        // the line's words: its code first and its cases third
        const char *word_text[3] = {NULL, NULL, NULL};
        unsigned long long length[3] = {0ULL, 0ULL, 0ULL};
        unsigned int words = 0u;
        unsigned long long walk = at;
        while ((walk < end) && (words < 3u))
        {
            walk = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                              .text = (const char *)args->list, .at = walk, .size = end, .set = " \t\r");
            const unsigned long long first = walk;
            walk = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                              .text = (const char *)args->list, .at = walk, .size = end, .set = " \t\r");
            if (walk > first)
            {
                word_text[words] = (const char *)(args->list + first);
                length[words] = walk - first;
                words += 1u;
            }
        }
        if (words >= 3u)
        {
            mmgr_line_put_n(args->names, word_text[0], length[0]);
            mmgr_line_ch(args->names, '\n');
            mmgr_line_put_n(args->names, word_text[2], length[2]);
            mmgr_line_ch(args->names, '\n');
        }
        at = end + 1ULL;
    }
}

// the questions.txt of the run at `latest` read and carried into the cross's run with every code and cases it names,
// what was carried told to `context`: 1, or 0 with why given into `err_msg`
EMBED_INLINE int query_list_carried(const CrossCtx *args)
{
    static QueryWriter s_other;
    s_other = s_here.writer;
    s_other.member = 0;
    const unsigned char *list = NULL;
    unsigned long long size = 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = s_other.qry,
                            .src = args->latest,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->latest, .cap = CLI_POOL_BYTES}) + 1u});
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_other,
                    .name = "questions.txt", .bytes_u8s = &list, .size_pointer = &size))
    {
        mmgr_line_put(args->err_msg, args->latest);
        mmgr_line_put(args->err_msg, " contains no questions.txt");
        return 0;
    }
    // the names a list writes, two of each line's words with a line ended after each, take less than the line, so a
    // cell of the list's size and the first name's line fits them
    static const char s_first[] = "questions.txt\n";
    char *const names_text = g_cli_guard->persistent_buf_alloc(size + sizeof(s_first), g_query_owner);
    int whole = names_text != NULL;
    if (whole)
    {
        VerbaLinea names = {names_text, size + sizeof(s_first), 0ULL};
        EMBED_CALL(query_list_by_name, CrossCtx, .list = list, .size = size, .names = &names);
        mmgr_line_finish(&names);
        char carried[QUERY_LINE_MAX_LEN];
        VerbaLinea transferred_line = {carried, sizeof(carried), 0ULL};
        mmgr_line_put(&transferred_line, "  ");
        whole = EMBED_CALL(query_from_writer_internal.writer_machine_transfer_list, QueryFromWriterCtx,
                           .writer = &s_here.writer, .from = args->latest, .list_entries = names_text,
                           .err_msg = &transferred_line);
        if (whole && (mmgr_line_finish(&transferred_line) != 0ULL))
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = carried);
        }
        else if (!whole)
        {
            mmgr_line_put_most(args->err_msg, carried + 2, transferred_line.at - 2u);
        }
    }
    g_cli_guard->persistent_buf_release(names_text, g_query_owner);
    return whole;
}

// the work's forms lists, forms_work.txt and forms.txt, and gated.txt carried into the cross's run from the latest run
// in `folder` that contains them, with the `crystal` where there is one: 1, or 0 with why given into `err_msg`
EMBED_INLINE int query_forms_carried(const CrossCtx *args)
{
    char work[QUERY_PATH_MAX_LEN];
    char forms_work[QUERY_PATH_MAX_LEN];
    char forms[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = args->context,
               .under = QUERY_LEARN_WORK, .path_out = work);
    VerbaLinea forms_work_line = {forms_work, sizeof(forms_work), 0ULL};
    mmgr_line_put(&forms_work_line, work);
    mmgr_line_put(&forms_work_line, "/forms_work.txt");
    mmgr_line_finish(&forms_work_line);
    VerbaLinea forms_line = {forms, sizeof(forms), 0ULL};
    mmgr_line_put(&forms_line, work);
    mmgr_line_put(&forms_line, "/forms.txt");
    mmgr_line_finish(&forms_line);
    const char *const files[3] = {forms_work, forms, "gated.txt"};
    return EMBED_CALL(query_count_folder_internal.run_machine_transfer, QueryCountFolderCtx, .run = &s_here,
                      .folder = args->folder, .files = files, .count = 3u, .crystal = args->crystal,
                      .err_msg = args->err_msg);
}

// the carrier run over the list with the part out, the `base` compiled where one is named, and the cross read over
// what the carrier wrote, where its report lies told to `context`; the last exit code into `code`: 1, or 0 with why
// given into `err_msg`
EMBED_INLINE int query_cross_steps(const CrossCtx *args)
{
    const char *const carried[9] = {s_transport,     "--dry", "--full", s_files.machine, s_files.layout, "--list",
                                    "questions.txt", NULL,    NULL};
    int whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                           .context = args->context, .run = &s_here, .name = "cubin_run.txt", .words = carried,
                           .code = args->code, .err_msg = args->err_msg);
    if (whole && (*args->code != 0LL))
    {
        mmgr_line_put(args->err_msg, "the carrier did not read the list");
        whole = 0;
    }
    if (whole && (args->base != NULL))
    {
        whole = EMBED_CALL(query_loops_context_err_msg.base_compile, QueryLoopsContextErrMsgCfg,
                           .context = args->context, .run = &s_here, .base = args->base, .err_msg = args->err_msg);
    }
    if (whole)
    {
        const char *const crossed[8] = {
            s_cross, "questions.txt", s_nvdisasm, "SM86", "measuring_stick_query.md", "gated.txt", NULL, NULL};
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = args->context, .run = &s_here, .name = "orior_measuring_stick_query",
                           .words = crossed, .code = args->code, .err_msg = args->err_msg);
    }
    if (whole)
    {
        char line[QUERY_PATH_MAX_LEN + 160u];
        VerbaLinea print_line = {line, sizeof(line), 0ULL};
        mmgr_line_put(&print_line, "  the report is the blob measuring_stick_query.md of ");
        mmgr_line_put(&print_line, s_here.writer.qry);
        mmgr_line_put(&print_line, "; the vendor's feedback is its blob gated.txt");
        if (mmgr_line_finish(&print_line) != 0ULL)
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = line);
        }
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context,
                   .text = "  a live run runs those encodings with: orior_cubin_run --gated gated.txt ...");
    }
    return whole;
}

int query_cross_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    const char *base = NULL;
    if (!EMBED_CALL(query_loops_err_msg_words.base_get, QueryLoopsErrMsgWordsCfg, .words = words, .count = count,
                    .base_pointer = &base, .err_msg = err_msg))
    {
        return -1;
    }
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = context->root, .part = "sm_86",
               .files = &s_files);
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_cubin_run", .binary = s_transport, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_query", .binary = s_cross, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg.nvdisasm, QueryLoopsErrMsgCfg, .context_read = context, .path = s_nvdisasm,
                    .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_here, .folder = QUERY_LEARN_FOLDER, .err_msg = err_msg))
    {
        return -1;
    }
    char folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_LEARN_FOLDER, .path_out = folder);
    char crystal[QUERY_PATH_MAX_LEN];
    const int has_crystal = EMBED_CALL(query_capacity_root_internal.crystal, QueryCapacityRootCtx,
                                       .root = context->root, .program = crystal, .capacity = sizeof(crystal));
    int whole = 1;
    if (!s_here.joined)
    {
        // the latest listed questions carried in, with every code and cases the list names
        char latest[QUERY_PATH_MAX_LEN];
        if (!EMBED_CALL(query_loops_folder.latest_run, QueryLoopsFolderCfg, .folder = folder, .name = "questions.txt",
                        .own = s_here.writer.qry, .writer = &s_here.writer, .crystal = has_crystal ? crystal : NULL,
                        .found = latest, .capacity = sizeof(latest)))
        {
            mmgr_line_put(err_msg, "no run in ");
            mmgr_line_put(err_msg, folder);
            mmgr_line_put(err_msg, " contains a questions.txt to read");
            whole = 0;
        }
        whole =
            whole && EMBED_CALL(query_list_carried, CrossCtx, .latest = latest, .context = context, .err_msg = err_msg);
        whole = whole && EMBED_CALL(query_forms_carried, CrossCtx, .context = context, .folder = folder,
                                    .crystal = has_crystal ? crystal : NULL, .err_msg = err_msg);
    }
    const unsigned char *questions_blob = NULL;
    unsigned long long questions_size = 0ULL;
    if (whole && !EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_here.writer,
                             .name = "questions.txt", .bytes_u8s = &questions_blob, .size_pointer = &questions_size))
    {
        mmgr_line_put(err_msg, "the run contains no questions.txt");
        whole = 0;
    }
    const char *const read[3] = {s_files.machine, s_files.layout, base};
    whole = whole && EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_here,
                                .paths_letters = read, .count = (base != NULL) ? 3u : 2u, .err_msg = err_msg);
    long long code = -1LL;
    whole = whole && EMBED_CALL(query_cross_steps, CrossCtx, .context = context, .base = base, .code = &code,
                                .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_here, .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    if (code == 2LL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  cross: the vendor's reader was not reached");
    }
    // an exit code is far below what an int represents
    return (int)code;
}
