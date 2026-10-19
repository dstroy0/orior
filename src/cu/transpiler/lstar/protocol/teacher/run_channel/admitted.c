// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// admitted.c: the admission: a held question's conference of cross, gate and record, the round checked, the carrier's output handed, one question traced and asked
#include "run_channel_internal.h"
#include "../../../../../includes/parsers/line_read.h"

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
    if (!qry_member_blob(RUN_AIRLOCK_READ, &read, &size, &sequence) ||
        (sequence < s_channel.airlock_began))
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
        ScripturaLine stem_line = {stem, sizeof(stem), 0ull};
        scriptura_text(&stem_line, RUN_AIRLOCK_ANSWERS);
        scriptura_text_most(&stem_line, answers_name, strcspn(answers_name, "."));
        scriptura_closed(&stem_line);
        ScripturaLine popped_line = {popped, sizeof(popped), 0ull};
        scriptura_text(&popped_line, RUN_AIRLOCK_ANSWERS);
        scriptura_text(&popped_line, answers_name);
        scriptura_closed(&popped_line);
        // the pop's answer, the gate's verdict with the part taken out: its first line
        const unsigned char *answer = NULL;
        unsigned long long answer_size = 0ull;
        unsigned long long answer_sequence = 0ull;
        if (!qry_member_blob(popped, &answer, &answer_size, &answer_sequence) ||
            (answer_sequence < s_channel.airlock_began))
        {
            continue;
        }
        char verdict[160];
        ScripturaLine verdict_line = {verdict, sizeof(verdict), 0ull};
        scriptura_text_most(&verdict_line, (const char *)answer, (answer_size < 159ull) ? answer_size : 159ull);
        scriptura_closed(&verdict_line);
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
        ScripturaLine account_line = {account, sizeof(account), 0ull};
        scriptura_text(&account_line, "reads ");
        scriptura_decimal(&account_line, clear, 0u);
        scriptura_text(&account_line, " instruction(s) of it clear, each legal, no control transfer, wait, memory "
                                      "access or register past the count; admit");
        scriptura_closed(&account_line);
        overseer_account(s_channel.buffer, identity, "cross", account);
        account_line.at = 0ull;
        scriptura_text(&account_line, "holds it, ");
        scriptura_text(&account_line, verdict);
        scriptura_text(&account_line, "; gray");
        scriptura_closed(&account_line);
        overseer_account(s_channel.buffer, identity, "gate", account);
        static KqrAsk s_recorded;
        account_line.at = 0ull;
        if (!s_record.open || !kqr_find(&s_record, &numbered, &s_recorded))
        {
            scriptura_text(&account_line, "holds no answer of it; admit");
        }
        else if ((s_recorded.answer == KQR_CENSORED) && run_channel_key_held(s_recorded.refusal))
        {
            scriptura_text(&account_line, "holds it censored under the gate's key rule, the part never saw it; admit");
        }
        else if (s_recorded.answer == KQR_ANSWERS)
        {
            scriptura_text(&account_line, "holds the part's answer to it; hold");
        }
        else
        {
            scriptura_text(&account_line, "holds it ");
            scriptura_text(&account_line, kqr_answer_name(s_recorded.answer));
            scriptura_text(&account_line, ": ");
            scriptura_text(&account_line, s_recorded.refusal);
            scriptura_text(&account_line, "; hold");
        }
        scriptura_closed(&account_line);
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
            ScripturaLine text_line = {text, sizeof(text), 0ull};
            scriptura_text_most(&text_line, line + stem_length + 1u, length - stem_length - 1u);
            scriptura_closed(&text_line);
            const char *at = text;
            if (!line_word(&at, operation, sizeof(operation)) || !line_number(&at, 1, &low) || !line_number(&at, 1, &high))
            {
                continue;
            }
            char encoding[48];
            ScripturaLine encoding_line = {encoding, sizeof(encoding), 0ull};
            scriptura_hex(&encoding_line, low, 16u);
            scriptura_character(&encoding_line, ' ');
            scriptura_hex(&encoding_line, high, 16u);
            scriptura_closed(&encoding_line);
            ruled += overseer_admission(s_channel.buffer, identity, encoding, &ruling) ? 1u : 0u;
            taught += ruling.taught ? 1u : 0u;
        }
        admitted += taught;
        char said[OVERSEER_RULING_LONGEST + 128u];
        ScripturaLine said_line = {said, sizeof(said), 0ull};
        if (ruled != 0u)
        {
            scriptura_text(&said_line, "admitted by its conference: ");
            scriptura_decimal(&said_line, ruled, 0u);
            scriptura_text(&said_line, " encoding(s), ");
            scriptura_decimal(&said_line, taught, 0u);
            scriptura_text(&said_line, " lesson(s) new; ");
            scriptura_text(&said_line, ruling.said);
        }
        else
        {
            scriptura_text(&said_line, "kept held: ");
            scriptura_text(&said_line, (ruling.accounts != 0u) ? ruling.said : "no account of it was heard");
        }
        scriptura_closed(&said_line);
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
            ScripturaLine refused_line = {refused, room, 0ull};
            scriptura_text(&refused_line, "the round could not be listed again after the check, and it is not carried");
            scriptura_closed(&refused_line);
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
    ScripturaLine said = {s_carrier_said, RUN_CARRIER_ENDING, 0ull};
    scriptura_text(&said, "the carrier ");
    scriptura_text(&said, interface_ending_name(answer->ending));
    scriptura_text(&said, ", code ");
    scriptura_hex(&said, answer->code, 0u);
    scriptura_text(&said, ", ");
    scriptura_decimal(&said, answer->output_bytes, 0u);
    scriptura_text(&said, " bytes of output");
    scriptura_text(&said, (kept < answer->output_bytes) ? ", the first of them kept" : "");
    scriptura_character(&said, '\n');
    // the line fits RUN_CARRIER_ENDING, and the output kept fits the rest
    const unsigned long long line = scriptura_closed(&said);
    scriptura_copy(s_carrier_said + line, s_carrier_output, kept);
    qry_member_append("carrier.txt", s_carrier_said, line + kept,
             clean ? QRY_ORDINARY : QRY_PANIC);
}

// The teacher's trace, the path `teacher` (qry_member.h): a line for each question of `asked`, `count` of
// them, handed to the run's trace: its identity, `how` it was answered, and what came of it, with why where it was not
// answered. Where a panic is on the line, the lines are a panic's, and always reach the run
void run_channel_traced(RunQuestion *const *asked, unsigned int count, const char *how)
{
    static const char *const s_outcomes[] = {"answered", "nothing", "illegal", "no channel", "held"};
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if ((s_channel.buffer == NULL) || (count == 0u))
    {
        return;
    }
    // a line is at most its identity, `how`, an outcome and a refusal; the trace is handed whenever the room left
    // could not take one more
    const unsigned long long line_most = 96ull + 64ull + sizeof(asked[0]->refused) + 256ull;
    static char s_trace_bytes[RUN_TRACE_ROOM];
    ScripturaLine text = {s_trace_bytes, sizeof(s_trace_bytes), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((text.capacity - text.at) <= line_most)
        {
            qry_member_append("trace", text.out, text.at, priority);
            text.at = 0ull;
        }
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(asked[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        const unsigned int outcome = asked[at]->outcome;
        scriptura_text(&text, "teacher: ");
        scriptura_text(&text, identity);
        scriptura_character(&text, ' ');
        scriptura_text_most(&text, how, 64ull);
        scriptura_text(&text, ": ");
        scriptura_text(&text, (outcome < (sizeof(s_outcomes) / sizeof(s_outcomes[0]))) ? s_outcomes[outcome] : "?");
        if (asked[at]->refused[0] != '\0')
        {
            scriptura_text(&text, ", ");
            scriptura_text_most(&text, asked[at]->refused, sizeof(asked[at]->refused));
        }
        scriptura_character(&text, '\n');
    }
    qry_member_append("trace", text.out, text.at, priority);
}

int run_channel_ask(RunQuestion *asked)
{
    asked->refused[0] = '\0';
    RunQuestion *const one[1] = {asked};
    // a run whose panic was ruled on carries nothing and answers nothing from R
    if (run_channel_panic_ruled())
    {
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
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
