// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// airlock.c: the airlock: the cross run over the list and passed or held, the questions named and relisted, the check
// traced, and a register hold put again with the count raised
#include "run_channel_internal.h"

// The words `command`, ended by NULL, run as a process given `limit_microseconds`, what it printed handed as the blob
// `said`: the code it exited with, or -1 where it was not started or did not exit of its own
static long long run_channel_airlock_ran(char *const *command, unsigned long long limit_microseconds, const char *said)
{
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    const InterfaceProbe probe = {(char **)command, NULL, limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_airlock_output;
    answer.output_capacity = sizeof(s_airlock_output);
    EngineError error = {0};
    // what this process handed moved into the run before the airlock reads it, and what it handed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    const unsigned long long kept =
        (answer.output_bytes < sizeof(s_airlock_output)) ? answer.output_bytes : sizeof(s_airlock_output);
    // what the pop and the cross printed, the airlock's trace (qry_member.h)
    if (s_channel.buffer != NULL)
    {
        qry_member_append(said, s_airlock_output, kept, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED))
    {
        return -1ll;
    }
    // an exit code is far below what a long long counts
    return (long long)answer.code;
}

// The airlock the round on the channel's list, `questions` of them, passes before the part. The list is handed as
// RUN_AIRLOCK_LIST, each question's answers named RUN_AIRLOCK_ANSWERS before its own name, so that what the pop hands
// stands apart from the round's own answers; the pop is run over it, handing each question's container as the part
// would be handed it; and the cross over the containers, handing the held list the carrier reads (--held). 1 where the
// pop and the cross ran, the cross reading the round (exit 0, or 1 where it held some), and the held list was handed
// after the list; 0 otherwise, and the round is not carried
static int run_channel_airlock_passed(unsigned int questions)
{
    // the list renamed, a record at most as long as the list's room
    static char s_renamed[RUN_LIST_ROOM];
    if ((s_channel.list_size + ((size_t)questions * sizeof(RUN_AIRLOCK_ANSWERS))) > sizeof(s_renamed))
    {
        return 0;
    }
    char *const renamed = s_renamed;
    size_t length = 0u;
    for (size_t at = 0u; at < s_channel.list_size;)
    {
        const char *const line = s_channel_list + at;
        const char *const end = (const char *)memchr(line, '\n', s_channel.list_size - at);
        const size_t line_length = (end != NULL) ? (size_t)(end - line) : (s_channel.list_size - at);
        at += line_length + 1u;
        // a line is `<code> <registers> <cases> <answers> <slot> <launches>`: the answers are its fourth word
        size_t fourth = 0u;
        for (unsigned int spaces = 0u; (fourth < line_length) && (spaces < 3u); fourth += 1u)
        {
            spaces += (line[fourth] == ' ') ? 1u : 0u;
        }
        memcpy(renamed + length, line, fourth);
        length += fourth;
        memcpy(renamed + length, RUN_AIRLOCK_ANSWERS, sizeof(RUN_AIRLOCK_ANSWERS) - 1u);
        length += sizeof(RUN_AIRLOCK_ANSWERS) - 1u;
        memcpy(renamed + length, line + fourth, line_length - fourth);
        length += line_length - fourth;
        renamed[length] = '\n';
        length += 1u;
    }
    const unsigned long long handed = qry_member_append(RUN_AIRLOCK_LIST, renamed, length, QRY_ORDINARY);
    // the list's sequence, once it is moved into the run: the held list and the holds the cross hands come after it
    const unsigned long long began = (handed != 0ull) ? qry_member_settled() : 0ull;
    s_channel.airlock_began = began;
    if (began == 0ull)
    {
        return 0;
    }
    const unsigned long long limit = s_channel.limit_microseconds * ((questions != 0u) ? questions : 1u);
    char list_option[] = "--list";
    char list_name[] = RUN_AIRLOCK_LIST;
    char *command[RUN_CARRIER_WORDS + 4u];
    unsigned int words = 0u;
    for (; words < s_channel.pop_words; words += 1u)
    {
        command[words] = s_channel.pop[words];
    }
    command[words] = list_option;
    command[words + 1u] = list_name;
    command[words + 2u] = NULL;
    const unsigned long long pop_began = run_channel_now();
    const long long popped = run_channel_airlock_ran(command, limit, "airlock_pop.txt");
    s_channel.pop_nanoseconds += run_channel_now() - pop_began;
    if (popped != 0ll)
    {
        return 0;
    }
    // the cross's words, the list after its first, and after its last the holds it hands the teacher and the slots it
    // read clear
    char holds_name[] = RUN_AIRLOCK_HOLDS;
    char read_name[] = RUN_AIRLOCK_READ;
    command[0] = s_channel.cross[0];
    command[1] = list_name;
    for (words = 1u; words < s_channel.cross_words; words += 1u)
    {
        command[words + 1u] = s_channel.cross[words];
    }
    command[words + 1u] = holds_name;
    command[words + 2u] = read_name;
    command[words + 3u] = NULL;
    const unsigned long long cross_began = run_channel_now();
    const long long crossed = run_channel_airlock_ran(command, limit, "airlock_cross.txt");
    s_channel.cross_nanoseconds += run_channel_now() - cross_began;
    if ((crossed != 0ll) && (crossed != 1ll))
    {
        return 0;
    }
    const unsigned char *held = NULL;
    unsigned long long held_size = 0ull;
    unsigned long long held_sequence = 0ull;
    return qry_member_blob(s_channel.held, &held, &held_size, &held_sequence) && (held_sequence >= began);
}

// 1 where the round on the channel's list, `questions` of them, may be carried: it passed the airlock; or the channel
// has none and its carrier is handed the held list of a cross that read the whole pass before it; or the channel is
// dry with no airlock at all, where nothing reaches the part and there is nothing to keep off it. 0 otherwise, `why`
// said into `refused`, which holds `room`: nothing the vendor has not read goes to the part
int run_channel_airlocked(unsigned int questions, char *refused, size_t room)
{
    if (s_channel.pop_words != 0u)
    {
        if (run_channel_airlock_passed(questions))
        {
            return 1;
        }
        ScripturaLine said = {refused, room, 0ull};
        scriptura_text(&said, "the airlock: the cross did not read this round, and nothing goes to the part");
        scriptura_closed(&said);
        return 0;
    }
    if (s_channel.held_given || s_channel.dry)
    {
        return 1;
    }
    ScripturaLine said = {refused, room, 0ull};
    scriptura_text(&said, "the airlock: the channel has none, its carrier no held list, and nothing goes to the part");
    scriptura_closed(&said);
    return 0;
}

// the names of the blobs of the question at `at` of a round, or of a question carried alone where `alone` is 1, each
// into its own room of RUN_NAME_LONGEST
void run_channel_named(unsigned int at, int alone, char *code_name, char *cases_name, char *answers_name)
{
    static const char *const s_stems[3] = {"question", "cases", "answers"};
    static const char *const s_endings[3] = {".bin", ".txt", ".txt"};
    char *const names[3] = {code_name, cases_name, answers_name};
    for (unsigned int name = 0u; name < 3u; name += 1u)
    {
        ScripturaLine line = {names[name], RUN_NAME_LONGEST, 0ull};
        scriptura_text(&line, s_stems[name]);
        if (!alone)
        {
            scriptura_decimal(&line, at, 0u);
        }
        scriptura_text(&line, s_endings[name]);
        scriptura_closed(&line);
    }
}

// The line of the question `asked` on the channel's list, `<code> <registers> <cases> <answers> <slot> <launches>`,
// written into `line`, which holds `room`: its length, or 0 where it did not fit
size_t run_channel_list_line_written(char *line, size_t room, const char *code_name, const char *cases_name,
                                     const char *answers_name, const RunQuestion *asked)
{
    ScripturaLine written = {line, room, 0ull};
    scriptura_text(&written, code_name);
    scriptura_character(&written, ' ');
    scriptura_decimal(&written, asked->registers, 0u);
    scriptura_character(&written, ' ');
    scriptura_text(&written, cases_name);
    scriptura_character(&written, ' ');
    scriptura_text(&written, answers_name);
    scriptura_character(&written, ' ');
    scriptura_decimal(&written, asked->slot, 0u);
    scriptura_character(&written, ' ');
    scriptura_decimal(&written, asked->launches, 0u);
    scriptura_character(&written, '\n');
    return (size_t)scriptura_finish(&written);
}

// The questions of `asked` from `first` to `count` put on the channel's list again, a line each, their blobs as they
// were handed and named as run_channel_named names them: 1, or 0 where there was no memory for the list
int run_channel_relisted(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone)
{
    s_channel.list_size = 0u;
    int listed = 1;
    for (unsigned int at = first; listed && (at < count); at += 1u)
    {
        char code_name[RUN_NAME_LONGEST];
        char cases_name[RUN_NAME_LONGEST];
        char answers_name[RUN_NAME_LONGEST];
        char line[RUN_LINE_LONGEST];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        const size_t line_length =
            run_channel_list_line_written(line, sizeof(line), code_name, cases_name, answers_name, asked[at]);
        listed = (line_length != 0u) && run_channel_listed(line, line_length);
    }
    return listed;
}

// The teacher's trace of the check before the part (qry_member.h, the record `teacher`): `said` of the
// question `asked`, a line handed to the run's trace after its identity, as run_channel_traced hands one, so that a
// branch the learner took is traced back to the check that put its question
void run_channel_check_traced(const RunQuestion *asked, const char *said)
{
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if (s_channel.buffer == NULL)
    {
        return;
    }
    KqrIdentity numbered;
    char identity[96];
    char line[1024];
    run_channel_identity(asked, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    ScripturaLine written = {line, sizeof(line), 0ull};
    scriptura_text(&written, "teacher: ");
    scriptura_text(&written, identity);
    scriptura_text(&written, " checked: ");
    scriptura_text_most(&written, said, sizeof(line) - 128u);
    scriptura_character(&written, '\n');
    if (scriptura_fits(&written))
    {
        qry_member_append("trace", line, written.at, priority);
    }
}

// The holds the airlock's cross handed for the round it last read, each question of `asked` from `first` to `count`
// read off them by its answers' stem: every hold of a question traced, and a question of no shape of its own, whose
// blocks the carrier sizes, held only for registers past the count its kernel allots given the count that clears
// every hold of it, where that is past its own, and that traced. The count of questions given a count
unsigned int run_channel_raised(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone)
{
    const unsigned char *holds = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_member_blob(RUN_AIRLOCK_HOLDS, &holds, &size, &sequence) ||
        (sequence < s_channel.airlock_began))
    {
        return 0u;
    }
    unsigned int raised = 0u;
    for (unsigned int at = first; at < count; at += 1u)
    {
        char code_name[RUN_NAME_LONGEST];
        char cases_name[RUN_NAME_LONGEST];
        char answers_name[RUN_NAME_LONGEST];
        char stem[RUN_NAME_LONGEST + sizeof(RUN_AIRLOCK_ANSWERS)];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        ScripturaLine stem_line = {stem, sizeof(stem), 0ull};
        scriptura_text(&stem_line, RUN_AIRLOCK_ANSWERS);
        scriptura_text_most(&stem_line, answers_name, strcspn(answers_name, "."));
        const size_t stem_length = (size_t)scriptura_closed(&stem_line);
        unsigned int clearing = 0u;
        int held = 0;
        int cleared = 1;
        for (unsigned long long walk = 0ull; walk < size;)
        {
            const char *const line = (const char *)(holds + walk);
            const char *const end = (const char *)memchr(line, '\n', (size_t)(size - walk));
            const size_t length = (end != NULL) ? (size_t)(end - line) : (size_t)(size - walk);
            walk += (unsigned long long)length + 1ull;
            if ((length <= stem_length) || (memcmp(line, stem, stem_length) != 0) || (line[stem_length] != ' '))
            {
                continue;
            }
            // a hold's line: the stem, the count that clears it, and why
            char text[RUN_LINE_LONGEST + 512u];
            ScripturaLine text_line = {text, sizeof(text), 0ull};
            scriptura_text_most(&text_line, line + stem_length + 1u, length - stem_length - 1u);
            scriptura_closed(&text_line);
            char *after = NULL;
            const unsigned long long count_read = strtoull(text, &after, 10);
            const char *const why = after + strspn(after, " ");
            char said[RUN_LINE_LONGEST + 640u];
            ScripturaLine said_line = {said, sizeof(said), 0ull};
            scriptura_text(&said_line, "held, ");
            scriptura_text(&said_line, why);
            scriptura_closed(&said_line);
            run_channel_check_traced(asked[at], said);
            held = 1;
            // a count of registers is at most what a kernel allots a thread, which an unsigned int holds
            cleared = cleared && (count_read != 0ull) && (count_read <= 0xffffffffull);
            clearing = (cleared && ((unsigned int)count_read > clearing)) ? (unsigned int)count_read : clearing;
        }
        const int shaped = (asked[at]->threads != 0u) || (asked[at]->blocks != 0u);
        if (!held)
        {
            continue;
        }
        if (!cleared)
        {
            run_channel_check_traced(asked[at], "kept held: no count of registers clears it");
            continue;
        }
        if (s_registers_past == RUN_REGISTERS_UNANSWERED)
        {
            run_channel_check_traced(asked[at],
                                     "kept held: the part has not answered how far a container's code reaches");
            continue;
        }
        // the cross clears a hold with one past the register it read, and a container's code reaches no further than
        // the part's count past short of what it declares
        const unsigned long long reaching = (unsigned long long)clearing - 1ull + (unsigned long long)s_registers_past;
        if (reaching > RUN_REGISTERS_MOST)
        {
            run_channel_check_traced(asked[at], "kept held: no count of registers a kernel allots reaches it");
            continue;
        }
        // a count at most RUN_REGISTERS_MOST fits an unsigned int
        clearing = (unsigned int)reaching;
        if (shaped)
        {
            run_channel_check_traced(
                asked[at], "kept held: a question of a shape of its own is not put again with more registers");
            continue;
        }
        if (clearing <= asked[at]->registers)
        {
            run_channel_check_traced(asked[at], "kept held: the count that clears it is no more than its own");
            continue;
        }
        char said[160];
        ScripturaLine said_line = {said, sizeof(said), 0ull};
        scriptura_text(&said_line, "put again with ");
        scriptura_decimal(&said_line, clearing, 0u);
        scriptura_text(&said_line, " registers in place of ");
        scriptura_decimal(&said_line, asked[at]->registers, 0u);
        scriptura_text(&said_line, ", every hold of it a register past its count");
        scriptura_closed(&said_line);
        run_channel_check_traced(asked[at], said);
        asked[at]->registers = clearing;
        raised += 1u;
    }
    return raised;
}
