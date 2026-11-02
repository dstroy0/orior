// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_modes.c: each mode of the identity run: every identity between texts in `Lstar.klq`, each side a kernel of a
// stick of its own; the modes over the stick's questions and the host's answers, each put to the part through the
// airlock and a pair run's log read back by the decoder; and the stick's questions sliced into identities
#include "../../../cli/src/text/text.h"
#include "pairs_internal.h"

// `folder`/`name` into `path`
void query_pairs_in(const QueryPairsFolderCtx *args)
{
    VerbaLinea line = {args->path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name_read);
    mmgr_line_finish(&line);
}

// `part` and `extension` under the bridge's folder into `path`
void query_part_file(const QueryPairsPartPathCtx *args)
{
    const QueryContext *context = args->context_read;
    char *path = args->path;
    char name[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    mmgr_line_put(&line, QUERY_TABLE "/");
    mmgr_line_put(&line, args->part);
    mmgr_line_put(&line, args->extension);
    mmgr_line_finish(&line);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = name,
               .path_out = path);
}

// the identity run with `words` teed as `orior_klq_identity.console`: 1 where it ran and exited 0
int query_identity_run(const QueryPairsContextErrMsgRunCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    const char *const *words = args->words_letters;
    long long *code = args->code;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_words[QUERY_WORDS_MAX + 2u];
    s_words[0] = args->identity;
    const unsigned int word_count = (args->count < QUERY_WORDS_MAX) ? args->count : QUERY_WORDS_MAX;
    memor.cpy(&(MemoriaCfg){.dst = &s_words[1], .src = words, .bytes = (size_t)word_count * sizeof(words[0])});
    s_words[word_count + 1u] = NULL;
    return EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                      .context = context, .run = run, .name = "orior_klq_identity", .words = s_words, .code = code,
                      .err_msg = err_msg);
}

int query_text_identity(const QueryPairsContextErrMsgCtx *args)
{
    QueryContext *context = args->context;
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    static char s_engine[QUERY_PATH_MAX_LEN];
    static char s_python[QUERY_PATH_MAX_LEN];
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    static char s_cuobjdump[QUERY_PATH_MAX_LEN];
    const QueryPrograms *const programs = context->programs;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_engine", .binary = s_engine, .err_msg = err_msg) ||
        !EMBED_CALL(programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = programs->context,
                    .role = QUERY_ROLE_PYTHON, .path = s_python, .capacity = sizeof(s_python), .err_msg = err_msg) ||
        !EMBED_CALL(programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = programs->context,
                    .role = QUERY_ROLE_NVCC, .path = s_nvcc, .capacity = sizeof(s_nvcc), .err_msg = err_msg) ||
        !EMBED_CALL(programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = programs->context,
                    .role = QUERY_ROLE_CUOBJDUMP, .path = s_cuobjdump, .capacity = sizeof(s_cuobjdump),
                    .err_msg = err_msg))
    {
        return -1;
    }
    char run_folder[QUERY_PATH_MAX_LEN];
    char work[QUERY_PATH_MAX_LEN];
    char sides[QUERY_PATH_MAX_LEN];
    char engine_out[QUERY_PATH_MAX_LEN];
    char cu[QUERY_PATH_MAX_LEN];
    char cubin[QUERY_PATH_MAX_LEN];
    char sass[QUERY_PATH_MAX_LEN];
    char tsv[QUERY_PATH_MAX_LEN];
    char generator[QUERY_PATH_MAX_LEN];
    char klq[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_WORK, .path_out = work);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = work, .name_read = "text_identity",
               .path = sides);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = sides, .name_read = "engine",
               .path = engine_out);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = sides, .name_read = "measuring_stick.cu",
               .path = cu);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = sides,
               .name_read = "measuring_stick_nvcc.cubin", .path = cubin);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = sides,
               .name_read = "measuring_stick_nvcc.sass", .path = sass);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = sides, .name_read = "measuring_stick.tsv",
               .path = tsv);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_NVIDIA "/measuring_stick.py", .path_out = generator);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/Lstar.klq", .path_out = klq);
    memor.cpy(&(MemoriaCfg){.dst = run_folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = run_folder, .err_msg = err_msg))
    {
        return -1;
    }
    long long code = -1ll;
    int whole = EMBED_CALL(query_pairs_part_context_internal.configs, QueryPairsPartContextCtx, .context = context,
                           .run_out = &s_run, .part = args->mode->part, .err_msg = err_msg);
    EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = sides);
    whole = whole && EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = engine_out);
    const char *const generate[6] = {s_python, generator, cu, tsv, klq, NULL};
    whole = whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                .context = context, .run = &s_run, .name = "measuring_stick_sides", .words = generate,
                                .code = &code, .err_msg = err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the sides were not written");
        whole = 0;
    }
    static QueryWords s_compile;
    memor.set(&(MemoriaCfg){.dst = &s_compile, .val = 0, .bytes = sizeof(s_compile)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (EMBED_CALL(programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = programs->context,
                   .role = QUERY_ROLE_CL, .path = cl, .capacity = sizeof(cl), .err_msg = &ignored) &&
        (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = cl,
                    .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}),
                    .byte = '\\') != NULL))
    {
        cl[EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = cl,
                      .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), .byte = '\\') -
           cl] = '\0';
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
                   .word = "-ccbin");
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg,
                   .words_words_pointer = &s_compile, .text = cl);
    }
#endif
    char arch[64];
    VerbaLinea arch_line = {arch, sizeof(arch), 0ull};
    mmgr_line_put(&arch_line, "-arch=");
    mmgr_line_put(&arch_line, args->mode->part);
    mmgr_line_finish(&arch_line);
    static const char *const s_flags[6] = {"-cubin", NULL, "-O3", "-diag-suppress", "177", "-o"};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
                   .word = (at == 1u) ? arch : s_flags[at]);
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = cubin);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = cu);
    whole = whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                .context = context, .run = &s_run, .name = "nvcc", .words = s_compile.word,
                                .code = &code, .err_msg = err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "nvcc did not compile the sides");
        whole = 0;
    }
    const char *const listing[4] = {s_cuobjdump, "-sass", cubin, NULL};
    whole =
        whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                            .context = context, .run = &s_run, .name = "measuring_stick_nvcc.sass", .words = listing,
                            .code = &code, .err_msg = err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "cuobjdump did not read the sides");
        whole = 0;
    }
    // the listing written beside the sides, which the engine reads from the disk
    const unsigned char *list_entries = NULL;
    unsigned long long listed_size = 0ull;
    whole = whole &&
            EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_run.writer,
                       .name = "measuring_stick_nvcc.sass", .bytes_u8s = &list_entries, .size_pointer = &listed_size) &&
            EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = sass,
                       .bytes = list_entries, .size = listed_size);
    const char *const engine_words[5] = {s_engine, cu, sass, engine_out, NULL};
    whole = whole && EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch,
                                QueryLoopsContextErrMsgNameRunCfg, .context = context, .run = &s_run,
                                .name = "assembler.log", .words = engine_words, .code = &code, .err_msg = err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the engine did not write the sides");
        whole = 0;
    }
    char reader[64];
    EMBED_CALL(query_pairs_part_name_internal.part_read, QueryPairsPartNameCtx, .part = args->mode->part,
               .name = reader, .capacity = sizeof(reader));
    whole = whole && EMBED_CALL(query_loops_context_folder.listings_read, QueryLoopsContextFolderCfg,
                                .context = context, .run = &s_run, .pattern = cubin, .prefix = "measuring_stick_",
                                .architecture = reader, .folder = engine_out, .err_msg = err_msg);
    const char *const sliced[4] = {"slice", sass, tsv, engine_out};
    whole = whole && EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                                .context = context, .run = &s_run, .identity = args->identity, .words_letters = sliced,
                                .count = 4u, .code = &code, .err_msg = err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the sides were not sliced");
        whole = 0;
    }
    whole = whole && EMBED_CALL(query_pairs_context_internal.host_answer, QueryPairsContextCtx, .context = context,
                                .run = &s_run, .out = sides, .err_msg = err_msg);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    char ksc[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
               .part = args->mode->part, .extension = ".ksc", .path = ksc);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = "text_identity");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = engine_out);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = "host_answers.txt");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = ksc);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = sides);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = klq);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = tsv);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = "--");
    whole = whole &&
            EMBED_CALL(query_pairs_part_context_dry_internal.carry, QueryPairsPartContextDryCtx, .context = context,
                       .part = args->mode->part, .dry = args->mode->dry, .words = &s_words, .err_msg = err_msg) &&
            EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                       .context = context, .run = &s_run, .identity = args->identity, .words_letters = s_words.word,
                       .count = s_words.count, .code = &code, .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}
