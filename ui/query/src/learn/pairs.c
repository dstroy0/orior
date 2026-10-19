// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs.c: the bridge's pairs put to the part, and the other modes of `klq_identity` (klq_identity.cu): `stall`,
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

// the words of a pairs read into `mode`: 1, or 0 with why said
int query_pairs_read(const char *const *words, unsigned int count, QueryPairsMode *mode, CliLine *said)
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
        cli_line_text(said, s_usage);
        return 0;
    }
    mode->mode = words[0];
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const char *const word = words[at];
        const char **const slot =
            (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--part", .cap = CLI_CELLS_POOL}))    ? &mode->part
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--to", .cap = CLI_CELLS_POOL}))    ? &mode->to
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--seed", .cap = CLI_CELLS_POOL}))  ? &mode->seed
            : (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--trace", .cap = CLI_CELLS_POOL})) ? &mode->trace
                                                                                                      : NULL;
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--dry", .cap = CLI_CELLS_POOL}))
        {
            mode->dry = 1;
        }
        else if (slot != NULL)
        {
            if ((at + 1u) >= count)
            {
                cli_line_text(said, word);
                cli_line_text(said, " takes a value");
                return 0;
            }
            *slot = words[at + 1u];
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
        cli_line_text(said, "klq_identity writes forms in sass alone, and --to names ");
        cli_line_text(said, mode->to);
        return 0;
    }
    return 1;
}

void query_part_read(const char *part, char *name, size_t room)
{
    CliLine line = {name, room, 0ull};
    for (const char *at = part; *at != '\0'; at += 1)
    {
        if (*at != '_')
        {
            cli_line_character(&line, ((*at >= 'a') && (*at <= 'z')) ? (char)(*at - 'a' + 'A') : *at);
        }
    }
    cli_line_closed(&line);
}

int query_pairs_carry(QueryContext *context, const char *part, int dry, QueryWords *words, CliLine *said)
{
    static char s_carrier[QUERY_PATH_LONGEST];
    static char s_cross[QUERY_PATH_LONGEST];
    static char s_nvdisasm[QUERY_PATH_LONGEST];
    if (!query_context_tool(context, "cubin_run", s_carrier, said) ||
        !query_context_tool(context, "measuring_stick_query", s_cross, said) ||
        !query_nvdisasm(context, s_nvdisasm, said))
    {
        return 0;
    }
    char machine[QUERY_PATH_LONGEST];
    char layout[QUERY_PATH_LONGEST];
    char name[QUERY_PATH_LONGEST];
    CliLine line = {name, sizeof(name), 0ull};
    cli_line_text(&line, QUERY_TABLE "/");
    cli_line_text(&line, part);
    cli_line_text(&line, ".khw");
    cli_line_closed(&line);
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
    // the carrier, its dry flags first where `dry` as it reads them, its held list and its lessons, the machine file
    // and the layout; the pop, dry and full; and the cross with the vendor's reader
    query_words_add(words, carrier);
    if (dry)
    {
        query_words_add(words, "--dry");
        query_words_add(words, "--full");
    }
    static const char *const s_held[4] = {"--held", "held.txt", "--lessons", "overseer.txt"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        query_words_add(words, s_held[at]);
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
    query_words_add(words, "held.txt");
}

int query_pairs_configs(QueryContext *context, QueryRun *run, const char *part, CliLine *said)
{
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    static char s_paths[7][QUERY_PATH_LONGEST];
    static const char *s_read[7];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_LONGEST];
        CliLine line = {name, sizeof(name), 0ull};
        cli_line_text(&line, QUERY_TABLE "/");
        cli_line_text(&line, part);
        cli_line_character(&line, '.');
        cli_line_text(&line, s_kinds[at]);
        cli_line_closed(&line);
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
    return query_run_read(run, s_read, 7u, said);
}

int query_host_answered(QueryContext *context, const QueryRun *run, const char *out, CliLine *said)
{
    const unsigned char *questions = NULL;
    unsigned long long size = 0ull;
    if (!query_writer_latest(&run->writer, "host_questions.cpp", &questions, &size))
    {
        cli_line_text(said, "the run holds no host_questions.cpp");
        return 0;
    }
    char source[QUERY_PATH_LONGEST];
    char program[QUERY_PATH_LONGEST];
    char base[64];
    CliLine source_line = {source, sizeof(source), 0ull};
    cli_line_text(&source_line, out);
    cli_line_text(&source_line, "/host_questions.cpp");
    cli_line_closed(&source_line);
    query_binary_name("host_questions", base, sizeof(base));
    CliLine program_line = {program, sizeof(program), 0ull};
    cli_line_text(&program_line, out);
    cli_line_character(&program_line, '/');
    cli_line_text(&program_line, base);
    cli_line_closed(&program_line);
    if (!query_file_written(source, questions, size))
    {
        cli_line_text(said, source);
        cli_line_text(said, " could not be written");
        return 0;
    }
    static char s_cxx[QUERY_PATH_LONGEST];
    if (!context->programs->program(context->programs->context, QUERY_ROLE_CXX, s_cxx, sizeof(s_cxx), said))
    {
        return 0;
    }
    const char *const compile[8] = {s_cxx, "-std=c++17", "-O0", "-fwrapv", "-w", "-o", program, NULL};
    static const char *s_compile[9];
    memor.cpy(&(MemoriaCfg){.dst = s_compile, .src = compile, .bytes = sizeof(compile)});
    s_compile[7] = source;
    s_compile[8] = NULL;
    long long code = -1ll;
    if (!query_context_teed(context, run, "host_questions_compile", s_compile, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "the host questions did not compile");
        return 0;
    }
    const char *const answer[2] = {program, NULL};
    if (!query_context_handed(context, run, "host_answers.txt", answer, &code, said))
    {
        return 0;
    }
    if (code != 0ll)
    {
        cli_line_text(said, "the host questions did not run");
        return 0;
    }
    return 1;
}

// the vendor's writer as the identity reads it (identity_settings.h): its program, its part, and the part's layout and
// mnemonics, each in its room and ended by a NUL
static unsigned char s_writer_setting[1024u + 64u + 1024u + 1024u];
static unsigned char s_seed_setting[8];
static char s_ruleset_setting[64];

static int query_identity_writer(const char *const *fields, CliLine *said)
{
    static const unsigned int s_rooms[4] = {1024u, 64u, 1024u, 1024u};
    memor.set(&(MemoriaCfg){.dst = s_writer_setting, .val = 0, .bytes = sizeof(s_writer_setting)});
    unsigned int at = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = fields[field], .cap = CLI_CELLS_POOL});
        if (length >= s_rooms[field])
        {
            cli_line_text(said, fields[field]);
            cli_line_text(said, " does not fit the writer's room");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = &s_writer_setting[at], .src = fields[field], .bytes = length});
        at += s_rooms[field];
    }
    return 1;
}

// Every mode: the stick's listing and the part's files held to, the settings given, and the mode run
static int query_pairs_mode_run(QueryContext *context, const QueryPairsMode *mode, CliLine *said)
{
    char stick[QUERY_PATH_LONGEST];
    query_at(context, QUERY_PAIRS_STICK "/measuring_stick_nvcc.sass", stick);
    if (!query_is_file(stick))
    {
        cli_line_text(said, "no stick: run orior protocol stick");
        return -1;
    }
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_LONGEST];
        char path[QUERY_PATH_LONGEST];
        CliLine line = {name, sizeof(name), 0ull};
        cli_line_text(&line, QUERY_TABLE "/");
        cli_line_text(&line, mode->part);
        cli_line_character(&line, '.');
        cli_line_text(&line, s_kinds[at]);
        cli_line_closed(&line);
        query_at(context, name, path);
        if (!query_is_file(path))
        {
            cli_line_text(said, "no ");
            cli_line_text(said, mode->part);
            cli_line_character(said, '.');
            cli_line_text(said, s_kinds[at]);
            cli_line_text(said, " beside the bridge: --part names no part the tree holds");
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
            cli_line_text(said, "the seed ");
            cli_line_text(said, mode->seed);
            cli_line_text(said, " is no number");
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
    static char s_writer[QUERY_PATH_LONGEST];
    static QueryFiles s_files;
    if (!query_context_tool(context, "khw_machine_write", s_writer, said))
    {
        return -1;
    }
    query_files_of(context->root, mode->part, &s_files);
    const char *const fields[4] = {s_writer, mode->part, s_files.layout, s_files.mnemonics};
    if (!query_identity_writer(fields, said))
    {
        return -1;
    }
    context->setting[context->settings] = (QuerySetting){"identity.writer", s_writer_setting, sizeof(s_writer_setting)};
    context->settings += 1u;
    // the ruleset of the language the mode writes forms in, which the identity reads its forms from
    CliLine ruleset = {s_ruleset_setting, sizeof(s_ruleset_setting), 0ull};
    cli_line_text(&ruleset, mode->to);
    cli_line_text(&ruleset, ".krs");
    context->setting[context->settings] = (QuerySetting){"identity.ruleset", s_ruleset_setting, ruleset.at};
    context->settings += 1u;
    static char s_identity[QUERY_PATH_LONGEST];
    if (!query_context_tool(context, "klq_identity", s_identity, said))
    {
        return -1;
    }
    char work[QUERY_PATH_LONGEST];
    query_at(context, QUERY_PAIRS_WORK, work);
    if (!query_folder_made(work))
    {
        cli_line_text(said, work);
        cli_line_text(said, " could not be made");
        return -1;
    }
    char folder[QUERY_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = folder,
        .src = (mode->trace != NULL) ? mode->trace : QUERY_PAIRS_FOLDER,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (mode->trace != NULL) ? mode->trace : QUERY_PAIRS_FOLDER,
                                               .cap = CLI_CELLS_POOL}) +
                 1u});
    int code = -1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "text_identity", .cap = CLI_CELLS_POOL}))
    {
        code = query_text_identity(context, mode, s_identity, folder, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "stick", .cap = CLI_CELLS_POOL}))
    {
        code = query_stick_sliced(context, mode, s_identity, folder, said);
    }
    else
    {
        code = query_judged(context, mode, s_identity, folder, said);
    }
    context->settings = 0u;
    return code;
}

int query_pairs_pair(QueryContext *context, const char *part, const char *to, int dry, const char *seed,
                     const char *trace, CliLine *said)
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
        cli_line_text(said, "klq_identity writes forms in sass alone, and --to names ");
        cli_line_text(said, to);
        return -1;
    }
    return query_pairs_mode_run(context, &mode, said);
}

int query_pairs_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    static QueryPairsMode s_mode;
    if (!query_pairs_read(words, count, &s_mode, said))
    {
        return -1;
    }
    return query_pairs_mode_run(context, &s_mode, said);
}
