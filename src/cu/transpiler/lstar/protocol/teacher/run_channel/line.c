// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line.c: the line: a gate hold on an unknown operation key read, the conference a fault or refusal calls, the line a
// panic opens ruled on and closed, the halt, and what the teacher said
#include "run_channel_internal.h"

int run_channel_key_held(const char *refused)
{
    return strncmp(refused, RUN_GATE_KEY_RULE, sizeof(RUN_GATE_KEY_RULE) - 1u) == 0;
}

RunChannel s_channel;
unsigned int s_registers_past = RUN_REGISTERS_UNANSWERED;
char s_answer_line[RUN_ANSWER_LONGEST];
char s_carrier_output[RUN_CARRIER_OUTPUT];
char s_carrier_said[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
char s_cases_text[RUN_CASES_TEXT];

// the panic the teacher has handed its account of: it speaks once for each
static unsigned long long s_panic_answered = 0ull;

// the sequence of the panic whose ruling stopped the run, 0 while no panic has been ruled on
static unsigned long long s_stopped_on = 0ull;

int run_channel_panic_ruled(void)
{
    return s_stopped_on != 0ull;
}

// The conference a question the part refused calls (QRY_CONFERENCE_CALL, overseer.h): the teacher's account of it, the
// question as it was put, how it came back and whether a panic is on the line, then every account the carrier said
// after the refusal in its answer, `said` of `said_size` bytes, as the carrier said it
void run_channel_conference_called(const RunQuestion *asked, const char *how, const unsigned char *said,
                                   unsigned long long said_size)
{
    KqrIdentity numbered;
    char identity[96];
    char name[160];
    char account[512];
    run_channel_identity(asked, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    snprintf(name, sizeof(name), "%s_%s", QRY_CONFERENCE_CALL, identity);
    // a blob's name takes no space: each space of the identity an underscore
    for (char *space = strchr(name, ' '); space != NULL; space = strchr(space, ' '))
    {
        *space = '_';
    }
    const unsigned long long panic = qry_panicked(s_channel.buffer);
    const int length =
        snprintf(account, sizeof(account),
                 "teacher: the question of %llu bytes of code, %u registers, %u threads in %u blocks, %u "
                 "launches, over %u cases, %s; %s\n",
                 asked->code_size, asked->registers, asked->threads, asked->blocks, asked->launches, asked->cases, how,
                 (panic != 0ull) ? "a panic is on the line" : "the line is quiet");
    // the account fits its room, and its length is not negative
    const size_t kept = (length < 0)                         ? 0u
                        : ((size_t)length < sizeof(account)) ? (size_t)length
                                                             : (sizeof(account) - 1u);
    unsigned char *const whole = (unsigned char *)malloc(kept + (size_t)said_size);
    if (whole == NULL)
    {
        return;
    }
    memcpy(whole, account, kept);
    if (said_size != 0ull)
    {
        memcpy(whole + kept, said, (size_t)said_size);
    }
    qry_hand(s_channel.buffer, name, whole, (unsigned long long)kept + said_size, QRY_ORDINARY);
    free(whole);
}

// The sequence of the panic on the line, or 0 while it is quiet: once a panic opens it the run is halted, and the
// teacher puts no question until a conference's ruling closes it (qry_buffer.h, run_channel_line_ruled)
unsigned long long run_channel_on_line(void)
{
    return qry_panicked(s_channel.buffer);
}

// The conference a refusal calls, held at once where the refusal put a panic on the line (overseer_convened): where
// every cause the parties read is the question's own, its instruction, an address it reached or a register it named,
// whichever clause each names, the line closes, R holding the question off the part for good; a launch that did not
// return read by any party, or no cause read at all, leaves the line open. A clean refusal, the carrier ended clean
// and the cause ruled the question's own instruction, an encoding the part refused as P9 reads it, stops nothing: the
// round goes on past it. Every other ruling stops the run: no question is carried after its panic
// (run_channel_panic_ruled). The ruling is traced
void run_channel_line_ruled(const RunQuestion *asked)
{
    const unsigned long long panic = run_channel_on_line();
    if (panic == 0ull)
    {
        return;
    }
    KqrIdentity numbered;
    char identity[96];
    run_channel_identity(asked, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    OverseerRuling ruling;
    char said[OVERSEER_RULING_LONGEST + 128u];
    const int heard = overseer_convened(s_channel.buffer, identity, 0, &ruling);
    const int clean_refusal = heard && ruling.own && (ruling.cause == (unsigned int)OVERSEER_CAUSE_INSTRUCTION) &&
                              (s_channel.carrier_clean != 0);
    if (!clean_refusal)
    {
        s_stopped_on = panic;
    }
    if (!heard)
    {
        snprintf(said, sizeof(said), "the line stays open, and the run stops: the conference heard no account");
    }
    else if (clean_refusal)
    {
        qry_line_closed(s_channel.buffer, panic);
        snprintf(said, sizeof(said), "the line closed on the ruling, a clean refusal, and the round goes on: %s",
                 ruling.said);
    }
    else if (ruling.own)
    {
        qry_line_closed(s_channel.buffer, panic);
        snprintf(said, sizeof(said), "the line closed on the ruling, and the run stops: %s", ruling.said);
    }
    else
    {
        snprintf(said, sizeof(said), "the line stays open on the ruling, and the run stops: %s", ruling.said);
    }
    run_channel_check_traced(asked, said);
}

// the questions of `asked` from `first` to `count` answered as held by the line: no question is put after a panic,
// while the line is open and once its panic has been ruled on
void run_channel_halted(RunQuestion *const *asked, unsigned int first, unsigned int count)
{
    const unsigned long long on_line = run_channel_on_line();
    for (unsigned int at = first; at < count; at += 1u)
    {
        asked[at]->outcome = RUN_NO_CHANNEL;
        if (s_stopped_on != 0ull)
        {
            snprintf(asked[at]->refused, sizeof(asked[at]->refused),
                     "the run stopped: the panic at sequence %llu was ruled on, and no question is carried after it",
                     s_stopped_on);
        }
        else
        {
            snprintf(asked[at]->refused, sizeof(asked[at]->refused),
                     "the run is halted: a panic is on the line at sequence %llu", on_line);
        }
    }
}

// The teacher's account on the line, handed as a panic of the call once for each panic: the round it handed the
// carrier, its count of questions and its sequence, how the carrier ended, how many it answered before it, the line of
// the question it was carrying, and that no question is put after this
void run_channel_teacher_said(unsigned int questions, unsigned long long round, const InterfaceAnswer *answer,
                              unsigned int answered, const char *carrying)
{
    const unsigned long long panic = run_channel_on_line();
    if ((panic == 0ull) || (panic == s_panic_answered))
    {
        return;
    }
    s_panic_answered = panic;
    char said[RUN_CARRIER_ENDING + RUN_LINE_LONGEST + 96u];
    const int length =
        snprintf(said, sizeof(said),
                 "teacher: the round questions.txt of %u questions, handed at sequence %llu; the carrier "
                 "%s, code %llx; %u answered before it ended; the question it was carrying: %s%sno "
                 "question is put until a conference rules\n",
                 questions, round, interface_ending_name(answer->ending), answer->code, answered, carrying,
                 ((carrying[0] != '\0') && (carrying[strlen(carrying) - 1u] == '\n')) ? "" : "\n");
    // the account fits its room, and its length is not negative
    qry_hand(s_channel.buffer, QRY_PANIC_CALL, said, (unsigned long long)((length > 0) ? length : 0), QRY_PANIC);
}
