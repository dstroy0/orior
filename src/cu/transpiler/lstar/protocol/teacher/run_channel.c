// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_channel.c: each question handed to the run's buffer as blobs of its .qry (qry_buffer.h) and carried by the
// carrier in a process of its own, or many in one, and the blob the carrier handed for each read back as that
// question's answer. The round a carrier is about to carry is handed critical, on disk before the carrier starts, and
// a round whose record cannot be put on disk is never carried
#include "run_channel.h"

#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../../interface/interface.h"
#include "../record_R/record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most words that start a carrier, and the longest of them
#define RUN_CARRIER_WORDS 8u
#define RUN_PATH_LONGEST 1024u
// the longest name of a blob the channel hands
#define RUN_NAME_LONGEST 32u
#define RUN_ANSWER_LONGEST (16u + (RUN_CASES_MOST * 17u))
// the longest line of a round's list: three names, three counts and their spaces
#define RUN_LINE_LONGEST ((3u * RUN_NAME_LONGEST) + 64u)
// the most of a carrier's output kept in its blob, and the line before it that says how the carrier ended
#define RUN_CARRIER_OUTPUT (1u << 20u)
#define RUN_CARRIER_ENDING 192u
// a question's cases as the carrier reads them: a case a line, its eight words in hex, each at most eight digits
#define RUN_CASES_TEXT (RUN_CASES_MOST * ((RUN_IN_WORDS * 9u) + 1u))

typedef struct
{
    char word[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int words;
    unsigned long long limit_microseconds;
    int open;
    // 1 where the carrier's only word is `dry`, and the count of questions written so far
    int dry;
    unsigned int dry_written;
    // the run's buffer every blob of the channel is handed to
    QryBuffer *buffer;
    // the list of a round: the dry run's every question, handed whole once the channel closes, or a carried round's,
    // handed before its carrier starts
    char *list;
    size_t list_size;
    size_t list_room;
} RunChannel;

static RunChannel s_channel;
static char s_answer_line[RUN_ANSWER_LONGEST];
static char s_carrier_output[RUN_CARRIER_OUTPUT];
static char s_carrier_said[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
static char s_cases_text[RUN_CASES_TEXT];

// the panic the teacher has handed its account of: it speaks once for each
static unsigned long long s_panic_answered = 0ull;

// The sequence of the panic on the line, or 0 while it is quiet: once a panic opens it the run is halted, and the
// teacher puts no question after it (qry_buffer.h)
static unsigned long long run_channel_on_line(void)
{
    return qry_panicked(s_channel.buffer);
}

// the questions of `asked` from `first` to `count` answered as held by the line: no question is put after a panic
static void run_channel_halted(RunQuestion *const *asked, unsigned int first, unsigned int count)
{
    for (unsigned int at = first; at < count; at += 1u)
    {
        asked[at]->outcome = RUN_NO_CHANNEL;
        snprintf(asked[at]->refused, sizeof(asked[at]->refused),
                 "the run is halted: a panic is on the line at sequence %llu", run_channel_on_line());
    }
}

// The teacher's account on the line, handed as a panic of the call once for each panic: the round it handed the
// carrier, its count of questions and its sequence, how the carrier ended, how many it answered before it, the line of
// the question it was carrying, and that no question is put after this
static void run_channel_teacher_said(unsigned int questions, unsigned long long round, const InterfaceAnswer *answer,
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
                 "question is put after this\n",
                 questions, round, interface_ending_name(answer->ending), answer->code, answered, carrying,
                 ((carrying[0] != '\0') && (carrying[strlen(carrying) - 1u] == '\n')) ? "" : "\n");
    // the account fits its room, and its length is not negative
    qry_hand(s_channel.buffer, QRY_PANIC_CALL, said, (unsigned long long)((length > 0) ? length : 0), QRY_PANIC);
}

int run_channel_open(const char *const *carrier, unsigned long long limit_microseconds)
{
    free(s_channel.list);
    memset(&s_channel, 0, sizeof(s_channel));
    unsigned int words = 0u;
    while ((carrier[words] != NULL) && (words < RUN_CARRIER_WORDS))
    {
        snprintf(s_channel.word[words], RUN_PATH_LONGEST, "%s", carrier[words]);
        words += 1u;
    }
    if ((words == 0u) || (carrier[words] != NULL))
    {
        printf("  run_channel: a carrier is from 1 to %u words\n", RUN_CARRIER_WORDS);
        return 0;
    }
    s_channel.buffer = qry_run();
    if (s_channel.buffer == NULL)
    {
        printf("  run_channel: no query writer runs: %s names no .qry a writer holds open (qry_buffer.h)\n",
               QRY_ENVIRONMENT);
        return 0;
    }
    s_channel.words = words;
    s_channel.limit_microseconds = limit_microseconds;
    s_channel.dry = (words == 1u) && (strcmp(s_channel.word[0], "dry") == 0);
    s_channel.open = 1;
    return 1;
}

// `length` bytes of a line added to the end of the channel's list: 1, or 0 where there is no memory for them
static int run_channel_listed(const char *line, size_t length)
{
    if ((s_channel.list_size + length) > s_channel.list_room)
    {
        size_t grown = (s_channel.list_room == 0u) ? 65536u : s_channel.list_room;
        while (grown < (s_channel.list_size + length))
        {
            grown *= 2u;
        }
        char *const larger = (char *)realloc(s_channel.list, grown);
        if (larger == NULL)
        {
            return 0;
        }
        s_channel.list = larger;
        s_channel.list_room = grown;
    }
    memcpy(s_channel.list + s_channel.list_size, line, length);
    s_channel.list_size += length;
    return 1;
}

// the question's code and cases handed as the blobs `code_name` and `cases_name`, the cases a line each as the
// carrier reads them: 1, or 0 where either was dumped
static int run_channel_write(const RunQuestion *asked, const char *code_name, const char *cases_name)
{
    if (qry_hand(s_channel.buffer, code_name, asked->code, asked->code_size, QRY_ORDINARY) == 0ull)
    {
        return 0;
    }
    size_t length = 0u;
    for (unsigned int place = 0u; place < asked->cases; place += 1u)
    {
        for (unsigned int at = 0u; at < RUN_IN_WORDS; at += 1u)
        {
            // the text is sized for every case at its longest: each word fits and the count is not negative
            length += (size_t)snprintf(s_cases_text + length, sizeof(s_cases_text) - length, (at == 0u) ? "%x" : " %x",
                                       asked->word[place][at]);
        }
        s_cases_text[length] = '\n';
        length += 1u;
    }
    return qry_hand(s_channel.buffer, cases_name, s_cases_text, length, QRY_ORDINARY) != 0ull;
}

// The blob the carrier handed for `answers_name` after the round handed as `after`, read into `asked`: the outcome
// its first line names, each case's answer where it answered, and where the question is timed, the time the second
// gives, its launches together and the least and the most one took. A blob of the name from before the round is an
// older question's, and is none. A timed question the carrier gives no time for answered nothing worth reading
static void run_channel_read(RunQuestion *asked, const char *answers_name, unsigned long long after)
{
    asked->outcome = RUN_NOTHING;
    asked->nanoseconds = 0ull;
    asked->least = 0ull;
    asked->most = 0ull;
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_latest(s_channel.buffer, answers_name, &bytes, &size, &sequence) || (sequence <= after) || (size == 0ull))
    {
        snprintf(asked->refused, sizeof(asked->refused), "the carrier wrote no answer");
        return;
    }
    // the first line, and the second where there is one
    const unsigned char *const first_end = (const unsigned char *)memchr(bytes, '\n', (size_t)size);
    const size_t first_length = (first_end != NULL) ? (size_t)(first_end - bytes) : (size_t)size;
    const size_t kept = (first_length < (sizeof(s_answer_line) - 1u)) ? first_length : (sizeof(s_answer_line) - 1u);
    memcpy(s_answer_line, bytes, kept);
    s_answer_line[kept] = '\0';
    char timed_line[64] = "";
    if (first_end != NULL)
    {
        const unsigned char *const second = first_end + 1;
        const size_t left = (size_t)size - (size_t)(second - bytes);
        const unsigned char *const second_end = (const unsigned char *)memchr(second, '\n', left);
        size_t second_length = (second_end != NULL) ? (size_t)(second_end - second) : left;
        second_length = (second_length < (sizeof(timed_line) - 1u)) ? second_length : (sizeof(timed_line) - 1u);
        memcpy(timed_line, second, second_length);
        timed_line[second_length] = '\0';
    }
    unsigned int launches = 0u;
    const int timed = (sscanf(timed_line, "timed %u %llu %llu %llu", &launches, &asked->nanoseconds, &asked->least,
                              &asked->most) >= 2) &&
                      (launches == asked->launches);
    s_answer_line[strcspn(s_answer_line, "\r\n")] = '\0';
    if (strncmp(s_answer_line, "answered", 8u) != 0)
    {
        asked->outcome = (strncmp(s_answer_line, "skipped", 7u) == 0) ? (unsigned int)RUN_HELD : (unsigned int)RUN_ILLEGAL;
        snprintf(asked->refused, sizeof(asked->refused), "%.120s", s_answer_line);
        return;
    }
    const char *at = s_answer_line + 8u;
    for (unsigned int place = 0u; place < asked->cases; place += 1u)
    {
        char *after_word = NULL;
        asked->answered[place] = strtoull(at, &after_word, 16);
        if (after_word == at)
        {
            snprintf(asked->refused, sizeof(asked->refused), "the carrier answered %u of %u cases", place,
                     asked->cases);
            return;
        }
        at = after_word;
    }
    if ((asked->launches != 0u) && !timed)
    {
        snprintf(asked->refused, sizeof(asked->refused), "the carrier gave no time for %u launches", asked->launches);
        return;
    }
    asked->outcome = RUN_ANSWERED;
}

// `asked` handed as the next question of a dry run, its line added to the run's list, and read as held: nothing
// carries it and nothing answers it
static void run_channel_dry_written(RunQuestion *asked)
{
    char code_name[RUN_NAME_LONGEST];
    char cases_name[RUN_NAME_LONGEST];
    char answers_name[RUN_NAME_LONGEST];
    char line[RUN_LINE_LONGEST];
    s_channel.dry_written += 1u;
    snprintf(code_name, sizeof(code_name), "question%u.bin", s_channel.dry_written);
    snprintf(cases_name, sizeof(cases_name), "cases%u.txt", s_channel.dry_written);
    snprintf(answers_name, sizeof(answers_name), "answers%u.txt", s_channel.dry_written);
    const int line_length = snprintf(line, sizeof(line), "%s %u %s %s %u %u\n", code_name, asked->registers, cases_name,
                                     answers_name, asked->slot, asked->launches);
    // a line of three short names and three counts fits the line, and its length is not negative
    const int written = run_channel_write(asked, code_name, cases_name) && (line_length > 0) &&
                        run_channel_listed(line, (size_t)line_length);
    asked->outcome = written ? (unsigned int)RUN_HELD : (unsigned int)RUN_NO_CHANNEL;
    snprintf(asked->refused, sizeof(asked->refused), "%s",
             written ? "dry: written, carried by nothing" : "dry: the question could not be written");
}

// a question's identity as R keeps it: the hash of its code, its registers, its shape and launches, its count of
// cases and the hash of its cases, into `identity`, which holds 96
static void run_channel_identity(const RunQuestion *question, char *identity)
{
    unsigned long long code = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < question->code_size; at += 1ull)
    {
        code = (code ^ question->code[at]) * 0x100000001b3ull;
    }
    unsigned long long cases = 0xcbf29ce484222325ull;
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        cases = (cases ^ words[at]) * 0x100000001b3ull;
    }
    snprintf(identity, 96u, "%016llx %u %u %u %u %u %016llx", code, question->registers, question->threads,
             question->blocks, question->launches, question->cases, cases);
}

// 1 where R holds an answer to the untimed question `asked`, which is then answered from it: answers with the
// word of every case, and illegal or censored as held off the part, as R keeps them
static int run_channel_recorded(RunQuestion *asked)
{
    static RecordAsk s_held;
    char identity[96];
    if ((asked->launches != 0u) || !record_held())
    {
        return 0;
    }
    run_channel_identity(asked, identity);
    if (!record_answer(identity, &s_held))
    {
        return 0;
    }
    const int answers = (strcmp(s_held.answer, "answers") == 0);
    const int held_off = (strcmp(s_held.answer, "illegal") == 0) || (strcmp(s_held.answer, "censored") == 0);
    asked->outcome = answers ? (unsigned int)RUN_ANSWERED : held_off ? (unsigned int)RUN_HELD : (unsigned int)RUN_NOTHING;
    snprintf(asked->refused, sizeof(asked->refused), "%s", s_held.refusal);
    for (unsigned int place = 0u; answers && (place < asked->cases) && (place < s_held.words); place += 1u)
    {
        asked->answered[place] = s_held.word[place];
    }
    return 1;
}

// What came back of the carried question `asked` kept in R: an untimed one as an ask and a timed one as a sample,
// answers, illegal, nothing, timed_out where its time ran out, or censored where the gate held it. A question nothing
// carried is kept nowhere
static void run_channel_kept(const RunQuestion *asked)
{
    static RecordAsk s_kept;
    const char *answer = NULL;
    switch (asked->outcome)
    {
    case RUN_ANSWERED:
        answer = "answers";
        break;
    case RUN_ILLEGAL:
        answer = "illegal";
        break;
    case RUN_HELD:
        answer = "censored";
        break;
    case RUN_NOTHING:
        answer = (strstr(asked->refused, interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME)) != NULL) ? "timed_out"
                                                                                                       : "nothing";
        break;
    default:
        return;
    }
    if (!record_held())
    {
        return;
    }
    char identity[96];
    run_channel_identity(asked, identity);
    if (asked->launches != 0u)
    {
        record_sample(identity, answer, asked->nanoseconds, asked->least, asked->most, asked->refused);
        return;
    }
    memset(&s_kept, 0, sizeof(s_kept));
    snprintf(s_kept.answer, sizeof(s_kept.answer), "%s", answer);
    snprintf(s_kept.refusal, sizeof(s_kept.refusal), "%s", asked->refused);
    for (unsigned int place = 0u; (asked->outcome == RUN_ANSWERED) && (place < asked->cases) && (place < RECORD_WORDS);
         place += 1u)
    {
        s_kept.word[place] = asked->answered[place];
        s_kept.words += 1u;
    }
    record_keep(identity, &s_kept);
}

int run_channel_record(const char *path, const char *member, const char *mode)
{
    return record_open(path, member, mode);
}

// the line of the channel's list for its `place`th question, into `line`, which holds RUN_LINE_LONGEST, its end kept:
// empty where the list holds no such line
static void run_channel_list_line(unsigned int place, char *line)
{
    size_t at = 0u;
    for (unsigned int seen = 0u; (seen < place) && (at < s_channel.list_size); seen += 1u)
    {
        const char *const end = (const char *)memchr(s_channel.list + at, '\n', s_channel.list_size - at);
        at = (end != NULL) ? ((size_t)(end - s_channel.list) + 1u) : s_channel.list_size;
    }
    const char *const end =
        (at < s_channel.list_size) ? (const char *)memchr(s_channel.list + at, '\n', s_channel.list_size - at) : NULL;
    size_t length = (at >= s_channel.list_size) ? 0u
                    : (end != NULL)             ? ((size_t)(end - (s_channel.list + at)) + 1u)
                                                : (s_channel.list_size - at);
    length = (length < (RUN_LINE_LONGEST - 1u)) ? length : (RUN_LINE_LONGEST - 1u);
    if (length != 0u)
    {
        memcpy(line, s_channel.list + at, length);
    }
    line[length] = '\0';
}

// The round about to be carried, the channel's list, handed critical as questions.txt: on disk before the carrier
// starts; a launch that faults the part leaves the record of what the carrier was handed. Its sequence, or 0 where
// it could not be put on disk, and then the round is not carried
static unsigned long long run_channel_round_handed(void)
{
    return qry_hand(s_channel.buffer, "questions.txt", s_channel.list, s_channel.list_size, QRY_CRITICAL);
}

// How the carrier ended and what it wrote, handed as carrier.txt: a line that names its ending, then its output. A
// panic where the carrier did not end clean, the carrier losing its mind: its ending is the record of it
static void run_channel_output_handed(const InterfaceAnswer *answer)
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

static int run_channel_carried(RunQuestion *asked);

int run_channel_ask(RunQuestion *asked)
{
    asked->refused[0] = '\0';
    if (run_channel_recorded(asked))
    {
        return asked->outcome == RUN_ANSWERED;
    }
    const int answered = run_channel_carried(asked);
    if (!s_channel.dry)
    {
        run_channel_kept(asked);
    }
    return answered;
}

// `asked` carried as run_channel_ask carries it, R aside
static int run_channel_carried(RunQuestion *asked)
{
    if (s_channel.open && s_channel.dry)
    {
        run_channel_dry_written(asked);
        return 0;
    }
    if (!s_channel.open)
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the channel is not open");
        return 0;
    }
    if (run_channel_on_line() != 0ull)
    {
        RunQuestion *const one[1] = {asked};
        run_channel_halted(one, 0u, 1u);
        return 0;
    }
    char code_name[] = "question.bin";
    char cases_name[] = "cases.txt";
    char answers_name[] = "answers.txt";
    char registers[16];
    char slot[16];
    char launches[16];
    char threads[16];
    char blocks[16];
    char line[RUN_LINE_LONGEST];
    snprintf(registers, sizeof(registers), "%u", asked->registers);
    snprintf(slot, sizeof(slot), "%u", asked->slot);
    snprintf(launches, sizeof(launches), "%u", asked->launches);
    snprintf(threads, sizeof(threads), "%u", asked->threads);
    snprintf(blocks, sizeof(blocks), "%u", asked->blocks);
    if (!run_channel_write(asked, code_name, cases_name))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the question could not be handed to the run's buffer");
        return 0;
    }
    // the round, this one question, its line handed critical before the carrier starts
    s_channel.list_size = 0u;
    const int line_length = snprintf(line, sizeof(line), "%s %u %s %s %u %u\n", code_name, asked->registers, cases_name,
                                     answers_name, asked->slot, asked->launches);
    // a line of three short names and three counts fits the line, and its length is not negative
    const unsigned long long after =
        ((line_length > 0) && run_channel_listed(line, (size_t)line_length)) ? run_channel_round_handed() : 0ull;
    if (after == 0ull)
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the round could not be put on disk, and it is not carried");
        return 0;
    }
    // the interface's command is a list of words it does not write to; the cast only meets its declared type
    char *command[RUN_CARRIER_WORDS + 8u];
    unsigned int words = 0u;
    for (; words < s_channel.words; words += 1u)
    {
        command[words] = s_channel.word[words];
    }
    command[words] = code_name;
    command[words + 1u] = registers;
    command[words + 2u] = cases_name;
    command[words + 3u] = answers_name;
    command[words + 4u] = slot;
    words += 5u;
    // an untimed question of no shape of its own is carried with the words it always was; a timed one with its count
    // of launches after, and one with a shape with its count of launches, 0 where it is untimed, and its shape
    const int shaped = (asked->threads != 0u) || (asked->blocks != 0u);
    if ((asked->launches != 0u) || shaped)
    {
        command[words] = launches;
        words += 1u;
    }
    if (shaped)
    {
        command[words] = threads;
        command[words + 1u] = blocks;
        words += 2u;
    }
    command[words] = NULL;
    // the carrier's output comes back through a pipe, and no file is made
    const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds};
    InterfaceAnswer answer = {0};
    answer.output = s_carrier_output;
    answer.output_capacity = sizeof(s_carrier_output);
    EngineError error = {0};
    const long ran = interface_probe_run(&probe, &answer, &error);
    run_channel_output_handed(&answer);
    // a panic on the line, the carrier's or its ending's: the teacher's account handed
    run_channel_teacher_said(1u, after, &answer, 0u, line);
    if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
    {
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %.96s was not started", s_channel.word[0]);
        return 0;
    }
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        // a carrier that did not end clean wrote no answer worth reading: its ending is the answer
        asked->outcome = (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL
                                                                     : (unsigned int)RUN_NOTHING;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %s, code %llx",
                 interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    run_channel_read(asked, answers_name, after);
    return asked->outcome == RUN_ANSWERED;
}

static unsigned int run_channel_carried_many(RunQuestion *const *asked, unsigned int count);

unsigned int run_channel_ask_many(RunQuestion *const *asked, unsigned int count)
{
    // the questions R answers taken first, and only the rest carried, each kept in R once it comes back
    RunQuestion **const left = (RunQuestion **)malloc((size_t)(count + 1u) * sizeof(RunQuestion *));
    if (left == NULL)
    {
        return run_channel_carried_many(asked, count);
    }
    unsigned int recorded = 0u;
    unsigned int lefts = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        asked[at]->refused[0] = '\0';
        if (run_channel_recorded(asked[at]))
        {
            recorded += (asked[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
            continue;
        }
        left[lefts] = asked[at];
        lefts += 1u;
    }
    const unsigned int carried = (lefts != 0u) ? run_channel_carried_many(left, lefts) : 0u;
    for (unsigned int at = 0u; (at < lefts) && !s_channel.dry; at += 1u)
    {
        run_channel_kept(left[at]);
    }
    free(left);
    return recorded + carried;
}

// the questions of `asked` carried as run_channel_ask_many carries them, R aside
static unsigned int run_channel_carried_many(RunQuestion *const *asked, unsigned int count)
{
    unsigned int answered = 0u;
    if (s_channel.open && s_channel.dry)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            asked[at]->refused[0] = '\0';
            run_channel_dry_written(asked[at]);
        }
        return 0u;
    }
    if (!s_channel.open)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            asked[at]->outcome = RUN_NO_CHANNEL;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the channel is not open");
        }
        return 0u;
    }
    char list_name[] = "questions.txt";
    // each process carries the questions from `first` on, and the next carries those after the first it left
    // unanswered
    unsigned int first = 0u;
    while (first < count)
    {
        if (run_channel_on_line() != 0ull)
        {
            run_channel_halted(asked, first, count);
            return answered;
        }
        s_channel.list_size = 0u;
        int written = 1;
        for (unsigned int at = first; written && (at < count); at += 1u)
        {
            char code_name[RUN_NAME_LONGEST];
            char cases_name[RUN_NAME_LONGEST];
            char answers_name[RUN_NAME_LONGEST];
            char line[RUN_LINE_LONGEST];
            snprintf(code_name, sizeof(code_name), "question%u.bin", at);
            snprintf(cases_name, sizeof(cases_name), "cases%u.txt", at);
            snprintf(answers_name, sizeof(answers_name), "answers%u.txt", at);
            asked[at]->refused[0] = '\0';
            const int line_length = snprintf(line, sizeof(line), "%s %u %s %s %u %u\n", code_name, asked[at]->registers,
                                             cases_name, answers_name, asked[at]->slot, asked[at]->launches);
            // a line of three short names and three counts fits the line, and its length is not negative
            written = run_channel_write(asked[at], code_name, cases_name) && (line_length > 0) &&
                      run_channel_listed(line, (size_t)line_length);
        }
        // the round handed critical, on disk before its carrier starts
        const unsigned long long after = written ? run_channel_round_handed() : 0ull;
        if (after == 0ull)
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused),
                         "the round could not be put on disk, and it is not carried");
            }
            return answered;
        }
        // the interface's command is a list of words it does not write to; the cast only meets its declared type
        char *command[RUN_CARRIER_WORDS + 4u];
        unsigned int words = 0u;
        for (; words < s_channel.words; words += 1u)
        {
            command[words] = s_channel.word[words];
        }
        command[words] = (char *)"--list";
        command[words + 1u] = list_name;
        command[words + 2u] = NULL;
        // the process is given each question's limit for every question it carries, and its output comes back
        // through a pipe
        const InterfaceProbe probe = {command, NULL, s_channel.limit_microseconds * (count - first)};
        InterfaceAnswer answer = {0};
        answer.output = s_carrier_output;
        answer.output_capacity = sizeof(s_carrier_output);
        EngineError error = {0};
        const long ran = interface_probe_run(&probe, &answer, &error);
        run_channel_output_handed(&answer);
        if ((ran != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            for (unsigned int at = first; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %.96s was not started",
                         s_channel.word[0]);
            }
            return answered;
        }
        // every question the process answered, in order, up to the first it left unanswered: an answer is one the
        // carrier handed after the round
        unsigned int at = first;
        const unsigned int answered_before = answered;
        for (; at < count; at += 1u)
        {
            char answers_name[RUN_NAME_LONGEST];
            snprintf(answers_name, sizeof(answers_name), "answers%u.txt", at);
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ull;
            unsigned long long sequence = 0ull;
            if (!qry_latest(s_channel.buffer, answers_name, &bytes, &size, &sequence) || (sequence <= after))
            {
                break;
            }
            run_channel_read(asked[at], answers_name, after);
            answered += (asked[at]->outcome == RUN_ANSWERED) ? 1u : 0u;
        }
        // the first question left unanswered by a process that did not end clean ended it, and its ending is its
        // answer; one left by a process that ended clean was left for a refusal before it, and is
        // carried again
        const int clean = (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull);
        const unsigned int unanswered = at;
        if ((at < count) && !clean)
        {
            asked[at]->outcome =
                (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL : (unsigned int)RUN_NOTHING;
            snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier %s, code %llx",
                     interface_ending_name(answer.ending), answer.code);
            at += 1u;
        }
        // a panic on the line: the teacher's account handed, the question the carrier was carrying the one its ending
        // answered or, where it ended clean, the last it answered, its refusal; and no question is put after it
        if (run_channel_on_line() != 0ull)
        {
            const unsigned int carrying = (!clean || (unanswered == first)) ? unanswered : (unanswered - 1u);
            char line[RUN_LINE_LONGEST];
            run_channel_list_line(carrying - first, line);
            run_channel_teacher_said(count - first, after, &answer, answered - answered_before, line);
            run_channel_halted(asked, at, count);
            return answered;
        }
        if (at == first)
        {
            // a process that answered nothing and ended clean carried nothing, and the questions after are not carried
            // again
            for (; at < count; at += 1u)
            {
                asked[at]->outcome = RUN_NO_CHANNEL;
                snprintf(asked[at]->refused, sizeof(asked[at]->refused), "the carrier answered none of the questions");
            }
        }
        first = at;
    }
    return answered;
}

void run_channel_close(void)
{
    // the dry run's list, every question it wrote, handed whole
    if (s_channel.open && s_channel.dry && (s_channel.list_size != 0u) &&
        (qry_hand(s_channel.buffer, "questions.txt", s_channel.list, s_channel.list_size, QRY_ORDINARY) == 0ull))
    {
        printf("  run_channel: the dry run's list of %u questions could not be handed to the run's buffer\n",
               s_channel.dry_written);
    }
    record_close();
    free(s_channel.list);
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
