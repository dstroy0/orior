// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_files.c: what the part passes share with the cross, the pairs and the writers: a program's list of words, the
// part's files in the tree and the vendor's tables, the carrier's words for a live pass, the base source through the
// vendor's compiler, and nvdisasm beside nvcc
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

void query_words_add(const QueryLoopsWordsWordsCfg *args)
{
    if (args->words_words_pointer->count < QUERY_WORDS_MOST)
    {
        args->words_words_pointer->word[args->words_words_pointer->count] = args->word;
        args->words_words_pointer->count += 1u;
    }
    args->words_words_pointer->word[args->words_words_pointer->count] = NULL;
}

void query_words_text(const QueryLoopsWordsWordsTextCfg *args)
{
    if (args->words_words_pointer->texts >= QUERY_TEXTS_MOST)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->words_words_pointer->text[args->words_words_pointer->texts],
                            .src = args->text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer,
               .word = args->words_words_pointer->text[args->words_words_pointer->texts]);
    args->words_words_pointer->texts += 1u;
}

typedef struct
{
    const char *root;
    const char *folder;
    const char *stem;
    const char *extension;
    char *path;
} LearnFilesCtx;
// `root`/`folder`/`stem` and `.extension` where one is given, into `path`
EMBED_INLINE void query_join(const LearnFilesCtx *args)
{
    VerbaLinea line = {args->path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->stem);
    if (args->extension != NULL)
    {
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, args->extension);
    }
    mmgr_line_finish(&line);
}

void query_files_get(const QueryLoopsRootCfg *args)
{
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_TABLE, .stem = args->part,
               .extension = "khw", .path = args->files->machine);
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_TABLE, .stem = args->part,
               .extension = "ksc", .path = args->files->costs);
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_TABLE, .stem = args->part,
               .extension = "kqr", .path = args->files->record);
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_NVIDIA, .stem = "elf64_nvidia.tsv",
               .extension = NULL, .path = args->files->layout);
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_NVIDIA, .stem = "mnemonic_nvidia.tsv",
               .extension = NULL, .path = args->files->mnemonics);
    EMBED_CALL(query_join, LearnFilesCtx, .root = args->root, .folder = QUERY_NVIDIA, .stem = "sass_nvidia.tsv",
               .extension = NULL, .path = args->files->fields);
}

void query_carrier_words(const QueryLoopsWordsWordsCarrierCfg *args)
{
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->carrier);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "--gated");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "gated.txt");
    if (args->lessons)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
                   .words_words_pointer = args->words_words_pointer, .word = "--lessons");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
                   .words_words_pointer = args->words_words_pointer, .word = "overseer.txt");
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->machine);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "--pop");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->carrier);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "--dry");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "--full");
    if (args->lessons)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
                   .words_words_pointer = args->words_words_pointer, .word = "--lessons");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
                   .words_words_pointer = args->words_words_pointer, .word = "overseer.txt");
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->machine);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "--cross");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->cross);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = args->nvdisasm);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "SM86");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "airlock.md");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg,
               .words_words_pointer = args->words_words_pointer, .word = "gated.txt");
}

int query_base_compile(const QueryLoopsContextErrMsgCfg *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->base))
    {
        return 1;
    }
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    static char s_cuobjdump[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(context->programs->program, QueryEachRolePathRoomErrMsgCfg, .context = context->programs->context,
                    .role = QUERY_ROLE_NVCC, .path = s_nvcc, .room = sizeof(s_nvcc), .err_msg = err_msg) ||
        !EMBED_CALL(context->programs->program, QueryEachRolePathRoomErrMsgCfg, .context = context->programs->context,
                    .role = QUERY_ROLE_CUOBJDUMP, .path = s_cuobjdump, .room = sizeof(s_cuobjdump), .err_msg = err_msg))
    {
        return 0;
    }
    char cubin[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_join, LearnFilesCtx, .root = context->root, .folder = QUERY_LEARN_WORK, .stem = "base_nvcc.cubin",
               .extension = NULL, .path = cubin);
    const char *const compile[8] = {s_nvcc, "-cubin", "-arch=sm_86", "-O3", "-o", cubin, args->base, NULL};
    long long code = -1ll;
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = args->run, .name = "nvcc.txt", .words = compile, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    int listed = 0;
    if (code == 0ll)
    {
        unsigned long long size = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(cubin, &size);
        if (bytes != NULL)
        {
            const int appended = EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx,
                                            .writer = &args->run->writer, .name = "base_nvcc.cubin", .bytes = bytes,
                                            .size = size, .priority = QRY_ORDINARY, .err_msg = err_msg);
            qry_member_file_unmapped(bytes, size);
            if (!appended)
            {
                return 0;
            }
        }
        const char *const listing[4] = {s_cuobjdump, "-sass", cubin, NULL};
        long long listing_code = -1ll;
        if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                        .context = context, .run = args->run, .name = "base_nvcc.sass", .words = listing,
                        .code = &listing_code, .err_msg = err_msg))
        {
            return 0;
        }
        listed = listing_code == 0ll;
    }
    const char *name = args->base;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->base,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->base + name_past) : name;
    char line[QUERY_LINE_MAX_LEN];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, listed ? "  the vendor's compiler read " : "  the vendor's compiler did not read ");
    mmgr_line_put(&print_line, name);
    mmgr_line_put(&print_line, listed ? "; its listing is the blob base_nvcc.sass" : "");
    if (mmgr_line_finish(&print_line) != 0ull)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = line);
    }
    EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = cubin);
    return 1;
}

int query_nvdisasm(const QueryLoopsErrMsgCfg *args)
{
    char why[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(args->context_read->programs->program, QueryEachRolePathRoomErrMsgCfg,
                    .context = args->context_read->programs->context, .role = QUERY_ROLE_NVDISASM, .path = args->path,
                    .room = QUERY_PATH_MAX_LEN, .err_msg = &ignored))
    {
        mmgr_line_put(args->err_msg,
                      "no nvdisasm beside nvcc: the airlock has no reader, and nothing goes to the part");
        return 0;
    }
    return 1;
}
