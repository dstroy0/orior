// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stick.c: the measuring stick: every function of the CUDA language in a kernel of its own (measuring_stick.py),
// compiled by nvcc and disassembled, the answer key what the engine builds for each function is held against; nvcc
// runs only where the stick's text changed since the container beside it was compiled. The engine's stick: each kernel
// read form by form through `cu.krs` and written in `sass.krs`, held against nvcc's listing on the host, its encodings
// read back through nvdisasm. The read: nvcc's listing read kernel by kernel, and against `sass.krs`, into
// `measuring_stick.md` (measuring_stick_read.c). Nothing goes to a device. The listing and each kernel's read are
// handed to the run's writer under their paths, the files a run writes
#include "../../../cli/src/text/text.h"
#include "../query_loops.h"

// the stick's folder, the vendor's reading of it: the generator's text and manifest, nvcc's container and listing
#define QUERY_STICK "build/measuring_stick"

// the most codes of one folder the vendor reads at once
#define QUERY_CODES_MOST 4096u

// `folder`/`name` into `path`
static void query_in(const char *folder, const char *name, char *path)
{
    CliLine line = {path, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

void query_stick_paths(const QueryContext *context, QueryStickPaths *paths)
{
    query_at(context, QUERY_STICK, paths->out);
    query_in(paths->out, "measuring_stick.cu", paths->cu);
    query_in(paths->out, "measuring_stick.cu.new", paths->cu_new);
    query_in(paths->out, "measuring_stick.tsv", paths->tsv);
    query_in(paths->out, "measuring_stick_nvcc.cubin", paths->cubin);
    query_in(paths->out, "measuring_stick_nvcc.sass", paths->sass);
}

// 1 where the stick's text at `fresh` differs from the one at `cu` nvcc compiled, or no container is there
int query_stick_changed(const char *cu, const char *fresh, const char *cubin)
{
    unsigned long long old_size = 0ull;
    unsigned long long new_size = 0ull;
    const unsigned char *const before = qry_member_file_mapped(cu, &old_size);
    const unsigned char *const after = qry_member_file_mapped(fresh, &new_size);
    const int same = (before != NULL) && (after != NULL) && (old_size == new_size) &&
                     ((old_size == 0ull) ||
                      (memor.cmp(&(MemoriaCfg){.src = before, .other = after, .bytes = (size_t)old_size}) == 0));
    if (before != NULL)
    {
        qry_member_file_unmapped(before, old_size);
    }
    if (after != NULL)
    {
        qry_member_file_unmapped(after, new_size);
    }
    return !(same && query_is_file(cubin));
}

// the program of `role` into `path`, which holds QUERY_PATH_LONGEST
static int query_program(const QueryContext *context, QueryRole role, char *path, CliLine *said)
{
    return context->programs->program(context->programs->context, role, path, QUERY_PATH_LONGEST, said);
}

// the latest blob of `name` handed to the run's writer as the file at `path`
static int query_blob_filed(const QueryRun *run, const char *name, const char *path, CliLine *said)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!query_writer_latest(&run->writer, name, &bytes, &size))
    {
        cli_line_text(said, "the run holds no ");
        cli_line_text(said, name);
        return 0;
    }
    return query_writer_file(&run->writer, path, bytes, size, said);
}

// The text's generator and nvcc's container and listing: the generator run, nvcc where the text changed, and the
// listing read by cuobjdump
static int query_vendor_read(QueryContext *context, const QueryRun *run, CliLine *said)
{
    static QueryStickPaths s_paths;
    query_stick_paths(context, &s_paths);
    if (!query_folder_made(s_paths.out))
    {
        cli_line_text(said, s_paths.out);
        cli_line_text(said, " could not be made");
        return 0;
    }
    static char s_python[QUERY_PATH_LONGEST];
    static char s_nvcc[QUERY_PATH_LONGEST];
    static char s_cuobjdump[QUERY_PATH_LONGEST];
    if (!query_program(context, QUERY_ROLE_PYTHON, s_python, said))
    {
        return 0;
    }
    char generator[QUERY_PATH_LONGEST];
    query_at(context, QUERY_NVIDIA "/measuring_stick.py", generator);
    const char *const generate[5] = {s_python, generator, s_paths.cu_new, s_paths.tsv, NULL};
    long long code = -1ll;
    if (!query_context_teed(context, run, "measuring_stick", generate, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "the stick was not written");
        return 0;
    }
    if (!query_stick_changed(s_paths.cu, s_paths.cu_new, s_paths.cubin))
    {
        query_removed(s_paths.cu_new);
        query_said(context, "  the stick is unchanged: nvcc's cubin kept");
        return 1;
    }
    if (!query_file_renamed(s_paths.cu_new, s_paths.cu))
    {
        cli_line_text(said, s_paths.cu);
        cli_line_text(said, ": the new text could not be put in its place");
        return 0;
    }
    query_removed(s_paths.cubin);
    if (!query_program(context, QUERY_ROLE_NVCC, s_nvcc, said) ||
        !query_program(context, QUERY_ROLE_CUOBJDUMP, s_cuobjdump, said))
    {
        return 0;
    }
    static QueryWords s_compile;
    memor.set(&(MemoriaCfg){.dst = &s_compile, .val = 0, .bytes = sizeof(s_compile)});
    query_words_add(&s_compile, s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_LONGEST];
    char ignored_text[QUERY_SAID_LONGEST];
    CliLine ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (query_program(context, QUERY_ROLE_CL, cl, &ignored))
    {
        // the folder cl stands in, nvcc's host compiler
        char *const slash = cli_text_last(cl, cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), '\\');
        if (slash != NULL)
        {
            *slash = '\0';
            query_words_add(&s_compile, "-ccbin");
            query_words_text(&s_compile, cl);
        }
    }
#endif
    static const char *const s_flags[7] = {"-cubin", "-arch=sm_86", "-O3", "-diag-suppress", "177", "-o", NULL};
    for (unsigned int at = 0u; s_flags[at] != NULL; at += 1u)
    {
        query_words_add(&s_compile, s_flags[at]);
    }
    query_words_add(&s_compile, s_paths.cubin);
    query_words_add(&s_compile, s_paths.cu);
    if (!query_context_teed(context, run, "nvcc", s_compile.word, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "nvcc did not compile the stick");
        return 0;
    }
    const char *const listing[4] = {s_cuobjdump, "-sass", s_paths.cubin, NULL};
    if (!query_context_handed(context, run, "measuring_stick_nvcc.sass", listing, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "cuobjdump did not read the stick");
        return 0;
    }
    return query_blob_filed(run, "measuring_stick_nvcc.sass", s_paths.sass, said);
}

// what the walk over a folder's codes keeps: the folder and the codes found, sorted
typedef struct
{
    const char *folder;
    QueryPaths *codes;
} QueryCodesWalk;

static void query_code_walked(void *context, const char *name, int is_folder)
{
    QueryCodesWalk *const walk = (QueryCodesWalk *)context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".bin", .cap = CLI_CELLS_POOL})) ||
        !query_paths_room(walk->codes, QUERY_CODES_MOST))
    {
        return;
    }
    char path[QUERY_PATH_LONGEST];
    query_in(walk->folder, name, path);
    char (*const codes)[QUERY_PATH_LONGEST] = walk->codes->path;
    unsigned int at = walk->codes->count;
    while ((at > 0u) && (cli_text_order(codes[at - 1u], path) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = codes[at], .src = codes[at - 1u], .bytes = QUERY_PATH_LONGEST});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = codes[at], .src = path, .bytes = QUERY_PATH_LONGEST});
    walk->codes->count += 1u;
}

// the codes, a line each, handed to the run as `cubin_listing_codes.txt` from a cell of their size: 1, or 0 with why
// said
static int query_codes_handed(const QueryRun *run, const QueryPaths *codes, CliLine *said)
{
    unsigned long long size = 0ull;
    for (unsigned int at = 0u; at < codes->count; at += 1u)
    {
        size += cellul.len(&(CatenaFinitaCfg){.src = codes->path[at], .cap = QUERY_PATH_LONGEST}) + 1u;
    }
    char *const text = cli_cell_taken_by(&g_query_cells, size + 1ull, "the codes the vendor reads");
    if (text == NULL)
    {
        cli_line_text(said, "the codes the vendor reads could not be held");
        return 0;
    }
    CliLine list = {text, size + 1ull, 0ull};
    for (unsigned int at = 0u; at < codes->count; at += 1u)
    {
        cli_line_text(&list, codes->path[at]);
        cli_line_character(&list, '\n');
    }
    const int handed = cli_line_fits(&list) &&
                       query_writer_append(&run->writer, "cubin_listing_codes.txt", text, list.at, QRY_ORDINARY, said);
    cli_cell_given_by(&g_query_cells, text);
    return handed;
}

// the vendor's listing of each code of `codes` read and filed: 1, or 0 with why said
static int query_codes_listed(QueryContext *context, const QueryRun *run, const char *pattern, const char *prefix,
                              const char *architecture, const QueryPaths *codes, CliLine *said)
{
    static char s_listing[QUERY_PATH_LONGEST];
    static char s_nvdisasm[QUERY_PATH_LONGEST];
    if (!query_context_tool(context, "orior_cubin_listing", s_listing, said) ||
        !query_program(context, QUERY_ROLE_NVDISASM, s_nvdisasm, said) || !query_codes_handed(run, codes, said))
    {
        return 0;
    }
    const char *const words[7] = {s_listing, pattern, prefix, architecture, s_nvdisasm, "cubin_listing_codes.txt",
                                  NULL};
    long long code = -1ll;
    if (!query_context_teed(context, run, "orior_cubin_listing", words, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "the vendor did not read the codes");
        return 0;
    }
    for (unsigned int at = 0u; at < codes->count; at += 1u)
    {
        // the code's number, its file name's stem, and its listing's blob `<number>.dis`
        const char *const code = codes->path[at];
        const char *name = cli_text_last(code, cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), '/');
        name = (name != NULL) ? (name + 1) : code;
        char blob[QUERY_PATH_LONGEST];
        char dis[QUERY_PATH_LONGEST];
        CliLine blob_line = {blob, sizeof(blob), 0ull};
        cli_line_text_most(&blob_line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) - 4u);
        cli_line_text(&blob_line, ".dis");
        cli_line_closed(&blob_line);
        CliLine dis_line = {dis, sizeof(dis), 0ull};
        cli_line_text_most(&dis_line, code, cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}) - 4u);
        cli_line_text(&dis_line, ".dis");
        cli_line_closed(&dis_line);
        if (!query_blob_filed(run, blob, dis, said))
        {
            return 0;
        }
    }
    return 1;
}

int query_listings_read(QueryContext *context, const QueryRun *run, const char *pattern, const char *prefix,
                        const char *architecture, const char *folder, CliLine *said)
{
    QueryPaths codes = {NULL, 0u, 0u};
    QueryCodesWalk walk = {folder, &codes};
    query_folder_each(folder, query_code_walked, &walk);
    const int read =
        (codes.count == 0u) || query_codes_listed(context, run, pattern, prefix, architecture, &codes, said);
    query_paths_given(&codes);
    return read;
}

// the vendor's reading of the stick, and nvcc's listing read against `sass.krs` into the record
static int query_stick_vendor(QueryContext *context, CliLine *said)
{
    static char s_reader[QUERY_PATH_LONGEST];
    static QueryStickPaths s_paths;
    static QueryRun s_run;
    query_stick_paths(context, &s_paths);
    if (!query_context_tool(context, "orior_measuring_stick_read", s_reader, said) ||
        !query_context_started(context, &s_run, QUERY_STICK "/qry", said))
    {
        return -1;
    }
    char krs[QUERY_PATH_LONGEST];
    char record[QUERY_PATH_LONGEST];
    query_at(context, QUERY_TABLE "/sass.krs", krs);
    query_at(context, QUERY_NVIDIA "/measuring_stick.md", record);
    long long code = -1ll;
    int whole = query_vendor_read(context, &s_run, said);
    if (whole)
    {
        // the record is the read's to write through the run's writer; named here as a leaf, so that it is shown and
        // asked
        const char *const left[1] = {record};
        query_run_left(&s_run, left, 1u);
        const char *const words[6] = {s_reader, s_paths.sass, s_paths.tsv, krs, record, NULL};
        whole = query_context_teed(context, &s_run, "orior_measuring_stick_read", words, &code, said);
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// what the walk that clears the engine's folder keeps: the folder
static void query_engine_cleared(void *context, const char *name, int is_folder)
{
    static const char *const s_kinds[4] = {".sass", ".bin", ".dis", ".registers"};
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    for (unsigned int at = 0u; !is_folder && (dot != NULL) && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = s_kinds[at], .cap = CLI_CELLS_POOL}))
        {
            char path[QUERY_PATH_LONGEST];
            query_in((const char *)context, name, path);
            query_removed(path);
        }
    }
}

// the engine's stick: each kernel written through the rulesets, its encodings read back, and the record written
// against nvcc's listing
static int query_stick_engine(QueryContext *context, CliLine *said)
{
    static char s_engine[QUERY_PATH_LONGEST];
    static char s_reader[QUERY_PATH_LONGEST];
    static QueryStickPaths s_paths;
    static QueryRun s_run;
    query_stick_paths(context, &s_paths);
    if (!query_context_tool(context, "orior_measuring_stick_engine", s_engine, said) ||
        !query_context_tool(context, "orior_measuring_stick_read", s_reader, said))
    {
        return -1;
    }
    char out[QUERY_PATH_LONGEST];
    query_in(s_paths.out, "engine", out);
    if (!query_folder_made(out))
    {
        cli_line_text(said, out);
        cli_line_text(said, " could not be made");
        return -1;
    }
    query_folder_each(out, query_engine_cleared, out);
    char cu_krs[QUERY_PATH_LONGEST];
    char sass_krs[QUERY_PATH_LONGEST];
    char sass_ksc[QUERY_PATH_LONGEST];
    char machine[QUERY_PATH_LONGEST];
    char record[QUERY_PATH_LONGEST];
    char engine_tsv[QUERY_PATH_LONGEST];
    query_at(context, QUERY_TABLE "/cu.krs", cu_krs);
    query_at(context, QUERY_TABLE "/sass.krs", sass_krs);
    query_at(context, QUERY_TABLE "/sass.ksc", sass_ksc);
    query_at(context, QUERY_TABLE "/sm_86.khw", machine);
    query_at(context, QUERY_NVIDIA "/measuring_stick.md", record);
    query_in(out, "engine.tsv", engine_tsv);
    if (!query_context_started(context, &s_run, QUERY_STICK "/engine/qry", said))
    {
        return -1;
    }
    const char *const read[7] = {cu_krs, sass_krs, sass_ksc, machine, s_paths.cu, s_paths.sass, s_paths.tsv};
    const char *const left[3] = {sass_krs, record, engine_tsv};
    int whole = query_run_read(&s_run, read, 7u, said);
    query_run_left(&s_run, left, 3u);
    long long code = -1ll;
    if (whole)
    {
        const char *const words[5] = {s_engine, s_paths.cu, s_paths.sass, out, NULL};
        whole = query_context_handed(context, &s_run, "assembler.log", words, &code, said);
    }
    if (whole && (code != 0ll))
    {
        // the engine's refusal ends the loop: the leaves handed as the run holds them, exit 1
        query_said(context, "  the engine did not write the stick");
        return query_context_ended(context, &s_run, said) ? 1 : -1;
    }
    whole = whole && query_listings_read(context, &s_run, s_paths.cubin, "measuring_stick_", "SM86", out, said);
    if (whole)
    {
        const char *const words[8] = {s_reader, s_paths.sass, s_paths.tsv, sass_krs, record, engine_tsv, out, NULL};
        whole = query_context_teed(context, &s_run, "orior_measuring_stick_read", words, &code, said);
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// the read with the words given, in the run joined or one of the stick's own
static int query_stick_read(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    static char s_reader[QUERY_PATH_LONGEST];
    static QueryRun s_run;
    if (!query_context_tool(context, "orior_measuring_stick_read", s_reader, said) ||
        !query_context_started(context, &s_run, QUERY_STICK "/engine/qry", said))
    {
        return -1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_words_add(&s_words, s_reader);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        query_words_add(&s_words, words[at]);
    }
    long long code = -1ll;
    const int whole = query_context_teed(context, &s_run, "orior_measuring_stick_read", s_words.word, &code, said);
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

int query_stick_words(const char *const *words, unsigned int count, CliLine *said)
{
    if ((count == 0u) ||
        ((count == 1u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "vendor", .cap = CLI_CELLS_POOL}))))
    {
        return QUERY_STICK_VENDOR;
    }
    if ((count == 1u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "engine", .cap = CLI_CELLS_POOL})))
    {
        return QUERY_STICK_ENGINE;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "read", .cap = CLI_CELLS_POOL}))
    {
        return QUERY_STICK_READ;
    }
    cli_line_text(said, "stick takes nothing, vendor, engine, or read <words>");
    return -1;
}

int query_stick_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    const int what = query_stick_words(words, count, said);
    if (what == QUERY_STICK_VENDOR)
    {
        return query_stick_vendor(context, said);
    }
    if (what == QUERY_STICK_ENGINE)
    {
        return query_stick_engine(context, said);
    }
    return (what == QUERY_STICK_READ) ? query_stick_read(context, &words[1], count - 1u, said) : -1;
}
