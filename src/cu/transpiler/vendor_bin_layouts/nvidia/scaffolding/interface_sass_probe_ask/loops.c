// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// loops.c: the loops: an operand renamed, a candidate and its answer, the loop written, and the loops asked
#include "probe_ask_internal.h"

// `from` into `into`, every register named `stem` ("R" or "UR") and a number made the one numbered 8, which the loop
// does not keep. A stem following a letter is part of another name and is copied as it stands
static void sass_loop_rename(const char *from, const char *stem, char *into, size_t room)
{
    const size_t stem_length = strlen(stem);
    size_t at = 0u;
    size_t walk = 0u;
    while ((from[walk] != '\0') && ((at + stem_length + 2u) < room))
    {
        const int alone = (walk == 0u) || !isalpha((unsigned char)from[walk - 1u]);
        if (alone && (strncmp(&from[walk], stem, stem_length) == 0) && isdigit((unsigned char)from[walk + stem_length]))
        {
            memcpy(&into[at], stem, stem_length);
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

// one operand of a candidate, from its form's own operand `base` of `kind` and `mark`: a register becomes R8 and a
// uniform one UR8, an address keeps its shape with its registers made R8, a predicate becomes the flag, a label the
// loop's and a number the distance back to it. RZ, URZ and PT write nowhere and read nothing and are kept, and so is
// any other operand, which the assembler takes by its text
static void sass_loop_operand(const char *base, unsigned int kind, unsigned int mark, char *operand, size_t room)
{
    static const char *const s_marks[] = {"", "-", "~", "!"};
    char filled[SASS_OPERAND_TEXT];
    switch (kind)
    {
    case SASS_OPERAND_REGISTER:
    case SASS_OPERAND_ADDRESS:
        sass_loop_rename(base, "R", filled, sizeof(filled));
        break;
    case SASS_OPERAND_UNIFORM:
        sass_loop_rename(base, "UR", filled, sizeof(filled));
        break;
    case SASS_OPERAND_PREDICATE:
        snprintf(filled, sizeof(filled), "%s", (strcmp(base, "PT") == 0) ? "PT" : "P0");
        break;
    case SASS_OPERAND_LABEL:
        snprintf(filled, sizeof(filled), "%s", SASS_LOOP_LABEL);
        break;
    case SASS_OPERAND_IMMEDIATE:
        snprintf(filled, sizeof(filled), "%s", SASS_LOOP_BACK);
        break;
    default:
        snprintf(filled, sizeof(filled), "%s", base);
        break;
    }
    snprintf(operand, room, "%s%s", s_marks[mark], filled);
}

// `form` written as the candidate in loop_back_if's place, guarded by the flag, into `text`: 1, or 0 where one of its
// operands is a kind the reader did not know, which no instruction of the form assembles from
static int sass_loop_candidate(const SassForm *form, char *text, size_t room)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    size_t at = (size_t)snprintf(text, room, "@P0 %s", form->operation);
    for (unsigned int place = 0u; (place < form->operands) && (at < room); place += 1u)
    {
        if (form->kind[place] == SASS_OPERAND_UNKNOWN)
        {
            return 0;
        }
        char operand[SASS_OPERAND_TEXT];
        sass_loop_operand(base.operand[place], form->kind[place], form->mark[place], operand, sizeof(operand));
        at += (size_t)snprintf(text + at, room - at, "%s%s", (place == 0u) ? " " : ", ", operand);
    }
    return at < room;
}

// the candidate's cubin, asked.cubin, run over a case whose first word is `count`, and the first word it answered
// through `word`: 1, or 0 where it did not run
static int sass_loop_answer(SassProbe *probe, unsigned int count, unsigned int *word)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/asked.cubin", probe->folder);
    char answered[256];
    if (!sass_cubin_answer_first(probe, path, count, answered, sizeof(answered)))
    {
        return 0;
    }
    // the answer's first word, which the run prints after "answered "
    *word = (unsigned int)strtoul(answered + 9, NULL, 16);
    return 1;
}

// `candidate` written into the loop and assembled into asked.cubin: 1, or 0 with the reason printed
static int sass_loop_written(SassProbe *probe, const SassMachine *machine, const char *was, const char *candidate)
{
    static char s_body[4096];
    static char s_asking[65536];
    snprintf(s_body, sizeof(s_body), "%s%s", SASS_LOOP_BODY, candidate);
    return sass_ask_text(was, s_body, s_asking, sizeof(s_asking)) &&
           sass_cubin_from_text(machine, probe->folder, "form_0", s_asking, "asked");
}

unsigned int sass_cubin_loops(SassProbe *probe, const SassMachine *machine, unsigned int *asked)
{
    static char s_was[65536];
    static char s_kept[SASS_LOOP_KEPT][SASS_TEXT];
    if (sass_cubin_text(probe->folder, "form_0", s_was, sizeof(s_was)) == 0u)
    {
        return 0u;
    }
    unsigned int kept = 0u;
    unsigned int written = 0u;
    unsigned int through = 0u;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        char candidate[SASS_TEXT];
        *asked += 1u;
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
                printf("  loop %-52s comes back at %u and not at %u, answering %08x\n", candidate, s_loop_counts[0],
                       missed, word);
            }
            sass_class_count(SASS_CHANNEL_RUN, SASS_CLASS_NOTHING);
            continue;
        }
        // it came back to the label on the flag at every count, and fell through once the flag was clear
        printf("  loop %-52s comes back on every count\n", candidate);
        sass_class_take(SASS_CHANNEL_RUN, SASS_CLASS_ANSWERS, candidate, 0u);
        if (kept < SASS_LOOP_KEPT)
        {
            snprintf(s_kept[kept], SASS_TEXT, "%s", candidate);
            kept += 1u;
        }
    }
    printf("interface sass loop: %u forms asked, %u written, %u fell through, %u came back on every count\n", *asked,
           written, through, kept);
    // The forms that came back, each timed over a long count and read at its least, since a run can only be lengthened
    // by what else the host is doing; and each walked as the instruction that takes a loop back, which the part's
    // answer checks. The cheapest is the part's loop_back_if
    double cheapest = 0.0;
    double second = 0.0;
    double spread = 0.0;
    unsigned int chosen = SASS_LOOP_KEPT;
    for (unsigned int index = 0u; index < kept; index += 1u)
    {
        double least = 0.0;
        double most = 0.0;
        for (unsigned int take = 0u; take < SASS_PREFER_TAKES; take += 1u)
        {
            char repeats[32];
            snprintf(repeats, sizeof(repeats), "%u", SASS_PREFER_RUNS);
            sass_environment("PROBE_REPEATS", repeats);
            unsigned int word = 0u;
            const int ran = sass_loop_written(probe, machine, s_was, s_kept[index]) &&
                            sass_loop_answer(probe, SASS_LOOP_TIMED, &word) && (word == SASS_LOOP_TIMED);
            const double taken = ran ? sass_cubin_nanoseconds() : 0.0;
            sass_environment("PROBE_REPEATS", "0");
            least = ((taken != 0.0) && ((least == 0.0) || (taken < least))) ? taken : least;
            most = (taken > most) ? taken : most;
        }
        spread = ((most - least) > spread) ? (most - least) : spread;
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        const SassLoopWalk walk = {SASS_LOOP_AT, SASS_LOOP_TARGET, SASS_LOOP_FLAG, s_loop_live,
                                   (unsigned int)(sizeof(s_loop_live) / sizeof(s_loop_live[0]))};
        unsigned int step = 0u;
        const int walked = sass_assemble(machine, s_kept[index], SASS_LOOP_AT, SASS_LOOP_TARGET, SASS_CONTROL_BASE,
                                         &low, &high) &&
                           sass_loop_walk(machine, &walk, low, high, &step);
        printf("  loop back %-52s %.4f ns a turn over a spread of %.4f, the walk %s at step %u\n", s_kept[index],
               least / (double)SASS_LOOP_TIMED, (most - least) / (double)SASS_LOOP_TIMED,
               walked ? "agrees" : "differs", step);
        if (least == 0.0)
        {
            continue;
        }
        if ((chosen == SASS_LOOP_KEPT) || (least < cheapest))
        {
            second = cheapest;
            cheapest = least;
            chosen = index;
            continue;
        }
        second = ((second == 0.0) || (least < second)) ? least : second;
    }
    // The cheapest is the part's loop_back_if where it stands apart from the next by more than any form's own runs
    // stood apart from each other. A difference under that spread is no reading, and the forms are then one cost
    if ((chosen != SASS_LOOP_KEPT) && ((second == 0.0) || ((second - cheapest) > spread)))
    {
        printf("interface sass loop back: %s, %.4f ns a turn\n", s_kept[chosen], cheapest / (double)SASS_LOOP_TIMED);
    }
    if ((chosen != SASS_LOOP_KEPT) && (second != 0.0) && ((second - cheapest) <= spread))
    {
        printf("interface sass loop back: no form cheaper above the floor, the next %.4f ns a turn above %s against a "
               "spread of %.4f\n",
               (second - cheapest) / (double)SASS_LOOP_TIMED, s_kept[chosen], spread / (double)SASS_LOOP_TIMED);
    }
    return kept;
}
