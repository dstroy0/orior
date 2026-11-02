// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stick.c: the measuring stick: every function of the CUDA language in a kernel of its own (measuring_stick.py),
// compiled by nvcc and disassembled, the answer key what the engine builds for each function is checked against; nvcc
// runs only where the stick's text changed since the container beside it was compiled. The engine's stick: each kernel
// read form by form through `cu.krs` and written in `sass.krs`, checked against nvcc's listing on the host, its
// encodings read back through nvdisasm. The read: nvcc's listing read kernel by kernel, and against `sass.krs`, into
// `measuring_stick.md` (measuring_stick_read.c). Nothing goes to a device. The listing and each kernel's read are
// handed to the run's writer under their paths, the files a run writes
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

// the stick's folder, the vendor's reading of it: the generator's text and manifest, nvcc's container and listing
#define QUERY_STICK "build/measuring_stick"

// the most codes of one folder the vendor reads at once
#define QUERY_CODES_MOST 4096u

typedef struct
{
    const char *folder;
    const char *name;
    char *path;
    const QueryContext *context_read;
    QueryRole role;
    VerbaLinea *err_msg;
    const QueryRun *run;
    const char *path_read;
    QueryContext *context;
    const QueryPaths *codes;
    const char *pattern;
    const char *prefix;
    const char *architecture;
    const char *const *words;
    unsigned int count;
} StickCtx;
// `folder`/`name` into `path`
EMBED_INLINE void query_in(const StickCtx *args)
{
    VerbaLinea line = {args->path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

void query_stick_paths(const QueryLoopsContextCfg *args)
{
    const QueryContext *context = args->context_read;
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = QUERY_STICK,
               .path_out = args->paths->out);
    EMBED_CALL(query_in, StickCtx, .folder = args->paths->out, .name = "measuring_stick.cu", .path = args->paths->cu);
    EMBED_CALL(query_in, StickCtx, .folder = args->paths->out, .name = "measuring_stick.cu.new",
               .path = args->paths->cu_new);
    EMBED_CALL(query_in, StickCtx, .folder = args->paths->out, .name = "measuring_stick.tsv", .path = args->paths->tsv);
    EMBED_CALL(query_in, StickCtx, .folder = args->paths->out, .name = "measuring_stick_nvcc.cubin",
               .path = args->paths->cubin);
    EMBED_CALL(query_in, StickCtx, .folder = args->paths->out, .name = "measuring_stick_nvcc.sass",
               .path = args->paths->sass);
}

// 1 where the stick's text at `fresh` differs from the one at `cu` nvcc compiled, or no container is there
int query_stick_change(const QueryLoopsCuCfg *args)
{
    unsigned long long old_size = 0ull;
    unsigned long long new_size = 0ull;
    const unsigned char *const before = qry_member_file_mapped(args->cu, &old_size);
    const unsigned char *const after = qry_member_file_mapped(args->fresh, &new_size);
    const int same = (before != NULL) && (after != NULL) && (old_size == new_size) &&
                     ((old_size == 0ull) ||
                      (memor.cmp(&(MemoriaCfg){.src = before, .other = after, .bytes = (size_t)old_size}) == 0));
    qry_member_file_unmapped(before, old_size);
    qry_member_file_unmapped(after, new_size);
    return !(same && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->cubin));
}

// the program of `role` into `path`, sized for QUERY_PATH_MAX_LEN
EMBED_INLINE int query_program(const StickCtx *args)
{
    return EMBED_CALL(args->context_read->programs->program, QueryEachRolePathRoomErrMsgCfg,
                      .context = args->context_read->programs->context, .role = args->role, .path = args->path,
                      .room = QUERY_PATH_MAX_LEN, .err_msg = args->err_msg);
}

// the latest blob of `name` handed to the run's writer as the file at `path`
EMBED_INLINE int query_blob_file(const StickCtx *args)
{
    const char *name = args->name;
    const char *path = args->path_read;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &args->run->writer,
                    .name = name, .bytes_u8s = &bytes, .size_pointer = &size))
    {
        mmgr_line_put(err_msg, "the run contains no ");
        mmgr_line_put(err_msg, name);
        return 0;
    }
    return EMBED_CALL(query_path_bytes_size_internal.writer_file, QueryPathBytesSizeCtx, .writer = &args->run->writer,
                      .path = path, .bytes = bytes, .size = size, .err_msg = err_msg);
}

// The text's generator and nvcc's container and listing: the generator run, nvcc where the text changed, and the
// listing read by cuobjdump
EMBED_INLINE int query_vendor_read(const StickCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    VerbaLinea *err_msg = args->err_msg;
    static QueryStickPaths s_paths;
    EMBED_CALL(query_loops_context.stick_paths, QueryLoopsContextCfg, .context_read = context, .paths = &s_paths);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_paths.out))
    {
        mmgr_line_put(err_msg, s_paths.out);
        mmgr_line_put(err_msg, " could not be made");
        return 0;
    }
    static char s_python[QUERY_PATH_MAX_LEN];
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    static char s_cuobjdump[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_program, StickCtx, .context_read = context, .role = QUERY_ROLE_PYTHON, .path = s_python,
                    .err_msg = err_msg))
    {
        return 0;
    }
    char generator[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_NVIDIA "/measuring_stick.py", .path_out = generator);
    const char *const generate[5] = {s_python, generator, s_paths.cu_new, s_paths.tsv, NULL};
    long long code = -1ll;
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "measuring_stick", .words = generate, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "the stick was not written");
        return 0;
    }
    if (!EMBED_CALL(query_loops_cu.stick_change, QueryLoopsCuCfg, .cu = s_paths.cu, .fresh = s_paths.cu_new,
                    .cubin = s_paths.cubin))
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = s_paths.cu_new);
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  the stick is unchanged: nvcc's cubin retained");
        return 1;
    }
    if (!EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = s_paths.cu_new, .to = s_paths.cu))
    {
        mmgr_line_put(err_msg, s_paths.cu);
        mmgr_line_put(err_msg, ": the new text could not be put in its place");
        return 0;
    }
    EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = s_paths.cubin);
    if (!EMBED_CALL(query_program, StickCtx, .context_read = context, .role = QUERY_ROLE_NVCC, .path = s_nvcc,
                    .err_msg = err_msg) ||
        !EMBED_CALL(query_program, StickCtx, .context_read = context, .role = QUERY_ROLE_CUOBJDUMP, .path = s_cuobjdump,
                    .err_msg = err_msg))
    {
        return 0;
    }
    static QueryWords s_compile;
    memor.set(&(MemoriaCfg){.dst = &s_compile, .val = 0, .bytes = sizeof(s_compile)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (EMBED_CALL(query_program, StickCtx, .context_read = context, .role = QUERY_ROLE_CL, .path = cl,
                   .err_msg = &ignored))
    {
        // the folder cl stands in, nvcc's host compiler
        char *const slash =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = cl,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), .byte = '\\');
        if (slash != NULL)
        {
            *slash = '\0';
            EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
                       .word = "-ccbin");
            EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg,
                       .words_words_pointer = &s_compile, .text = cl);
        }
    }
#endif
    static const char *const s_flags[7] = {"-cubin", "-arch=sm_86", "-O3", "-diag-suppress", "177", "-o", NULL};
    for (unsigned int at = 0u; s_flags[at] != NULL; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
                   .word = s_flags[at]);
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = s_paths.cubin);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_compile,
               .word = s_paths.cu);
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "nvcc", .words = s_compile.word, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "nvcc did not compile the stick");
        return 0;
    }
    const char *const listing[4] = {s_cuobjdump, "-sass", s_paths.cubin, NULL};
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "measuring_stick_nvcc.sass", .words = listing,
                    .code = &code, .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "cuobjdump did not read the stick");
        return 0;
    }
    return EMBED_CALL(query_blob_file, StickCtx, .run = run, .name = "measuring_stick_nvcc.sass",
                      .path_read = s_paths.sass, .err_msg = err_msg);
}

// what the walk over a folder's codes records: the folder and the codes found, sorted
typedef struct
{
    const char *folder;
    QueryPaths *codes;
} QueryCodesWalk;

static void query_code_walk(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    QueryCodesWalk *const walk = (QueryCodesWalk *)args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".bin", .cap = CLI_CELLS_POOL})) ||
        (walk->codes->count >= walk->codes->room))
    {
        return;
    }
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_in, StickCtx, .folder = walk->folder, .name = name, .path = path);
    char (*const codes)[QUERY_PATH_MAX_LEN] = walk->codes->path;
    unsigned int at = walk->codes->count;
    while ((at > 0u) &&
           (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = codes[at - 1u], .other_pointer = path) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = codes[at], .src = codes[at - 1u], .bytes = QUERY_PATH_MAX_LEN});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = codes[at], .src = path, .bytes = QUERY_PATH_MAX_LEN});
    walk->codes->count += 1u;
}

// the codes, a line each, handed to the run as `cubin_listing_codes.txt` from a cell of their size: 1, or 0 with why
// given
EMBED_INLINE int query_codes_dispatch(const StickCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ull;
    for (unsigned int at = 0u; at < args->codes->count; at += 1u)
    {
        size += cellul.len(&(CatenaFinitaCfg){.src = args->codes->path[at], .cap = QUERY_PATH_MAX_LEN}) + 1u;
    }
    char *const text = g_cli_cells->persistent_buf_alloc(size + 1ull, &g_query_cells);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "the codes the vendor reads could not be stored");
        return 0;
    }
    VerbaLinea list = {text, size + 1ull, 0ull};
    for (unsigned int at = 0u; at < args->codes->count; at += 1u)
    {
        mmgr_line_put(&list, args->codes->path[at]);
        mmgr_line_ch(&list, '\n');
    }
    const int handed =
        mmgr_line_ok(&list) && EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx,
                                          .writer = &args->run->writer, .name = "cubin_listing_codes.txt",
                                          .bytes = text, .size = list.at, .priority = QRY_ORDINARY, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(text, &g_query_cells);
    return handed;
}

// the vendor's listing of each code of `codes` read and filed: 1, or 0 with why given
EMBED_INLINE int query_codes_list(const StickCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    VerbaLinea *err_msg = args->err_msg;
    static char s_listing[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_cubin_listing", .binary = s_listing, .err_msg = err_msg) ||
        !EMBED_CALL(query_program, StickCtx, .context_read = context, .role = QUERY_ROLE_NVDISASM, .path = s_nvdisasm,
                    .err_msg = err_msg) ||
        !EMBED_CALL(query_codes_dispatch, StickCtx, .run = run, .codes = args->codes, .err_msg = err_msg))
    {
        return 0;
    }
    const char *const words[7] = {
        s_listing, args->pattern, args->prefix, args->architecture, s_nvdisasm, "cubin_listing_codes.txt", NULL};
    long long code = -1ll;
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "orior_cubin_listing", .words = words, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "the vendor did not read the codes");
        return 0;
    }
    for (unsigned int at = 0u; at < args->codes->count; at += 1u)
    {
        // the code's number, its file name's stem, and its listing's blob `<number>.dis`
        const char *const code = args->codes->path[at];
        const char *name =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = code,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), .byte = '/');
        name = (name != NULL) ? (name + 1) : code;
        char blob[QUERY_PATH_MAX_LEN];
        char dis[QUERY_PATH_MAX_LEN];
        VerbaLinea blob_line = {blob, sizeof(blob), 0ull};
        mmgr_line_put_most(&blob_line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) - 4u);
        mmgr_line_put(&blob_line, ".dis");
        mmgr_line_finish(&blob_line);
        VerbaLinea dis_line = {dis, sizeof(dis), 0ull};
        mmgr_line_put_most(&dis_line, code, cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}) - 4u);
        mmgr_line_put(&dis_line, ".dis");
        mmgr_line_finish(&dis_line);
        if (!EMBED_CALL(query_blob_file, StickCtx, .run = run, .name = blob, .path_read = dis, .err_msg = err_msg))
        {
            return 0;
        }
    }
    return 1;
}

int query_listings_read(const QueryLoopsContextFolderCfg *args)
{
    QueryPaths codes = {NULL, 0u, 0u};
    (void)EMBED_CALL(query_paths_internal.paths_take, QueryPathsCtx, .paths = &codes, .count = QUERY_CODES_MOST);
    QueryCodesWalk walk = {args->folder, &codes};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_code_walk, .context_bytess = &walk);
    const int read = (codes.count == 0u) ||
                     EMBED_CALL(query_codes_list, StickCtx, .context = args->context, .run = args->run,
                                .pattern = args->pattern, .prefix = args->prefix, .architecture = args->architecture,
                                .codes = &codes, .err_msg = args->err_msg);
    EMBED_CALL(query_paths_internal.paths_free, QueryPathsCtx, .paths = &codes);
    return read;
}

// the vendor's reading of the stick, and nvcc's listing read against `sass.krs` into the record
EMBED_INLINE int query_stick_vendor(const StickCtx *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    static char s_reader[QUERY_PATH_MAX_LEN];
    static QueryStickPaths s_paths;
    static QueryRun s_run;
    EMBED_CALL(query_loops_context.stick_paths, QueryLoopsContextCfg, .context_read = context, .paths = &s_paths);
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_read", .binary = s_reader, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_STICK "/qry", .err_msg = err_msg))
    {
        return -1;
    }
    char krs[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/sass.krs", .path_out = krs);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_NVIDIA "/measuring_stick.md", .path_out = record);
    long long code = -1ll;
    int whole = EMBED_CALL(query_vendor_read, StickCtx, .context = context, .run = &s_run, .err_msg = err_msg);
    if (whole)
    {
        // the record is the read's to write through the run's writer; named here as a leaf, so that it is shown and
        // requested
        const char *const left[1] = {record};
        EMBED_CALL(query_count_err_msg_internal.run_leave, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = left,
                   .count = 1u);
        const char *const words[6] = {s_reader, s_paths.sass, s_paths.tsv, krs, record, NULL};
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "orior_measuring_stick_read", .words = words,
                           .code = &code, .err_msg = err_msg);
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

// what the walk that clears the engine's folder records: the folder
static void query_engine_clear(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    static const char *const s_kinds[4] = {".sass", ".bin", ".dis", ".registers"};
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    for (unsigned int at = 0u; !args->is_folder && (dot != NULL) && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = s_kinds[at], .cap = CLI_CELLS_POOL}))
        {
            char path[QUERY_PATH_MAX_LEN];
            EMBED_CALL(query_in, StickCtx, .folder = (const char *)args->context, .name = name, .path = path);
            EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
        }
    }
}

// the engine's stick: each kernel written through the rulesets, its encodings read back, and the record written
// against nvcc's listing
EMBED_INLINE int query_stick_engine(const StickCtx *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    static char s_engine[QUERY_PATH_MAX_LEN];
    static char s_reader[QUERY_PATH_MAX_LEN];
    static QueryStickPaths s_paths;
    static QueryRun s_run;
    EMBED_CALL(query_loops_context.stick_paths, QueryLoopsContextCfg, .context_read = context, .paths = &s_paths);
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_engine", .binary = s_engine, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_read", .binary = s_reader, .err_msg = err_msg))
    {
        return -1;
    }
    char out[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_in, StickCtx, .folder = s_paths.out, .name = "engine", .path = out);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out))
    {
        mmgr_line_put(err_msg, out);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = out, .each = query_engine_clear,
               .context_bytess = out);
    char cu_krs[QUERY_PATH_MAX_LEN];
    char sass_krs[QUERY_PATH_MAX_LEN];
    char sass_ksc[QUERY_PATH_MAX_LEN];
    char machine[QUERY_PATH_MAX_LEN];
    char record[QUERY_PATH_MAX_LEN];
    char engine_tsv[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/cu.krs", .path_out = cu_krs);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/sass.krs", .path_out = sass_krs);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/sass.ksc", .path_out = sass_ksc);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/sm_86.khw", .path_out = machine);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_NVIDIA "/measuring_stick.md", .path_out = record);
    EMBED_CALL(query_in, StickCtx, .folder = out, .name = "engine.tsv", .path = engine_tsv);
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_STICK "/engine/qry", .err_msg = err_msg))
    {
        return -1;
    }
    const char *const read[7] = {cu_krs, sass_krs, sass_ksc, machine, s_paths.cu, s_paths.sass, s_paths.tsv};
    const char *const left[3] = {sass_krs, record, engine_tsv};
    int whole = EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = &s_run,
                           .paths_letters = read, .count = 7u, .err_msg = err_msg);
    EMBED_CALL(query_count_err_msg_internal.run_leave, QueryCountErrMsgCtx, .run = &s_run, .paths_letters = left,
               .count = 3u);
    long long code = -1ll;
    if (whole)
    {
        const char *const words[5] = {s_engine, s_paths.cu, s_paths.sass, out, NULL};
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "assembler.log", .words = words, .code = &code,
                           .err_msg = err_msg);
    }
    if (whole && (code != 0ll))
    {
        // the engine's refusal ends the loop: the leaves handed as the run contains them, exit 1
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  the engine did not write the stick");
        return EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context,
                          .run_out = &s_run, .err_msg = err_msg)
                   ? 1
                   : -1;
    }
    whole =
        whole && EMBED_CALL(query_loops_context_folder.listings_read, QueryLoopsContextFolderCfg, .context = context,
                            .run = &s_run, .pattern = s_paths.cubin, .prefix = "measuring_stick_",
                            .architecture = "SM86", .folder = out, .err_msg = err_msg);
    if (whole)
    {
        const char *const words[8] = {s_reader, s_paths.sass, s_paths.tsv, sass_krs, record, engine_tsv, out, NULL};
        whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                           .context = context, .run = &s_run, .name = "orior_measuring_stick_read", .words = words,
                           .code = &code, .err_msg = err_msg);
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

// the read with the words given, in the run joined or one of the stick's own
EMBED_INLINE int query_stick_read(const StickCtx *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    VerbaLinea *err_msg = args->err_msg;
    static char s_reader[QUERY_PATH_MAX_LEN];
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_read", .binary = s_reader, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_STICK "/engine/qry", .err_msg = err_msg))
    {
        return -1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
               .word = s_reader);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = &s_words,
                   .word = words[at]);
    }
    long long code = -1ll;
    const int whole = EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                                 .context = context, .run = &s_run, .name = "orior_measuring_stick_read",
                                 .words = s_words.word, .code = &code, .err_msg = err_msg);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg) ||
        !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}

int query_stick_words(const QueryLoopsErrMsgWordsCfg *args)
{
    if ((args->count == 0u) ||
        ((args->count == 1u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "vendor", .cap = CLI_CELLS_POOL}))))
    {
        return QUERY_STICK_VENDOR;
    }
    if ((args->count == 1u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "engine", .cap = CLI_CELLS_POOL})))
    {
        return QUERY_STICK_ENGINE;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->words[0], .other = "read", .cap = CLI_CELLS_POOL}))
    {
        return QUERY_STICK_READ;
    }
    mmgr_line_put(args->err_msg, "stick takes nothing, vendor, engine, or read <words>");
    return -1;
}

int query_stick_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    const int what = EMBED_CALL(query_loops_err_msg_words.stick_words, QueryLoopsErrMsgWordsCfg, .words = words,
                                .count = count, .err_msg = err_msg);
    if (what == QUERY_STICK_VENDOR)
    {
        return EMBED_CALL(query_stick_vendor, StickCtx, .context = context, .err_msg = err_msg);
    }
    if (what == QUERY_STICK_ENGINE)
    {
        return EMBED_CALL(query_stick_engine, StickCtx, .context = context, .err_msg = err_msg);
    }
    return (what == QUERY_STICK_READ) ? EMBED_CALL(query_stick_read, StickCtx, .context = context, .words = &words[1],
                                                   .count = count - 1u, .err_msg = err_msg)
                                      : -1;
}
