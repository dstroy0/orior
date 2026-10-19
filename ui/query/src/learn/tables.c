// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables.c: the vendor's tables: the instruction set and the latencies of every architecture its compiler lists,
// pulled off its disassembler. For each architecture nvcc compiles a key kernel to a cubin, and the disassembler for
// Linux, fetched once from the vendor's archive at the compiler's version, is run over the cubin in the test harness's
// Ubuntu image with every copy and heap block it makes written out (tables_intercept.c, tables_capture.sh), and the
// funnel reads the two tables off that capture (tables_funnel). An architecture whose two tables stand already is kept
// as it is
#include "../../../cli/src/text/text.h"
#include "tables_internal.h"

// `folder`/`name` into `path`
void query_tables_in(const char *folder, const char *name, char *path)
{
    CliLine line = {path, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// the tables of `arch` under `out`: its instructions and its latencies
void query_tables_of(const char *out, const char *arch, char *instructions, char *latencies)
{
    char name[128];
    CliLine line = {name, sizeof(name), 0ull};
    cli_line_text(&line, "tables/");
    cli_line_text(&line, arch);
    cli_line_text(&line, "_instructions.txt");
    cli_line_closed(&line);
    query_tables_in(out, name, instructions);
    line.at = 0ull;
    cli_line_text(&line, "tables/");
    cli_line_text(&line, arch);
    cli_line_text(&line, "_latencies.txt");
    cli_line_closed(&line);
    query_tables_in(out, name, latencies);
}

// the compiler's version, `V13.3.73`, read off `nvcc --version` into `version`: 1, or 0 where it names none
int query_version_of(const unsigned char *said, unsigned long long size, char *version, size_t room)
{
    for (unsigned long long at = 0ull; (at + 1ull) < size; at += 1ull)
    {
        if ((said[at] != 'V') || ((at != 0ull) && (said[at - 1u] != ' ') && (said[at - 1u] != '\n')))
        {
            continue;
        }
        unsigned long long end = at + 1ull;
        while ((end < size) && (((said[end] >= '0') && (said[end] <= '9')) || (said[end] == '.')))
        {
            end += 1ull;
        }
        if ((end > (at + 1ull)) && ((end == size) || (said[end] == ' ') || (said[end] == '\n') || (said[end] == '\r')))
        {
            CliLine line = {version, room, 0ull};
            cli_line_counted(&line, (const char *)(said + at + 1u), end - at - 1ull);
            return cli_line_closed(&line) != 0ull;
        }
    }
    return 0;
}

void query_archive_named(const char *version, char *archive, size_t room)
{
    CliLine line = {archive, room, 0ull};
    cli_line_text(&line, "cuda_nvdisasm-linux-x86_64-");
    cli_line_text(&line, version);
    cli_line_text(&line, "-archive");
    cli_line_closed(&line);
}

// the architectures `nvcc --list-gpu-arch` lists, `compute_86` read as `sm_86`, into `arches`: their count
unsigned int query_architectures_of(const unsigned char *said, unsigned long long size, char (*arches)[16])
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    while ((at < size) && (count < QUERY_ARCHITECTURES_MOST))
    {
        while ((at < size) && ((said[at] == ' ') || (said[at] == '\n') || (said[at] == '\r') || (said[at] == '\t')))
        {
            at += 1ull;
        }
        const unsigned long long first = at;
        while ((at < size) && (said[at] != ' ') && (said[at] != '\n') && (said[at] != '\r') && (said[at] != '\t'))
        {
            at += 1ull;
        }
        if (((at - first) > 8ull) && ((at - first) < 18ull) &&
            (memor.cmp(&(MemoriaCfg){.src = said + first, .other = "compute_", .bytes = 8u}) == 0))
        {
            CliLine line = {arches[count], 16u, 0ull};
            cli_line_text(&line, "sm_");
            cli_line_counted(&line, (const char *)(said + first + 8u), at - first - 8ull);
            count += (cli_line_closed(&line) != 0ull) ? 1u : 0u;
        }
    }
    return count;
}

// `path` under the tree as the container sees it, the tree mounted at /tree, into `inside`
void query_in_container(const char *root, const char *path, char *inside)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    const char *under = (cli_text_same(path, root, length) && ((path[length] == '/') || (path[length] == '\\')))
                            ? (path + length + 1u)
                            : path;
    CliLine line = {inside, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, "/tree/");
    for (const char *at = under; *at != '\0'; at += 1)
    {
        cli_line_character(&line, (*at == '\\') ? '/' : *at);
    }
    cli_line_closed(&line);
}

// the program of `role` into `path`
int query_program(const QueryContext *context, QueryRole role, char *path, CliLine *said)
{
    return context->programs->program(context->programs->context, role, path, QUERY_PATH_LONGEST, said);
}

// The architectures the words name, `sm_86 sm_90`, or every one the compiler lists where none is named, into
// `arches`: their count, or 0 with why said
unsigned int query_architectures(QueryContext *context, const QueryRun *run, const char *const *words,
                                 unsigned int count, char (*arches)[16], CliLine *said)
{
    if (count != 0u)
    {
        unsigned int kept = 0u;
        for (unsigned int at = 0u; (at < count) && (kept < QUERY_ARCHITECTURES_MOST); at += 1u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = arches[kept],
                              .src = words[at],
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}) + 1u});
            kept += (arches[kept][0] != '\0') ? 1u : 0u;
        }
        return kept;
    }
    static char s_nvcc[QUERY_PATH_LONGEST];
    long long code = -1ll;
    const char *const listed[3] = {s_nvcc, "--list-gpu-arch", NULL};
    if (!query_program(context, QUERY_ROLE_NVCC, s_nvcc, said) ||
        !query_context_handed(context, run, "nvcc_architectures.txt", listed, &code, said))
    {
        return 0u;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!query_writer_latest(&run->writer, "nvcc_architectures.txt", &bytes, &size))
    {
        cli_line_text(said, "the run holds no nvcc_architectures.txt");
        return 0u;
    }
    const unsigned int found = query_architectures_of(bytes, size, arches);
    if (found == 0u)
    {
        cli_line_text(said, "nvcc lists no architecture");
    }
    return found;
}

// the disassembler for Linux under the tables' folder, fetched and unpacked where it is not there, into `program`
int query_disassembler(QueryContext *context, const QueryRun *run, const char *out, char *program, CliLine *said)
{
    static char s_nvcc[QUERY_PATH_LONGEST];
    long long code = -1ll;
    const char *const asked[3] = {s_nvcc, "--version", NULL};
    if (!query_program(context, QUERY_ROLE_NVCC, s_nvcc, said) ||
        !query_context_handed(context, run, "nvcc_version.txt", asked, &code, said))
    {
        return 0;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    char version[64];
    if (!query_writer_latest(&run->writer, "nvcc_version.txt", &bytes, &size) ||
        !query_version_of(bytes, size, version, sizeof(version)))
    {
        cli_line_text(said, "nvcc --version names no version");
        return 0;
    }
    char archive[128];
    query_archive_named(version, archive, sizeof(archive));
    char download[QUERY_PATH_LONGEST];
    char unpacked[QUERY_PATH_LONGEST];
    char tar[QUERY_PATH_LONGEST];
    query_tables_in(out, "download", download);
    query_tables_in(download, archive, unpacked);
    query_tables_in(unpacked, "bin/nvdisasm", program);
    if (query_is_file(program))
    {
        return 1;
    }
    if (!query_folder_made(download))
    {
        cli_line_text(said, download);
        cli_line_text(said, " could not be made");
        return 0;
    }
    char tar_name[160];
    CliLine tar_line = {tar_name, sizeof(tar_name), 0ull};
    cli_line_text(&tar_line, archive);
    cli_line_text(&tar_line, ".tar.xz");
    cli_line_closed(&tar_line);
    query_tables_in(download, tar_name, tar);
    char url[512];
    CliLine url_line = {url, sizeof(url), 0ull};
    cli_line_text(&url_line, "https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvdisasm/linux-x86_64/");
    cli_line_text(&url_line, tar_name);
    cli_line_closed(&url_line);
    static char s_curl[QUERY_PATH_LONGEST];
    static char s_tar[QUERY_PATH_LONGEST];
    if (!query_program(context, QUERY_ROLE_CURL, s_curl, said) || !query_program(context, QUERY_ROLE_TAR, s_tar, said))
    {
        return 0;
    }
    char fetching[256];
    CliLine fetching_line = {fetching, sizeof(fetching), 0ull};
    cli_line_text(&fetching_line, "  the vendor's disassembler for Linux ");
    cli_line_text(&fetching_line, version);
    cli_line_text(&fetching_line, " fetched");
    if (cli_line_closed(&fetching_line) != 0ull)
    {
        query_said(context, fetching);
    }
    const char *const fetch[7] = {s_curl, "-s", "-f", "-o", tar, url, NULL};
    if (!query_context_teed(context, run, "curl", fetch, &code, said) || (code != 0ll))
    {
        cli_line_text(said, "the vendor's archive could not be fetched: ");
        cli_line_text(said, url);
        return 0;
    }
    const char *const unpack[6] = {s_tar, "-xf", tar, "-C", download, NULL};
    if (!query_context_teed(context, run, "tar", unpack, &code, said) || (code != 0ll))
    {
        cli_line_text(said, tar);
        cli_line_text(said, " could not be unpacked");
        return 0;
    }
    if (!query_is_file(program))
    {
        cli_line_text(said, tar);
        cli_line_text(said, " holds no nvdisasm");
        return 0;
    }
    return 1;
}

// the key cubin of `arch` compiled where it is not there, into `cubin`
int query_key_cubin(QueryContext *context, const QueryRun *run, const char *out, const char *arch, char *cubin,
                    CliLine *said)
{
    char cubins[QUERY_PATH_LONGEST];
    char key[QUERY_PATH_LONGEST];
    query_tables_in(out, "cubins", cubins);
    query_tables_in(cubins, "key.cu", key);
    if (!query_folder_made(cubins) ||
        (!query_is_file(key) && !query_file_written(key, QUERY_TABLES_KEY, sizeof(QUERY_TABLES_KEY) - 1u)))
    {
        cli_line_text(said, key);
        cli_line_text(said, " could not be written");
        return 0;
    }
    char name[32];
    CliLine name_line = {name, sizeof(name), 0ull};
    cli_line_text(&name_line, arch);
    cli_line_text(&name_line, ".cubin");
    cli_line_closed(&name_line);
    query_tables_in(cubins, name, cubin);
    if (query_is_file(cubin))
    {
        return 1;
    }
    static char s_nvcc[QUERY_PATH_LONGEST];
    char flag[32];
    CliLine flag_line = {flag, sizeof(flag), 0ull};
    cli_line_text(&flag_line, "-arch=");
    cli_line_text(&flag_line, arch);
    cli_line_closed(&flag_line);
    long long code = -1ll;
    const char *const compile[7] = {s_nvcc, flag, "-cubin", "-o", cubin, key, NULL};
    if (!query_program(context, QUERY_ROLE_NVCC, s_nvcc, said) ||
        !query_context_teed(context, run, "nvcc", compile, &code, said) || (code != 0ll))
    {
        cli_line_text(said, "nvcc did not compile the key for ");
        cli_line_text(said, arch);
        return 0;
    }
    return 1;
}
