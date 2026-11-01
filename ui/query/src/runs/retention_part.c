// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// retention_part.c: whether the part is free, the crystal's program built against the engine, and a folder's runs
// retained as their crystals
#include "retention_internal.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

// the output of `words` run, piped back, into `output`, sized for `room`, ended by a zero: 1 where it ran and ended 0
static int query_output_of(const char *const *words, char *output, size_t room)
{
    QueryProcess process;
    if (!query_process_start(&process, words, NULL, QUERY_OUTPUT_PIPED))
    {
        return 0;
    }
    unsigned long long length = 0ull;
    for (;;)
    {
        const unsigned long long got = query_process_read(&process, output + length, room - 1u - length);
        if (got == 0ull)
        {
            break;
        }
        length += got;
    }
    output[length] = '\0';
    return query_process_wait(&process) == 0ll;
}

// 1 where a line of `apps`, nvidia-smi's processes on the part, names a program under `root`, whichever separators and
// case the host writes the path with
int query_on_the_part(const char *apps, const char *root)
{
    char top[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {top, sizeof(top), 0ull};
    (void)cli_text_put_swapped(&line, root, '\\', '/');
    cli_text_lowered(top, line.at);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return 0;
    }
    // the lines as the root is written, in a cell of their size: where it is refused the answer is that a program
    // is there, which keeps a run from being taken for one the part has let go
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = apps, .cap = CLI_CELLS_POOL});
    char *const plain = g_cli_cells->temporary_buf_alloc(size + 1u, &g_query_cells);
    if (plain == NULL)
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = plain, .src = apps, .bytes = size + 1u});
    cli_text_swapped(plain, size, '\\', '/');
    cli_text_lowered(plain, size);
    const int there =
        cellul.find(&(CatenaFinitaCfg){.src = plain, .cap = size + 1u, .other = top, .other_cap = sizeof(top)}) != NULL;
    g_cli_cells->persistent_buf_release(plain, &g_query_cells);
    return there;
}

long long query_part_used_most(const char *given)
{
    if (given == NULL)
    {
        return 10ll;
    }
    char *end = NULL;
    const long long read = strtoll(given, &end, 10);
    return ((end != given) && (*end == '\0')) ? read : 10ll;
}

int query_part_free(QueryContext *context, VerbaLinea *err_msg)
{
    static char s_smi[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVIDIA_SMI, s_smi, sizeof(s_smi), &ignored))
    {
        mmgr_line_put(err_msg, "no nvidia-smi: the part is not read");
        return 0;
    }
    static char s_output[65536];
    const char *const apps[4] = {s_smi, "--query-compute-apps=process_name", "--format=csv,noheader", NULL};
    if (!query_output_of(apps, s_output, sizeof(s_output)))
    {
        mmgr_line_put(err_msg, "nvidia-smi did not answer");
        return 0;
    }
    if (query_on_the_part(s_output, context->root))
    {
        mmgr_line_put(err_msg, "a process of the tree is on the part");
        return 0;
    }
    // the part's allowed use in percent, as QRY_PART_USED gives it, 10 where it is not set or is no number; a negative
    // bound bounds the part used whatever it reads
    char given[32];
    const long long most =
        query_part_used_most(query_environment_read("QRY_PART_USED", given, sizeof(given)) ? given : NULL);
    const char *const used[4] = {s_smi, "--query-gpu=utilization.gpu", "--format=csv,noheader,nounits", NULL};
    for (unsigned int reading = 0u; reading < 5u; reading += 1u)
    {
        char *end = NULL;
        const long long percent =
            query_output_of(used, s_output, sizeof(s_output)) ? strtoll(s_output, &end, 10) : -1ll;
        if ((end == NULL) || (end == s_output))
        {
            mmgr_line_put(err_msg, "the part's use was not read");
            return 0;
        }
        if (percent > most)
        {
            mmgr_line_put(err_msg, "the part is used ");
            mmgr_line_decimal(err_msg, (unsigned long long)percent, 0u);
            mmgr_line_put(err_msg, " percent");
            return 0;
        }
        if (reading < 4u)
        {
            query_paused(1000u);
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

static unsigned long long query_size_under(const char *folder);

static void query_size_walked(void *context, const char *name, int is_folder)
{
    QuerySizeWalk *const walk = (QuerySizeWalk *)context;
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (is_folder)
    {
        walk->total += query_size_under(path);
        return;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
        walk->total += size;
    }
}

// the size of every file under `folder`
static unsigned long long query_size_under(const char *folder)
{
    QuerySizeWalk walk = {folder, 0ull};
    query_folder_each(folder, query_size_walked, &walk);
    return walk.total;
}

void query_crystals(QueryContext *context, const QueryWriter *own, const char *folder, const char *crystal)
{
    QueryRuns listed;
    (void)query_runs_taken(folder, &listed);
    char (*const paths)[QUERY_PATH_MAX_LEN] = listed.path;
    const unsigned int runs = listed.count;
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
        QueryWriter writer = *own;
        writer.member = 0;
        memor.cpy(&(MemoriaCfg){.dst = writer.qry,
                                .src = paths[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}) + 1u});
        if (query_writer_runs(&writer))
        {
            continue;
        }
        const char *const name =
            cli_text_last(paths[at], cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}), '/')
                ? (cli_text_last(paths[at], cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}),
                                 '/') +
                   1)
                : paths[at];
        char crystal_folder[QUERY_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = crystal_folder, .src = paths[at], .bytes = length - 4u});
        crystal_folder[length - 4u] = '\0';
        const unsigned long long began = query_now();
        static char s_output[4096];
        const char *const words[5] = {crystal, "write", paths[at], crystal_folder, NULL};
        const int written = query_output_of(words, s_output, sizeof(s_output));
        char text[QUERY_PATH_MAX_LEN + 4096u];
        VerbaLinea line = {text, sizeof(text), 0ull};
        unsigned long long size = 0ull;
        const unsigned char *const mapped = qry_member_file_mapped(paths[at], &size);
        if (mapped != NULL)
        {
            qry_member_file_unmapped(mapped, size);
        }
        if (written && (query_size_under(crystal_folder) >= size))
        {
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, " is stored as its .qry: its crystal is no smaller");
            query_removed(crystal_folder);
        }
        else if (written)
        {
            query_removed(paths[at]);
            // the crystal's own line, its end trimmed, and the seconds it took
            size_t len = cellul.len(&(CatenaFinitaCfg){.src = s_output, .cap = CLI_CELLS_POOL});
            len = (size_t)cli_text_back_past(s_output, len, "\n\r");
            mmgr_line_put_most(&line, s_output, len);
            mmgr_line_put(&line, ", in ");
            mmgr_line_decimal(&line, (query_now() - began) * 1000ull, 0u);
            mmgr_line_put(&line, " ms");
        }
        else
        {
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, " is stored as its .qry: its crystal did not check");
            query_removed(crystal_folder);
        }
        if (mmgr_line_finish(&line) != 0ull)
        {
            query_print(context, text);
        }
    }
    query_runs_given(&listed);
}

int query_crystal_built(QueryContext *context, char *binary)
{
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVCC, s_nvcc, sizeof(s_nvcc), &ignored))
    {
        return 0;
    }
    char engine[QUERY_PATH_MAX_LEN];
    query_at(context, "build/qry_crystal/engine", engine);
    char base[64];
    query_binary_name("qry_crystal", base, sizeof(base));
    VerbaLinea binary_line = {binary, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&binary_line, engine);
    mmgr_line_ch(&binary_line, '/');
    mmgr_line_put(&binary_line, base);
    mmgr_line_finish(&binary_line);
    if (!query_folder_made(engine))
    {
        return 0;
    }
    // the engine's changed sources compiled first through src/build_engine.sh, with BUILD_OUT naming its folder
    char script[QUERY_PATH_MAX_LEN];
    query_at(context, "src/build_engine.sh", script);
    query_environment_set("BUILD_OUT", engine);
    query_environment_set("PATH", context->programs->run_path(context->programs->context));
    const char *const engine_build[3] = {"bash", script, NULL};
    QueryProcess built;
    if (!query_process_start(&built, engine_build, NULL, QUERY_OUTPUT_NONE) || (query_process_wait(&built) != 0ll))
    {
        query_print(context, "  the crystal's engine did not build");
        return 0;
    }
    char source[QUERY_PATH_MAX_LEN];
    char header[QUERY_PATH_MAX_LEN];
    query_at(context, "src/cu/transpiler/lstar/protocol/qry_crystal.c", source);
    query_at(context, "src/cu/engine/engine.h", header);
    // on Windows the engine's objects, listed one a line in cell_tracking_engine.objects, are linked in
    // the list's text in a cell of its size, retained until the next build looks at it, the words naming its lines
    static char *s_objects_text;
    static const char *s_objects[QUERY_WORDS_MOST];
    g_cli_cells->persistent_buf_release(s_objects_text, &g_query_cells);
    s_objects_text = NULL;
    unsigned int objects = 0u;
#if defined(_WIN32)
    char listed[QUERY_PATH_MAX_LEN];
    VerbaLinea listed_line = {listed, sizeof(listed), 0ull};
    mmgr_line_put(&listed_line, engine);
    mmgr_line_put(&listed_line, "/cell_tracking_engine.objects");
    mmgr_line_finish(&listed_line);
    unsigned long long listed_size = 0ull;
    const unsigned char *const list = qry_member_file_mapped(listed, &listed_size);
    s_objects_text = (list != NULL) ? g_cli_cells->persistent_buf_alloc(listed_size + 1ull, &g_query_cells) : NULL;
    if (s_objects_text != NULL)
    {
        // a file mapped whole is counted by a size_t
        memor.cpy(&(MemoriaCfg){.dst = s_objects_text, .src = list, .bytes = (size_t)listed_size});
        s_objects_text[listed_size] = '\0';
        char *line = s_objects_text;
        while ((*line != '\0') && (objects < QUERY_WORDS_MOST))
        {
            char *const end_of = line + cli_text_upto(line, "\r\n");
            const char saved_char = *end_of;
            *end_of = '\0';
            if (*line != '\0')
            {
                s_objects[objects] = line;
                objects += 1u;
            }
            line = end_of + ((saved_char != '\0') ? 1 : 0);
            line += cli_text_over(line, "\r\n");
        }
    }
    if (list != NULL)
    {
        qry_member_file_unmapped(list, listed_size);
    }
#endif
    unsigned long long built_time = 0ull;
    unsigned long long changed = 0ull;
    int stale = !query_modified(binary, &built_time);
    stale = stale || (query_modified(source, &changed) && (changed > built_time));
    stale = stale || (query_modified(header, &changed) && (changed > built_time));
    for (unsigned int at = 0u; !stale && (at < objects); at += 1u)
    {
        stale = query_modified(s_objects[at], &changed) && (changed > built_time);
    }
    if (!stale)
    {
        return 1;
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    query_words_add(&s_words, s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_MAX_LEN];
    if (context->programs->program(context->programs->context, QUERY_ROLE_CL, cl, sizeof(cl), &ignored) &&
        (cli_text_last(cl, cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), '\\') != NULL))
    {
        cl[cli_text_last(cl, cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), '\\') - cl] = '\0';
        query_words_add(&s_words, "-ccbin");
        query_words_text(&s_words, cl);
        query_words_add(&s_words, "-Xcompiler");
        query_words_add(&s_words, "/std:c11 /O2 /Zc:preprocessor /D_CRT_SECURE_NO_WARNINGS");
    }
#endif
    char include[QUERY_PATH_MAX_LEN + 2u];
    char engine_folder[QUERY_PATH_MAX_LEN];
    query_at(context, "src/cu/engine", engine_folder);
    VerbaLinea include_line = {include, sizeof(include), 0ull};
    mmgr_line_put(&include_line, "-I");
    mmgr_line_put(&include_line, engine_folder);
    mmgr_line_finish(&include_line);
    query_words_add(&s_words, include);
    query_words_add(&s_words, "-o");
    query_words_add(&s_words, binary);
    query_words_add(&s_words, source);
#if defined(_WIN32)
    for (unsigned int at = 0u; at < objects; at += 1u)
    {
        query_words_add(&s_words, s_objects[at]);
    }
#else
    char library[QUERY_PATH_MAX_LEN + 2u];
    VerbaLinea library_line = {library, sizeof(library), 0ull};
    mmgr_line_put(&library_line, "-L");
    mmgr_line_put(&library_line, engine);
    mmgr_line_finish(&library_line);
    query_words_add(&s_words, library);
    query_words_add(&s_words, "-lcell_tracking_engine");
    query_words_add(&s_words, "-Xlinker");
    query_words_add(&s_words, "-rpath");
    query_words_add(&s_words, "-Xlinker");
    query_words_add(&s_words, engine);
#endif
    QueryProcess linked;
    if (!query_process_start(&linked, s_words.word, NULL, QUERY_OUTPUT_NONE) || (query_process_wait(&linked) != 0ll))
    {
        query_print(context, "  qry_crystal did not build; runs are stored as their .qry");
        return 0;
    }
    return 1;
}
