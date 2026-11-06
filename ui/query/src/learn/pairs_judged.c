// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_judged.c: the modes over the stick's questions and the host's answers, `stall`, `register`, `end`, `queue`,
// `pair`, `cost`, `curve`, `frame` and `fixed`, each put to the part through the airlock and a pair run's log read back
// by the decoder; and the stick's questions sliced into identities, the integer questions computed on the host over
// every case, each identity's verdict written to `Lstar.klq` beside the forms of the rulesets given, and the engine's
// writing of the stick sifted through what nvcc writes
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "pairs_internal.h"

// the words a mode's program is given after its folder, added to `words`: the mode's own targets, or the words given
void query_targets(const QueryPairsWordsCtx *args)
{
    const QueryContext *context = args->context_read;
    QueryWords *words = args->words;
    char path[QUERY_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "queue", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
                   .under = QUERY_PAIRS_STICK "/measuring_stick.tsv", .path_out = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "pair", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
                   .under = QUERY_TABLE "/Lstar.klq", .path_out = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
        EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
                   .part = args->mode->part, .extension = ".khw", .path = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "stall", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
                   .part = args->mode->part, .extension = ".khw", .path = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "cost", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
                   .part = args->mode->part, .extension = ".kdm", .path = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
        EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
                   .part = args->mode->part, .extension = ".khw", .path = path);
        EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
                   .text = path);
    }
    else
    {
        for (unsigned int at = 0u; at < args->mode->word_count; at += 1u)
        {
            EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                       .word_text = args->mode->words[at]);
        }
    }
}

// 1 where a mode opens with the kernel the system accepted: `frame` and `fixed`, which read it, and `pair`, whose
// opening runs both
int query_kernel_dispatch(const QueryPairsCtx *args)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->mode_text, .other = "frame", .cap = CLI_POOL_BYTES})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->mode_text, .other = "fixed", .cap = CLI_POOL_BYTES})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->mode_text, .other = "pair", .cap = CLI_POOL_BYTES}));
}

void query_kernel_words(const QueryPairsPartCtx *args)
{
    QueryWords *words = args->words;
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->part);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->files->machine);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->files->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->files->mnemonics);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "kernel.bin");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "kernel");
}

int query_judged(const QueryPairsContextErrMsgCtx *args)
{
    QueryContext *context = args->context;
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    char work[QUERY_PATH_MAX_LEN];
    char out[QUERY_PATH_MAX_LEN];
    char engine[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_WORK, .path_out = work);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = work, .name_read = args->mode->mode,
               .path = out);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_STICK "/engine", .path_out = engine);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out))
    {
        mmgr_line_put(err_msg, out);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = folder, .err_msg = err_msg))
    {
        return -1;
    }
    static QueryFiles s_files;
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = context->root, .part = args->mode->part,
               .files = &s_files);
    static char s_writer[QUERY_PATH_MAX_LEN];
    long long code = -1LL;
    int whole =
        EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                   .name = "orior_khw_machine_write", .binary = s_writer, .err_msg = err_msg);
    if (whole && EMBED_CALL(query_pairs_internal.kernel_dispatch, QueryPairsCtx, .mode_text = args->mode->mode))
    {
        // the kernel the system accepted, handed to the run by the vendor's writer for the frame's queries
        static QueryWords s_kernel;
        memor.set(&(MemoriaCfg){.dst = &s_kernel, .val = 0, .bytes = sizeof(s_kernel)});
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_kernel,
                   .word_text = s_writer);
        EMBED_CALL(query_pairs_part_internal.kernel_words, QueryPairsPartCtx, .words = &s_kernel,
                   .part = args->mode->part, .files = &s_files);
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "orior_khw_machine_write",
                           .words = s_kernel.word_text, .code = &code, .err_msg = err_msg);
        if (whole && (code != 0LL))
        {
            mmgr_line_put(err_msg, "the vendor's writer handed the run no kernel");
            whole = 0;
        }
    }
    char host_answers[QUERY_PATH_MAX_LEN];
    char overseer[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = work, .name_read = "host_answers.txt",
               .path = host_answers);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = work, .name_read = "overseer.txt",
               .path = overseer);
    char run_folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = folder,
               .path_out = run_folder);
    char crystal[QUERY_PATH_MAX_LEN];
    const int has_crystal = EMBED_CALL(query_capacity_root_internal.crystal, QueryCapacityRootCtx,
                                       .root = context->root, .program = crystal, .capacity = sizeof(crystal));
    const char *const carried[3] = {host_answers, overseer, "faulted.txt"};
    whole = whole && EMBED_CALL(query_count_folder_internal.run_machine_transfer, QueryCountFolderCtx, .run = &s_run,
                                .folder = run_folder, .files = carried, .count = 3u,
                                .crystal = has_crystal ? crystal : NULL, .err_msg = err_msg);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (whole && (!cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "frame", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "fixed", .cap = CLI_POOL_BYTES})) &&
        !EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_run.writer,
                    .name = "host_answers.txt", .bytes_u8s = &bytes, .size_pointer = &size))
    {
        mmgr_line_put(err_msg, "no host answers: run orior_cli protocol pairs stick first");
        whole = 0;
    }
    whole = whole && EMBED_CALL(query_pairs_part_context_internal.configs, QueryPairsPartContextCtx, .context = context,
                                .run_out = &s_run, .part = args->mode->part, .err_msg = err_msg);
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "cost", .cap = CLI_POOL_BYTES})))
    {
        char kdm[QUERY_PATH_MAX_LEN];
        EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
                   .part = args->mode->part, .extension = ".kdm", .path = kdm);
        const char *const read[1] = {kdm};
        whole = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = 1u, .err_msg = err_msg);
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    char ksc[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_pairs_part_path_internal.part_file, QueryPairsPartPathCtx, .context_read = context,
               .part = args->mode->part, .extension = ".ksc", .path = ksc);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = args->mode->mode);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = engine);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "host_answers.txt");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = ksc);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = out);
    EMBED_CALL(query_pairs_words_internal.targets, QueryPairsWordsCtx, .context_read = context, .mode = args->mode,
               .words = &s_words);
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "cost", .cap = CLI_POOL_BYTES})))
    {
        // the machine file's forms placed by the vendor's writer, handed to the run for the identity to cost in the
        // part's clock beside the arrangements
        const char *const positioned[8] = {s_writer,          args->mode->part, s_files.machine, s_files.layout,
                                           s_files.mnemonics, "krs_forms.bin",  "krs",           NULL};
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "orior_khw_machine_write", .words = positioned,
                           .code = &code, .err_msg = err_msg);
        if (whole && (code != 0LL))
        {
            mmgr_line_put(err_msg, "the vendor's writer placed no forms");
            whole = 0;
        }
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word_text = "krs_forms.bin");
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word_text = "--");
    whole = whole &&
            EMBED_CALL(query_pairs_part_context_dry_internal.carry, QueryPairsPartContextDryCtx, .context = context,
                       .part = args->mode->part, .dry = args->mode->dry, .words = &s_words, .err_msg = err_msg) &&
            EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                       .context = context, .run = &s_run, .identity = args->identity,
                       .words_letters = s_words.word_text, .count = s_words.count, .code = &code, .err_msg = err_msg);
    if (whole && (code != 0LL))
    {
        return EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context,
                          .run_out = &s_run, .err_msg = err_msg)
                   ? 1
                   : -1;
    }
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = args->mode->mode, .other = "pair", .cap = CLI_POOL_BYTES})) &&
        EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_run.writer, .name = "log",
                   .bytes_u8s = &bytes, .size_pointer = &size))
    {
        static char s_decoder[QUERY_PATH_MAX_LEN];
        char klq[QUERY_PATH_MAX_LEN];
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
                   .under = QUERY_TABLE "/Lstar.klq", .path_out = klq);
        const char *const decoded[5] = {s_decoder, s_run.writer.qry, engine, klq, NULL};
        whole =
            EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                       .name = "orior_klq_decoder", .binary = s_decoder, .err_msg = err_msg) &&
            EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                       .context = context, .run = &s_run, .name = "orior_klq_decoder", .words = decoded, .code = &code,
                       .err_msg = err_msg);
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

// what the walk for a read-back listing in the engine's folder records: whether one `.dis` stands there
static void query_dis_find(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->name,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}), .byte = '.');
    *(int *)context =
        *(int *)context || (!args->is_folder && (dot != NULL) &&
                            (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".dis", .cap = CLI_POOL_BYTES})));
}

int query_stick_slice(const QueryPairsContextErrMsgCtx *args)
{
    QueryContext *context = args->context;
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    char stick[QUERY_PATH_MAX_LEN];
    char work[QUERY_PATH_MAX_LEN];
    char klq[QUERY_PATH_MAX_LEN];
    char sass[QUERY_PATH_MAX_LEN];
    char tsv[QUERY_PATH_MAX_LEN];
    char cu[QUERY_PATH_MAX_LEN];
    char engine[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_STICK, .path_out = stick);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_WORK, .path_out = work);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/Lstar.klq", .path_out = klq);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = stick,
               .name_read = "measuring_stick_nvcc.sass", .path = sass);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = stick, .name_read = "measuring_stick.tsv",
               .path = tsv);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = stick, .name_read = "measuring_stick.cu",
               .path = cu);
    EMBED_CALL(query_pairs_folder_internal.in, QueryPairsFolderCtx, .folder = stick, .name_read = "engine",
               .path = engine);
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = folder, .err_msg = err_msg))
    {
        return -1;
    }
    long long code = -1LL;
    int stopped = 0;
    int whole = EMBED_CALL(query_pairs_part_context_internal.configs, QueryPairsPartContextCtx, .context = context,
                           .run_out = &s_run, .part = args->mode->part, .err_msg = err_msg);
    const char *const sliced[6] = {"slice", sass, tsv, cu, work, engine};
    whole = whole && EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                                .context = context, .run = &s_run, .identity = args->identity, .words_letters = sliced,
                                .count = 6u, .code = &code, .err_msg = err_msg);
    stopped = whole && (code != 0LL);
    whole = whole && !stopped &&
            EMBED_CALL(query_pairs_context_internal.host_answer, QueryPairsContextCtx, .context = context,
                       .run = &s_run, .out = work, .err_msg = err_msg);
    static QueryWords s_read;
    memor.set(&(MemoriaCfg){.dst = &s_read, .val = 0, .bytes = sizeof(s_read)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_read,
               .word_text = "read");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_read,
               .word_text = work);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_read,
               .word_text = klq);
    if (args->mode->word_count == 0u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_read,
                   .word_text = "sass.krs");
    }
    for (unsigned int at = 0u; at < args->mode->word_count; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_read,
                   .word_text = args->mode->words[at]);
    }
    whole = whole && !stopped &&
            EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                       .context = context, .run = &s_run, .identity = args->identity, .words_letters = s_read.word_text,
                       .count = s_read.count, .code = &code, .err_msg = err_msg);
    stopped = stopped || (whole && (code != 0LL));
    const char *const permuted[4] = {"permute", sass, tsv, work};
    whole = whole && !stopped &&
            EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                       .context = context, .run = &s_run, .identity = args->identity, .words_letters = permuted,
                       .count = 4u, .code = &code, .err_msg = err_msg);
    stopped = stopped || (whole && (code != 0LL));
    int read_back = 0;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = engine, .each = query_dis_find,
               .context_bytess = &read_back);
    if (whole && !stopped && read_back)
    {
        const char *const known[5] = {"known", sass, tsv, work, engine};
        whole = EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                           .context = context, .run = &s_run, .identity = args->identity, .words_letters = known,
                           .count = 5u, .code = &code, .err_msg = err_msg);
        stopped = whole && (code != 0LL);
        static QueryWords s_broken;
        memor.set(&(MemoriaCfg){.dst = &s_broken, .val = 0, .bytes = sizeof(s_broken)});
        static const char *const s_head[2] = {"broken", NULL};
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = s_head[0]);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = sass);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = tsv);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = work);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = engine);
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                   .word_text = "--");
        for (unsigned int at = 0u; at < s_read.count; at += 1u)
        {
            // the rulesets the read was given, every word after its folder and the bridge
            if (at >= 3u)
            {
                EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_broken,
                           .word_text = s_read.word_text[at]);
            }
        }
        whole =
            whole && !stopped &&
            EMBED_CALL(query_pairs_context_err_msg_run_internal.identity_run, QueryPairsContextErrMsgRunCtx,
                       .context = context, .run = &s_run, .identity = args->identity,
                       .words_letters = s_broken.word_text, .count = s_broken.count, .code = &code, .err_msg = err_msg);
    }
    else if (whole && !stopped)
    {
        code = 0LL;
    }
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // a step that did not end clean ends the mode with exit 1; an exit code is far below what an int represents
    return stopped ? 1 : (int)code;
}
