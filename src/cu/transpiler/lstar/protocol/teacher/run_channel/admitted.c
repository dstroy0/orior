// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// admitted.c: the admission: a held question's conference of cross, gate and record, the round checked, the carrier's output handed, one question traced and asked
#include "run_channel_internal.h"

// The conferences the gate calls for the round the airlock last read: each question of `asked` from `first` to
// `count` whose answer the pop gave is a hold under the key rule (RUN_GATE_KEY_RULE), read against what the cross
// read clear (RUN_AIRLOCK_READ) by its answers' stem, its slot or every instruction of a question read whole. Where
// the cross read any of it clear, the accounts are handed, the cross's, the gate's and the record's, and the conference
// held for each encoding (overseer_admission): each admitted is a lesson the carrier loads (--lessons), and the
// question is put as the round is carried; every other stays held. Each is traced. The count of lessons taught
// that the run did not hold
static unsigned int run_channel_admitted(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone)
{
    const unsigned char *read = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_latest(s_channel.buffer, RUN_AIRLOCK_READ, &read, &size, &sequence) ||
        (sequence <= s_channel.airlock_began))
    {
        return 0u;
    }
    unsigned int admitted = 0u;
    for (unsigned int at = first; at < count; at += 1u)
    {
        char code_name[RUN_NAME_LONGEST];
        char cases_name[RUN_NAME_LONGEST];
        char answers_name[RUN_NAME_LONGEST];
        char stem[RUN_NAME_LONGEST + sizeof(RUN_AIRLOCK_ANSWERS)];
        char popped[RUN_NAME_LONGEST + sizeof(RUN_AIRLOCK_ANSWERS)];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        snprintf(stem, sizeof(stem), "%s%.*s", RUN_AIRLOCK_ANSWERS, (int)strcspn(answers_name, "."), answers_name);
        snprintf(popped, sizeof(popped), "%s%s", RUN_AIRLOCK_ANSWERS, answers_name);
        // the pop's answer, the gate's verdict with the part taken out: its first line
        const unsigned char *answer = NULL;
        unsigned long long answer_size = 0ull;
        unsigned long long answer_sequence = 0ull;
        if (!qry_latest(s_channel.buffer, popped, &answer, &answer_size, &answer_sequence) ||
            (answer_sequence <= s_channel.airlock_began))
        {
            continue;
        }
        char verdict[160];
        snprintf(verdict, sizeof(verdict), "%.*s", (int)((answer_size < 159ull) ? answer_size : 159ull),
                 (const char *)answer);
        verdict[strcspn(verdict, "\r\n")] = '\0';
        if (!run_channel_key_held(verdict))
        {
            continue;
        }
        // the cross's clear readings of the question, the lines of the read list that lead with its stem: its slot's,
        // or every instruction's of a question read whole, each `<operation> <low> <high>`
        const size_t stem_length = strlen(stem);
        unsigned int clear = 0u;
        for (unsigned long long walk = 0ull; walk < size;)
        {
            const char *const line = (const char *)(read + walk);
            const char *const end = (const char *)memchr(line, '\n', (size_t)(size - walk));
            const size_t length = (end != NULL) ? (size_t)(end - line) : (size_t)(size - walk);
            walk += (unsigned long long)length + 1ull;
            clear += ((length > stem_length) && (memcmp(line, stem, stem_length) == 0) && (line[stem_length] == ' '))
                         ? 1u
                         : 0u;
        }
        if (clear == 0u)
        {
            run_channel_check_traced(asked[at],
                                     "kept held: the gate holds its key unknown, and the cross read nothing of it clear");
            continue;
        }
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(asked[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        // the accounts: the cross's reading, the gate's hold, and what the record holds of the identity
        char account[512];
        snprintf(account, sizeof(account),
                 "reads %u instruction(s) of it clear, each legal, no control transfer, wait, memory access or "
                 "register past the count; admit",
                 clear);
        overseer_account(s_channel.buffer, identity, "cross", account);
        snprintf(account, sizeof(account), "holds it, %s; gray", verdict);
        overseer_account(s_channel.buffer, identity, "gate", account);
        static KqrAsk s_recorded;
        if (!s_record.open || !kqr_find(&s_record, &numbered, &s_recorded))
        {
            snprintf(account, sizeof(account), "holds no answer of it; admit");
        }
        else if ((s_recorded.answer == KQR_CENSORED) && run_channel_key_held(s_recorded.refusal))
        {
            snprintf(account, sizeof(account),
                     "holds it censored under the gate's key rule, the part never saw it; admit");
        }
        else if (s_recorded.answer == KQR_ANSWERS)
        {
            snprintf(account, sizeof(account), "holds the part's answer to it; hold");
        }
        else
        {
            snprintf(account, sizeof(account), "holds it %s: %s; hold", kqr_answer_name(s_recorded.answer),
                     s_recorded.refusal);
        }
        overseer_account(s_channel.buffer, identity, "record", account);
        // the conference held for each encoding read clear: a lesson taught for each the run did not hold
        unsigned int taught = 0u;
        unsigned int ruled = 0u;
        OverseerRuling ruling;
        memset(&ruling, 0, sizeof(ruling));
        for (unsigned long long walk = 0ull; walk < size;)
        {
            const char *const line = (const char *)(read + walk);
            const char *const end = (const char *)memchr(line, '\n', (size_t)(size - walk));
            const size_t length = (end != NULL) ? (size_t)(end - line) : (size_t)(size - walk);
            walk += (unsigned long long)length + 1ull;
            if ((length <= stem_length) || (memcmp(line, stem, stem_length) != 0) || (line[stem_length] != ' '))
            {
                continue;
            }
            char operation[64];
            unsigned long long low = 0ull;
            unsigned long long high = 0ull;
            char text[400];
            snprintf(text, sizeof(text), "%.*s", (int)(length - stem_length - 1u), line + stem_length + 1u);
            if (sscanf(text, "%63s %llx %llx", operation, &low, &high) != 3)
            {
                continue;
            }
            char encoding[48];
            snprintf(encoding, sizeof(encoding), "%016llx %016llx", low, high);
            ruled += overseer_admission(s_channel.buffer, identity, encoding, &ruling) ? 1u : 0u;
            taught += ruling.taught ? 1u : 0u;
        }
        admitted += taught;
        char said[OVERSEER_RULING_LONGEST + 128u];
        if (ruled != 0u)
        {
            snprintf(said, sizeof(said), "admitted by its conference: %u encoding(s), %u lesson(s) new; %s", ruled,
                     taught, ruling.said);
        }
        else
        {
            snprintf(said, sizeof(said), "kept held: %s",
                     (ruling.accounts != 0u) ? ruling.said : "no account of it was heard");
        }
        run_channel_check_traced(asked[at], said);
    }
    return admitted;
}

// The round on the channel's list, the questions of `asked` from `first` to `count`, checked before the part and fixed
// where a count of registers fixes it: through the airlock, then each question its cross holds only for registers past
// the count its kernel allots given the count that clears it, the round listed again and read again, until no
// question is given one; every hold and every count given traced (run_channel_raised). The reading again ends: a count
// only rises, and to at most what a kernel allots a thread. 1 where the round may be carried, 0 with why in `refused`,
// which holds `room`
int run_channel_checked(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone,
                               char *refused, size_t room)
{
    for (;;)
    {
        if (!run_channel_airlocked(count - first, refused, room))
        {
            return 0;
        }
        // the conferences the gate calls, each question it holds for an unknown key the cross read clear admitted
        // or kept held, before the round's registers are raised; a round that taught a lesson is read again, so that
        // the pop puts what the carrier will
        const unsigned int taught = (s_channel.pop_words != 0u) ? run_channel_admitted(asked, first, count, alone) : 0u;
        if ((s_channel.pop_words == 0u) ||
            ((run_channel_raised(asked, first, count, alone) == 0u) && (taught == 0u)))
        {
            return 1;
        }
        if (!run_channel_relisted(asked, first, count, alone))
        {
            snprintf(refused, room, "the round could not be listed again after the check, and it is not carried");
            return 0;
        }
    }
}

// How the carrier ended and what it wrote, handed as carrier.txt: a line that names its ending, then its output. A
// panic where the carrier did not end clean, the carrier losing its mind: its ending is the record of it
void run_channel_output_handed(const InterfaceAnswer *answer)
{
    const int clean = (answer->ending == INTERFACE_ENDING_EXITED) && (answer->code == 0ull);
    const unsigned long long kept = (answer->output_bytes < (sizeof(s_carrier_output) - 1u))
                                        ? answer->output_bytes
                                        : (sizeof(s_carrier_output) - 1u);
    const int said = snprintf(s_carrier_said, RUN_CARRIER_ENDING, "the carrier %s, code %llx, %llu bytes of output%s\n",
                              interface_ending_name(answer->ending), answer->code, answer->output_bytes,
                              (kept < answer->output_bytes) ? ", the first of them kept" : "");
    // the line fits RUN_CARRIER_ENDING, and the output kept fits the rest
    const size_t line = (said > 0) ? (size_t)said : 0u;
    memcpy(s_carrier_said + line, s_carrier_output, (size_t)kept);
    qry_hand(s_channel.buffer, "carrier.txt", s_carrier_said, (unsigned long long)line + kept,
             clean ? QRY_ORDINARY : QRY_PANIC);
}

// The teacher's trace, the path `teacher` (qry_buffer.h, QRY_TRACE): a line for each question of `asked`, `count` of
// them, handed to the run's trace: its identity, `how` it was answered, and what came of it, with why where it was not
// answered. Where a panic is on the line, the lines are a panic's, and always reach the run
void run_channel_traced(RunQuestion *const *asked, unsigned int count, const char *how)
{
    static const char *const s_outcomes[] = {"answered", "nothing", "illegal", "no channel", "held"};
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if ((s_channel.buffer == NULL) || (count == 0u) || !qry_traced("teacher", priority))
    {
        return;
    }
    const size_t line_most = 96u + 64u + sizeof(asked[0]->refused);
    char *const text = (char *)malloc(((size_t)count * line_most) + 1u);
    if (text == NULL)
    {
        return;
    }
    size_t length = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(asked[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        const unsigned int outcome = asked[at]->outcome;
        const int written =
            snprintf(text + length, line_most, "teacher: %s %s: %s%s%s\n", identity, how,
                     (outcome < (sizeof(s_outcomes) / sizeof(s_outcomes[0]))) ? s_outcomes[outcome] : "?",
                     (asked[at]->refused[0] != '\0') ? ", " : "", asked[at]->refused);
        // a line fits the room kept for it, and its length is not negative
        length += (written > 0) ? (((size_t)written < line_most) ? (size_t)written : (line_most - 1u)) : 0u;
    }
    qry_hand(s_channel.buffer, "trace", text, (unsigned long long)length, QRY_ORDINARY);
    free(text);
}

int run_channel_ask(RunQuestion *asked)
{
    asked->refused[0] = '\0';
    RunQuestion *const one[1] = {asked};
    if (run_channel_recorded(asked))
    {
        run_channel_traced(one, 1u, "from R");
        return asked->outcome == RUN_ANSWERED;
    }
    const int answered = run_channel_carried(asked);
    if (!s_channel.dry && !s_channel.stopped)
    {
        run_channel_kept(asked);
    }
    run_channel_traced(one, 1u, s_channel.dry ? "written dry" : "carried");
    return answered;
}
