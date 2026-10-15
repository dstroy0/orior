// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carrier.c: the carrier: the channel opened with the carrier's words, a line added to the list, the list written, an answer read back, and a dry run's answer written
#include "run_channel_internal.h"

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
    s_channel.dry = (words >= 1u) && (strcmp(s_channel.word[0], "dry") == 0);
    s_channel.stopped = 0;
    for (unsigned int at = 1u; at < words; at += 1u)
    {
        s_channel.stopped = s_channel.stopped || (strcmp(s_channel.word[at], "--dry") == 0);
    }
    s_channel.open = 1;
    return 1;
}

// `length` bytes of a line added to the end of the channel's list: 1, or 0 where there is no memory for them
int run_channel_listed(const char *line, size_t length)
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
int run_channel_write(const RunQuestion *asked, const char *code_name, const char *cases_name)
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
    if (!qry_latest(s_channel.buffer, answers_name, &bytes, &size, &sequence) || (sequence <= after) || (size == 0ull))
    {
        // a question the carrier ended the round before, after a launch that faulted the part or hung: carried by no
        // one, kept nowhere, and put again once the conference has ruled
        asked->outcome = RUN_NO_CHANNEL;
        snprintf(asked->refused, sizeof(asked->refused), "the carrier wrote no answer: it ended the round before it");
        return;
    }
    // the first line, the answer; and after it, in any order, the lines the carrier adds: the registers it read off
    // the part, and the time its launches took, which is read wherever it stands
    const unsigned char *const first_end = (const unsigned char *)memchr(bytes, '\n', (size_t)size);
    const size_t first_length = (first_end != NULL) ? (size_t)(first_end - bytes) : (size_t)size;
    const size_t kept = (first_length < (sizeof(s_answer_line) - 1u)) ? first_length : (sizeof(s_answer_line) - 1u);
    memcpy(s_answer_line, bytes, kept);
    s_answer_line[kept] = '\0';
    char timed_line[64] = "";
    unsigned long long line_at = (first_end != NULL) ? (unsigned long long)(first_end - bytes) + 1ull : size;
    while (line_at < size)
    {
        char line[128];
        if (!qry_line_next(bytes, size, &line_at, line, sizeof(line)))
        {
            break;
        }
        if (strncmp(line, "timed ", 6u) == 0)
        {
            snprintf(timed_line, sizeof(timed_line), "%s", line);
        }
        if (strncmp(line, "clock address ", 14u) == 0)
        {
            // the timer register's address, in hex as the carrier writes it
            asked->clock_address = (unsigned int)strtoul(line + 14, NULL, 16);
        }
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
void run_channel_dry_written(RunQuestion *asked)
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
