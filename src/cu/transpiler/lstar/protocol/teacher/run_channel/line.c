// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line.c: the line: a gate block on an unknown operation key read, the conference a fault or refusal calls, the line a
// panic opens ruled on and closed, the halt, and what the teacher reported
#include "run_channel_internal.h"

int run_channel_key_gated(const char *refused)
{
    return strncmp(refused, RUN_GATE_KEY_RULE, sizeof(RUN_GATE_KEY_RULE) - 1u) == 0;
}

int run_channel_vendor_gated(const char *refused)
{
    return strncmp(refused, RUN_GATE_VENDOR_RULE, sizeof(RUN_GATE_VENDOR_RULE) - 1u) == 0;
}

RunChannel s_channel;
unsigned int s_registers_past = RUN_REGISTERS_UNANSWERED;
char s_answer_line[RUN_ANSWER_MAX_LEN];
char s_carrier_output[RUN_CARRIER_OUTPUT];
char s_channel_list[RUN_LIST_ROOM];
char s_carrier_text[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
char s_cases_text[RUN_CASES_TEXT];

// `message` passed to the run's trace: where there is no run, it is reported to none
void run_channel_trace_line(const char *message)
{
    qry_member_append("trace", message, strlen(message), QRY_ORDINARY);
}

// `message` written as the refusal of `question`, cut at the room the refusal has
void run_channel_refused(RunQuestion *question, const char *message)
{
    VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
    mmgr_line_put_max(&refused, message, sizeof(question->refused) - 1u);
    mmgr_line_finish(&refused);
}

// `the carrier <its first word> was not started`, the word cut at 96 bytes, written as the refusal of `question`
void run_channel_started_refused(RunQuestion *question)
{
    VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
    mmgr_line_put(&refused, "the carrier ");
    mmgr_line_put_max(&refused, s_channel.word[0], 96ull);
    mmgr_line_put(&refused, " was not started");
    mmgr_line_finish(&refused);
}

// how the carrier ended and its code, `the carrier <ending>, code <hex>`, written as the refusal of `question`
void run_channel_ending_refused(RunQuestion *question, const InterfaceAnswer *answer)
{
    VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
    mmgr_line_put(&refused, "the carrier ");
    mmgr_line_put(&refused, interface_ending_name(answer->ending));
    mmgr_line_put(&refused, ", code ");
    mmgr_line_hex(&refused, answer->code, 0u);
    mmgr_line_finish(&refused);
}

// `count` in decimal written into `text`, which has room for `room`
void run_channel_counted(char *text, size_t room, unsigned long long count)
{
    VerbaLinea counted = {text, room, 0ull};
    mmgr_line_decimal(&counted, count, 0u);
    mmgr_line_finish(&counted);
}

// the panic the teacher has passed its account of: it reports once for each
static unsigned long long s_panic_answered = 0ull;

// the sequence of the panic whose ruling stopped the run, 0 while no panic has been ruled on
static unsigned long long s_stopped_on = 0ull;

int run_channel_panic_ruled(void)
{
    return s_stopped_on != 0ull;
}

// The conference a question the part refused calls (QRY_CONFERENCE_CALL, overseer.h): the teacher's account of it, the
// question as it was put, how it came back and whether a panic is on the line, then every account the carrier reported
// after the refusal in its answer, `output` of `output_size` bytes, as the carrier reported it
void run_channel_conference_called(const RunQuestion *question, const char *how, const unsigned char *output,
                                   unsigned long long output_size)
{
    KqrIdentity numbered;
    char identity[96];
    char name[160];
    run_channel_identity(question, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, QRY_CONFERENCE_CALL);
    mmgr_line_ch(&name_line, '_');
    mmgr_line_put(&name_line, identity);
    mmgr_line_finish(&name_line);
    // a blob's name takes no space: each space of the identity an underscore
    for (char *space = strchr(name, ' '); space != NULL; space = strchr(space, ' '))
    {
        *space = '_';
    }
    const unsigned long long panic = qry_member_panicked();
    // the account and what the carrier reported after it, a record at max half a ring, what the carrier reported cut
    // to it
    static char s_conference[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea whole = {s_conference, sizeof(s_conference), 0ull};
    mmgr_line_put(&whole, "teacher: the question of ");
    mmgr_line_decimal(&whole, question->code_size, 0u);
    mmgr_line_put(&whole, " bytes of code, ");
    mmgr_line_decimal(&whole, question->registers, 0u);
    mmgr_line_put(&whole, " registers, ");
    mmgr_line_decimal(&whole, question->threads, 0u);
    mmgr_line_put(&whole, " threads in ");
    mmgr_line_decimal(&whole, question->blocks, 0u);
    mmgr_line_put(&whole, " blocks, ");
    mmgr_line_decimal(&whole, question->launches, 0u);
    mmgr_line_put(&whole, " launches, over ");
    mmgr_line_decimal(&whole, question->cases, 0u);
    mmgr_line_put(&whole, " cases, ");
    mmgr_line_put_max(&whole, how, 256ull);
    mmgr_line_put(&whole, "; ");
    mmgr_line_put(&whole, (panic != 0ull) ? "a panic is on the line" : "the line is quiet");
    mmgr_line_ch(&whole, '\n');
    if (output_size != 0ull)
    {
        const unsigned long long room = mmgr_line_ok(&whole) ? (whole.capacity - whole.at - 1ull) : 0ull;
        mmgr_line_put_n(&whole, (const char *)output, (output_size < room) ? output_size : room);
    }
    qry_member_append(name, whole.out, whole.at, QRY_ORDINARY);
}

// The sequence of the panic on the line, or 0 while it is quiet: once a panic opens it the run is halted, and the
// teacher puts no question until a conference's ruling closes it (qry_member.h, run_channel_line_ruled)
unsigned long long run_channel_on_line(void)
{
    return qry_member_panicked();
}

// The conference a refusal calls, convened at once where the refusal put a panic on the line (overseer_convened): where
// every cause the parties read is the question's own, its instruction, an address it reached or a register it named,
// whichever clause each names, the line closes, R blocking the question from the part for good; a launch that did not
// return read by any party, or no cause read at all, leaves the line open. A clean refusal, the carrier ended clean
// and the cause ruled the question's own instruction, an encoding the part refused as P9 reads it, stops nothing: the
// round goes on past it. Every other ruling stops the run: no question is carried after its panic
// (run_channel_panic_ruled). The ruling is traced
void run_channel_line_ruled(const RunQuestion *question)
{
    const unsigned long long panic = run_channel_on_line();
    if (panic == 0ull)
    {
        return;
    }
    KqrIdentity numbered;
    char identity[96];
    run_channel_identity(question, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    OverseerRuling ruling;
    char message[OVERSEER_RULING_MAX_LEN + 128u];
    VerbaLinea message_line = {message, sizeof(message), 0ull};
    const int convened = overseer_convened(s_channel.buffer, identity, 0, &ruling);
    const int clean_refusal = convened && ruling.own && (ruling.cause == (unsigned int)OVERSEER_CAUSE_INSTRUCTION) &&
                              (s_channel.carrier_clean != 0);
    if (!clean_refusal)
    {
        s_stopped_on = panic;
    }
    if (!convened)
    {
        mmgr_line_put(&message_line, "the line stays open, and the run stops: the conference received no account");
    }
    else if (clean_refusal)
    {
        qry_member_line_closed(panic);
        mmgr_line_put(&message_line, "the line closed on the ruling, a clean refusal, and the round goes on: ");
        mmgr_line_put(&message_line, ruling.text);
    }
    else if (ruling.own)
    {
        qry_member_line_closed(panic);
        mmgr_line_put(&message_line, "the line closed on the ruling, and the run stops: ");
        mmgr_line_put(&message_line, ruling.text);
    }
    else
    {
        mmgr_line_put(&message_line, "the line stays open on the ruling, and the run stops: ");
        mmgr_line_put(&message_line, ruling.text);
    }
    mmgr_line_finish(&message_line);
    run_channel_check_traced(question, message);
}

// the questions of `questions` from `first` to `count` answered as blocked by the line: no question is put after a
// panic, while the line is open and once its panic has been ruled on
void run_channel_halted(RunQuestion *const *questions, unsigned int first, unsigned int count)
{
    const unsigned long long on_line = run_channel_on_line();
    for (unsigned int at = first; at < count; at += 1u)
    {
        questions[at]->outcome = RUN_NO_CHANNEL;
        VerbaLinea refused = {questions[at]->refused, sizeof(questions[at]->refused), 0ull};
        if (s_stopped_on != 0ull)
        {
            mmgr_line_put(&refused, "the run stopped: the panic at sequence ");
            mmgr_line_decimal(&refused, s_stopped_on, 0u);
            mmgr_line_put(&refused, " was ruled on, and no question is carried after it");
        }
        else
        {
            mmgr_line_put(&refused, "the run is halted: a panic is on the line at sequence ");
            mmgr_line_decimal(&refused, on_line, 0u);
        }
        mmgr_line_finish(&refused);
    }
}

// The teacher's account on the line, passed as a panic of the call once for each panic: the round it passed the
// carrier, its count of questions and its sequence, how the carrier ended, how many it answered before it, the line of
// the question it was carrying, and that no question is put after this
void run_channel_panic_written(unsigned int questions, unsigned long long round, const InterfaceAnswer *answer,
                               unsigned int answered, const char *carrying)
{
    const unsigned long long panic = run_channel_on_line();
    if ((panic == 0ull) || (panic == s_panic_answered))
    {
        return;
    }
    s_panic_answered = panic;
    char message[RUN_CARRIER_ENDING + RUN_LINE_MAX_LEN + 256u];
    VerbaLinea line = {message, sizeof(message), 0ull};
    mmgr_line_put(&line, "teacher: the round questions.txt of ");
    mmgr_line_decimal(&line, questions, 0u);
    mmgr_line_put(&line, " questions, passed at sequence ");
    mmgr_line_decimal(&line, round, 0u);
    mmgr_line_put(&line, "; the carrier ");
    mmgr_line_put(&line, interface_ending_name(answer->ending));
    mmgr_line_put(&line, ", code ");
    mmgr_line_hex(&line, answer->code, 0u);
    mmgr_line_put(&line, "; ");
    mmgr_line_decimal(&line, answered, 0u);
    mmgr_line_put(&line, " answered before it ended; the question it was carrying: ");
    mmgr_line_put_max(&line, carrying, RUN_LINE_MAX_LEN);
    if ((carrying[0] == '\0') || (carrying[strlen(carrying) - 1u] != '\n'))
    {
        mmgr_line_ch(&line, '\n');
    }
    mmgr_line_put(&line, "no question is put until a conference rules\n");
    if (mmgr_line_ok(&line))
    {
        qry_member_append(QRY_PANIC_CALL, message, line.at, QRY_PANIC);
    }
}
