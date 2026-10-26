// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_modes.c: each mode of the identity run: every identity between texts in `Lstar.klq`, each side a kernel of a
// stick of its own; the modes over the stick's questions and the host's answers, each put to the part through the
// airlock and a pair run's log read back by the decoder; and the stick's questions sliced into identities
#include "../../../cli/src/text/text.h"
#include "pairs_internal.h"

// `folder`/`name` into `path`
void query_pairs_in(const char *folder, const char *name, char *path)
{
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    mmgr_line_finish(&line);
}

// `part` and `extension` under the bridge's folder into `path`
void query_part_file(const QueryContext *context, const char *part, const char *extension, char *path)
{
    char name[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    mmgr_line_put(&line, QUERY_TABLE "/");
    mmgr_line_put(&line, part);
    mmgr_line_put(&line, extension);
    mmgr_line_finish(&line);
    query_at(context, name, path);
}

// the identity run with `words` teed as `orior_klq_identity.console`: 1 where it ran and exited 0
int query_identity_ran(QueryContext *context, const QueryRun *run, const char *identity, const char *const *words,
                       unsigned int count, long long *code, VerbaLinea *err_msg)
{
    static const char *s_words[QUERY_WORDS_MOST + 2u];
    s_words[0] = identity;
    const unsigned int word_count = (count < QUERY_WORDS_MOST) ? count : QUERY_WORDS_MOST;
    memor.cpy(&(MemoriaCfg){.dst = &s_words[1], .src = words, .bytes = (size_t)word_count * sizeof(words[0])});
    s_words[word_count + 1u] = NULL;
    return query_context_teed(context, run, "orior_klq_identity", s_words, code, err_msg);
}

int query_text_identity(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                        VerbaLinea *err_msg)
{
    static char s_engine[QUERY_PATH_MAX_LEN];
    static char s_python[QUERY_PATH_MAX_LEN];
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    static char s_cuobjdump[QUERY_PATH_MAX_LEN];
    const QueryPrograms *const programs = context->programs;
    if (!query_context_tool(context, "orior_measuring_stick_engine", s_engine, err_msg) ||
        !programs->program(programs->context, QUERY_ROLE_PYTHON, s_python, sizeof(s_python), err_msg) ||
        !programs->program(programs->context, QUERY_ROLE_NVCC, s_nvcc, sizeof(s_nvcc), err_msg) ||
        !programs->program(programs->context, QUERY_ROLE_CUOBJDUMP, s_cuobjdump, sizeof(s_cuobjdump), err_msg))
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
    query_at(context, QUERY_PAIRS_WORK, work);
    query_pairs_in(work, "text_identity", sides);
    query_pairs_in(sides, "engine", engine_out);
    query_pairs_in(sides, "measuring_stick.cu", cu);
    query_pairs_in(sides, "measuring_stick_nvcc.cubin", cubin);
    query_pairs_in(sides, "measuring_stick_nvcc.sass", sass);
    query_pairs_in(sides, "measuring_stick.tsv", tsv);
    query_at(context, QUERY_NVIDIA "/measuring_stick.py", generator);
    query_at(context, QUERY_TABLE "/Lstar.klq", klq);
    memor.cpy(&(MemoriaCfg){.dst = run_folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, run_folder, err_msg))
    {
        return -1;
    }
    long long code = -1ll;
    int whole = query_pairs_configs(context, &s_run, mode->part, err_msg);
    query_removed(sides);
    whole = whole && query_folder_made(engine_out);
    const char *const generate[6] = {s_python, generator, cu, tsv, klq, NULL};
    whole = whole && query_context_teed(context, &s_run, "measuring_stick_sides", generate, &code, err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the sides were not written");
        whole = 0;
    }
    static QueryWords s_compile;
    memor.set(&(MemoriaCfg){.dst = &s_compile, .val = 0, .bytes = sizeof(s_compile)});
    query_words_add(&s_compile, s_nvcc);
#if defined(_WIN32)
    char cl[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ull};
    if (programs->program(programs->context, QUERY_ROLE_CL, cl, sizeof(cl), &ignored) &&
        (cli_text_last(cl, cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), '\\') != NULL))
    {
        cl[cli_text_last(cl, cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_CELLS_POOL}), '\\') - cl] = '\0';
        query_words_add(&s_compile, "-ccbin");
        query_words_text(&s_compile, cl);
    }
#endif
    char arch[64];
    VerbaLinea arch_line = {arch, sizeof(arch), 0ull};
    mmgr_line_put(&arch_line, "-arch=");
    mmgr_line_put(&arch_line, mode->part);
    mmgr_line_finish(&arch_line);
    static const char *const s_flags[6] = {"-cubin", NULL, "-O3", "-diag-suppress", "177", "-o"};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        query_words_add(&s_compile, (at == 1u) ? arch : s_flags[at]);
    }
    query_words_add(&s_compile, cubin);
    query_words_add(&s_compile, cu);
    whole = whole && query_context_teed(context, &s_run, "nvcc", s_compile.word, &code, err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "nvcc did not compile the sides");
        whole = 0;
    }
    const char *const listing[4] = {s_cuobjdump, "-sass", cubin, NULL};
    whole = whole && query_context_handed(context, &s_run, "measuring_stick_nvcc.sass", listing, &code, err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "cuobjdump did not read the sides");
        whole = 0;
    }
    // the listing written beside the sides, which the engine reads from the disk
    const unsigned char *listed = NULL;
    unsigned long long listed_size = 0ull;
    whole = whole && query_writer_latest(&s_run.writer, "measuring_stick_nvcc.sass", &listed, &listed_size) &&
            query_file_written(sass, listed, listed_size);
    const char *const engine_words[5] = {s_engine, cu, sass, engine_out, NULL};
    whole = whole && query_context_handed(context, &s_run, "assembler.log", engine_words, &code, err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the engine did not write the sides");
        whole = 0;
    }
    char reader[64];
    query_part_read(mode->part, reader, sizeof(reader));
    whole = whole && query_listings_read(context, &s_run, cubin, "measuring_stick_", reader, engine_out, err_msg);
    const char *const sliced[4] = {"slice", sass, tsv, engine_out};
    whole = whole && query_identity_ran(context, &s_run, identity, sliced, 4u, &code, err_msg);
    if (whole && (code != 0ll))
    {
        mmgr_line_put(err_msg, "the sides were not sliced");
        whole = 0;
    }
    whole = whole && query_host_answered(context, &s_run, sides, err_msg);
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    char ksc[QUERY_PATH_MAX_LEN];
    query_part_file(context, mode->part, ".ksc", ksc);
    query_words_add(&s_words, "text_identity");
    query_words_add(&s_words, engine_out);
    query_words_add(&s_words, "host_answers.txt");
    query_words_add(&s_words, ksc);
    query_words_add(&s_words, sides);
    query_words_add(&s_words, klq);
    query_words_add(&s_words, tsv);
    query_words_add(&s_words, "--");
    whole = whole && query_pairs_carry(context, mode->part, mode->dry, &s_words, err_msg) &&
            query_identity_ran(context, &s_run, identity, s_words.word, s_words.count, &code, err_msg);
    if (!query_context_ended(context, &s_run, err_msg) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}
