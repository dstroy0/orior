// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line.c: the line: a gate hold on an unknown operation key read, the conference a fault or refusal calls, the line a
// panic opens ruled on and closed, the halt, and what the teacher said
#include "run_channel_internal.h"

int run_channel_key_held(const char *refused)
{
    return strncmp(refused, RUN_GATE_KEY_RULE, sizeof(RUN_GATE_KEY_RULE) - 1u) == 0;
}

int run_channel_vendor_held(const char *refused)
{
    return strncmp(refused, RUN_GATE_VENDOR_RULE, sizeof(RUN_GATE_VENDOR_RULE) - 1u) == 0;
}

RunChannel s_channel;
unsigned int s_registers_past = RUN_REGISTERS_UNANSWERED;
char s_answer_line[RUN_ANSWER_LONGEST];
char s_carrier_output[RUN_CARRIER_OUTPUT];
char s_channel_list[RUN_LIST_ROOM];
char s_carrier_said[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
char s_cases_text[RUN_CASES_TEXT];

// `said` handed to the run's trace: where there is no run, it is said to none
void run_channel_said(const char *said)
{
    qry_member_append("trace", said, strlen(said), QRY_ORDINARY);
}

// `said` written as the refusal of `asked`, cut at the room the refusal holds
void run_channel_refused(RunQuestion *asked, const char *said)
{
    ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
    scriptura_text_most(&refused, said, sizeof(asked->refused) - 1u);
    scriptura_closed(&refused);
}

// `the carrier <its first word> was not started`, the word cut at 96 bytes, written as the refusal of `asked`
void run_channel_started_refused(RunQuestion *asked)
{
    ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
    scriptura_text(&refused, "the carrier ");
    scriptura_text_most(&refused, s_channel.word[0], 96ull);
    scriptura_text(&refused, " was not started");
    scriptura_closed(&refused);
}

// how the carrier ended and its code, `the carrier <ending>, code <hex>`, written as the refusal of `asked`
void run_channel_ending_refused(RunQuestion *asked, const InterfaceAnswer *answer)
{
    ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
    scriptura_text(&refused, "the carrier ");
    scriptura_text(&refused, interface_ending_name(answer->ending));
    scriptura_text(&refused, ", code ");
    scriptura_hex(&refused, answer->code, 0u);
    scriptura_closed(&refused);
}

// `count` in decimal written into `text`, which holds `room`
void run_channel_counted(char *text, size_t room, unsigned long long count)
{
    ScripturaLine counted = {text, room, 0ull};
    scriptura_decimal(&counted, count, 0u);
    scriptura_closed(&counted);
}

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
    run_channel_identity(asked, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    ScripturaLine name_line = {name, sizeof(name), 0ull};
    scriptura_text(&name_line, QRY_CONFERENCE_CALL);
    scriptura_character(&name_line, '_');
    scriptura_text(&name_line, identity);
    scriptura_closed(&name_line);
    // a blob's name takes no space: each space of the identity an underscore
    for (char *space = strchr(name, ' '); space != NULL; space = strchr(space, ' '))
    {
        *space = '_';
    }
    const unsigned long long panic = qry_member_panicked();
    // the account and what the carrier said after it, a record at most half a ring, what the carrier said cut to it
    static char s_conference[QRY_WRITER_RING_BYTES / 2ull];
    ScripturaLine whole = {s_conference, sizeof(s_conference), 0ull};
    scriptura_text(&whole, "teacher: the question of ");
    scriptura_decimal(&whole, asked->code_size, 0u);
    scriptura_text(&whole, " bytes of code, ");
    scriptura_decimal(&whole, asked->registers, 0u);
    scriptura_text(&whole, " registers, ");
    scriptura_decimal(&whole, asked->threads, 0u);
    scriptura_text(&whole, " threads in ");
    scriptura_decimal(&whole, asked->blocks, 0u);
    scriptura_text(&whole, " blocks, ");
    scriptura_decimal(&whole, asked->launches, 0u);
    scriptura_text(&whole, " launches, over ");
    scriptura_decimal(&whole, asked->cases, 0u);
    scriptura_text(&whole, " cases, ");
    scriptura_text_most(&whole, how, 256ull);
    scriptura_text(&whole, "; ");
    scriptura_text(&whole, (panic != 0ull) ? "a panic is on the line" : "the line is quiet");
    scriptura_character(&whole, '\n');
    if (said_size != 0ull)
    {
        const unsigned long long room = scriptura_fits(&whole) ? (whole.capacity - whole.at - 1ull) : 0ull;
        scriptura_counted(&whole, (const char *)said, (said_size < room) ? said_size : room);
    }
    qry_member_append(name, whole.out, whole.at, QRY_ORDINARY);
}

// The sequence of the panic on the line, or 0 while it is quiet: once a panic opens it the run is halted, and the
// teacher puts no question until a conference's ruling closes it (qry_member.h, run_channel_line_ruled)
unsigned long long run_channel_on_line(void)
{
    return qry_member_panicked();
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
    ScripturaLine said_line = {said, sizeof(said), 0ull};
    const int heard = overseer_convened(s_channel.buffer, identity, 0, &ruling);
    const int clean_refusal = heard && ruling.own && (ruling.cause == (unsigned int)OVERSEER_CAUSE_INSTRUCTION) &&
                              (s_channel.carrier_clean != 0);
    if (!clean_refusal)
    {
        s_stopped_on = panic;
    }
    if (!heard)
    {
        scriptura_text(&said_line, "the line stays open, and the run stops: the conference heard no account");
    }
    else if (clean_refusal)
    {
        qry_member_line_closed(panic);
        scriptura_text(&said_line, "the line closed on the ruling, a clean refusal, and the round goes on: ");
        scriptura_text(&said_line, ruling.said);
    }
    else if (ruling.own)
    {
        qry_member_line_closed(panic);
        scriptura_text(&said_line, "the line closed on the ruling, and the run stops: ");
        scriptura_text(&said_line, ruling.said);
    }
    else
    {
        scriptura_text(&said_line, "the line stays open on the ruling, and the run stops: ");
        scriptura_text(&said_line, ruling.said);
    }
    scriptura_closed(&said_line);
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
        ScripturaLine refused = {asked[at]->refused, sizeof(asked[at]->refused), 0ull};
        if (s_stopped_on != 0ull)
        {
            scriptura_text(&refused, "the run stopped: the panic at sequence ");
            scriptura_decimal(&refused, s_stopped_on, 0u);
            scriptura_text(&refused, " was ruled on, and no question is carried after it");
        }
        else
        {
            scriptura_text(&refused, "the run is halted: a panic is on the line at sequence ");
            scriptura_decimal(&refused, on_line, 0u);
        }
        scriptura_closed(&refused);
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
    char said[RUN_CARRIER_ENDING + RUN_LINE_LONGEST + 256u];
    ScripturaLine line = {said, sizeof(said), 0ull};
    scriptura_text(&line, "teacher: the round questions.txt of ");
    scriptura_decimal(&line, questions, 0u);
    scriptura_text(&line, " questions, handed at sequence ");
    scriptura_decimal(&line, round, 0u);
    scriptura_text(&line, "; the carrier ");
    scriptura_text(&line, interface_ending_name(answer->ending));
    scriptura_text(&line, ", code ");
    scriptura_hex(&line, answer->code, 0u);
    scriptura_text(&line, "; ");
    scriptura_decimal(&line, answered, 0u);
    scriptura_text(&line, " answered before it ended; the question it was carrying: ");
    scriptura_text_most(&line, carrying, RUN_LINE_LONGEST);
    if ((carrying[0] == '\0') || (carrying[strlen(carrying) - 1u] != '\n'))
    {
        scriptura_character(&line, '\n');
    }
    scriptura_text(&line, "no question is put until a conference rules\n");
    if (scriptura_fits(&line))
    {
        qry_member_append(QRY_PANIC_CALL, said, line.at, QRY_PANIC);
    }
}
