// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// retention_part.c: whether the part is free, the crystal's program built against the engine, and a folder's runs
// retained as their crystals
#include "../../../cli/src/command_line/callback_cfg.h"
#include "retention_internal.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

typedef struct
{
    const char *const *words;
    char *output;
    size_t capacity;
    const char *folder;
} RetentionPartCtx;

// the output of `words` run, piped back, into `output`, sized for `capacity`, ended by a zero: 1 where it ran and ended
// 0
EMBED_INLINE int query_output_get(const RetentionPartCtx *args)
{
    const char *const *words = args->words;
    char *output = args->output;
    size_t capacity = args->capacity;
    QueryProcess process;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &process, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_PIPED))
    {
        return 0;
    }
    unsigned long long length = 0ULL;
    for (;;)
    {
        const unsigned long long got =
            EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &process,
                       .piece = output + length, .capacity_u64 = capacity - 1u - length);
        if (got == 0ULL)
        {
            break;
        }
        length += got;
    }
    output[length] = '\0';
    return EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &process) == 0LL;
}

// 1 where a line of `apps`, nvidia-smi's processes on the part, names a program under `root`, whichever separators and
// case the host writes the path with
int query_on_the_part(const QueryRetentionRootCtx *args)
{
    char top[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {top, sizeof(top), 0ULL};
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = args->root, .from = '\\',
                     .to = '/');
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = top, .size = line.at);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return 0;
    }
    // the lines as the root is written, in a cell of their size: where it is refused the answer is that a program
    // is there, which saves a run from being taken for one the part has let go
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->apps, .cap = CLI_CELLS_POOL});
    char *const plain = g_cli_cells->temporary_buf_alloc(size + 1u, &g_query_cells);
    if (plain == NULL)
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = plain, .src = args->apps, .bytes = size + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = plain, .size = size, .from = '\\', .to = '/');
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = plain, .size = size);
    const int there =
        cellul.find(&(CatenaFinitaCfg){.src = plain, .cap = size + 1u, .other = top, .other_cap = sizeof(top)}) != NULL;
    g_cli_cells->persistent_buf_release(plain, &g_query_cells);
    return there;
}

long long query_part_used_max(const QueryRetentionInputCtx *args)
{
    if (args->supplied == NULL)
    {
        return 10LL;
    }
    char *end = NULL;
    const long long read = strtoll(args->supplied, &end, 10);
    return ((end != args->supplied) && (*end == '\0')) ? read : 10LL;
}

int query_part_free(const QueryRetentionContextErrMsgCtx *args)
{
    static char s_smi[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ULL};
    if (!EMBED_CALL(args->context->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                    .context = args->context->programs->context, .role = QUERY_ROLE_NVIDIA_SMI, .path = s_smi,
                    .capacity = sizeof(s_smi), .err_msg = &ignored))
    {
        mmgr_line_put(args->err_msg, "no nvidia-smi: the part is not read");
        return 0;
    }
    static char s_output[65536];
    const char *const apps[4] = {s_smi, "--query-compute-apps=process_name", "--format=csv,noheader", NULL};
    if (!EMBED_CALL(query_output_get, RetentionPartCtx, .words = apps, .output = s_output,
                    .capacity = sizeof(s_output)))
    {
        mmgr_line_put(args->err_msg, "nvidia-smi did not answer");
        return 0;
    }
    if (EMBED_CALL(query_retention_root_internal.on_the_part, QueryRetentionRootCtx, .apps = s_output,
                   .root = args->context->root))
    {
        mmgr_line_put(args->err_msg, "a process of the tree is on the part");
        return 0;
    }
    // the part's allowed use in percent, as QRY_PART_USED gives it, 10 where it is not set or is no number; a negative
    // bound bounds the part used whatever it reads
    char supplied[32];
    const long long most =
        EMBED_CALL(query_retention_input_internal.part_used_max, QueryRetentionInputCtx,
                   .supplied = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                          .name = "QRY_PART_USED", .value_out = supplied, .capacity = sizeof(supplied))
                                   ? supplied
                                   : NULL);
    const char *const used[4] = {s_smi, "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits", NULL};
    for (unsigned int read_state = 0u; read_state < 5u; read_state += 1u)
    {
        char *end = NULL;
        const long long percent = EMBED_CALL(query_output_get, RetentionPartCtx, .words = used, .output = s_output,
                                             .capacity = sizeof(s_output))
                                      ? strtoll(s_output, &end, 10)
                                      : -1LL;
        if ((end == NULL) || (end == s_output))
        {
            mmgr_line_put(args->err_msg, "the part's use was not read");
            return 0;
        }
        if (percent > most)
        {
            mmgr_line_put(args->err_msg, "the part is used ");
            mmgr_line_decimal(args->err_msg, (unsigned long long)percent, 0u);
            mmgr_line_put(args->err_msg, " percent");
            return 0;
        }
        if (read_state < 4u)
        {
            EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1000u);
        }
    }
    return 1;
}

// what the walk for the size of every file under a folder records
typedef struct
{
    const char *folder;
    unsigned long long total;
} QuerySizeWalk;

EMBED_INLINE unsigned long long query_size_under(const RetentionPartCtx *args);

static void query_size_walk(const CliEachNameIsFolderCfg *args)
{
    QuerySizeWalk *const walk = (QuerySizeWalk *)args->context;
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    if (args->is_folder)
    {
        walk->total += EMBED_CALL(query_size_under, RetentionPartCtx, .folder = path);
        return;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
        walk->total += size;
    }
}

// the size of every file under `folder`
EMBED_INLINE unsigned long long query_size_under(const RetentionPartCtx *args)
{
    QuerySizeWalk walk = {args->folder, 0ULL};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_size_walk, .context_bytess = &walk);
    return walk.total;
}

void query_crystals(const QueryRetentionContextOwnCtx *args)
{
    QueryContext *context = args->context;
    const char *folder = args->folder;
    QueryRuns list_entries;
    (void)EMBED_CALL(query_folder_runs_internal.runs_alloc, QueryFolderRunsCtx, .folder = folder,
                     .runs = &list_entries);
    char (*const paths)[QUERY_PATH_MAX_LEN] = list_entries.path;
    const unsigned int runs = list_entries.count;
    int newest = 1;
    for (unsigned int at = 0u; at < runs; at += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL});
        if ((length < 4u) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = paths[at] + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        if (newest)
        {
            newest = 0;
            continue;
        }
        QueryWriter writer = *args->own;
        writer.member = 0;
        memor.cpy(&(MemoriaCfg){.dst = writer.qry,
                                .src = paths[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        if (EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &writer))
        {
            continue;
        }
        const char *const name =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = paths[at],
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}),
                       .byte = '/')
                ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = paths[at],
                              .length_value = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}),
                              .byte = '/') +
                   1)
                : paths[at];
        char crystal_folder[QUERY_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = crystal_folder, .src = paths[at], .bytes = length - 4u});
        crystal_folder[length - 4u] = '\0';
        const unsigned long long began = query_now();
        static char s_output[4096];
        const char *const words[5] = {args->crystal, "write", paths[at], crystal_folder, NULL};
        const int written = EMBED_CALL(query_output_get, RetentionPartCtx, .words = words, .output = s_output,
                                       .capacity = sizeof(s_output));
        char text[QUERY_PATH_MAX_LEN + 4096u];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        unsigned long long size = 0ULL;
        const unsigned char *const mapped = qry_member_file_mapped(paths[at], &size);
        if (mapped != NULL)
        {
            qry_member_file_unmapped(mapped, size);
        }
        if (written && (EMBED_CALL(query_size_under, RetentionPartCtx, .folder = crystal_folder) >= size))
        {
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, " is stored as its .qry: its crystal is no smaller");
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = crystal_folder);
        }
        else if (written)
        {
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = paths[at]);
            // the crystal's own line, its end trimmed, and the seconds it took
            size_t len = cellul.len(&(CatenaFinitaCfg){.src = s_output, .cap = CLI_CELLS_POOL});
            len = (size_t)EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = s_output,
                                     .at = len, .set = "\n\r");
            mmgr_line_put_most(&line, s_output, len);
            mmgr_line_put(&line, ", in ");
            mmgr_line_decimal(&line, (query_now() - began) * 1000ULL, 0u);
            mmgr_line_put(&line, " ms");
        }
        else
        {
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, " is stored as its .qry: its crystal did not check");
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = crystal_folder);
        }
        if (mmgr_line_finish(&line) != 0ULL)
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = text);
        }
    }
    g_cli_cells->persistent_buf_release(list_entries.number, &g_query_cells);
    g_cli_cells->persistent_buf_release(list_entries.path, &g_query_cells);
    *(&list_entries) = (QueryRuns){NULL, NULL, 0u};
}

int query_crystal_build(const QueryRetentionContextCtx *args)
{
    QueryContext *context = args->context;
    char *binary = args->binary;
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ULL};
    if (!EMBED_CALL(context->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                    .context = context->programs->context, .role = QUERY_ROLE_NVCC, .path = s_nvcc,
                    .capacity = sizeof(s_nvcc), .err_msg = &ignored))
    {
        return 0;
    }
    char engine[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = "build/qry_crystal/engine", .path_out = engine);
    char base[64];
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = "qry_crystal", .binary = base,
               .capacity = sizeof(base));
    VerbaLinea binary_line = {binary, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&binary_line, engine);
    mmgr_line_ch(&binary_line, '/');
    mmgr_line_put(&binary_line, base);
    mmgr_line_finish(&binary_line);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = engine))
    {
        return 0;
    }
    // the engine's changed sources compiled first through src/build_engine.sh, with BUILD_OUT naming its folder
    char script[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = "src/build_engine.sh", .path_out = script);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "BUILD_OUT", .value = engine);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH",
               .value = context->programs->run_path(context->programs->context));
    const char *const engine_build[3] = {"bash", script, NULL};
    QueryProcess built;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &built, .words = engine_build,
                    .folder = NULL, .output = QUERY_OUTPUT_NONE) ||
        (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &built) != 0LL))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  the crystal's engine did not build");
        return 0;
    }
    char source[QUERY_PATH_MAX_LEN];
    char header[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = "src/cu/transpiler/lstar/protocol/qry_crystal.c", .path_out = source);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = "src/cu/engine/engine.h", .path_out = header);
    // on Windows the engine's objects, listed one a line in cell_tracking_engine.objects, are linked in
    // the list's text in a cell of its size, retained until the next build looks at it, the words naming its lines
    static char *s_objects_text;
    static const char *s_objects[QUERY_WORDS_MAX];
    g_cli_cells->persistent_buf_release(s_objects_text, &g_query_cells);
    s_objects_text = NULL;
    unsigned int objects = 0u;
#if defined(_WIN32)
    char list_entries[QUERY_PATH_MAX_LEN];
    VerbaLinea listing_line = {list_entries, sizeof(list_entries), 0ULL};
    mmgr_line_put(&listing_line, engine);
    mmgr_line_put(&listing_line, "/cell_tracking_engine.objects");
    mmgr_line_finish(&listing_line);
    unsigned long long listed_size = 0ULL;
    const unsigned char *const list = qry_member_file_mapped(list_entries, &listed_size);
    s_objects_text = (list != NULL) ? g_cli_cells->persistent_buf_alloc(listed_size + 1ULL, &g_query_cells) : NULL;
    if (s_objects_text != NULL)
    {
        // a file mapped whole is counted by a size_t
        memor.cpy(&(MemoriaCfg){.dst = s_objects_text, .src = list, .bytes = (size_t)listed_size});
        s_objects_text[listed_size] = '\0';
        char *line = s_objects_text;
        while ((*line != '\0') && (objects < QUERY_WORDS_MAX))
        {
            char *const end_of = line + EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = line, .set = "\r\n");
            const char saved_char = *end_of;
            *end_of = '\0';
            if (*line != '\0')
            {
                s_objects[objects] = line;
                objects += 1u;
            }
            line = end_of + ((saved_char != '\0') ? 1 : 0);
            line += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = line, .set = "\r\n");
        }
    }
    if (list != NULL)
    {
        qry_member_file_unmapped(list, listed_size);
    }
#endif
    unsigned long long built_time = 0ULL;
    unsigned long long changed = 0ULL;
    int stale =
        !EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = binary, .seconds = &built_time);
    stale = stale ||
            (EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = source, .seconds = &changed) &&
             (changed > built_time));
    stale = stale ||
            (EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = header, .seconds = &changed) &&
             (changed > built_time));
    {
        unsigned int at = 0u;
        while (!stale && (at < objects))
        {
            stale = EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = s_objects[at],
                               .seconds = &changed) &&
                    (changed > built_time);
            at += 1u;
        }
    }
    if (!stale)
    {
        return 1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_MAX_LEN];
    if (EMBED_CALL(context->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                   .context = context->programs->context, .role = QUERY_ROLE_CL, .path = cl, .capacity = sizeof(cl),
                   .err_msg = &ignored) &&
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = cl,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}),
                    .byte = '\\') != NULL))
    {
        cl[EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = cl,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), .byte = '\\') -
           cl] = '\0';
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "-ccbin");
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg,
                   .words_words_pointer = &s_words, .text = cl);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "-Xcompiler");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "/std:c11 /O2 /Zc:preprocessor /D_CRT_SECURE_NO_WARNINGS");
    }
#endif
    char include[QUERY_PATH_MAX_LEN + 2u];
    char engine_folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = "src/cu/engine",
               .path_out = engine_folder);
    VerbaLinea include_line = {include, sizeof(include), 0ULL};
    mmgr_line_put(&include_line, "-I");
    mmgr_line_put(&include_line, engine_folder);
    mmgr_line_finish(&include_line);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = include);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "-o");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = binary);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = source);
#if defined(_WIN32)
    for (unsigned int at = 0u; at < objects; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = s_objects[at]);
    }
#else
    char library[QUERY_PATH_MAX_LEN + 2u];
    VerbaLinea library_line = {library, sizeof(library), 0ULL};
    mmgr_line_put(&library_line, "-L");
    mmgr_line_put(&library_line, engine);
    mmgr_line_finish(&library_line);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = library);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "-lcell_tracking_engine");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "-Xlinker");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "-rpath");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "-Xlinker");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = engine);
#endif
    QueryProcess linked;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &linked,
                    .words = s_words.word_text, .folder = NULL, .output = QUERY_OUTPUT_NONE) ||
        (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &linked) != 0LL))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  qry_crystal did not build; runs are stored as their .qry");
        return 0;
    }
    return 1;
}
