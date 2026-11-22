// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// loops.c: the loops: an operand renamed, a candidate and its answer, the loop written, and the loops queried
#include "probe_query_internal.h"
#include "../../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// `from` into `into`, every register named `stem` ("R" or "UR") and a number made the one numbered 8, which the loop
// does not retain. A stem following a character is part of another name and is copied as it is
static void sass_loop_rename(const char *from, const char *stem, char *into, size_t room)
{
    const size_t stem_length = mmgr_text_length(stem, PLATFORM_TEXT_TO_TERMINATOR);
    size_t at = 0u;
    size_t walk = 0u;
    while ((from[walk] != '\0') && ((at + stem_length + 2u) < room))
    {
        const int alone = (walk == 0u) || !isalpha((unsigned char)from[walk - 1u]);
        if (alone && (mmgr_text_order(&from[walk], stem, (size_t)(stem_length)) == 0) &&
            isdigit((unsigned char)from[walk + stem_length]))
        {
            mmgr_bytes_copy(&into[at], stem, stem_length);
            at += stem_length;
            into[at] = '8';
            at += 1u;
            walk += stem_length;
            while (isdigit((unsigned char)from[walk]))
            {
                walk += 1u;
            }
            continue;
        }
        into[at] = from[walk];
        at += 1u;
        walk += 1u;
    }
    into[at] = '\0';
}

// one operand of a candidate, from its form's own operand `base` of `type` and `mark`: a register becomes R8 and a
// uniform one UR8, an address retains its shape with its registers made R8, a predicate becomes the flag, a label the
// loop's and a number the distance back to it. RZ, URZ and PT write nowhere and read nothing and are retained, and so
// is any other operand, which the assembler takes by its text
static void sass_loop_operand(const char *base, unsigned int type, unsigned int mark, char *operand, size_t room)
{
    static const char *const s_marks[] = {"", "-", "~", "!"};
    char filled[SASS_OPERAND_TEXT];
    switch (type)
    {
    case SASS_OPERAND_REGISTER:
    case SASS_OPERAND_ADDRESS:
        sass_loop_rename(base, "R", filled, sizeof(filled));
        break;
    case SASS_OPERAND_UNIFORM:
        sass_loop_rename(base, "UR", filled, sizeof(filled));
        break;
    case SASS_OPERAND_PREDICATE: {
        VerbaLinea formed_line_11 = mmgr_line_over(filled, sizeof(filled));
        mmgr_line_put(&formed_line_11, (mmgr_text_equal(base, "PT", PLATFORM_TEXT_TO_TERMINATOR)) ? "PT" : "P0");
        (void)mmgr_line_finish(&formed_line_11);
        (void)formed_line_11.at;
    }
    break;
    case SASS_OPERAND_LABEL: {
        VerbaLinea formed_line_12 = mmgr_line_over(filled, sizeof(filled));
        mmgr_line_put(&formed_line_12, SASS_LOOP_LABEL);
        (void)mmgr_line_finish(&formed_line_12);
        (void)formed_line_12.at;
    }
    break;
    case SASS_OPERAND_IMMEDIATE: {
        VerbaLinea formed_line_13 = mmgr_line_over(filled, sizeof(filled));
        mmgr_line_put(&formed_line_13, SASS_LOOP_BACK);
        (void)mmgr_line_finish(&formed_line_13);
        (void)formed_line_13.at;
    }
    break;
    default: {
        VerbaLinea formed_line_14 = mmgr_line_over(filled, sizeof(filled));
        mmgr_line_put(&formed_line_14, base);
        (void)mmgr_line_finish(&formed_line_14);
        (void)formed_line_14.at;
    }
    break;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(operand, room);
        mmgr_line_put(&buffer_line, s_marks[mark]);
        mmgr_line_put(&buffer_line, filled);
        (void)mmgr_line_finish(&buffer_line);
    }
}

// `form` written as the candidate in loop_back_if's place, guarded by the flag, into `text`: 1, or 0 where one of its
// operands is a type the reader did not know, which no instruction of the form assembles from
static int sass_loop_candidate(const SassForm *form, char *text, size_t room)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    VerbaLinea formed_line_1 = mmgr_line_over(text, room);
    mmgr_line_put(&formed_line_1, "@P0 ");
    mmgr_line_put(&formed_line_1, form->operation);
    (void)mmgr_line_finish(&formed_line_1);
    size_t at = (size_t)(int)formed_line_1.at;
    for (unsigned int place = 0u; (place < form->operands) && (at < room); place += 1u)
    {
        if (form->type[place] == SASS_OPERAND_UNKNOWN)
        {
            return 0;
        }
        char operand[SASS_OPERAND_TEXT];
        sass_loop_operand(base.operand[place], form->type[place], form->mark[place], operand, sizeof(operand));
        VerbaLinea formed_line_2 = mmgr_line_over(text + at, room - at);
        mmgr_line_put(&formed_line_2, (place == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_2, operand);
        (void)mmgr_line_finish(&formed_line_2);
        at += (size_t)(int)formed_line_2.at;
    }
    return at < room;
}

// the candidate's cubin, queried.cubin, run over a case whose first word is `count`, and the first word it answered
// through `word`: 1, or 0 where it did not run
static int sass_loop_answer(SassProbe *probe, unsigned int count, unsigned int *word)
{
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/queried.cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    char answered[256];
    if (!sass_cubin_answer_first(probe, path, count, answered, sizeof(answered)))
    {
        return 0;
    }
    // the answer's first word, which the run prints after "answered "
    *word = (unsigned int)mmgr_text_unsigned_in(answered + 9, NULL, 16);
    return 1;
}

// `candidate` written into the loop and assembled into queried.cubin: 1, or 0 with the reason printed
static int sass_loop_written(SassProbe *probe, const SassMachine *machine, const char *was, const char *candidate)
{
    static char s_body[4096];
    static char s_query_text[65536];
    {
        VerbaLinea buffer_line = mmgr_line_over(s_body, sizeof(s_body));
        mmgr_line_put(&buffer_line, SASS_LOOP_BODY);
        mmgr_line_put(&buffer_line, candidate);
        (void)mmgr_line_finish(&buffer_line);
    }
    return sass_query_text(was, s_body, s_query_text, sizeof(s_query_text)) &&
           sass_cubin_from_text(machine, probe->folder, "form_0", s_query_text, "queried");
}

unsigned int sass_cubin_loops(SassProbe *probe, const SassMachine *machine, unsigned int *queried)
{
    static char s_was[65536];
    static char s_retained[SASS_LOOPS_MAX][SASS_TEXT];
    if (sass_cubin_text(probe->folder, "form_0", s_was, sizeof(s_was)) == 0u)
    {
        return 0u;
    }
    unsigned int retained = 0u;
    unsigned int written = 0u;
    unsigned int through = 0u;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        char candidate[SASS_TEXT];
        *queried += 1u;
        if (!sass_loop_candidate(&machine->form[number], candidate, sizeof(candidate)) ||
            !sass_loop_written(probe, machine, s_was, candidate))
        {
            // the candidate could not be written as the part's own code, a refusal before the part sees it
            sass_class_count(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL);
            continue;
        }
        written += 1u;
        unsigned int word = 0u;
        if (!sass_loop_answer(probe, s_loop_counts[0], &word))
        {
            sass_class_count(SASS_CHANNEL_RUN, SASS_CLASS_ILLEGAL);
            continue;
        }
        through += (word == 1u) ? 1u : 0u;
        unsigned int missed = (word == s_loop_counts[0]) ? 0u : s_loop_counts[0];
        for (unsigned int index = 1u; (missed == 0u) && (index < (sizeof(s_loop_counts) / sizeof(s_loop_counts[0])));
             index += 1u)
        {
            const int ran = sass_loop_answer(probe, s_loop_counts[index], &word);
            missed = (ran && (word == s_loop_counts[index])) ? 0u : s_loop_counts[index];
        }
        if (missed != 0u)
        {
            if (missed != s_loop_counts[0])
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  loop ");
                mmgr_line_put_columns(output_line, candidate, 52u);
                mmgr_line_put(output_line, " comes back at ");
                mmgr_line_decimal(output_line, s_loop_counts[0], 1u);
                mmgr_line_put(output_line, " and not at ");
                mmgr_line_decimal(output_line, missed, 1u);
                mmgr_line_put(output_line, ", answering ");
                mmgr_line_hex(output_line, word, 8u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            sass_class_count(SASS_CHANNEL_RUN, SASS_CLASS_NOTHING);
            continue;
        }
        // it came back to the label on the flag at every count, and fell through once the flag was clear
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  loop ");
            mmgr_line_put_columns(output_line, candidate, 52u);
            mmgr_line_put(output_line, " comes back on every count");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ANSWERS, candidate, 0u);
        if (retained < SASS_LOOPS_MAX)
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(s_retained[retained], SASS_TEXT);
                mmgr_line_put(&buffer_line, candidate);
                (void)mmgr_line_finish(&buffer_line);
            }
            retained += 1u;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass loop: ");
        mmgr_line_decimal(output_line, *queried, 1u);
        mmgr_line_put(output_line, " forms queried, ");
        mmgr_line_decimal(output_line, written, 1u);
        mmgr_line_put(output_line, " written, ");
        mmgr_line_decimal(output_line, through, 1u);
        mmgr_line_put(output_line, " fell through, ");
        mmgr_line_decimal(output_line, retained, 1u);
        mmgr_line_put(output_line, " came back on every count");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // The forms that came back, each timed over a long count and read at its least, since a run can only be lengthened
    // by what else the host is doing; and each walked as the instruction that takes a loop back, which the part's
    // answer checks. The cheapest is the part's loop_back_if
    double cheapest = 0.0;
    double second = 0.0;
    double spread = 0.0;
    unsigned int chosen = SASS_LOOPS_MAX;
    for (unsigned int index = 0u; index < retained; index += 1u)
    {
        double least = 0.0;
        double max = 0.0;
        for (unsigned int take = 0u; take < SASS_PREFER_TAKES; take += 1u)
        {
            char repeats[32];
            {
                VerbaLinea buffer_line = mmgr_line_over(repeats, sizeof(repeats));
                mmgr_line_decimal(&buffer_line, SASS_PREFER_RUNS, 1u);
                (void)mmgr_line_finish(&buffer_line);
            }
            sass_environment("PROBE_REPEATS", repeats);
            unsigned int word = 0u;
            const int ran = sass_loop_written(probe, machine, s_was, s_retained[index]) &&
                            sass_loop_answer(probe, SASS_LOOP_TIMED, &word) && (word == SASS_LOOP_TIMED);
            const double taken = ran ? sass_cubin_nanoseconds() : 0.0;
            sass_environment("PROBE_REPEATS", "0");
            least = ((taken != 0.0) && ((least == 0.0) || (taken < least))) ? taken : least;
            max = (taken > max) ? taken : max;
        }
        spread = ((max - least) > spread) ? (max - least) : spread;
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        const SassLoopWalk walk = {SASS_LOOP_AT, SASS_LOOP_TARGET, SASS_LOOP_FLAG, s_loop_live,
                                   (unsigned int)(sizeof(s_loop_live) / sizeof(s_loop_live[0]))};
        unsigned int step = 0u;
        const int walked =
            sass_assemble(machine, s_retained[index], SASS_LOOP_AT, SASS_LOOP_TARGET, SASS_CONTROL_BASE, &low, &high) &&
            sass_loop_walk(machine, &walk, low, high, &step);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  loop back ");
            mmgr_line_put_columns(output_line, s_retained[index], 52u);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(least / (double)SASS_LOOP_TIMED), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns a turn over a spread of ");
            mmgr_line_fixed(output_line, (double)((max - least) / (double)SASS_LOOP_TIMED), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", the walk ");
            mmgr_line_put(output_line, walked ? "agrees" : "differs");
            mmgr_line_put(output_line, " at step ");
            mmgr_line_decimal(output_line, step, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (least == 0.0)
        {
            continue;
        }
        if ((chosen == SASS_LOOPS_MAX) || (least < cheapest))
        {
            second = cheapest;
            cheapest = least;
            chosen = index;
            continue;
        }
        second = ((second == 0.0) || (least < second)) ? least : second;
    }
    // The cheapest is the part's loop_back_if where it is apart from the next by more than any form's own runs
    // was apart from each other. A difference under that spread is no reading, and the forms are then one cost
    if ((chosen != SASS_LOOPS_MAX) && ((second == 0.0) || ((second - cheapest) > spread)))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass loop back: ");
        mmgr_line_put(output_line, s_retained[chosen]);
        mmgr_line_put(output_line, ", ");
        mmgr_line_fixed(output_line, (double)(cheapest / (double)SASS_LOOP_TIMED), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " ns a turn");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if ((chosen != SASS_LOOPS_MAX) && (second != 0.0) && ((second - cheapest) <= spread))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass loop back: no form cheaper above the floor, the next ");
        mmgr_line_fixed(output_line, (double)((second - cheapest) / (double)SASS_LOOP_TIMED), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " ns a turn above ");
        mmgr_line_put(output_line, s_retained[chosen]);
        mmgr_line_put(output_line, " against a spread of ");
        mmgr_line_fixed(output_line, (double)(spread / (double)SASS_LOOP_TIMED), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return retained;
}
