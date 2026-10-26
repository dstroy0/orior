// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs.c: the bridge's pairs put to the part, and the other modes of `orior_klq_identity` (klq_identity.cu): `stall`,
// `register`, `past`, `queue`, `cost` and `curve` over the stick's questions and the host's answers to them,
// `text_identity` over every identity between texts in `Lstar.klq`, each side a kernel of a stick of its own, and
// `stick`, the no-argument run, which slices the stick's questions into identities, computes the integer questions on
// the host over every case, and sifts the engine's writing through what nvcc writes. Every mode that puts questions
// runs them through the airlock, and the decoder reads a pair run's log back into the bridge. This file reads the words
// and gives the settings; pairs_modes.c runs the modes
#include "pairs_internal.h"

#include <stdlib.h>

static const char *const s_modes[11] = {"frame", "past",  "pair",  "cost",          "stall", "register",
                                        "queue", "curve", "fixed", "text_identity", "stick"};

// the words of a pairs read into `mode`: 1, or 0 with why given
int query_pairs_read(const char *const *words, unsigned int count, QueryPairsMode *mode, VerbaLinea *err_msg)
{
    static const char *const s_usage = "pairs takes frame|fixed|pair|cost|stall|register|past|queue|curve|"
                                       "text_identity|stick [<words>] [--part <part>] [--to <language>] [--dry] "
                                       "[--seed <n>] [--trace <folder>]";
    memor.set(&(MemoriaCfg){.dst = mode, .val = 0, .bytes = sizeof(*mode)});
    mode->part = "sm_86";
    mode->to = "sass";
    int known = 0;
    for (unsigned int at = 0u; (count != 0u) && (at < 11u); at += 1u)
    {
        known = known || (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = s_modes[at], .cap = CLI_CELLS_POOL}));
    }
    if (!known)
    {
        mmgr_line_put(err_msg, s_usage);
        return 0;
    }
    mode->mode = words[0];
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const char *const word = words[at];
        const char **const field =
            (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--part", .cap = CLI_CELLS_POOL}))    ? &mode->part
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--to", .cap = CLI_CELLS_POOL}))    ? &mode->to
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--seed", .cap = CLI_CELLS_POOL}))  ? &mode->seed
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--trace", .cap = CLI_CELLS_POOL})) ? &mode->trace
                                                                                                      : NULL;
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--dry", .cap = CLI_CELLS_POOL}))
        {
            mode->dry = 1;
        }
        else if (field != NULL)
        {
            if ((at + 1u) >= count)
            {
                mmgr_line_put(err_msg, word);
                mmgr_line_put(err_msg, " takes a value");
                return 0;
            }
            *field = words[at + 1u];
            at += 1u;
        }
        else if (mode->word_count < QUERY_WORDS_MOST)
        {
            mode->words[mode->word_count] = word;
            mode->word_count += 1u;
        }
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = mode->to, .other = "sass", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(err_msg, "orior_klq_identity writes forms in sass alone, and --to names ");
        mmgr_line_put(err_msg, mode->to);
        return 0;
    }
    return 1;
}

void query_part_read(const char *part, char *name, size_t room)
{
    VerbaLinea line = {name, room, 0ull};
    for (const char *at = part; *at != '\0'; at += 1)
    {
        if (*at != '_')
        {
            mmgr_line_ch(&line, ((*at >= 'a') && (*at <= 'z')) ? (char)(*at - 'a' + 'A') : *at);
        }
    }
    mmgr_line_finish(&line);
}

int query_pairs_carry(QueryContext *context, const char *part, int dry, QueryWords *words, VerbaLinea *err_msg)
{
    static char s_carrier[QUERY_PATH_MAX_LEN];
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    if (!query_context_tool(context, "orior_cubin_run", s_carrier, err_msg) ||
        !query_context_tool(context, "orior_measuring_stick_query", s_cross, err_msg) ||
        !query_nvdisasm(context, s_nvdisasm, err_msg))
    {
        return 0;
    }
    char machine[QUERY_PATH_MAX_LEN];
    char layout[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    mmgr_line_put(&line, QUERY_TABLE "/");
    mmgr_line_put(&line, part);
    mmgr_line_put(&line, ".khw");
    mmgr_line_finish(&line);
    query_at(context, name, machine);
    query_at(context, QUERY_NVIDIA "/elf64_nvidia.tsv", layout);
    query_pairs_carry_words(words, s_carrier, s_cross, s_nvdisasm, machine, layout, part, dry);
    return 1;
}

void query_pairs_carry_words(QueryWords *words, const char *carrier, const char *cross, const char *nvdisasm,
                             const char *machine, const char *layout, const char *part, int dry)
{
    char reader[64];
    query_part_read(part, reader, sizeof(reader));
    // the carrier, its dry flags first where `dry` as it reads them, its gated list and its lessons, the machine file
    // and the layout; the pop, dry and full; and the cross with the vendor's reader
    query_words_add(words, carrier);
    if (dry)
    {
        query_words_add(words, "--dry");
        query_words_add(words, "--full");
    }
    static const char *const s_feedback_words[4] = {"--gated", "gated.txt", "--lessons", "overseer.txt"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        query_words_add(words, s_feedback_words[at]);
    }
    query_words_text(words, machine);
    query_words_text(words, layout);
    query_words_add(words, "--pop");
    query_words_add(words, carrier);
    query_words_add(words, "--dry");
    query_words_add(words, "--full");
    query_words_add(words, "--lessons");
    query_words_add(words, "overseer.txt");
    query_words_text(words, machine);
    query_words_text(words, layout);
    query_words_add(words, "--cross");
    query_words_add(words, cross);
    query_words_add(words, nvdisasm);
    query_words_text(words, reader);
    query_words_add(words, "airlock.md");
    query_words_add(words, "gated.txt");
}

int query_pairs_configs(QueryContext *context, QueryRun *run, const char *part, VerbaLinea *err_msg)
{
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    static char s_paths[7][QUERY_PATH_MAX_LEN];
    static const char *s_read[7];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ull};
        mmgr_line_put(&line, QUERY_TABLE "/");
        mmgr_line_put(&line, part);
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, s_kinds[at]);
        mmgr_line_finish(&line);
        query_at(context, name, s_paths[at]);
    }
    query_at(context, QUERY_TABLE "/Lstar.klq", s_paths[3]);
    query_at(context, QUERY_TABLE "/cu.krs", s_paths[4]);
    query_at(context, QUERY_TABLE "/sass.krs", s_paths[5]);
    query_at(context, QUERY_PAIRS_STICK "/measuring_stick.tsv", s_paths[6]);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        s_read[at] = s_paths[at];
    }
    return query_run_read(run, s_read, 7u, err_msg);
}

int query_host_answered(QueryContext *context, const QueryRun *run, const char *out, VerbaLinea *err_msg)
{
    const unsigned char *questions = NULL;
    unsigned long long size = 0ull;
    if (!query_writer_latest(&run->writer, "host_questions.cpp", &questions, &size))
    {
        mmgr_line_put(err_msg, "the run contains no host_questions.cpp");
        return 0;
    }
    char source[QUERY_PATH_MAX_LEN];
    char program[QUERY_PATH_MAX_LEN];
    char base[64];
    VerbaLinea source_line = {source, sizeof(source), 0ull};
    mmgr_line_put(&source_line, out);
    mmgr_line_put(&source_line, "/host_questions.cpp");
    mmgr_line_finish(&source_line);
    query_binary_name("host_questions", base, sizeof(base));
    VerbaLinea program_line = {program, sizeof(program), 0ull};
    mmgr_line_put(&program_line, out);
    mmgr_line_ch(&program_line, '/');
    mmgr_line_put(&program_line, base);
    mmgr_line_finish(&program_line);
    if (!query_file_written(source, questions, size))
    {
        mmgr_line_put(err_msg, source);
        mmgr_line_put(err_msg, " could not be written");
        return 0;
    }
    static char s_cxx[QUERY_PATH_MAX_LEN];
    if (!context->programs->program(context->programs->context, QUERY_ROLE_CXX, s_cxx, sizeof(s_cxx), err_msg))
    {
        return 0;
    }
    const char *const compile[8] = {s_cxx, "-std=c++17", "-O0", "-fwrapv", "-w", "-o", program, NULL};
    static const char *s_compile[9];
    memor.cpy(&(MemoriaCfg){.dst = s_compile, .src = compile, .bytes = sizeof(compile)});
    s_compile[7] = source;
    s_compile[8] = NULL;
    long long code = -1ll;
    if (!query_context_teed(context, run, "host_questions_compile", s_compile, &code, err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "the host questions did not compile");
        return 0;
    }
    const char *const answer[2] = {program, NULL};
    if (!query_context_handed(context, run, "host_answers.txt", answer, &code, err_msg))
    {
        return 0;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "the host questions did not run");
        return 0;
    }
    return 1;
}

// the vendor's writer as the identity reads it (identity_settings.h): its program, its part, and the part's layout and
// mnemonics, each in its room and ended by a NUL
static unsigned char s_writer_setting[1024u + 64u + 1024u + 1024u];
static unsigned char s_seed_setting[8];
static char s_ruleset_setting[64];

static int query_identity_writer(const char *const *fields, VerbaLinea *err_msg)
{
    static const unsigned int s_rooms[4] = {1024u, 64u, 1024u, 1024u};
    memor.set(&(MemoriaCfg){.dst = s_writer_setting, .val = 0, .bytes = sizeof(s_writer_setting)});
    unsigned int at = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = fields[field], .cap = CLI_CELLS_POOL});
        if (length >= s_rooms[field])
        {
            mmgr_line_put(err_msg, fields[field]);
            mmgr_line_put(err_msg, " does not fit the writer's room");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = &s_writer_setting[at], .src = fields[field], .bytes = length});
        at += s_rooms[field];
    }
    return 1;
}

// Every mode: the stick's listing and the part's files read, the settings given, and the mode run
static int query_pairs_mode_run(QueryContext *context, const QueryPairsMode *mode, VerbaLinea *err_msg)
{
    char stick[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_PAIRS_STICK "/measuring_stick_nvcc.sass", stick);
    if (!query_is_file(stick))
    {
        mmgr_line_put(err_msg, "no stick: run orior_cli protocol stick");
        return -1;
    }
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_MAX_LEN];
        char path[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ull};
        mmgr_line_put(&line, QUERY_TABLE "/");
        mmgr_line_put(&line, mode->part);
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, s_kinds[at]);
        mmgr_line_finish(&line);
        query_at(context, name, path);
        if (!query_is_file(path))
        {
            mmgr_line_put(err_msg, "no ");
            mmgr_line_put(err_msg, mode->part);
            mmgr_line_ch(err_msg, '.');
            mmgr_line_put(err_msg, s_kinds[at]);
            mmgr_line_put(err_msg, " beside the bridge: --part names no part the tree contains");
            return -1;
        }
    }
    context->settings = 0u;
    if (mode->seed != NULL)
    {
        char *end = NULL;
        const unsigned long long seed = strtoull(mode->seed, &end, 10);
        if ((end == mode->seed) || (*end != '\0'))
        {
            mmgr_line_put(err_msg, "the seed ");
            mmgr_line_put(err_msg, mode->seed);
            mmgr_line_put(err_msg, " is no number");
            return -1;
        }
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            // the seed's bytes, the low first
            s_seed_setting[at] = (unsigned char)((seed >> (8u * at)) & 0xffull);
        }
        context->setting[context->settings] = (QuerySetting){"identity.seed", s_seed_setting, 8ull};
        context->settings += 1u;
    }
    // the vendor's writer, which assembles every text of the vendor's language the identity puts, and its words but
    // the machine file the mode names
    static char s_writer[QUERY_PATH_MAX_LEN];
    static QueryFiles s_files;
    if (!query_context_tool(context, "orior_khw_machine_write", s_writer, err_msg))
    {
        return -1;
    }
    query_files_of(context->root, mode->part, &s_files);
    const char *const fields[4] = {s_writer, mode->part, s_files.layout, s_files.mnemonics};
    if (!query_identity_writer(fields, err_msg))
    {
        return -1;
    }
    context->setting[context->settings] = (QuerySetting){"identity.writer", s_writer_setting, sizeof(s_writer_setting)};
    context->settings += 1u;
    // the ruleset of the language the mode writes forms in, which the identity reads its forms from
    VerbaLinea ruleset = {s_ruleset_setting, sizeof(s_ruleset_setting), 0ull};
    mmgr_line_put(&ruleset, mode->to);
    mmgr_line_put(&ruleset, ".krs");
    context->setting[context->settings] = (QuerySetting){"identity.ruleset", s_ruleset_setting, ruleset.at};
    context->settings += 1u;
    static char s_identity[QUERY_PATH_MAX_LEN];
    if (!query_context_tool(context, "orior_klq_identity", s_identity, err_msg))
    {
        return -1;
    }
    char work[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_PAIRS_WORK, work);
    if (!query_folder_made(work))
    {
        mmgr_line_put(err_msg, work);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    char folder[QUERY_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = folder,
        .src = (mode->trace != NULL) ? mode->trace : QUERY_PAIRS_FOLDER,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (mode->trace != NULL) ? mode->trace : QUERY_PAIRS_FOLDER,
                                               .cap = CLI_CELLS_POOL}) +
                 1u});
    int code = -1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "text_identity", .cap = CLI_CELLS_POOL}))
    {
        code = query_text_identity(context, mode, s_identity, folder, err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "stick", .cap = CLI_CELLS_POOL}))
    {
        code = query_stick_sliced(context, mode, s_identity, folder, err_msg);
    }
    else
    {
        code = query_judged(context, mode, s_identity, folder, err_msg);
    }
    context->settings = 0u;
    return code;
}

int query_pairs_pair(QueryContext *context, const char *part, const char *to, int dry, const char *seed,
                     const char *trace, VerbaLinea *err_msg)
{
    QueryPairsMode mode;
    memor.set(&(MemoriaCfg){.dst = &mode, .val = 0, .bytes = sizeof(mode)});
    mode.mode = "pair";
    mode.part = part;
    mode.to = to;
    mode.dry = dry;
    mode.seed = seed;
    mode.trace = trace;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = to, .other = "sass", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(err_msg, "orior_klq_identity writes forms in sass alone, and --to names ");
        mmgr_line_put(err_msg, to);
        return -1;
    }
    return query_pairs_mode_run(context, &mode, err_msg);
}

int query_pairs_run(QueryContext *context, const char *const *words, unsigned int count, VerbaLinea *err_msg)
{
    static QueryPairsMode s_mode;
    if (!query_pairs_read(words, count, &s_mode, err_msg))
    {
        return -1;
    }
    return query_pairs_mode_run(context, &s_mode, err_msg);
}
