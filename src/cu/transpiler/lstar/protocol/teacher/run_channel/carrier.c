// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carrier.c: the carrier: the channel opened with the carrier's words, a line added to the list, the list written, an answer read back, and a dry run's answer written
#include "run_channel_internal.h"
#include "../../../../../../../includes/parsers/line_read.h"

int run_channel_open(const char *const *carrier, unsigned long long limit_microseconds)
{
    memset(&s_channel, 0, sizeof(s_channel));
    // the carrier's words, then the pop's after `--pop` and the cross's after `--cross`, each from 1 to
    // RUN_CARRIER_WORDS
    char (*part)[RUN_PATH_MAX_LEN] = s_channel.word;
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
            char message[128];
            VerbaLinea line = {message, sizeof(message), 0ull};
            mmgr_line_put(&line, "run_channel: a carrier, its pop and its cross are each from 1 to ");
            mmgr_line_decimal(&line, RUN_CARRIER_WORDS, 0u);
            mmgr_line_put(&line, " words\n");
            mmgr_line_finish(&line);
            run_channel_trace_line(message);
            return 0;
        }
        VerbaLinea word = {part[*count], RUN_PATH_MAX_LEN, 0ull};
        mmgr_line_put_most(&word, carrier[at], RUN_PATH_MAX_LEN - 1u);
        mmgr_line_finish(&word);
        *count += 1u;
    }
    const unsigned int words = s_channel.words;
    if ((words == 0u) || ((s_channel.pop_words == 0u) != (s_channel.cross_words == 0u)))
    {
        char message[128];
        VerbaLinea line = {message, sizeof(message), 0ull};
        mmgr_line_put(&line, "run_channel: a carrier is from 1 to ");
        mmgr_line_decimal(&line, RUN_CARRIER_WORDS, 0u);
        mmgr_line_put(&line, " words, and an airlock both a pop and a cross\n");
        mmgr_line_finish(&line);
        run_channel_trace_line(message);
        return 0;
    }
    // the gated list the carrier reads, named after its `--gated`
    for (unsigned int at = 0u; (at + 1u) < words; at += 1u)
    {
        if (strcmp(s_channel.word[at], "--gated") == 0)
        {
            VerbaLinea gated_line = {s_channel.gated_list, sizeof(s_channel.gated_list), 0ull};
            mmgr_line_put_most(&gated_line, s_channel.word[at + 1u], sizeof(s_channel.gated_list) - 1u);
            mmgr_line_finish(&gated_line);
            s_channel.gated_given = 1;
        }
    }
    if ((s_channel.pop_words != 0u) && !s_channel.gated_given)
    {
        run_channel_trace_line(
            "run_channel: an airlock's carrier reads the cross's gated list, and this one is handed none (--gated)\n");
        return 0;
    }
    s_channel.buffer = qry_member_run();
    if (s_channel.buffer == NULL)
    {
        return 0;
    }
    s_channel.words = words;
    s_channel.limit_microseconds = limit_microseconds;
    s_channel.dry = (words >= 1u) && (strcmp(s_channel.word[0], "dry") == 0);
    s_channel.stopped = 0;
    for (unsigned int at = 1u; at < words; at += 1u)
    {
        s_channel.stopped = s_channel.stopped || (strcmp(s_channel.word[at], "--dry") == 0);
    }
    s_channel.open = 1;
    return 1;
}

// `length` bytes of a line added to the end of the channel's list: 1, or 0 where the list has no room for them
int run_channel_listed(const char *line, size_t length)
{
    if ((s_channel.list_size + length) > sizeof(s_channel_list))
    {
        return 0;
    }
    memcpy(s_channel_list + s_channel.list_size, line, length);
    s_channel.list_size += length;
    return 1;
}

// the question's code and cases handed as the blobs `code_name` and `cases_name`, the cases a line each as the
// carrier reads them: 1, or 0 where either was dumped
int run_channel_write(const RunQuestion *question, const char *code_name, const char *cases_name)
{
    if (qry_member_append(code_name, question->code, question->code_size, QRY_ORDINARY) == 0)
    {
        return 0;
    }
    // the text is sized for every case at its longest
    VerbaLinea cases = {s_cases_text, sizeof(s_cases_text), 0ull};
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        for (unsigned int at = 0u; at < RUN_IN_WORDS; at += 1u)
        {
            if (at != 0u)
            {
                mmgr_line_ch(&cases, ' ');
            }
            mmgr_line_hex(&cases, question->word[place][at], 0u);
        }
        mmgr_line_ch(&cases, '\n');
    }
    return mmgr_line_ok(&cases) && (qry_member_append(cases_name, s_cases_text, cases.at, QRY_ORDINARY) != 0);
}

// The blob the carrier handed for `answers_name` after the round handed as `after`, read into `question`: the outcome
// its first line names, each case's answer where it answered, and where the question is timed, the time the second
// gives, its launches together and the least and the most one took. A blob of the name from before the round is an
// older question's, and is none. A timed question the carrier gives no time for answered nothing worth reading
void run_channel_read(RunQuestion *question, const char *answers_name, unsigned long long after)
{
    question->outcome = RUN_NOTHING;
    question->nanoseconds = 0ull;
    question->least = 0ull;
    question->most = 0ull;
    question->clock_address = 0u;
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_member_blob(answers_name, &bytes, &size, &sequence) || (sequence < after) || (size == 0ull))
    {
        // a question the carrier ended the round before, after a launch that faulted the part or hung: carried by no
        // one, stored nowhere, and put again once the conference has ruled
        question->outcome = RUN_NO_CHANNEL;
        VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
        mmgr_line_put(&refused, "the carrier wrote no answer: it ended the round before it");
        mmgr_line_finish(&refused);
        return;
    }
    // the first line, the answer; and after it, in any order, the lines the carrier adds: the registers it read off
    // the part, and the time its launches took, which is read wherever it stands
    const unsigned char *const first_end = (const unsigned char *)memchr(bytes, '\n', (size_t)size);
    const size_t first_length = (first_end != NULL) ? (size_t)(first_end - bytes) : (size_t)size;
    const size_t copied = (first_length < (sizeof(s_answer_line) - 1u)) ? first_length : (sizeof(s_answer_line) - 1u);
    memcpy(s_answer_line, bytes, copied);
    s_answer_line[copied] = '\0';
    char timed_line[128] = "";
    unsigned long long line_at = (first_end != NULL) ? (unsigned long long)(first_end - bytes) + 1ull : size;
    while (line_at < size)
    {
        char line[128];
        if (!line_read(bytes, size, &line_at, line, sizeof(line)))
        {
            break;
        }
        if (strncmp(line, "timed ", 6u) == 0)
        {
            VerbaLinea timed_text = {timed_line, sizeof(timed_line), 0ull};
            mmgr_line_put_most(&timed_text, line, sizeof(timed_line) - 1u);
            mmgr_line_finish(&timed_text);
        }
        if (strncmp(line, "clock address ", 14u) == 0)
        {
            // the timer register's address, in hex as the carrier writes it
            question->clock_address = (unsigned int)strtoul(line + 14, NULL, 16);
        }
    }
    unsigned int launches = 0u;
    const char *timed_at = timed_line;
    int timed = line_letters(&timed_at, "timed ") && line_unsigned(&timed_at, &launches) &&
                line_number(&timed_at, 0, &question->nanoseconds);
    // the least launch and the most follow where the carrier timed them
    if (timed && line_number(&timed_at, 0, &question->least))
    {
        line_number(&timed_at, 0, &question->most);
    }
    timed = timed && (launches == question->launches);
    s_answer_line[strcspn(s_answer_line, "\r\n")] = '\0';
    if (strncmp(s_answer_line, "answered", 8u) != 0)
    {
        question->outcome =
            (strncmp(s_answer_line, "skipped", 7u) == 0) ? (unsigned int)RUN_GATED : (unsigned int)RUN_ILLEGAL;
        VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
        mmgr_line_put_most(&refused, s_answer_line, 120ull);
        mmgr_line_finish(&refused);
        // a refusal calls a conference, the carrier's accounts after the refusal's line handed with the teacher's
        if (question->outcome == RUN_ILLEGAL)
        {
            char how[160];
            VerbaLinea how_line = {how, sizeof(how), 0ull};
            mmgr_line_put(&how_line, "the part refused it, ");
            mmgr_line_put_most(&how_line, s_answer_line, 120ull);
            mmgr_line_finish(&how_line);
            const unsigned long long output_at =
                (first_end != NULL) ? (unsigned long long)(first_end - bytes) + 1ull : size;
            run_channel_conference_called(question, how, bytes + output_at, size - output_at);
            run_channel_line_ruled(question);
        }
        return;
    }
    const char *at = s_answer_line + 8u;
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        char *after_word = NULL;
        question->answered[place] = strtoull(at, &after_word, 16);
        if (after_word == at)
        {
            VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
            mmgr_line_put(&refused, "the carrier answered ");
            mmgr_line_decimal(&refused, place, 0u);
            mmgr_line_put(&refused, " of ");
            mmgr_line_decimal(&refused, question->cases, 0u);
            mmgr_line_put(&refused, " cases");
            mmgr_line_finish(&refused);
            return;
        }
        at = after_word;
    }
    if ((question->launches != 0u) && !timed)
    {
        VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
        mmgr_line_put(&refused, "the carrier gave no time for ");
        mmgr_line_decimal(&refused, question->launches, 0u);
        mmgr_line_put(&refused, " launches");
        mmgr_line_finish(&refused);
        return;
    }
    question->outcome = RUN_ANSWERED;
}

// `question` handed as the next question of a dry run, its line added to the run's list, and read as gated: nothing
// carries it and nothing answers it
void run_channel_dry_written(RunQuestion *question)
{
    char code_name[RUN_NAME_MAX_LEN];
    char cases_name[RUN_NAME_MAX_LEN];
    char answers_name[RUN_NAME_MAX_LEN];
    char line[RUN_LINE_MAX_LEN];
    s_channel.dry_written += 1u;
    run_channel_named(s_channel.dry_written, 0, code_name, cases_name, answers_name);
    const size_t line_length =
        run_channel_list_line_written(line, sizeof(line), code_name, cases_name, answers_name, question);
    const int written = run_channel_write(question, code_name, cases_name) && (line_length != 0u) &&
                        run_channel_listed(line, line_length);
    question->outcome = written ? (unsigned int)RUN_GATED : (unsigned int)RUN_NO_CHANNEL;
    VerbaLinea refused = {question->refused, sizeof(question->refused), 0ull};
    mmgr_line_put(&refused, written ? "dry: written, carried by nothing" : "dry: the question could not be written");
    mmgr_line_finish(&refused);
}

void run_channel_close(void)
{
    // the dry run's list, every question it wrote, handed whole
    if (s_channel.open && s_channel.dry && (s_channel.list_size != 0u) &&
        (qry_member_append("questions.txt", s_channel_list, s_channel.list_size, QRY_ORDINARY) == 0))
    {
        run_channel_trace_line("run_channel: the dry run's list could not be handed to the run\n");
    }
    // R written back where it changed and was read whole
    if (s_record.changed && (s_record_path[0] != '\0') && !kqr_write(&s_record, s_record_path))
    {
        char message[RUN_PATH_MAX_LEN + 64u];
        VerbaLinea line = {message, sizeof(message), 0ull};
        mmgr_line_put(&line, "record: ");
        mmgr_line_put(&line, s_record_path);
        mmgr_line_put(&line, " could not be written\n");
        mmgr_line_finish(&line);
        run_channel_trace_line(message);
    }
    kqr_free(&s_record);
    s_record_path[0] = '\0';
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
