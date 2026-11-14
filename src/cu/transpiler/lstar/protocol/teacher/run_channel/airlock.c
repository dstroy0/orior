// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// airlock.c: the airlock: the cross run over the list and passed or gated, the questions named and relisted, the check
// traced, and a register refusal put again with the count raised
#include "run_channel_internal.h"

// The words `command`, ended by NULL, run as a process given `limit_microseconds`, what it printed passed as the blob
// returned: the code it exited with, or -1 where it was not started or did not exit of its own
static long long run_channel_airlock_ran(char *const *command, unsigned long long limit_microseconds,
                                         const char *member)
{
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    const InterfaceProbe probe = {(char **)command, NULL, limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_airlock_output;
    answer.output_capacity = sizeof(s_airlock_output);
    EngineError error = {0};
    // what this process passed moved into the run before the airlock reads it, and what it passed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    const unsigned long long copied =
        (answer.output_bytes < sizeof(s_airlock_output)) ? answer.output_bytes : sizeof(s_airlock_output);
    // what the pop and the cross printed, the airlock's trace (qry_member.h)
    if (s_channel.buffer != NULL)
    {
        qry_member_append(member, s_airlock_output, copied, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED))
    {
        return -1ll;
    }
    // an exit code is far below what a long long counts
    return (long long)answer.code;
}

// The airlock the round on the channel's list, `questions` of them, passes before the part. The list is passed as
// RUN_AIRLOCK_LIST, each question's answers named RUN_AIRLOCK_ANSWERS before its own name, so that what the pop passes
// is apart from the round's own answers; the pop is run over it, handing each question's container as the part
// would be passed it; and the cross over the containers, handing the gated list the carrier reads (--gated). 1 where
// the pop and the cross ran, the cross reading the round (exit 0, or 1 where it gated some), and the gated list was
// passed after the list; 0 otherwise, and the round is not carried
static int run_channel_airlock_passed(unsigned int questions)
{
    // the list renamed, a record at max as long as the list's room
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
        // a line is `<code> <registers> <cases> <answers> <probe> <launches>`: the answers are its fourth word
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
    const unsigned long long passed = qry_member_append(RUN_AIRLOCK_LIST, renamed, length, QRY_ORDINARY);
    // the list's sequence, once it is moved into the run: the gated list and the refusals the cross passes come after it
    const unsigned long long began = (passed != 0ull) ? qry_member_settled() : 0ull;
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
    // the cross's words, the list after its first, and after its last the refusals it passes the teacher and the probe
    // places it read clear
    char gated_name[] = RUN_AIRLOCK_GATED;
    char read_name[] = RUN_AIRLOCK_READ;
    command[0] = s_channel.cross[0];
    command[1] = list_name;
    for (words = 1u; words < s_channel.cross_words; words += 1u)
    {
        command[words + 1u] = s_channel.cross[words];
    }
    command[words + 1u] = gated_name;
    command[words + 2u] = read_name;
    command[words + 3u] = NULL;
    const unsigned long long cross_began = run_channel_now();
    const long long crossed = run_channel_airlock_ran(command, limit, "airlock_cross.txt");
    s_channel.cross_nanoseconds += run_channel_now() - cross_began;
    if ((crossed != 0ll) && (crossed != 1ll))
    {
        return 0;
    }
    const unsigned char *gated = NULL;
    unsigned long long gated_size = 0ull;
    unsigned long long gated_sequence = 0ull;
    return qry_member_blob(s_channel.gated_list, &gated, &gated_size, &gated_sequence) && (gated_sequence >= began);
}

// 1 where the round on the channel's list, `questions` of them, may be carried: it passed the airlock; or the channel
// has none and its carrier is passed the gated list of a cross that read the whole pass before it; or the channel is
// dry with no airlock at all, where nothing reaches the part and there is nothing to block off it. 0 otherwise, `why`
// reported into `refused`, which has room for `room`: nothing the vendor has not read goes to the part
int run_channel_airlocked(unsigned int questions, char *refused, size_t room)
{
    if (s_channel.pop_words != 0u)
    {
        if (run_channel_airlock_passed(questions))
        {
            return 1;
        }
        VerbaLinea refusal_line = {refused, room, 0ull};
        mmgr_line_put(&refusal_line, "the airlock: the cross did not read this round, and nothing goes to the part");
        mmgr_line_finish(&refusal_line);
        return 0;
    }
    if (s_channel.gated_given || s_channel.dry)
    {
        return 1;
    }
    VerbaLinea refusal_line = {refused, room, 0ull};
    mmgr_line_put(&refusal_line,
                   "the airlock: the channel has none, its carrier no gated list, and nothing goes to the part");
    mmgr_line_finish(&refusal_line);
    return 0;
}

// the names of the blobs of the question at `at` of a round, or of a question carried alone where `alone` is 1, each
// into its own room of RUN_NAME_MAX_LEN
void run_channel_named(unsigned int at, int alone, char *code_name, char *cases_name, char *answers_name)
{
    static const char *const s_stems[3] = {"question", "cases", "answers"};
    static const char *const s_endings[3] = {".bin", ".txt", ".txt"};
    char *const names[3] = {code_name, cases_name, answers_name};
    for (unsigned int name = 0u; name < 3u; name += 1u)
    {
        VerbaLinea line = {names[name], RUN_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&line, s_stems[name]);
        if (!alone)
        {
            mmgr_line_decimal(&line, at, 0u);
        }
        mmgr_line_put(&line, s_endings[name]);
        mmgr_line_finish(&line);
    }
}

// The line of the question `question` on the channel's list, `<code> <registers> <cases> <answers> <probe> <launches>`,
// written into `line`, which has room for `room`: its length, or 0 where it did not fit
size_t run_channel_list_line_written(char *line, size_t room, const char *code_name, const char *cases_name,
                                     const char *answers_name, const RunQuestion *question)
{
    VerbaLinea written = {line, room, 0ull};
    mmgr_line_put(&written, code_name);
    mmgr_line_ch(&written, ' ');
    mmgr_line_decimal(&written, question->registers, 0u);
    mmgr_line_ch(&written, ' ');
    mmgr_line_put(&written, cases_name);
    mmgr_line_ch(&written, ' ');
    mmgr_line_put(&written, answers_name);
    mmgr_line_ch(&written, ' ');
    mmgr_line_decimal(&written, question->place, 0u);
    mmgr_line_ch(&written, ' ');
    mmgr_line_decimal(&written, question->launches, 0u);
    mmgr_line_ch(&written, '\n');
    return (size_t)mmgr_line_finish(&written);
}

// The questions of `questions` from `first` to `count` put on the channel's list again, a line each, their blobs as
// they were passed and named as run_channel_named names them: 1, or 0 where there was no memory for the list
int run_channel_relisted(RunQuestion *const *questions, unsigned int first, unsigned int count, int alone)
{
    s_channel.list_size = 0u;
    int listed = 1;
    for (unsigned int at = first; listed && (at < count); at += 1u)
    {
        char code_name[RUN_NAME_MAX_LEN];
        char cases_name[RUN_NAME_MAX_LEN];
        char answers_name[RUN_NAME_MAX_LEN];
        char line[RUN_LINE_MAX_LEN];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        const size_t line_length =
            run_channel_list_line_written(line, sizeof(line), code_name, cases_name, answers_name, questions[at]);
        listed = (line_length != 0u) && run_channel_listed(line, line_length);
    }
    return listed;
}

// The teacher's trace of the check before the part (qry_member.h, the record `teacher`): `message` of the
// question `question`, a line passed to the run's trace after its identity, as run_channel_traced passes one, so that a
// branch the learner took is traced back to the check that put its question
void run_channel_check_traced(const RunQuestion *question, const char *message)
{
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if (s_channel.buffer == NULL)
    {
        return;
    }
    KqrIdentity numbered;
    char identity[96];
    char line[1024];
    run_channel_identity(question, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    VerbaLinea written = {line, sizeof(line), 0ull};
    mmgr_line_put(&written, "teacher: ");
    mmgr_line_put(&written, identity);
    mmgr_line_put(&written, " checked: ");
    mmgr_line_put_max(&written, message, sizeof(line) - 128u);
    mmgr_line_ch(&written, '\n');
    if (mmgr_line_ok(&written))
    {
        qry_member_append("trace", line, written.at, priority);
    }
}

// The refusals the airlock's cross passed for the round it last read, each question of `questions` from `first` to
// `count` read off them by its answers' stem: every refusal of a question traced, and a question of no shape of its
// own, whose blocks the carrier sizes, refused only for registers past the count its kernel allots given the count that
// clears every refusal of it, where that is past its own, and that traced. The count of questions given a count
unsigned int run_channel_raised(RunQuestion *const *questions, unsigned int first, unsigned int count, int alone)
{
    const unsigned char *gated_list = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_member_blob(RUN_AIRLOCK_GATED, &gated_list, &size, &sequence) || (sequence < s_channel.airlock_began))
    {
        return 0u;
    }
    unsigned int raised = 0u;
    for (unsigned int at = first; at < count; at += 1u)
    {
        char code_name[RUN_NAME_MAX_LEN];
        char cases_name[RUN_NAME_MAX_LEN];
        char answers_name[RUN_NAME_MAX_LEN];
        char stem[RUN_NAME_MAX_LEN + sizeof(RUN_AIRLOCK_ANSWERS)];
        run_channel_named(at, alone, code_name, cases_name, answers_name);
        VerbaLinea stem_line = {stem, sizeof(stem), 0ull};
        mmgr_line_put(&stem_line, RUN_AIRLOCK_ANSWERS);
        mmgr_line_put_max(&stem_line, answers_name, strcspn(answers_name, "."));
        const size_t stem_length = (size_t)mmgr_line_finish(&stem_line);
        unsigned int clearing = 0u;
        int gated = 0;
        int cleared = 1;
        for (unsigned long long walk = 0ull; walk < size;)
        {
            const char *const line = (const char *)(gated_list + walk);
            const char *const end = (const char *)memchr(line, '\n', (size_t)(size - walk));
            const size_t length = (end != NULL) ? (size_t)(end - line) : (size_t)(size - walk);
            walk += (unsigned long long)length + 1ull;
            if ((length <= stem_length) || (memcmp(line, stem, stem_length) != 0) || (line[stem_length] != ' '))
            {
                continue;
            }
            // a refusal's line: the stem, the count that clears it, and why
            char text[RUN_LINE_MAX_LEN + 512u];
            VerbaLinea text_line = {text, sizeof(text), 0ull};
            mmgr_line_put_max(&text_line, line + stem_length + 1u, length - stem_length - 1u);
            mmgr_line_finish(&text_line);
            char *after = NULL;
            const unsigned long long count_read = strtoull(text, &after, 10);
            const char *const why = after + strspn(after, " ");
            char message[RUN_LINE_MAX_LEN + 640u];
            VerbaLinea message_line = {message, sizeof(message), 0ull};
            mmgr_line_put(&message_line, "gated, ");
            mmgr_line_put(&message_line, why);
            mmgr_line_finish(&message_line);
            run_channel_check_traced(questions[at], message);
            gated = 1;
            // a count of registers is at max what a kernel allots a thread, which an unsigned int represents
            cleared = cleared && (count_read != 0ull) && (count_read <= 0xffffffffull);
            clearing = (cleared && ((unsigned int)count_read > clearing)) ? (unsigned int)count_read : clearing;
        }
        const int shaped = (questions[at]->threads != 0u) || (questions[at]->blocks != 0u);
        if (!gated)
        {
            continue;
        }
        if (!cleared)
        {
            run_channel_check_traced(questions[at], "still refused: no count of registers clears it");
            continue;
        }
        if (s_registers_past == RUN_REGISTERS_UNANSWERED)
        {
            run_channel_check_traced(questions[at],
                                     "still refused: the part has not answered how far a container's code reaches");
            continue;
        }
        // the cross clears a refusal with one past the register it read, and a container's code reaches no further than
        // the part's count past short of what it declares
        const unsigned long long reaching = (unsigned long long)clearing - 1ull + (unsigned long long)s_registers_past;
        if (reaching > RUN_REGISTERS_MAX)
        {
            run_channel_check_traced(questions[at], "still refused: no count of registers a kernel allots reaches it");
            continue;
        }
        // a count at max RUN_REGISTERS_MAX fits an unsigned int
        clearing = (unsigned int)reaching;
        if (shaped)
        {
            run_channel_check_traced(
                questions[at], "still refused: a question of a shape of its own is not put again with more registers");
            continue;
        }
        if (clearing <= questions[at]->registers)
        {
            run_channel_check_traced(questions[at], "still refused: the count that clears it is no more than its own");
            continue;
        }
        char message[160];
        VerbaLinea message_line = {message, sizeof(message), 0ull};
        mmgr_line_put(&message_line, "put again with ");
        mmgr_line_decimal(&message_line, clearing, 0u);
        mmgr_line_put(&message_line, " registers in place of ");
        mmgr_line_decimal(&message_line, questions[at]->registers, 0u);
        mmgr_line_put(&message_line, ", every refusal of it a register past its count");
        mmgr_line_finish(&message_line);
        run_channel_check_traced(questions[at], message);
        questions[at]->registers = clearing;
        raised += 1u;
    }
    return raised;
}
