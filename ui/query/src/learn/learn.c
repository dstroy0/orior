// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// learn.c: the part passes (khw_learn.c, run_channel.h): a part pass is one live run whose every round is checked
// before the part by the airlock, the carrier with the part taken out popping each question's container and the cross
// reading them against the vendor's disassembler, and fixed where a count of registers fixes it. `discover` seeds the
// kernel's own form into the working set the first time and after that widens the forms the part has classified a
// round; `seed` is a discover pass seeded as well from a listing of the vendor's compiler; `turns` asks the turns of
// the forms a discover left; `registers` sets the runs those find to the two registers. `rounds` widens for a count of
// rounds; `dry` puts every question to the dry carrier and holds each to the gate with the part taken out;
// `enumerate` emits one slot's questions for the cross to read. This file reads a learn's words and gives what the
// passes share; learn_steps.c runs them
#include "learn_internal.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

int query_learn_step_read(const char *const *words, unsigned int count, QueryLearnStep *step, CliLine *said)
{
    static const char *const s_usage = "learn takes discover|turns|registers|costs [--base <cu>], seed <listing.sass>, "
                                       "rounds [<n>], dry [<n>], enumerate <slot> or clock [<from> [<words>]]";
    memor.set(&(MemoriaCfg){.dst = step, .val = 0, .bytes = sizeof(*step)});
    if (count == 0u)
    {
        cli_line_text(said, s_usage);
        return 0;
    }
    const char *const first = words[0];
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "discover", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "turns", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "registers", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "costs", .cap = CLI_CELLS_POOL})))
    {
        step->kind = QUERY_LEARN_PASS;
        step->word = first;
        if (count == 1u)
        {
            return 1;
        }
        if ((count == 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[1], .other = "--base", .cap = CLI_CELLS_POOL})))
        {
            step->base = words[2];
            return 1;
        }
        cli_line_text(said, s_usage);
        return 0;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "seed", .cap = CLI_CELLS_POOL})) && (count >= 2u))
    {
        step->kind = QUERY_LEARN_SEED;
        step->word = words[1];
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "rounds", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "dry", .cap = CLI_CELLS_POOL})))
    {
        step->kind = (first[0] == 'r') ? QUERY_LEARN_ROUNDS : QUERY_LEARN_DRY;
        step->rounds = 1u;
        if (count >= 2u)
        {
            char *end = NULL;
            const unsigned long rounds = strtoul(words[1], &end, 10);
            if ((end == words[1]) || (*end != '\0') || (rounds > 0xffffffffull))
            {
                cli_line_text(said, s_usage);
                return 0;
            }
            // a count of rounds read under what an unsigned int holds
            step->rounds = (unsigned int)rounds;
        }
        return 1;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "enumerate", .cap = CLI_CELLS_POOL})) && (count >= 2u))
    {
        step->kind = QUERY_LEARN_ENUMERATE;
        step->word = words[1];
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "clock", .cap = CLI_CELLS_POOL}))
    {
        step->kind = QUERY_LEARN_CLOCK;
        step->word = (count >= 2u) ? words[1] : QUERY_CLOCK_FROM;
        step->base = (count >= 3u) ? words[2] : QUERY_CLOCK_WORDS;
        return 1;
    }
    cli_line_text(said, s_usage);
    return 0;
}

unsigned int query_learn_lines(const QueryRun *run, const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!query_writer_latest(&run->writer, name, &bytes, &size))
    {
        return 0u;
    }
    unsigned int lines = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const end = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long past = (end != NULL) ? (unsigned long long)(end - bytes) : size;
        int blank = 1;
        for (unsigned long long letter = at; letter < past; letter += 1ull)
        {
            blank = blank && ((bytes[letter] == ' ') || (bytes[letter] == '\t') || (bytes[letter] == '\r'));
        }
        lines += blank ? 0u : 1u;
        at = past + 1ull;
    }
    return lines;
}

int query_learn_carried(QueryContext *context, QueryRun *run, int lessons, CliLine *said)
{
    char work[QUERY_PATH_LONGEST];
    char folder[QUERY_PATH_LONGEST];
    char forms_work[QUERY_PATH_LONGEST];
    char forms[QUERY_PATH_LONGEST];
    query_at(context, QUERY_LEARN_WORK, work);
    query_at(context, QUERY_LEARN_FOLDER, folder);
    CliLine forms_work_line = {forms_work, sizeof(forms_work), 0ull};
    cli_line_text(&forms_work_line, work);
    cli_line_text(&forms_work_line, "/forms_work.txt");
    cli_line_closed(&forms_work_line);
    CliLine forms_line = {forms, sizeof(forms), 0ull};
    cli_line_text(&forms_line, work);
    cli_line_text(&forms_line, "/forms.txt");
    cli_line_closed(&forms_line);
    const char *const files[4] = {forms_work, forms, "faulted.txt", "overseer.txt"};
    char crystal[QUERY_PATH_LONGEST];
    const int has_crystal = query_crystal(context->root, crystal, sizeof(crystal));
    return query_run_carried(run, folder, files, lessons ? 4u : 3u, has_crystal ? crystal : NULL, said);
}

void query_teacher_words(QueryWords *words, const char *machine, const QueryFiles *files, const char *writer)
{
    query_words_add(words, "sm_86");
    query_words_add(words, machine);
    query_words_add(words, files->layout);
    query_words_add(words, files->mnemonics);
    query_words_add(words, files->fields);
    query_words_add(words, writer);
}

void query_verdicts_said(QueryContext *context, const QueryRun *run)
{
    unsigned long long dry_size = 0ull;
    char *const dry = query_writer_read_taken(&run->writer, "dry.txt", &dry_size);
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
        unsigned long long taken = 0ull;
        const unsigned long long room = length - (unsigned long long)(found - line);
        while ((taken < room) && (taken < 127ull) &&
               (((found[taken] >= 'a') && (found[taken] <= 'z')) || (found[taken] == ' ') || (found[taken] == ',') ||
                (found[taken] == ':')))
        {
            taken += 1ull;
        }
        while ((taken != 0ull) && (found[taken - 1u] == ' '))
        {
            taken -= 1ull;
        }
        char verdict[128];
        memor.cpy(&(MemoriaCfg){.dst = verdict, .src = found, .bytes = (size_t)taken});
        verdict[taken] = '\0';
        unsigned int place = 0u;
        while ((place < verdicts) && (cli_text_order(s_verdicts[place], verdict) < 0))
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
    cli_cell_given_by(&g_query_cells, dry);
    for (unsigned int place = 0u; place < verdicts; place += 1u)
    {
        char text[160];
        CliLine line = {text, sizeof(text), 0ull};
        cli_line_decimal_columns(&line, s_counts[place], 7u);
        cli_line_character(&line, ' ');
        cli_line_text(&line, s_verdicts[place]);
        if (cli_line_closed(&line) != 0ull)
        {
            query_said(context, text);
        }
    }
}

int query_bar0_driver_runs(void)
{
    const char *const words[4] = {"sc.exe", "query", QUERY_LISTENER, NULL};
    QueryProcess asked;
    if (!query_process_start(&asked, words, NULL, QUERY_OUTPUT_PIPED))
    {
        return 0;
    }
    static char s_output[65536];
    unsigned long long length = 0ull;
    for (;;)
    {
        const unsigned long long got = query_process_read(&asked, s_output + length, sizeof(s_output) - 1u - length);
        if (got == 0ull)
        {
            break;
        }
        length += got;
    }
    query_process_wait(&asked);
    s_output[length] = '\0';
    return cellul.find(&(CatenaFinitaCfg){
               .src = s_output, .cap = CLI_CELLS_POOL, .other = "RUNNING", .other_cap = CLI_CELLS_POOL}) != NULL;
}
