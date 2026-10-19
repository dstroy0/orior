// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept_part.c: whether the part is free, the crystal's program built against the engine, and a folder's runs kept as
// their crystals
#include "kept_internal.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

// the output of `words` run, piped back, into `output`, which holds `room`, ended by a zero: 1 where it ran and ended 0
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
    char top[QUERY_PATH_LONGEST];
    CliLine line = {top, sizeof(top), 0ull};
    for (const char *at = root; *at != '\0'; at += 1)
    {
        const char letter = (*at == '\\') ? '/' : *at;
        cli_line_character(&line, ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter);
    }
    if (cli_line_closed(&line) == 0ull)
    {
        return 0;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
    for (const char *walk = apps; *walk != '\0'; walk += 1)
    {
        size_t matched = 0u;
        while ((matched < length) && (walk[matched] != '\0') && (walk[matched] != '\n'))
        {
            char letter = (walk[matched] == '\\') ? '/' : walk[matched];
            letter = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
            if (letter != top[matched])
            {
                break;
            }
            matched += 1u;
        }
        if (matched == length)
        {
            return 1;
        }
    }
    return 0;
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

int query_part_free(QueryContext *context, CliLine *said)
{
    static char s_smi[QUERY_PATH_LONGEST];
    char ignored_text[QUERY_SAID_LONGEST];
    CliLine ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVIDIA_SMI, s_smi, sizeof(s_smi), &ignored))
    {
        cli_line_text(said, "no nvidia-smi: the part is not read");
        return 0;
    }
    static char s_output[65536];
    const char *const apps[4] = {s_smi, "--query-compute-apps=process_name", "--format=csv,noheader", NULL};
    if (!query_output_of(apps, s_output, sizeof(s_output)))
    {
        cli_line_text(said, "nvidia-smi did not answer");
        return 0;
    }
    if (query_on_the_part(s_output, context->root))
    {
        cli_line_text(said, "a process of the tree is on the part");
        return 0;
    }
    // the part's allowed use in percent, as QRY_PART_USED gives it, 10 where it is not set or is no number; a negative
    // bound holds the part used whatever it reads
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
            cli_line_text(said, "the part's use was not read");
            return 0;
        }
        if (percent > most)
        {
            cli_line_text(said, "the part is used ");
            cli_line_decimal(said, (unsigned long long)percent, 0u);
            cli_line_text(said, " percent");
            return 0;
        }
        if (reading < 4u)
        {
            query_paused(1000u);
        }
    }
    return 1;
}

// what the walk for the size of every file under a folder keeps
typedef struct
{
    const char *folder;
    unsigned long long total;
} QuerySizeWalk;

static unsigned long long query_size_under(const char *folder);

static void query_size_walked(void *context, const char *name, int is_folder)
{
    QuerySizeWalk *const walk = (QuerySizeWalk *)context;
    char path[QUERY_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, walk->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
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
    char (*const paths)[QUERY_PATH_LONGEST] = listed.path;
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
        char crystal_folder[QUERY_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){.dst = crystal_folder, .src = paths[at], .bytes = length - 4u});
        crystal_folder[length - 4u] = '\0';
        const unsigned long long began = query_now();
        static char s_output[4096];
        const char *const words[5] = {crystal, "write", paths[at], crystal_folder, NULL};
        const int written = query_output_of(words, s_output, sizeof(s_output));
        char text[QUERY_PATH_LONGEST + 4096u];
        CliLine line = {text, sizeof(text), 0ull};
        unsigned long long size = 0ull;
        const unsigned char *const held = qry_member_file_mapped(paths[at], &size);
        if (held != NULL)
        {
            qry_member_file_unmapped(held, size);
        }
        if (written && (query_size_under(crystal_folder) >= size))
        {
            cli_line_text(&line, "  ");
            cli_line_text(&line, name);
            cli_line_text(&line, " is kept as its .qry: its crystal is no smaller");
            query_removed(crystal_folder);
        }
        else if (written)
        {
            query_removed(paths[at]);
            // the crystal's own line, its end trimmed, and the seconds it took
            size_t said = cellul.len(&(CatenaFinitaCfg){.src = s_output, .cap = CLI_CELLS_POOL});
            while ((said != 0u) && ((s_output[said - 1u] == '\n') || (s_output[said - 1u] == '\r')))
            {
                said -= 1u;
            }
            cli_line_text_most(&line, s_output, said);
            cli_line_text(&line, ", in ");
            cli_line_decimal(&line, (query_now() - began) * 1000ull, 0u);
            cli_line_text(&line, " ms");
        }
        else
        {
            cli_line_text(&line, "  ");
            cli_line_text(&line, name);
            cli_line_text(&line, " is kept as its .qry: its crystal did not hold");
            query_removed(crystal_folder);
        }
        if (cli_line_closed(&line) != 0ull)
        {
            query_said(context, text);
        }
    }
    query_runs_given(&listed);
}

int query_crystal_built(QueryContext *context, char *binary)
{
    static char s_nvcc[QUERY_PATH_LONGEST];
    char ignored_text[QUERY_SAID_LONGEST];
    CliLine ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVCC, s_nvcc, sizeof(s_nvcc), &ignored))
    {
        return 0;
    }
    char engine[QUERY_PATH_LONGEST];
    query_at(context, "build/qry_crystal/engine", engine);
    char base[64];
    query_binary_name("qry_crystal", base, sizeof(base));
    CliLine binary_line = {binary, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&binary_line, engine);
    cli_line_character(&binary_line, '/');
    cli_line_text(&binary_line, base);
    cli_line_closed(&binary_line);
    if (!query_folder_made(engine))
    {
        return 0;
    }
    // the engine's changed sources compiled first through src/build_engine.sh, with BUILD_OUT naming its folder
    char script[QUERY_PATH_LONGEST];
    query_at(context, "src/build_engine.sh", script);
    query_environment_set("BUILD_OUT", engine);
    query_environment_set("PATH", context->programs->run_path(context->programs->context));
    const char *const engine_build[3] = {"bash", script, NULL};
    QueryProcess built;
    if (!query_process_start(&built, engine_build, NULL, QUERY_OUTPUT_NONE) || (query_process_wait(&built) != 0ll))
    {
        query_said(context, "  the crystal's engine did not build");
        return 0;
    }
    char source[QUERY_PATH_LONGEST];
    char header[QUERY_PATH_LONGEST];
    query_at(context, "src/cu/transpiler/lstar/protocol/qry_crystal.c", source);
    query_at(context, "src/cu/engine/engine.h", header);
    // on Windows the engine's objects, listed one a line in cell_tracking_engine.objects, are linked in
    // the list's text in a cell of its size, kept until the next build looks at it, the words naming its lines
    static char *s_objects_text;
    static const char *s_objects[QUERY_WORDS_MOST];
    cli_cell_given_by(&g_query_cells, s_objects_text);
    s_objects_text = NULL;
    unsigned int objects = 0u;
#if defined(_WIN32)
    char listed[QUERY_PATH_LONGEST];
    CliLine listed_line = {listed, sizeof(listed), 0ull};
    cli_line_text(&listed_line, engine);
    cli_line_text(&listed_line, "/cell_tracking_engine.objects");
    cli_line_closed(&listed_line);
    unsigned long long listed_size = 0ull;
    const unsigned char *const list = qry_member_file_mapped(listed, &listed_size);
    s_objects_text =
        (list != NULL) ? cli_cell_taken_by(&g_query_cells, listed_size + 1ull, "the engine's objects listed") : NULL;
    if (s_objects_text != NULL)
    {
        // a file mapped whole is counted by a size_t
        memor.cpy(&(MemoriaCfg){.dst = s_objects_text, .src = list, .bytes = (size_t)listed_size});
        s_objects_text[listed_size] = '\0';
        char *line = s_objects_text;
        while ((*line != '\0') && (objects < QUERY_WORDS_MOST))
        {
            char *const end_of = line + cli_text_upto(line, "\r\n");
            const char held = *end_of;
            *end_of = '\0';
            if (*line != '\0')
            {
                s_objects[objects] = line;
                objects += 1u;
            }
            line = end_of + ((held != '\0') ? 1 : 0);
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
    char cl[QUERY_PATH_LONGEST];
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
    char include[QUERY_PATH_LONGEST + 2u];
    char engine_folder[QUERY_PATH_LONGEST];
    query_at(context, "src/cu/engine", engine_folder);
    CliLine include_line = {include, sizeof(include), 0ull};
    cli_line_text(&include_line, "-I");
    cli_line_text(&include_line, engine_folder);
    cli_line_closed(&include_line);
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
    char library[QUERY_PATH_LONGEST + 2u];
    CliLine library_line = {library, sizeof(library), 0ull};
    cli_line_text(&library_line, "-L");
    cli_line_text(&library_line, engine);
    cli_line_closed(&library_line);
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
        query_said(context, "  qry_crystal did not build; runs are kept as their .qry");
        return 0;
    }
    return 1;
}
