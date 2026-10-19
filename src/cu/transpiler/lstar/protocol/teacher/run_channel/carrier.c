// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carrier.c: the carrier: the channel opened with the carrier's words, a line added to the list, the list written, an answer read back, and a dry run's answer written
#include "run_channel_internal.h"
#include "../../../../../includes/parsers/line_read.h"

int run_channel_open(const char *const *carrier, unsigned long long limit_microseconds)
{
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
            char said[128];
            ScripturaLine line = {said, sizeof(said), 0ull};
            scriptura_text(&line, "run_channel: a carrier, its pop and its cross are each from 1 to ");
            scriptura_decimal(&line, RUN_CARRIER_WORDS, 0u);
            scriptura_text(&line, " words\n");
            scriptura_closed(&line);
            run_channel_said(said);
            return 0;
        }
        ScripturaLine word = {part[*count], RUN_PATH_LONGEST, 0ull};
        scriptura_text_most(&word, carrier[at], RUN_PATH_LONGEST - 1u);
        scriptura_closed(&word);
        *count += 1u;
    }
    const unsigned int words = s_channel.words;
    if ((words == 0u) || ((s_channel.pop_words == 0u) != (s_channel.cross_words == 0u)))
    {
        char said[128];
        ScripturaLine line = {said, sizeof(said), 0ull};
        scriptura_text(&line, "run_channel: a carrier is from 1 to ");
        scriptura_decimal(&line, RUN_CARRIER_WORDS, 0u);
        scriptura_text(&line, " words, and an airlock both a pop and a cross\n");
        scriptura_closed(&line);
        run_channel_said(said);
        return 0;
    }
    // the held list the carrier reads, named after its `--held`
    for (unsigned int at = 0u; (at + 1u) < words; at += 1u)
    {
        if (strcmp(s_channel.word[at], "--held") == 0)
        {
            ScripturaLine held = {s_channel.held, sizeof(s_channel.held), 0ull};
            scriptura_text_most(&held, s_channel.word[at + 1u], sizeof(s_channel.held) - 1u);
            scriptura_closed(&held);
            s_channel.held_given = 1;
        }
    }
    if ((s_channel.pop_words != 0u) && !s_channel.held_given)
    {
        run_channel_said(
            "run_channel: an airlock's carrier reads the cross's held list, and this one is handed none (--held)\n");
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
int run_channel_write(const RunQuestion *asked, const char *code_name, const char *cases_name)
{
    if (qry_member_append(code_name, asked->code, asked->code_size, QRY_ORDINARY) == 0)
    {
        return 0;
    }
    // the text is sized for every case at its longest
    ScripturaLine cases = {s_cases_text, sizeof(s_cases_text), 0ull};
    for (unsigned int place = 0u; place < asked->cases; place += 1u)
    {
        for (unsigned int at = 0u; at < RUN_IN_WORDS; at += 1u)
        {
            if (at != 0u)
            {
                scriptura_character(&cases, ' ');
            }
            scriptura_hex(&cases, asked->word[place][at], 0u);
        }
        scriptura_character(&cases, '\n');
    }
    return scriptura_fits(&cases) && (qry_member_append(cases_name, s_cases_text, cases.at, QRY_ORDINARY) != 0);
}

// The blob the carrier handed for `answers_name` after the round handed as `after`, read into `asked`: the outcome
// its first line names, each case's answer where it answered, and where the question is timed, the time the second
// gives, its launches together and the least and the most one took. A blob of the name from before the round is an
// older question's, and is none. A timed question the carrier gives no time for answered nothing worth reading
void run_channel_read(RunQuestion *asked, const char *answers_name, unsigned long long after)
{
    asked->outcome = RUN_NOTHING;
    asked->nanoseconds = 0ull;
    asked->least = 0ull;
    asked->most = 0ull;
    asked->clock_address = 0u;
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    if (!qry_member_blob(answers_name, &bytes, &size, &sequence) || (sequence < after) || (size == 0ull))
    {
        // a question the carrier ended the round before, after a launch that faulted the part or hung: carried by no
        // one, kept nowhere, and put again once the conference has ruled
        asked->outcome = RUN_NO_CHANNEL;
        ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
        scriptura_text(&refused, "the carrier wrote no answer: it ended the round before it");
        scriptura_closed(&refused);
        return;
    }
    // the first line, the answer; and after it, in any order, the lines the carrier adds: the registers it read off
    // the part, and the time its launches took, which is read wherever it stands
    const unsigned char *const first_end = (const unsigned char *)memchr(bytes, '\n', (size_t)size);
    const size_t first_length = (first_end != NULL) ? (size_t)(first_end - bytes) : (size_t)size;
    const size_t kept = (first_length < (sizeof(s_answer_line) - 1u)) ? first_length : (sizeof(s_answer_line) - 1u);
    memcpy(s_answer_line, bytes, kept);
    s_answer_line[kept] = '\0';
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
            ScripturaLine timed_text = {timed_line, sizeof(timed_line), 0ull};
            scriptura_text_most(&timed_text, line, sizeof(timed_line) - 1u);
            scriptura_closed(&timed_text);
        }
        if (strncmp(line, "clock address ", 14u) == 0)
        {
            // the timer register's address, in hex as the carrier writes it
            asked->clock_address = (unsigned int)strtoul(line + 14, NULL, 16);
        }
    }
    unsigned int launches = 0u;
    const char *timed_at = timed_line;
    int timed = line_letters(&timed_at, "timed ") && line_unsigned(&timed_at, &launches) &&
                line_number(&timed_at, 0, &asked->nanoseconds);
    // the least launch and the most follow where the carrier timed them
    if (timed && line_number(&timed_at, 0, &asked->least))
    {
        line_number(&timed_at, 0, &asked->most);
    }
    timed = timed && (launches == asked->launches);
    s_answer_line[strcspn(s_answer_line, "\r\n")] = '\0';
    if (strncmp(s_answer_line, "answered", 8u) != 0)
    {
        asked->outcome = (strncmp(s_answer_line, "skipped", 7u) == 0) ? (unsigned int)RUN_HELD : (unsigned int)RUN_ILLEGAL;
        ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
        scriptura_text_most(&refused, s_answer_line, 120ull);
        scriptura_closed(&refused);
        // a refusal calls a conference, the carrier's accounts after the refusal's line handed with the teacher's
        if (asked->outcome == RUN_ILLEGAL)
        {
            char how[160];
            ScripturaLine how_line = {how, sizeof(how), 0ull};
            scriptura_text(&how_line, "the part refused it, ");
            scriptura_text_most(&how_line, s_answer_line, 120ull);
            scriptura_closed(&how_line);
            const unsigned long long said_at =
                (first_end != NULL) ? (unsigned long long)(first_end - bytes) + 1ull : size;
            run_channel_conference_called(asked, how, bytes + said_at, size - said_at);
            run_channel_line_ruled(asked);
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
            ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
            scriptura_text(&refused, "the carrier answered ");
            scriptura_decimal(&refused, place, 0u);
            scriptura_text(&refused, " of ");
            scriptura_decimal(&refused, asked->cases, 0u);
            scriptura_text(&refused, " cases");
            scriptura_closed(&refused);
            return;
        }
        at = after_word;
    }
    if ((asked->launches != 0u) && !timed)
    {
        ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
        scriptura_text(&refused, "the carrier gave no time for ");
        scriptura_decimal(&refused, asked->launches, 0u);
        scriptura_text(&refused, " launches");
        scriptura_closed(&refused);
        return;
    }
    asked->outcome = RUN_ANSWERED;
}

// `asked` handed as the next question of a dry run, its line added to the run's list, and read as held: nothing
// carries it and nothing answers it
void run_channel_dry_written(RunQuestion *asked)
{
    char code_name[RUN_NAME_LONGEST];
    char cases_name[RUN_NAME_LONGEST];
    char answers_name[RUN_NAME_LONGEST];
    char line[RUN_LINE_LONGEST];
    s_channel.dry_written += 1u;
    run_channel_named(s_channel.dry_written, 0, code_name, cases_name, answers_name);
    const size_t line_length =
        run_channel_list_line_written(line, sizeof(line), code_name, cases_name, answers_name, asked);
    const int written = run_channel_write(asked, code_name, cases_name) && (line_length != 0u) &&
                        run_channel_listed(line, line_length);
    asked->outcome = written ? (unsigned int)RUN_HELD : (unsigned int)RUN_NO_CHANNEL;
    ScripturaLine refused = {asked->refused, sizeof(asked->refused), 0ull};
    scriptura_text(&refused, written ? "dry: written, carried by nothing" : "dry: the question could not be written");
    scriptura_closed(&refused);
}

void run_channel_close(void)
{
    // the dry run's list, every question it wrote, handed whole
    if (s_channel.open && s_channel.dry && (s_channel.list_size != 0u) &&
        (qry_member_append("questions.txt", s_channel_list, s_channel.list_size, QRY_ORDINARY) == 0))
    {
        run_channel_said("run_channel: the dry run's list could not be handed to the run\n");
    }
    // R written back where it changed and was read whole
    if (s_record.changed && (s_record_path[0] != '\0') && !kqr_write(&s_record, s_record_path))
    {
        char said[RUN_PATH_LONGEST + 64u];
        ScripturaLine line = {said, sizeof(said), 0ull};
        scriptura_text(&line, "record: ");
        scriptura_text(&line, s_record_path);
        scriptura_text(&line, " could not be written\n");
        scriptura_closed(&line);
        run_channel_said(said);
    }
    kqr_free(&s_record);
    s_record_path[0] = '\0';
    memset(&s_channel, 0, sizeof(s_channel));
}

const char *run_channel_carrier(void)
{
    return s_channel.open ? s_channel.word[0] : "none";
}
