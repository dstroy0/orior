// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_channel.c: each question handed to the run's buffer as blobs of its .qry (qry_buffer.h) and carried by the
// carrier in a process of its own, or many in one, and the blob the carrier handed for each read back as that
// question's answer. The round a carrier is about to carry is handed critical, on disk before the carrier starts, and
// a round whose record cannot be put on disk is never carried
#include "run_channel.h"

#include "../../../../types/file_defs/kqr/kqr.h"
#include "../../../../types/file_defs/ksc/ksc.h"
#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../../interface/interface.h"
#include "../overseer/overseer.h"

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

// the round's list as the airlock reads it, each question's answers named after the prefix, and the holds its cross
// hands the teacher, a line a hold, `<answers stem> <registers> <why>` (measuring_stick_query.c)
#define RUN_AIRLOCK_LIST "airlock.txt"
#define RUN_AIRLOCK_ANSWERS "airlock_"
#define RUN_AIRLOCK_HOLDS "airlock_holds.txt"
// the slots the cross read clear, a line `<stem> <operation>` each, for the conferences the gate calls
#define RUN_AIRLOCK_READ "airlock_read.txt"
// the gate's hold under rule 3 of the airlock, an operation key unknown, as the carrier answers it: `skipped` and
// the rule's number (cubin_safe.h, CUBIN_SAFE_KEY)
#define RUN_GATE_KEY_RULE "skipped 5 "

// 1 where `refused` is the gate's hold for an unknown operation key
static int run_channel_key_held(const char *refused)
{
    return strncmp(refused, RUN_GATE_KEY_RULE, sizeof(RUN_GATE_KEY_RULE) - 1u) == 0;
}

typedef struct
{
    char word[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int words;
    // the airlock: the words that start the carrier with the part taken out, which pops each question's container
    // (`--list <list>` put after them), and those that start the vendor's reader over the containers (the list put
    // after its first word); the held list the carrier reads, named by its `--held`, and 1 where the carrier is handed
    // one at all
    char pop[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int pop_words;
    char cross[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int cross_words;
    char held[RUN_PATH_LONGEST];
    int held_given;
    // the sequence of the list the airlock last read: what its cross hands comes after it
    unsigned long long airlock_began;
    unsigned long long limit_microseconds;
    int open;
    // 1 where the carrier's only word is `dry`, and the count of questions written so far
    int dry;
    unsigned int dry_written;
    // 1 where the carrier's own words hold --dry: it stops before the driver, every round through the airlock as a
    // live one, and nothing it answers is kept in R
    int stopped;
    // the run's buffer every blob of the channel is handed to
    QryBuffer *buffer;
    // the list of a round: the dry run's every question, handed whole once the channel closes, or a carried round's,
    // handed before its carrier starts
    char *list;
    size_t list_size;
    size_t list_room;
} RunChannel;

static RunChannel s_channel;

// how many registers past the highest its code names a container declares, as the part answered it, and the mark of
// no answer: no hold of a register past a kernel's count is then cleared
#define RUN_REGISTERS_UNANSWERED 0xffffffffu
static unsigned int s_registers_past = RUN_REGISTERS_UNANSWERED;

// the most registers a kernel allots a thread, R254 the highest a field names short of RZ
#define RUN_REGISTERS_MOST 255ull
static char s_answer_line[RUN_ANSWER_LONGEST];
static char s_carrier_output[RUN_CARRIER_OUTPUT];
static char s_carrier_said[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
static char s_cases_text[RUN_CASES_TEXT];

// the panic the teacher has handed its account of: it speaks once for each
static unsigned long long s_panic_answered = 0ull;

static void run_channel_identity(const RunQuestion *question, KqrIdentity *identity);

// The conference a question the part refused calls (QRY_CONFERENCE_CALL, overseer.h): the teacher's account of it, the
// question as it was put, how it came back and whether a panic is on the line, then every account the carrier said
// after the refusal in its answer, `said` of `said_size` bytes, as the carrier said it
static void run_channel_conference_called(const RunQuestion *asked, const char *how, const unsigned char *said,
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
    // the carrier's words, then the pop's after `--pop` and the cross's after `--cross`, each from 1 to
    // RUN_CARRIER_WORDS
    char (*part)[RUN_PATH_LONGEST] = s_channel.word;
    unsigned int *count = &s_channel.words;
    for (unsigned int at = 0u; carrier[at] != NULL; at += 1u)
    {
        if ((strcmp(carrier[at], "--pop") == 0) || (strcmp(carrier[at], "--cross") == 0))
        {
            const int pop = (strcmp(carrier[at], "--pop") == 0);
            part = pop ? s_channel.pop : s_channel.cross;
            count = pop ? &s_channel.pop_words : &s_channel.cross_words;
            continue;
        }
        if (*count >= RUN_CARRIER_WORDS)
        {
            printf("  run_channel: a carrier, its pop and its cross are each from 1 to %u words\n", RUN_CARRIER_WORDS);
            return 0;
        }
        snprintf(part[*count], RUN_PATH_LONGEST, "%s", carrier[at]);
        *count += 1u;
    }
    const unsigned int words = s_channel.words;
    if ((words == 0u) || ((s_channel.pop_words == 0u) != (s_channel.cross_words == 0u)))
    {
        printf("  run_channel: a carrier is from 1 to %u words, and an airlock both a pop and a cross\n",
               RUN_CARRIER_WORDS);
        return 0;
    }
    // the held list the carrier reads, named after its `--held`
    for (unsigned int at = 0u; (at + 1u) < words; at += 1u)
    {
        if (strcmp(s_channel.word[at], "--held") == 0)
        {
            snprintf(s_channel.held, sizeof(s_channel.held), "%s", s_channel.word[at + 1u]);
            s_channel.held_given = 1;
        }
    }
    if ((s_channel.pop_words != 0u) && !s_channel.held_given)
    {
        printf(
            "  run_channel: an airlock's carrier reads the cross's held list, and this one is handed none (--held)\n");
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
    s_channel.stopped = 0;
    for (unsigned int at = 1u; at < words; at += 1u)
    {
        s_channel.stopped = s_channel.stopped || (strcmp(s_channel.word[at], "--dry") == 0);
    }
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
        // a refusal calls a conference, the carrier's accounts after the refusal's line handed with the teacher's
        if (asked->outcome == RUN_ILLEGAL)
        {
            char how[160];
            snprintf(how, sizeof(how), "the part refused it, %.120s", s_answer_line);
            const unsigned long long said_at =
                (first_end != NULL) ? (unsigned long long)(first_end - bytes) + 1ull : size;
            run_channel_conference_called(asked, how, bytes + said_at, size - said_at);
        }
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
// cases and the hash of its cases, into `identity`
static void run_channel_identity(const RunQuestion *question, KqrIdentity *identity)
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
    identity->code = code;
    identity->registers = question->registers;
    identity->threads = question->threads;
    identity->blocks = question->blocks;
    identity->launches = question->launches;
    identity->cases = question->cases;
    identity->cases_hash = cases;
}

// R, the part's record (kqr.h), open where run_channel_record read it, and the path it is written back to, empty
// where it is not to be
static KqrRecord s_record;
static char s_record_path[1024];

// 1 where R holds an answer to the untimed question `asked`, which is then answered from it: answers with the
// word of every case, and illegal or censored as held off the part, as R keeps them. An ask that timed out is no
// answer
static int run_channel_recorded(RunQuestion *asked)
{
    static KqrAsk s_held;
    KqrIdentity identity;
    if ((asked->launches != 0u) || !s_record.open)
    {
        return 0;
    }
    run_channel_identity(asked, &identity);
    // an ask the gate held for an unknown key is open as a timed-out one is: the part never saw it, and a conference
    // may admit it
    if (!kqr_find(&s_record, &identity, &s_held) || (s_held.answer == KQR_TIMED_OUT) ||
        ((s_held.answer == KQR_CENSORED) && run_channel_key_held(s_held.refusal)))
    {
        return 0;
    }
    const int answers = (s_held.answer == KQR_ANSWERS);
    const int held_off = (s_held.answer == KQR_ILLEGAL) || (s_held.answer == KQR_CENSORED);
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
    static KqrAsk s_kept;
    static KqrSample s_sample;
    unsigned int answer = KQR_ANSWER_COUNT;
    switch (asked->outcome)
    {
    case RUN_ANSWERED:
        answer = KQR_ANSWERS;
        break;
    case RUN_ILLEGAL:
        answer = KQR_ILLEGAL;
        break;
    case RUN_HELD:
        answer = KQR_CENSORED;
        break;
    case RUN_NOTHING:
        answer = (strstr(asked->refused, interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME)) != NULL) ? KQR_TIMED_OUT
                                                                                                       : KQR_NOTHING;
        break;
    default:
        return;
    }
    if (!s_record.open)
    {
        return;
    }
    KqrIdentity identity;
    run_channel_identity(asked, &identity);
    if (asked->launches != 0u)
    {
        memset(&s_sample, 0, sizeof(s_sample));
        s_sample.identity = identity;
        s_sample.answer = answer;
        s_sample.nanoseconds = asked->nanoseconds;
        s_sample.launched = (asked->most != 0ull) ? 1 : 0;
        s_sample.least = asked->least;
        s_sample.most = asked->most;
        snprintf(s_sample.refusal, sizeof(s_sample.refusal), "%s", asked->refused);
        kqr_sample(&s_record, &s_sample);
        return;
    }
    memset(&s_kept, 0, sizeof(s_kept));
    s_kept.identity = identity;
    s_kept.answer = answer;
    snprintf(s_kept.refusal, sizeof(s_kept.refusal), "%s", asked->refused);
    for (unsigned int place = 0u; (asked->outcome == RUN_ANSWERED) && (place < asked->cases) && (place < KQR_WORDS);
         place += 1u)
    {
        s_kept.word[place] = asked->answered[place];
        s_kept.words += 1u;
    }
    kqr_keep(&s_record, &s_kept);
}

int run_channel_registers_past(const char *ksc)
{
    static KscFile s_file;
    static KscRow s_row;
    s_registers_past = RUN_REGISTERS_UNANSWERED;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (ksc_row_read(s_file.line[line], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strncmp(s_row.question, "register past", 13u) == 0) && (s_row.value != 0ull) &&
            (s_row.value < RUN_REGISTERS_UNANSWERED))
        {
            // a count below the mark of no answer is one an unsigned int holds
            s_registers_past = (unsigned int)s_row.value;
        }
    }
    ksc_free(&s_file);
    return s_registers_past != RUN_REGISTERS_UNANSWERED;
}

int run_channel_record(const char *path, const char *member, const char *mode)
{
    char error[1280];
    s_record_path[0] = '\0';
    if (!kqr_read(&s_record, path, member, error, sizeof(error)))
    {
        printf("  record: %s\n", error);
        return 0;
    }
    snprintf(s_record_path, sizeof(s_record_path), "%s", path);
    // R a view of the run's table of asks, written as its rows put it together; with no run it is written as held
    kqr_tabled(&s_record, qry_run(), path);
    kqr_mark(&s_record, KQR_CYCLE_RECORD, mode);
    return 1;
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

static char s_airlock_output[RUN_CARRIER_OUTPUT];

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
    qry_moved(s_channel.buffer);
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_settled(s_channel.buffer);
    const unsigned long long kept =
        (answer.output_bytes < sizeof(s_airlock_output)) ? answer.output_bytes : sizeof(s_airlock_output);
    // what the pop and the cross printed, the airlock's trace (qry_buffer.h, QRY_TRACE)
    if (qry_traced("airlock", QRY_ORDINARY))
    {
        qry_hand(s_channel.buffer, said, s_airlock_output, kept, QRY_ORDINARY);
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
    const size_t room = s_channel.list_size + ((size_t)questions * sizeof(RUN_AIRLOCK_ANSWERS)) + 1u;
    char *const renamed = (char *)malloc(room);
    if (renamed == NULL)
    {
        return 0;
    }
    size_t length = 0u;
    for (size_t at = 0u; at < s_channel.list_size;)
    {
        const char *const line = s_channel.list + at;
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
    const unsigned long long handed = qry_hand(s_channel.buffer, RUN_AIRLOCK_LIST, renamed, length, QRY_ORDINARY);
    free(renamed);
    // the list's sequence, once it is moved into the run: the held list and the holds the cross hands come after it
    const unsigned long long began = (handed != 0ull) ? qry_moved(s_channel.buffer) : 0ull;
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
    if (run_channel_airlock_ran(command, limit, "airlock_pop.txt") != 0ll)
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
    const long long crossed = run_channel_airlock_ran(command, limit, "airlock_cross.txt");
    if ((crossed != 0ll) && (crossed != 1ll))
    {
        return 0;
    }
    const unsigned char *held = NULL;
    unsigned long long held_size = 0ull;
    unsigned long long held_sequence = 0ull;
    return qry_latest(s_channel.buffer, s_channel.held, &held, &held_size, &held_sequence) && (held_sequence > began);
}

// 1 where the round on the channel's list, `questions` of them, may be carried: it passed the airlock, or the channel
// has none and its carrier is handed the held list of a cross that read the whole pass before it. 0 otherwise, `why`
// said into `refused`, which holds `room`: nothing the vendor has not read goes to the part
static int run_channel_airlocked(unsigned int questions, char *refused, size_t room)
{
    if (s_channel.pop_words != 0u)
    {
        if (run_channel_airlock_passed(questions))
        {
            return 1;
        }
        snprintf(refused, room, "the airlock: the cross did not read this round, and nothing goes to the part");
        return 0;
    }
    if (s_channel.held_given)
    {
        return 1;
    }
    snprintf(refused, room,
             "the airlock: the channel has none, its carrier no held list, and nothing goes to the part");
    return 0;
}

// the names of the blobs of the question at `at` of a round, or of a question carried alone where `alone` is 1, each
// into its own room of RUN_NAME_LONGEST
static void run_channel_named(unsigned int at, int alone, char *code_name, char *cases_name, char *answers_name)
{
    if (alone)
    {
        snprintf(code_name, RUN_NAME_LONGEST, "question.bin");
        snprintf(cases_name, RUN_NAME_LONGEST, "cases.txt");
        snprintf(answers_name, RUN_NAME_LONGEST, "answers.txt");
        return;
    }
    snprintf(code_name, RUN_NAME_LONGEST, "question%u.bin", at);
    snprintf(cases_name, RUN_NAME_LONGEST, "cases%u.txt", at);
    snprintf(answers_name, RUN_NAME_LONGEST, "answers%u.txt", at);
}

// The questions of `asked` from `first` to `count` put on the channel's list again, a line each, their blobs as they
// were handed and named as run_channel_named names them: 1, or 0 where there was no memory for the list
static int run_channel_relisted(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone)
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
        const int line_length = snprintf(line, sizeof(line), "%s %u %s %s %u %u\n", code_name, asked[at]->registers,
                                         cases_name, answers_name, asked[at]->slot, asked[at]->launches);
        // a line of three short names and three counts fits the line, and its length is not negative
        listed = (line_length > 0) && run_channel_listed(line, (size_t)line_length);
    }
    return listed;
}

// The teacher's trace of the check before the part (qry_buffer.h, QRY_TRACE, the path `teacher`): `said` of the
// question `asked`, a line handed to the run's trace after its identity, as run_channel_traced hands one, so that a
// branch the learner took is traced back to the check that put its question
static void run_channel_check_traced(const RunQuestion *asked, const char *said)
{
    const unsigned int priority = (run_channel_on_line() != 0ull) ? QRY_PANIC : QRY_ORDINARY;
    if ((s_channel.buffer == NULL) || !qry_traced("teacher", priority))
    {
        return;
    }
    KqrIdentity numbered;
    char identity[96];
    char line[1024];
    run_channel_identity(asked, &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    const int length = snprintf(line, sizeof(line), "teacher: %s checked: %s\n", identity, said);
    // the line fits its room, and its length is not negative
    const size_t kept = (length < 0) ? 0u : (((size_t)length < sizeof(line)) ? (size_t)length : (sizeof(line) - 1u));
    qry_hand(s_channel.buffer, "trace", line, (unsigned long long)kept, QRY_ORDINARY);
}

// The holds the airlock's cross handed for the round it last read, each question of `asked` from `first` to `count`
// read off them by its answers' stem: every hold of a question traced, and a question of no shape of its own, whose
// blocks the carrier sizes, held only for registers past the count its kernel allots given the count that clears
// every hold of it, where that is past its own, and that traced. The count of questions given a count
static unsigned int run_channel_raised(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone)
{
    const unsigned char *holds = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_latest(s_channel.buffer, RUN_AIRLOCK_HOLDS, &holds, &size, &sequence) ||
        (sequence <= s_channel.airlock_began))
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
        snprintf(stem, sizeof(stem), "%s%.*s", RUN_AIRLOCK_ANSWERS, (int)strcspn(answers_name, "."), answers_name);
        const size_t stem_length = strlen(stem);
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
            snprintf(text, sizeof(text), "%.*s", (int)(length - stem_length - 1u), line + stem_length + 1u);
            char *after = NULL;
            const unsigned long long count_read = strtoull(text, &after, 10);
            const char *const why = after + strspn(after, " ");
            char said[RUN_LINE_LONGEST + 640u];
            snprintf(said, sizeof(said), "held, %s", why);
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
            run_channel_check_traced(asked[at], "kept held: the part has not answered how far a container's code reaches");
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
        snprintf(said, sizeof(said),
                 "put again with %u registers in place of %u, every hold of it a register past its count", clearing,
                 asked[at]->registers);
        run_channel_check_traced(asked[at], said);
        asked[at]->registers = clearing;
        raised += 1u;
    }
    return raised;
}

// The conferences the gate calls for the round the airlock last read: each question of `asked` from `first` to
// `count` whose answer the pop gave is a hold under the key rule (RUN_GATE_KEY_RULE), read against the slots the cross
// read clear (RUN_AIRLOCK_READ) by its answers' stem. Where the cross read its slot clear, the accounts are handed,
// the cross's, the gate's and the record's, and the conference held (overseer_admission): a question admitted has its
// slot encoding a lesson the carrier loads (--lessons), and is put as the round is carried; every other stays held.
// Each is traced. The count admitted
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
        // the cross's reading of its slot, the line of the read list that leads with its stem
        char operation[64] = "";
        const size_t stem_length = strlen(stem);
        for (unsigned long long walk = 0ull; (operation[0] == '\0') && (walk < size);)
        {
            const char *const line = (const char *)(read + walk);
            const char *const end = (const char *)memchr(line, '\n', (size_t)(size - walk));
            const size_t length = (end != NULL) ? (size_t)(end - line) : (size_t)(size - walk);
            walk += (unsigned long long)length + 1ull;
            if ((length > stem_length) && (memcmp(line, stem, stem_length) == 0) && (line[stem_length] == ' '))
            {
                snprintf(operation, sizeof(operation), "%.*s", (int)(length - stem_length - 1u),
                         line + stem_length + 1u);
            }
        }
        if (operation[0] == '\0')
        {
            run_channel_check_traced(asked[at],
                                     "kept held: the gate holds its key unknown, and the cross did not read its slot clear");
            continue;
        }
        if ((asked[at]->slot == RUN_SLOT_NONE) ||
            ((((unsigned long long)asked[at]->slot * 16ull) + 16ull) > asked[at]->code_size))
        {
            run_channel_check_traced(asked[at], "kept held: the gate holds its key unknown, and it holds no slot to admit");
            continue;
        }
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        memcpy(&low, asked[at]->code + ((size_t)asked[at]->slot * 16u), 8u);
        memcpy(&high, asked[at]->code + ((size_t)asked[at]->slot * 16u) + 8u, 8u);
        char encoding[48];
        snprintf(encoding, sizeof(encoding), "%016llx %016llx", low, high);
        KqrIdentity numbered;
        char identity[96];
        run_channel_identity(asked[at], &numbered);
        kqr_identity_text(&numbered, identity, sizeof(identity));
        // the accounts: the cross's reading, the gate's hold, and what the record holds of the identity
        char account[512];
        snprintf(account, sizeof(account),
                 "reads %s at the slot, a legal instruction, no control transfer, wait, memory access or register "
                 "past the count; admit",
                 operation);
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
        OverseerRuling ruling;
        char said[RUN_LINE_LONGEST + 640u];
        if (overseer_admission(s_channel.buffer, identity, encoding, &ruling))
        {
            admitted += 1u;
            snprintf(said, sizeof(said), "admitted by its conference: %s", ruling.said);
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
static int run_channel_checked(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone,
                               char *refused, size_t room)
{
    for (;;)
    {
        if (!run_channel_airlocked(count - first, refused, room))
        {
            return 0;
        }
        // the conferences the gate calls, each question it holds for an unknown key the cross read clear admitted
        // or kept held, before the round's registers are raised
        if (s_channel.pop_words != 0u)
        {
            run_channel_admitted(asked, first, count, alone);
        }
        if ((s_channel.pop_words == 0u) || (run_channel_raised(asked, first, count, alone) == 0u))
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

// The teacher's trace, the path `teacher` (qry_buffer.h, QRY_TRACE): a line for each question of `asked`, `count` of
// them, handed to the run's trace: its identity, `how` it was answered, and what came of it, with why where it was not
// answered. Where a panic is on the line, the lines are a panic's, and always reach the run
static void run_channel_traced(RunQuestion *const *asked, unsigned int count, const char *how)
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
    // the round, this one question, checked before the part and fixed where a count of registers fixes it, then its
    // line handed critical before the carrier starts
    RunQuestion *const one[1] = {asked};
    const int listed = run_channel_relisted(one, 0u, 1u, 1);
    if (listed && !run_channel_checked(one, 0u, 1u, 1, asked->refused, sizeof(asked->refused)))
    {
        asked->outcome = RUN_NO_CHANNEL;
        return 0;
    }
    // the count of registers the check left the question, which the carrier is handed
    run_channel_list_line(0u, line);
    snprintf(registers, sizeof(registers), "%u", asked->registers);
    const unsigned long long after = listed ? run_channel_round_handed() : 0ull;
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
    // what this process handed moved into the run before the carrier reads it, and what it handed once it ends
    qry_moved(s_channel.buffer);
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_settled(s_channel.buffer);
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
        // a carrier that did not end clean wrote no answer worth reading: its ending is the answer, and where it was
        // ended, or a panic is on the line, it ended carrying this question, which calls a conference
        asked->outcome = (answer.ending == INTERFACE_ENDING_EXITED) ? (unsigned int)RUN_NO_CHANNEL
                                                                     : (unsigned int)RUN_NOTHING;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier %s, code %llx",
                 interface_ending_name(answer.ending), answer.code);
        if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
        {
            run_channel_conference_called(asked, asked->refused, NULL, 0ull);
        }
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
            RunQuestion *const one[1] = {asked[at]};
            run_channel_traced(one, 1u, "from R");
            continue;
        }
        left[lefts] = asked[at];
        lefts += 1u;
    }
    const unsigned int carried = (lefts != 0u) ? run_channel_carried_many(left, lefts) : 0u;
    for (unsigned int at = 0u; (at < lefts) && !s_channel.dry && !s_channel.stopped; at += 1u)
    {
        run_channel_kept(left[at]);
    }
    run_channel_traced(left, lefts, s_channel.dry ? "written dry" : "carried");
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
    // unanswered. The airlock reads the whole round once, before the first process: the held list it hands holds for
    // every process after, each carrying questions it read
    unsigned int first = 0u;
    int airlocked = 0;
    while (first < count)
    {
        if (run_channel_on_line() != 0ull)
        {
            run_channel_halted(asked, first, count);
            return answered;
        }
        int written = 1;
        for (unsigned int at = first; written && (at < count); at += 1u)
        {
            char code_name[RUN_NAME_LONGEST];
            char cases_name[RUN_NAME_LONGEST];
            char answers_name[RUN_NAME_LONGEST];
            run_channel_named(at, 0, code_name, cases_name, answers_name);
            asked[at]->refused[0] = '\0';
            written = run_channel_write(asked[at], code_name, cases_name);
        }
        written = written && run_channel_relisted(asked, first, count, 0);
        // the round checked before the part and fixed where a count of registers fixes it, where no process of it has
        // passed the check yet: one it does not pass is not carried
        if (written && !airlocked)
        {
            char refused[sizeof(asked[first]->refused)];
            if (!run_channel_checked(asked, first, count, 0, refused, sizeof(refused)))
            {
                for (unsigned int at = first; at < count; at += 1u)
                {
                    asked[at]->outcome = RUN_NO_CHANNEL;
                    snprintf(asked[at]->refused, sizeof(asked[at]->refused), "%s", refused);
                }
                return answered;
            }
            airlocked = 1;
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
        // what this process handed moved into the run before the carrier reads it, and what it handed once it ends
        qry_moved(s_channel.buffer);
        const long ran = interface_probe_run(&probe, &answer, &error);
        qry_settled(s_channel.buffer);
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
            // a carrier ended, or one that left a panic on the line, ended carrying this question: a conference
            if ((answer.ending != INTERFACE_ENDING_EXITED) || (run_channel_on_line() != 0ull))
            {
                run_channel_conference_called(asked[at], asked[at]->refused, NULL, 0ull);
            }
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
    // R written back where it changed and was read whole
    if (s_record.changed && (s_record_path[0] != '\0') && !kqr_write(&s_record, s_record_path))
    {
        printf("  record: %s could not be written\n", s_record_path);
    }
    kqr_free(&s_record);
    s_record_path[0] = '\0';
    free(s_channel.list);
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
