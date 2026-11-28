// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.c: the record: a question's identity, the record's answer to it, a question stored in the record, the
// registers past, the record written, a line of the list, and the round passed
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "run_channel_internal.h"

// the identity of a question as R stores it: the hash of its code, its registers, and its shape and launches. Then its
// count of cases and the hash of its cases, into `identity`
void run_channel_identity(const RunQuestion *question, KqrIdentity *identity)
{
    unsigned long long code = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < question->code_size; at += 1ULL)
    {
        code = (code ^ question->code[at]) * 0x100000001b3ULL;
    }
    unsigned long long cases = 0xcbf29ce484222325ULL;
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        cases = (cases ^ words[at]) * 0x100000001b3ULL;
    }
    identity->code = code;
    identity->registers = question->registers;
    identity->threads = question->threads;
    identity->blocks = question->blocks;
    identity->launches = question->launches;
    identity->cases = question->cases;
    identity->cases_hash = cases;
}

// R, the record of the part (kqr.h), open where run_channel_record read it. Then the path that the program writes it
// back to, empty where the program does not write it back
KqrRecord s_record;

char s_record_path[1024];

// 1 where R holds an answer to the untimed question `question`, and the call then answers it from R, as R stores them.
// Answers come with the word of every case, and illegal comes as blocked from the part. Censored comes as blocked,
// where the reader of the vendor blocked it. A query that timed out is no answer
int run_channel_recorded(RunQuestion *question)
{
    static KqrQuery s_found;
    KqrIdentity identity;
    if ((question->launches != 0u) || !s_record.open)
    {
        return 0;
    }
    run_channel_identity(question, &identity);
    // a query that the gate blocked under a rule of its own is open, as a timed-out one is. The part never saw it, and
    // the gate reads it again as its rules are now. A conference can admit one blocked for an unknown key. The verdict
    // of the vendor on an encoding stays, and a query that it blocked stays blocked
    if (!kqr_find(&s_record, &identity, &s_found) || (s_found.answer == KQR_TIMED_OUT) ||
        ((s_found.answer == KQR_CENSORED) && !run_channel_vendor_gated(s_found.refusal)))
    {
        return 0;
    }
    const int answers = (s_found.answer == KQR_ANSWERS);
    const int blocked = (s_found.answer == KQR_ILLEGAL) || (s_found.answer == KQR_CENSORED);
    question->outcome = answers   ? (unsigned int)RUN_ANSWERED
                        : blocked ? (unsigned int)RUN_GATED
                                  : (unsigned int)RUN_NOTHING;
    run_channel_refused(question, s_found.refusal);
    for (unsigned int place = 0u; answers && (place < question->cases) && (place < s_found.words); place += 1u)
    {
        question->answered[place] = s_found.word[place];
    }
    return 1;
}

// What came back of the carried question `question`, stored in R: an untimed one as a query, and a timed one as a
// sample. It is answers, illegal, nothing, timed_out where its time ran out, or censored where the gate blocked it. The
// call stores nowhere a question that nothing carried
void run_channel_stored(const RunQuestion *question)
{
    static KqrQuery s_stored;
    static KqrSample s_sample;
    unsigned int answer = KQR_ANSWER_COUNT;
    switch (question->outcome)
    {
    case RUN_ANSWERED:
        answer = KQR_ANSWERS;
        break;
    case RUN_ILLEGAL:
        answer = KQR_ILLEGAL;
        break;
    case RUN_GATED:
        answer = KQR_CENSORED;
        break;
    case RUN_NOTHING:
        answer = (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = question->refused, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME),
                             .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
                     ? KQR_TIMED_OUT
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
    run_channel_identity(question, &identity);
    if (question->launches != 0u)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &s_sample, .bytes = sizeof(s_sample), .val = 0);
        s_sample.identity = identity;
        s_sample.answer = answer;
        s_sample.nanoseconds = question->nanoseconds;
        s_sample.launched = (question->max != 0ULL) ? 1 : 0;
        s_sample.least = question->least;
        s_sample.max = question->max;
        VerbaLinea refusal = {s_sample.refusal, sizeof(s_sample.refusal), 0ULL};
        mmgr_line_put_max(&refusal, question->refused, sizeof(s_sample.refusal) - 1u);
        mmgr_line_finish(&refusal);
        kqr_sample(&s_record, &s_sample);
        return;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &s_stored, .bytes = sizeof(s_stored), .val = 0);
    s_stored.identity = identity;
    s_stored.answer = answer;
    VerbaLinea refusal = {s_stored.refusal, sizeof(s_stored.refusal), 0ULL};
    mmgr_line_put_max(&refusal, question->refused, sizeof(s_stored.refusal) - 1u);
    mmgr_line_finish(&refusal);
    for (unsigned int place = 0u;
         (question->outcome == RUN_ANSWERED) && (place < question->cases) && (place < KQR_WORDS); place += 1u)
    {
        s_stored.word[place] = question->answered[place];
        s_stored.words += 1u;
    }
    kqr_store(&s_record, &s_stored);
}

int run_channel_registers_past(const char *ksc)
{
    static KscFile s_file;
    static KscRow s_row;
    s_registers_past = RUN_REGISTERS_UNANSWERED;
    ksc_read(&s_file, ksc);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (ksc_row_read(s_file.line[line], "run", &s_row) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "answers", .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "register past", .ci = EMBED_FALSE)) &&
            (s_row.value != 0ULL) && (s_row.value < RUN_REGISTERS_UNANSWERED))
        {
            // a count below the mark of no answer is one an unsigned int represents
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
        char message[sizeof(error) + 16u];
        VerbaLinea line = {message, sizeof(message), 0ULL};
        mmgr_line_put(&line, "record: ");
        mmgr_line_put(&line, error);
        mmgr_line_ch(&line, '\n');
        mmgr_line_finish(&line);
        run_channel_trace_line(message);
        return 0;
    }
    VerbaLinea record_path = {s_record_path, sizeof(s_record_path), 0ULL};
    mmgr_line_put(&record_path, path);
    mmgr_line_finish(&record_path);
    // R, a view of the table of queries of the run, written as its rows build it. With no run, the call writes it as
    // stored
    kqr_tabled(&s_record, qry_member_run(), path);
    kqr_mark(&s_record, KQR_CYCLE_RECORD, mode);
    return 1;
}

// the line of the list of the channel for its `place`th question, into `line`, which has room for RUN_LINE_MAX_LEN,
// with its end kept. It is empty where the list holds no such line
void run_channel_list_line(unsigned int place, char *line)
{
    size_t at = 0u;
    for (unsigned int seen = 0u; (seen < place) && (at < s_channel.list_size); seen += 1u)
    {
        const char *const end = (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = s_channel_list + at,
                                                         .bytes = s_channel.list_size - at, .val = '\n');
        at = (end != NULL) ? ((size_t)(end - s_channel_list) + 1u) : s_channel.list_size;
    }
    const char *const end = (at < s_channel.list_size)
                                ? (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = s_channel_list + at,
                                                           .bytes = s_channel.list_size - at, .val = '\n')
                                : NULL;
    size_t length = (at >= s_channel.list_size) ? 0u
                    : (end != NULL)             ? ((size_t)(end - (s_channel_list + at)) + 1u)
                                                : (s_channel.list_size - at);
    length = (length < (RUN_LINE_MAX_LEN - 1u)) ? length : (RUN_LINE_MAX_LEN - 1u);
    if (length != 0u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = line, .src = s_channel_list + at, .bytes = length);
    }
    line[length] = '\0';
}

// The round that goes to the carrier next, the list of the channel, added as critical as questions.txt. It is on disk
// before the carrier starts, so that a launch that faults the part leaves the record of what the carrier got. The
// return is the place that the schedule of the run then holds, and every answer of the round lies at it or past it.
// It is 0 where the call could not put the list on disk, and then the round does not go to the carrier
unsigned long long run_channel_round_passed(void)
{
    return qry_member_append("questions.txt", s_channel_list, s_channel.list_size, QRY_CRITICAL) ? qry_member_settled()
                                                                                                 : 0ULL;
}

char s_airlock_output[RUN_CARRIER_OUTPUT];
