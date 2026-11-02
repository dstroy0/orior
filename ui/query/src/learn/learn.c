// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn.c: the part passes (khw_learn.c, run_channel.h): a part pass is one live run whose every round is checked
// before the part by the airlock, the carrier with the part taken out popping each question's container and the cross
// reading them against the vendor's disassembler, and fixed where a count of registers fixes it. `discover` seeds the
// kernel's own form into the working set the first time and after that widens the forms the part has classified a
// round; `seed` is a discover pass seeded as well from a listing of the vendor's compiler; `turns` requests the turns
// of the forms a discover left; `registers` sets the runs those find to the two registers. `rounds` widens for a count
// of rounds; `dry` puts every question to the dry carrier and checks each at the gate with the part taken out;
// `enumerate` emits one place's questions for the cross to read. This file reads a learn's words and gives what the
// passes share; learn_steps.c runs them
#include "learn_internal.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

int query_learn_step_read(const QueryLearnWordsCtx *args)
{
    static const char *const s_usage = "learn takes discover|turns|registers|costs [--base <cu>], seed <listing.sass>, "
                                       "rounds [<n>], dry [<n>], enumerate <place> or clock [<from> [<words>]]";
    memor.set(&(MemoriaCfg){.dst = args->step, .val = 0, .bytes = sizeof(*args->step)});
    if (args->count == 0u)
    {
        mmgr_line_put(args->err_msg, s_usage);
        return 0;
    }
    const char *const first = args->words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "discover", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "turns", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "registers", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "costs", .cap = CLI_CELLS_POOL})))
    {
        args->step->kind = QUERY_LEARN_PASS;
        args->step->word = first;
        if (args->count == 1u)
        {
            return 1;
        }
        if ((args->count == 3u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = args->words[1], .other = "--base", .cap = CLI_CELLS_POOL})))
        {
            args->step->base = args->words[2];
            return 1;
        }
        mmgr_line_put(args->err_msg, s_usage);
        return 0;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "seed", .cap = CLI_CELLS_POOL})) && (args->count >= 2u))
    {
        args->step->kind = QUERY_LEARN_SEED;
        args->step->word = args->words[1];
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "rounds", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "dry", .cap = CLI_CELLS_POOL})))
    {
        args->step->kind = (first[0] == 'r') ? QUERY_LEARN_ROUNDS : QUERY_LEARN_DRY;
        args->step->rounds = 1u;
        if (args->count >= 2u)
        {
            char *end = NULL;
            const unsigned long rounds = strtoul(args->words[1], &end, 10);
            if ((end == args->words[1]) || (*end != '\0') || (rounds > 0xffffffffull))
            {
                mmgr_line_put(args->err_msg, s_usage);
                return 0;
            }
            // a count of rounds read under what an unsigned int represents
            args->step->rounds = (unsigned int)rounds;
        }
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "enumerate", .cap = CLI_CELLS_POOL})) &&
        (args->count >= 2u))
    {
        args->step->kind = QUERY_LEARN_ENUMERATE;
        args->step->word = args->words[1];
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "clock", .cap = CLI_CELLS_POOL}))
    {
        args->step->kind = QUERY_LEARN_CLOCK;
        args->step->word = (args->count >= 2u) ? args->words[1] : QUERY_CLOCK_FROM;
        args->step->base = (args->count >= 3u) ? args->words[2] : QUERY_CLOCK_WORDS;
        return 1;
    }
    mmgr_line_put(args->err_msg, s_usage);
    return 0;
}

unsigned int query_learn_lines(const QueryLearnCtx *args)
{
    const char *name = args->name;
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &args->run->writer,
                    .name = name, .bytes_u8s = &bytes, .size_pointer = &size))
    {
        return 0u;
    }
    unsigned int lines = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const newline = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        int blank = 1;
        for (unsigned long long letter = at; letter < end; letter += 1ull)
        {
            blank = blank && ((bytes[letter] == ' ') || (bytes[letter] == '\t') || (bytes[letter] == '\r'));
        }
        lines += blank ? 0u : 1u;
        at = end + 1ull;
    }
    return lines;
}

int query_learn_machine_transfer(const QueryLearnErrMsgCtx *args)
{
    QueryContext *context = args->context;
    QueryRun *run = args->run_out;
    VerbaLinea *err_msg = args->err_msg;
    char work[QUERY_PATH_MAX_LEN];
    char folder[QUERY_PATH_MAX_LEN];
    char forms_work[QUERY_PATH_MAX_LEN];
    char forms[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_LEARN_WORK, .path_out = work);
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context,
               .under = QUERY_LEARN_FOLDER, .path_out = folder);
    VerbaLinea forms_work_line = {forms_work, sizeof(forms_work), 0ull};
    mmgr_line_put(&forms_work_line, work);
    mmgr_line_put(&forms_work_line, "/forms_work.txt");
    mmgr_line_finish(&forms_work_line);
    VerbaLinea forms_line = {forms, sizeof(forms), 0ull};
    mmgr_line_put(&forms_line, work);
    mmgr_line_put(&forms_line, "/forms.txt");
    mmgr_line_finish(&forms_line);
    const char *const files[4] = {forms_work, forms, "faulted.txt", "overseer.txt"};
    char crystal[QUERY_PATH_MAX_LEN];
    const int has_crystal = EMBED_CALL(query_capacity_root_internal.crystal, QueryCapacityRootCtx,
                                       .root = context->root, .program = crystal, .capacity = sizeof(crystal));
    return EMBED_CALL(query_count_folder_internal.run_machine_transfer, QueryCountFolderCtx, .run = run,
                      .folder = folder, .files = files, .count = args->lessons ? 4u : 3u,
                      .crystal = has_crystal ? crystal : NULL, .err_msg = err_msg);
}

void query_teacher_words(const QueryLearnWordsWordsCtx *args)
{
    QueryWords *words = args->words_words_pointer;
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = "sm_86");
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = args->machine);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = args->files->layout);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = args->files->mnemonics);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = args->files->fields);
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = words,
               .word = args->writer);
}

void query_verdicts_print(const QueryLearnRunCtx *args)
{
    QueryContext *context = args->context;
    unsigned long long dry_size = 0ull;
    char *const dry = EMBED_CALL(query_count_name_internal.writer_read_alloc, QueryCountNameCtx,
                                 .writer = &args->run->writer, .name = "dry.txt", .size_pointer = &dry_size);
    if (dry == NULL)
    {
        return;
    }
    // each verdict the gate gave and its count, in the order of the verdicts' texts
    static char s_verdicts[256][128];
    static unsigned long long s_counts[256];
    unsigned int verdicts = 0u;
    unsigned long long at = 0ull;
    while (at < dry_size)
    {
        const char *const line = dry + at;
        const char *const end =
            (const char *)memor.chr(&(MemoriaCfg){.src = line, .val = '\n', .bytes = (size_t)(dry_size - at)});
        const unsigned long long length = (end != NULL) ? (unsigned long long)(end - line) : (dry_size - at);
        at += length + 1ull;
        const char *found = NULL;
        for (unsigned long long letter = 0ull; (found == NULL) && ((letter + 10ull) <= length); letter += 1ull)
        {
            found = (memor.cmp(&(MemoriaCfg){.src = line + letter, .other = "the gate: ", .bytes = 10u}) == 0)
                        ? (line + letter)
                        : NULL;
        }
        if (found == NULL)
        {
            continue;
        }
        // the verdict: its lower-case letters, spaces, commas and colons, its end trimmed
        unsigned long long reserved = 0ull;
        const unsigned long long capacity = length - (unsigned long long)(found - line);
        reserved = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = found, .at = 0ull,
                              .size = (capacity < 127ull) ? capacity : 127ull, .classes = CLI_TEXT_LOWER, .set = " ,:");
        reserved = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = found, .at = reserved,
                              .set = " ");
        char verdict[128];
        memor.cpy(&(MemoriaCfg){.dst = verdict, .src = found, .bytes = (size_t)reserved});
        verdict[reserved] = '\0';
        unsigned int place = 0u;
        while ((place < verdicts) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_verdicts[place],
                                                 .other_pointer = verdict) < 0))
        {
            place += 1u;
        }
        if ((place < verdicts) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_verdicts[place], .other = verdict, .cap = CLI_CELLS_POOL})))
        {
            s_counts[place] += 1ull;
            continue;
        }
        if (verdicts >= 256u)
        {
            continue;
        }
        memor.move_up(&(MemoriaCfg){.dst = s_verdicts[place + 1u],
                                    .src = s_verdicts[place],
                                    .bytes = (size_t)(verdicts - place) * sizeof(s_verdicts[0])});
        memor.move_up(&(MemoriaCfg){.dst = &s_counts[place + 1u],
                                    .src = &s_counts[place],
                                    .bytes = (size_t)(verdicts - place) * sizeof(s_counts[0])});
        memor.cpy(&(MemoriaCfg){.dst = s_verdicts[place], .src = verdict, .bytes = sizeof(verdict)});
        s_counts[place] = 1ull;
        verdicts += 1u;
    }
    g_cli_cells->persistent_buf_release(dry, &g_query_cells);
    for (unsigned int place = 0u; place < verdicts; place += 1u)
    {
        char text[160];
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_decimal_columns(&line, s_counts[place], 7u);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, s_verdicts[place]);
        if (mmgr_line_finish(&line) != 0ull)
        {
            EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = text);
        }
    }
}

int query_bar0_driver_runs(void)
{
    const char *const words[4] = {"sc.exe", "query", QUERY_LISTENER, NULL};
    QueryProcess child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_PIPED))
    {
        return 0;
    }
    static char s_output[65536];
    unsigned long long length = 0ull;
    for (;;)
    {
        const unsigned long long got =
            EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &child,
                       .piece = s_output + length, .capacity_u64 = sizeof(s_output) - 1u - length);
        if (got == 0ull)
        {
            break;
        }
        length += got;
    }
    EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child);
    s_output[length] = '\0';
    return cellul.find(&(CatenaFinitaCfg){
               .src = s_output, .cap = CLI_CELLS_POOL, .other = "RUNNING", .other_cap = CLI_CELLS_POOL}) != NULL;
}
