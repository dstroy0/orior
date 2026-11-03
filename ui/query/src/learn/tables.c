// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables.c: the vendor's tables: the instruction set and the latencies of every architecture its compiler lists,
// pulled off its disassembler. For each architecture nvcc compiles a key kernel to a cubin, and the disassembler for
// Linux, fetched once from the vendor's archive at the compiler's version, is run over the cubin in the test harness's
// Ubuntu image with every copy and heap block it makes written out (tables_intercept.c, tables_capture.sh), and the
// funnel reads the two tables off that capture (orior_tables_funnel). An architecture whose two tables stand already is
// retained as it is
#include "../../../cli/src/text/text.h"
#include "tables_internal.h"

// `folder`/`name` into `path`
void query_tables_in(const QueryTablesFolderCtx *args)
{
    VerbaLinea line = {args->path, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

// the tables of `arch` under `out`: its instructions and its latencies
void query_tables_get(const QueryTablesOutCtx *args)
{
    char name[128];
    VerbaLinea line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&line, "tables/");
    mmgr_line_put(&line, args->arch);
    mmgr_line_put(&line, "_instructions.txt");
    mmgr_line_finish(&line);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = args->out, .name = name,
               .path = args->instructions);
    line.at = 0ULL;
    mmgr_line_put(&line, "tables/");
    mmgr_line_put(&line, args->arch);
    mmgr_line_put(&line, "_latencies.txt");
    mmgr_line_finish(&line);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = args->out, .name = name,
               .path = args->latencies);
}

// the compiler's version, `V13.3.73`, read off `nvcc --version` into `version`: 1, or 0 where it names none
int query_version_get(const QueryTablesOutputCtx *args)
{
    unsigned long long size = args->size;
    for (unsigned long long at = 0ULL; (at + 1ULL) < size; at += 1ULL)
    {
        if ((args->output[at] != 'V') ||
            ((at != 0ULL) && (args->output[at - 1u] != ' ') && (args->output[at - 1u] != '\n')))
        {
            continue;
        }
        unsigned long long end = at + 1ULL;
        end = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->output, .at = end,
                         .size = size, .classes = CLI_TEXT_DIGITS, .set = ".");
        if ((end > (at + 1ULL)) &&
            ((end == size) || (args->output[end] == ' ') || (args->output[end] == '\n') || (args->output[end] == '\r')))
        {
            VerbaLinea line = {args->version, args->capacity, 0ULL};
            mmgr_line_put_n(&line, (const char *)(args->output + at + 1u), end - at - 1ULL);
            return mmgr_line_finish(&line) != 0ULL;
        }
    }
    return 0;
}

void query_archive_by_name(const QueryTablesCapacityCtx *args)
{
    VerbaLinea line = {args->archive, args->capacity, 0ULL};
    mmgr_line_put(&line, "cuda_nvdisasm-linux-x86_64-");
    mmgr_line_put(&line, args->version_read);
    mmgr_line_put(&line, "-archive");
    mmgr_line_finish(&line);
}

// the architectures `nvcc --list-gpu-arch` lists, `compute_86` read as `sm_86`, into `arches`: their count
unsigned int query_architectures_get(const QueryTablesCtx *args)
{
    unsigned long long size = args->size;
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    while ((at < size) && (count < QUERY_ARCHITECTURES_MAX))
    {
        at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = (const char *)args->output,
                        .at = at, .size = size, .set = " \n\r\t");
        const unsigned long long first = at;
        at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)args->output,
                        .at = at, .size = size, .set = " \n\r\t");
        if (((at - first) > 8ULL) && ((at - first) < 18ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = args->output + first, .other = "compute_", .bytes = 8u}) == 0))
        {
            VerbaLinea line = {args->arches[count], 16u, 0ULL};
            mmgr_line_put(&line, "sm_");
            mmgr_line_put_n(&line, (const char *)(args->output + first + 8u), at - first - 8ULL);
            count += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
        }
    }
    return count;
}

// `path` under the tree as the container sees it, the tree mounted at /tree, into `inside`
void query_in_container(const QueryTablesRootCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    const char *under = (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path_read,
                                    .other_pointer = args->root, .size = length) &&
                         ((args->path_read[length] == '/') || (args->path_read[length] == '\\')))
                            ? (args->path_read + length + 1u)
                            : args->path_read;
    VerbaLinea line = {args->inside, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "/tree/");
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = under, .from = '\\', .to = '/');
    mmgr_line_finish(&line);
}

// the program of `role` into `path`
int query_program(const QueryTablesPathCtx *args)
{
    return EMBED_CALL(args->context_read->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                      .context = args->context_read->programs->context, .role = args->role, .path = args->path,
                      .capacity = QUERY_PATH_MAX_LEN, .err_msg = args->err_msg);
}

// The architectures the words name, `sm_86 sm_90`, or every one the compiler lists where none is named, into
// `arches`: their count, or 0 with why given
unsigned int query_architectures(const QueryTablesErrMsgCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    const char *const *words = args->words;
    VerbaLinea *err_msg = args->err_msg;
    if (args->count != 0u)
    {
        unsigned int arch_count = 0u;
        for (unsigned int at = 0u; (at < args->count) && (arch_count < QUERY_ARCHITECTURES_MAX); at += 1u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->arches[arch_count],
                              .src = words[at],
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}) + 1u});
            arch_count += (args->arches[arch_count][0] != '\0') ? 1u : 0u;
        }
        return arch_count;
    }
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    long long code = -1LL;
    const char *const list_entries[3] = {s_nvcc, "--list-gpu-arch", NULL};
    if (!EMBED_CALL(query_tables_path_internal.program, QueryTablesPathCtx, .context_read = context,
                    .role = QUERY_ROLE_NVCC, .path = s_nvcc, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "nvcc_architectures.txt", .words = list_entries,
                    .code = &code, .err_msg = err_msg))
    {
        return 0u;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &run->writer,
                    .name = "nvcc_architectures.txt", .bytes_u8s = &bytes, .size_pointer = &size))
    {
        mmgr_line_put(err_msg, "the run contains no nvcc_architectures.txt");
        return 0u;
    }
    const unsigned int found = EMBED_CALL(query_tables_internal.architectures_get, QueryTablesCtx, .output = bytes,
                                          .size = size, .arches = args->arches);
    if (found == 0u)
    {
        mmgr_line_put(err_msg, "nvcc lists no architecture");
    }
    return found;
}

// the disassembler for Linux under the tables' folder, fetched and unpacked where it is not there, into `program`
int query_disassembler(const QueryTablesOutErrMsgCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    VerbaLinea *err_msg = args->err_msg;
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    long long code = -1LL;
    const char *const version_words[3] = {s_nvcc, "--version", NULL};
    if (!EMBED_CALL(query_tables_path_internal.program, QueryTablesPathCtx, .context_read = context,
                    .role = QUERY_ROLE_NVCC, .path = s_nvcc, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "nvcc_version.txt", .words = version_words, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    char version[64];
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &run->writer,
                    .name = "nvcc_version.txt", .bytes_u8s = &bytes, .size_pointer = &size) ||
        !EMBED_CALL(query_tables_output_internal.version_get, QueryTablesOutputCtx, .output = bytes, .size = size,
                    .version = version, .capacity = sizeof(version)))
    {
        mmgr_line_put(err_msg, "nvcc --version names no version");
        return 0;
    }
    char archive[128];
    EMBED_CALL(query_tables_capacity_internal.archive_by_name, QueryTablesCapacityCtx, .version_read = version,
               .archive = archive, .capacity = sizeof(archive));
    char download[QUERY_PATH_MAX_LEN];
    char unpacked[QUERY_PATH_MAX_LEN];
    char tar[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = args->out, .name = "download",
               .path = download);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = download, .name = archive,
               .path = unpacked);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = unpacked, .name = "bin/nvdisasm",
               .path = args->program);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->program))
    {
        return 1;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = download))
    {
        mmgr_line_put(err_msg, download);
        mmgr_line_put(err_msg, " could not be made");
        return 0;
    }
    char tar_name[160];
    VerbaLinea tar_line = {tar_name, sizeof(tar_name), 0ULL};
    mmgr_line_put(&tar_line, archive);
    mmgr_line_put(&tar_line, ".tar.xz");
    mmgr_line_finish(&tar_line);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = download, .name = tar_name,
               .path = tar);
    char url[512];
    VerbaLinea url_line = {url, sizeof(url), 0ULL};
    mmgr_line_put(&url_line, "https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvdisasm/linux-x86_64/");
    mmgr_line_put(&url_line, tar_name);
    mmgr_line_finish(&url_line);
    static char s_curl[QUERY_PATH_MAX_LEN];
    static char s_tar[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_tables_path_internal.program, QueryTablesPathCtx, .context_read = context,
                    .role = QUERY_ROLE_CURL, .path = s_curl, .err_msg = err_msg) ||
        !EMBED_CALL(query_tables_path_internal.program, QueryTablesPathCtx, .context_read = context,
                    .role = QUERY_ROLE_TAR, .path = s_tar, .err_msg = err_msg))
    {
        return 0;
    }
    char fetching[256];
    VerbaLinea fetching_line = {fetching, sizeof(fetching), 0ULL};
    mmgr_line_put(&fetching_line, "  the vendor's disassembler for Linux ");
    mmgr_line_put(&fetching_line, version);
    mmgr_line_put(&fetching_line, " fetched");
    if (mmgr_line_finish(&fetching_line) != 0ULL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = fetching);
    }
    const char *const fetch[7] = {s_curl, "-s", "-f", "-o", tar, url, NULL};
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "curl", .words = fetch, .code = &code,
                    .err_msg = err_msg) ||
        (code != 0LL))
    {
        mmgr_line_put(err_msg, "the vendor's archive could not be fetched: ");
        mmgr_line_put(err_msg, url);
        return 0;
    }
    const char *const unpack[6] = {s_tar, "-xf", tar, "-C", download, NULL};
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "tar", .words = unpack, .code = &code,
                    .err_msg = err_msg) ||
        (code != 0LL))
    {
        mmgr_line_put(err_msg, tar);
        mmgr_line_put(err_msg, " could not be unpacked");
        return 0;
    }
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->program))
    {
        mmgr_line_put(err_msg, tar);
        mmgr_line_put(err_msg, " contains no nvdisasm");
        return 0;
    }
    return 1;
}

// the key cubin of `arch` compiled where it is not there, into `cubin`
int query_key_cubin(const QueryTablesOutArchCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    VerbaLinea *err_msg = args->err_msg;
    char cubins[QUERY_PATH_MAX_LEN];
    char key[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = args->out, .name = "cubins",
               .path = cubins);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = cubins, .name = "key.cu", .path = key);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = cubins) ||
        (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = key) &&
         !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = key,
                     .bytes = QUERY_TABLES_KEY, .size = sizeof(QUERY_TABLES_KEY) - 1u)))
    {
        mmgr_line_put(err_msg, key);
        mmgr_line_put(err_msg, " could not be written");
        return 0;
    }
    char name[32];
    VerbaLinea name_line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&name_line, args->arch);
    mmgr_line_put(&name_line, ".cubin");
    mmgr_line_finish(&name_line);
    EMBED_CALL(query_tables_folder_internal.in, QueryTablesFolderCtx, .folder = cubins, .name = name,
               .path = args->cubin);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->cubin))
    {
        return 1;
    }
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    char flag[32];
    VerbaLinea flag_line = {flag, sizeof(flag), 0ULL};
    mmgr_line_put(&flag_line, "-arch=");
    mmgr_line_put(&flag_line, args->arch);
    mmgr_line_finish(&flag_line);
    long long code = -1LL;
    const char *const compile[7] = {s_nvcc, flag, "-cubin", "-o", args->cubin, key, NULL};
    if (!EMBED_CALL(query_tables_path_internal.program, QueryTablesPathCtx, .context_read = context,
                    .role = QUERY_ROLE_NVCC, .path = s_nvcc, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "nvcc", .words = compile, .code = &code,
                    .err_msg = err_msg) ||
        (code != 0LL))
    {
        mmgr_line_put(err_msg, "nvcc did not compile the key for ");
        mmgr_line_put(err_msg, args->arch);
        return 0;
    }
    return 1;
}
