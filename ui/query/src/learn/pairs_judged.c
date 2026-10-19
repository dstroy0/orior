// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_judged.c: the modes over the stick's questions and the host's answers, `stall`, `register`, `past`, `queue`,
// `pair`, `cost`, `curve`, `frame` and `fixed`, each put to the part through the airlock and a pair run's log read back
// by the decoder; and the stick's questions sliced into identities, the integer questions computed on the host over
// every case, each identity's verdict written to `Lstar.klq` beside the forms of the rulesets given, and the engine's
// writing of the stick sifted through what nvcc writes
#include "../../../cli/src/text/text.h"
#include "pairs_internal.h"

// the words a mode's program is given after its folder, added to `words`: the mode's own targets, or the words given
void query_targets(const QueryContext *context, const QueryPairsMode *mode, QueryWords *words)
{
    char path[QUERY_PATH_LONGEST];
    if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "queue", .cap = CLI_CELLS_POOL}))
    {
        query_at(context, QUERY_PAIRS_STICK "/measuring_stick.tsv", path);
        query_words_text(words, path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "pair", .cap = CLI_CELLS_POOL}))
    {
        query_at(context, QUERY_TABLE "/Lstar.klq", path);
        query_words_text(words, path);
        query_part_file(context, mode->part, ".khw", path);
        query_words_text(words, path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "stall", .cap = CLI_CELLS_POOL}))
    {
        query_part_file(context, mode->part, ".khw", path);
        query_words_text(words, path);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "cost", .cap = CLI_CELLS_POOL}))
    {
        query_part_file(context, mode->part, ".kdm", path);
        query_words_text(words, path);
        query_part_file(context, mode->part, ".khw", path);
        query_words_text(words, path);
    }
    else
    {
        for (unsigned int at = 0u; at < mode->word_count; at += 1u)
        {
            query_words_add(words, mode->words[at]);
        }
    }
}

// 1 where a mode opens with the kernel the system accepted: `frame` and `fixed`, which read it, and `pair`, whose
// opening runs both
int query_kernel_handed(const char *mode)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = mode, .other = "frame", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = mode, .other = "fixed", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = mode, .other = "pair", .cap = CLI_CELLS_POOL}));
}

void query_kernel_words(QueryWords *words, const char *part, const QueryFiles *files)
{
    query_words_add(words, part);
    query_words_add(words, files->machine);
    query_words_add(words, files->layout);
    query_words_add(words, files->mnemonics);
    query_words_add(words, "kernel.bin");
    query_words_add(words, "kernel");
}

int query_judged(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                 CliLine *said)
{
    char work[QUERY_PATH_LONGEST];
    char out[QUERY_PATH_LONGEST];
    char engine[QUERY_PATH_LONGEST];
    query_at(context, QUERY_PAIRS_WORK, work);
    query_pairs_in(work, mode->mode, out);
    query_at(context, QUERY_PAIRS_STICK "/engine", engine);
    if (!query_folder_made(out))
    {
        cli_line_text(said, out);
        cli_line_text(said, " could not be made");
        return -1;
    }
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, folder, said))
    {
        return -1;
    }
    static QueryFiles s_files;
    query_files_of(context->root, mode->part, &s_files);
    static char s_writer[QUERY_PATH_LONGEST];
    long long code = -1ll;
    int whole = query_context_tool(context, "khw_machine_write", s_writer, said);
    if (whole && query_kernel_handed(mode->mode))
    {
        // the kernel the system accepted, handed to the run by the vendor's writer for the frame's asks
        static QueryWords s_kernel;
        memor.set(&(MemoriaCfg){.dst = &s_kernel, .val = 0, .bytes = sizeof(s_kernel)});
        query_words_add(&s_kernel, s_writer);
        query_kernel_words(&s_kernel, mode->part, &s_files);
        whole = query_context_teed(context, &s_run, "khw_machine_write", s_kernel.word, &code, said);
        if (whole && (code != 0ll))
        {
            cli_line_text(said, "the vendor's writer handed the run no kernel");
            whole = 0;
        }
    }
    char host_answers[QUERY_PATH_LONGEST];
    char overseer[QUERY_PATH_LONGEST];
    query_pairs_in(work, "host_answers.txt", host_answers);
    query_pairs_in(work, "overseer.txt", overseer);
    char run_folder[QUERY_PATH_LONGEST];
    query_at(context, folder, run_folder);
    char crystal[QUERY_PATH_LONGEST];
    const int has_crystal = query_crystal(context->root, crystal, sizeof(crystal));
    const char *const carried[3] = {host_answers, overseer, "faulted.txt"};
    whole = whole && query_run_carried(&s_run, run_folder, carried, 3u, has_crystal ? crystal : NULL, said);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (whole && (!cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "frame", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "fixed", .cap = CLI_CELLS_POOL})) &&
        !query_writer_latest(&s_run.writer, "host_answers.txt", &bytes, &size))
    {
        cli_line_text(said, "no host answers: run orior protocol pairs stick first");
        whole = 0;
    }
    whole = whole && query_pairs_configs(context, &s_run, mode->part, said);
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "cost", .cap = CLI_CELLS_POOL})))
    {
        char kdm[QUERY_PATH_LONGEST];
        query_part_file(context, mode->part, ".kdm", kdm);
        const char *const read[1] = {kdm};
        whole = query_run_read(&s_run, read, 1u, said);
    }
    static QueryWords s_words;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    char ksc[QUERY_PATH_LONGEST];
    query_part_file(context, mode->part, ".ksc", ksc);
    query_words_add(&s_words, mode->mode);
    query_words_add(&s_words, engine);
    query_words_add(&s_words, "host_answers.txt");
    query_words_add(&s_words, ksc);
    query_words_add(&s_words, out);
    query_targets(context, mode, &s_words);
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "cost", .cap = CLI_CELLS_POOL})))
    {
        // the machine file's forms placed by the vendor's writer, handed to the run for the identity to cost in the
        // part's clock beside the arrangements
        const char *const placed[8] = {s_writer,          mode->part,      s_files.machine, s_files.layout,
                                       s_files.mnemonics, "krs_forms.bin", "krs",           NULL};
        whole = query_context_teed(context, &s_run, "khw_machine_write", placed, &code, said);
        if (whole && (code != 0ll))
        {
            cli_line_text(said, "the vendor's writer placed no forms");
            whole = 0;
        }
        query_words_add(&s_words, "krs_forms.bin");
    }
    query_words_add(&s_words, "--");
    whole = whole && query_pairs_carry(context, mode->part, mode->dry, &s_words, said) &&
            query_identity_ran(context, &s_run, identity, s_words.word, s_words.count, &code, said);
    if (whole && (code != 0ll))
    {
        return query_context_ended(context, &s_run, said) ? 1 : -1;
    }
    if (whole && (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "pair", .cap = CLI_CELLS_POOL})) &&
        query_writer_latest(&s_run.writer, "log", &bytes, &size))
    {
        static char s_decoder[QUERY_PATH_LONGEST];
        char klq[QUERY_PATH_LONGEST];
        query_at(context, QUERY_TABLE "/Lstar.klq", klq);
        const char *const decoded[5] = {s_decoder, s_run.writer.qry, engine, klq, NULL};
        whole = query_context_tool(context, "klq_decoder", s_decoder, said) &&
                query_context_teed(context, &s_run, "klq_decoder", decoded, &code, said);
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // an exit code is far below what an int holds
    return (int)code;
}

// what the walk for a read-back listing in the engine's folder keeps: whether one `.dis` stands there
static void query_dis_found(void *context, const char *name, int is_folder)
{
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    *(int *)context =
        *(int *)context || (!is_folder && (dot != NULL) &&
                            (cellul.eq(&(CatenaFinitaCfg){.src = dot, .other = ".dis", .cap = CLI_CELLS_POOL})));
}

int query_stick_sliced(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                       CliLine *said)
{
    char stick[QUERY_PATH_LONGEST];
    char work[QUERY_PATH_LONGEST];
    char klq[QUERY_PATH_LONGEST];
    char sass[QUERY_PATH_LONGEST];
    char tsv[QUERY_PATH_LONGEST];
    char cu[QUERY_PATH_LONGEST];
    char engine[QUERY_PATH_LONGEST];
    query_at(context, QUERY_PAIRS_STICK, stick);
    query_at(context, QUERY_PAIRS_WORK, work);
    query_at(context, QUERY_TABLE "/Lstar.klq", klq);
    query_pairs_in(stick, "measuring_stick_nvcc.sass", sass);
    query_pairs_in(stick, "measuring_stick.tsv", tsv);
    query_pairs_in(stick, "measuring_stick.cu", cu);
    query_pairs_in(stick, "engine", engine);
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, folder, said))
    {
        return -1;
    }
    long long code = -1ll;
    int stopped = 0;
    int whole = query_pairs_configs(context, &s_run, mode->part, said);
    const char *const sliced[6] = {"slice", sass, tsv, cu, work, engine};
    whole = whole && query_identity_ran(context, &s_run, identity, sliced, 6u, &code, said);
    stopped = whole && (code != 0ll);
    whole = whole && !stopped && query_host_answered(context, &s_run, work, said);
    static QueryWords s_read;
    memor.set(&(MemoriaCfg){.dst = &s_read, .val = 0, .bytes = sizeof(s_read)});
    query_words_add(&s_read, "read");
    query_words_add(&s_read, work);
    query_words_add(&s_read, klq);
    if (mode->word_count == 0u)
    {
        query_words_add(&s_read, "sass.krs");
    }
    for (unsigned int at = 0u; at < mode->word_count; at += 1u)
    {
        query_words_add(&s_read, mode->words[at]);
    }
    whole = whole && !stopped && query_identity_ran(context, &s_run, identity, s_read.word, s_read.count, &code, said);
    stopped = stopped || (whole && (code != 0ll));
    const char *const permuted[4] = {"permute", sass, tsv, work};
    whole = whole && !stopped && query_identity_ran(context, &s_run, identity, permuted, 4u, &code, said);
    stopped = stopped || (whole && (code != 0ll));
    int read_back = 0;
    query_folder_each(engine, query_dis_found, &read_back);
    if (whole && !stopped && read_back)
    {
        const char *const known[5] = {"known", sass, tsv, work, engine};
        whole = query_identity_ran(context, &s_run, identity, known, 5u, &code, said);
        stopped = whole && (code != 0ll);
        static QueryWords s_broken;
        memor.set(&(MemoriaCfg){.dst = &s_broken, .val = 0, .bytes = sizeof(s_broken)});
        static const char *const s_head[2] = {"broken", NULL};
        query_words_add(&s_broken, s_head[0]);
        query_words_add(&s_broken, sass);
        query_words_add(&s_broken, tsv);
        query_words_add(&s_broken, work);
        query_words_add(&s_broken, engine);
        query_words_add(&s_broken, "--");
        for (unsigned int at = 0u; at < s_read.count; at += 1u)
        {
            // the rulesets the read was given, every word after its folder and the bridge
            if (at >= 3u)
            {
                query_words_add(&s_broken, s_read.word[at]);
            }
        }
        whole = whole && !stopped &&
                query_identity_ran(context, &s_run, identity, s_broken.word, s_broken.count, &code, said);
    }
    else if (whole && !stopped)
    {
        code = 0ll;
    }
    if (!query_context_ended(context, &s_run, said) || !whole)
    {
        return -1;
    }
    // a step that did not end clean ends the mode with exit 1; an exit code is far below what an int holds
    return stopped ? 1 : (int)code;
}
