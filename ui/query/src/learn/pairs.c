// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs.c: the bridge's pairs put to the part, and the other modes of `orior_klq_identity` (klq_identity.cu): `stall`,
// `register`, `end`, `queue`, `cost` and `curve` over the stick's questions and the host's answers to them,
// `text_identity` over every identity between texts in `Lstar.klq`, each side a kernel of a stick of its own, and
// `stick`, the no-argument run, which slices the stick's questions into identities, computes the integer questions on
// the host over every case, and sifts the engine's writing through what nvcc writes. Every mode that puts questions
// runs them through the airlock, and the decoder reads a pair run's log back into the bridge. This file reads the words
// and gives the settings; pairs_modes.c runs the modes
#include "../../../cli/src/text/text.h"
#include "pairs_internal.h"

#include <stdlib.h>

static const char *const s_modes[11] = {"frame", "past",  "pair",  "cost",          "stall", "register",
                                        "queue", "curve", "fixed", "text_identity", "stick"};

// the words of a pairs read into `mode`: 1, or 0 with why given
int query_pairs_read(const QueryPairsErrMsgCtx *args)
{
    QueryPairsMode *mode = args->mode_out;
    static const char *const s_usage = "pairs takes frame|fixed|pair|cost|stall|register|past|queue|curve|"
                                       "text_identity|stick [<words>] [--part <part>] [--to <language>] [--dry] "
                                       "[--seed <n>] [--trace <folder>]";
    memor.set(&(MemoriaCfg){.dst = mode, .val = 0, .bytes = sizeof(*mode)});
    mode->part = "sm_86";
    mode->to = "sass";
    int known = 0;
    for (unsigned int at = 0u; (args->count != 0u) && (at < 11u); at += 1u)
    {
        known =
            known ||
            (cellul.eq(&(CatenaFinitaCfg){.src = args->words_letters[0], .other = s_modes[at], .cap = CLI_POOL_BYTES}));
    }
    if (!known)
    {
        mmgr_line_put(args->err_msg, s_usage);
        return 0;
    }
    mode->mode = args->words_letters[0];
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const char *const word_text = args->words_letters[at];
        const char **field;
        if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--part", .cap = CLI_POOL_BYTES}))
        {
            field = &mode->part;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--to", .cap = CLI_POOL_BYTES}))
        {
            field = &mode->to;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--seed", .cap = CLI_POOL_BYTES}))
        {
            field = &mode->seed;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--trace", .cap = CLI_POOL_BYTES}))
        {
            field = &mode->trace;
        }
        else
        {
            field = NULL;
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = word_text, .other = "--dry", .cap = CLI_POOL_BYTES}))
        {
            mode->dry = 1;
        }
        else if (field != NULL)
        {
            if ((at + 1u) >= args->count)
            {
                mmgr_line_put(args->err_msg, word_text);
                mmgr_line_put(args->err_msg, " takes a value");
                return 0;
            }
            *field = args->words_letters[at + 1u];
            at += 1u;
        }
        else if (mode->word_count < QUERY_WORDS_MAX)
        {
            mode->words[mode->word_count] = word_text;
            mode->word_count += 1u;
        }
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = mode->to, .other = "sass", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(args->err_msg, "orior_klq_identity writes forms in sass alone, and --to names ");
        mmgr_line_put(args->err_msg, mode->to);
        return 0;
    }
    return 1;
}

void query_part_read(const QueryPairsPartNameCtx *args)
{
    VerbaLinea line = {args->name, args->capacity, 0ULL};
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = 0ULL; at < whole;)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->part, .at = at, .size = whole, .set = "_");
        const size_t from = line.at;
        const size_t reserved = mmgr_line_put_n(&line, args->part + at, (size_t)(stop - at));
        EMBED_CALL(cli_text_size_from.raise, CliTextSizeFromCfg, .text_out = line.out + from, .size = reserved);
        at = stop + 1ULL;
    }
    mmgr_line_finish(&line);
}

int query_pairs_machine_transfer(const QueryPairsPartContextDryCtx *args)
{
    const QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    static char s_transport[QUERY_PATH_MAX_LEN];
    static char s_cross[QUERY_PATH_MAX_LEN];
    static char s_nvdisasm[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_cubin_run", .binary = s_transport, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_measuring_stick_query", .binary = s_cross, .err_msg = err_msg) ||
        !EMBED_CALL(query_loops_err_msg.nvdisasm, QueryLoopsErrMsgCfg, .context_read = context, .path = s_nvdisasm,
                    .err_msg = err_msg))
    {
        return 0;
    }
    char machine[QUERY_PATH_MAX_LEN];
    char layout[QUERY_PATH_MAX_LEN];
    char name[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&line, QUERY_TABLE "/");
    mmgr_line_put(&line, args->part);
    mmgr_line_put(&line, ".khw");
    mmgr_line_finish(&line);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = name,
               .path_out = machine);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_NVIDIA "/elf64_nvidia.tsv", .path_out = layout);
    EMBED_CALL(query_pairs_part_dry_internal.machine_transfer_words, QueryPairsPartDryCtx, .words = args->words,
               .transport = s_transport, .cross = s_cross, .nvdisasm = s_nvdisasm, .machine = machine, .layout = layout,
               .part = args->part, .dry = args->dry);
    return 1;
}

void query_pairs_machine_transfer_words(const QueryPairsPartDryCtx *args)
{
    QueryWords *words = args->words;
    char reader[64];
    EMBED_CALL(query_pairs_part_name_internal.part_read, QueryPairsPartNameCtx, .part = args->part, .name = reader,
               .capacity = sizeof(reader));
    // the carrier, its dry flags first where `dry` as it reads them, its gated list and its lessons, the machine file
    // and the layout; the pop, dry and full; and the cross with the vendor's reader
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->transport);
    if (args->dry)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = "--dry");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = "--full");
    }
    static const char *const s_feedback_words[4] = {"--gated", "gated.txt", "--lessons", "overseer.txt"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
                   .word_text = s_feedback_words[at]);
    }
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
               .text = args->machine);
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
               .text = args->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--pop");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->transport);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--dry");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--full");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--lessons");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "overseer.txt");
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
               .text = args->machine);
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
               .text = args->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "--cross");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->cross);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = args->nvdisasm);
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = words,
               .text = reader);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "airlock.md");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word_text = "gated.txt");
}

int query_pairs_configs(const QueryPairsPartContextCtx *args)
{
    const QueryContext *context = args->context;
    QueryRun *run = args->run_out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    static char s_paths[7][QUERY_PATH_MAX_LEN];
    static const char *s_read[7];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ULL};
        mmgr_line_put(&line, QUERY_TABLE "/");
        mmgr_line_put(&line, args->part);
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, s_kinds[at]);
        mmgr_line_finish(&line);
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = name,
                   .path_out = s_paths[at]);
    }
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/Lstar.klq", .path_out = s_paths[3]);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/cu.krs", .path_out = s_paths[4]);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_TABLE "/sass.krs", .path_out = s_paths[5]);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_STICK "/measuring_stick.tsv", .path_out = s_paths[6]);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        s_read[at] = s_paths[at];
    }
    return EMBED_CALL(query_count_err_msg_internal.run_read, QueryCountErrMsgCtx, .run = run, .paths_letters = s_read,
                      .count = 7u, .err_msg = err_msg);
}

int query_host_answer(const QueryPairsContextCtx *args)
{
    QueryContext *context = args->context;
    const QueryRun *run = args->run;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned char *questions = NULL;
    unsigned long long size = 0ULL;
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &run->writer,
                    .name = "host_questions.cpp", .bytes_u8s = &questions, .size_pointer = &size))
    {
        mmgr_line_put(err_msg, "the run contains no host_questions.cpp");
        return 0;
    }
    char source[QUERY_PATH_MAX_LEN];
    char program[QUERY_PATH_MAX_LEN];
    char base[64];
    VerbaLinea source_line = {source, sizeof(source), 0ULL};
    mmgr_line_put(&source_line, args->out);
    mmgr_line_put(&source_line, "/host_questions.cpp");
    mmgr_line_finish(&source_line);
    EMBED_CALL(query_name_capacity_internal.binary_name, QueryNameCapacityCtx, .name = "host_questions", .binary = base,
               .capacity = sizeof(base));
    VerbaLinea program_line = {program, sizeof(program), 0ULL};
    mmgr_line_put(&program_line, args->out);
    mmgr_line_ch(&program_line, '/');
    mmgr_line_put(&program_line, base);
    mmgr_line_finish(&program_line);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = source,
                    .bytes = questions, .size = size))
    {
        mmgr_line_put(err_msg, source);
        mmgr_line_put(err_msg, " could not be written");
        return 0;
    }
    static char s_cxx[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(context->programs->program, QueryEachRolePathCapacityErrMsgCfg,
                    .context = context->programs->context, .role = QUERY_ROLE_CXX, .path = s_cxx,
                    .capacity = sizeof(s_cxx), .err_msg = err_msg))
    {
        return 0;
    }
    const char *const compile[8] = {s_cxx, "-std=c++17", "-O0", "-fwrapv", "-w", "-o", program, NULL};
    static const char *s_compile[9];
    memor.cpy(&(MemoriaCfg){.dst = s_compile, .src = compile, .bytes = sizeof(compile)});
    s_compile[7] = source;
    s_compile[8] = NULL;
    long long code = -1LL;
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_teed, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "host_questions_compile", .words = s_compile, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0LL)
    {
        mmgr_line_put(err_msg, "the host questions did not compile");
        return 0;
    }
    const char *const answer[2] = {program, NULL};
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_dispatch, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = run, .name = "host_answers.txt", .words = answer, .code = &code,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (code != 0LL)
    {
        mmgr_line_put(err_msg, "the host questions did not run");
        return 0;
    }
    return 1;
}

// the vendor's writer as the identity reads it (identity_settings.h): its program, its part, and the part's layout and
// mnemonics, each in its capacity and ended by a NUL
static unsigned char s_writer_setting[1024u + 64u + 1024u + 1024u];
static unsigned char s_seed_setting[8];
static char s_ruleset_setting[64];

typedef struct
{
    const char *const *fields;
    VerbaLinea *err_msg;
    QueryContext *context;
    const QueryPairsMode *mode;
} PairsCtx;
EMBED_INLINE int query_identity_writer(const PairsCtx *args)
{
    static const unsigned int s_capacities[4] = {1024u, 64u, 1024u, 1024u};
    memor.set(&(MemoriaCfg){.dst = s_writer_setting, .val = 0, .bytes = sizeof(s_writer_setting)});
    unsigned int at = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->fields[field], .cap = CLI_POOL_BYTES});
        if (length >= s_capacities[field])
        {
            mmgr_line_put(args->err_msg, args->fields[field]);
            mmgr_line_put(args->err_msg, " does not fit the writer's capacity");
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = &s_writer_setting[at], .src = args->fields[field], .bytes = length});
        at += s_capacities[field];
    }
    return 1;
}

// Every mode: the stick's listing and the part's files read, the settings given, and the mode run
static int query_pairs_mode_run(const PairsCtx *args)
{
    QueryContext *context = args->context;
    const QueryPairsMode *mode = args->mode;
    VerbaLinea *err_msg = args->err_msg;
    char stick[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_STICK "/measuring_stick_nvcc.sass", .path_out = stick);
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = stick))
    {
        mmgr_line_put(err_msg, "no stick: run orior_cli protocol stick");
        return -1;
    }
    static const char *const s_kinds[3] = {"khw", "ksc", "kqr"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        char name[QUERY_PATH_MAX_LEN];
        char path[QUERY_PATH_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ULL};
        mmgr_line_put(&line, QUERY_TABLE "/");
        mmgr_line_put(&line, mode->part);
        mmgr_line_ch(&line, '.');
        mmgr_line_put(&line, s_kinds[at]);
        mmgr_line_finish(&line);
        EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = name,
                   .path_out = path);
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
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
            s_seed_setting[at] = (unsigned char)((seed >> (8u * at)) & 0xffULL);
        }
        context->setting[context->settings] = (QuerySetting){"identity.seed", s_seed_setting, 8ULL};
        context->settings += 1u;
    }
    // the vendor's writer, which assembles every text of the vendor's language the identity puts, and its words but
    // the machine file the mode names
    static char s_writer[QUERY_PATH_MAX_LEN];
    static QueryFiles s_files;
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_khw_machine_write", .binary = s_writer, .err_msg = err_msg))
    {
        return -1;
    }
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = context->root, .part = mode->part,
               .files = &s_files);
    const char *const fields[4] = {s_writer, mode->part, s_files.layout, s_files.mnemonics};
    if (!EMBED_CALL(query_identity_writer, PairsCtx, .fields = fields, .err_msg = err_msg))
    {
        return -1;
    }
    context->setting[context->settings] = (QuerySetting){"identity.writer", s_writer_setting, sizeof(s_writer_setting)};
    context->settings += 1u;
    // the ruleset of the language the mode writes forms in, which the identity reads its forms from
    VerbaLinea ruleset = {s_ruleset_setting, sizeof(s_ruleset_setting), 0ULL};
    mmgr_line_put(&ruleset, mode->to);
    mmgr_line_put(&ruleset, ".krs");
    context->setting[context->settings] = (QuerySetting){"identity.ruleset", s_ruleset_setting, ruleset.at};
    context->settings += 1u;
    static char s_identity[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_klq_identity", .binary = s_identity, .err_msg = err_msg))
    {
        return -1;
    }
    char work[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_PAIRS_WORK, .path_out = work);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = work))
    {
        mmgr_line_put(err_msg, work);
        mmgr_line_put(err_msg, " could not be made");
        return -1;
    }
    char folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = folder, .text = mode->trace,
               .otherwise = QUERY_PAIRS_FOLDER);
    int code = -1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "text_identity", .cap = CLI_POOL_BYTES}))
    {
        code =
            EMBED_CALL(query_pairs_context_err_msg_internal.text_identity, QueryPairsContextErrMsgCtx,
                       .context = context, .mode = mode, .identity = s_identity, .folder = folder, .err_msg = err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = mode->mode, .other = "stick", .cap = CLI_POOL_BYTES}))
    {
        code =
            EMBED_CALL(query_pairs_context_err_msg_internal.stick_slice, QueryPairsContextErrMsgCtx, .context = context,
                       .mode = mode, .identity = s_identity, .folder = folder, .err_msg = err_msg);
    }
    else
    {
        code = EMBED_CALL(query_pairs_context_err_msg_internal.judged, QueryPairsContextErrMsgCtx, .context = context,
                          .mode = mode, .identity = s_identity, .folder = folder, .err_msg = err_msg);
    }
    context->settings = 0u;
    return code;
}

int query_pairs_pair(const QueryLoopsContextErrMsgPartCfg *args)
{
    const char *part = args->part;
    const char *to = args->to;
    int dry = args->dry;
    const char *seed = args->seed;
    const char *trace = args->trace;
    QueryPairsMode mode;
    memor.set(&(MemoriaCfg){.dst = &mode, .val = 0, .bytes = sizeof(mode)});
    mode.mode = "pair";
    mode.part = part;
    mode.to = to;
    mode.dry = dry;
    mode.seed = seed;
    mode.trace = trace;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = to, .other = "sass", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(args->err_msg, "orior_klq_identity writes forms in sass alone, and --to names ");
        mmgr_line_put(args->err_msg, to);
        return -1;
    }
    return EMBED_CALL(query_pairs_mode_run, PairsCtx, .context = args->context, .mode = &mode,
                      .err_msg = args->err_msg);
}

int query_pairs_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    static QueryPairsMode s_mode;
    if (!EMBED_CALL(query_pairs_err_msg_internal.read, QueryPairsErrMsgCtx, .words_letters = words, .count = count,
                    .mode_out = &s_mode, .err_msg = err_msg))
    {
        return -1;
    }
    return EMBED_CALL(query_pairs_mode_run, PairsCtx, .context = context, .mode = &s_mode, .err_msg = err_msg);
}
