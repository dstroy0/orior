// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// admitted.c: the admission: a gated question's conference of cross, gate and record, the round checked, the carrier's
// output handed, one question traced and queried
#include "run_channel_internal.h"
#include "../../../../../../../includes/parsers/line_read.h"

// The conferences the gate calls for the round the airlock last read: each question of `questions` from `first` to
// `count` whose answer the pop gave is a block under the key rule (RUN_GATE_KEY_RULE), read against what the cross
// read clear (RUN_AIRLOCK_READ) by its answers' stem, its probe place or every instruction of a question read whole.
// Where the cross read any of it clear, the accounts are handed, the cross's, the gate's and the record's, and the
// conference convened for each encoding (overseer_admission): each admitted is a lesson the carrier loads (--lessons),
// and the question is put as the round is carried; every other stays blocked. Each is traced. The count of lessons
// taught that the run did not block
static unsigned int run_channel_admitted(RunQuestion *const *questions, unsigned int first, unsigned int count,
                                         int alone)
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
        char code_name[RUN_NAME_MAX_LEN];
        char cases_name[RUN_NAME_MAX_LEN];
        char answers_name[RUN_NAME_MAX_LEN];
        char stem[RUN_NAME_MAX_LEN + sizeof(RUN_AIRLOCK_ANSWERS)];
        char popped[RUN_NAME_MAX_LEN + sizeof(RUN_AIRLOCK_ANSWERS)];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        VerbaLinea stem_line = {stem, sizeof(stem), 0ull};
        mmgr_line_put(&stem_line, RUN_AIRLOCK_ANSWERS);
        mmgr_line_put_most(&stem_line, answers_name, strcspn(answers_name, "."));
        mmgr_line_finish(&stem_line);
        VerbaLinea popped_line = {popped, sizeof(popped), 0ull};
        mmgr_line_put(&popped_line, RUN_AIRLOCK_ANSWERS);
        mmgr_line_put(&popped_line, answers_name);
        mmgr_line_finish(&popped_line);
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
        VerbaLinea verdict_line = {verdict, sizeof(verdict), 0ull};
        mmgr_line_put_most(&verdict_line, (const char *)answer, (answer_size < 159ull) ? answer_size : 159ull);
        mmgr_line_finish(&verdict_line);
        verdict[strcspn(verdict, "\r\n")] = '\0';
        if (!run_channel_key_gated(verdict))
        {
            continue;
        }
        // the cross's clear readings of the question, the lines of the read list that lead with its stem: its probe
        // place's, or every instruction's of a question read whole, each `<operation> <low> <high>`
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
            run_channel_check_traced(
                questions[at],
                "still blocked: the gate blocks its key unknown, and the cross read nothing of it clear");
            continue;
        }
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(questions[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        // the accounts: the cross's reading, the gate's block, and what the record contains of the identity
        char account[512];
        VerbaLinea account_line = {account, sizeof(account), 0ull};
        mmgr_line_put(&account_line, "reads ");
        mmgr_line_decimal(&account_line, clear, 0u);
        mmgr_line_put(&account_line, " instruction(s) of it clear, each legal, no control transfer, wait, memory "
                                      "access or register past the count; admit");
        mmgr_line_finish(&account_line);
        overseer_account(s_channel.buffer, identity, "cross", account);
        account_line.at = 0ull;
        mmgr_line_put(&account_line, "contains it,");
        mmgr_line_put(&account_line, verdict);
        mmgr_line_put(&account_line, "; gray");
        mmgr_line_finish(&account_line);
        overseer_account(s_channel.buffer, identity, "gate", account);
        static KqrQuery s_recorded;
        account_line.at = 0ull;
        if (!s_record.open || !kqr_find(&s_record, &numbered, &s_recorded))
        {
            mmgr_line_put(&account_line, "contains no answer of it; admit");
        }
        else if ((s_recorded.answer == KQR_CENSORED) && run_channel_key_gated(s_recorded.refusal))
        {
            mmgr_line_put(&account_line,
                           "contains it censored under the gate's key rule, the part never saw it; admit");
        }
        else if (s_recorded.answer == KQR_ANSWERS)
        {
            mmgr_line_put(&account_line, "contains the part's answer to it; block");
        }
        else
        {
            mmgr_line_put(&account_line, "contains it");
            mmgr_line_put(&account_line, kqr_answer_name(s_recorded.answer));
            mmgr_line_put(&account_line, ": ");
            mmgr_line_put(&account_line, s_recorded.refusal);
            mmgr_line_put(&account_line, "; block");
        }
        mmgr_line_finish(&account_line);
        overseer_account(s_channel.buffer, identity, "record", account);
        // the conference convened for each encoding read clear: a lesson taught for each the run did not block
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
            VerbaLinea text_line = {text, sizeof(text), 0ull};
            mmgr_line_put_most(&text_line, line + stem_length + 1u, length - stem_length - 1u);
            mmgr_line_finish(&text_line);
            const char *at = text;
            if (!line_word(&at, operation, sizeof(operation)) || !line_number(&at, 1, &low) || !line_number(&at, 1, &high))
            {
                continue;
            }
            char encoding[48];
            VerbaLinea encoding_line = {encoding, sizeof(encoding), 0ull};
            mmgr_line_hex(&encoding_line, low, 16u);
            mmgr_line_ch(&encoding_line, ' ');
            mmgr_line_hex(&encoding_line, high, 16u);
            mmgr_line_finish(&encoding_line);
            ruled += overseer_admission(s_channel.buffer, identity, encoding, &ruling) ? 1u : 0u;
            taught += ruling.taught ? 1u : 0u;
        }
        admitted += taught;
        char message[OVERSEER_RULING_MAX_LEN + 128u];
        VerbaLinea message_line = {message, sizeof(message), 0ull};
        if (ruled != 0u)
        {
            mmgr_line_put(&message_line, "admitted by its conference: ");
            mmgr_line_decimal(&message_line, ruled, 0u);
            mmgr_line_put(&message_line, " encoding(s), ");
            mmgr_line_decimal(&message_line, taught, 0u);
            mmgr_line_put(&message_line, " lesson(s) new; ");
            mmgr_line_put(&message_line, ruling.text);
        }
        else
        {
            mmgr_line_put(&message_line, "still blocked: ");
            mmgr_line_put(&message_line, (ruling.accounts != 0u) ? ruling.text : "no account of it was received");
        }
        mmgr_line_finish(&message_line);
        run_channel_check_traced(questions[at], message);
    }
    return admitted;
}

// The round on the channel's list, the questions of `questions` from `first` to `count`, checked before the part and
// fixed where a count of registers fixes it: through the airlock, then each question its cross refuses only for
// registers past the count its kernel allots given the count that clears it, the round listed again and read again,
// until no question is given one; every refusal and every count given traced (run_channel_raised). The reading again
// ends: a count only rises, and to at most what a kernel allots a thread. 1 where the round may be carried, 0 with why
// in `refused`, which has room for `room`
int run_channel_checked(RunQuestion *const *questions, unsigned int first, unsigned int count, int alone, char *refused,
                        size_t room)
{
    for (;;)
    {
        if (!run_channel_airlocked(count - first, refused, room))
        {
            return 0;
        }
        // the conferences the gate calls, each question it blocks for an unknown key the cross read clear admitted
        // or still blocked, before the round's registers are raised; a round that taught a lesson is read again, so
        // that the pop puts what the carrier will
        const unsigned int taught =
            (s_channel.pop_words != 0u) ? run_channel_admitted(questions, first, count, alone) : 0u;
        if ((s_channel.pop_words == 0u) ||
            ((run_channel_raised(questions, first, count, alone) == 0u) && (taught == 0u)))
        {
            return 1;
        }
        if (!run_channel_relisted(questions, first, count, alone))
        {
            VerbaLinea refused_line = {refused, room, 0ull};
            mmgr_line_put(&refused_line, "the round could not be listed again after the check, and it is not carried");
            mmgr_line_finish(&refused_line);
            return 0;
        }
    }
}

// How the carrier ended and what it wrote, handed as carrier.txt: a line that names its ending, then its output. A
// panic where the carrier did not end clean, the carrier losing its mind: its ending is the record of it
void run_channel_output_handed(const InterfaceAnswer *answer)
{
    const int clean = (answer->ending == INTERFACE_ENDING_EXITED) && (answer->code == 0ull);
    const unsigned long long copied = (answer->output_bytes < (sizeof(s_carrier_output) - 1u))
                                          ? answer->output_bytes
                                          : (sizeof(s_carrier_output) - 1u);
    VerbaLinea ending_line = {s_carrier_text, RUN_CARRIER_ENDING, 0ull};
    mmgr_line_put(&ending_line, "the carrier ");
    mmgr_line_put(&ending_line, interface_ending_name(answer->ending));
    mmgr_line_put(&ending_line, ", code ");
    mmgr_line_hex(&ending_line, answer->code, 0u);
    mmgr_line_put(&ending_line, ", ");
    mmgr_line_decimal(&ending_line, answer->output_bytes, 0u);
    mmgr_line_put(&ending_line, " bytes of output");
    mmgr_line_put(&ending_line, (copied < answer->output_bytes) ? ", the first of them retained" : "");
    mmgr_line_ch(&ending_line, '\n');
    // the line fits RUN_CARRIER_ENDING, and the output retained fits the rest
    const unsigned long long line = mmgr_line_finish(&ending_line);
    memcpy(s_carrier_text + line, s_carrier_output, copied);
    qry_member_append("carrier.txt", s_carrier_text, line + copied, clean ? QRY_ORDINARY : QRY_PANIC);
}

// The teacher's trace, the path `teacher` (qry_member.h): a line for each question of `questions`, `count` of
// them, handed to the run's trace: its identity, `how` it was answered, and what came of it, with why where it was not
// answered. Where a panic is on the line, the lines are a panic's, and always reach the run
void run_channel_traced(RunQuestion *const *questions, unsigned int count, const char *how)
{
    static const char *const s_outcomes[] = {"answered", "nothing", "illegal", "no channel", "gated"};
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if ((s_channel.buffer == NULL) || (count == 0u))
    {
        return;
    }
    // a line is at most its identity, `how`, an outcome and a refusal; the trace is handed whenever the room left
    // could not take one more
    const unsigned long long line_most = 96ull + 64ull + sizeof(questions[0]->refused) + 256ull;
    static char s_trace_bytes[RUN_TRACE_ROOM];
    VerbaLinea text = {s_trace_bytes, sizeof(s_trace_bytes), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((text.capacity - text.at) <= line_most)
        {
            qry_member_append("trace", text.out, text.at, priority);
            text.at = 0ull;
        }
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(questions[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        const unsigned int outcome = questions[at]->outcome;
        mmgr_line_put(&text, "teacher: ");
        mmgr_line_put(&text, identity);
        mmgr_line_ch(&text, ' ');
        mmgr_line_put_most(&text, how, 64ull);
        mmgr_line_put(&text, ": ");
        mmgr_line_put(&text, (outcome < (sizeof(s_outcomes) / sizeof(s_outcomes[0]))) ? s_outcomes[outcome] : "?");
        if (questions[at]->refused[0] != '\0')
        {
            mmgr_line_put(&text, ", ");
            mmgr_line_put_most(&text, questions[at]->refused, sizeof(questions[at]->refused));
        }
        mmgr_line_ch(&text, '\n');
    }
    qry_member_append("trace", text.out, text.at, priority);
}

int run_channel_put(RunQuestion *question)
{
    question->refused[0] = '\0';
    RunQuestion *const one[1] = {question};
    // a run whose panic was ruled on carries nothing and answers nothing from R
    if (run_channel_panic_ruled())
    {
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    if (run_channel_recorded(question))
    {
        run_channel_traced(one, 1u, "from R");
        return question->outcome == RUN_ANSWERED;
    }
    const int answered = run_channel_carried(question);
    if (!s_channel.dry && !s_channel.stopped)
    {
        run_channel_stored(question);
    }
    run_channel_traced(one, 1u, s_channel.dry ? "written dry" : "carried");
    return answered;
}
