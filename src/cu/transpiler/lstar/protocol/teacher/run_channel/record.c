// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.c: the record: a question's identity, the record's answer to it, a question kept in the record, the registers past, the record written, a line of the list, and the round handed
#include "run_channel_internal.h"

// a question's identity as R keeps it: the hash of its code, its registers, its shape and launches, its count of
// cases and the hash of its cases, into `identity`
void run_channel_identity(const RunQuestion *question, KqrIdentity *identity)
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
KqrRecord s_record;

char s_record_path[1024];

// 1 where R holds an answer to the untimed question `asked`, which is then answered from it: answers with the
// word of every case, illegal as held off the part, and censored as held where the vendor's reader held it, as R
// keeps them. An ask that timed out is no answer
int run_channel_recorded(RunQuestion *asked)
{
    static KqrAsk s_held;
    KqrIdentity identity;
    if ((asked->launches != 0u) || !s_record.open)
    {
        return 0;
    }
    run_channel_identity(asked, &identity);
    // an ask the gate held under a rule of its own is open as a timed-out one is: the part never saw it, the gate
    // reads it again as its rules stand, and one held for an unknown key a conference may admit. The vendor's verdict
    // on an encoding stands, and an ask it held stays held
    if (!kqr_find(&s_record, &identity, &s_held) || (s_held.answer == KQR_TIMED_OUT) ||
        ((s_held.answer == KQR_CENSORED) && !run_channel_vendor_held(s_held.refusal)))
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
void run_channel_kept(const RunQuestion *asked)
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
void run_channel_list_line(unsigned int place, char *line)
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
unsigned long long run_channel_round_handed(void)
{
    return qry_hand(s_channel.buffer, "questions.txt", s_channel.list, s_channel.list_size, QRY_CRITICAL);
}

char s_airlock_output[RUN_CARRIER_OUTPUT];
