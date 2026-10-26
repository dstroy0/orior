// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn_files.c: what the part passes share with the cross, the pairs and the writers: a program's list of words, the
// part's files in the tree and the vendor's tables, the carrier's words for a live pass, the base source through the
// vendor's compiler, and nvdisasm beside nvcc
#include "../query_loops.h"

void query_words_add(QueryWords *words, const char *word)
{
    if (words->count < QUERY_WORDS_MOST)
    {
        words->word[words->count] = word;
        words->count += 1u;
    }
    words->word[words->count] = NULL;
}

void query_words_text(QueryWords *words, const char *text)
{
    if (words->texts >= QUERY_TEXTS_MOST)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = words->text[words->texts],
                            .src = text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    query_words_add(words, words->text[words->texts]);
    words->texts += 1u;
}

// `root`/`folder`/`stem` and `.extension` where one is given, into `path`
static void query_joined(const char *root, const char *folder, const char *stem, const char *extension, char *path)
{
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, stem);
    if (extension != NULL)
    {
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, extension);
    }
    mmgr_line_finish(&line);
}

void query_files_of(const char *root, const char *part, QueryFiles *files)
{
    query_joined(root, QUERY_TABLE, part, "khw", files->machine);
    query_joined(root, QUERY_TABLE, part, "ksc", files->costs);
    query_joined(root, QUERY_TABLE, part, "kqr", files->record);
    query_joined(root, QUERY_NVIDIA, "elf64_nvidia.tsv", NULL, files->layout);
    query_joined(root, QUERY_NVIDIA, "mnemonic_nvidia.tsv", NULL, files->mnemonics);
    query_joined(root, QUERY_NVIDIA, "sass_nvidia.tsv", NULL, files->fields);
}

void query_carrier_words(QueryWords *words, const char *carrier, const char *cross, const char *nvdisasm,
                         const char *machine, const char *layout, int lessons)
{
    query_words_add(words, carrier);
    query_words_add(words, "--gated");
    query_words_add(words, "gated.txt");
    if (lessons)
    {
        query_words_add(words, "--lessons");
        query_words_add(words, "overseer.txt");
    }
    query_words_add(words, machine);
    query_words_add(words, layout);
    query_words_add(words, "--pop");
    query_words_add(words, carrier);
    query_words_add(words, "--dry");
    query_words_add(words, "--full");
    if (lessons)
    {
        query_words_add(words, "--lessons");
        query_words_add(words, "overseer.txt");
    }
    query_words_add(words, machine);
    query_words_add(words, layout);
    query_words_add(words, "--cross");
    query_words_add(words, cross);
    query_words_add(words, nvdisasm);
    query_words_add(words, "SM86");
    query_words_add(words, "airlock.md");
    query_words_add(words, "gated.txt");
}

int query_base_compiled(QueryContext *context, const QueryRun *run, const char *base, VerbaLinea *err_msg)
{
    if (!query_is_file(base))
    {
        return 1;
    }
    static char s_nvcc[QUERY_PATH_MAX_LEN];
    static char s_cuobjdump[QUERY_PATH_MAX_LEN];
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVCC, s_nvcc, sizeof(s_nvcc), err_msg) ||
        !context->programs->program(context->programs->context, QUERY_ROLE_CUOBJDUMP, s_cuobjdump, sizeof(s_cuobjdump),
                                    err_msg))
    {
        return 0;
    }
    char cubin[QUERY_PATH_MAX_LEN];
    query_joined(context->root, QUERY_LEARN_WORK, "base_nvcc.cubin", NULL, cubin);
    const char *const compile[8] = {s_nvcc, "-cubin", "-arch=sm_86", "-O3", "-o", cubin, base, NULL};
    long long code = -1ll;
    if (!query_context_handed(context, run, "nvcc.txt", compile, &code, err_msg))
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
            const int appended =
                query_writer_append(&run->writer, "base_nvcc.cubin", bytes, size, QRY_ORDINARY, err_msg);
            qry_member_file_unmapped(bytes, size);
            if (!appended)
            {
                return 0;
            }
        }
        const char *const listing[4] = {s_cuobjdump, "-sass", cubin, NULL};
        long long listing_code = -1ll;
        if (!query_context_handed(context, run, "base_nvcc.sass", listing, &listing_code, err_msg))
        {
            return 0;
        }
        listed = listing_code == 0ll;
    }
    const char *name = base;
    for (const char *at = base; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? (at + 1) : name;
    }
    char line[QUERY_LINE_MAX_LEN];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, listed ? "  the vendor's compiler read " : "  the vendor's compiler did not read ");
    mmgr_line_put(&print_line, name);
    mmgr_line_put(&print_line, listed ? "; its listing is the blob base_nvcc.sass" : "");
    if (mmgr_line_finish(&print_line) != 0ull)
    {
        query_print(context, line);
    }
    query_removed(cubin);
    return 1;
}

int query_nvdisasm(const QueryContext *context, char *path, VerbaLinea *err_msg)
{
    char why[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    if (!context->programs->program(context->programs->context, QUERY_ROLE_NVDISASM, path, QUERY_PATH_MAX_LEN,
                                    &ignored))
    {
        mmgr_line_put(err_msg, "no nvdisasm beside nvcc: the airlock has no reader, and nothing goes to the part");
        return 0;
    }
    return 1;
}
